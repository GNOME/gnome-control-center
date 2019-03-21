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

#include "net-device.h"
#include "net-device-mobile.h"
#include "net-device-wifi.h"
#include "net-device-ethernet.h"
#include "net-object.h"
#include "net-proxy.h"
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

        GCancellable     *cancellable;
        GHashTable       *device_to_stack;
        GPtrArray        *devices;
        NMClient         *client;
        MMManager        *modem_manager;
        GtkSizeGroup     *sizegroup;
        gboolean          updating_device;

        /* widgets */
        GtkWidget        *box_proxy;
        GtkWidget        *box_simple;
        GtkWidget        *box_vpn;
        GtkWidget        *box_wired;
        GtkWidget        *container_simple;
        GtkWidget        *empty_listbox;

        /* wireless dialog stuff */
        CmdlineOperation  arg_operation;
        gchar            *arg_device;
        gchar            *arg_access_point;
        gboolean          operation_done;
};

enum {
        PANEL_DEVICES_COLUMN_ICON,
        PANEL_DEVICES_COLUMN_OBJECT,
        PANEL_DEVICES_COLUMN_LAST
};

enum {
        PROP_0,
        PROP_PARAMETERS
};

static NetObject *find_net_object_by_id (CcNetworkPanel *panel, const gchar *id);
static void handle_argv (CcNetworkPanel *panel);

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
                        GPtrArray *array;
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
                                g_ptr_array_unref (array);
                                return;
                        }
                        g_ptr_array_unref (array);
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

        g_cancellable_cancel (self->cancellable);

        g_clear_object (&self->cancellable);
        g_clear_object (&self->client);
        g_clear_object (&self->modem_manager);

        g_clear_pointer (&self->device_to_stack, g_hash_table_destroy);
        g_clear_pointer (&self->devices, g_ptr_array_unref);

        G_OBJECT_CLASS (cc_network_panel_parent_class)->dispose (object);
}

static void
cc_network_panel_finalize (GObject *object)
{
        CcNetworkPanel *panel = CC_NETWORK_PANEL (object);

        reset_command_line_args (panel);

        G_OBJECT_CLASS (cc_network_panel_parent_class)->finalize (object);
}

static const char *
cc_network_panel_get_help_uri (CcPanel *panel)
{
	return "help:gnome-help/net";
}

static void
object_removed_cb (NetObject *object, CcNetworkPanel *panel)
{
        GtkWidget *stack;

        /* remove device */
        stack = g_hash_table_lookup (panel->device_to_stack, object);
        if (stack != NULL)
                gtk_widget_destroy (stack);
}

GPtrArray *
cc_network_panel_get_devices (CcNetworkPanel *panel)
{
        GPtrArray *devices;
        guint i;

        g_return_val_if_fail (CC_IS_NETWORK_PANEL (panel), NULL);

        devices = g_ptr_array_new_with_free_func (g_object_unref);

        for (i = 0; i < panel->devices->len; i++) {
                NetObject *object = g_ptr_array_index (panel->devices, i);

                if (!NET_IS_DEVICE (object))
                        continue;

                g_ptr_array_add (devices, g_object_ref (object));
        }

        return devices;
}

static void
panel_refresh_device_titles (CcNetworkPanel *panel)
{
        GPtrArray *ndarray, *nmdarray;
        NetDevice **devices;
        NMDevice **nm_devices, *nm_device;
        gchar **titles;
        gint i, num_devices;

        ndarray = cc_network_panel_get_devices (panel);
        if (!ndarray->len) {
                g_ptr_array_free (ndarray, TRUE);
                return;
        }

        nmdarray = g_ptr_array_new ();
        for (i = 0; i < ndarray->len; i++) {
                nm_device = net_device_get_nm_device (ndarray->pdata[i]);
                if (nm_device)
                        g_ptr_array_add (nmdarray, nm_device);
                else
                        g_ptr_array_remove_index (ndarray, i--);
        }

        devices = (NetDevice **)ndarray->pdata;
        nm_devices = (NMDevice **)nmdarray->pdata;
        num_devices = ndarray->len;

        titles = nm_device_disambiguate_names (nm_devices, num_devices);
        for (i = 0; i < num_devices; i++) {
                const gchar *bt_name = NULL;

                if (NM_IS_DEVICE_BT (nm_devices[i]))
                        bt_name = nm_device_bt_get_name (NM_DEVICE_BT (nm_devices[i]));

                /* For bluetooth devices, use their device name. */
                if (bt_name)
                        net_object_set_title (NET_OBJECT (devices[i]), bt_name);
                else
                        net_object_set_title (NET_OBJECT (devices[i]), titles[i]);
                g_free (titles[i]);
        }
        g_free (titles);
        g_ptr_array_free (ndarray, TRUE);
        g_ptr_array_free (nmdarray, TRUE);
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

static gboolean
handle_argv_for_connection (CcNetworkPanel *panel,
                            NMConnection   *connection)
{
        if (panel->arg_operation == OPERATION_NULL)
                return TRUE;
        if (panel->arg_operation != OPERATION_SHOW_DEVICE)
                return FALSE;

        if (g_strcmp0 (nm_connection_get_path (connection), panel->arg_device) == 0) {
                reset_command_line_args (panel);
                return TRUE;
        }

        return FALSE;
}


static void
handle_argv (CcNetworkPanel *panel)
{
        gint i;

        if (panel->arg_operation == OPERATION_NULL)
                return;

        for (i = 0; i < panel->devices->len; i++) {
                GObject *object_tmp;
                NMDevice *device;
                NMConnection *connection;
                gboolean done = FALSE;

                object_tmp = g_ptr_array_index (panel->devices, i);

                if (NET_IS_DEVICE (object_tmp)) {
                        g_object_get (object_tmp, "nm-device", &device, NULL);
                        done = handle_argv_for_device (panel, device);
                        g_object_unref (device);
                } else if (NET_IS_VPN (object_tmp)) {
                        g_object_get (object_tmp, "connection", &connection, NULL);
                        done = handle_argv_for_connection (panel, connection);
                        g_object_unref (connection);
                }

                if (done)
                        return;
        }

        g_debug ("Could not handle argv operation, no matching device yet?");
}

/* HACK: this function is basically a workaround. We don't have a single
 * listbox in the VPN section, thus we need to track the separators and the
 * stub row manually.
 */
static void
update_vpn_section (CcNetworkPanel *self)
{
        guint i, n_vpns;

        for (i = 0, n_vpns = 0; i < self->devices->len; i++) {
                NetObject *net_object = g_ptr_array_index (self->devices, i);

                if (!NET_IS_VPN (net_object))
                        continue;

                net_vpn_set_show_separator (NET_VPN (net_object), n_vpns > 0);
                n_vpns++;
        }

        gtk_widget_set_visible (self->empty_listbox, n_vpns == 0);
}

static void
update_simple_section (CcNetworkPanel *self)
{
        guint i, n_simple;

        for (i = 0, n_simple = 0; i < self->devices->len; i++) {
                NetObject *net_object = g_ptr_array_index (self->devices, i);

                /* NetDeviceSimple but none of the subclasses */
                if (G_OBJECT_TYPE (net_object) != NET_TYPE_DEVICE_SIMPLE)
                        continue;

                net_device_simple_set_show_separator (NET_DEVICE_SIMPLE (net_object), n_simple > 0);
                n_simple++;
        }

        gtk_widget_set_visible (self->container_simple, n_simple > 0);
}

static GtkWidget *
add_device_stack (CcNetworkPanel *self, NetObject *object)
{
        GtkWidget *stack;

        stack = gtk_stack_new ();
        gtk_widget_show (stack);
        g_hash_table_insert (self->device_to_stack, object, stack);

        net_object_add_to_stack (object, GTK_STACK (stack), self->sizegroup);

        return stack;
}

static void
panel_add_device (CcNetworkPanel *panel, NMDevice *device)
{
        NMDeviceType type;
        NetDevice *net_device;
        GType device_g_type;
        const char *udi;

        if (!nm_device_get_managed (device))
                return;

        /* do we have an existing object with this id? */
        udi = nm_device_get_udi (device);
        if (find_net_object_by_id (panel, udi) != NULL)
                return;

        type = nm_device_get_device_type (device);

        g_debug ("device %s type %i path %s",
                 udi, type, nm_object_get_path (NM_OBJECT (device)));

        /* map the NMDeviceType to the GType, or ignore */
        switch (type) {
        case NM_DEVICE_TYPE_ETHERNET:
                device_g_type = NET_TYPE_DEVICE_ETHERNET;
                break;
        case NM_DEVICE_TYPE_MODEM:
                device_g_type = NET_TYPE_DEVICE_MOBILE;
                break;
        case NM_DEVICE_TYPE_BT:
                device_g_type = NET_TYPE_DEVICE_SIMPLE;
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

        /* create device */
        net_device = g_object_new (device_g_type,
                                   "panel", panel,
                                   "removable", FALSE,
                                   "cancellable", panel->cancellable,
                                   "client", panel->client,
                                   "nm-device", device,
                                   "id", nm_device_get_udi (device),
                                   NULL);

        if (type == NM_DEVICE_TYPE_MODEM &&
            g_str_has_prefix (nm_device_get_udi (device), "/org/freedesktop/ModemManager1/Modem/")) {
                GDBusObject *modem_object;

                if (panel->modem_manager == NULL) {
                        g_warning ("Cannot grab information for modem at %s: No ModemManager support",
                                   nm_device_get_udi (device));
                        return;
                }

                modem_object = g_dbus_object_manager_get_object (G_DBUS_OBJECT_MANAGER (panel->modem_manager),
                                                                 nm_device_get_udi (device));
                if (modem_object == NULL) {
                        g_warning ("Cannot grab information for modem at %s: Not found",
                                   nm_device_get_udi (device));
                        return;
                }

                /* Set the modem object in the NetDeviceMobile */
                g_object_set (net_device,
                              "mm-object", modem_object,
                              NULL);
                g_object_unref (modem_object);
        }

        /* add as a panel */
        if (device_g_type != NET_TYPE_DEVICE) {
                GtkWidget *stack;

                stack = add_device_stack (panel, NET_OBJECT (net_device));

                if (device_g_type == NET_TYPE_DEVICE_SIMPLE)
                        gtk_container_add (GTK_CONTAINER (panel->box_simple), stack);
                else
                        gtk_container_add (GTK_CONTAINER (panel->box_wired), stack);
        }

        /* Add to the devices array */
        g_ptr_array_add (panel->devices, net_device);

        /* Update the device_simple section if we're adding a simple
         * device. This is a temporary solution though, for these will
         * be handled by the future Mobile Broadband panel */
        if (device_g_type == NET_TYPE_DEVICE_SIMPLE)
                update_simple_section (panel);

        g_signal_connect_object (net_device, "removed",
                                 G_CALLBACK (object_removed_cb), panel, 0);
}

static void
panel_remove_device (CcNetworkPanel *panel, NMDevice *device)
{
        NetObject *object;

        /* remove device from array */
        object = find_net_object_by_id (panel, nm_device_get_udi (device));

        if (object == NULL)
                return;

        /* NMObject will not fire the "removed" signal, so handle the UI removal explicitly */
        object_removed_cb (object, panel);
        g_ptr_array_remove (panel->devices, object);

        /* update vpn widgets */
        update_vpn_section (panel);

        /* update device_simple widgets */
        update_simple_section (panel);
}

static void
panel_add_proxy_device (CcNetworkPanel *panel)
{
        GtkWidget *stack;
        NetProxy *proxy;

        proxy = net_proxy_new ();

        /* add proxy to stack */
        stack = add_device_stack (panel, NET_OBJECT (proxy));
        gtk_container_add (GTK_CONTAINER (panel->box_proxy), stack);

        /* add proxy to device list */
        net_object_set_title (NET_OBJECT (proxy), _("Network proxy"));

        /* NOTE: No connect to notify::title here as it is guaranteed to not
         *       be changed by anyone.*/
        g_ptr_array_add (panel->devices, proxy);
}

static void
connection_state_changed (NMActiveConnection *c, GParamSpec *pspec, CcNetworkPanel *panel)
{
}

static void
active_connections_changed (NMClient *client, GParamSpec *pspec, gpointer user_data)
{
        CcNetworkPanel *panel = user_data;
        const GPtrArray *connections;
        int i, j;

        g_debug ("Active connections changed:");
        connections = nm_client_get_active_connections (client);
        for (i = 0; connections && (i < connections->len); i++) {
                NMActiveConnection *connection;
                const GPtrArray *devices;

                connection = g_ptr_array_index (connections, i);
                g_debug ("    %s", nm_object_get_path (NM_OBJECT (connection)));
                devices = nm_active_connection_get_devices (connection);
                for (j = 0; devices && j < devices->len; j++)
                        g_debug ("           %s", nm_device_get_udi (g_ptr_array_index (devices, j)));
                if (NM_IS_VPN_CONNECTION (connection))
                        g_debug ("           VPN base connection: %s", nm_active_connection_get_specific_object_path (connection));

                if (g_object_get_data (G_OBJECT (connection), "has-state-changed-handler") == NULL) {
                        g_signal_connect_object (connection, "notify::state",
                                                 G_CALLBACK (connection_state_changed), panel, 0);
                        g_object_set_data (G_OBJECT (connection), "has-state-changed-handler", GINT_TO_POINTER (TRUE));
                }
        }
}

static void
device_added_cb (NMClient *client, NMDevice *device, CcNetworkPanel *panel)
{
        g_debug ("New device added");
        panel_add_device (panel, device);
        panel_refresh_device_titles (panel);
}

static void
device_removed_cb (NMClient *client, NMDevice *device, CcNetworkPanel *panel)
{
        g_debug ("Device removed");
        panel_remove_device (panel, device);
        panel_refresh_device_titles (panel);
}

static void
manager_running (NMClient *client, GParamSpec *pspec, gpointer user_data)
{
        const GPtrArray *devices;
        int i;
        NMDevice *device_tmp;
        CcNetworkPanel *panel = CC_NETWORK_PANEL (user_data);

        /* clear all devices we added */
        if (!nm_client_get_nm_running (client)) {
                g_debug ("NM disappeared");
                panel_add_proxy_device (panel);
                goto out;
        }

        g_debug ("coldplugging devices");
        devices = nm_client_get_devices (client);
        if (devices == NULL) {
                g_debug ("No devices to add");
                return;
        }
        for (i = 0; i < devices->len; i++) {
                device_tmp = g_ptr_array_index (devices, i);
                panel_add_device (panel, device_tmp);
        }
out:
        panel_refresh_device_titles (panel);

        g_debug ("Calling handle_argv() after cold-plugging devices");
        handle_argv (panel);
}

static NetObject *
find_net_object_by_id (CcNetworkPanel *panel, const gchar *id)
{
        NetObject *object_tmp;
        NetObject *object = NULL;
        guint i;

        for (i = 0; i < panel->devices->len; i++) {
                object_tmp = g_ptr_array_index (panel->devices, i);

                if (g_strcmp0 (net_object_get_id (object_tmp), id) == 0) {
                        object = object_tmp;
                        break;
                }
        }

        return object;
}

static void
panel_add_vpn_device (CcNetworkPanel *panel, NMConnection *connection)
{
        GtkWidget *stack;
        gchar *title;
        NetVpn *net_vpn;
        const gchar *id;

        /* does already exist */
        id = nm_connection_get_path (connection);
        if (find_net_object_by_id (panel, id) != NULL)
                return;

        /* add as a VPN object */
        net_vpn = g_object_new (NET_TYPE_VPN,
                                "panel", panel,
                                "removable", TRUE,
                                "id", id,
                                "connection", connection,
                                "client", panel->client,
                                NULL);
        g_signal_connect_object (net_vpn, "removed",
                                 G_CALLBACK (object_removed_cb), panel, 0);

        /* add as a panel */
        stack = add_device_stack (panel, NET_OBJECT (net_vpn));
        gtk_container_add (GTK_CONTAINER (panel->box_vpn), stack);

        title = g_strdup_printf (_("%s VPN"), nm_connection_get_id (connection));

        net_object_set_title (NET_OBJECT (net_vpn), title);

        /* store in the devices array */
        g_ptr_array_add (panel->devices, net_vpn);

        g_free (title);

        /* update vpn widgets */
        update_vpn_section (panel);
}

static void
add_connection (CcNetworkPanel *panel,
                NMConnection *connection)
{
        NMSettingConnection *s_con;
        const gchar *type, *iface;

        s_con = NM_SETTING_CONNECTION (nm_connection_get_setting (connection,
                                                                  NM_TYPE_SETTING_CONNECTION));
        type = nm_setting_connection_get_connection_type (s_con);
        iface = nm_connection_get_interface_name (connection);
        if (g_strcmp0 (type, "vpn") != 0 && iface == NULL)
                return;

        /* Don't add the libvirtd bridge to the UI */
        if (g_strcmp0 (nm_setting_connection_get_interface_name (s_con), "virbr0") == 0)
                return;

        g_debug ("add %s/%s remote connection: %s",
                 type, g_type_name_from_instance ((GTypeInstance*)connection),
                 nm_connection_get_path (connection));
        if (!iface)
                panel_add_vpn_device (panel, connection);
}

static void
notify_connection_added_cb (NMClient           *client,
                            NMRemoteConnection *connection,
                            CcNetworkPanel     *panel)
{
        add_connection (panel, NM_CONNECTION (connection));
}

static void
panel_check_network_manager_version (CcNetworkPanel *panel)
{
        GtkWidget *box;
        GtkWidget *label;
        gchar *markup;
        const gchar *version;

        /* parse running version */
        version = nm_client_get_version (panel->client);
        if (version == NULL) {
                gtk_container_remove (GTK_CONTAINER (panel), gtk_bin_get_child (GTK_BIN (panel)));

                box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 20);
                gtk_box_set_homogeneous (GTK_BOX (box), TRUE);
                gtk_widget_set_vexpand (box, TRUE);
                gtk_container_add (GTK_CONTAINER (panel), box);

                label = gtk_label_new (_("Oops, something has gone wrong. Please contact your software vendor."));
                gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
                gtk_widget_set_valign (label, GTK_ALIGN_END);
                gtk_box_pack_start (GTK_BOX (box), label, TRUE, TRUE, 0);

                markup = g_strdup_printf ("<small><tt>%s</tt></small>",
                                          _("NetworkManager needs to be running."));
                label = gtk_label_new (NULL);
                gtk_label_set_markup (GTK_LABEL (label), markup);
                gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
                gtk_widget_set_valign (label, GTK_ALIGN_START);
                gtk_box_pack_start (GTK_BOX (box), label, TRUE, TRUE, 0);

                gtk_widget_show_all (box);
                g_free (markup);
        } else {
                manager_running (panel->client, NULL, panel);
        }
}

static void
create_connection_cb (GtkWidget      *button,
                      CcNetworkPanel *self)
{
        NetConnectionEditor *editor;
        GtkWindow *toplevel;

        toplevel = GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (self)));
        editor = net_connection_editor_new (toplevel, NULL, NULL, NULL, self->client);
        net_connection_editor_run (editor);
}

static void
on_toplevel_map (GtkWidget      *widget,
                 CcNetworkPanel *panel)
{
        /* is the user compiling against a new version, but not running
         * the daemon? */
        panel_check_network_manager_version (panel);
}


static void
cc_network_panel_class_init (CcNetworkPanelClass *klass)
{
        GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
        GObjectClass *object_class = G_OBJECT_CLASS (klass);
	CcPanelClass *panel_class = CC_PANEL_CLASS (klass);

	panel_class->get_help_uri = cc_network_panel_get_help_uri;

        object_class->get_property = cc_network_panel_get_property;
        object_class->set_property = cc_network_panel_set_property;
        object_class->dispose = cc_network_panel_dispose;
        object_class->finalize = cc_network_panel_finalize;

        g_object_class_override_property (object_class, PROP_PARAMETERS, "parameters");

        gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/network/cc-network-panel.ui");

        gtk_widget_class_bind_template_child (widget_class, CcNetworkPanel, box_proxy);
        gtk_widget_class_bind_template_child (widget_class, CcNetworkPanel, box_simple);
        gtk_widget_class_bind_template_child (widget_class, CcNetworkPanel, box_vpn);
        gtk_widget_class_bind_template_child (widget_class, CcNetworkPanel, box_wired);
        gtk_widget_class_bind_template_child (widget_class, CcNetworkPanel, container_simple);
        gtk_widget_class_bind_template_child (widget_class, CcNetworkPanel, empty_listbox);
        gtk_widget_class_bind_template_child (widget_class, CcNetworkPanel, sizegroup);

        gtk_widget_class_bind_template_callback (widget_class, create_connection_cb);
}

static void
cc_network_panel_init (CcNetworkPanel *panel)
{
        GError *error = NULL;
        GtkWidget *toplevel;
        GDBusConnection *system_bus;
        const GPtrArray *connections;
        guint i;

        g_resources_register (cc_network_get_resource ());

        gtk_widget_init_template (GTK_WIDGET (panel));

        panel->cancellable = g_cancellable_new ();
        panel->devices = g_ptr_array_new_with_free_func (g_object_unref);
        panel->device_to_stack = g_hash_table_new (g_direct_hash, g_direct_equal);

        /* add the virtual proxy device */
        panel_add_proxy_device (panel);

        /* Create and store a NMClient instance if it doesn't exist yet */
        if (!cc_object_storage_has_object (CC_OBJECT_NMCLIENT)) {
                NMClient *client = nm_client_new (NULL, NULL);
                cc_object_storage_add_object (CC_OBJECT_NMCLIENT, client);
                g_object_unref (client);
        }

        /* use NetworkManager client */
        panel->client = cc_object_storage_get_object (CC_OBJECT_NMCLIENT);

        g_signal_connect_object (panel->client, "notify::nm-running" ,
                                 G_CALLBACK (manager_running), panel, 0);
        g_signal_connect_object (panel->client, "notify::active-connections",
                                 G_CALLBACK (active_connections_changed), panel, 0);
        g_signal_connect_object (panel->client, "device-added",
                                 G_CALLBACK (device_added_cb), panel, 0);
        g_signal_connect_object (panel->client, "device-removed",
                                 G_CALLBACK (device_removed_cb), panel, 0);

        /* Setup ModemManager client */
        system_bus = g_bus_get_sync (G_BUS_TYPE_SYSTEM, NULL, &error);
        if (system_bus == NULL) {
                g_warning ("Error connecting to system D-Bus: %s",
                           error->message);
                g_clear_error (&error);
        } else {
                panel->modem_manager = mm_manager_new_sync (system_bus,
                                                            G_DBUS_OBJECT_MANAGER_CLIENT_FLAGS_NONE,
                                                            NULL,
                                                            &error);
                if (panel->modem_manager == NULL) {
                        g_warning ("Error connecting to ModemManager: %s",
                                   error->message);
                        g_clear_error (&error);
                }
                g_object_unref (system_bus);
        }

        /* add remote settings such as VPN settings as virtual devices */
        g_signal_connect_object (panel->client, NM_CLIENT_CONNECTION_ADDED,
                                 G_CALLBACK (notify_connection_added_cb), panel, 0);

        toplevel = gtk_widget_get_toplevel (GTK_WIDGET (panel));
        g_signal_connect_after (toplevel, "map", G_CALLBACK (on_toplevel_map), panel);

        /* Cold-plug existing connections */
        connections = nm_client_get_connections (panel->client);
        if (connections) {
                for (i = 0; i < connections->len; i++)
                        add_connection (panel, connections->pdata[i]);
        }

        g_debug ("Calling handle_argv() after cold-plugging connections");
        handle_argv (panel);
}
