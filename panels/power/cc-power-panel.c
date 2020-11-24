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
#include <handy.h>

#ifdef HAVE_NETWORK_MANAGER
#include <NetworkManager.h>
#endif

#include "shell/cc-object-storage.h"
#include "list-box-helper.h"
#include "cc-battery-row.h"
#include "cc-brightness-scale.h"
#include "cc-power-profile-row.h"
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

  GtkDialog         *automatic_suspend_dialog;
  GtkLabel          *battery_heading;
  GtkListBox        *battery_listbox;
  GtkSizeGroup      *battery_row_sizegroup;
  GtkBox            *battery_section;
  GtkSizeGroup      *battery_sizegroup;
  GtkSizeGroup      *charge_sizegroup;
  GtkLabel          *device_heading;
  GtkListBox        *device_listbox;
  GtkBox            *device_section;
  GtkListStore      *idle_time_liststore;
  GtkSizeGroup      *level_sizegroup;
  GtkScrolledWindow *main_scroll;
  HdyClamp          *main_box;
  GtkListStore      *power_button_liststore;
  GtkLabel          *power_profile_heading;
  GtkListBox        *power_profile_listbox;
  GtkBox            *power_profile_section;
  GtkBox            *power_vbox;
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

  GList         *boxes;
  GList         *boxes_reverse;

  GtkWidget     *dim_screen_row;
  GtkWidget     *brightness_row;
  CcBrightnessScale *brightness_scale;
  GtkWidget     *kbd_brightness_row;
  CcBrightnessScale *kbd_brightness_scale;

  GtkWidget     *automatic_suspend_row;
  GtkWidget     *automatic_suspend_label;

  GDBusProxy    *bt_rfkill;
  GDBusProxy    *bt_properties;
  GtkWidget     *bt_switch;
  GtkWidget     *bt_row;

  GDBusProxy    *iio_proxy;
  guint          iio_proxy_watch_id;
  GtkWidget     *als_switch;
  GtkWidget     *als_row;

  GDBusProxy    *power_profiles_proxy;
  guint          power_profiles_prop_id;
  GtkWidget     *power_profiles_row[NUM_CC_POWER_PROFILES];
  gboolean       power_profiles_in_update;

  GtkWidget     *power_button_combo;
  GtkWidget     *idle_delay_combo;

#ifdef HAVE_NETWORK_MANAGER
  NMClient      *nm_client;
  GtkWidget     *wifi_switch;
  GtkWidget     *wifi_row;
  GtkWidget     *mobile_switch;
  GtkWidget     *mobile_row;
#endif

  GtkAdjustment *focus_adjustment;
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
  g_clear_object (&self->bt_rfkill);
  g_clear_object (&self->bt_properties);
  g_clear_object (&self->iio_proxy);
#ifdef HAVE_NETWORK_MANAGER
  g_clear_object (&self->nm_client);
#endif
  g_clear_pointer (&self->boxes, g_list_free);
  g_clear_pointer (&self->boxes_reverse, g_list_free);
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

static GtkWidget *
no_prelight_row_new (void)
{
  GtkWidget *row = gtk_list_box_row_new ();
  gtk_list_box_row_set_selectable (GTK_LIST_BOX_ROW (row), FALSE);
  gtk_list_box_row_set_activatable (GTK_LIST_BOX_ROW (row), FALSE);
  return row;
}

static GtkWidget *
row_box_new (void)
{
  GtkWidget *box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, FALSE);
  gtk_widget_set_margin_end (box, 12);
  gtk_widget_set_margin_start (box, 12);
  gtk_box_set_spacing (GTK_BOX (box), 12);
  gtk_widget_show (box);
  return box;
}

static GtkWidget *
row_title_new (const gchar  *title,
               const gchar  *subtitle,
               GtkWidget   **title_label)
{
  PangoAttrList *attributes;
  GtkWidget *box, *label;

  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, FALSE);
  gtk_widget_show (box);
  gtk_widget_set_margin_bottom (box, 6);
  gtk_widget_set_margin_top (box, 6);
  gtk_box_set_spacing (GTK_BOX (box), 4);
  gtk_widget_set_valign (box, GTK_ALIGN_CENTER);

  label = gtk_label_new (NULL);
  gtk_widget_show (label);
  gtk_label_set_markup (GTK_LABEL (label), title);
  gtk_label_set_use_underline (GTK_LABEL (label), TRUE);
  gtk_label_set_ellipsize (GTK_LABEL (label), PANGO_ELLIPSIZE_END);
  gtk_widget_set_halign (label, GTK_ALIGN_START);
  gtk_label_set_xalign (GTK_LABEL (label), 0.0);

  if (title_label)
    *title_label = label;
  gtk_container_add (GTK_CONTAINER (box), label);

  if (subtitle == NULL)
    return box;

  attributes = pango_attr_list_new ();
  pango_attr_list_insert (attributes, pango_attr_scale_new (0.9));

  label = gtk_label_new (NULL);
  gtk_widget_show (label);
  gtk_label_set_markup (GTK_LABEL (label), subtitle);
  gtk_label_set_use_underline (GTK_LABEL (label), TRUE);
  gtk_label_set_ellipsize (GTK_LABEL (label), PANGO_ELLIPSIZE_END);
  gtk_widget_set_halign (label, GTK_ALIGN_START);
  gtk_label_set_xalign (GTK_LABEL (label), 0.0);
  gtk_label_set_attributes (GTK_LABEL (label), attributes);
  gtk_style_context_add_class (gtk_widget_get_style_context (label),
                               GTK_STYLE_CLASS_DIM_LABEL);
  gtk_container_add (GTK_CONTAINER (box), label);

  pango_attr_list_unref (attributes);

  return box;
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
  cc_battery_row_set_charge_sizegroup (row, panel->charge_sizegroup);
  cc_battery_row_set_battery_sizegroup (row, panel->battery_sizegroup);

  gtk_container_add (GTK_CONTAINER (panel->battery_listbox), GTK_WIDGET (row));
  gtk_widget_set_visible (GTK_WIDGET (panel->battery_section), TRUE);
}

static void
add_device (CcPowerPanel *panel, UpDevice *device)
{
  CcBatteryRow *row = cc_battery_row_new (device, FALSE);
  cc_battery_row_set_level_sizegroup (row, panel->level_sizegroup);
  cc_battery_row_set_row_sizegroup (row, panel->row_sizegroup);
  cc_battery_row_set_charge_sizegroup (row, panel->charge_sizegroup);
  cc_battery_row_set_battery_sizegroup (row, panel->battery_sizegroup);

  gtk_container_add (GTK_CONTAINER (panel->device_listbox), GTK_WIDGET (row));
  gtk_widget_set_visible (GTK_WIDGET (panel->device_section), TRUE);
}

static void
up_client_changed (CcPowerPanel *self)
{
  g_autoptr(GList) battery_children = NULL;
  g_autoptr(GList) device_children = NULL;
  GList *l;
  gint i;
  UpDeviceKind kind;
  guint n_batteries;
  gboolean on_ups;
  g_autoptr(UpDevice) composite = NULL;
  g_autofree gchar *s = NULL;

  battery_children = gtk_container_get_children (GTK_CONTAINER (self->battery_listbox));
  for (l = battery_children; l != NULL; l = l->next)
    gtk_container_remove (GTK_CONTAINER (self->battery_listbox), l->data);
  gtk_widget_hide (GTK_WIDGET (self->battery_section));

  device_children = gtk_container_get_children (GTK_CONTAINER (self->device_listbox));
  for (l = device_children; l != NULL; l = l->next)
    gtk_container_remove (GTK_CONTAINER (self->device_listbox), l->data);
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
    s = g_strdup_printf ("<b>%s</b>", _("Batteries"));
  else
    s = g_strdup_printf ("<b>%s</b>", _("Battery"));
  gtk_label_set_label (GTK_LABEL (self->battery_heading), s);

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
als_switch_changed (CcPowerPanel *self)
{
  gboolean enabled;
  enabled = gtk_switch_get_active (GTK_SWITCH (self->als_switch));
  g_debug ("Setting ALS enabled %s", enabled ? "on" : "off");
  g_settings_set_boolean (self->gsd_settings, "ambient-enabled", enabled);
}

static void
als_enabled_state_changed (CcPowerPanel *self)
{
  gboolean enabled;
  gboolean has_brightness = FALSE;
  gboolean visible = FALSE;

  has_brightness = cc_brightness_scale_get_has_brightness (self->brightness_scale);

  if (self->iio_proxy != NULL)
    {
      g_autoptr(GVariant) v = g_dbus_proxy_get_cached_property (self->iio_proxy, "HasAmbientLight");
      if (v != NULL)
        visible = g_variant_get_boolean (v);
    }

  enabled = g_settings_get_boolean (self->gsd_settings, "ambient-enabled");
  g_debug ("ALS enabled: %s", enabled ? "on" : "off");
  g_signal_handlers_block_by_func (self->als_switch, als_switch_changed, self);
  gtk_switch_set_active (GTK_SWITCH (self->als_switch), enabled);
  gtk_widget_set_visible (self->als_row, visible && has_brightness);
  g_signal_handlers_unblock_by_func (self->als_switch, als_switch_changed, self);
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

static void
bt_set_powered (CcPowerPanel *self,
                gboolean      powered)
{
  g_dbus_proxy_call (self->bt_properties,
		     "Set",
		     g_variant_new_parsed ("('org.gnome.SettingsDaemon.Rfkill', 'BluetoothAirplaneMode', %v)",
					   g_variant_new_boolean (!powered)),
		     G_DBUS_CALL_FLAGS_NONE,
		     -1,
		     cc_panel_get_cancellable (CC_PANEL (self)),
		     NULL, NULL);
}

static void
bt_switch_changed (CcPowerPanel *self)
{
  gboolean powered;

  powered = gtk_switch_get_active (GTK_SWITCH (self->bt_switch));

  g_debug ("Setting bt power %s", powered ? "on" : "off");

  bt_set_powered (self, powered);
}

static void
bt_powered_state_changed (CcPowerPanel *panel)
{
  gboolean powered, has_airplane_mode;
  g_autoptr(GVariant) v1 = NULL;
  g_autoptr(GVariant) v2 = NULL;

  v1 = g_dbus_proxy_get_cached_property (panel->bt_rfkill, "BluetoothHasAirplaneMode");
  has_airplane_mode = g_variant_get_boolean (v1);

  if (!has_airplane_mode)
    {
      g_debug ("BluetoothHasAirplaneMode is false, hiding Bluetooth power row");
      gtk_widget_hide (panel->bt_row);
      return;
    }

  v2 = g_dbus_proxy_get_cached_property (panel->bt_rfkill, "BluetoothAirplaneMode");
  powered = !g_variant_get_boolean (v2);

  g_debug ("bt powered state changed to %s", powered ? "on" : "off");

  gtk_widget_show (panel->bt_row);

  g_signal_handlers_block_by_func (panel->bt_switch, bt_switch_changed, panel);
  gtk_switch_set_active (GTK_SWITCH (panel->bt_switch), powered);
  g_signal_handlers_unblock_by_func (panel->bt_switch, bt_switch_changed, panel);
}

#ifdef HAVE_NETWORK_MANAGER
static gboolean
has_wifi_devices (NMClient *client)
{
  const GPtrArray *devices;
  NMDevice *device;
  gint i;

  if (!nm_client_get_nm_running (client))
    return FALSE;

  devices = nm_client_get_devices (client);
  if (devices == NULL)
    return FALSE;

  for (i = 0; i < devices->len; i++)
    {
      device = g_ptr_array_index (devices, i);
      switch (nm_device_get_device_type (device))
        {
        case NM_DEVICE_TYPE_WIFI:
          return TRUE;
        default:
          break;
        }
    }

  return FALSE;
}

static void
wifi_switch_changed (CcPowerPanel *self)
{
  gboolean enabled;

  enabled = gtk_switch_get_active (GTK_SWITCH (self->wifi_switch));
  g_debug ("Setting wifi %s", enabled ? "enabled" : "disabled");
  nm_client_wireless_set_enabled (self->nm_client, enabled);
}

static gboolean
has_mobile_devices (NMClient *client)
{
  const GPtrArray *devices;
  NMDevice *device;
  gint i;

  if (!nm_client_get_nm_running (client))
    return FALSE;

  devices = nm_client_get_devices (client);
  if (devices == NULL)
    return FALSE;

  for (i = 0; i < devices->len; i++)
    {
      device = g_ptr_array_index (devices, i);
      switch (nm_device_get_device_type (device))
        {
        case NM_DEVICE_TYPE_MODEM:
          return TRUE;
        default:
          break;
        }
    }

  return FALSE;
}

static void
mobile_switch_changed (CcPowerPanel *self)
{
  gboolean enabled;

  enabled = gtk_switch_get_active (GTK_SWITCH (self->mobile_switch));
  g_debug ("Setting wwan %s", enabled ? "enabled" : "disabled");
  nm_client_wwan_set_enabled (self->nm_client, enabled);
}

static void
nm_client_state_changed (CcPowerPanel *self)
{
  gboolean visible;
  gboolean active;
  gboolean sensitive;

  visible = has_wifi_devices (self->nm_client);
  active = nm_client_networking_get_enabled (self->nm_client) &&
           nm_client_wireless_get_enabled (self->nm_client) &&
           nm_client_wireless_hardware_get_enabled (self->nm_client);
  sensitive = nm_client_networking_get_enabled (self->nm_client) &&
              nm_client_wireless_hardware_get_enabled (self->nm_client);

  g_debug ("wifi state changed to %s", active ? "enabled" : "disabled");

  g_signal_handlers_block_by_func (self->wifi_switch, wifi_switch_changed, self);
  gtk_switch_set_active (GTK_SWITCH (self->wifi_switch), active);
  gtk_widget_set_sensitive (self->wifi_switch, sensitive);
  gtk_widget_set_visible (self->wifi_row, visible);
  g_signal_handlers_unblock_by_func (self->wifi_switch, wifi_switch_changed, self);

  visible = has_mobile_devices (self->nm_client);

  /* Set the switch active, if wwan is enabled. */
  active = nm_client_networking_get_enabled (self->nm_client) &&
           (nm_client_wwan_get_enabled (self->nm_client) &&
            nm_client_wwan_hardware_get_enabled (self->nm_client));
  sensitive = nm_client_networking_get_enabled (self->nm_client) &&
              nm_client_wwan_hardware_get_enabled (self->nm_client);

  g_debug ("mobile state changed to %s", active ? "enabled" : "disabled");

  g_signal_handlers_block_by_func (self->mobile_switch, mobile_switch_changed, self);
  gtk_switch_set_active (GTK_SWITCH (self->mobile_switch), active);
  gtk_widget_set_sensitive (self->mobile_switch, sensitive);
  gtk_widget_set_visible (self->mobile_row, visible);
  g_signal_handlers_unblock_by_func (self->mobile_switch, mobile_switch_changed, self);
}

static void
nm_device_changed (CcPowerPanel *self)
{
  gtk_widget_set_visible (self->wifi_row, has_wifi_devices (self->nm_client));
  gtk_widget_set_visible (self->mobile_row, has_mobile_devices (self->nm_client));
}

static void
setup_nm_client (CcPowerPanel *self,
                 NMClient     *client)
{
  self->nm_client = client;

  g_signal_connect_object (self->nm_client, "notify",
                           G_CALLBACK (nm_client_state_changed), self, G_CONNECT_SWAPPED);
  g_signal_connect_object (self->nm_client, "device-added",
                           G_CALLBACK (nm_device_changed), self, G_CONNECT_SWAPPED);
  g_signal_connect_object (self->nm_client, "device-removed",
                           G_CALLBACK (nm_device_changed), self, G_CONNECT_SWAPPED);

  nm_client_state_changed (self);
  nm_device_changed (self);
}

static void
nm_client_ready_cb (GObject *source_object,
                    GAsyncResult *res,
                    gpointer user_data)
{
  CcPowerPanel *self;
  NMClient *client;
  g_autoptr(GError) error = NULL;

  client = nm_client_new_finish (res, &error);
  if (!client)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        {
          g_warning ("Failed to create NetworkManager client: %s",
                     error->message);

          self = user_data;
          gtk_widget_set_sensitive (self->wifi_row, FALSE);
          gtk_widget_set_sensitive (self->mobile_row, FALSE);
        }
      return;
    }

  self = user_data;

  /* Setup the client */
  setup_nm_client (self, client);

  /* Store the object in the cache too */
  cc_object_storage_add_object (CC_OBJECT_NMCLIENT, client);
}

#endif

static gboolean
keynav_failed_cb (CcPowerPanel *self, GtkDirectionType direction, GtkWidget *list)
{
  GtkWidget *next_list = NULL;
  GList *item, *boxes_list;
  gdouble value, lower, upper, page;

  /* Find the list in the list of GtkListBoxes */
  if (direction == GTK_DIR_DOWN)
    boxes_list = self->boxes;
  else
    boxes_list = self->boxes_reverse;

  item = g_list_find (boxes_list, list);
  g_assert (item);
  item = item->next;
  while (1)
    {
      if (item == NULL)
        item = boxes_list;

      /* Avoid looping */
      if (item->data == list)
        break;

      if (gtk_widget_is_visible (item->data))
        {
          next_list = item->data;
          break;
        }

    item = item->next;
  }

  if (next_list)
    {
      gtk_widget_child_focus (next_list, direction);
      return TRUE;
    }

  value = gtk_adjustment_get_value (self->focus_adjustment);
  lower = gtk_adjustment_get_lower (self->focus_adjustment);
  upper = gtk_adjustment_get_upper (self->focus_adjustment);
  page  = gtk_adjustment_get_page_size (self->focus_adjustment);

  if (direction == GTK_DIR_UP && value > lower)
    {
      gtk_adjustment_set_value (self->focus_adjustment, lower);
      return TRUE;
    }
  else if (direction == GTK_DIR_DOWN && value < upper - page)
    {
      gtk_adjustment_set_value (self->focus_adjustment, upper - page);
      return TRUE;
    }

  return FALSE;
}

static void
combo_idle_delay_changed_cb (CcPowerPanel *self)
{
  GtkTreeIter iter;
  GtkTreeModel *model;
  gint value;
  gboolean ret;

  /* no selection */
  ret = gtk_combo_box_get_active_iter (GTK_COMBO_BOX (self->idle_delay_combo), &iter);
  if (!ret)
    return;

  /* get entry */
  model = gtk_combo_box_get_model (GTK_COMBO_BOX (self->idle_delay_combo));
  gtk_tree_model_get (model, &iter,
                      1, &value,
                      -1);

  /* set both keys */
  g_settings_set_uint (self->session_settings, "idle-delay", value);
}

static void
combo_power_button_changed_cb (CcPowerPanel *self)
{
  GtkTreeIter iter;
  GtkTreeModel *model;
  gint value;
  gboolean ret;

  /* no selection */
  ret = gtk_combo_box_get_active_iter (GTK_COMBO_BOX (self->power_button_combo), &iter);
  if (!ret)
    return;

  /* get entry */
  model = gtk_combo_box_get_model (GTK_COMBO_BOX (self->power_button_combo));
  gtk_tree_model_get (model, &iter,
                      1, &value,
                      -1);

  /* set both keys */
  g_settings_set_enum (self->gsd_settings, "power-button-action", value);
}

static GtkWidget *
add_brightness_row (CcPowerPanel       *self,
                    BrightnessDevice    device,
		    const char         *text,
		    CcBrightnessScale **brightness_scale)
{
  GtkWidget *row, *box, *label, *title, *box2, *w, *scale;

  row = no_prelight_row_new ();
  gtk_widget_show (row);
  box = row_box_new ();
  gtk_container_add (GTK_CONTAINER (row), box);
  title = row_title_new (text, NULL, &label);
  gtk_box_pack_start (GTK_BOX (box), title, FALSE, TRUE, 0);
  gtk_size_group_add_widget (self->battery_sizegroup, title);
  box2 = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 12);
  gtk_widget_show (box2);
  w = gtk_label_new ("");
  gtk_widget_show (w);
  gtk_box_pack_start (GTK_BOX (box2), w, FALSE, TRUE, 0);
  gtk_size_group_add_widget (self->charge_sizegroup, w);

  scale = g_object_new (CC_TYPE_BRIGHTNESS_SCALE,
                        "device", device,
                        NULL);
  gtk_widget_show (scale);
  gtk_label_set_mnemonic_widget (GTK_LABEL (label), scale);
  gtk_box_pack_start (GTK_BOX (box2), scale, TRUE, TRUE, 0);
  gtk_size_group_add_widget (self->level_sizegroup, scale);
  *brightness_scale = CC_BRIGHTNESS_SCALE (scale);

  gtk_box_pack_start (GTK_BOX (box), box2, TRUE, TRUE, 0);

  return row;
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
activate_row (CcPowerPanel *self,
              GtkListBoxRow *row)
{
  GtkWidget *toplevel;

  if (row == GTK_LIST_BOX_ROW (self->automatic_suspend_row))
    {
      toplevel = gtk_widget_get_toplevel (GTK_WIDGET (self));
      gtk_window_set_transient_for (GTK_WINDOW (self->automatic_suspend_dialog), GTK_WINDOW (toplevel));
      gtk_window_set_modal (GTK_WINDOW (self->automatic_suspend_dialog), TRUE);
      gtk_window_present (GTK_WINDOW (self->automatic_suspend_dialog));
    }
}

static gboolean
automatic_suspend_activate (CcPowerPanel *self)
{
  activate_row (self, GTK_LIST_BOX_ROW (self->automatic_suspend_row));
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
populate_power_button_model (GtkTreeModel *model,
                             gboolean      can_suspend,
                             gboolean      can_hibernate)
{
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

  for (i = 0; i < G_N_ELEMENTS (actions); i++)
    {
      if (!can_suspend && actions[i].value == GSD_POWER_BUTTON_ACTION_SUSPEND)
        continue;

      if (!can_hibernate && actions[i].value == GSD_POWER_BUTTON_ACTION_HIBERNATE)
        continue;

      gtk_list_store_insert_with_values (GTK_LIST_STORE (model),
                                         NULL, -1,
                                         0, _(actions[i].name),
                                         1, actions[i].value,
                                         -1);
    }
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
    gtk_label_set_label (GTK_LABEL (self->automatic_suspend_label), s);
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
has_brightness_cb (CcPowerPanel *self)
{
  gboolean has_brightness;

  has_brightness = cc_brightness_scale_get_has_brightness (self->brightness_scale);

  gtk_widget_set_visible (self->brightness_row, has_brightness);
  gtk_widget_set_visible (self->dim_screen_row, has_brightness);

  als_enabled_state_changed (self);

}

static void
has_kbd_brightness_cb (CcPowerPanel *self,
                       GParamSpec   *pspec,
                       GObject      *object)
{
  gboolean has_brightness;

  has_brightness = cc_brightness_scale_get_has_brightness (self->kbd_brightness_scale);

  gtk_widget_set_visible (self->kbd_brightness_row, has_brightness);
}

static void
add_power_saving_section (CcPowerPanel *self)
{
  GtkWidget *widget, *box, *label, *row;
  GtkWidget *title;
  GtkWidget *sw;
  int value;
  g_autofree gchar *s = NULL;
  gboolean can_suspend;

  s = g_strdup_printf ("<b>%s</b>", _("Power Saving"));
  label = gtk_label_new (s);
  gtk_widget_show (label);
  gtk_label_set_ellipsize (GTK_LABEL (label), PANGO_ELLIPSIZE_END);
  gtk_label_set_xalign (GTK_LABEL (label), 0.0);
  gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
  gtk_widget_set_halign (label, GTK_ALIGN_START);
  gtk_widget_set_margin_bottom (label, 12);
  gtk_box_pack_start (self->power_vbox, label, FALSE, TRUE, 0);
  gtk_widget_show (label);

  widget = gtk_list_box_new ();
  gtk_widget_show (widget);
  self->boxes_reverse = g_list_prepend (self->boxes_reverse, widget);
  g_signal_connect_object (widget, "keynav-failed", G_CALLBACK (keynav_failed_cb), self, G_CONNECT_SWAPPED);
  gtk_list_box_set_selection_mode (GTK_LIST_BOX (widget), GTK_SELECTION_NONE);
  gtk_list_box_set_header_func (GTK_LIST_BOX (widget),
                                cc_list_box_update_header_func,
                                NULL, NULL);
  g_signal_connect_object (widget, "row-activated",
                           G_CALLBACK (activate_row), self, G_CONNECT_SWAPPED);

  atk_object_add_relationship (ATK_OBJECT (gtk_widget_get_accessible (label)),
                               ATK_RELATION_LABEL_FOR,
                               ATK_OBJECT (gtk_widget_get_accessible (widget)));
  atk_object_add_relationship (ATK_OBJECT (gtk_widget_get_accessible (widget)),
                               ATK_RELATION_LABELLED_BY,
                               ATK_OBJECT (gtk_widget_get_accessible (label)));

  box = gtk_frame_new (NULL);
  gtk_widget_show (box);
  gtk_frame_set_shadow_type (GTK_FRAME (box), GTK_SHADOW_IN);
  gtk_widget_set_margin_bottom (box, 32);
  gtk_container_add (GTK_CONTAINER (box), widget);
  gtk_box_pack_start (self->power_vbox, box, FALSE, TRUE, 0);

  row = add_brightness_row (self, BRIGHTNESS_DEVICE_SCREEN, _("_Screen Brightness"), &self->brightness_scale);
  g_signal_connect_object (self->brightness_scale, "notify::has-brightness",
                           G_CALLBACK (has_brightness_cb), self, G_CONNECT_SWAPPED);
  gtk_widget_show (row);
  self->brightness_row = row;

  gtk_container_add (GTK_CONTAINER (widget), row);
  gtk_size_group_add_widget (self->row_sizegroup, row);

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
  self->als_row = row = no_prelight_row_new ();
  gtk_widget_show (row);
  box = row_box_new ();
  gtk_container_add (GTK_CONTAINER (row), box);
  title = row_title_new (_("Automatic Brightness"), NULL, &label);
  gtk_box_pack_start (GTK_BOX (box), title, TRUE, TRUE, 0);

  self->als_switch = gtk_switch_new ();
  gtk_widget_show (self->als_switch);
  gtk_widget_set_valign (self->als_switch, GTK_ALIGN_CENTER);
  gtk_box_pack_start (GTK_BOX (box), self->als_switch, FALSE, TRUE, 0);
  gtk_label_set_mnemonic_widget (GTK_LABEL (label), self->als_switch);
  gtk_container_add (GTK_CONTAINER (widget), row);
  gtk_size_group_add_widget (self->row_sizegroup, row);
  g_signal_connect_object (self->als_switch, "notify::active",
                           G_CALLBACK (als_switch_changed), self, G_CONNECT_SWAPPED);

  row = add_brightness_row (self, BRIGHTNESS_DEVICE_KBD, _("_Keyboard Brightness"), &self->kbd_brightness_scale);
  g_signal_connect_object (self->kbd_brightness_scale, "notify::has-brightness",
                           G_CALLBACK (has_kbd_brightness_cb), self, G_CONNECT_SWAPPED);
  gtk_widget_show (row);
  self->kbd_brightness_row = row;

  gtk_container_add (GTK_CONTAINER (widget), row);
  gtk_size_group_add_widget (self->row_sizegroup, row);

  self->dim_screen_row = row = no_prelight_row_new ();
  gtk_widget_show (row);
  box = row_box_new ();
  gtk_container_add (GTK_CONTAINER (row), box);
  title = row_title_new (_("_Dim Screen When Inactive"), NULL, &label);
  gtk_box_pack_start (GTK_BOX (box), title, TRUE, TRUE, 0);

  sw = gtk_switch_new ();
  gtk_widget_show (sw);
  g_settings_bind (self->gsd_settings, "idle-dim",
                   sw, "active",
                   G_SETTINGS_BIND_DEFAULT);
  gtk_widget_set_valign (sw, GTK_ALIGN_CENTER);
  gtk_box_pack_start (GTK_BOX (box), sw, FALSE, TRUE, 0);
  gtk_label_set_mnemonic_widget (GTK_LABEL (label), sw);
  gtk_container_add (GTK_CONTAINER (widget), row);
  gtk_size_group_add_widget (self->row_sizegroup, row);

  row = no_prelight_row_new ();
  gtk_widget_show (row);
  box = row_box_new ();
  gtk_container_add (GTK_CONTAINER (row), box);
  title = row_title_new (_("_Blank Screen"), NULL, &label);
  gtk_box_pack_start (GTK_BOX (box), title, TRUE, TRUE, 0);

  self->idle_delay_combo = gtk_combo_box_text_new ();
  gtk_widget_show (self->idle_delay_combo);
  gtk_combo_box_set_entry_text_column (GTK_COMBO_BOX (self->idle_delay_combo), 0);
  gtk_combo_box_set_model (GTK_COMBO_BOX (self->idle_delay_combo),
                           GTK_TREE_MODEL (self->idle_time_liststore));
  value = g_settings_get_uint (self->session_settings, "idle-delay");
  set_value_for_combo (GTK_COMBO_BOX (self->idle_delay_combo), value);
  g_signal_connect_object (self->idle_delay_combo, "changed",
                           G_CALLBACK (combo_idle_delay_changed_cb), self, G_CONNECT_SWAPPED);
  gtk_widget_set_valign (self->idle_delay_combo, GTK_ALIGN_CENTER);
  gtk_box_pack_start (GTK_BOX (box), self->idle_delay_combo, FALSE, TRUE, 0);
  gtk_label_set_mnemonic_widget (GTK_LABEL (label), self->idle_delay_combo);
  gtk_container_add (GTK_CONTAINER (widget), row);
  gtk_size_group_add_widget (self->row_sizegroup, row);

  can_suspend = can_suspend_or_hibernate (self, "CanSuspend");

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
  if (can_suspend)
    {
      self->automatic_suspend_row = row = gtk_list_box_row_new ();
      gtk_widget_show (row);
      box = row_box_new ();
      gtk_container_add (GTK_CONTAINER (row), box);
      title = row_title_new (_("_Automatic Suspend"), NULL, NULL);
      atk_object_set_name (ATK_OBJECT (gtk_widget_get_accessible (self->automatic_suspend_row)), _("Automatic suspend"));
      gtk_box_pack_start (GTK_BOX (box), title, TRUE, TRUE, 0);

      self->automatic_suspend_label = gtk_label_new ("");
      gtk_widget_show (self->automatic_suspend_label);
      gtk_label_set_mnemonic_widget (GTK_LABEL (label), self->automatic_suspend_label);
      g_signal_connect_object (self->automatic_suspend_label, "mnemonic-activate",
                               G_CALLBACK (automatic_suspend_activate), self, G_CONNECT_SWAPPED);
      gtk_widget_set_halign (self->automatic_suspend_label, GTK_ALIGN_END);
      gtk_box_pack_start (GTK_BOX (box), self->automatic_suspend_label, FALSE, TRUE, 0);
      gtk_container_add (GTK_CONTAINER (widget), row);
      gtk_size_group_add_widget (self->row_sizegroup, row);

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

#ifdef HAVE_NETWORK_MANAGER
  self->wifi_row = row = no_prelight_row_new ();
  gtk_widget_hide (row);
  box = row_box_new ();
  gtk_container_add (GTK_CONTAINER (row), box);
  title = row_title_new (_("_Wi-Fi"),
                         _("Wi-Fi can be turned off to save power."),
                         NULL);
  gtk_box_pack_start (GTK_BOX (box), title, TRUE, TRUE, 0);

  self->wifi_switch = gtk_switch_new ();
  gtk_widget_show (self->wifi_switch);
  gtk_widget_set_valign (self->wifi_switch, GTK_ALIGN_CENTER);
  gtk_box_pack_start (GTK_BOX (box), self->wifi_switch, FALSE, TRUE, 0);
  gtk_label_set_mnemonic_widget (GTK_LABEL (label), self->wifi_switch);
  gtk_container_add (GTK_CONTAINER (widget), row);
  gtk_size_group_add_widget (self->row_sizegroup, row);

  self->mobile_row = row = no_prelight_row_new ();
  gtk_widget_hide (row);
  box = row_box_new ();
  gtk_container_add (GTK_CONTAINER (row), box);
  title = row_title_new (_("_Mobile Broadband"),
                         _("Mobile broadband (LTE, 4G, 3G, etc.) can be turned off to save power."),
                         NULL);
  gtk_box_pack_start (GTK_BOX (box), title, TRUE, TRUE, 0);

  self->mobile_switch = gtk_switch_new ();
  gtk_widget_show (self->mobile_switch);
  gtk_widget_set_valign (self->mobile_switch, GTK_ALIGN_CENTER);
  gtk_box_pack_start (GTK_BOX (box), self->mobile_switch, FALSE, TRUE, 0);
  gtk_label_set_mnemonic_widget (GTK_LABEL (label), self->mobile_switch);
  gtk_container_add (GTK_CONTAINER (widget), row);
  gtk_size_group_add_widget (self->row_sizegroup, row);

  g_signal_connect_object (G_OBJECT (self->mobile_switch), "notify::active",
                           G_CALLBACK (mobile_switch_changed), self, G_CONNECT_SWAPPED);

  /* Create and store a NMClient instance if it doesn't exist yet */
  if (cc_object_storage_has_object (CC_OBJECT_NMCLIENT))
    setup_nm_client (self, cc_object_storage_get_object (CC_OBJECT_NMCLIENT));
  else
    nm_client_new_async (cc_panel_get_cancellable (CC_PANEL (self)), nm_client_ready_cb, self);

  g_signal_connect_object (G_OBJECT (self->wifi_switch), "notify::active",
                           G_CALLBACK (wifi_switch_changed), self, G_CONNECT_SWAPPED);
#endif

#ifdef HAVE_BLUETOOTH

  self->bt_rfkill = cc_object_storage_create_dbus_proxy_sync (G_BUS_TYPE_SESSION,
                                                              G_DBUS_PROXY_FLAGS_NONE,
                                                              "org.gnome.SettingsDaemon.Rfkill",
                                                              "/org/gnome/SettingsDaemon/Rfkill",
                                                              "org.gnome.SettingsDaemon.Rfkill",
                                                              NULL,
                                                              NULL);

  if (self->bt_rfkill)
    {
      self->bt_properties = cc_object_storage_create_dbus_proxy_sync (G_BUS_TYPE_SESSION,
                                                                      G_DBUS_PROXY_FLAGS_NONE,
                                                                      "org.gnome.SettingsDaemon.Rfkill",
                                                                      "/org/gnome/SettingsDaemon/Rfkill",
                                                                      "org.freedesktop.DBus.Properties",
                                                                      NULL,
                                                                      NULL);
    }

  row = no_prelight_row_new ();
  gtk_widget_hide (row);
  box = row_box_new ();
  gtk_container_add (GTK_CONTAINER (row), box);
  title = row_title_new (_("_Bluetooth"),
                         _("Bluetooth can be turned off to save power."),
                         NULL);
  gtk_box_pack_start (GTK_BOX (box), title, TRUE, TRUE, 0);

  self->bt_switch = gtk_switch_new ();
  gtk_widget_show (self->bt_switch);
  gtk_widget_set_valign (self->bt_switch, GTK_ALIGN_CENTER);
  gtk_box_pack_start (GTK_BOX (box), self->bt_switch, FALSE, TRUE, 0);
  gtk_label_set_mnemonic_widget (GTK_LABEL (label), self->bt_switch);
  gtk_container_add (GTK_CONTAINER (widget), row);
  gtk_size_group_add_widget (self->row_sizegroup, row);
  self->bt_row = row;
  g_signal_connect_object (self->bt_rfkill, "g-properties-changed",
                           G_CALLBACK (bt_powered_state_changed), self, G_CONNECT_SWAPPED);
  g_signal_connect_object (G_OBJECT (self->bt_switch), "notify::active",
                           G_CALLBACK (bt_switch_changed), self, G_CONNECT_SWAPPED);

  bt_powered_state_changed (self);
#endif
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
  GtkWidget *row;

  row = self->power_profiles_row[CC_POWER_PROFILE_PERFORMANCE];
  g_assert (row != NULL);
  cc_power_profile_row_set_performance_inhibited (CC_POWER_PROFILE_ROW (row),
                                                  performance_inhibited);
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
          performance_profile_set_inhibited (self,
                                             g_variant_get_string (value, NULL));
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
  const char *performance_inhibited;
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

  self->boxes_reverse = g_list_prepend (self->boxes_reverse, self->power_profile_listbox);

  props = g_variant_get_child_value (variant, 0);
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
      GtkWidget *row;

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
      row = cc_power_profile_row_new (cc_power_profile_from_str (name),
                                      performance_inhibited);
      g_signal_connect_object (G_OBJECT (row), "button-toggled",
                               G_CALLBACK (power_profile_button_toggled_cb), self,
                               0);
      self->power_profiles_row[profile] = row;
      gtk_widget_show (row);
      gtk_container_add (GTK_CONTAINER (self->power_profile_listbox), row);
      gtk_size_group_add_widget (self->row_sizegroup, row);

      /* Connect radio button to group */
      button = cc_power_profile_row_get_radio_button (CC_POWER_PROFILE_ROW (row));
      gtk_radio_button_join_group (button, last_button);
      last_button = button;
    }

  self->power_profiles_in_update = TRUE;
  performance_profile_set_active (self, active_profile);
  self->power_profiles_in_update = FALSE;

  self->power_profiles_prop_id = g_signal_connect_object (G_OBJECT (self->power_profiles_proxy), "g-properties-changed",
                                                          G_CALLBACK (power_profiles_properties_changed_cb), self, G_CONNECT_SWAPPED);
}

static void
add_battery_percentage (CcPowerPanel *self,
                        GtkListBox   *listbox)
{
  GtkWidget *box, *label, *title;
  GtkWidget *row;
  GtkWidget *sw;

  if (!self->has_batteries)
    return;

  /* Show Battery Percentage */
  row = no_prelight_row_new ();
  gtk_widget_show (row);
  box = row_box_new ();
  gtk_container_add (GTK_CONTAINER (row), box);
  title = row_title_new (_("Show Battery _Percentage"), NULL, &label);
  gtk_box_pack_start (GTK_BOX (box), title, TRUE, TRUE, 0);

  sw = gtk_switch_new ();
  gtk_widget_show (sw);
  g_settings_bind (self->interface_settings, "show-battery-percentage",
                   sw, "active",
                   G_SETTINGS_BIND_DEFAULT);
  gtk_widget_set_valign (sw, GTK_ALIGN_CENTER);
  gtk_box_pack_start (GTK_BOX (box), sw, FALSE, TRUE, 0);
  gtk_label_set_mnemonic_widget (GTK_LABEL (label), sw);
  gtk_container_add (GTK_CONTAINER (listbox), row);
  gtk_size_group_add_widget (self->row_sizegroup, row);
}

static void
add_general_section (CcPowerPanel *self)
{
  GtkWidget *widget, *box, *label, *title;
  GtkWidget *row;
  g_autofree gchar *s = NULL;
  GtkTreeModel *model;
  GsdPowerButtonActionType button_value;
  gboolean can_suspend, can_hibernate;

  /* Frame header */
  s = g_markup_printf_escaped ("<b>%s</b>", _("Suspend & Power Button"));
  label = gtk_label_new (s);
  gtk_widget_show (label);
  gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
  gtk_widget_set_halign (label, GTK_ALIGN_START);
  gtk_widget_set_margin_bottom (label, 12);
  gtk_box_pack_start (self->power_vbox, label, FALSE, TRUE, 0);

  widget = gtk_list_box_new ();
  gtk_widget_show (widget);
  self->boxes_reverse = g_list_prepend (self->boxes_reverse, widget);
  g_signal_connect_object (widget, "keynav-failed", G_CALLBACK (keynav_failed_cb), self, G_CONNECT_SWAPPED);
  gtk_list_box_set_selection_mode (GTK_LIST_BOX (widget), GTK_SELECTION_NONE);
  gtk_list_box_set_header_func (GTK_LIST_BOX (widget),
                                cc_list_box_update_header_func,
                                NULL, NULL);

  atk_object_add_relationship (ATK_OBJECT (gtk_widget_get_accessible (label)),
                               ATK_RELATION_LABEL_FOR,
                               ATK_OBJECT (gtk_widget_get_accessible (widget)));
  atk_object_add_relationship (ATK_OBJECT (gtk_widget_get_accessible (widget)),
                               ATK_RELATION_LABELLED_BY,
                               ATK_OBJECT (gtk_widget_get_accessible (label)));

  box = gtk_frame_new (NULL);
  gtk_widget_show (box);
  gtk_frame_set_shadow_type (GTK_FRAME (box), GTK_SHADOW_IN);
  gtk_widget_set_margin_bottom (box, 32);
  gtk_container_add (GTK_CONTAINER (box), widget);
  gtk_box_pack_start (self->power_vbox, box, FALSE, TRUE, 0);

  can_suspend = can_suspend_or_hibernate (self, "CanSuspend");
  can_hibernate = can_suspend_or_hibernate (self, "CanHibernate");

  if ((!can_hibernate && !can_suspend) ||
      g_strcmp0 (self->chassis_type, "vm") == 0 ||
      g_strcmp0 (self->chassis_type, "tablet") == 0 ||
      g_strcmp0 (self->chassis_type, "handset") == 0)
    {
      add_battery_percentage (self, GTK_LIST_BOX (widget));
      return;
    }

  /* Power button row */
  row = no_prelight_row_new ();
  gtk_widget_show (row);
  box = row_box_new ();
  gtk_container_add (GTK_CONTAINER (row), box);

  title = row_title_new (_("Po_wer Button Behavior"), NULL, &label);
  gtk_box_pack_start (GTK_BOX (box), title, TRUE, TRUE, 0);

  self->power_button_combo = gtk_combo_box_text_new ();
  gtk_widget_show (self->power_button_combo);
  gtk_combo_box_set_entry_text_column (GTK_COMBO_BOX (self->power_button_combo), 0);
  model = GTK_TREE_MODEL (self->power_button_liststore);
  populate_power_button_model (model, can_suspend, can_hibernate);
  gtk_combo_box_set_model (GTK_COMBO_BOX (self->power_button_combo), model);
  button_value = g_settings_get_enum (self->gsd_settings, "power-button-action");
  set_value_for_combo (GTK_COMBO_BOX (self->power_button_combo), button_value);
  g_signal_connect_object (self->power_button_combo, "changed",
                           G_CALLBACK (combo_power_button_changed_cb), self, G_CONNECT_SWAPPED);
  gtk_widget_set_valign (self->power_button_combo, GTK_ALIGN_CENTER);
  gtk_box_pack_start (GTK_BOX (box), self->power_button_combo, FALSE, TRUE, 0);
  gtk_label_set_mnemonic_widget (GTK_LABEL (label), self->power_button_combo);
  gtk_container_add (GTK_CONTAINER (widget), row);
  gtk_size_group_add_widget (self->row_sizegroup, row);

  add_battery_percentage (self, GTK_LIST_BOX (widget));
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

  gtk_widget_class_bind_template_child (widget_class, CcPowerPanel, automatic_suspend_dialog);
  gtk_widget_class_bind_template_child (widget_class, CcPowerPanel, battery_heading);
  gtk_widget_class_bind_template_child (widget_class, CcPowerPanel, battery_listbox);
  gtk_widget_class_bind_template_child (widget_class, CcPowerPanel, battery_row_sizegroup);
  gtk_widget_class_bind_template_child (widget_class, CcPowerPanel, battery_section);
  gtk_widget_class_bind_template_child (widget_class, CcPowerPanel, battery_sizegroup);
  gtk_widget_class_bind_template_child (widget_class, CcPowerPanel, charge_sizegroup);
  gtk_widget_class_bind_template_child (widget_class, CcPowerPanel, device_heading);
  gtk_widget_class_bind_template_child (widget_class, CcPowerPanel, device_listbox);
  gtk_widget_class_bind_template_child (widget_class, CcPowerPanel, device_section);
  gtk_widget_class_bind_template_child (widget_class, CcPowerPanel, idle_time_liststore);
  gtk_widget_class_bind_template_child (widget_class, CcPowerPanel, level_sizegroup);
  gtk_widget_class_bind_template_child (widget_class, CcPowerPanel, main_scroll);
  gtk_widget_class_bind_template_child (widget_class, CcPowerPanel, main_box);
  gtk_widget_class_bind_template_child (widget_class, CcPowerPanel, power_button_liststore);
  gtk_widget_class_bind_template_child (widget_class, CcPowerPanel, power_profile_heading);
  gtk_widget_class_bind_template_child (widget_class, CcPowerPanel, power_profile_listbox);
  gtk_widget_class_bind_template_child (widget_class, CcPowerPanel, power_profile_section);
  gtk_widget_class_bind_template_child (widget_class, CcPowerPanel, power_vbox);
  gtk_widget_class_bind_template_child (widget_class, CcPowerPanel, row_sizegroup);
  gtk_widget_class_bind_template_child (widget_class, CcPowerPanel, suspend_on_battery_delay_combo);
  gtk_widget_class_bind_template_child (widget_class, CcPowerPanel, suspend_on_battery_delay_label);
  gtk_widget_class_bind_template_child (widget_class, CcPowerPanel, suspend_on_battery_label);
  gtk_widget_class_bind_template_child (widget_class, CcPowerPanel, suspend_on_battery_switch);
  gtk_widget_class_bind_template_child (widget_class, CcPowerPanel, suspend_on_ac_delay_combo);
  gtk_widget_class_bind_template_child (widget_class, CcPowerPanel, suspend_on_ac_label);
  gtk_widget_class_bind_template_child (widget_class, CcPowerPanel, suspend_on_ac_switch);

  gtk_widget_class_bind_template_callback (widget_class, keynav_failed_cb);
  gtk_widget_class_bind_template_callback (widget_class, power_profiles_row_activated_cb);
}

static void
cc_power_panel_init (CcPowerPanel *self)
{
  g_autofree gchar *battery_label = NULL;
  g_autofree gchar *device_label = NULL;
  g_autofree gchar *power_profile_label = NULL;
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

  battery_label = g_markup_printf_escaped ("<b>%s</b>", _("Battery"));
  gtk_label_set_markup (self->battery_heading, battery_label);

  self->boxes_reverse = g_list_prepend (self->boxes_reverse, self->battery_listbox);
  gtk_list_box_set_header_func (self->battery_listbox,
                                cc_list_box_update_header_func,
                                NULL, NULL);
  gtk_list_box_set_sort_func (self->battery_listbox,
                              (GtkListBoxSortFunc)battery_sort_func, NULL, NULL);

  device_label = g_markup_printf_escaped ("<b>%s</b>", _("Devices"));
  gtk_label_set_markup (self->device_heading, device_label);

  self->boxes_reverse = g_list_prepend (self->boxes_reverse, self->device_listbox);
  gtk_list_box_set_header_func (self->device_listbox,
                                cc_list_box_update_header_func,
                                NULL, NULL);
  gtk_list_box_set_sort_func (self->device_listbox,
                              (GtkListBoxSortFunc)battery_sort_func, NULL, NULL);

  power_profile_label = g_strdup_printf ("<b>%s</b>", _("Power Mode"));
  gtk_label_set_markup (self->power_profile_heading, power_profile_label);
  gtk_list_box_set_sort_func (self->power_profile_listbox,
                              perf_profile_list_box_sort,
                              NULL, NULL);
  gtk_list_box_set_header_func (self->power_profile_listbox,
                                cc_list_box_update_header_func,
                                NULL, NULL);
  setup_power_profiles (self);

  add_power_saving_section (self);
  add_general_section (self);

  self->boxes = g_list_copy (self->boxes_reverse);
  self->boxes = g_list_reverse (self->boxes);

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

  self->focus_adjustment = gtk_scrolled_window_get_vadjustment (self->main_scroll);
  gtk_container_set_focus_vadjustment (GTK_CONTAINER (self->main_box), self->focus_adjustment);
}
