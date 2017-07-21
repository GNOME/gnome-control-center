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
#include <NetworkManager.h>

#include "panel-common.h"

#include "net-vpn.h"

#include "connection-editor/net-connection-editor.h"

#define NET_VPN_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), NET_TYPE_VPN, NetVpnPrivate))

struct _NetVpnPrivate
{
        GtkBuilder              *builder;
        NMConnection            *connection;
        NMActiveConnection      *active_connection;
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

void
net_vpn_set_show_separator (NetVpn   *self,
                            gboolean  show_separator)
{
        GtkWidget *separator;

        separator = GTK_WIDGET (gtk_builder_get_object (self->priv->builder, "separator"));
        gtk_widget_set_visible (separator, show_separator);
}

static void
connection_vpn_state_changed_cb (NMVpnConnection *connection,
                                 NMVpnConnectionState state,
                                 NMVpnConnectionStateReason reason,
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
connection_removed_cb (NMClient     *client,
                       NMConnection *connection,
                       NetVpn       *vpn)
{
        NetVpnPrivate *priv = vpn->priv;

        if (priv->connection == connection)
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
        NMClient *client;

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

        client = net_object_get_client (NET_OBJECT (vpn));
        g_signal_connect (client,
                          NM_CLIENT_CONNECTION_REMOVED,
                          G_CALLBACK (connection_removed_cb),
                          vpn);
        g_signal_connect (connection,
                          NM_CONNECTION_CHANGED,
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

static NMVpnConnectionState
net_vpn_get_state (NetVpn *vpn)
{
        NetVpnPrivate *priv = vpn->priv;
        if (!NM_IS_VPN_CONNECTION (priv->connection))
                return NM_VPN_CONNECTION_STATE_DISCONNECTED;
        return nm_vpn_connection_get_vpn_state (NM_VPN_CONNECTION (priv->connection));
}

static void
vpn_proxy_delete (NetObject *object)
{
        NetVpn *vpn = NET_VPN (object);
        nm_remote_connection_delete_async (NM_REMOTE_CONNECTION (vpn->priv->connection),
                                           NULL, NULL, vpn);
}

static GtkWidget *
vpn_proxy_add_to_stack (NetObject    *object,
                        GtkStack     *stack,
                        GtkSizeGroup *heading_size_group)
{
        GtkWidget *widget;
        NetVpn *vpn = NET_VPN (object);

        /* add widgets to size group */
        widget = GTK_WIDGET (gtk_builder_get_object (vpn->priv->builder,
                                                     "vbox9"));
        gtk_stack_add_named (stack, widget, net_object_get_id (object));
        return widget;
}

static void
nm_device_refresh_vpn_ui (NetVpn *vpn)
{
        GtkWidget *widget;
        GtkWidget *sw;
        NetVpnPrivate *priv = vpn->priv;
        const GPtrArray *acs;
        NMActiveConnection *a;
        gint i;
        NMVpnConnectionState state;
        gchar *title;
        NMClient *client;

        /* update title */
        widget = GTK_WIDGET (gtk_builder_get_object (priv->builder,
                                                     "label_device"));
        /* Translators: this is the title of the connection details
         * window for vpn connections, it is also used to display
         * vpn connections in the device list.
         */
        title = g_strdup_printf (_("%s VPN"), nm_connection_get_id (vpn->priv->connection));
        net_object_set_title (NET_OBJECT (vpn), title);
        gtk_label_set_label (GTK_LABEL (widget), title);
        g_free (title);

        if (priv->active_connection) {
                g_signal_handlers_disconnect_by_func (vpn->priv->active_connection,
                                                      nm_device_refresh_vpn_ui,
                                                      vpn);
                g_clear_object (&priv->active_connection);
        }


        /* use status */
        state = net_vpn_get_state (vpn);
        client = net_object_get_client (NET_OBJECT (vpn));
        acs = nm_client_get_active_connections (client);
        if (acs != NULL) {
                const gchar *uuid;

                uuid = nm_connection_get_uuid (vpn->priv->connection);
                for (i = 0; i < acs->len; i++) {
                        const gchar *auuid;

                        a = (NMActiveConnection*)acs->pdata[i];

                        auuid = nm_active_connection_get_uuid (a);
                        if (NM_IS_VPN_CONNECTION (a) && strcmp (auuid, uuid) == 0) {
                                priv->active_connection = g_object_ref (a);
                                g_signal_connect_swapped (a, "notify::vpn-state",
                                                          G_CALLBACK (nm_device_refresh_vpn_ui),
                                                          vpn);
                                state = nm_vpn_connection_get_vpn_state (NM_VPN_CONNECTION (a));
                                break;
                        }
                }
        }

        priv->updating_device = TRUE;
        sw = GTK_WIDGET (gtk_builder_get_object (priv->builder, "device_off_switch"));
        gtk_switch_set_active (GTK_SWITCH (sw),
                               state != NM_VPN_CONNECTION_STATE_FAILED &&
                               state != NM_VPN_CONNECTION_STATE_DISCONNECTED);
        priv->updating_device = FALSE;
}

static void
nm_active_connections_changed (NetVpn *vpn)
{
        nm_device_refresh_vpn_ui (vpn);
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
                nm_client_activate_connection_async (client,
                                                     vpn->priv->connection, NULL, NULL,
                                                     NULL, NULL, NULL);
        } else {
                const gchar *uuid;

                uuid = nm_connection_get_uuid (vpn->priv->connection);
                client = net_object_get_client (NET_OBJECT (vpn));
                acs = nm_client_get_active_connections (client);
                for (i = 0; acs && i < acs->len; i++) {
                        a = (NMActiveConnection*)acs->pdata[i];
                        if (strcmp (nm_active_connection_get_uuid (a), uuid) == 0) {
                                nm_client_deactivate_connection (client, a, NULL, NULL);
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
editor_done (NetConnectionEditor *editor,
             gboolean             success,
             NetVpn              *vpn)
{
        g_object_unref (editor);
        net_object_refresh (NET_OBJECT (vpn));
        g_object_unref (vpn);
}

static void
vpn_proxy_edit (NetObject *object)
{
        NetVpn *vpn = NET_VPN (object);
        GtkWidget *button, *window;
        NetConnectionEditor *editor;
        NMClient *client;
        gchar *title;

        button = GTK_WIDGET (gtk_builder_get_object (vpn->priv->builder,
                                                     "button_options"));
        window = gtk_widget_get_toplevel (button);

        client = net_object_get_client (object);

        editor = net_connection_editor_new (GTK_WINDOW (window),
                                            vpn->priv->connection,
                                            NULL, NULL, client);
        title = g_strdup_printf (_("%s VPN"), nm_connection_get_id (vpn->priv->connection));
        net_connection_editor_set_title (editor, title);
        g_free (title);

        g_signal_connect (editor, "done", G_CALLBACK (editor_done), g_object_ref (vpn));
        net_connection_editor_run (editor);
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
        NMClient *client = net_object_get_client (NET_OBJECT (object));

        G_OBJECT_CLASS (net_vpn_parent_class)->constructed (object);

        nm_device_refresh_vpn_ui (vpn);

        g_signal_connect_swapped (client,
                                  "notify::active-connections",
                                  G_CALLBACK (nm_active_connections_changed),
                                  vpn);

}

static void
net_vpn_finalize (GObject *object)
{
        NetVpn *vpn = NET_VPN (object);
        NetVpnPrivate *priv = vpn->priv;
        NMClient *client = net_object_get_client (NET_OBJECT (object));

        if (client) {
                g_signal_handlers_disconnect_by_func (client,
                                                      nm_active_connections_changed,
                                                      vpn);
        }

        if (priv->active_connection) {
                g_signal_handlers_disconnect_by_func (priv->active_connection,
                                                      nm_device_refresh_vpn_ui,
                                                      vpn);
                g_object_unref (priv->active_connection);
        }

        g_signal_handlers_disconnect_by_func (priv->connection,
                                              connection_vpn_state_changed_cb,
                                              vpn);
        g_signal_handlers_disconnect_by_func (priv->connection,
                                              connection_removed_cb,
                                              vpn);
        g_signal_handlers_disconnect_by_func (priv->connection,
                                              connection_changed_cb,
                                              vpn);
        g_object_unref (priv->connection);
        g_free (priv->service_type);

        g_clear_object (&priv->builder);

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
        parent_class->add_to_stack = vpn_proxy_add_to_stack;
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
        gtk_builder_add_from_resource (vpn->priv->builder,
                                       "/org/gnome/control-center/network/network-vpn.ui",
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
