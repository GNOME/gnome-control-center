/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2011 Richard Hughes <richard@hughsie.com>
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

#include "net-vpn.h"
#include "nm-setting-vpn.h"
#include "nm-remote-connection.h"

#define NET_VPN_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), NET_TYPE_VPN, NetVpnPrivate))

struct _NetVpnPrivate
{
        NMConnection            *connection;
        gchar                   *service_type;
        gboolean                 valid;
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

void
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

NMConnection *
net_vpn_get_connection (NetVpn *vpn)
{
        return vpn->priv->connection;
}

const gchar *
net_vpn_get_service_type (NetVpn *vpn)
{
        return vpn->priv->service_type;
}

NMVPNConnectionState
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

const gchar *
net_vpn_get_gateway (NetVpn *vpn)
{
        NetVpnPrivate *priv = vpn->priv;
        const gchar *key;

        key = get_vpn_key_gateway (priv->service_type);
        return nm_setting_vpn_get_data_item (nm_connection_get_setting_vpn (priv->connection), key);
}

const gchar *
net_vpn_get_id (NetVpn *vpn)
{
        NetVpnPrivate *priv = vpn->priv;
        const gchar *key;

        key = get_vpn_key_group (priv->service_type);
        return nm_setting_vpn_get_data_item (nm_connection_get_setting_vpn (priv->connection), key);
}

const gchar *
net_vpn_get_username (NetVpn *vpn)
{
        NetVpnPrivate *priv = vpn->priv;
        const gchar *key;

        key = get_vpn_key_username (priv->service_type);
        return nm_setting_vpn_get_data_item (nm_connection_get_setting_vpn (priv->connection), key);
}

const gchar *
net_vpn_get_password (NetVpn *vpn)
{
        NetVpnPrivate *priv = vpn->priv;
        const gchar *key;

        key = get_vpn_key_group_password (priv->service_type);
        return nm_setting_vpn_get_data_item (nm_connection_get_setting_vpn (priv->connection), key);
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
        GObjectClass *object_class = G_OBJECT_CLASS (klass);
        object_class->finalize = net_vpn_finalize;
        g_type_class_add_private (klass, sizeof (NetVpnPrivate));
}

static void
net_vpn_init (NetVpn *vpn)
{
        vpn->priv = NET_VPN_GET_PRIVATE (vpn);
}

NetVpn *
net_vpn_new (void)
{
        NetVpn *vpn;
        vpn = g_object_new (NET_TYPE_VPN, NULL);
        return NET_VPN (vpn);
}

