/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2012 Red Hat, Inc.
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

#include <nm-client.h>
#include <nm-device.h>
#include <nm-device-bridge.h>
#include <nm-remote-connection.h>

#include "panel-common.h"
#include "cc-network-panel.h"

#include "net-device-bridge.h"

#define NET_DEVICE_BRIDGE_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), NET_TYPE_DEVICE_BRIDGE, NetDeviceBridgePrivate))

struct _NetDeviceBridgePrivate {
        char *slaves;
};

enum {
        PROP_0,
        PROP_SLAVES,
        PROP_LAST
};

G_DEFINE_TYPE (NetDeviceBridge, net_device_bridge, NET_TYPE_VIRTUAL_DEVICE)

static void
net_device_bridge_get_property (GObject *object,
                              guint prop_id,
                              GValue *value,
                              GParamSpec *pspec)
{
        NetDeviceBridge *device_bridge = NET_DEVICE_BRIDGE (object);
        NetDeviceBridgePrivate *priv = device_bridge->priv;

        switch (prop_id) {
        case PROP_SLAVES:
                g_value_set_string (value, priv->slaves);
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (device_bridge, prop_id, pspec);
                break;

        }
}

static void
net_device_bridge_constructed (GObject *object)
{
        NetDeviceBridge *device_bridge = NET_DEVICE_BRIDGE (object);

        net_virtual_device_add_row (NET_VIRTUAL_DEVICE (device_bridge),
                                    _("Bridge slaves"), "slaves");

        G_OBJECT_CLASS (net_device_bridge_parent_class)->constructed (object);
}

static void
nm_device_slaves_changed (GObject    *object,
                          GParamSpec *pspec,
                          gpointer    user_data)
{
        NetDeviceBridge *device_bridge = NET_DEVICE_BRIDGE (user_data);
        NetDeviceBridgePrivate *priv = device_bridge->priv;
        NMDeviceBridge *nm_device = NM_DEVICE_BRIDGE (object);
        CcNetworkPanel *panel;
        GPtrArray *net_devices;
        NetDevice *net_device;
        NMDevice *slave;
        const GPtrArray *slaves;
        int i, j;
        GString *str;

        g_free (priv->slaves);

        slaves = nm_device_bridge_get_slaves (nm_device);
        if (!slaves) {
                priv->slaves = g_strdup (_("(none)"));
                g_object_notify (G_OBJECT (device_bridge), "slaves");
                return;
        }

        panel = net_object_get_panel (NET_OBJECT (device_bridge));
        net_devices = cc_network_panel_get_devices (panel);

        str = g_string_new (NULL);
        for (i = 0; i < slaves->len; i++) {
                if (i > 0)
                        g_string_append (str, ", ");
                slave = slaves->pdata[i];

                for (j = 0; j < net_devices->len; j++) {
                        net_device = net_devices->pdata[j];
                        if (slave == net_device_get_nm_device (net_device)) {
                                g_string_append (str, net_object_get_title (NET_OBJECT (net_device)));
                                break;
                        }
                }
                if (j == net_devices->len)
                        g_string_append (str, nm_device_get_iface (slave));
        }
        priv->slaves = g_string_free (str, FALSE);
        g_object_notify (G_OBJECT (device_bridge), "slaves");
}

static void
net_device_bridge_device_set (NetVirtualDevice *virtual_device,
                            NMDevice         *nm_device)
{
        NetDeviceBridge *device_bridge = NET_DEVICE_BRIDGE (virtual_device);

        g_signal_connect_object (nm_device, "notify::slaves",
                                 G_CALLBACK (nm_device_slaves_changed), device_bridge, 0);
        nm_device_slaves_changed (G_OBJECT (nm_device), NULL, device_bridge);
}

static void
net_device_bridge_device_unset (NetVirtualDevice *virtual_device,
                              NMDevice         *nm_device)
{
        NetDeviceBridge *device_bridge = NET_DEVICE_BRIDGE (virtual_device);

        g_signal_handlers_disconnect_by_func (nm_device,
                                              G_CALLBACK (nm_device_slaves_changed),
                                              device_bridge);
        nm_device_slaves_changed (G_OBJECT (nm_device), NULL, device_bridge);
}

static void
net_device_bridge_finalize (GObject *object)
{
        NetDeviceBridge *device_bridge = NET_DEVICE_BRIDGE (object);
        NetDeviceBridgePrivate *priv = device_bridge->priv;

        g_free (priv->slaves);

        G_OBJECT_CLASS (net_device_bridge_parent_class)->finalize (object);
}

static void
net_device_bridge_class_init (NetDeviceBridgeClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);
        NetVirtualDeviceClass *virtual_device_class = NET_VIRTUAL_DEVICE_CLASS (klass);
        GParamSpec *pspec;

        object_class->constructed = net_device_bridge_constructed;
        object_class->finalize = net_device_bridge_finalize;
        object_class->get_property = net_device_bridge_get_property;

        virtual_device_class->device_set = net_device_bridge_device_set;
        virtual_device_class->device_unset = net_device_bridge_device_unset;

        pspec = g_param_spec_string ("slaves", NULL, NULL,
                                     NULL,
                                     G_PARAM_READABLE);
        g_object_class_install_property (object_class, PROP_SLAVES, pspec);

        g_type_class_add_private (klass, sizeof (NetDeviceBridgePrivate));
}

static void
net_device_bridge_init (NetDeviceBridge *device_bridge)
{
        device_bridge->priv = NET_DEVICE_BRIDGE_GET_PRIVATE (device_bridge);
}
