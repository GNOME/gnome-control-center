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

#define NET_VPN_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), NET_TYPE_VPN, NetVpnPrivate))

struct _NetVpnPrivate
{
        NMSettingVPN            *setting;
        NMConnection            *connection;
};

G_DEFINE_TYPE (NetVpn, net_vpn, NET_TYPE_OBJECT)

static void
connection_state_changed_cb (NMVPNConnection *connection,
                             NMVPNConnectionState state,
                             NMVPNConnectionStateReason reason,
                             NetVpn *vpn)
{
        net_object_emit_changed (NET_OBJECT (vpn));
}

void
net_vpn_set_connection (NetVpn *vpn, NMConnection *connection)
{
        NetVpnPrivate *priv = vpn->priv;
        /*
         * key=IKE DH Group, value=dh2
         * key=xauth-password-type, value=ask
         * key=ipsec-secret-type, value=save
         * key=IPSec gateway, value=66.187.233.252
         * key=NAT Traversal Mode, value=natt
         * key=IPSec ID, value=rh-vpn
         * key=Xauth username, value=rhughes
         */
        priv->connection = g_object_ref (connection);
        if (NM_IS_VPN_CONNECTION (priv->connection)) {
                g_signal_connect (priv->connection,
                                  NM_VPN_CONNECTION_VPN_STATE,
                                  G_CALLBACK (connection_state_changed_cb),
                                  vpn);
        }
        priv->setting = NM_SETTING_VPN (nm_connection_get_setting_by_name (connection, "vpn"));
}

NMVPNConnectionState
net_vpn_get_state (NetVpn *vpn)
{
        NetVpnPrivate *priv = vpn->priv;
        if (!NM_IS_VPN_CONNECTION (priv->connection))
                return NM_VPN_CONNECTION_STATE_DISCONNECTED;
        return nm_vpn_connection_get_vpn_state (NM_VPN_CONNECTION (priv->connection));
}

const gchar *
net_vpn_get_gateway (NetVpn *vpn)
{
        NetVpnPrivate *priv = vpn->priv;
        return nm_setting_vpn_get_data_item (priv->setting, "IPSec gateway");
}

const gchar *
net_vpn_get_id (NetVpn *vpn)
{
        NetVpnPrivate *priv = vpn->priv;
        return nm_setting_vpn_get_data_item (priv->setting, "IPSec ID");
}

const gchar *
net_vpn_get_username (NetVpn *vpn)
{
        NetVpnPrivate *priv = vpn->priv;
        return nm_setting_vpn_get_data_item (priv->setting, "Xauth username");
}

const gchar *
net_vpn_get_password (NetVpn *vpn)
{
        NetVpnPrivate *priv = vpn->priv;
        return nm_setting_vpn_get_data_item (priv->setting, "Xauth password");
}

static void
net_vpn_finalize (GObject *object)
{
        NetVpn *vpn = NET_VPN (object);
        NetVpnPrivate *priv = vpn->priv;

        g_object_unref (priv->connection);
        g_object_unref (priv->setting);

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

