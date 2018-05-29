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

#include "shell/cc-object-storage.h"

struct _CcNightLightDialog {
  GtkDialog            parent;

  GtkWidget           *box_manual;
  GtkWidget           *infobar_disabled;
  GtkWidget           *night_light_widget;
  GtkWidget           *spinbutton_from_hours;
  GtkWidget           *spinbutton_from_minutes;
  GtkWidget           *spinbutton_to_hours;
  GtkWidget           *spinbutton_to_minutes;
  GtkStack            *stack_from;
  GtkStack            *stack_to;
  GtkWidget           *switch_enable;
  GtkWidget           *togglebutton_automatic;
  GtkWidget           *togglebutton_manual;

  GtkAdjustment       *adjustment_from_hours;
  GtkAdjustment       *adjustment_from_minutes;
  GtkAdjustment       *adjustment_to_hours;
  GtkAdjustment       *adjustment_to_minutes;

  GSettings           *settings_display;
  GSettings           *settings_clock;
  GDBusProxy          *proxy_color;
  GDBusProxy          *proxy_color_props;
  GCancellable        *cancellable;
  gboolean             ignore_value_changed;
  guint                timer_id;
  GDesktopClockFormat  clock_format;
};

G_DEFINE_TYPE (CcNightLightDialog, cc_night_light_dialog, GTK_TYPE_DIALOG);

#define CLOCK_SCHEMA     "org.gnome.desktop.interface"
#define DISPLAY_SCHEMA   "org.gnome.settings-daemon.plugins.color"
#define CLOCK_FORMAT_KEY "clock-format"

static gdouble
frac_day_from_dt (GDateTime *dt)
{
  return g_date_time_get_hour (dt) +
          (gdouble) g_date_time_get_minute (dt) / 60.f +
          (gdouble) g_date_time_get_second (dt) / 3600.f;
}

static void
dialog_adjustments_set_frac_hours (CcNightLightDialog *self,
                                   gdouble             value,
                                   GtkAdjustment      *adj_hours,
                                   GtkAdjustment      *adj_mins,
                                   GtkStack           *stack)
{
  gdouble hours;
  gdouble mins = 0.f;
  gboolean is_pm = FALSE;
  gboolean is_24h;

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
  gtk_adjustment_set_value (GTK_ADJUSTMENT (adj_hours), hours);
  gtk_adjustment_set_value (GTK_ADJUSTMENT (adj_mins), mins);
  self->ignore_value_changed = FALSE;

  if (is_24h)
    gtk_stack_set_visible_child_name (stack, "blank");
  else
    gtk_stack_set_visible_child_name (stack, is_pm ? "pm" : "am");
}

static void
dialog_update_state (CcNightLightDialog *self)
{
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
  gtk_widget_set_visible (self->infobar_disabled, disabled_until_tomorrow);

  /* make things insensitive if the switch is disabled */
  enabled = g_settings_get_boolean (self->settings_display, "night-light-enabled");
  automatic = g_settings_get_boolean (self->settings_display, "night-light-schedule-automatic");

  gtk_widget_set_sensitive (self->night_light_widget, enabled);
  gtk_widget_set_sensitive (self->togglebutton_automatic, enabled);
  gtk_widget_set_sensitive (self->togglebutton_manual, enabled);

  self->ignore_value_changed = TRUE;
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (self->togglebutton_automatic), automatic);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (self->togglebutton_manual), !automatic);
  self->ignore_value_changed = FALSE;

  gtk_widget_set_sensitive (self->box_manual, enabled && !automatic);

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
                                     self->adjustment_from_hours,
                                     self->adjustment_from_minutes,
                                     self->stack_from);
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
                                     self->adjustment_to_hours,
                                     self->adjustment_to_minutes,
                                     self->stack_to);

  cc_night_light_widget_set_to (CC_NIGHT_LIGHT_WIDGET (self->night_light_widget), value);

  /* set new time */
  cc_night_light_widget_set_now (CC_NIGHT_LIGHT_WIDGET (self->night_light_widget), frac_day_from_dt (dt));
}

static gboolean
dialog_tick_cb (gpointer user_data)
{
  CcNightLightDialog *self = (CcNightLightDialog *) user_data;
  dialog_update_state (self);
  return G_SOURCE_CONTINUE;
}

static void
dialog_enabled_notify_cb (GtkSwitch          *sw,
                          GParamSpec         *pspec,
                          CcNightLightDialog *self)
{
  g_settings_set_boolean (self->settings_display, "night-light-enabled",
                          gtk_switch_get_active (sw));
}

static void
dialog_mode_changed_cb (GtkToggleButton    *togglebutton,
                        CcNightLightDialog *self)
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
dialog_undisable_call_cb (GObject      *source_object,
                          GAsyncResult *res,
                          gpointer      user_data)
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
dialog_undisable_clicked_cb (GtkButton          *button,
                             CcNightLightDialog *self)
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
                                   GtkAdjustment      *adj_hours,
                                   GtkAdjustment      *adj_mins,
                                   GtkStack           *stack)
{
  gdouble value;

  value = gtk_adjustment_get_value (adj_hours);
  value += gtk_adjustment_get_value (adj_mins) / 60.0f;

  if (g_strcmp0 (gtk_stack_get_visible_child_name (stack), "pm") == 0)
    value += 12.f;

  return value;
}

static void
dialog_time_from_value_changed_cb (GtkAdjustment      *adjustment,
                                   CcNightLightDialog *self)
{
  gdouble value;

  if (self->ignore_value_changed)
    return;

  value = dialog_adjustments_get_frac_hours (self,
                                             self->adjustment_from_hours,
                                             self->adjustment_from_minutes,
                                             self->stack_from);

  if (value >= 24.f)
    value = fmod (value, 24);

  g_debug ("new value = %.3f", value);

  g_settings_set_double (self->settings_display, "night-light-schedule-from", value);
}

static void
dialog_time_to_value_changed_cb (GtkAdjustment      *adjustment,
                                 CcNightLightDialog *self)
{
  gdouble value;

  if (self->ignore_value_changed)
    return;

  value = dialog_adjustments_get_frac_hours (self,
                                             self->adjustment_to_hours,
                                             self->adjustment_to_minutes,
                                             self->stack_to);
  if (value >= 24.f)
    value = fmod (value, 24);

  g_debug ("new value = %.3f", value);

  g_settings_set_double (self->settings_display, "night-light-schedule-to", value);
}

static void
dialog_color_properties_changed_cb (GDBusProxy         *proxy,
                                    GVariant           *changed_properties,
                                    GStrv               invalidated_properties,
                                    CcNightLightDialog *self)
{
  dialog_update_state (self);
}

static void
dialog_got_proxy_cb (GObject      *source_object,
                     GAsyncResult *res,
                     gpointer      user_data)
{
  CcNightLightDialog *self = (CcNightLightDialog *) user_data;
  GDBusProxy *proxy;
  g_autoptr(GError) error = NULL;

  proxy = cc_object_storage_create_dbus_proxy_finish (res, &error);
  if (proxy == NULL)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_warning ("failed to connect to g-s-d: %s", error->message);
      return;
    }

  self->proxy_color = proxy;

  g_signal_connect_object (self->proxy_color, "g-properties-changed",
                           G_CALLBACK (dialog_color_properties_changed_cb), self, 0);
  dialog_update_state (self);
  self->timer_id = g_timeout_add_seconds (10, dialog_tick_cb, self);
}

static void
dialog_got_proxy_props_cb (GObject      *source_object,
                           GAsyncResult *res,
                           gpointer      user_data)
{
  CcNightLightDialog *self = (CcNightLightDialog *) user_data;
  GDBusProxy *proxy;
  g_autoptr(GError) error = NULL;

  proxy = cc_object_storage_create_dbus_proxy_finish (res, &error);
  if (proxy == NULL)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_warning ("failed to connect to g-s-d: %s", error->message);
      return;
    }

  self->proxy_color_props = proxy;
}

static gboolean
dialog_format_minutes_combobox (GtkSpinButton      *spin,
                                CcNightLightDialog *self)
{
  GtkAdjustment *adjustment;
  g_autofree gchar *text = NULL;
  adjustment = gtk_spin_button_get_adjustment (spin);
  text = g_strdup_printf ("%02.0f", gtk_adjustment_get_value (adjustment));
  gtk_entry_set_text (GTK_ENTRY (spin), text);
  return TRUE;
}

static gboolean
dialog_format_hours_combobox (GtkSpinButton      *spin,
                              CcNightLightDialog *self)
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
  /* from */
  if (self->clock_format == G_DESKTOP_CLOCK_FORMAT_24H)
    {
      gtk_adjustment_set_lower (self->adjustment_from_hours, 0);
      gtk_adjustment_set_upper (self->adjustment_from_hours, 23);
    }
  else
    {
      if (gtk_adjustment_get_value (self->adjustment_from_hours) > 12)
          gtk_stack_set_visible_child_name (self->stack_from, "pm");

      gtk_adjustment_set_lower (self->adjustment_from_hours, 1);
      gtk_adjustment_set_upper (self->adjustment_from_hours, 12);
    }

  /* to */
  if (self->clock_format == G_DESKTOP_CLOCK_FORMAT_24H)
    {
      gtk_adjustment_set_lower (self->adjustment_to_hours, 0);
      gtk_adjustment_set_upper (self->adjustment_to_hours, 23);
    }
  else
    {
      if (gtk_adjustment_get_value (self->adjustment_to_hours) > 12)
          gtk_stack_set_visible_child_name (self->stack_to, "pm");

      gtk_adjustment_set_lower (self->adjustment_to_hours, 1);
      gtk_adjustment_set_upper (self->adjustment_to_hours, 12);
    }
}

static void
dialog_settings_changed_cb (GSettings          *settings_display,
                            gchar              *key,
                            CcNightLightDialog *self)
{
  dialog_update_state (self);
}

static void
dialog_clock_settings_changed_cb (GSettings          *settings_display,
                                  gchar              *key,
                                  CcNightLightDialog *self)
{
  self->clock_format = g_settings_get_enum (settings_display, CLOCK_FORMAT_KEY);

  /* uncontionally widen this to avoid truncation */
  gtk_adjustment_set_lower (self->adjustment_from_hours, 0);
  gtk_adjustment_set_upper (self->adjustment_from_hours, 23);
  gtk_adjustment_set_lower (self->adjustment_to_hours, 0);
  gtk_adjustment_set_upper (self->adjustment_to_hours, 23);

  /* update spinbuttons */
  gtk_spin_button_update (GTK_SPIN_BUTTON (self->spinbutton_from_hours));
  gtk_spin_button_update (GTK_SPIN_BUTTON (self->spinbutton_to_hours));

  /* update UI */
  dialog_update_state (self);
  dialog_update_adjustments (self);
}

static void
dialog_am_pm_from_button_clicked_cb (GtkButton          *button,
                                     CcNightLightDialog *self)
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
dialog_am_pm_to_button_clicked_cb (GtkButton          *button,
                                   CcNightLightDialog *self)
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

/* GObject overrides */
static void
cc_night_light_dialog_finalize (GObject *object)
{
  CcNightLightDialog *self = CC_NIGHT_LIGHT_DIALOG (object);

  g_cancellable_cancel (self->cancellable);

  g_clear_object (&self->cancellable);
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
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = cc_night_light_dialog_finalize;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/display/cc-night-light-dialog.ui");

  gtk_widget_class_bind_template_child (widget_class, CcNightLightDialog, adjustment_from_hours);
  gtk_widget_class_bind_template_child (widget_class, CcNightLightDialog, adjustment_from_minutes);
  gtk_widget_class_bind_template_child (widget_class, CcNightLightDialog, adjustment_to_hours);
  gtk_widget_class_bind_template_child (widget_class, CcNightLightDialog, adjustment_to_minutes);
  gtk_widget_class_bind_template_child (widget_class, CcNightLightDialog, box_manual);
  gtk_widget_class_bind_template_child (widget_class, CcNightLightDialog, infobar_disabled);
  gtk_widget_class_bind_template_child (widget_class, CcNightLightDialog, night_light_widget);
  gtk_widget_class_bind_template_child (widget_class, CcNightLightDialog, spinbutton_from_hours);
  gtk_widget_class_bind_template_child (widget_class, CcNightLightDialog, spinbutton_from_minutes);
  gtk_widget_class_bind_template_child (widget_class, CcNightLightDialog, spinbutton_to_hours);
  gtk_widget_class_bind_template_child (widget_class, CcNightLightDialog, spinbutton_to_minutes);
  gtk_widget_class_bind_template_child (widget_class, CcNightLightDialog, stack_from);
  gtk_widget_class_bind_template_child (widget_class, CcNightLightDialog, stack_to);
  gtk_widget_class_bind_template_child (widget_class, CcNightLightDialog, switch_enable);
  gtk_widget_class_bind_template_child (widget_class, CcNightLightDialog, togglebutton_automatic);
  gtk_widget_class_bind_template_child (widget_class, CcNightLightDialog, togglebutton_manual);

  gtk_widget_class_bind_template_callback (widget_class, dialog_am_pm_from_button_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, dialog_am_pm_to_button_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, dialog_enabled_notify_cb);
  gtk_widget_class_bind_template_callback (widget_class, dialog_format_hours_combobox);
  gtk_widget_class_bind_template_callback (widget_class, dialog_format_minutes_combobox);
  gtk_widget_class_bind_template_callback (widget_class, dialog_mode_changed_cb);
  gtk_widget_class_bind_template_callback (widget_class, dialog_time_from_value_changed_cb);
  gtk_widget_class_bind_template_callback (widget_class, dialog_time_to_value_changed_cb);
  gtk_widget_class_bind_template_callback (widget_class, dialog_undisable_clicked_cb);

  g_type_ensure (CC_TYPE_NIGHT_LIGHT_WIDGET);
}

static void
cc_night_light_dialog_init (CcNightLightDialog *self)
{
  g_autoptr(GtkCssProvider) provider = NULL;
  g_autoptr(GError) error = NULL;

  gtk_widget_init_template (GTK_WIDGET (self));

  self->cancellable = g_cancellable_new ();
  self->settings_display = g_settings_new (DISPLAY_SCHEMA);

  g_signal_connect (self->settings_display, "changed", G_CALLBACK (dialog_settings_changed_cb), self);

  /* connect widgets */
  g_settings_bind (self->settings_display,
                   "night-light-enabled",
                   self->switch_enable,
                   "active",
                   G_SETTINGS_BIND_DEFAULT);

  g_settings_bind_writable (self->settings_display, "night-light-enabled",
                            self->switch_enable, "sensitive",
                            FALSE);


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

  gtk_style_context_add_provider_for_screen (gdk_screen_get_default (),
                                             GTK_STYLE_PROVIDER (provider),
                                             GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

  cc_object_storage_create_dbus_proxy (G_BUS_TYPE_SESSION,
                                       G_DBUS_PROXY_FLAGS_NONE,
                                       "org.gnome.SettingsDaemon.Color",
                                       "/org/gnome/SettingsDaemon/Color",
                                       "org.gnome.SettingsDaemon.Color",
                                       self->cancellable,
                                       dialog_got_proxy_cb,
                                       self);

  cc_object_storage_create_dbus_proxy (G_BUS_TYPE_SESSION,
                                       G_DBUS_PROXY_FLAGS_NONE,
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
  g_signal_connect (self->settings_clock,
                    "changed::" CLOCK_FORMAT_KEY,
                    G_CALLBACK (dialog_clock_settings_changed_cb),
                    self);

  dialog_update_state (self);
}

CcNightLightDialog *
cc_night_light_dialog_new (void)
{
  return g_object_new (CC_TYPE_NIGHT_LIGHT_DIALOG,
                       "use-header-bar", TRUE,
                       NULL);
}

