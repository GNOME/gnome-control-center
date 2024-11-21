/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2010-2012 Richard Hughes <richard@hughsie.com>
 * Copyright (C) 2012 Thomas Bechtold <thomasbechtold@jpberlin.de>
 * Copyright (C) 2013 Aleksander Morgado <aleksander@gnu.org>
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
#include <glib/gi18n.h>
#include <stdlib.h>

#include "shell/cc-object-storage.h"

#include "cc-network-panel.h"
#include "cc-network-resources.h"

#include <NetworkManager.h>

#include "cc-list-row.h"
#include "cc-net-proxy-page.h"
#include "cc-vpn-page.h"
#include "connection-editor/ce-page.h"
#include "net-device-bluetooth.h"
#include "net-device-ethernet.h"
#include "net-device-mobile.h"
#include "net-device-wifi.h"
#include "net-vpn.h"

#include "panel-common.h"

#include "network-dialogs.h"
#include "connection-editor/net-connection-editor.h"

#include <libmm-glib.h>

typedef enum {
        OPERATION_NULL,
        OPERATION_SHOW_DEVICE,
        OPERATION_CONNECT_MOBILE
} CmdlineOperation;

struct _CcNetworkPanel
{
        CcPanel           parent;

        GPtrArray        *bluetooth_devices;
        GPtrArray        *ethernet_devices;
        GPtrArray        *mobile_devices;
        GHashTable       *nm_device_to_device;

        NMClient         *client;
        MMManager        *modem_manager;
        gboolean          updating_device;

        /* widgets */
        AdwViewStack     *stack;
        AdwPreferencesGroup *device_list;
        GtkWidget        *box_bluetooth;
        GtkWidget        *proxy_row;
        GtkWidget        *save_button;
        CcVpnPage        *vpn_page;

        /* RFKill (Airplane Mode) */
        GDBusProxy         *rfkill_proxy;
        AdwSwitchRow       *rfkill_row;
        GtkWidget          *rfkill_widget;

        /* wireless dialog stuff */
        CmdlineOperation  arg_operation;
        gchar            *arg_device;
        gchar            *arg_access_point;
        gboolean          operation_done;
};

enum {
        PROP_0,
        PROP_PARAMETERS
};

static void handle_argv (CcNetworkPanel *self);
static void device_managed_cb (CcNetworkPanel *self, GParamSpec *pspec, NMDevice *device);

CC_PANEL_REGISTER (CcNetworkPanel, cc_network_panel)

static void
cc_network_panel_get_property (GObject    *object,
                               guint       property_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
        switch (property_id) {
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        }
}

static CmdlineOperation
cmdline_operation_from_string (const gchar *string)
{
        if (g_strcmp0 (string, "connect-3g") == 0)
                return OPERATION_CONNECT_MOBILE;
        if (g_strcmp0 (string, "show-device") == 0)
                return OPERATION_SHOW_DEVICE;

        g_warning ("Invalid additional argument %s", string);
        return OPERATION_NULL;
}

static void
reset_command_line_args (CcNetworkPanel *self)
{
	self->arg_operation = OPERATION_NULL;
	g_clear_pointer (&self->arg_device, g_free);
	g_clear_pointer (&self->arg_access_point, g_free);
}

static gboolean
verify_argv (CcNetworkPanel *self,
	     const char    **args)
{
	switch (self->arg_operation) {
	case OPERATION_CONNECT_MOBILE:
	case OPERATION_SHOW_DEVICE:
		if (self->arg_device == NULL) {
			g_warning ("Operation %s requires an object path", args[0]);
		        return FALSE;
		}
		G_GNUC_FALLTHROUGH;
	default:
		return TRUE;
	}
}

static GPtrArray *
variant_av_to_string_array (GVariant *array)
{
        GVariantIter iter;
        GVariant *v;
        GPtrArray *strv;
        gsize count;
        count = g_variant_iter_init (&iter, array);
        strv = g_ptr_array_sized_new (count + 1);
        while (g_variant_iter_next (&iter, "v", &v)) {
                g_ptr_array_add (strv, (gpointer)g_variant_get_string (v, NULL));
                g_variant_unref (v);
        }
        g_ptr_array_add (strv, NULL); /* NULL-terminate the strv data array */
        return strv;
}

static void
cc_network_panel_set_property (GObject      *object,
                               guint         property_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
        CcNetworkPanel *self = CC_NETWORK_PANEL (object);

        switch (property_id) {
        case PROP_PARAMETERS: {
                GVariant *parameters;

                reset_command_line_args (self);

                parameters = g_value_get_variant (value);
                if (parameters) {
                        g_autoptr(GPtrArray) array = NULL;
                        const gchar **args;
                        array = variant_av_to_string_array (parameters);
                        args = (const gchar **) array->pdata;

                        g_debug ("Invoked with operation %s", args[0]);

                        if (args[0])
                                self->arg_operation = cmdline_operation_from_string (args[0]);
                        if (args[0] && args[1])
                                self->arg_device = g_strdup (args[1]);
                        if (args[0] && args[1] && args[2])
                                self->arg_access_point = g_strdup (args[2]);

                        if (verify_argv (self, (const char **) args) == FALSE) {
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
cc_network_panel_dispose (GObject *object)
{
        CcNetworkPanel *self = CC_NETWORK_PANEL (object);

        g_clear_object (&self->client);
        g_clear_object (&self->modem_manager);

        g_clear_pointer (&self->bluetooth_devices, g_ptr_array_unref);
        g_clear_pointer (&self->ethernet_devices, g_ptr_array_unref);
        g_clear_pointer (&self->mobile_devices, g_ptr_array_unref);
        g_clear_pointer (&self->nm_device_to_device, g_hash_table_destroy);
        g_clear_object (&self->rfkill_proxy);

        G_OBJECT_CLASS (cc_network_panel_parent_class)->dispose (object);
}

static void
cc_network_panel_finalize (GObject *object)
{
        CcNetworkPanel *self = CC_NETWORK_PANEL (object);

        reset_command_line_args (self);

        G_OBJECT_CLASS (cc_network_panel_parent_class)->finalize (object);
}

static const char *
cc_network_panel_get_help_uri (CcPanel *panel)
{
	return "help:gnome-help/net";
}

static void
panel_refresh_device_titles (CcNetworkPanel *self)
{
        g_autoptr(GPtrArray) ndarray = NULL;
        g_autoptr(GPtrArray) nmdarray = NULL;
        GtkWidget **devices;
        NMDevice **nm_devices;
        g_auto(GStrv) titles = NULL;
        guint i, num_devices;

        ndarray = g_ptr_array_new ();
        nmdarray = g_ptr_array_new ();
        for (i = 0; i < self->bluetooth_devices->len; i++) {
                NetDeviceBluetooth *device = g_ptr_array_index (self->bluetooth_devices, i);
                g_ptr_array_add (ndarray, device);
                g_ptr_array_add (nmdarray, net_device_bluetooth_get_device (device));
        }
        for (i = 0; i < self->ethernet_devices->len; i++) {
                NetDeviceEthernet *device = g_ptr_array_index (self->ethernet_devices, i);
                g_ptr_array_add (ndarray, device);
                g_ptr_array_add (nmdarray, net_device_ethernet_get_device (device));
        }
        for (i = 0; i < self->mobile_devices->len; i++) {
                NetDeviceMobile *device = g_ptr_array_index (self->mobile_devices, i);
                g_ptr_array_add (ndarray, device);
                g_ptr_array_add (nmdarray, net_device_mobile_get_device (device));
        }

        if (ndarray->len == 0)
                return;

        devices = (GtkWidget **)ndarray->pdata;
        nm_devices = (NMDevice **)nmdarray->pdata;
        num_devices = ndarray->len;

        titles = nm_device_disambiguate_names (nm_devices, num_devices);
        for (i = 0; i < num_devices; i++) {
                if (NM_IS_DEVICE_BT (nm_devices[i]))
                        adw_preferences_row_set_title (ADW_PREFERENCES_ROW (devices[i]), nm_device_bt_get_name (NM_DEVICE_BT (nm_devices[i])));
                else if (NET_IS_DEVICE_ETHERNET (devices[i])) {
                        GSList *connections;
                        gint n_connections;

                        connections = net_device_get_valid_connections (self->client, nm_devices[i]);
                        n_connections = g_slist_length (connections);

                        /* Only renamed when there are multiple ethernet entries */
                        if (n_connections <= 1)
                                adw_preferences_row_set_title (ADW_PREFERENCES_ROW (devices[i]), titles[i]);
                } else if (NET_IS_DEVICE_MOBILE (devices[i]))
                        net_device_mobile_set_title (NET_DEVICE_MOBILE (devices[i]), titles[i]);
        }
}

static gboolean
handle_argv_for_device (CcNetworkPanel *self,
			NMDevice       *device)
{
        GtkWidget *toplevel = cc_shell_get_toplevel (cc_panel_get_shell (CC_PANEL (self)));

        if (self->arg_operation == OPERATION_NULL)
                return TRUE;

        if (g_strcmp0 (nm_object_get_path (NM_OBJECT (device)), self->arg_device) == 0) {
                if (self->arg_operation == OPERATION_CONNECT_MOBILE) {
                        cc_network_panel_connect_to_3g_network (toplevel, self->client, device);

                        reset_command_line_args (self); /* done */
                        return TRUE;
                } else if (self->arg_operation == OPERATION_SHOW_DEVICE) {
                        reset_command_line_args (self); /* done */
                        return TRUE;
                }
        }

        return FALSE;
}

static void
handle_argv (CcNetworkPanel *self)
{
        gint i;

        if (self->arg_operation == OPERATION_NULL)
                return;

        for (i = 0; i < self->bluetooth_devices->len; i++) {
                NetDeviceBluetooth *device = g_ptr_array_index (self->bluetooth_devices, i);
                if (handle_argv_for_device (self, net_device_bluetooth_get_device (device)))
                        return;
        }
        for (i = 0; i < self->ethernet_devices->len; i++) {
                NetDeviceEthernet *device = g_ptr_array_index (self->ethernet_devices, i);
                if (handle_argv_for_device (self, net_device_ethernet_get_device (device)))
                        return;
        }
        for (i = 0; i < self->mobile_devices->len; i++) {
                NetDeviceMobile *device = g_ptr_array_index (self->mobile_devices, i);
                if (handle_argv_for_device (self, net_device_mobile_get_device (device)))
                        return;
        }
        g_debug ("Could not handle argv operation, no matching device yet?");
}

static gboolean
wwan_panel_supports_modem (GDBusObject *object)
{
        MMObject *mm_object;
        MMModem *modem;
        MMModemCapability capability, supported_capabilities;

        g_assert (G_IS_DBUS_OBJECT (object));

        supported_capabilities = MM_MODEM_CAPABILITY_GSM_UMTS | MM_MODEM_CAPABILITY_LTE;
#if MM_CHECK_VERSION (1,14,0)
        supported_capabilities |= MM_MODEM_CAPABILITY_5GNR;
#endif

        mm_object = MM_OBJECT (object);
        modem = mm_object_get_modem (mm_object);
        capability = mm_modem_get_current_capabilities (modem);

        return capability & supported_capabilities;
}

static void
open_connection_editor (CcNetworkPanel *self,
                        AdwActionRow   *row)
{
        NMDevice *device;
        NMConnection *connection;
        NetConnectionEditor *editor;

        device = g_object_get_data (G_OBJECT (row), "device");
        connection = net_device_get_find_connection (self->client, device);
        if (!connection)
                connection = g_object_get_data (G_OBJECT (row), "connection");

        editor = net_connection_editor_new (connection, device, NULL, self->client);
        g_signal_connect_swapped (G_OBJECT (editor), "done",
                                  G_CALLBACK (cc_panel_pop_visible_subpage), CC_PANEL (self));

        cc_panel_push_subpage (CC_PANEL (self), ADW_NAVIGATION_PAGE (editor));
}

static void
panel_add_device (CcNetworkPanel *self, NMDevice *device)
{
        NMDeviceType type;
        NetDeviceMobile *device_mobile;
        NetDeviceBluetooth *device_bluetooth;
        g_autoptr(GDBusObject) modem_object = NULL;

        /* does already exist */
        if (g_hash_table_lookup (self->nm_device_to_device, device) != NULL)
                return;

        type = nm_device_get_device_type (device);

        g_debug ("device %s type %i path %s",
                 nm_device_get_udi (device), type, nm_object_get_path (NM_OBJECT (device)));

        /* map the NMDeviceType to the GType, or ignore */
        switch (type) {
        case NM_DEVICE_TYPE_ETHERNET:
        case NM_DEVICE_TYPE_INFINIBAND:
                GSList *connections, *l;
                gint n_connections;
                GtkWidget *device_ethernet;

                connections = net_device_get_valid_connections (self->client, device);
                n_connections = g_slist_length (connections);

                /* Ethernet devices with multiple existing profiles */
                for (l = connections; l; l = l->next) {
                        NMConnection *connection = NM_CONNECTION (l->data);

                        device_ethernet = net_device_ethernet_new (self->client, device, connection);
                        g_object_set_data (G_OBJECT (device_ethernet), "device", device);
                        g_object_set_data (G_OBJECT (device_ethernet), "connection", connection);
                        g_signal_connect_swapped (G_OBJECT (device_ethernet), "activated",
                                                  G_CALLBACK (open_connection_editor), self);

                        if (n_connections > 1)
                                adw_preferences_row_set_title (ADW_PREFERENCES_ROW (device_ethernet), nm_connection_get_id (connection));

                        g_ptr_array_add (self->ethernet_devices, device_ethernet);
                        g_hash_table_insert (self->nm_device_to_device, device, device_ethernet);
                        adw_preferences_group_add (self->device_list, device_ethernet);
                        g_debug ("Adding wired connection %s for device %s", nm_connection_get_id (connection), nm_device_get_udi (device));
                }

                /* Single profile device */
                if (!connections) {
                        device_ethernet = net_device_ethernet_new (self->client, device, NULL);

                        g_ptr_array_add (self->ethernet_devices, device_ethernet);
                        g_hash_table_insert (self->nm_device_to_device, device, device_ethernet);
                        adw_preferences_group_add (self->device_list, device_ethernet);

                        panel_refresh_device_titles (self);
                }

                break;
        case NM_DEVICE_TYPE_MODEM:
                if (g_str_has_prefix (nm_device_get_udi (device), "/org/freedesktop/ModemManager1/Modem/")) {
                        if (self->modem_manager == NULL) {
                                g_warning ("Cannot grab information for modem at %s: No ModemManager support",
                                           nm_device_get_udi (device));
                                return;
                        }

                        modem_object = g_dbus_object_manager_get_object (G_DBUS_OBJECT_MANAGER (self->modem_manager),
                                                                         nm_device_get_udi (device));
                        if (modem_object == NULL) {
                                g_warning ("Cannot grab information for modem at %s: Not found",
                                           nm_device_get_udi (device));
                                return;
                        }

                        /* This will be handled by cellular panel */
                        if (wwan_panel_supports_modem (modem_object))
                                return;
                }
                device_mobile = net_device_mobile_new (self->client, device, modem_object);
                //gtk_box_append (GTK_BOX (self->box_wired), GTK_WIDGET (device_mobile));
                g_ptr_array_add (self->mobile_devices, device_mobile);
                g_hash_table_insert (self->nm_device_to_device, device, device_mobile);
                break;
        case NM_DEVICE_TYPE_BT:
                device_bluetooth = net_device_bluetooth_new (self->client, device);

                g_object_set_data (G_OBJECT (device_bluetooth), "device", device);
                g_signal_connect_swapped (G_OBJECT (device_bluetooth), "activated",
                                          G_CALLBACK (open_connection_editor), self);

                g_ptr_array_add (self->bluetooth_devices, device_bluetooth);
                g_hash_table_insert (self->nm_device_to_device, device, device_bluetooth);
                adw_preferences_group_add (ADW_PREFERENCES_GROUP (self->device_list), GTK_WIDGET (device_bluetooth));

                break;
         /* For Wi-Fi and VPN we handle connections separately; we correctly manage
          * them, but not here.
          */
        case NM_DEVICE_TYPE_WIFI:
        case NM_DEVICE_TYPE_TUN:
        /* And the rest we simply cannot deal with currently. */
        default:
                return;
        }
}

static void
panel_remove_device (CcNetworkPanel *self, NMDevice *device)
{
        GtkWidget *net_device;

        net_device = g_hash_table_lookup (self->nm_device_to_device, device);
        if (net_device == NULL)
                return;

        g_ptr_array_remove (self->bluetooth_devices, net_device);
        g_ptr_array_remove (self->ethernet_devices, net_device);
        g_ptr_array_remove (self->mobile_devices, net_device);
        g_hash_table_remove (self->nm_device_to_device, device);

        if (nm_device_get_device_type (device) == NM_DEVICE_TYPE_BT)
                adw_preferences_group_remove (self->device_list, net_device);
        else
                gtk_box_remove (GTK_BOX (gtk_widget_get_parent (net_device)), net_device);
}

static void
connection_state_changed (CcNetworkPanel *self)
{
}

static void
active_connections_changed (CcNetworkPanel *self)
{
        const GPtrArray *connections;
        int i, j;

        g_debug ("Active connections changed:");
        connections = nm_client_get_active_connections (self->client);
        for (i = 0; connections && (i < connections->len); i++) {
                NMActiveConnection *connection;
                const GPtrArray *devices;

                connection = g_ptr_array_index (connections, i);
                g_debug ("    %s", nm_object_get_path (NM_OBJECT (connection)));
                devices = nm_active_connection_get_devices (connection);
                for (j = 0; devices && j < devices->len; j++)
                        g_debug ("           %s", nm_device_get_udi (g_ptr_array_index (devices, j)));

                if (nm_is_wireguard_connection (connection))
                        g_debug ("           WireGuard connection: %s", nm_active_connection_get_id(connection));

                if (NM_IS_VPN_CONNECTION (connection))
                        g_debug ("           VPN base connection: %s", nm_active_connection_get_specific_object_path (connection));

                if (g_object_get_data (G_OBJECT (connection), "has-state-changed-handler") == NULL) {
                        g_signal_connect_object (connection, "notify::state",
                                                 G_CALLBACK (connection_state_changed), self, G_CONNECT_SWAPPED);
                        g_object_set_data (G_OBJECT (connection), "has-state-changed-handler", GINT_TO_POINTER (TRUE));
                }
        }
}

static void
device_managed_cb (CcNetworkPanel *self, GParamSpec *pspec, NMDevice *device)
{
        if (!nm_device_get_managed (device))
                return;

        panel_add_device (self, device);
        panel_refresh_device_titles (self);
}

static void
device_added_cb (CcNetworkPanel *self, NMDevice *device)
{
        g_debug ("New device added");

        if (nm_device_get_managed (device))
                device_managed_cb (self, NULL, device);
        else
                g_signal_connect_object (device, "notify::managed", G_CALLBACK (device_managed_cb), self, G_CONNECT_SWAPPED);
}

static void
device_removed_cb (CcNetworkPanel *self, NMDevice *device)
{
        g_debug ("Device removed");
        panel_remove_device (self, device);
        panel_refresh_device_titles (self);

        g_signal_handlers_disconnect_by_func (device,
                                              G_CALLBACK (device_managed_cb),
                                              self);
}

static void
manager_running (CcNetworkPanel *self)
{
        const GPtrArray *devices;
        int i;

        /* clear all devices we added */
        if (!nm_client_get_nm_running (self->client)) {
                g_debug ("NM disappeared");
                goto out;
        }

        g_debug ("coldplugging devices");
        devices = nm_client_get_devices (self->client);
        if (devices == NULL) {
                g_debug ("No devices to add");
                return;
        }
        for (i = 0; i < devices->len; i++) {
                NMDevice *device = g_ptr_array_index (devices, i);
                device_added_cb (self, device);
        }
out:
        panel_refresh_device_titles (self);

        g_debug ("Calling handle_argv() after cold-plugging devices");
        handle_argv (self);
}

static void
panel_check_network_manager_version (CcNetworkPanel *self)
{
        const gchar *version;

        /* parse running version */
        version = nm_client_get_version (self->client);

        if (version == NULL) {
                adw_view_stack_set_visible_child_name (ADW_VIEW_STACK (self->stack), "nm-error-page");
        } else {
                adw_view_stack_set_visible_child_name (ADW_VIEW_STACK (self->stack), "network-page");
                manager_running (self);
        }
}

/* Airplane Mode */
static inline gboolean
get_cached_rfkill_property (CcNetworkPanel *self,
                            const gchar    *property)
{
  g_autoptr(GVariant) result = NULL;

  result = g_dbus_proxy_get_cached_property (self->rfkill_proxy, property);
  return result ? g_variant_get_boolean (result) : FALSE;
}

static void rfkill_switch_notify_activate_cb (CcNetworkPanel *self);

static void
sync_airplane_mode_switch (CcNetworkPanel *self)
{
  gboolean enabled, should_show, hw_enabled;

  enabled = get_cached_rfkill_property (self, "HasAirplaneMode");
  should_show = get_cached_rfkill_property (self, "ShouldShowAirplaneMode");

  gtk_widget_set_visible (GTK_WIDGET (self->rfkill_widget), enabled && should_show);
  if (!enabled || !should_show)
    return;

  enabled = get_cached_rfkill_property (self, "AirplaneMode");
  hw_enabled = get_cached_rfkill_property (self, "HardwareAirplaneMode");

  enabled |= hw_enabled;

  if (enabled != adw_switch_row_get_active (self->rfkill_row))
    {
      g_signal_handlers_block_by_func (self->rfkill_row,
                                       rfkill_switch_notify_activate_cb,
                                       self);
      g_object_set (self->rfkill_row, "active", enabled, NULL);
      g_signal_handlers_unblock_by_func (self->rfkill_row,
                                         rfkill_switch_notify_activate_cb,
                                         self);
  }

  gtk_widget_set_sensitive (GTK_WIDGET (self->rfkill_row), !hw_enabled);
}

static void
on_rfkill_proxy_properties_changed_cb (CcNetworkPanel *self)
{
  g_debug ("Rfkill properties changed");

  sync_airplane_mode_switch (self);
}

static void
rfkill_proxy_acquired_cb (GObject      *source_object,
                          GAsyncResult *res,
                          gpointer      user_data)
{
  CcNetworkPanel *self;
  GDBusProxy *proxy;
  g_autoptr(GError) error = NULL;

  proxy = cc_object_storage_create_dbus_proxy_finish (res, &error);

  if (error)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_printerr ("Error creating rfkill proxy: %s\n", error->message);

      return;
    }

  self = CC_NETWORK_PANEL (user_data);

  self->rfkill_proxy = proxy;

  g_signal_connect_object (proxy,
                           "g-properties-changed",
                           G_CALLBACK (on_rfkill_proxy_properties_changed_cb),
                           self,
                           G_CONNECT_SWAPPED);

  sync_airplane_mode_switch (self);
}

static void
rfkill_switch_notify_activate_cb (CcNetworkPanel *self)
{
  gboolean enable;

  enable = adw_switch_row_get_active (self->rfkill_row);

  g_dbus_proxy_call (self->rfkill_proxy,
                     "org.freedesktop.DBus.Properties.Set",
                     g_variant_new_parsed ("('org.gnome.SettingsDaemon.Rfkill',"
                                           "'AirplaneMode', %v)",
                                           g_variant_new_boolean (enable)),
                     G_DBUS_CALL_FLAGS_NONE,
                     -1,
                     cc_panel_get_cancellable (CC_PANEL (self)),
                     NULL,
                     NULL);
}

static void
cc_network_panel_map (GtkWidget *widget)
{
        GTK_WIDGET_CLASS (cc_network_panel_parent_class)->map (widget);

        /* is the user compiling against a new version, but not running
         * the daemon? */
        panel_check_network_manager_version (CC_NETWORK_PANEL (widget));
}


static void
cc_network_panel_class_init (CcNetworkPanelClass *klass)
{
        GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
        GObjectClass *object_class = G_OBJECT_CLASS (klass);
	CcPanelClass *panel_class = CC_PANEL_CLASS (klass);

	panel_class->get_help_uri = cc_network_panel_get_help_uri;

        widget_class->map = cc_network_panel_map;

        object_class->get_property = cc_network_panel_get_property;
        object_class->set_property = cc_network_panel_set_property;
        object_class->dispose = cc_network_panel_dispose;
        object_class->finalize = cc_network_panel_finalize;

        g_object_class_override_property (object_class, PROP_PARAMETERS, "parameters");

        gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/network/cc-network-panel.ui");

        gtk_widget_class_bind_template_child (widget_class, CcNetworkPanel, stack);
        gtk_widget_class_bind_template_child (widget_class, CcNetworkPanel, device_list);
        gtk_widget_class_bind_template_child (widget_class, CcNetworkPanel, proxy_row);
        gtk_widget_class_bind_template_child (widget_class, CcNetworkPanel, rfkill_row);
        gtk_widget_class_bind_template_child (widget_class, CcNetworkPanel, rfkill_widget);
        gtk_widget_class_bind_template_child (widget_class, CcNetworkPanel, vpn_page);

        gtk_widget_class_bind_template_callback (widget_class, rfkill_switch_notify_activate_cb);

        g_type_ensure (CC_TYPE_LIST_ROW);
        g_type_ensure (CC_TYPE_NET_PROXY_PAGE);
        g_type_ensure (CC_TYPE_VPN_PAGE);
}

static void
cc_network_panel_init (CcNetworkPanel *self)
{
        g_autoptr(GDBusConnection) system_bus = NULL;
        g_autoptr(GError) error = NULL;

        g_resources_register (cc_network_get_resource ());

        gtk_widget_init_template (GTK_WIDGET (self));

        self->bluetooth_devices = g_ptr_array_new ();
        self->ethernet_devices = g_ptr_array_new ();
        self->mobile_devices = g_ptr_array_new ();
        self->nm_device_to_device = g_hash_table_new (g_direct_hash, g_direct_equal);

        /* Create and store a NMClient instance if it doesn't exist yet */
        if (!cc_object_storage_has_object (CC_OBJECT_NMCLIENT)) {
                g_autoptr(NMClient) client = nm_client_new (NULL, NULL);
                cc_object_storage_add_object (CC_OBJECT_NMCLIENT, client);
        }

        /* use NetworkManager client */
        self->client = cc_object_storage_get_object (CC_OBJECT_NMCLIENT);

        g_signal_connect_object (self->client, "notify::nm-running" ,
                                 G_CALLBACK (manager_running), self, G_CONNECT_SWAPPED);
        g_signal_connect_object (self->client, "notify::active-connections",
                                 G_CALLBACK (active_connections_changed), self, G_CONNECT_SWAPPED);
        g_signal_connect_object (self->client, "device-added",
                                 G_CALLBACK (device_added_cb), self, G_CONNECT_SWAPPED);
        g_signal_connect_object (self->client, "device-removed",
                                 G_CALLBACK (device_removed_cb), self, G_CONNECT_SWAPPED);

        /* Setup ModemManager client */
        system_bus = g_bus_get_sync (G_BUS_TYPE_SYSTEM, NULL, &error);
        if (system_bus == NULL) {
                g_warning ("Error connecting to system D-Bus: %s",
                           error->message);
        } else {
                self->modem_manager = mm_manager_new_sync (system_bus,
                                                            G_DBUS_OBJECT_MANAGER_CLIENT_FLAGS_NONE,
                                                            NULL,
                                                            &error);
                if (self->modem_manager == NULL)
                        g_warning ("Error connecting to ModemManager: %s",
                                   error->message);
        }

        /* Acquire Airplane Mode proxy */
        cc_object_storage_create_dbus_proxy (G_BUS_TYPE_SESSION,
                                             G_DBUS_PROXY_FLAGS_NONE,
                                             "org.gnome.SettingsDaemon.Rfkill",
                                             "/org/gnome/SettingsDaemon/Rfkill",
                                             "org.gnome.SettingsDaemon.Rfkill",
                                             cc_panel_get_cancellable (CC_PANEL (self)),
                                             rfkill_proxy_acquired_cb,
                                             self);

        g_debug ("Calling handle_argv() after cold-plugging connections");
        handle_argv (self);
}
