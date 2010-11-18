/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*-
 *
 * Copyright (C) 2010 Red Hat, Inc
 * Copyright (C) 2008 William Jon McCann <jmccann@redhat.com>
 * Copyright (C) 2010 Richard Hughes <richard@hughsie.com>
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 */

#include <libupower-glib/upower.h>
#include <glib/gi18n.h>

#include "cc-power-panel.h"

#define WID(b, w) (GtkWidget *) gtk_builder_get_object (b, w)

G_DEFINE_DYNAMIC_TYPE (CcPowerPanel, cc_power_panel, CC_TYPE_PANEL)

#define POWER_PANEL_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), CC_TYPE_POWER_PANEL, CcPowerPanelPrivate))

struct _CcPowerPanelPrivate
{
  GSettings     *lock_settings;
  GSettings     *gsd_settings;
  GCancellable  *cancellable;
  GtkBuilder    *builder;
  GDBusProxy    *proxy;
  UpClient      *up_client;
};


static void
cc_power_panel_get_property (GObject    *object,
                              guint       property_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
  switch (property_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
cc_power_panel_set_property (GObject      *object,
                              guint         property_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  switch (property_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
cc_power_panel_dispose (GObject *object)
{
  CcPowerPanelPrivate *priv = CC_POWER_PANEL (object)->priv;

  if (priv->gsd_settings)
    {
      g_object_unref (priv->gsd_settings);
      priv->gsd_settings = NULL;
    }
  if (priv->cancellable != NULL)
    {
      g_object_unref (priv->cancellable);
      priv->cancellable = NULL;
    }
  if (priv->builder != NULL)
    {
      g_object_unref (priv->builder);
      priv->builder = NULL;
    }
  if (priv->proxy != NULL)
    {
      g_object_unref (priv->proxy);
      priv->proxy = NULL;
    }
  if (priv->up_client != NULL)
    {
      g_object_unref (priv->up_client);
      priv->up_client = NULL;
    }

  G_OBJECT_CLASS (cc_power_panel_parent_class)->dispose (object);
}

static void
cc_power_panel_finalize (GObject *object)
{
  CcPowerPanelPrivate *priv = CC_POWER_PANEL (object)->priv;
  g_cancellable_cancel (priv->cancellable);
  G_OBJECT_CLASS (cc_power_panel_parent_class)->finalize (object);
}

static void
on_lock_settings_changed (GSettings     *settings,
                          const char    *key,
                          CcPowerPanel *panel)
{
}

static void
cc_power_panel_class_init (CcPowerPanelClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  g_type_class_add_private (klass, sizeof (CcPowerPanelPrivate));

  object_class->get_property = cc_power_panel_get_property;
  object_class->set_property = cc_power_panel_set_property;
  object_class->dispose = cc_power_panel_dispose;
  object_class->finalize = cc_power_panel_finalize;
}

static void
cc_power_panel_class_finalize (CcPowerPanelClass *klass)
{
}

static gchar *
get_timestring (guint64 time_secs)
{
  gchar* timestring = NULL;
  gint  hours;
  gint  minutes;

  /* Add 0.5 to do rounding */
  minutes = (int) ( ( time_secs / 60.0 ) + 0.5 );

  if (minutes == 0)
    {
      timestring = g_strdup (_("Unknown time"));
      return timestring;
    }

  if (minutes < 60)
    {
      timestring = g_strdup_printf (ngettext ("%i minute",
                                    "%i minutes",
                                    minutes), minutes);
      return timestring;
    }

  hours = minutes / 60;
  minutes = minutes % 60;

  if (minutes == 0)
    {
      timestring = g_strdup_printf (ngettext (
                                    "%i hour",
                                    "%i hours",
                                    hours), hours);
      return timestring;
    }

  /* TRANSLATOR: "%i %s %i %s" are "%i hours %i minutes"
   * Swap order with "%2$s %2$i %1$s %1$i if needed */
  timestring = g_strdup_printf (_("%i %s %i %s"),
                                hours, ngettext ("hour", "hours", hours),
                                minutes, ngettext ("minute", "minutes", minutes));
  return timestring;
}

static void
get_primary_device_cb (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  const gchar *title = NULL;
  gchar *details = NULL;
  gchar *icon_name = NULL;
  gchar *object_path = NULL;
  gdouble percentage;
  GError *error = NULL;
  GtkWidget *widget;
  guint64 time;
  gchar *time_string = NULL;
  GVariant *result;
  UpDeviceKind kind;
  UpDeviceState state;
  GIcon *icon;
  CcPowerPanelPrivate *priv = CC_POWER_PANEL (user_data)->priv;

  result = g_dbus_proxy_call_finish (G_DBUS_PROXY (source_object), res, &error);
  if (result == NULL)
    {
      g_printerr ("Error getting primary device: %s\n", error->message);
      g_error_free (error);
      widget = GTK_WIDGET (gtk_builder_get_object (priv->builder,
                                                   "hbox_status"));
      gtk_widget_hide (widget);
      return;
    }

  /* set the icon and text */
  g_variant_get (result,
                 "((susdut))",
                 &object_path,
                 &kind,
                 &icon_name,
                 &percentage,
                 &state,
                 &time);

  g_debug ("got data from object %s", object_path);

  /* set icon and text parameters */
  widget = GTK_WIDGET (gtk_builder_get_object (priv->builder,
                                               "image_status"));
  icon = g_icon_new_for_string (icon_name, NULL);
  if (icon != NULL)
    {
      gtk_image_set_from_gicon (GTK_IMAGE (widget),
                                icon,
                                GTK_ICON_SIZE_DIALOG);
      g_object_unref (icon);
    }
  else
    {
      gtk_image_set_from_icon_name (GTK_IMAGE (widget),
                                    "dialog-error",
                                    GTK_ICON_SIZE_DIALOG);
    }
  widget = GTK_WIDGET (gtk_builder_get_object (priv->builder,
                                               "label_title"));

  /* translate the title, which has limited entries as devices that are
   * fully charged are not returned as the primary device */
  if (kind == UP_DEVICE_KIND_BATTERY)
    {
      switch (state)
        {
          case UP_DEVICE_STATE_CHARGING:
            title = _("Battery charging");
            break;
          case UP_DEVICE_STATE_DISCHARGING:
            title = _("Battery discharging");
            break;
          default:
            break;
        }
    }
  else if (kind == UP_DEVICE_KIND_UPS)
    {
      switch (state)
        {
          case UP_DEVICE_STATE_CHARGING:
            title = _("UPS charging");
            break;
          case UP_DEVICE_STATE_DISCHARGING:
            title = _("UPS discharging");
            break;
          default:
            break;
        }
    }
  gtk_label_set_label (GTK_LABEL (widget),
                       title != NULL ? title : "");
  gtk_widget_set_visible (widget, (title != NULL));

  /* get the description */
  if (time > 0)
    {
      time_string = get_timestring (time);

      if (state == UP_DEVICE_STATE_CHARGING)
        {
          /* TRANSLATORS: %1 is a time string, e.g. "1 hour 5 minutes" */
          details = g_strdup_printf(_("%s until charged (%.0lf%%)"),
                                    time_string, percentage);
        }
      else if (state == UP_DEVICE_STATE_DISCHARGING)
        {
          /* TRANSLATORS: %1 is a time string, e.g. "1 hour 5 minutes" */
          details = g_strdup_printf(_("%s until empty (%.0lf%%)"),
                                    time_string, percentage);
        }
    }
  else
    {
      /* TRANSLATORS: %1 is a percentage value. Note: this string is only
       * used when we don't have a time value */
      details = g_strdup_printf(_("%.0lf%% charged"),
                                percentage);
    }
  widget = GTK_WIDGET (gtk_builder_get_object (priv->builder,
                                               "label_description"));
  gtk_label_set_label (GTK_LABEL (widget),
                       details);

  g_free (details);
  g_free (time_string);
  g_free (object_path);
  g_free (icon_name);
  g_variant_unref (result);
}

static void
on_signal (GDBusProxy *proxy,
           gchar      *sender_name,
           gchar      *signal_name,
           GVariant   *parameters,
           gpointer    user_data)
{
  CcPowerPanelPrivate *priv = CC_POWER_PANEL (user_data)->priv;

  if (g_strcmp0 (signal_name, "Changed") == 0)
    {
      /* get the new state */
      g_dbus_proxy_call (priv->proxy,
                         "GetPrimaryDevice",
                         NULL,
                         G_DBUS_CALL_FLAGS_NONE,
                         -1,
                         priv->cancellable,
                         get_primary_device_cb,
                         user_data);
    }
}

static void
got_power_proxy_cb (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  GError *error = NULL;
  CcPowerPanelPrivate *priv = CC_POWER_PANEL (user_data)->priv;

  priv->proxy = g_dbus_proxy_new_for_bus_finish (res, &error);
  if (priv->proxy == NULL)
    {
      g_printerr ("Error creating proxy: %s\n", error->message);
      g_error_free (error);
      return;
    }

  /* we want to change the primary device changes */
  g_signal_connect (priv->proxy,
                    "g-signal",
                    G_CALLBACK (on_signal),
                    user_data);

  /* get the initial state */
  g_dbus_proxy_call (priv->proxy,
                     "GetPrimaryDevice",
                     NULL,
                     G_DBUS_CALL_FLAGS_NONE,
                     200, /* we don't want to randomly expand the dialog */
                     priv->cancellable,
                     get_primary_device_cb,
                     user_data);
}

static void
combo_time_changed_cb (GtkWidget *widget, CcPowerPanel *self)
{
  GtkTreeIter iter;
  GtkTreeModel *model;
  gint value;
  gboolean ret;
  const gchar *key = (const gchar *)g_object_get_data (G_OBJECT(widget), "_gsettings_key");

  /* no selection */
  ret = gtk_combo_box_get_active_iter (GTK_COMBO_BOX(widget), &iter);
  if (!ret)
    return;

  /* get entry */
  model = gtk_combo_box_get_model (GTK_COMBO_BOX(widget));
  gtk_tree_model_get (model, &iter,
                      1, &value,
                      -1);

  /* set both battery and ac keys */
  g_settings_set_int (self->priv->gsd_settings, key, value);
}

static void
combo_enum_changed_cb (GtkWidget *widget, CcPowerPanel *self)
{
  GtkTreeIter iter;
  GtkTreeModel *model;
  gint value;
  gboolean ret;
  const gchar *key = (const gchar *)g_object_get_data (G_OBJECT(widget), "_gsettings_key");

  /* no selection */
  ret = gtk_combo_box_get_active_iter (GTK_COMBO_BOX(widget), &iter);
  if (!ret)
    return;

  /* get entry */
  model = gtk_combo_box_get_model (GTK_COMBO_BOX(widget));
  gtk_tree_model_get (model, &iter,
                      1, &value,
                      -1);

  /* set both battery and ac keys */
  g_settings_set_enum (self->priv->gsd_settings, key, value);
}

static void
set_value_for_combo (GtkComboBox *combo_box, gint value)
{
  GtkTreeIter iter;
  GtkTreeModel *model;
  gint value_tmp;
  gboolean ret;

  /* get entry */
  model = gtk_combo_box_get_model (combo_box);
  ret = gtk_tree_model_get_iter_first (model, &iter);
  if (!ret)
    return;

  /* try to make the UI match the setting */
  do
    {
      gtk_tree_model_get (model, &iter,
                          1, &value_tmp,
                          -1);
      if (value == value_tmp)
        {
          gtk_combo_box_set_active_iter (combo_box, &iter);
          break;
        }
    } while (gtk_tree_model_iter_next (model, &iter));
}

static void
set_ac_battery_ui_mode (CcPowerPanel *self)
{
  gboolean has_batteries = FALSE;
  gboolean ret;
  GError *error = NULL;
  GPtrArray *devices;
  GtkWidget *widget;
  guint i;
  UpDevice *device;
  UpDeviceKind kind;
  CcPowerPanelPrivate *priv = self->priv;

  /* this is sync, but it's cached in the daemon and so quick */
  ret = up_client_enumerate_devices_sync (self->priv->up_client, NULL, &error);
  if (!ret)
    {
      g_warning ("failed to get device list: %s", error->message);
      g_error_free (error);
      goto out;
    }

  devices = up_client_get_devices (self->priv->up_client);
  for (i=0; i<devices->len; i++)
    {
      device = g_ptr_array_index (devices, i);
      g_object_get (device,
                    "kind", &kind,
                    NULL);
      if (kind == UP_DEVICE_KIND_BATTERY ||
          kind == UP_DEVICE_KIND_UPS)
        {
          has_batteries = TRUE;
          break;
        }
    }
  g_ptr_array_unref (devices);
out:
  widget = GTK_WIDGET (gtk_builder_get_object (priv->builder,
                                               "vbox_battery"));
  gtk_widget_set_visible (widget, has_batteries);
  widget = GTK_WIDGET (gtk_builder_get_object (priv->builder,
                                               "hbox_ac"));
  gtk_widget_set_visible (widget, !has_batteries);
}

static void
cc_power_panel_init (CcPowerPanel *self)
{
  GError     *error;
  GtkWidget  *widget;
  GtkWidget  *target;
  gint        value;

  self->priv = POWER_PANEL_PRIVATE (self);

  self->priv->builder = gtk_builder_new ();

  error = NULL;
  gtk_builder_add_from_file (self->priv->builder,
                             GNOMECC_UI_DIR "/power.ui",
                             &error);

  if (error != NULL)
    {
      g_warning ("Could not load interface file: %s", error->message);
      g_error_free (error);
      return;
    }

  self->priv->cancellable = g_cancellable_new ();

  /* get initial icon state */
  g_dbus_proxy_new_for_bus (G_BUS_TYPE_SESSION,
                            G_DBUS_PROXY_FLAGS_NONE,
                            NULL,
                            "org.gnome.PowerManager",
                            "/org/gnome/PowerManager",
                            "org.gnome.PowerManager",
                            self->priv->cancellable,
                            got_power_proxy_cb,
                            self);

  /* find out if there are any battery or UPS devices attached
   * and setup UI accordingly */
  self->priv->up_client = up_client_new ();
  set_ac_battery_ui_mode (self);

  self->priv->gsd_settings = g_settings_new ("org.gnome.settings-daemon.plugins.power");
  g_signal_connect (self->priv->gsd_settings,
                    "changed",
                    G_CALLBACK (on_lock_settings_changed),
                    self);

  /* setup the checkboxes correcty */
  widget = GTK_WIDGET (gtk_builder_get_object (self->priv->builder,
                                               "checkbutton_sleep_ac"));
  target = GTK_WIDGET (gtk_builder_get_object (self->priv->builder,
                                               "combobox_sleep_ac"));
  g_object_bind_property (widget, "active",
                          target, "sensitive",
                          G_BINDING_SYNC_CREATE);
  widget = GTK_WIDGET (gtk_builder_get_object (self->priv->builder,
                                               "checkbutton_sleep_battery"));
  target = GTK_WIDGET (gtk_builder_get_object (self->priv->builder,
                                               "combobox_sleep_battery"));
  g_object_bind_property (widget, "active",
                          target, "sensitive",
                          G_BINDING_SYNC_CREATE);
  widget = GTK_WIDGET (gtk_builder_get_object (self->priv->builder,
                                               "checkbutton_sleep"));
  target = GTK_WIDGET (gtk_builder_get_object (self->priv->builder,
                                               "combobox_sleep"));
  g_object_bind_property (widget, "active",
                          target, "sensitive",
                          G_BINDING_SYNC_CREATE);

  /* bind the checkboxes */
  widget = GTK_WIDGET (gtk_builder_get_object (self->priv->builder,
                                               "checkbutton_sleep_ac"));
  g_settings_bind (self->priv->gsd_settings,
                   "sleep-inactive-ac",
                   widget, "active",
                   G_SETTINGS_BIND_DEFAULT);
  widget = GTK_WIDGET (gtk_builder_get_object (self->priv->builder,
                                               "checkbutton_sleep_battery"));
  g_settings_bind (self->priv->gsd_settings,
                   "sleep-inactive-battery",
                   widget, "active",
                   G_SETTINGS_BIND_DEFAULT);
  widget = GTK_WIDGET (gtk_builder_get_object (self->priv->builder,
                                               "checkbutton_sleep"));
  g_settings_bind (self->priv->gsd_settings,
                   "sleep-inactive-ac",
                   widget, "active",
                   G_SETTINGS_BIND_DEFAULT);

  /* auto-sleep time */
  value = g_settings_get_int (self->priv->gsd_settings, "sleep-inactive-ac-timeout");
  widget = GTK_WIDGET (gtk_builder_get_object (self->priv->builder,
                                               "combobox_sleep_ac"));
  set_value_for_combo (GTK_COMBO_BOX (widget), value);
  g_object_set_data (G_OBJECT(widget), "_gsettings_key", "sleep-inactive-ac-timeout");
  g_signal_connect (widget, "changed",
                    G_CALLBACK (combo_time_changed_cb),
                    self);
  widget = GTK_WIDGET (gtk_builder_get_object (self->priv->builder,
                                               "combobox_sleep"));
  set_value_for_combo (GTK_COMBO_BOX (widget), value);
  g_object_set_data (G_OBJECT(widget), "_gsettings_key", "sleep-inactive-ac-timeout");
  g_signal_connect (widget, "changed",
                    G_CALLBACK (combo_time_changed_cb),
                    self);
  value = g_settings_get_int (self->priv->gsd_settings, "sleep-inactive-battery-timeout");
  widget = GTK_WIDGET (gtk_builder_get_object (self->priv->builder,
                                               "combobox_sleep_battery"));
  set_value_for_combo (GTK_COMBO_BOX (widget), value);
  g_object_set_data (G_OBJECT(widget), "_gsettings_key", "sleep-inactive-battery-timeout");
  g_signal_connect (widget, "changed",
                    G_CALLBACK (combo_time_changed_cb),
                    self);

  /* actions */
  value = g_settings_get_enum (self->priv->gsd_settings, "critical-battery-action");
  widget = GTK_WIDGET (gtk_builder_get_object (self->priv->builder,
                                               "combobox_critical"));
  set_value_for_combo (GTK_COMBO_BOX (widget), value);
  g_object_set_data (G_OBJECT(widget), "_gsettings_key", "critical-battery-action");
  g_signal_connect (widget, "changed",
                    G_CALLBACK (combo_enum_changed_cb),
                    self);

  value = g_settings_get_enum (self->priv->gsd_settings, "button-power");
  widget = GTK_WIDGET (gtk_builder_get_object (self->priv->builder,
                                               "combobox_button_power"));
  set_value_for_combo (GTK_COMBO_BOX (widget), value);
  g_object_set_data (G_OBJECT(widget), "_gsettings_key", "button-power");
  g_signal_connect (widget, "changed",
                    G_CALLBACK (combo_enum_changed_cb),
                    self);

  value = g_settings_get_enum (self->priv->gsd_settings, "button-sleep");
  widget = GTK_WIDGET (gtk_builder_get_object (self->priv->builder,
                                               "combobox_button_sleep"));
  set_value_for_combo (GTK_COMBO_BOX (widget), value);
  g_object_set_data (G_OBJECT(widget), "_gsettings_key", "button-sleep");
  g_signal_connect (widget, "changed",
                    G_CALLBACK (combo_enum_changed_cb),
                    self);

  widget = WID (self->priv->builder, "vbox_power");
  gtk_widget_reparent (widget, (GtkWidget *) self);
}

void
cc_power_panel_register (GIOModule *module)
{
  cc_power_panel_register_type (G_TYPE_MODULE (module));
  g_io_extension_point_implement (CC_SHELL_PANEL_EXTENSION_POINT,
                                  CC_TYPE_POWER_PANEL,
                                  "power", 0);
}

