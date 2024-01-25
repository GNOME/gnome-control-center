/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* cc-wwan-page.c
 *
 * Copyright 2019,2022 Purism SPC
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Author(s):
 *   Mohammed Sadiq <sadiq@sadiqpk.org>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "cc-wwan-page"

#include <config.h>
#include <glib/gi18n.h>
#include <libmm-glib.h>

#include "cc-wwan-device.h"
#include "cc-wwan-data.h"
#include "cc-wwan-device-page.h"
#include "cc-wwan-page.h"
#include "cc-wwan-resources.h"

#include "shell/cc-application.h"
#include "shell/cc-log.h"
#include "shell/cc-object-storage.h"

typedef enum {
  OPERATION_NULL,
  OPERATION_SHOW_DEVICE,
} CmdlineOperation;

struct _CcWwanPage
{
  AdwNavigationPage parent_instance;

  AdwToastOverlay  *toast_overlay;
  AdwComboRow      *data_list_row;
  GtkListBox       *data_sim_select_listbox;
  GtkStack         *devices_stack;
  GtkStackSwitcher *devices_switcher;
  GtkSwitch        *enable_switch;
  GtkStack         *main_stack;
  GtkRevealer      *multi_device_revealer;

  GDBusProxy   *rfkill_proxy;
  MMManager    *mm_manager;
  NMClient     *nm_client;

  /* The default device that will be used for data */
  CcWwanDevice *data_device;
  GListStore   *devices;
  GListStore   *data_devices;
  GListStore   *data_devices_name_list;
  GCancellable *cancellable;

  CmdlineOperation  arg_operation;
  char             *arg_device;
};

enum {
  PROP_0,
  PROP_PARAMETERS
};

G_DEFINE_TYPE (CcWwanPage, cc_wwan_page, ADW_TYPE_NAVIGATION_PAGE)


#define CC_TYPE_DATA_DEVICE_ROW (cc_data_device_row_get_type())
G_DECLARE_FINAL_TYPE (CcDataDeviceRow, cc_data_device_row, CC, DATA_DEVICE_ROW, GtkListBoxRow)

struct _CcDataDeviceRow
{
  GtkListBoxRow  parent_instance;

  GtkImage      *ok_emblem;
  CcWwanDevice  *device;
};

G_DEFINE_TYPE (CcDataDeviceRow, cc_data_device_row, GTK_TYPE_LIST_BOX_ROW)

static void
cc_data_device_row_class_init (CcDataDeviceRowClass *klass)
{
}

static void
cc_data_device_row_init (CcDataDeviceRow *row)
{
}

static CmdlineOperation
cmdline_operation_from_string (const gchar *str)
{
  if (g_strcmp0 (str, "show-device") == 0)
    return OPERATION_SHOW_DEVICE;

  g_warning ("Invalid additional argument %s", str);
  return OPERATION_NULL;
}

static void
reset_command_line_args (CcWwanPage *self)
{
  self->arg_operation = OPERATION_NULL;
  g_clear_pointer (&self->arg_device, g_free);
}

static gboolean
verify_argv (CcWwanPage  *self,
             const char  **args)
{
	switch (self->arg_operation)
    {
    case OPERATION_SHOW_DEVICE:
      if (self->arg_device == NULL)
        {
          g_warning ("Operation %s requires an object path", args[0]);
          return FALSE;
        }
    default:
      return TRUE;
    }
}

static void
handle_argv (CcWwanPage *self)
{
  if (self->arg_operation == OPERATION_SHOW_DEVICE &&
      self->arg_operation)
    {
      for (GtkWidget *child = gtk_widget_get_first_child (GTK_WIDGET (self->devices_stack));
           child;
           child = gtk_widget_get_next_sibling (child))
        {
          CcWwanDevice *device;

          device = cc_wwan_device_page_get_device (CC_WWAN_DEVICE_PAGE (child));

          if (g_strcmp0 (cc_wwan_device_get_path (device), self->arg_device) == 0)
            {
              gtk_stack_set_visible_child (GTK_STACK (self->devices_stack), child);
              g_debug ("Opening device %s", self->arg_device);
              reset_command_line_args (self);
              return;
            }
        }
    }
}

static gboolean
wwan_page_device_is_supported (GDBusObject *object)
{
  MMObject *mm_object;
  MMModem *modem;
  MMModemCapability capability;

  g_assert (G_IS_DBUS_OBJECT (object));

  mm_object = MM_OBJECT (object);
  modem = mm_object_get_modem (mm_object);
  capability = mm_modem_get_current_capabilities (modem);

  /* We Support only GSM/3G/LTE devices */
  if (capability & (MM_MODEM_CAPABILITY_GSM_UMTS |
                    MM_MODEM_CAPABILITY_LTE |
                    MM_MODEM_CAPABILITY_LTE_ADVANCED))
    return TRUE;

  return FALSE;
}

static gint
wwan_model_get_item_index (GListModel *model,
                           gpointer    item)
{
  guint i, n_items;

  g_assert (G_IS_LIST_MODEL (model));
  g_assert (G_IS_OBJECT (item));

  n_items = g_list_model_get_n_items (model);

  for (i = 0; i < n_items; i++)
    {
      g_autoptr(GObject) object = NULL;

      object = g_list_model_get_item (model, i);

      if (object == item)
        return i;
    }

  return -1;
}

static CcWwanDevice *
wwan_model_get_item_from_mm_object (GListModel *model,
                                    MMObject   *mm_object)
{
  const gchar *modem_path, *device_path;
  guint i, n_items;

  n_items = g_list_model_get_n_items (model);
  modem_path = mm_object_get_path (mm_object);

  for (i = 0; i < n_items; i++)
    {
      g_autoptr(CcWwanDevice) device = NULL;

      device = g_list_model_get_item (model, i);
      device_path = cc_wwan_device_get_path (device);

      if (g_str_equal (modem_path, device_path))
        return g_steal_pointer (&device);
    }

  return NULL;
}

static void
cc_wwan_page_update_data_selection (CcWwanPage *self)
{
  int i;

  if (!self->data_device)
    return;

  i = wwan_model_get_item_index (G_LIST_MODEL (self->data_devices), self->data_device);

  if (i != -1)
    adw_combo_row_set_selected (self->data_list_row, i);
}

static void
cc_wwan_data_item_activate_cb (CcWwanPage  *self,
                               CcWwanDevice *device)
{
  CcWwanData *data;

  if (device == self->data_device)
    return;

  if (!self->data_device)
    return;

  /* Set lower priority for previously selected APN */
  data = cc_wwan_device_get_data (self->data_device);
  cc_wwan_data_set_priority (data, CC_WWAN_APN_PRIORITY_LOW);
  cc_wwan_data_save_settings (data, NULL, NULL, NULL);

  /* Set high priority for currently selected APN */
  data = cc_wwan_device_get_data (device);
  cc_wwan_data_set_priority (data, CC_WWAN_APN_PRIORITY_HIGH);
  cc_wwan_data_save_settings (data, NULL, NULL, NULL);

  self->data_device = device;
  cc_wwan_page_update_data_selection (self);
}

static void
wwan_on_airplane_off_clicked_cb (CcWwanPage *self)
{
  g_debug ("Airplane Mode Off clicked, disabling airplane mode");
  g_dbus_proxy_call (self->rfkill_proxy,
                     "org.freedesktop.DBus.Properties.Set",
                     g_variant_new_parsed ("('org.gnome.SettingsDaemon.Rfkill',"
                                           "'AirplaneMode', %v)",
                                           g_variant_new_boolean (FALSE)),
                     G_DBUS_CALL_FLAGS_NONE,
                     -1,
                     self->cancellable,
                     NULL,
                     NULL);
}

static void
wwan_data_list_selected_sim_changed_cb (CcWwanPage *self)
{
  CcWwanDevice *device;
  GObject *selected;

  g_assert (CC_IS_WWAN_PAGE (self));

  selected = adw_combo_row_get_selected_item (self->data_list_row);
  if (!selected)
    return;

  device = g_object_get_data (selected, "device");
  cc_wwan_data_item_activate_cb (self, device);
}

static gboolean
cc_wwan_page_get_cached_dbus_property (GDBusProxy  *proxy,
                                        const gchar *property)
{
  g_autoptr(GVariant) result = NULL;

  g_assert (G_IS_DBUS_PROXY (proxy));
  g_assert (property && *property);

  result = g_dbus_proxy_get_cached_property (proxy, property);
  g_assert (!result || g_variant_is_of_type (result, G_VARIANT_TYPE_BOOLEAN));

  return result ? g_variant_get_boolean (result) : FALSE;
}

static void
cc_wwan_page_update_view (CcWwanPage *self)
{
  gboolean has_airplane, is_airplane = FALSE, enabled = FALSE;

  has_airplane = cc_wwan_page_get_cached_dbus_property (self->rfkill_proxy, "HasAirplaneMode");
  has_airplane &= cc_wwan_page_get_cached_dbus_property (self->rfkill_proxy, "ShouldShowAirplaneMode");

  if (has_airplane)
    {
      is_airplane = cc_wwan_page_get_cached_dbus_property (self->rfkill_proxy, "AirplaneMode");
      is_airplane |= cc_wwan_page_get_cached_dbus_property (self->rfkill_proxy, "HardwareAirplaneMode");
    }

  if (self->nm_client)
    enabled = nm_client_wwan_get_enabled (self->nm_client);

  if (has_airplane && is_airplane)
    gtk_stack_set_visible_child_name (self->main_stack, "airplane-mode");
  else if (enabled && g_list_model_get_n_items (G_LIST_MODEL (self->devices)) > 0)
    gtk_stack_set_visible_child_name (self->main_stack, "device-settings");
  else
    gtk_stack_set_visible_child_name (self->main_stack, "no-wwan-devices");

  gtk_widget_set_sensitive (GTK_WIDGET (self->enable_switch), !is_airplane);

  if (enabled)
    gtk_revealer_set_reveal_child (self->multi_device_revealer,
                                   g_list_model_get_n_items (G_LIST_MODEL (self->devices)) > 1);
}

static void
cc_wwan_page_add_device (CcWwanPage  *self,
                          CcWwanDevice *device)
{
  CcWwanDevicePage *device_page;
  g_autofree gchar *operator_name = NULL;
  g_autofree gchar *stack_name = NULL;
  guint n_items;

  g_list_store_append (self->devices, device);

  n_items = g_list_model_get_n_items (G_LIST_MODEL (self->devices));
  operator_name = g_strdup_printf (_("SIM %d"), n_items);
  stack_name = g_strdup_printf ("sim-%d", n_items);

  device_page = cc_wwan_device_page_new (device, GTK_WIDGET (self->toast_overlay));
  cc_wwan_device_page_set_sim_index (device_page, n_items);
  gtk_stack_add_titled (self->devices_stack,
                        GTK_WIDGET (device_page), stack_name, operator_name);
}

static void
cc_wwan_page_update_page_title (CcWwanDevicePage *device_page,
                                 CcWwanPage      *self)
{
  g_autofree gchar *title = NULL;
  g_autofree gchar *name = NULL;
  CcWwanDevice *device;
  GtkStackPage *page;
  gint index;

  g_assert (CC_IS_WWAN_DEVICE_PAGE (device_page));

  device = cc_wwan_device_page_get_device (device_page);

  page = gtk_stack_get_page (GTK_STACK (self->devices_stack), GTK_WIDGET (device_page));
  index  = wwan_model_get_item_index (G_LIST_MODEL (self->devices), device);

  if (index == -1)
    g_return_if_reached ();

  /* index starts with 0, but we need human readable index to be 1+ */
  cc_wwan_device_page_set_sim_index (device_page, index + 1);
  title = g_strdup_printf (_("SIM %d"), index + 1);
  name = g_strdup_printf ("sim-%d", index + 1);
  gtk_stack_page_set_title (page, title);
  gtk_stack_page_set_name (page, name);
}

static void
cc_wwan_page_remove_mm_object (CcWwanPage *self,
                                MMObject    *mm_object)
{
  g_autoptr(CcWwanDevice) device = NULL;
  GtkWidget *device_page;
  g_autofree gchar *stack_name = NULL;
  guint n_items;
  gint index;

  device = wwan_model_get_item_from_mm_object (G_LIST_MODEL (self->devices), mm_object);

  if (!device)
    return;

  index = wwan_model_get_item_index (G_LIST_MODEL (self->data_devices), device);
  if (index != -1) {
    g_list_store_remove (self->data_devices, index);
    g_list_store_remove (self->data_devices_name_list, index);
  }

  index = wwan_model_get_item_index (G_LIST_MODEL (self->devices), device);
  if (index == -1)
    return;

  g_list_store_remove (self->devices, index);
  stack_name = g_strdup_printf ("sim-%d", index + 1);
  device_page = gtk_stack_get_child_by_name (self->devices_stack, stack_name);
  gtk_stack_remove (GTK_STACK (self->devices_stack), device_page);

  n_items = g_list_model_get_n_items (G_LIST_MODEL (self->data_devices));
  g_list_model_items_changed (G_LIST_MODEL (self->data_devices), 0, n_items, n_items);

  for (GtkWidget *child = gtk_widget_get_first_child (GTK_WIDGET (self->devices_stack));
       child;
       child = gtk_widget_get_next_sibling (child))
    cc_wwan_page_update_page_title (CC_WWAN_DEVICE_PAGE (child), self);
}

static void
wwan_page_add_data_device_to_list (CcWwanPage  *self,
                                    CcWwanDevice *device)
{
  g_autoptr(GtkStringObject) str = NULL;
  g_autofree char *operator = NULL;
  int index;

  index = wwan_model_get_item_index (G_LIST_MODEL (self->data_devices), device);
  if (index != -1)
    return;

  g_list_store_append (self->data_devices, device);

  index = wwan_model_get_item_index (G_LIST_MODEL (self->devices), device);
  operator = g_strdup_printf ("SIM %d", index + 1);
  str = gtk_string_object_new (operator);
  g_object_set_data_full (G_OBJECT (str), "device", g_object_ref (device), g_object_unref);
  g_list_store_append (self->data_devices_name_list, str);
}

static void
cc_wwan_page_update_data_connections (CcWwanPage *self)
{
  CcWwanData *device_data, *active_data = NULL;
  guint n_items;
  gint i;

  /*
   * We can’t predict the order in which the data of device is enabled.
   * But we have to keep data store in the same order as device store.
   * So let’s remove every data device and re-add.
   */
  g_list_store_remove_all (self->data_devices);
  g_list_store_remove_all (self->data_devices_name_list);
  n_items = g_list_model_get_n_items (G_LIST_MODEL (self->devices));

  for (i = 0; i < n_items; i++)
    {
      g_autoptr(CcWwanDevice) device = NULL;

      device = g_list_model_get_item (G_LIST_MODEL (self->devices), i);
      device_data = cc_wwan_device_get_data (device);

      if (!device_data)
        continue;

      if ((!active_data ||
           cc_wwan_data_get_priority (device_data) > cc_wwan_data_get_priority (active_data)) &&
          cc_wwan_data_get_enabled (device_data))
        {
          active_data = device_data;
          self->data_device = device;
        }

      if (cc_wwan_data_get_enabled (device_data))
        wwan_page_add_data_device_to_list (self, device);
    }

  if (active_data)
    cc_wwan_page_update_data_selection (self);
}

static void
cc_wwan_page_update_devices (CcWwanPage *self)
{
  GList *devices, *iter;

  devices = g_dbus_object_manager_get_objects (G_DBUS_OBJECT_MANAGER (self->mm_manager));

  for (iter = devices; iter; iter = iter->next)
    {
      MMObject *mm_object = iter->data;
      CcWwanDevice *device;

      if(!wwan_page_device_is_supported (iter->data))
        continue;

      device = cc_wwan_device_new (mm_object, G_OBJECT (self->nm_client));
      cc_wwan_page_add_device (self, device);
      g_signal_connect_object (device, "notify::has-data",
                               G_CALLBACK (cc_wwan_page_update_data_connections),
                               self, G_CONNECT_SWAPPED);

      if (cc_wwan_device_get_data (device))
        wwan_page_add_data_device_to_list (self, device);
    }

  cc_wwan_page_update_data_connections (self);
  handle_argv (self);
}

static void
wwan_page_device_added_cb (CcWwanPage *self,
                            GDBusObject *object)
{
  CcWwanDevice *device;

  if(!wwan_page_device_is_supported (object))
    return;

  device = cc_wwan_device_new (MM_OBJECT (object), G_OBJECT (self->nm_client));
  cc_wwan_page_add_device (self, device);
  g_signal_connect_object (device, "notify::has-data",
                           G_CALLBACK (cc_wwan_page_update_data_connections),
                           self, G_CONNECT_SWAPPED);
  cc_wwan_page_update_view (self);
  handle_argv (self);
}

static void
wwan_page_device_removed_cb (CcWwanPage *self,
                              GDBusObject *object)
{
  if (!wwan_page_device_is_supported (object))
    return;

  cc_wwan_page_remove_mm_object (self, MM_OBJECT (object));

  gtk_revealer_set_reveal_child (self->multi_device_revealer,
                                 g_list_model_get_n_items (G_LIST_MODEL (self->devices)) > 1);
}

static GPtrArray *
variant_av_to_string_array (GVariant *array)
{
  GVariant *v;
  GPtrArray *strv;
  GVariantIter iter;
  gsize count;

  count = g_variant_iter_init (&iter, array);
  strv = g_ptr_array_sized_new (count + 1);

  while (g_variant_iter_next (&iter, "v", &v))
    {
      g_ptr_array_add (strv, (gpointer)g_variant_get_string (v, NULL));
      g_variant_unref (v);
    }
  g_ptr_array_add (strv, NULL); /* NULL-terminate the strv data array */

  return strv;
}

static void
cc_wwan_page_set_property (GObject      *object,
                            guint         property_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
  CcWwanPage *self = CC_WWAN_PAGE (object);

  switch (property_id)
    {
    case PROP_PARAMETERS:
      {
        GVariant *parameters;

        reset_command_line_args (self);

        parameters = g_value_get_variant (value);
        if (parameters)
          {
            g_autoptr(GPtrArray) array = NULL;
            const gchar **args;

            array = variant_av_to_string_array (parameters);
            args = (const gchar **) array->pdata;

            g_debug ("Invoked with operation %s", args[0]);

            if (args[0])
              self->arg_operation = cmdline_operation_from_string (args[0]);
            if (args[0] && args[1])
              self->arg_device = g_strdup (args[1]);

            if (!verify_argv (self, (const char **) args))
              {
                reset_command_line_args (self);
                return;
              }
            g_debug ("Calling handle_argv() after setting property");
            handle_argv (self);
          }
        break;
      }
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
cc_wwan_page_dispose (GObject *object)
{
  CcWwanPage *self = (CcWwanPage *)object;

  g_cancellable_cancel (self->cancellable);

  g_clear_object (&self->devices);
  g_clear_object (&self->data_devices);
  g_clear_object (&self->data_devices_name_list);
  g_clear_object (&self->mm_manager);
  g_clear_object (&self->nm_client);
  g_clear_object (&self->cancellable);
  g_clear_object (&self->rfkill_proxy);
  g_clear_pointer (&self->arg_device, g_free);

  G_OBJECT_CLASS (cc_wwan_page_parent_class)->dispose (object);
}

static void
cc_wwan_page_class_init (CcWwanPageClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = cc_wwan_page_set_property;
  object_class->dispose = cc_wwan_page_dispose;

  g_object_class_override_property (object_class, PROP_PARAMETERS, "parameters");

  gtk_widget_class_set_template_from_resource (widget_class,
                                               "/org/gnome/control-center/wwan/cc-wwan-page.ui");

  gtk_widget_class_bind_template_child (widget_class, CcWwanPage, toast_overlay);
  gtk_widget_class_bind_template_child (widget_class, CcWwanPage, data_list_row);
  gtk_widget_class_bind_template_child (widget_class, CcWwanPage, data_sim_select_listbox);
  gtk_widget_class_bind_template_child (widget_class, CcWwanPage, devices_stack);
  gtk_widget_class_bind_template_child (widget_class, CcWwanPage, devices_switcher);
  gtk_widget_class_bind_template_child (widget_class, CcWwanPage, enable_switch);
  gtk_widget_class_bind_template_child (widget_class, CcWwanPage, main_stack);
  gtk_widget_class_bind_template_child (widget_class, CcWwanPage, multi_device_revealer);

  gtk_widget_class_bind_template_callback (widget_class, wwan_data_list_selected_sim_changed_cb);
  gtk_widget_class_bind_template_callback (widget_class, wwan_on_airplane_off_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, cc_wwan_data_item_activate_cb);
}

static void
cc_wwan_page_init (CcWwanPage *self)
{
  g_autoptr(GError) error = NULL;

  g_resources_register (cc_wwan_get_resource ());

  gtk_widget_init_template (GTK_WIDGET (self));

  self->cancellable = g_cancellable_new ();
  self->devices = g_list_store_new (CC_TYPE_WWAN_DEVICE);
  self->data_devices = g_list_store_new (CC_TYPE_WWAN_DEVICE);
  self->data_devices_name_list = g_list_store_new (GTK_TYPE_STRING_OBJECT);
  adw_combo_row_set_model (ADW_COMBO_ROW (self->data_list_row),
                           G_LIST_MODEL (self->data_devices_name_list));

  if (cc_object_storage_has_object (CC_OBJECT_NMCLIENT))
    {
      self->nm_client = cc_object_storage_get_object (CC_OBJECT_NMCLIENT);
      g_signal_connect_object (self->nm_client,
                               "notify::wwan-enabled",
                               G_CALLBACK (cc_wwan_page_update_view),
                               self, G_CONNECT_SWAPPED);

    }
  else
    {
      g_warn_if_reached ();
    }

  if (self->nm_client)
    {
      g_object_bind_property (self->nm_client, "wwan-enabled",
                              self->enable_switch, "active",
                              G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE);
    }

  if (cc_object_storage_has_object ("CcObjectStorage::mm-manager"))
    {
      self->mm_manager = cc_object_storage_get_object ("CcObjectStorage::mm-manager");

      g_signal_connect_object (self->mm_manager, "object-added",
                               G_CALLBACK (wwan_page_device_added_cb),
                               self, G_CONNECT_SWAPPED);
      g_signal_connect_object (self->mm_manager, "object-removed",
                               G_CALLBACK (wwan_page_device_removed_cb),
                               self, G_CONNECT_SWAPPED);

      cc_wwan_page_update_devices (self);
    }
  else
    {
      g_warn_if_reached ();
    }

  /* Acquire Airplane Mode proxy */
  self->rfkill_proxy = cc_object_storage_create_dbus_proxy_sync (G_BUS_TYPE_SESSION,
                                                                 G_DBUS_PROXY_FLAGS_NONE,
                                                                 "org.gnome.SettingsDaemon.Rfkill",
                                                                 "/org/gnome/SettingsDaemon/Rfkill",
                                                                 "org.gnome.SettingsDaemon.Rfkill",
                                                                 self->cancellable,
                                                                 &error);

  if (error)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_printerr ("Error creating rfkill proxy: %s\n", error->message);
    }
  else
    {
      g_signal_connect_object (self->rfkill_proxy,
                               "g-properties-changed",
                               G_CALLBACK (cc_wwan_page_update_view),
                               self, G_CONNECT_SWAPPED);

      cc_wwan_page_update_view (self);
    }
}
