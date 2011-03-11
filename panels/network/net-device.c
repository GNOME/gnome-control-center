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

#include "net-device.h"

#define NET_DEVICE_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), NET_TYPE_DEVICE, NetDevicePrivate))

struct _NetDevicePrivate
{
        NMDevice                        *nm_device;
};

G_DEFINE_TYPE (NetDevice, net_device, NET_TYPE_OBJECT)

static void
state_changed_cb (NMDevice *device,
                  NMDeviceState new_state,
                  NMDeviceState old_state,
                  NMDeviceStateReason reason,
                  NetDevice *net_device)
{
        net_object_emit_changed (NET_OBJECT (net_device));
}

void
net_device_set_nm_device (NetDevice *device, NMDevice *nm_device)
{
        device->priv->nm_device = g_object_ref (nm_device);
        g_signal_connect (nm_device,
                          "state-changed",
                          G_CALLBACK (state_changed_cb),
                          device);
}

NMDevice *
net_device_get_nm_device (NetDevice *device)
{
        return device->priv->nm_device;
}

static void
net_device_finalize (GObject *object)
{
        NetDevice *device = NET_DEVICE (object);
        NetDevicePrivate *priv = device->priv;

        if (priv->nm_device != NULL)
                g_object_unref (priv->nm_device);

        G_OBJECT_CLASS (net_device_parent_class)->finalize (object);
}

static void
net_device_class_init (NetDeviceClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);
        object_class->finalize = net_device_finalize;
        g_type_class_add_private (klass, sizeof (NetDevicePrivate));
}

static void
net_device_init (NetDevice *device)
{
        device->priv = NET_DEVICE_GET_PRIVATE (device);
}

NetDevice *
net_device_new (void)
{
        NetDevice *device;
        device = g_object_new (NET_TYPE_DEVICE, NULL);
        return NET_DEVICE (device);
}

