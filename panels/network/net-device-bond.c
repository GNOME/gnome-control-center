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
#include <nm-device-bond.h>
#include <nm-remote-connection.h>

#include "panel-common.h"
#include "cc-network-panel.h"

#include "net-device-bond.h"

#define NET_DEVICE_BOND_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), NET_TYPE_DEVICE_BOND, NetDeviceBondPrivate))

struct _NetDeviceBondPrivate {
        char *slaves;
};

enum {
        PROP_0,
        PROP_SLAVES,
        PROP_LAST
};

G_DEFINE_TYPE (NetDeviceBond, net_device_bond, NET_TYPE_VIRTUAL_DEVICE)

static void
net_device_bond_get_property (GObject *object,
                              guint prop_id,
                              GValue *value,
                              GParamSpec *pspec)
{
        NetDeviceBond *device_bond = NET_DEVICE_BOND (object);
        NetDeviceBondPrivate *priv = device_bond->priv;

        switch (prop_id) {
        case PROP_SLAVES:
                g_value_set_string (value, priv->slaves);
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (device_bond, prop_id, pspec);
                break;

        }
}

static void
net_device_bond_constructed (GObject *object)
{
        NetDeviceBond *device_bond = NET_DEVICE_BOND (object);

        net_virtual_device_add_row (NET_VIRTUAL_DEVICE (device_bond),
                                    _("Bond slaves"), "slaves");

        G_OBJECT_CLASS (net_device_bond_parent_class)->constructed (object);
}

static void
nm_device_slaves_changed (GObject    *object,
                          GParamSpec *pspec,
                          gpointer    user_data)
{
        NetDeviceBond *device_bond = NET_DEVICE_BOND (user_data);
        NetDeviceBondPrivate *priv = device_bond->priv;
        NMDeviceBond *nm_device = NM_DEVICE_BOND (object);
        CcNetworkPanel *panel;
        GPtrArray *net_devices;
        NetDevice *net_device;
        NMDevice *slave;
        const GPtrArray *slaves;
        int i, j;
        GString *str;

        g_free (priv->slaves);

        slaves = nm_device_bond_get_slaves (nm_device);
        if (!slaves) {
                priv->slaves = g_strdup (_("(none)"));
                g_object_notify (G_OBJECT (device_bond), "slaves");
                return;
        }

        panel = net_object_get_panel (NET_OBJECT (device_bond));
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
        g_object_notify (G_OBJECT (device_bond), "slaves");
}

static void
net_device_bond_device_set (NetVirtualDevice *virtual_device,
                            NMDevice         *nm_device)
{
        NetDeviceBond *device_bond = NET_DEVICE_BOND (virtual_device);

        g_signal_connect_object (nm_device, "notify::slaves",
                                 G_CALLBACK (nm_device_slaves_changed), device_bond, 0);
        nm_device_slaves_changed (G_OBJECT (nm_device), NULL, device_bond);
}

static void
net_device_bond_device_unset (NetVirtualDevice *virtual_device,
                              NMDevice         *nm_device)
{
        NetDeviceBond *device_bond = NET_DEVICE_BOND (virtual_device);

        g_signal_handlers_disconnect_by_func (nm_device,
                                              G_CALLBACK (nm_device_slaves_changed),
                                              device_bond);
        nm_device_slaves_changed (G_OBJECT (nm_device), NULL, device_bond);
}

static void
net_device_bond_finalize (GObject *object)
{
        NetDeviceBond *device_bond = NET_DEVICE_BOND (object);
        NetDeviceBondPrivate *priv = device_bond->priv;

        g_free (priv->slaves);

        G_OBJECT_CLASS (net_device_bond_parent_class)->finalize (object);
}

static void
net_device_bond_class_init (NetDeviceBondClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);
        NetVirtualDeviceClass *virtual_device_class = NET_VIRTUAL_DEVICE_CLASS (klass);
        GParamSpec *pspec;

        object_class->constructed = net_device_bond_constructed;
        object_class->finalize = net_device_bond_finalize;
        object_class->get_property = net_device_bond_get_property;

        virtual_device_class->device_set = net_device_bond_device_set;
        virtual_device_class->device_unset = net_device_bond_device_unset;

        pspec = g_param_spec_string ("slaves", NULL, NULL,
                                     NULL,
                                     G_PARAM_READABLE);
        g_object_class_install_property (object_class, PROP_SLAVES, pspec);

        g_type_class_add_private (klass, sizeof (NetDeviceBondPrivate));
}

static void
net_device_bond_init (NetDeviceBond *device_bond)
{
        device_bond->priv = NET_DEVICE_BOND_GET_PRIVATE (device_bond);
}
