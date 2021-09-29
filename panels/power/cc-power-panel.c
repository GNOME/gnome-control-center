/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*-
 *
 * Copyright (C) 2010 Red Hat, Inc
 * Copyright (C) 2008 William Jon McCann <jmccann@redhat.com>
 * Copyright (C) 2010,2015 Richard Hughes <richard@hughsie.com>
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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 */

#include <config.h>

#include <libupower-glib/upower.h>
#include <glib/gi18n.h>
#include <gnome-settings-daemon/gsd-enums.h>
#include <gio/gdesktopappinfo.h>
#include <handy.h>

#include "shell/cc-object-storage.h"
#include "list-box-helper.h"
#include "cc-battery-row.h"
#include "cc-power-profile-row.h"
#include "cc-power-profile-info-row.h"
#include "cc-power-panel.h"
#include "cc-power-resources.h"
#include "cc-util.h"

/* Uncomment this to test the behaviour of the panel in
 * battery-less situations:
 *
 * #define TEST_NO_BATTERIES
 */

/* Uncomment this to test the behaviour of the panel with
 * multiple appearing devices
 *
 * #define TEST_FAKE_DEVICES
 */

/* Uncomment this to test the behaviour of a desktop machine
 * with a UPS
 *
 * #define TEST_UPS
 */

struct _CcPowerPanel
{
  CcPanel            parent_instance;

  GtkListBoxRow     *als_row;
  GtkSwitch         *als_switch;
  GtkDialog         *automatic_suspend_dialog;
  GtkLabel          *automatic_suspend_label;
  GtkListBoxRow     *automatic_suspend_row;
  GtkListBox        *battery_listbox;
  HdyActionRow      *battery_percentage_row;
  GtkSwitch         *battery_percentage_switch;
  GtkSizeGroup      *battery_row_sizegroup;
  HdyPreferencesGroup *battery_section;
  HdyComboRow       *blank_screen_row;
  GtkListBox        *device_listbox;
  HdyPreferencesGroup *device_section;
  GtkListBoxRow     *dim_screen_row;
  GtkSwitch         *dim_screen_switch;
  HdyPreferencesGroup *general_section;
  GtkSizeGroup      *level_sizegroup;
  HdyComboRow       *power_button_row;
  GtkListBox        *power_profile_listbox;
  GtkListBox        *power_profile_info_listbox;
  HdyPreferencesGroup *power_profile_section;
  HdyActionRow      *power_saver_low_battery_row;
  GtkSwitch         *power_saver_low_battery_switch;
  GtkSizeGroup      *row_sizegroup;
  GtkComboBox       *suspend_on_battery_delay_combo;
  GtkLabel          *suspend_on_battery_delay_label;
  GtkLabel          *suspend_on_battery_label;
  GtkSwitch         *suspend_on_battery_switch;
  GtkComboBox       *suspend_on_ac_delay_combo;
  GtkLabel          *suspend_on_ac_label;
  GtkSwitch         *suspend_on_ac_switch;

  GSettings     *gsd_settings;
  GSettings     *session_settings;
  GSettings     *interface_settings;
  UpClient      *up_client;
  GPtrArray     *devices;
  gboolean       has_batteries;
  char          *chassis_type;

  GDBusProxy    *iio_proxy;
  guint          iio_proxy_watch_id;
  gboolean       has_brightness;

  GDBusProxy    *power_profiles_proxy;
  guint          power_profiles_prop_id;
  CcPowerProfileRow *power_profiles_row[NUM_CC_POWER_PROFILES];
  gboolean       power_profiles_in_update;
  gboolean       has_performance_degraded;
};

CC_PANEL_REGISTER (CcPowerPanel, cc_power_panel)

enum
{
  ACTION_MODEL_TEXT,
  ACTION_MODEL_VALUE
};

static void
cc_power_panel_dispose (GObject *object)
{
  CcPowerPanel *self = CC_POWER_PANEL (object);

  g_clear_pointer (&self->chassis_type, g_free);
  g_clear_object (&self->gsd_settings);
  g_clear_object (&self->session_settings);
  g_clear_object (&self->interface_settings);
  g_clear_pointer ((GtkWidget **) &self->automatic_suspend_dialog, gtk_widget_destroy);
  g_clear_pointer (&self->devices, g_ptr_array_unref);
  g_clear_object (&self->up_client);
  g_clear_object (&self->iio_proxy);
  g_clear_object (&self->power_profiles_proxy);
  if (self->iio_proxy_watch_id != 0)
    g_bus_unwatch_name (self->iio_proxy_watch_id);
  self->iio_proxy_watch_id = 0;

  G_OBJECT_CLASS (cc_power_panel_parent_class)->dispose (object);
}

static const char *
cc_power_panel_get_help_uri (CcPanel *panel)
{
  return "help:gnome-help/power";
}

static char *
get_chassis_type (GCancellable *cancellable)
{
  g_autoptr(GError) error = NULL;
  g_autoptr(GVariant) inner = NULL;
  g_autoptr(GVariant) variant = NULL;
  g_autoptr(GDBusConnection) connection = NULL;

  connection = g_bus_get_sync (G_BUS_TYPE_SYSTEM,
                               cancellable,
                               &error);
  if (!connection)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_warning ("system bus not available: %s", error->message);
      return NULL;
    }

  variant = g_dbus_connection_call_sync (connection,
                                         "org.freedesktop.hostname1",
                                         "/org/freedesktop/hostname1",
                                         "org.freedesktop.DBus.Properties",
                                         "Get",
                                         g_variant_new ("(ss)",
                                                        "org.freedesktop.hostname1",
                                                        "Chassis"),
                                         NULL,
                                         G_DBUS_CALL_FLAGS_NONE,
                                         -1,
                                         cancellable,
                                         &error);
  if (!variant)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_debug ("Failed to get property '%s': %s", "Chassis", error->message);
      return NULL;
    }

  g_variant_get (variant, "(v)", &inner);
  return g_variant_dup_string (inner, NULL);
}

static void
load_custom_css (CcPowerPanel *self,
                 const char   *path)
{
  g_autoptr(GtkCssProvider) provider = NULL;

  /* use custom CSS */
  provider = gtk_css_provider_new ();
  gtk_css_provider_load_from_resource (provider, path);
  gtk_style_context_add_provider_for_screen (gdk_screen_get_default (),
                                             GTK_STYLE_PROVIDER (provider),
                                             GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
}

static void
add_battery (CcPowerPanel *panel, UpDevice *device, gboolean primary)
{
  CcBatteryRow *row = cc_battery_row_new (device, primary);
  cc_battery_row_set_level_sizegroup (row, panel->level_sizegroup);
  cc_battery_row_set_row_sizegroup (row, panel->battery_row_sizegroup);

  gtk_container_add (GTK_CONTAINER (panel->battery_listbox), GTK_WIDGET (row));
  gtk_widget_set_visible (GTK_WIDGET (panel->battery_section), TRUE);
}

static void
add_device (CcPowerPanel *self, UpDevice *device)
{
  CcBatteryRow *row = cc_battery_row_new (device, FALSE);
  cc_battery_row_set_level_sizegroup (row, self->level_sizegroup);
  cc_battery_row_set_row_sizegroup (row, self->row_sizegroup);

  gtk_container_add (GTK_CONTAINER (self->device_listbox), GTK_WIDGET (row));
  gtk_widget_set_visible (GTK_WIDGET (self->device_section), TRUE);
}

static void
empty_listbox (GtkListBox *listbox)
{
  g_autoptr(GList) children = NULL;
  GList *l;

  children = gtk_container_get_children (GTK_CONTAINER (listbox));
  for (l = children; l != NULL; l = l->next)
    gtk_container_remove (GTK_CONTAINER (listbox), l->data);
}

static void
update_power_saver_low_battery_row_visibility (CcPowerPanel *self)
{
  g_autoptr(UpDevice) composite = NULL;
  UpDeviceKind kind;

  composite = up_client_get_display_device (self->up_client);
  g_object_get (composite, "kind", &kind, NULL);
  gtk_widget_set_visible (GTK_WIDGET (self->power_saver_low_battery_row),
                          self->power_profiles_proxy && kind == UP_DEVICE_KIND_BATTERY);
}

static void
up_client_changed (CcPowerPanel *self)
{
  gint i;
  UpDeviceKind kind;
  guint n_batteries;
  gboolean on_ups;
  g_autoptr(UpDevice) composite = NULL;

  empty_listbox (self->battery_listbox);
  gtk_widget_hide (GTK_WIDGET (self->battery_section));

  empty_listbox (self->device_listbox);
  gtk_widget_hide (GTK_WIDGET (self->device_section));

#ifdef TEST_FAKE_DEVICES
  {
    static gboolean fake_devices_added = FALSE;
    UpDevice *device;

    if (!fake_devices_added)
      {
        fake_devices_added = TRUE;
        g_print ("adding fake devices\n");
        device = up_device_new ();
        g_object_set (device,
                      "kind", UP_DEVICE_KIND_MOUSE,
                      "native-path", "dummy:native-path1",
                      "model", "My mouse",
                      "percentage", 71.0,
                      "state", UP_DEVICE_STATE_DISCHARGING,
                      "time-to-empty", 287,
                      "icon-name", "battery-full-symbolic",
                      "power-supply", FALSE,
                      "is-present", TRUE,
                      "battery-level", UP_DEVICE_LEVEL_NORMAL,
                      NULL);
        g_ptr_array_add (self->devices, device);
        device = up_device_new ();
        g_object_set (device,
                      "kind", UP_DEVICE_KIND_KEYBOARD,
                      "native-path", "dummy:native-path2",
                      "model", "My keyboard",
                      "percentage", 59.0,
                      "state", UP_DEVICE_STATE_DISCHARGING,
                      "time-to-empty", 250,
                      "icon-name", "battery-good-symbolic",
                      "power-supply", FALSE,
                      "is-present", TRUE,
                      "battery-level", UP_DEVICE_LEVEL_NONE,
                      NULL);
        g_ptr_array_add (self->devices, device);
        device = up_device_new ();
        g_object_set (device,
                      "kind", UP_DEVICE_KIND_BATTERY,
                      "native-path", "dummy:native-path3",
                      "model", "Battery from some factory",
                      "percentage", 100.0,
                      "state", UP_DEVICE_STATE_FULLY_CHARGED,
                      "energy", 55.0,
                      "energy-full", 55.0,
                      "energy-rate", 15.0,
                      "time-to-empty", 400,
                      "time-to-full", 0,
                      "icon-name", "battery-full-charged-symbolic",
                      "power-supply", TRUE,
                      "is-present", TRUE,
                      "battery-level", UP_DEVICE_LEVEL_NONE,
                      NULL);
        g_ptr_array_add (self->devices, device);
      }
  }
#endif

#ifdef TEST_UPS
  {
    static gboolean fake_devices_added = FALSE;
    UpDevice *device;

    if (!fake_devices_added)
      {
        fake_devices_added = TRUE;
        g_print ("adding fake UPS\n");
        device = up_device_new ();
        g_object_set (device,
                      "kind", UP_DEVICE_KIND_UPS,
                      "native-path", "dummy:usb-hiddev0",
                      "model", "APC UPS",
                      "percentage", 70.0,
                      "state", UP_DEVICE_STATE_DISCHARGING,
                      "is-present", TRUE,
                      "power-supply", TRUE,
                      "battery-level", UP_DEVICE_LEVEL_NONE,
                      NULL);
        g_ptr_array_add (self->devices, device);
      }
  }
#endif

  on_ups = FALSE;
  n_batteries = 0;
  composite = up_client_get_display_device (self->up_client);
  g_object_get (composite, "kind", &kind, NULL);
  if (kind == UP_DEVICE_KIND_UPS)
    {
      on_ups = TRUE;
    }
  else
    {
      gboolean is_extra_battery = FALSE;

      /* Count the batteries */
      for (i = 0; self->devices != NULL && i < self->devices->len; i++)
        {
          UpDevice *device = (UpDevice*) g_ptr_array_index (self->devices, i);
          gboolean is_power_supply = FALSE;
          g_object_get (device,
                        "kind", &kind,
                        "power-supply", &is_power_supply,
                        NULL);
          if (kind == UP_DEVICE_KIND_BATTERY &&
              is_power_supply)
            {
              n_batteries++;
              if (is_extra_battery == FALSE)
                {
                  is_extra_battery = TRUE;
                  g_object_set_data (G_OBJECT (device), "is-main-battery", GINT_TO_POINTER(TRUE));
                }
            }
        }
    }

  if (n_batteries > 1)
    hdy_preferences_group_set_title (self->battery_section, _("Batteries"));
  else
    hdy_preferences_group_set_title (self->battery_section, _("Battery"));

  if (!on_ups && n_batteries > 1)
    add_battery (self, composite, TRUE);

  for (i = 0; self->devices != NULL && i < self->devices->len; i++)
    {
      UpDevice *device = (UpDevice*) g_ptr_array_index (self->devices, i);
      gboolean is_power_supply = FALSE;
      g_object_get (device,
                    "kind", &kind,
                    "power-supply", &is_power_supply,
                    NULL);
      if (kind == UP_DEVICE_KIND_LINE_POWER)
        {
          /* do nothing */
        }
      else if (kind == UP_DEVICE_KIND_UPS && on_ups)
        {
          add_battery (self, device, TRUE);
        }
      else if (kind == UP_DEVICE_KIND_BATTERY && is_power_supply && !on_ups && n_batteries == 1)
        {
          add_battery (self, device, TRUE);
        }
      else if (kind == UP_DEVICE_KIND_BATTERY && is_power_supply)
        {
          add_battery (self, device, FALSE);
        }
      else
        {
          add_device (self, device);
        }
    }

  update_power_saver_low_battery_row_visibility (self);
}

static void
up_client_device_removed (CcPowerPanel *self,
                          const char   *object_path)
{
  guint i;

  if (self->devices == NULL)
    return;

  for (i = 0; i < self->devices->len; i++)
    {
      UpDevice *device = g_ptr_array_index (self->devices, i);

      if (g_strcmp0 (object_path, up_device_get_object_path (device)) == 0)
        {
          g_ptr_array_remove_index (self->devices, i);
          break;
        }
    }

  up_client_changed (self);
}

static void
up_client_device_added (CcPowerPanel *self,
                        UpDevice     *device)
{
  g_ptr_array_add (self->devices, g_object_ref (device));
  g_signal_connect_object (G_OBJECT (device), "notify",
                           G_CALLBACK (up_client_changed), self, G_CONNECT_SWAPPED);
  up_client_changed (self);
}

static void
als_switch_changed_cb (CcPowerPanel *self)
{
  gboolean enabled;
  enabled = gtk_switch_get_active (self->als_switch);
  g_debug ("Setting ALS enabled %s", enabled ? "on" : "off");
  g_settings_set_boolean (self->gsd_settings, "ambient-enabled", enabled);
}

static void
als_enabled_state_changed (CcPowerPanel *self)
{
  gboolean enabled;
  gboolean visible = FALSE;

  if (self->iio_proxy != NULL)
    {
      g_autoptr(GVariant) v = g_dbus_proxy_get_cached_property (self->iio_proxy, "HasAmbientLight");
      if (v != NULL)
        visible = g_variant_get_boolean (v);
    }

  enabled = g_settings_get_boolean (self->gsd_settings, "ambient-enabled");
  g_debug ("ALS enabled: %s", enabled ? "on" : "off");
  g_signal_handlers_block_by_func (self->als_switch, als_switch_changed_cb, self);
  gtk_switch_set_active (self->als_switch, enabled);
  gtk_widget_set_visible (GTK_WIDGET (self->als_row), visible && self->has_brightness);
  g_signal_handlers_unblock_by_func (self->als_switch, als_switch_changed_cb, self);
}

static void
combo_time_changed_cb (CcPowerPanel *self, GtkWidget *widget)
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

  /* set both keys */
  g_settings_set_int (self->gsd_settings, key, value);
}

static void
set_value_for_combo (GtkComboBox *combo_box, gint value)
{
  GtkTreeIter iter;
  g_autoptr(GtkTreeIter) insert = NULL;
  GtkTreeIter new;
  GtkTreeModel *model;
  gint value_tmp;
  gint value_last = 0;
  g_autofree gchar *text = NULL;
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
                          ACTION_MODEL_VALUE, &value_tmp,
                          -1);
      if (value_tmp == value)
        {
          gtk_combo_box_set_active_iter (combo_box, &iter);
          return;
        }

      /* Insert before if the next value is larger or the value is lower
       * again (i.e. "Never" is zero and last). */
      if (!insert && (value_tmp > value || value_last > value_tmp))
        insert = gtk_tree_iter_copy (&iter);

      value_last = value_tmp;
    } while (gtk_tree_model_iter_next (model, &iter));

  /* The value is not listed, so add it at the best point (or the end). */
  gtk_list_store_insert_before (GTK_LIST_STORE (model), &new, insert);

  text = cc_util_time_to_string_text (value * 1000);
  gtk_list_store_set (GTK_LIST_STORE (model), &new,
                      ACTION_MODEL_TEXT, text,
                      ACTION_MODEL_VALUE, value,
                      -1);
  gtk_combo_box_set_active_iter (combo_box, &new);
}

static void
set_value_for_combo_row (HdyComboRow *combo_row, gint value)
{
  gboolean insert = FALSE;
  guint insert_before = 0;
  guint i;
  HdyValueObject *new;
  GListModel *model;
  gint value_last = 0;
  g_autofree gchar *text = NULL;

  /* try to make the UI match the setting */
  model = hdy_combo_row_get_model (combo_row);
  for (i = 0; i < g_list_model_get_n_items (model); i++)
    {
      HdyValueObject *value_object = g_list_model_get_item (model, i);
      gint value_tmp = GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (value_object), "value"));
      if (value_tmp == value)
        {
          hdy_combo_row_set_selected_index (combo_row, i);
          return;
        }

      /* Insert before if the next value is larger or the value is lower
       * again (i.e. "Never" is zero and last). */
      if (!insert && (value_tmp > value || value_last > value_tmp))
        {
          insert = TRUE;
          insert_before = i;
        }

      value_last = value_tmp;
    }

  /* The value is not listed, so add it at the best point (or the end). */
  text = cc_util_time_to_string_text (value * 1000);
  new = hdy_value_object_new_string (text);
  g_object_set_data (G_OBJECT (new), "value",
                     GUINT_TO_POINTER (value));
  g_list_store_insert (G_LIST_STORE (model), insert_before, new);
  hdy_combo_row_set_selected_index (combo_row, insert_before);
}

static void
set_ac_battery_ui_mode (CcPowerPanel *self)
{
  gboolean has_batteries = FALSE;
  GPtrArray *devices;
  guint i;

  devices = up_client_get_devices2 (self->up_client);
  g_debug ("got %d devices from upower\n", devices ? devices->len : 0);

  for (i = 0; devices != NULL && i < devices->len; i++)
    {
      UpDevice *device;
      gboolean is_power_supply;
      UpDeviceKind kind;

      device = g_ptr_array_index (devices, i);
      g_object_get (device,
                    "kind", &kind,
                    "power-supply", &is_power_supply,
                    NULL);
      if (kind == UP_DEVICE_KIND_UPS ||
          (kind == UP_DEVICE_KIND_BATTERY && is_power_supply))
        {
          has_batteries = TRUE;
          break;
        }
    }
  g_clear_pointer (&devices, g_ptr_array_unref);

#ifdef TEST_NO_BATTERIES
  g_print ("forcing no batteries\n");
  has_batteries = FALSE;
#endif

  self->has_batteries = has_batteries;

  if (!has_batteries)
    {
      gtk_widget_hide (GTK_WIDGET (self->suspend_on_battery_switch));
      gtk_widget_hide (GTK_WIDGET (self->suspend_on_battery_label));
      gtk_widget_hide (GTK_WIDGET (self->suspend_on_battery_delay_label));
      gtk_widget_hide (GTK_WIDGET (self->suspend_on_battery_delay_combo));
      gtk_label_set_label (self->suspend_on_ac_label, _("When _idle"));
    }
}

static gboolean
keynav_failed_cb (CcPowerPanel *self, GtkDirectionType direction, GtkWidget *list)
{
  if (direction != GTK_DIR_UP && direction != GTK_DIR_DOWN)
    return FALSE;

  direction = GTK_DIR_UP ? GTK_DIR_TAB_BACKWARD : GTK_DIR_TAB_FORWARD;

  return gtk_widget_child_focus (GTK_WIDGET (self), direction);
}

static void
blank_screen_row_changed_cb (CcPowerPanel *self)
{
  GListModel *model;
  gint selected_index;
  HdyValueObject *value_object;
  gint value;

  model = hdy_combo_row_get_model (self->blank_screen_row);
  selected_index = hdy_combo_row_get_selected_index (self->blank_screen_row);
  value_object = g_list_model_get_item (model, selected_index);
  value = GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (value_object), "value"));

  g_settings_set_uint (self->session_settings, "idle-delay", value);
}

static void
power_button_row_changed_cb (CcPowerPanel *self)
{
  GListModel *model;
  gint selected_index;
  HdyValueObject *value_object;
  gint value;

  model = hdy_combo_row_get_model (self->power_button_row);
  selected_index = hdy_combo_row_get_selected_index (self->power_button_row);
  value_object = g_list_model_get_item (model, selected_index);
  value = GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (value_object), "value"));

  g_settings_set_enum (self->gsd_settings, "power-button-action", value);
}

static void
als_enabled_setting_changed (CcPowerPanel *self)
{
  als_enabled_state_changed (self);
}

static void
iio_proxy_appeared_cb (GDBusConnection *connection,
                       const gchar *name,
                       const gchar *name_owner,
                       gpointer user_data)
{
  CcPowerPanel *self = CC_POWER_PANEL (user_data);
  g_autoptr(GError) error = NULL;

  self->iio_proxy =
    cc_object_storage_create_dbus_proxy_sync (G_BUS_TYPE_SYSTEM,
                                              G_DBUS_PROXY_FLAGS_NONE,
                                              "net.hadess.SensorProxy",
                                              "/net/hadess/SensorProxy",
                                              "net.hadess.SensorProxy",
                                              NULL, &error);
  if (error != NULL)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_warning ("Could not create IIO sensor proxy: %s", error->message);
      return;
    }

  g_signal_connect_object (G_OBJECT (self->iio_proxy), "g-properties-changed",
                           G_CALLBACK (als_enabled_state_changed), self,
                           G_CONNECT_SWAPPED);
  als_enabled_state_changed (self);
}

static void
iio_proxy_vanished_cb (GDBusConnection *connection,
                       const gchar *name,
                       gpointer user_data)
{
  CcPowerPanel *self = CC_POWER_PANEL (user_data);
  g_clear_object (&self->iio_proxy);
  als_enabled_state_changed (self);
}

static void
automatic_suspend_row_activated_cb (CcPowerPanel *self)
{
  GtkWidget *toplevel;

  toplevel = gtk_widget_get_toplevel (GTK_WIDGET (self));
  gtk_window_set_transient_for (GTK_WINDOW (self->automatic_suspend_dialog), GTK_WINDOW (toplevel));
  gtk_window_set_modal (GTK_WINDOW (self->automatic_suspend_dialog), TRUE);
  gtk_window_present (GTK_WINDOW (self->automatic_suspend_dialog));
}

static gboolean
automatic_suspend_label_mnemonic_activate_cb (CcPowerPanel *self)
{
  automatic_suspend_row_activated_cb (self);
  return TRUE;
}

static gboolean
get_sleep_type (GValue   *value,
                GVariant *variant,
                gpointer  data)
{
  gboolean enabled;

  if (g_strcmp0 (g_variant_get_string (variant, NULL), "nothing") == 0)
    enabled = FALSE;
  else
    enabled = TRUE;

  g_value_set_boolean (value, enabled);

  return TRUE;
}

static GVariant *
set_sleep_type (const GValue       *value,
                const GVariantType *expected_type,
                gpointer            data)
{
  GVariant *res;

  if (g_value_get_boolean (value))
    res = g_variant_new_string ("suspend");
  else
    res = g_variant_new_string ("nothing");

  return res;
}

static void
populate_power_button_row (HdyComboRow *combo_row,
                           gboolean     can_suspend,
                           gboolean     can_hibernate)
{
  g_autoptr (GListStore) list_store = NULL;
  struct {
    char *name;
    GsdPowerButtonActionType value;
  } actions[] = {
    { N_("Suspend"), GSD_POWER_BUTTON_ACTION_SUSPEND },
    { N_("Power Off"), GSD_POWER_BUTTON_ACTION_INTERACTIVE },
    { N_("Hibernate"), GSD_POWER_BUTTON_ACTION_HIBERNATE },
    { N_("Nothing"), GSD_POWER_BUTTON_ACTION_NOTHING }
  };
  guint i;

  list_store = g_list_store_new (HDY_TYPE_VALUE_OBJECT);
  for (i = 0; i < G_N_ELEMENTS (actions); i++)
    {
      g_autoptr (HdyValueObject) value_object = NULL;

      if (!can_suspend && actions[i].value == GSD_POWER_BUTTON_ACTION_SUSPEND)
        continue;

      if (!can_hibernate && actions[i].value == GSD_POWER_BUTTON_ACTION_HIBERNATE)
        continue;

      value_object = hdy_value_object_new_string (_(actions[i].name));
      g_object_set_data (G_OBJECT (value_object),
                         "value",
                         GUINT_TO_POINTER (actions[i].value));
      g_list_store_append (list_store, value_object);
    }

  hdy_combo_row_bind_name_model (combo_row,
                                 G_LIST_MODEL (list_store),
                                 (HdyComboRowGetNameFunc) hdy_value_object_dup_string,
                                 NULL, NULL);
}

#define NEVER 0

static void
update_automatic_suspend_label (CcPowerPanel *self)
{
  GsdPowerActionType ac_action;
  GsdPowerActionType battery_action;
  gint ac_timeout;
  gint battery_timeout;
  const gchar *s;

  ac_action = g_settings_get_enum (self->gsd_settings, "sleep-inactive-ac-type");
  battery_action = g_settings_get_enum (self->gsd_settings, "sleep-inactive-battery-type");
  ac_timeout = g_settings_get_int (self->gsd_settings, "sleep-inactive-ac-timeout");
  battery_timeout = g_settings_get_int (self->gsd_settings, "sleep-inactive-battery-timeout");

  if (ac_timeout < 0)
    g_warning ("Invalid negative timeout for 'sleep-inactive-ac-timeout': %d", ac_timeout);
  if (battery_timeout < 0)
    g_warning ("Invalid negative timeout for 'sleep-inactive-battery-timeout': %d", battery_timeout);

  if (ac_action == GSD_POWER_ACTION_NOTHING || ac_timeout < 0)
    ac_timeout = NEVER;
  if (battery_action == GSD_POWER_ACTION_NOTHING || battery_timeout < 0)
    battery_timeout = NEVER;

  if (self->has_batteries)
    {
      if (ac_timeout == NEVER && battery_timeout == NEVER)
        s = _("Off");
      else if (ac_timeout == NEVER && battery_timeout > 0)
        s = _("When on battery power");
      else if (ac_timeout > 0 && battery_timeout == NEVER)
        s = _("When plugged in");
      else
        s = _("On");
    }
  else
    {
      if (ac_timeout == NEVER)
        s = _("Off");
      else
        s = _("On");
    }

  if (self->automatic_suspend_label)
    gtk_label_set_label (self->automatic_suspend_label, s);
}

static void
on_suspend_settings_changed (CcPowerPanel *self,
                             const char   *key)
{
  if (g_str_has_prefix (key, "sleep-inactive-"))
    {
      update_automatic_suspend_label (self);
    }
}

static gboolean
can_suspend_or_hibernate (CcPowerPanel *self,
                          const char   *method_name)
{
  g_autoptr(GDBusConnection) connection = NULL;
  g_autoptr(GVariant) variant = NULL;
  g_autoptr(GError) error = NULL;
  const char *s;

  connection = g_bus_get_sync (G_BUS_TYPE_SYSTEM,
                               cc_panel_get_cancellable (CC_PANEL (self)),
                               &error);
  if (!connection)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_warning ("system bus not available: %s", error->message);
      return FALSE;
    }

  variant = g_dbus_connection_call_sync (connection,
                                         "org.freedesktop.login1",
                                         "/org/freedesktop/login1",
                                         "org.freedesktop.login1.Manager",
                                         method_name,
                                         NULL,
                                         NULL,
                                         G_DBUS_CALL_FLAGS_NONE,
                                         -1,
                                         cc_panel_get_cancellable (CC_PANEL (self)),
                                         &error);

  if (!variant)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_debug ("Failed to call %s(): %s", method_name, error->message);
      return FALSE;
    }

  g_variant_get (variant, "(&s)", &s);
  return g_strcmp0 (s, "yes") == 0;
}

static void
got_brightness_cb (GObject      *source_object,
                   GAsyncResult *res,
                   gpointer      user_data)
{
  g_autoptr(GVariant) result = NULL;
  g_autoptr(GError) error = NULL;
  gint32 brightness = -1.0;
  CcPowerPanel *self;

  result = g_dbus_connection_call_finish (G_DBUS_CONNECTION (source_object), res, &error);
  if (!result)
    {
      g_debug ("Failed to get Brightness property: %s", error->message);
      if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        return;
    }
  else
    {
      g_autoptr(GVariant) v = NULL;
      g_variant_get (result, "(v)", &v);
      brightness = v ? g_variant_get_int32 (v) : -1.0;
    }

  self = user_data;
  self->has_brightness = brightness >= 0.0;

  gtk_widget_set_visible (GTK_WIDGET (self->dim_screen_row), self->has_brightness);
  als_enabled_state_changed (self);
}

static void
populate_blank_screen_row (HdyComboRow *combo_row)
{
  g_autoptr (GListStore) list_store = g_list_store_new (HDY_TYPE_VALUE_OBJECT);
  gint minutes[] = { 1, 2, 3, 4, 5, 8, 10, 12, 15 };
  guint i;
  g_autoptr (HdyValueObject) never_value_object = NULL;

  for (i = 0; i < G_N_ELEMENTS (minutes); i++)
    {
      gchar *text = NULL;
      g_autoptr (HdyValueObject) value_object = NULL;

      /* Translators: Option for "Blank Screen" in "Power" panel */
      text = g_strdup_printf (g_dngettext (GETTEXT_PACKAGE, "%d minute", "%d minutes", minutes[i]), minutes[i]);
      value_object = hdy_value_object_new_take_string (text);

      g_object_set_data (G_OBJECT (value_object), "value", GUINT_TO_POINTER (minutes[i] * 60));
      g_list_store_append (list_store, value_object);
    }

  never_value_object = hdy_value_object_new_string (C_("Idle time", "Never"));
  g_object_set_data (G_OBJECT (never_value_object), "value", GUINT_TO_POINTER (0));
  g_list_store_append (list_store, never_value_object);

  hdy_combo_row_bind_name_model (combo_row,
                                 G_LIST_MODEL (list_store),
                                 (HdyComboRowGetNameFunc) hdy_value_object_dup_string,
                                 NULL, NULL);
}

static void
setup_power_saving (CcPowerPanel *self)
{
  g_autoptr(GDBusConnection) connection = NULL;
  g_autoptr(GError) error = NULL;
  int value;

  /* ambient light sensor */
  self->iio_proxy_watch_id =
    g_bus_watch_name (G_BUS_TYPE_SYSTEM,
                      "net.hadess.SensorProxy",
                      G_BUS_NAME_WATCHER_FLAGS_NONE,
                      iio_proxy_appeared_cb,
                      iio_proxy_vanished_cb,
                      self, NULL);
  g_signal_connect_object (self->gsd_settings, "changed",
                           G_CALLBACK (als_enabled_setting_changed), self, G_CONNECT_SWAPPED);

  connection = g_bus_get_sync (G_BUS_TYPE_SESSION,
                               cc_panel_get_cancellable (CC_PANEL (self)),
                               &error);
  if (connection)
    {
      g_dbus_connection_call (connection,
                              "org.gnome.SettingsDaemon.Power",
                              "/org/gnome/SettingsDaemon/Power",
                              "org.freedesktop.DBus.Properties",
                              "Get",
                              g_variant_new ("(ss)",
                                             "org.gnome.SettingsDaemon.Power.Screen",
                                             "Brightness"),
                              NULL,
                              G_DBUS_CALL_FLAGS_NONE,
                              -1,
                              cc_panel_get_cancellable (CC_PANEL (self)),
                              got_brightness_cb,
                              self);
    }
  else
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_warning ("session bus not available: %s", error->message);
    }


  g_settings_bind (self->gsd_settings, "idle-dim",
                   self->dim_screen_switch, "active",
                   G_SETTINGS_BIND_DEFAULT);

  g_signal_handlers_block_by_func (self->blank_screen_row, blank_screen_row_changed_cb, self);
  populate_blank_screen_row (self->blank_screen_row);
  value = g_settings_get_uint (self->session_settings, "idle-delay");
  set_value_for_combo_row (self->blank_screen_row, value);
  g_signal_handlers_unblock_by_func (self->blank_screen_row, blank_screen_row_changed_cb, self);

  /* The default values for these settings are unfortunate for us;
   * timeout == 0, action == suspend means 'do nothing' - just
   * as timout === anything, action == nothing.
   * For our switch/combobox combination, the second choice works
   * much better, so translate the first to the second here.
   */
  if (g_settings_get_int (self->gsd_settings, "sleep-inactive-ac-timeout") == 0)
    {
      g_settings_set_enum (self->gsd_settings, "sleep-inactive-ac-type", GSD_POWER_ACTION_NOTHING);
      g_settings_set_int (self->gsd_settings, "sleep-inactive-ac-timeout", 3600);
    }
  if (g_settings_get_int (self->gsd_settings, "sleep-inactive-battery-timeout") == 0)
    {
      g_settings_set_enum (self->gsd_settings, "sleep-inactive-battery-type", GSD_POWER_ACTION_NOTHING);
      g_settings_set_int (self->gsd_settings, "sleep-inactive-battery-timeout", 1800);
    }

  /* Automatic suspend row */
  if (can_suspend_or_hibernate (self, "CanSuspend"))
    {
      gtk_widget_show (GTK_WIDGET (self->automatic_suspend_row));
      atk_object_set_name (ATK_OBJECT (gtk_widget_get_accessible (GTK_WIDGET (self->automatic_suspend_row))), _("Automatic suspend"));

      g_signal_connect (self->automatic_suspend_dialog, "delete-event", G_CALLBACK (gtk_widget_hide_on_delete), NULL);
      g_signal_connect_object (self->gsd_settings, "changed", G_CALLBACK (on_suspend_settings_changed), self, G_CONNECT_SWAPPED);

      g_settings_bind_with_mapping (self->gsd_settings, "sleep-inactive-battery-type",
                                    self->suspend_on_battery_switch, "active",
                                    G_SETTINGS_BIND_DEFAULT,
                                    get_sleep_type, set_sleep_type, NULL, NULL);

      g_object_set_data (G_OBJECT (self->suspend_on_battery_delay_combo), "_gsettings_key", "sleep-inactive-battery-timeout");
      value = g_settings_get_int (self->gsd_settings, "sleep-inactive-battery-timeout");
      set_value_for_combo (self->suspend_on_battery_delay_combo, value);
      g_signal_connect_object (self->suspend_on_battery_delay_combo, "changed",
                               G_CALLBACK (combo_time_changed_cb), self, G_CONNECT_SWAPPED);
      g_object_bind_property (self->suspend_on_battery_switch, "active", self->suspend_on_battery_delay_combo, "sensitive",
                              G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE);

      g_settings_bind_with_mapping (self->gsd_settings, "sleep-inactive-ac-type",
                                    self->suspend_on_ac_switch, "active",
                                    G_SETTINGS_BIND_DEFAULT,
                                    get_sleep_type, set_sleep_type, NULL, NULL);

      g_object_set_data (G_OBJECT (self->suspend_on_ac_delay_combo), "_gsettings_key", "sleep-inactive-ac-timeout");
      value = g_settings_get_int (self->gsd_settings, "sleep-inactive-ac-timeout");
      set_value_for_combo (self->suspend_on_ac_delay_combo, value);
      g_signal_connect_object (self->suspend_on_ac_delay_combo, "changed",
                               G_CALLBACK (combo_time_changed_cb), self, G_CONNECT_SWAPPED);
      g_object_bind_property (self->suspend_on_ac_switch, "active", self->suspend_on_ac_delay_combo, "sensitive",
                              G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE);

      set_ac_battery_ui_mode (self);
      update_automatic_suspend_label (self);
    }
}

static const char *
variant_lookup_string (GVariant   *dict,
                       const char *key)
{
  GVariant *variant;

  variant = g_variant_lookup_value (dict, key, G_VARIANT_TYPE_STRING);
  if (!variant)
    return NULL;
  return g_variant_get_string (variant, NULL);
}

static void
performance_profile_set_active (CcPowerPanel  *self,
                                const char    *profile_str)
{
  CcPowerProfile profile = cc_power_profile_from_str (profile_str);
  GtkRadioButton *button;

  button = cc_power_profile_row_get_radio_button (CC_POWER_PROFILE_ROW (self->power_profiles_row[profile]));
  g_assert (button);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), TRUE);
}

static void
performance_profile_set_inhibited (CcPowerPanel  *self,
                                   const char    *performance_inhibited)
{
  CcPowerProfileRow *row;

  row = self->power_profiles_row[CC_POWER_PROFILE_PERFORMANCE];
  if (!row)
      return;
  cc_power_profile_row_set_performance_inhibited (row, performance_inhibited);
}

static void
power_profile_update_info_boxes (CcPowerPanel *self)
{
  g_autoptr(GVariant) degraded_variant = NULL;
  g_autoptr(GVariant) holds_variant = NULL;
  g_autoptr(GVariant) profile_variant = NULL;
  guint i, num_children;
  const char *degraded = NULL;
  const char *profile;
  CcPowerProfileInfoRow *row;
  int next_insert = 0;

  empty_listbox (self->power_profile_info_listbox);
  gtk_widget_hide (GTK_WIDGET (self->power_profile_info_listbox));

  profile_variant = g_dbus_proxy_get_cached_property (self->power_profiles_proxy, "ActiveProfile");
  if (!profile_variant)
    {
      g_warning ("No 'ActiveProfile' property on power-profiles-daemon service");
      return;
    }
  profile = g_variant_get_string (profile_variant, NULL);

  degraded_variant = g_dbus_proxy_get_cached_property (self->power_profiles_proxy, "PerformanceDegraded");
  if (degraded_variant)
    degraded = g_variant_get_string (degraded_variant, NULL);
  if (degraded && *degraded != '\0')
    {
      const char *text;

      gtk_widget_show (GTK_WIDGET (self->power_profile_info_listbox));

      if (g_str_equal (degraded, "high-operating-temperature"))
        text = _("Performance mode temporarily disabled due to high operating temperature.");
      else if (g_str_equal (degraded, "lap-detected"))
        text = _("Lap detected: performance mode temporarily unavailable. Move the device to a stable surface to restore.");
      else
        text = _("Performance mode temporarily disabled.");

      row = cc_power_profile_info_row_new (text);
      gtk_widget_show (GTK_WIDGET (row));
      gtk_container_add (GTK_CONTAINER (self->power_profile_info_listbox), GTK_WIDGET (row));
      if (g_str_equal (profile, "performance"))
        next_insert = 1;
    }

  holds_variant = g_dbus_proxy_get_cached_property (self->power_profiles_proxy, "ActiveProfileHolds");
  if (!holds_variant)
    {
      g_warning ("No 'ActiveProfileHolds' property on power-profiles-daemon service");
      return;
    }

  num_children = g_variant_n_children (holds_variant);
  for (i = 0; i < num_children; i++)
    {
      g_autoptr(GDesktopAppInfo) app_info = NULL;
      g_autoptr(GVariant) hold_variant = NULL;
      g_autofree char *text = NULL;
      const char *app_id, *held_profile, *reason, *name;

      hold_variant = g_variant_get_child_value (holds_variant, i);
      if (!hold_variant || !g_variant_is_of_type (hold_variant, G_VARIANT_TYPE ("a{sv}")))
        continue;

      app_id = variant_lookup_string (hold_variant, "ApplicationId");
      if (!app_id)
        continue;

      gtk_widget_show (GTK_WIDGET (self->power_profile_info_listbox));

      app_info = g_desktop_app_info_new (app_id);
      name = app_info ? g_app_info_get_name (G_APP_INFO (app_info)) : app_id;
      held_profile = variant_lookup_string (hold_variant, "Profile");
      reason = variant_lookup_string (hold_variant, "Reason");
      g_debug ("Adding info row for %s hold by %s: %s", held_profile, app_id, reason);

      if (g_strcmp0 (held_profile, "power-saver") == 0 &&
          g_strcmp0 (app_id, "org.gnome.SettingsDaemon.Power") == 0)
        {
          text = g_strdup (_("Low battery: power saver enabled. Previous mode will be restored when battery is sufficiently charged."));
        }
      else
        {
          switch (cc_power_profile_from_str (held_profile))
          {
          case CC_POWER_PROFILE_POWER_SAVER:
            /* translators: "%s" is an application name */
            text = g_strdup_printf (_("Power Saver mode activated by “%s”."), name);
            break;
          case CC_POWER_PROFILE_PERFORMANCE:
            /* translators: "%s" is an application name */
            text = g_strdup_printf (_("Performance mode activated by “%s”."), name);
            break;
          default:
            g_assert_not_reached ();
          }
        }

      row = cc_power_profile_info_row_new (text);
      gtk_widget_show (GTK_WIDGET (row));
      if (g_strcmp0 (held_profile, profile) != 0)
        gtk_list_box_insert (GTK_LIST_BOX (self->power_profile_info_listbox), GTK_WIDGET (row), -1);
      else
        gtk_list_box_insert (GTK_LIST_BOX (self->power_profile_info_listbox), GTK_WIDGET (row), next_insert);
    }
}

static void
power_profiles_row_activated_cb (GtkListBox    *box,
                                 GtkListBoxRow *box_row,
                                 gpointer       user_data)
{
  if (!gtk_widget_is_sensitive (GTK_WIDGET (box_row)))
    return;

  cc_power_profile_row_set_active (CC_POWER_PROFILE_ROW(box_row), TRUE);
}

static gint
perf_profile_list_box_sort (GtkListBoxRow *row1,
                            GtkListBoxRow *row2,
                            gpointer       user_data)
{
  CcPowerProfile row1_profile, row2_profile;

  row1_profile = cc_power_profile_row_get_profile (CC_POWER_PROFILE_ROW (row1));
  row2_profile = cc_power_profile_row_get_profile (CC_POWER_PROFILE_ROW (row2));

  if (row1_profile < row2_profile)
    return -1;
  if (row1_profile > row2_profile)
    return 1;
  return 0;
}

static void
power_profiles_properties_changed_cb (CcPowerPanel *self,
                                      GVariant   *changed_properties,
                                      GStrv       invalidated_properties,
                                      GDBusProxy *proxy)
{
  g_autoptr(GVariantIter) iter = NULL;
  const char *key;
  g_autoptr(GVariant) value = NULL;

  g_variant_get (changed_properties, "a{sv}", &iter);
  while (g_variant_iter_next (iter, "{&sv}", &key, &value))
    {
      if (g_strcmp0 (key, "PerformanceInhibited") == 0)
        {
          if (!self->has_performance_degraded)
            performance_profile_set_inhibited (self,
                                               g_variant_get_string (value, NULL));
        }
      else if (g_strcmp0 (key, "PerformanceDegraded") == 0 ||
               g_strcmp0 (key, "ActiveProfileHolds") == 0)
        {
          power_profile_update_info_boxes (self);
        }
      else if (g_strcmp0 (key, "ActiveProfile") == 0)
        {
          self->power_profiles_in_update = TRUE;
          performance_profile_set_active (self, g_variant_get_string (value, NULL));
          self->power_profiles_in_update = FALSE;
        }
      else
        {
          g_debug ("Unhandled change on '%s' property", key);
        }
    }
}

static void
set_active_profile_cb (GObject      *source_object,
                       GAsyncResult *res,
                       gpointer      user_data)
{
  g_autoptr(GVariant) variant = NULL;
  g_autoptr(GError) error = NULL;

  variant = g_dbus_connection_call_finish (G_DBUS_CONNECTION (source_object),
                                           res, &error);
  if (!variant)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_warning ("Could not set active profile: %s", error->message);
    }
}

static void
power_profile_button_toggled_cb (CcPowerProfileRow *row,
                                 gpointer         user_data)
{
  CcPowerPanel *self = user_data;
  CcPowerProfile profile;
  g_autoptr(GDBusConnection) connection = NULL;
  g_autoptr(GError) error = NULL;

  if (!cc_power_profile_row_get_active (row))
    return;
  if (self->power_profiles_in_update)
    return;

  profile = cc_power_profile_row_get_profile (row);

  connection = g_bus_get_sync (G_BUS_TYPE_SYSTEM,
                               cc_panel_get_cancellable (CC_PANEL (self)),
                               &error);
  if (!connection)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_warning ("system bus not available: %s", error->message);
      return;
    }

  g_dbus_connection_call (connection,
                          "net.hadess.PowerProfiles",
                          "/net/hadess/PowerProfiles",
                          "org.freedesktop.DBus.Properties",
                          "Set",
                          g_variant_new ("(ssv)",
                                         "net.hadess.PowerProfiles",
                                         "ActiveProfile",
                                         g_variant_new_string (cc_power_profile_to_str (profile))),
                          NULL,
                          G_DBUS_CALL_FLAGS_NONE,
                          -1,
                          cc_panel_get_cancellable (CC_PANEL (self)),
                          set_active_profile_cb,
                          NULL);
}

static void
setup_power_profiles (CcPowerPanel *self)
{
  g_autoptr(GDBusConnection) connection = NULL;
  g_autoptr(GVariant) variant = NULL;
  g_autoptr(GVariant) props = NULL;
  guint i, num_children;
  g_autoptr(GError) error = NULL;
  const char *performance_inhibited = NULL;
  const char *performance_degraded;
  const char *active_profile;
  g_autoptr(GVariant) profiles = NULL;
  GtkRadioButton *last_button;

  self->power_profiles_proxy = cc_object_storage_create_dbus_proxy_sync (G_BUS_TYPE_SYSTEM,
                                                                         G_DBUS_PROXY_FLAGS_NONE,
                                                                         "net.hadess.PowerProfiles",
                                                                         "/net/hadess/PowerProfiles",
                                                                         "net.hadess.PowerProfiles",
                                                                         NULL,
                                                                         &error);

  if (!self->power_profiles_proxy)
    {
      g_debug ("Could not create Power Profiles proxy: %s", error->message);
      return;
    }

  connection = g_bus_get_sync (G_BUS_TYPE_SYSTEM,
                               cc_panel_get_cancellable (CC_PANEL (self)),
                               &error);
  if (!connection)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_warning ("system bus not available: %s", error->message);
      return;
    }

  variant = g_dbus_connection_call_sync (connection,
                                         "net.hadess.PowerProfiles",
                                         "/net/hadess/PowerProfiles",
                                         "org.freedesktop.DBus.Properties",
                                         "GetAll",
                                         g_variant_new ("(s)",
                                                        "net.hadess.PowerProfiles"),
                                         NULL,
                                         G_DBUS_CALL_FLAGS_NONE,
                                         -1,
                                         NULL,
                                         &error);

  if (!variant)
    {
      g_debug ("Failed to get properties for Power Profiles: %s",
               error->message);
      g_clear_object (&self->power_profiles_proxy);
      return;
    }

  gtk_widget_show (GTK_WIDGET (self->power_profile_section));

  props = g_variant_get_child_value (variant, 0);
  performance_degraded = variant_lookup_string (props, "PerformanceDegraded");
  self->has_performance_degraded = performance_degraded != NULL;
  if (performance_degraded == NULL)
    performance_inhibited = variant_lookup_string (props, "PerformanceInhibited");
  active_profile = variant_lookup_string (props, "ActiveProfile");

  last_button = NULL;
  profiles = g_variant_lookup_value (props, "Profiles", NULL);
  num_children = g_variant_n_children (profiles);
  for (i = 0; i < num_children; i++)
    {
      g_autoptr(GVariant) profile_variant;
      const char *name;
      GtkRadioButton *button;
      CcPowerProfile profile;
      CcPowerProfileRow *row;

      profile_variant = g_variant_get_child_value (profiles, i);
      if (!profile_variant ||
          !g_variant_is_of_type (profile_variant, G_VARIANT_TYPE ("a{sv}")))
        continue;

      name = variant_lookup_string (profile_variant, "Profile");
      if (!name)
        continue;
      g_debug ("Adding row for profile '%s' (driver: %s)",
               name, variant_lookup_string (profile_variant, "Driver"));

      profile = cc_power_profile_from_str (name);
      row = cc_power_profile_row_new (cc_power_profile_from_str (name));
      cc_power_profile_row_set_performance_inhibited (row, performance_inhibited);
      g_signal_connect_object (G_OBJECT (row), "button-toggled",
                               G_CALLBACK (power_profile_button_toggled_cb), self,
                               0);
      self->power_profiles_row[profile] = row;
      gtk_widget_show (GTK_WIDGET (row));
      gtk_container_add (GTK_CONTAINER (self->power_profile_listbox), GTK_WIDGET (row));
      gtk_size_group_add_widget (self->row_sizegroup, GTK_WIDGET (row));

      /* Connect radio button to group */
      button = cc_power_profile_row_get_radio_button (row);
      gtk_radio_button_join_group (button, last_button);
      last_button = button;
    }

  self->power_profiles_in_update = TRUE;
  performance_profile_set_active (self, active_profile);
  self->power_profiles_in_update = FALSE;

  self->power_profiles_prop_id = g_signal_connect_object (G_OBJECT (self->power_profiles_proxy), "g-properties-changed",
                                                          G_CALLBACK (power_profiles_properties_changed_cb), self, G_CONNECT_SWAPPED);

  if (self->has_performance_degraded)
    power_profile_update_info_boxes (self);

  update_power_saver_low_battery_row_visibility (self);
}

static void
setup_general_section (CcPowerPanel *self)
{
  gboolean can_suspend, can_hibernate, show_section = FALSE;

  can_suspend = can_suspend_or_hibernate (self, "CanSuspend");
  can_hibernate = can_suspend_or_hibernate (self, "CanHibernate");

  if ((can_hibernate || can_suspend) &&
      g_strcmp0 (self->chassis_type, "vm") != 0 &&
      g_strcmp0 (self->chassis_type, "tablet") != 0 &&
      g_strcmp0 (self->chassis_type, "handset") != 0)
    {
      gtk_widget_show (GTK_WIDGET (self->power_button_row));

      g_signal_handlers_block_by_func (self->power_button_row,
                                       power_button_row_changed_cb,
                                       self);
      populate_power_button_row (self->power_button_row,
                                 can_suspend,
                                 can_hibernate);
      set_value_for_combo_row (self->power_button_row,
                               g_settings_get_enum (self->gsd_settings, "power-button-action"));
      g_signal_handlers_unblock_by_func (self->power_button_row,
                                         power_button_row_changed_cb,
                                         self);

      show_section = TRUE;
    }

  if (self->has_batteries)
    {
      gtk_widget_show (GTK_WIDGET (self->battery_percentage_row));

      g_settings_bind (self->interface_settings, "show-battery-percentage",
                       self->battery_percentage_switch, "active",
                       G_SETTINGS_BIND_DEFAULT);

      show_section = TRUE;
    }

  gtk_widget_set_visible (GTK_WIDGET (self->general_section), show_section);
}

static gint
battery_sort_func (GtkListBoxRow *a, GtkListBoxRow *b, gpointer data)
{
  CcBatteryRow *row_a = CC_BATTERY_ROW (a);
  CcBatteryRow *row_b = CC_BATTERY_ROW (b);
  gboolean a_primary;
  gboolean b_primary;
  UpDeviceKind a_kind;
  UpDeviceKind b_kind;

  a_primary = cc_battery_row_get_primary(row_a);
  b_primary = cc_battery_row_get_primary(row_b);

  if (a_primary)
    return -1;
  else if (b_primary)
    return 1;

  a_kind = cc_battery_row_get_kind(row_a);
  b_kind = cc_battery_row_get_kind(row_b);

  return a_kind - b_kind;
}

static void
cc_power_panel_class_init (CcPowerPanelClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  CcPanelClass *panel_class = CC_PANEL_CLASS (klass);

  object_class->dispose = cc_power_panel_dispose;

  panel_class->get_help_uri = cc_power_panel_get_help_uri;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/power/cc-power-panel.ui");

  gtk_widget_class_bind_template_child (widget_class, CcPowerPanel, als_row);
  gtk_widget_class_bind_template_child (widget_class, CcPowerPanel, als_switch);
  gtk_widget_class_bind_template_child (widget_class, CcPowerPanel, automatic_suspend_dialog);
  gtk_widget_class_bind_template_child (widget_class, CcPowerPanel, automatic_suspend_label);
  gtk_widget_class_bind_template_child (widget_class, CcPowerPanel, automatic_suspend_row);
  gtk_widget_class_bind_template_child (widget_class, CcPowerPanel, battery_listbox);
  gtk_widget_class_bind_template_child (widget_class, CcPowerPanel, battery_percentage_row);
  gtk_widget_class_bind_template_child (widget_class, CcPowerPanel, battery_percentage_switch);
  gtk_widget_class_bind_template_child (widget_class, CcPowerPanel, battery_row_sizegroup);
  gtk_widget_class_bind_template_child (widget_class, CcPowerPanel, battery_section);
  gtk_widget_class_bind_template_child (widget_class, CcPowerPanel, blank_screen_row);
  gtk_widget_class_bind_template_child (widget_class, CcPowerPanel, device_listbox);
  gtk_widget_class_bind_template_child (widget_class, CcPowerPanel, device_section);
  gtk_widget_class_bind_template_child (widget_class, CcPowerPanel, dim_screen_row);
  gtk_widget_class_bind_template_child (widget_class, CcPowerPanel, dim_screen_switch);
  gtk_widget_class_bind_template_child (widget_class, CcPowerPanel, general_section);
  gtk_widget_class_bind_template_child (widget_class, CcPowerPanel, level_sizegroup);
  gtk_widget_class_bind_template_child (widget_class, CcPowerPanel, power_button_row);
  gtk_widget_class_bind_template_child (widget_class, CcPowerPanel, power_profile_listbox);
  gtk_widget_class_bind_template_child (widget_class, CcPowerPanel, power_profile_info_listbox);
  gtk_widget_class_bind_template_child (widget_class, CcPowerPanel, power_profile_section);
  gtk_widget_class_bind_template_child (widget_class, CcPowerPanel, power_saver_low_battery_row);
  gtk_widget_class_bind_template_child (widget_class, CcPowerPanel, power_saver_low_battery_switch);
  gtk_widget_class_bind_template_child (widget_class, CcPowerPanel, row_sizegroup);
  gtk_widget_class_bind_template_child (widget_class, CcPowerPanel, suspend_on_battery_delay_combo);
  gtk_widget_class_bind_template_child (widget_class, CcPowerPanel, suspend_on_battery_delay_label);
  gtk_widget_class_bind_template_child (widget_class, CcPowerPanel, suspend_on_battery_label);
  gtk_widget_class_bind_template_child (widget_class, CcPowerPanel, suspend_on_battery_switch);
  gtk_widget_class_bind_template_child (widget_class, CcPowerPanel, suspend_on_ac_delay_combo);
  gtk_widget_class_bind_template_child (widget_class, CcPowerPanel, suspend_on_ac_label);
  gtk_widget_class_bind_template_child (widget_class, CcPowerPanel, suspend_on_ac_switch);

  gtk_widget_class_bind_template_callback (widget_class, als_switch_changed_cb);
  gtk_widget_class_bind_template_callback (widget_class, automatic_suspend_label_mnemonic_activate_cb);
  gtk_widget_class_bind_template_callback (widget_class, blank_screen_row_changed_cb);
  gtk_widget_class_bind_template_callback (widget_class, keynav_failed_cb);
  gtk_widget_class_bind_template_callback (widget_class, power_button_row_changed_cb);
  gtk_widget_class_bind_template_callback (widget_class, power_profiles_row_activated_cb);
  gtk_widget_class_bind_template_callback (widget_class, automatic_suspend_row_activated_cb);
}

static void
cc_power_panel_init (CcPowerPanel *self)
{
  guint i;

  g_resources_register (cc_power_get_resource ());

  gtk_widget_init_template (GTK_WIDGET (self));
  load_custom_css (self, "/org/gnome/control-center/power/battery-levels.css");
  load_custom_css (self, "/org/gnome/control-center/power/power-profiles.css");

  self->chassis_type = get_chassis_type (cc_panel_get_cancellable (CC_PANEL (self)));

  self->up_client = up_client_new ();

  self->gsd_settings = g_settings_new ("org.gnome.settings-daemon.plugins.power");
  self->session_settings = g_settings_new ("org.gnome.desktop.session");
  self->interface_settings = g_settings_new ("org.gnome.desktop.interface");

  gtk_list_box_set_sort_func (self->battery_listbox,
                              (GtkListBoxSortFunc)battery_sort_func, NULL, NULL);

  gtk_list_box_set_sort_func (self->device_listbox,
                              (GtkListBoxSortFunc)battery_sort_func, NULL, NULL);

  gtk_list_box_set_sort_func (self->power_profile_listbox,
                              perf_profile_list_box_sort,
                              NULL, NULL);
  setup_power_profiles (self);

  setup_power_saving (self);
  g_settings_bind (self->gsd_settings, "power-saver-profile-on-low-battery",
                   self->power_saver_low_battery_switch, "active",
                   G_SETTINGS_BIND_DEFAULT);

  setup_general_section (self);

  /* populate batteries */
  g_signal_connect_object (self->up_client, "device-added", G_CALLBACK (up_client_device_added), self, G_CONNECT_SWAPPED);
  g_signal_connect_object (self->up_client, "device-removed", G_CALLBACK (up_client_device_removed), self, G_CONNECT_SWAPPED);

  self->devices = up_client_get_devices2 (self->up_client);
  for (i = 0; self->devices != NULL && i < self->devices->len; i++) {
    UpDevice *device = g_ptr_array_index (self->devices, i);
    g_signal_connect_object (G_OBJECT (device), "notify",
                             G_CALLBACK (up_client_changed), self, G_CONNECT_SWAPPED);
  }
  up_client_changed (self);
}
