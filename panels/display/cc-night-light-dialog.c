/*
 * Copyright (C) 2017 Richard Hughes <richard@hughsie.com>
 *
 * Licensed under the GNU General Public License Version 2
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "config.h"

#include <gdesktop-enums.h>
#include <gtk/gtk.h>
#include <math.h>

#include "cc-night-light-dialog.h"
#include "cc-night-light-widget.h"

struct _CcNightLightDialog {
  GObject              parent;
  GtkBuilder          *builder;
  GSettings           *settings_display;
  GSettings           *settings_clock;
  GDBusProxy          *proxy_color;
  GDBusProxy          *proxy_color_props;
  GCancellable        *cancellable;
  GtkWidget           *night_light_widget;
  GtkWidget           *main_window;
  gboolean             ignore_value_changed;
  guint                timer_id;
  GDesktopClockFormat  clock_format;
};

G_DEFINE_TYPE (CcNightLightDialog, cc_night_light_dialog, G_TYPE_OBJECT);

#define CLOCK_SCHEMA     "org.gnome.desktop.interface"
#define DISPLAY_SCHEMA   "org.gnome.settings-daemon.plugins.color"
#define CLOCK_FORMAT_KEY "clock-format"

void
cc_night_light_dialog_present (CcNightLightDialog *self, GtkWindow *parent)
{
  GtkWindow *window = GTK_WINDOW (self->main_window);
  if (parent != NULL)
    {
      gtk_window_set_transient_for (window, parent);
      gtk_window_set_modal (window, TRUE);
    }
  gtk_window_present (window);
}

static void
cc_night_light_dialog_finalize (GObject *object)
{
  CcNightLightDialog *self = CC_NIGHT_LIGHT_DIALOG (object);

  if (self->cancellable != NULL)
    {
      g_cancellable_cancel (self->cancellable);
      g_clear_object (&self->cancellable);
    }

  if (self->main_window)
    {
      gtk_widget_destroy (self->main_window);
      self->main_window = NULL;
    }

  g_clear_object (&self->builder);
  g_clear_object (&self->proxy_color);
  g_clear_object (&self->proxy_color_props);
  g_clear_object (&self->settings_display);
  g_clear_object (&self->settings_clock);
  if (self->timer_id > 0)
    g_source_remove (self->timer_id);

  G_OBJECT_CLASS (cc_night_light_dialog_parent_class)->finalize (object);
}

static void
cc_night_light_dialog_class_init (CcNightLightDialogClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  object_class->finalize = cc_night_light_dialog_finalize;
}

static gdouble
frac_day_from_dt (GDateTime *dt)
{
  return g_date_time_get_hour (dt) +
          (gdouble) g_date_time_get_minute (dt) / 60.f +
          (gdouble) g_date_time_get_second (dt) / 3600.f;
}

static void
dialog_adjustments_set_frac_hours (CcNightLightDialog *self,
                                   gdouble value,
                                   const gchar *id_hours,
                                   const gchar *id_mins,
                                   const gchar *id_stack)
{
  GtkAdjustment *adj;
  gdouble hours;
  gdouble mins = 0.f;
  gboolean is_pm = FALSE;
  gboolean is_24h;
  GtkWidget *widget;

  /* display the right thing for AM/PM */
  is_24h = self->clock_format == G_DESKTOP_CLOCK_FORMAT_24H;
  mins = modf (value, &hours) * 60.f;
  if (!is_24h)
    {
      if (hours > 12)
        {
          hours -= 12;
          is_pm = TRUE;
        }
      else if (hours < 1.0)
        {
          hours += 12;
          is_pm = FALSE;
        }
      else if (hours == 12.f)
        {
          is_pm = TRUE;
        }
    }
  g_debug ("setting adjustment %.3f to %.0f:%02.0f", value, hours, mins);

  self->ignore_value_changed = TRUE;
  adj = GTK_ADJUSTMENT (gtk_builder_get_object (self->builder, id_hours));
  gtk_adjustment_set_value (GTK_ADJUSTMENT (adj), hours);
  adj = GTK_ADJUSTMENT (gtk_builder_get_object (self->builder, id_mins));
  gtk_adjustment_set_value (GTK_ADJUSTMENT (adj), mins);
  self->ignore_value_changed = FALSE;

  widget = GTK_WIDGET (gtk_builder_get_object (self->builder, id_stack));
  if (is_24h)
    gtk_stack_set_visible_child_name (GTK_STACK (widget), "blank");
  else
    gtk_stack_set_visible_child_name (GTK_STACK (widget), is_pm ? "pm" : "am");
}

static void
dialog_update_state (CcNightLightDialog *self)
{
  GtkWidget *widget;
  gboolean automatic;
  gboolean disabled_until_tomorrow = FALSE;
  gboolean enabled;
  gdouble value = 0.f;
  g_autoptr(GDateTime) dt = g_date_time_new_now_local ();

  /* only show the infobar if we are disabled */
  if (self->proxy_color != NULL)
    {
      g_autoptr(GVariant) disabled = NULL;
      disabled = g_dbus_proxy_get_cached_property (self->proxy_color,
                                                   "DisabledUntilTomorrow");
      if (disabled != NULL)
        disabled_until_tomorrow = g_variant_get_boolean (disabled);
    }
  widget = GTK_WIDGET (gtk_builder_get_object (self->builder, "infobar_disabled"));
  gtk_widget_set_visible (widget, disabled_until_tomorrow);

  /* make things insensitive if the switch is disabled */
  enabled = g_settings_get_boolean (self->settings_display,
                                    "night-light-enabled");
  automatic = g_settings_get_boolean (self->settings_display,
                                      "night-light-schedule-automatic");
  gtk_widget_set_sensitive (self->night_light_widget, enabled);
  widget = GTK_WIDGET (gtk_builder_get_object (self->builder, "togglebutton_automatic"));
  gtk_widget_set_sensitive (widget, enabled);
  self->ignore_value_changed = TRUE;
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), automatic);
  self->ignore_value_changed = FALSE;
  widget = GTK_WIDGET (gtk_builder_get_object (self->builder, "togglebutton_manual"));
  gtk_widget_set_sensitive (widget, enabled);
  self->ignore_value_changed = TRUE;
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), !automatic);
  self->ignore_value_changed = FALSE;
  widget = GTK_WIDGET (gtk_builder_get_object (self->builder, "box_manual"));
  gtk_widget_set_sensitive (widget, enabled && !automatic);

  /* show the sunset & sunrise icons when required */
  cc_night_light_widget_set_mode (CC_NIGHT_LIGHT_WIDGET (self->night_light_widget),
                                  automatic ? CC_NIGHT_LIGHT_WIDGET_MODE_AUTOMATIC :
                                              CC_NIGHT_LIGHT_WIDGET_MODE_MANUAL);

  /* set from */
  if (automatic && self->proxy_color != NULL)
    {
      g_autoptr(GVariant) sunset = NULL;
      sunset = g_dbus_proxy_get_cached_property (self->proxy_color, "Sunset");
      if (sunset != NULL)
        {
          value = g_variant_get_double (sunset);
        }
      else
        {
          value = 16.0f;
          g_warning ("no sunset data, using %02.2f", value);
        }
    }
  else
    {
      value = g_settings_get_double (self->settings_display, "night-light-schedule-from");
      value = fmod (value, 24.f);
    }
  dialog_adjustments_set_frac_hours (self, value,
                                    "adjustment_from_hours",
                                    "adjustment_from_minutes",
                                    "stack_from");
  cc_night_light_widget_set_from (CC_NIGHT_LIGHT_WIDGET (self->night_light_widget), value);

  /* set to */
  if (automatic && self->proxy_color != NULL)
    {
      g_autoptr(GVariant) sunset = NULL;
      sunset = g_dbus_proxy_get_cached_property (self->proxy_color, "Sunrise");
      if (sunset != NULL)
        {
          value = g_variant_get_double (sunset);
        }
      else
        {
          value = 8.0f;
          g_warning ("no sunrise data, using %02.2f", value);
        }
    }
  else
    {
      value = g_settings_get_double (self->settings_display, "night-light-schedule-to");
      value = fmod (value, 24.f);
    }
  dialog_adjustments_set_frac_hours (self, value,
                                     "adjustment_to_hours",
                                     "adjustment_to_minutes",
                                     "stack_to");
  cc_night_light_widget_set_to (CC_NIGHT_LIGHT_WIDGET (self->night_light_widget), value);

  /* set new time */
  cc_night_light_widget_set_now (CC_NIGHT_LIGHT_WIDGET (self->night_light_widget),
                                   frac_day_from_dt (dt));
}

static gboolean
dialog_tick_cb (gpointer user_data)
{
  CcNightLightDialog *self = (CcNightLightDialog *) user_data;
  dialog_update_state (self);
  return G_SOURCE_CONTINUE;
}

static void
dialog_enabled_notify_cb (GtkSwitch *sw, GParamSpec *pspec, CcNightLightDialog *self)
{
  g_settings_set_boolean (self->settings_display, "night-light-enabled",
                          gtk_switch_get_active (sw));
}

static void
dialog_mode_changed_cb (GtkToggleButton *togglebutton, CcNightLightDialog *self)
{
  gboolean ret;

  if (self->ignore_value_changed)
    return;
  if (!gtk_toggle_button_get_active (togglebutton))
    return;

  ret = g_settings_get_boolean (self->settings_display, "night-light-schedule-automatic");
  g_settings_set_boolean (self->settings_display, "night-light-schedule-automatic", !ret);
}

static void
dialog_undisable_call_cb (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  g_autoptr(GVariant) val = NULL;
  g_autoptr(GError) error = NULL;

  val = g_dbus_proxy_call_finish (G_DBUS_PROXY (source_object),
                                  res, &error);
  if (val == NULL)
    {
      g_warning ("failed to undisable: %s", error->message);
      return;
    }
}

static void
dialog_undisable_clicked_cb (GtkButton *button, CcNightLightDialog *self)
{
  g_dbus_proxy_call (self->proxy_color_props,
                     "Set",
                     g_variant_new ("(ssv)",
                                    "org.gnome.SettingsDaemon.Color",
                                    "DisabledUntilTomorrow",
                                    g_variant_new_boolean (FALSE)),
                     G_DBUS_CALL_FLAGS_NONE,
                     5000,
                     self->cancellable,
                     dialog_undisable_call_cb,
                     self);
}

static gdouble
dialog_adjustments_get_frac_hours (CcNightLightDialog *self,
                                   const gchar *id_hours,
                                   const gchar *id_mins,
                                   const gchar *id_stack)
{
  GtkAdjustment *adj;
  GtkWidget *widget;
  gdouble value;
  adj = GTK_ADJUSTMENT (gtk_builder_get_object (self->builder, id_hours));
  value = gtk_adjustment_get_value (adj);
  adj = GTK_ADJUSTMENT (gtk_builder_get_object (self->builder, id_mins));
  value += gtk_adjustment_get_value (adj) / 60.0f;
  widget = GTK_WIDGET (gtk_builder_get_object (self->builder, id_stack));
  if (g_strcmp0 (gtk_stack_get_visible_child_name (GTK_STACK (widget)), "pm") == 0)
    value += 12.f;
  return value;
}

static void
dialog_time_from_value_changed_cb (GtkAdjustment *adjustment, CcNightLightDialog *self)
{
  gdouble value;

  if (self->ignore_value_changed)
    return;

  value = dialog_adjustments_get_frac_hours (self,
                                             "adjustment_from_hours",
                                             "adjustment_from_minutes",
                                             "stack_from");
  if (value >= 24.f)
    value = fmod (value, 24);
  g_debug ("new value = %.3f", value);
  g_settings_set_double (self->settings_display, "night-light-schedule-from", value);
}

static void
dialog_time_to_value_changed_cb (GtkAdjustment *adjustment, CcNightLightDialog *self)
{
  gdouble value;

  if (self->ignore_value_changed)
    return;

  value = dialog_adjustments_get_frac_hours (self,
                                      "adjustment_to_hours",
                                      "adjustment_to_minutes",
                                      "stack_to");
  if (value >= 24.f)
    value = fmod (value, 24);
  g_debug ("new value = %.3f", value);
  g_settings_set_double (self->settings_display, "night-light-schedule-to", value);
}

static void
dialog_color_properties_changed_cb (GDBusProxy *proxy,
                                    GVariant *changed_properties,
                                    GStrv invalidated_properties,
                                    CcNightLightDialog *self)
{
  dialog_update_state (self);
}

static void
dialog_got_proxy_cb (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  CcNightLightDialog *self = (CcNightLightDialog *) user_data;
  GDBusProxy *proxy;
  g_autoptr(GError) error = NULL;

  proxy = g_dbus_proxy_new_for_bus_finish (res, &error);
  if (proxy == NULL)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_warning ("failed to connect to g-s-d: %s", error->message);
      return;
    }

  self->proxy_color = proxy;

  g_signal_connect (self->proxy_color, "g-properties-changed",
                    G_CALLBACK (dialog_color_properties_changed_cb), self);
  dialog_update_state (self);
  self->timer_id = g_timeout_add_seconds (10, dialog_tick_cb, self);
}

static void
dialog_got_proxy_props_cb (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  CcNightLightDialog *self = (CcNightLightDialog *) user_data;
  GDBusProxy *proxy;
  g_autoptr(GError) error = NULL;

  proxy = g_dbus_proxy_new_for_bus_finish (res, &error);
  if (proxy == NULL)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_warning ("failed to connect to g-s-d: %s", error->message);
      return;
    }

  self->proxy_color_props = proxy;
}

static gboolean
dialog_format_minutes_combobox (GtkSpinButton *spin, CcNightLightDialog *self)
{
  GtkAdjustment *adjustment;
  g_autofree gchar *text = NULL;
  adjustment = gtk_spin_button_get_adjustment (spin);
  text = g_strdup_printf ("%02.0f", gtk_adjustment_get_value (adjustment));
  gtk_entry_set_text (GTK_ENTRY (spin), text);
  return TRUE;
}

static gboolean
dialog_format_hours_combobox (GtkSpinButton *spin, CcNightLightDialog *self)
{
  GtkAdjustment *adjustment;
  g_autofree gchar *text = NULL;
  adjustment = gtk_spin_button_get_adjustment (spin);
  if (self->clock_format == G_DESKTOP_CLOCK_FORMAT_12H)
    text = g_strdup_printf ("%.0f", gtk_adjustment_get_value (adjustment));
  else
    text = g_strdup_printf ("%02.0f", gtk_adjustment_get_value (adjustment));
  gtk_entry_set_text (GTK_ENTRY (spin), text);
  return TRUE;
}

static void
dialog_update_adjustments (CcNightLightDialog *self)
{
  GtkAdjustment *adj;
  GtkWidget *widget;

  /* from */
  adj = GTK_ADJUSTMENT (gtk_builder_get_object (self->builder, "adjustment_from_hours"));
  if (self->clock_format == G_DESKTOP_CLOCK_FORMAT_24H)
    {
      gtk_adjustment_set_lower (adj, 0);
      gtk_adjustment_set_upper (adj, 23);
    }
  else
    {
      if (gtk_adjustment_get_value (adj) > 12)
        {
          widget = GTK_WIDGET (gtk_builder_get_object (self->builder, "stack_from"));
          gtk_stack_set_visible_child_name (GTK_STACK (widget), "pm");
        }
      gtk_adjustment_set_lower (adj, 1);
      gtk_adjustment_set_upper (adj, 12);
    }

  /* to */
  adj = GTK_ADJUSTMENT (gtk_builder_get_object (self->builder, "adjustment_to_hours"));
  if (self->clock_format == G_DESKTOP_CLOCK_FORMAT_24H)
    {
      gtk_adjustment_set_lower (adj, 0);
      gtk_adjustment_set_upper (adj, 23);
    }
  else
    {
      if (gtk_adjustment_get_value (adj) > 12)
        {
          widget = GTK_WIDGET (gtk_builder_get_object (self->builder, "stack_to"));
          gtk_stack_set_visible_child_name (GTK_STACK (widget), "pm");
        }
      gtk_adjustment_set_lower (adj, 1);
      gtk_adjustment_set_upper (adj, 12);
    }
}

static void
dialog_settings_changed_cb (GSettings *settings_display, gchar *key, CcNightLightDialog *self)
{
  dialog_update_state (self);
}

static void
dialog_clock_settings_changed_cb (GSettings *settings_display, gchar *key, CcNightLightDialog *self)
{
  GtkAdjustment *adj;
  GtkWidget *widget;

  self->clock_format = g_settings_get_enum (settings_display, CLOCK_FORMAT_KEY);

  /* uncontionally widen this to avoid truncation */
  adj = GTK_ADJUSTMENT (gtk_builder_get_object (self->builder, "adjustment_from_hours"));
  gtk_adjustment_set_lower (adj, 0);
  gtk_adjustment_set_upper (adj, 23);
  adj = GTK_ADJUSTMENT (gtk_builder_get_object (self->builder, "adjustment_to_hours"));
  gtk_adjustment_set_lower (adj, 0);
  gtk_adjustment_set_upper (adj, 23);

  /* update spinbuttons */
  widget = GTK_WIDGET (gtk_builder_get_object (self->builder, "spinbutton_from_hours"));
  gtk_spin_button_update (GTK_SPIN_BUTTON (widget));
  widget = GTK_WIDGET (gtk_builder_get_object (self->builder, "spinbutton_to_hours"));
  gtk_spin_button_update (GTK_SPIN_BUTTON (widget));

  /* update UI */
  dialog_update_state (self);
  dialog_update_adjustments (self);
}

static void
dialog_am_pm_from_button_clicked_cb (GtkButton *button, CcNightLightDialog *self)
{
  gdouble value;
  value = g_settings_get_double (self->settings_display, "night-light-schedule-from");
  if (value > 12.f)
    value -= 12.f;
  else
    value += 12.f;
  if (value >= 24.f)
    value = fmod (value, 24);
  g_settings_set_double (self->settings_display, "night-light-schedule-from", value);
  g_debug ("new value = %.3f", value);
}

static void
dialog_am_pm_to_button_clicked_cb (GtkButton *button, CcNightLightDialog *self)
{
  gdouble value;
  value = g_settings_get_double (self->settings_display, "night-light-schedule-to");
  if (value > 12.f)
    value -= 12.f;
  else
    value += 12.f;
  if (value >= 24.f)
    value = fmod (value, 24);
  g_settings_set_double (self->settings_display, "night-light-schedule-to", value);
  g_debug ("new value = %.3f", value);
}

static gboolean
dialog_delete_event_cb (GtkWidget *widget,
                        GdkEvent *event,
                        CcNightLightDialog *self)
{
  gtk_widget_hide (widget);
  return TRUE;
}

static void
cc_night_light_dialog_init (CcNightLightDialog *self)
{
  GdkScreen *screen;
  GtkAdjustment *adj;
  GtkBox *box;
  GtkWidget *sw;
  GtkWidget *widget;
  g_autoptr(GError) error = NULL;
  g_autoptr(GtkCssProvider) provider = NULL;

  self->cancellable = g_cancellable_new ();
  self->settings_display = g_settings_new (DISPLAY_SCHEMA);
  g_signal_connect (self->settings_display, "changed",
                    G_CALLBACK (dialog_settings_changed_cb), self);

  self->builder = gtk_builder_new ();
  gtk_builder_add_from_resource (self->builder,
                                 "/org/gnome/control-center/display/display.ui",
                                 &error);

  if (error != NULL)
    {
      g_critical ("Could not load interface file: %s", error->message);
      g_error_free (error);
      return;
    }

  /* connect widgets */
  sw = GTK_WIDGET (gtk_builder_get_object (self->builder, "switch_enable"));
  g_settings_bind (self->settings_display,
                   "night-light-enabled",
                   GTK_SWITCH (sw),
                   "active",
                   G_SETTINGS_BIND_DEFAULT);
  g_signal_connect (sw, "notify::active",
                    G_CALLBACK (dialog_enabled_notify_cb), self);
  g_settings_bind_writable (self->settings_display, "night-light-enabled",
                            sw, "sensitive",
                            FALSE);
  widget = GTK_WIDGET (gtk_builder_get_object (self->builder, "togglebutton_automatic"));
  g_signal_connect (widget, "toggled",
                    G_CALLBACK (dialog_mode_changed_cb), self);
  widget = GTK_WIDGET (gtk_builder_get_object (self->builder, "togglebutton_manual"));
  g_signal_connect (widget, "toggled",
                    G_CALLBACK (dialog_mode_changed_cb), self);
  adj = GTK_ADJUSTMENT (gtk_builder_get_object (self->builder, "adjustment_from_hours"));
  g_signal_connect (adj, "value-changed",
                    G_CALLBACK (dialog_time_from_value_changed_cb), self);
  adj = GTK_ADJUSTMENT (gtk_builder_get_object (self->builder, "adjustment_from_minutes"));
  g_signal_connect (adj, "value-changed",
                    G_CALLBACK (dialog_time_from_value_changed_cb), self);
  adj = GTK_ADJUSTMENT (gtk_builder_get_object (self->builder, "adjustment_to_hours"));
  g_signal_connect (adj, "value-changed",
                    G_CALLBACK (dialog_time_to_value_changed_cb), self);
  adj = GTK_ADJUSTMENT (gtk_builder_get_object (self->builder, "adjustment_to_minutes"));
  g_signal_connect (adj, "value-changed",
                    G_CALLBACK (dialog_time_to_value_changed_cb), self);
  widget = GTK_WIDGET (gtk_builder_get_object (self->builder, "button_undisable"));
  g_signal_connect (widget, "clicked",
                    G_CALLBACK (dialog_undisable_clicked_cb), self);

  widget = GTK_WIDGET (gtk_builder_get_object (self->builder, "button_from_pm"));
  g_signal_connect (widget, "clicked",
                    G_CALLBACK (dialog_am_pm_from_button_clicked_cb), self);
  widget = GTK_WIDGET (gtk_builder_get_object (self->builder, "button_from_am"));
  g_signal_connect (widget, "clicked",
                    G_CALLBACK (dialog_am_pm_from_button_clicked_cb), self);
  widget = GTK_WIDGET (gtk_builder_get_object (self->builder, "button_to_pm"));
  g_signal_connect (widget, "clicked",
                    G_CALLBACK (dialog_am_pm_to_button_clicked_cb), self);
  widget = GTK_WIDGET (gtk_builder_get_object (self->builder, "button_to_am"));
  g_signal_connect (widget, "clicked",
                    G_CALLBACK (dialog_am_pm_to_button_clicked_cb), self);

  self->main_window = GTK_WIDGET (gtk_builder_get_object (self->builder, "window_night_light"));
  g_signal_connect (self->main_window, "delete-event",
                    G_CALLBACK (dialog_delete_event_cb), self);

  /* use custom CSS */
  provider = gtk_css_provider_new ();
  if (!gtk_css_provider_load_from_data (provider,
                                        ".padded-spinbutton {\n"
                                        "    font-size: 110%;\n"
                                        "    min-width: 50px;\n"
                                        "}\n"
                                        ".unpadded-button {\n"
                                        "    padding: 6px;\n"
                                        "}\n",
                                        -1,
                                        &error))
    {
      g_error ("Failed to load CSS: %s", error->message);
    }
  widget = GTK_WIDGET (gtk_builder_get_object (self->builder, "spinbutton_from_hours"));
  screen = gtk_widget_get_screen (widget);
  gtk_style_context_add_provider_for_screen (screen, GTK_STYLE_PROVIDER (provider),
                                             GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

  widget = GTK_WIDGET (gtk_builder_get_object (self->builder, "spinbutton_from_hours"));
  g_signal_connect (widget, "output",
                    G_CALLBACK (dialog_format_hours_combobox), self);
  widget = GTK_WIDGET (gtk_builder_get_object (self->builder, "spinbutton_from_minutes"));
  g_signal_connect (widget, "output",
                    G_CALLBACK (dialog_format_minutes_combobox), self);
  widget = GTK_WIDGET (gtk_builder_get_object (self->builder, "spinbutton_to_hours"));
  g_signal_connect (widget, "output",
                    G_CALLBACK (dialog_format_hours_combobox), self);
  widget = GTK_WIDGET (gtk_builder_get_object (self->builder, "spinbutton_to_minutes"));
  g_signal_connect (widget, "output",
                    G_CALLBACK (dialog_format_minutes_combobox), self);

  /* add custom widget */
  self->night_light_widget = cc_night_light_widget_new ();
  gtk_widget_set_size_request (self->night_light_widget, -1, 40);
  box = GTK_BOX (gtk_builder_get_object (self->builder, "box_content"));
  gtk_box_pack_start (box, self->night_light_widget, FALSE, FALSE, 0);
  gtk_widget_show (self->night_light_widget);

  g_dbus_proxy_new_for_bus (G_BUS_TYPE_SESSION,
                            G_DBUS_PROXY_FLAGS_NONE,
                            NULL,
                            "org.gnome.SettingsDaemon.Color",
                            "/org/gnome/SettingsDaemon/Color",
                            "org.gnome.SettingsDaemon.Color",
                            self->cancellable,
                            dialog_got_proxy_cb,
                            self);

  g_dbus_proxy_new_for_bus (G_BUS_TYPE_SESSION,
                            G_DBUS_PROXY_FLAGS_NONE,
                            NULL,
                            "org.gnome.SettingsDaemon.Color",
                            "/org/gnome/SettingsDaemon/Color",
                            "org.freedesktop.DBus.Properties",
                            self->cancellable,
                            dialog_got_proxy_props_cb,
                            self);

  /* clock settings_display */
  self->settings_clock = g_settings_new (CLOCK_SCHEMA);
  self->clock_format = g_settings_get_enum (self->settings_clock, CLOCK_FORMAT_KEY);
  dialog_update_adjustments (self);
  g_signal_connect (self->settings_clock, "changed::" CLOCK_FORMAT_KEY,
                    G_CALLBACK (dialog_clock_settings_changed_cb), self);

  dialog_update_state (self);
}

CcNightLightDialog *
cc_night_light_dialog_new (void)
{
  return g_object_new (CC_TYPE_NIGHT_LIGHT_DIALOG, NULL);
}

