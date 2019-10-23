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


struct _NetVpn
{
        NetObject               parent;

        GtkBuilder              *builder;
        GtkBox                  *box;
        GtkLabel                *device_label;
        GtkSwitch               *device_off_switch;
        GtkButton               *options_button;
        GtkSeparator            *separator;

        NMClient                *client;
        NMConnection            *connection;
        NMActiveConnection      *active_connection;
        gchar                   *service_type;
        gboolean                 valid;
        gboolean                 updating_device;
};

G_DEFINE_TYPE (NetVpn, net_vpn, NET_TYPE_OBJECT)

static void nm_device_refresh_vpn_ui (NetVpn *self);

static void
connection_changed_cb (NetVpn *self)
{
        net_object_emit_changed (NET_OBJECT (self));
        nm_device_refresh_vpn_ui (self);
}

static void
connection_removed_cb (NetVpn *self, NMConnection *connection)
{
        if (self->connection == connection)
                net_object_emit_removed (NET_OBJECT (self));
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

static GtkWidget *
vpn_proxy_get_widget (NetObject    *object,
                      GtkSizeGroup *heading_size_group)
{
        NetVpn *self = NET_VPN (object);

        return GTK_WIDGET (self->box);
}

static void
nm_device_refresh_vpn_ui (NetVpn *self)
{
        const GPtrArray *acs;
        NMActiveConnection *a;
        gint i;
        NMVpnConnectionState state;
        g_autofree gchar *title = NULL;

        /* update title */
        /* Translators: this is the title of the connection details
         * window for vpn connections, it is also used to display
         * vpn connections in the device list.
         */
        title = g_strdup_printf (_("%s VPN"), nm_connection_get_id (self->connection));
        net_object_set_title (NET_OBJECT (self), title);
        gtk_label_set_label (self->device_label, title);

        if (self->active_connection) {
                g_signal_handlers_disconnect_by_func (self->active_connection,
                                                      nm_device_refresh_vpn_ui,
                                                      self);
                g_clear_object (&self->active_connection);
        }


        /* Default to disconnected if there is no active connection */
        state = NM_VPN_CONNECTION_STATE_DISCONNECTED;
        acs = nm_client_get_active_connections (self->client);
        if (acs != NULL) {
                const gchar *uuid;
                uuid = nm_connection_get_uuid (self->connection);

                for (i = 0; i < acs->len; i++) {
                        const gchar *auuid;

                        a = (NMActiveConnection*)acs->pdata[i];

                        auuid = nm_active_connection_get_uuid (a);
                        if (NM_IS_VPN_CONNECTION (a) && strcmp (auuid, uuid) == 0) {
                                self->active_connection = g_object_ref (a);
                                g_signal_connect_swapped (a, "notify::vpn-state",
                                                          G_CALLBACK (nm_device_refresh_vpn_ui),
                                                          self);
                                state = nm_vpn_connection_get_vpn_state (NM_VPN_CONNECTION (a));
                                break;
                        }
                }
        }

        self->updating_device = TRUE;
        gtk_switch_set_active (self->device_off_switch,
                               state != NM_VPN_CONNECTION_STATE_FAILED &&
                               state != NM_VPN_CONNECTION_STATE_DISCONNECTED);
        self->updating_device = FALSE;
}

static void
nm_active_connections_changed (NetVpn *self)
{
        nm_device_refresh_vpn_ui (self);
}

static void
vpn_proxy_refresh (NetObject *object)
{
        NetVpn *self = NET_VPN (object);
        nm_device_refresh_vpn_ui (self);
}

static void
device_off_toggled (NetVpn *self)
{
        const GPtrArray *acs;
        gboolean active;
        gint i;
        NMActiveConnection *a;

        if (self->updating_device)
                return;

        active = gtk_switch_get_active (self->device_off_switch);
        if (active) {
                nm_client_activate_connection_async (self->client,
                                                     self->connection, NULL, NULL,
                                                     NULL, NULL, NULL);
        } else {
                const gchar *uuid;

                uuid = nm_connection_get_uuid (self->connection);
                acs = nm_client_get_active_connections (self->client);
                for (i = 0; acs && i < acs->len; i++) {
                        a = (NMActiveConnection*)acs->pdata[i];
                        if (strcmp (nm_active_connection_get_uuid (a), uuid) == 0) {
                                nm_client_deactivate_connection (self->client, a, NULL, NULL);
                                break;
                        }
                }
        }
}

static void
editor_done (NetVpn *self)
{
        net_object_refresh (NET_OBJECT (self));
        g_object_unref (self);
}

static void
edit_connection (NetVpn *self)
{
        GtkWidget *window;
        NetConnectionEditor *editor;
        g_autofree gchar *title = NULL;

        window = gtk_widget_get_toplevel (GTK_WIDGET (self->options_button));

        editor = net_connection_editor_new (GTK_WINDOW (window),
                                            self->connection,
                                            NULL, NULL, self->client);
        title = g_strdup_printf (_("%s VPN"), nm_connection_get_id (self->connection));
        net_connection_editor_set_title (editor, title);

        g_signal_connect_swapped (editor, "done", G_CALLBACK (editor_done), g_object_ref (self));
        net_connection_editor_run (editor);
}

static void
net_vpn_finalize (GObject *object)
{
        NetVpn *self = NET_VPN (object);

        if (self->active_connection)
                g_signal_handlers_disconnect_by_func (self->active_connection,
                                                      nm_device_refresh_vpn_ui,
                                                      self);

        g_clear_object (&self->active_connection);
        g_clear_object (&self->client);
        g_clear_object (&self->connection);
        g_clear_pointer (&self->service_type, g_free);
        g_clear_object (&self->builder);

        G_OBJECT_CLASS (net_vpn_parent_class)->finalize (object);
}

static void
net_vpn_class_init (NetVpnClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);
        NetObjectClass *parent_class = NET_OBJECT_CLASS (klass);

        object_class->finalize = net_vpn_finalize;
        parent_class->get_widget = vpn_proxy_get_widget;
        parent_class->refresh = vpn_proxy_refresh;
}

static void
net_vpn_init (NetVpn *self)
{
        g_autoptr(GError) error = NULL;

        self->builder = gtk_builder_new ();
        gtk_builder_add_from_resource (self->builder,
                                       "/org/gnome/control-center/network/network-vpn.ui",
                                       &error);
        if (error != NULL) {
                g_warning ("Could not load interface file: %s", error->message);
                return;
        }

        self->box = GTK_BOX (gtk_builder_get_object (self->builder, "box"));
        self->device_label = GTK_LABEL (gtk_builder_get_object (self->builder, "device_label"));
        self->device_off_switch = GTK_SWITCH (gtk_builder_get_object (self->builder, "device_off_switch"));
        self->options_button = GTK_BUTTON (gtk_builder_get_object (self->builder, "options_button"));
        self->separator = GTK_SEPARATOR (gtk_builder_get_object (self->builder, "separator"));

        g_signal_connect_swapped (self->device_off_switch, "notify::active",
                                  G_CALLBACK (device_off_toggled), self);

        g_signal_connect_swapped (self->options_button, "clicked",
                                  G_CALLBACK (edit_connection), self);
}

NetVpn *
net_vpn_new (NMConnection *connection,
             NMClient     *client)
{
        NetVpn *self;

        self = g_object_new (NET_TYPE_VPN, NULL);
        self->client = g_object_ref (client);
        self->connection = g_object_ref (connection);

        g_signal_connect_object (self->client,
                                 NM_CLIENT_CONNECTION_REMOVED,
                                 G_CALLBACK (connection_removed_cb),
                                 self, G_CONNECT_SWAPPED);
        g_signal_connect_object (connection,
                                 NM_CONNECTION_CHANGED,
                                 G_CALLBACK (connection_changed_cb),
                                 self, G_CONNECT_SWAPPED);

        self->service_type = net_vpn_connection_to_type (self->connection);

        nm_device_refresh_vpn_ui (self);

        g_signal_connect_object (client,
                                 "notify::active-connections",
                                 G_CALLBACK (nm_active_connections_changed),
                                 self, G_CONNECT_SWAPPED);

        return self;
}

NMConnection *
net_vpn_get_connection (NetVpn *self)
{
        g_return_val_if_fail (NET_IS_VPN (self), NULL);
        return self->connection;
}

void
net_vpn_set_show_separator (NetVpn   *self,
                            gboolean  show_separator)
{
        g_return_if_fail (NET_IS_VPN (self));
        gtk_widget_set_visible (GTK_WIDGET (self->separator), show_separator);
}
