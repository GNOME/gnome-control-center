/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2011-2012 Richard Hughes <richard@hughsie.com>
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

#include <glib-object.h>
#include <glib/gi18n.h>

#include "panel-common.h"

#include "net-vpn.h"
#include "nm-client.h"
#include "nm-remote-connection.h"
#include "nm-setting-vpn.h"

#define NET_VPN_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), NET_TYPE_VPN, NetVpnPrivate))

struct _NetVpnPrivate
{
        GtkBuilder              *builder;
        NMConnection            *connection;
        gchar                   *service_type;
        gboolean                 valid;
        gboolean                 updating_device;
};

enum {
        PROP_0,
        PROP_CONNECTION,
        PROP_LAST
};

G_DEFINE_TYPE (NetVpn, net_vpn, NET_TYPE_OBJECT)

static void
connection_vpn_state_changed_cb (NMVPNConnection *connection,
                                 NMVPNConnectionState state,
                                 NMVPNConnectionStateReason reason,
                                 NetVpn *vpn)
{
        net_object_emit_changed (NET_OBJECT (vpn));
}

static void
connection_changed_cb (NMConnection *connection,
                       NetVpn *vpn)
{
        net_object_emit_changed (NET_OBJECT (vpn));
}

static void
connection_removed_cb (NMConnection *connection,
                       NetVpn *vpn)
{
        net_object_emit_removed (NET_OBJECT (vpn));
}

static char *
net_vpn_connection_to_type (NMConnection *connection)
{
        const gchar *type, *p;

        type = nm_setting_vpn_get_service_type (nm_connection_get_setting_vpn (connection));
        /* Go from "org.freedesktop.NetworkManager.vpnc" to "vpnc" for example */
        p = strrchr (type, '.');
        return g_strdup (p ? p + 1 : type);
}

static void
net_vpn_set_connection (NetVpn *vpn, NMConnection *connection)
{
        NetVpnPrivate *priv = vpn->priv;
        /*
         * vpnc config exmaple:
         * key=IKE DH Group, value=dh2
         * key=xauth-password-type, value=ask
         * key=ipsec-secret-type, value=save
         * key=IPSec gateway, value=66.187.233.252
         * key=NAT Traversal Mode, value=natt
         * key=IPSec ID, value=rh-vpn
         * key=Xauth username, value=rhughes
         */
        priv->connection = g_object_ref (connection);
        g_signal_connect (priv->connection,
                          NM_REMOTE_CONNECTION_REMOVED,
                          G_CALLBACK (connection_removed_cb),
                          vpn);
        g_signal_connect (priv->connection,
                          NM_REMOTE_CONNECTION_UPDATED,
                          G_CALLBACK (connection_changed_cb),
                          vpn);
        if (NM_IS_VPN_CONNECTION (priv->connection)) {
                g_signal_connect (priv->connection,
                                  NM_VPN_CONNECTION_VPN_STATE,
                                  G_CALLBACK (connection_vpn_state_changed_cb),
                                  vpn);
        }

        priv->service_type = net_vpn_connection_to_type (priv->connection);
}

static NMVPNConnectionState
net_vpn_get_state (NetVpn *vpn)
{
        NetVpnPrivate *priv = vpn->priv;
        if (!NM_IS_VPN_CONNECTION (priv->connection))
                return NM_VPN_CONNECTION_STATE_DISCONNECTED;
        return nm_vpn_connection_get_vpn_state (NM_VPN_CONNECTION (priv->connection));
}

/* VPN parameters can be found at:
 * http://git.gnome.org/browse/network-manager-openvpn/tree/src/nm-openvpn-service.h
 * http://git.gnome.org/browse/network-manager-vpnc/tree/src/nm-vpnc-service.h
 * http://git.gnome.org/browse/network-manager-pptp/tree/src/nm-pptp-service.h
 * http://git.gnome.org/browse/network-manager-openconnect/tree/src/nm-openconnect-service.h
 * http://git.gnome.org/browse/network-manager-openswan/tree/src/nm-openswan-service.h
 * See also 'properties' directory in these plugins.
 */
static const gchar *
get_vpn_key_gateway (const char *vpn_type)
{
        if (g_strcmp0 (vpn_type, "openvpn") == 0)     return "remote";
        if (g_strcmp0 (vpn_type, "vpnc") == 0)        return "IPSec gateway";
        if (g_strcmp0 (vpn_type, "pptp") == 0)        return "gateway";
        if (g_strcmp0 (vpn_type, "openconnect") == 0) return "gateway";
        if (g_strcmp0 (vpn_type, "openswan") == 0)    return "right";
        return "";
}

static const gchar *
get_vpn_key_group (const char *vpn_type)
{
        if (g_strcmp0 (vpn_type, "openvpn") == 0)     return "";
        if (g_strcmp0 (vpn_type, "vpnc") == 0)        return "IPSec ID";
        if (g_strcmp0 (vpn_type, "pptp") == 0)        return "";
        if (g_strcmp0 (vpn_type, "openconnect") == 0) return "";
        if (g_strcmp0 (vpn_type, "openswan") == 0)    return "";
        return "";
}

static const gchar *
get_vpn_key_username (const char *vpn_type)
{
        if (g_strcmp0 (vpn_type, "openvpn") == 0)     return "username";
        if (g_strcmp0 (vpn_type, "vpnc") == 0)        return "Xauth username";
        if (g_strcmp0 (vpn_type, "pptp") == 0)        return "user";
        if (g_strcmp0 (vpn_type, "openconnect") == 0) return "username";
        if (g_strcmp0 (vpn_type, "openswan") == 0)    return "leftxauthusername";
        return "";
}

static const gchar *
get_vpn_key_group_password (const char *vpn_type)
{
        if (g_strcmp0 (vpn_type, "openvpn") == 0)     return "";
        if (g_strcmp0 (vpn_type, "vpnc") == 0)        return "Xauth password";
        if (g_strcmp0 (vpn_type, "pptp") == 0)        return "";
        if (g_strcmp0 (vpn_type, "openconnect") == 0) return "";
        if (g_strcmp0 (vpn_type, "openswan") == 0)    return "";
        return "";
}

static const gchar *
net_vpn_get_gateway (NetVpn *vpn)
{
        NetVpnPrivate *priv = vpn->priv;
        const gchar *key;

        key = get_vpn_key_gateway (priv->service_type);
        return nm_setting_vpn_get_data_item (nm_connection_get_setting_vpn (priv->connection), key);
}

static const gchar *
net_vpn_get_id (NetVpn *vpn)
{
        NetVpnPrivate *priv = vpn->priv;
        const gchar *key;

        key = get_vpn_key_group (priv->service_type);
        return nm_setting_vpn_get_data_item (nm_connection_get_setting_vpn (priv->connection), key);
}

static const gchar *
net_vpn_get_username (NetVpn *vpn)
{
        NetVpnPrivate *priv = vpn->priv;
        const gchar *key;

        key = get_vpn_key_username (priv->service_type);
        return nm_setting_vpn_get_data_item (nm_connection_get_setting_vpn (priv->connection), key);
}

static const gchar *
net_vpn_get_password (NetVpn *vpn)
{
        NetVpnPrivate *priv = vpn->priv;
        const gchar *key;

        key = get_vpn_key_group_password (priv->service_type);
        return nm_setting_vpn_get_data_item (nm_connection_get_setting_vpn (priv->connection), key);
}

static void
vpn_proxy_delete (NetObject *object)
{
        NetVpn *vpn = NET_VPN (object);
        nm_remote_connection_delete (NM_REMOTE_CONNECTION (vpn->priv->connection),
                                     NULL, vpn);
}

static GtkWidget *
vpn_proxy_add_to_notebook (NetObject *object,
                           GtkNotebook *notebook,
                           GtkSizeGroup *heading_size_group)
{
        GtkWidget *widget;
        GtkWindow *window;
        NetVpn *vpn = NET_VPN (object);

        /* add widgets to size group */
        widget = GTK_WIDGET (gtk_builder_get_object (vpn->priv->builder,
                                                     "heading_group_password"));
        gtk_size_group_add_widget (heading_size_group, widget);

        /* reparent */
        window = GTK_WINDOW (gtk_builder_get_object (vpn->priv->builder,
                                                     "window_tmp"));
        widget = GTK_WIDGET (gtk_builder_get_object (vpn->priv->builder,
                                                     "vbox9"));
        g_object_ref (widget);
        gtk_container_remove (GTK_CONTAINER (window), widget);
        gtk_notebook_append_page (notebook, widget, NULL);
        g_object_unref (widget);
        return widget;
}

static void
nm_device_refresh_vpn_ui (NetVpn *vpn)
{
        GtkWidget *widget;
        GtkWidget *sw;
        const gchar *status;
        NetVpnPrivate *priv = vpn->priv;
        const GPtrArray *acs;
        NMActiveConnection *a;
        gint i;
        const gchar *path;
        const gchar *apath;
        NMVPNConnectionState state;
        gchar *title;
        NMClient *client;

        sw = GTK_WIDGET (gtk_builder_get_object (priv->builder,
                                                 "device_off_switch"));
        gtk_widget_set_visible (sw, TRUE);

        /* update title */
        widget = GTK_WIDGET (gtk_builder_get_object (priv->builder,
                                                     "label_device"));
        title = g_strdup_printf (_("%s VPN"), nm_connection_get_id (vpn->priv->connection));
        net_object_set_title (NET_OBJECT (vpn), title);
        gtk_label_set_label (GTK_LABEL (widget), title);
        g_free (title);

        /* use status */
        state = net_vpn_get_state (vpn);
        client = net_object_get_client (NET_OBJECT (vpn));
        acs = nm_client_get_active_connections (client);
        if (acs != NULL) {
                path = nm_connection_get_path (vpn->priv->connection);
                for (i = 0; i < acs->len; i++) {
                        a = (NMActiveConnection*)acs->pdata[i];

                        apath = nm_active_connection_get_connection (a);
                        if (NM_IS_VPN_CONNECTION (a) && strcmp (apath, path) == 0) {
                                state = nm_vpn_connection_get_vpn_state (NM_VPN_CONNECTION (a));
                                break;
                        }
                }
        }

        widget = GTK_WIDGET (gtk_builder_get_object (priv->builder,
                                                     "label_status"));
        status = panel_vpn_state_to_localized_string (state);
        gtk_label_set_label (GTK_LABEL (widget), status);
        priv->updating_device = TRUE;
        gtk_switch_set_active (GTK_SWITCH (sw),
                               state != NM_VPN_CONNECTION_STATE_FAILED &&
                               state != NM_VPN_CONNECTION_STATE_DISCONNECTED);
        priv->updating_device = FALSE;

        /* service type */
        panel_set_device_widget_details (vpn->priv->builder,
                                         "service_type",
                                         vpn->priv->service_type);

        /* gateway */
        panel_set_device_widget_details (vpn->priv->builder,
                                         "gateway",
                                         net_vpn_get_gateway (vpn));

        /* groupname */
        panel_set_device_widget_details (vpn->priv->builder,
                                         "group_name",
                                         net_vpn_get_id (vpn));

        /* username */
        panel_set_device_widget_details (vpn->priv->builder,
                                         "username",
                                         net_vpn_get_username (vpn));

        /* password */
        panel_set_device_widget_details (vpn->priv->builder,
                                         "group_password",
                                         net_vpn_get_password (vpn));
}

static void
vpn_proxy_refresh (NetObject *object)
{
        NetVpn *vpn = NET_VPN (object);
        nm_device_refresh_vpn_ui (vpn);
}

static void
device_off_toggled (GtkSwitch *sw,
                    GParamSpec *pspec,
                    NetVpn *vpn)
{
        const gchar *path;
        const GPtrArray *acs;
        gboolean active;
        gint i;
        NMActiveConnection *a;
        NMClient *client;

        if (vpn->priv->updating_device)
                return;

        active = gtk_switch_get_active (sw);
        if (active) {
                client = net_object_get_client (NET_OBJECT (vpn));
                nm_client_activate_connection (client,
                                               vpn->priv->connection, NULL, NULL,
                                               NULL, NULL);
        } else {
                path = nm_connection_get_path (vpn->priv->connection);
                client = net_object_get_client (NET_OBJECT (vpn));
                acs = nm_client_get_active_connections (client);
                for (i = 0; i < acs->len; i++) {
                        a = (NMActiveConnection*)acs->pdata[i];
                        if (strcmp (nm_active_connection_get_connection (a), path) == 0) {
                                nm_client_deactivate_connection (client, a);
                                break;
                        }
                }
        }
}

static void
edit_connection (GtkButton *button, NetVpn *vpn)
{
        net_object_edit (NET_OBJECT (vpn));
}

static void
vpn_proxy_edit (NetObject *object)
{
        const gchar *uuid;
        gchar *cmdline;
        GError *error = NULL;
        NetVpn *vpn = NET_VPN (object);

        uuid = nm_connection_get_uuid (vpn->priv->connection);
        cmdline = g_strdup_printf ("nm-connection-editor --edit %s", uuid);
        g_debug ("Launching '%s'\n", cmdline);
        if (!g_spawn_command_line_async (cmdline, &error)) {
                g_warning ("Failed to launch nm-connection-editor: %s", error->message);
                g_error_free (error);
        }
        g_free (cmdline);
}

/**
 * net_vpn_get_property:
 **/
static void
net_vpn_get_property (GObject *object,
                      guint prop_id,
                      GValue *value,
                      GParamSpec *pspec)
{
        NetVpn *vpn = NET_VPN (object);
        NetVpnPrivate *priv = vpn->priv;

        switch (prop_id) {
        case PROP_CONNECTION:
                g_value_set_object (value, priv->connection);
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (vpn, prop_id, pspec);
                break;
        }
}

/**
 * net_vpn_set_property:
 **/
static void
net_vpn_set_property (GObject *object,
                      guint prop_id,
                      const GValue *value,
                      GParamSpec *pspec)
{
        NetVpn *vpn = NET_VPN (object);

        switch (prop_id) {
        case PROP_CONNECTION:
                net_vpn_set_connection (vpn, g_value_get_object (value));
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (vpn, prop_id, pspec);
                break;
        }
}

static void
net_vpn_constructed (GObject *object)
{
        NetVpn *vpn = NET_VPN (object);

        G_OBJECT_CLASS (net_vpn_parent_class)->constructed (object);

        nm_device_refresh_vpn_ui (vpn);
}

static void
net_vpn_finalize (GObject *object)
{
        NetVpn *vpn = NET_VPN (object);
        NetVpnPrivate *priv = vpn->priv;

        g_object_unref (priv->connection);
        g_free (priv->service_type);

        G_OBJECT_CLASS (net_vpn_parent_class)->finalize (object);
}

static void
net_vpn_class_init (NetVpnClass *klass)
{
        GParamSpec *pspec;
        GObjectClass *object_class = G_OBJECT_CLASS (klass);
        NetObjectClass *parent_class = NET_OBJECT_CLASS (klass);

        object_class->get_property = net_vpn_get_property;
        object_class->set_property = net_vpn_set_property;
        object_class->constructed = net_vpn_constructed;
        object_class->finalize = net_vpn_finalize;
        parent_class->add_to_notebook = vpn_proxy_add_to_notebook;
        parent_class->delete = vpn_proxy_delete;
        parent_class->refresh = vpn_proxy_refresh;
        parent_class->edit = vpn_proxy_edit;

        pspec = g_param_spec_object ("connection", NULL, NULL,
                                     NM_TYPE_CONNECTION,
                                     G_PARAM_READWRITE | G_PARAM_CONSTRUCT);
        g_object_class_install_property (object_class, PROP_CONNECTION, pspec);

        g_type_class_add_private (klass, sizeof (NetVpnPrivate));
}

static void
net_vpn_init (NetVpn *vpn)
{
        GError *error = NULL;
        GtkWidget *widget;

        vpn->priv = NET_VPN_GET_PRIVATE (vpn);

        vpn->priv->builder = gtk_builder_new ();
        gtk_builder_add_from_file (vpn->priv->builder,
                                   GNOMECC_UI_DIR "/network-vpn.ui",
                                   &error);
        if (error != NULL) {
                g_warning ("Could not load interface file: %s", error->message);
                g_error_free (error);
                return;
        }

        widget = GTK_WIDGET (gtk_builder_get_object (vpn->priv->builder,
                                                     "device_off_switch"));
        g_signal_connect (widget, "notify::active",
                          G_CALLBACK (device_off_toggled), vpn);

        widget = GTK_WIDGET (gtk_builder_get_object (vpn->priv->builder,
                                                     "button_options"));
        g_signal_connect (widget, "clicked",
                          G_CALLBACK (edit_connection), vpn);
}
