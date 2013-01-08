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

#ifndef __NET_DEVICE_BRIDGE_H
#define __NET_DEVICE_BRIDGE_H

#include <glib-object.h>

#include "net-virtual-device.h"

G_BEGIN_DECLS

#define NET_TYPE_DEVICE_BRIDGE          (net_device_bridge_get_type ())
#define NET_DEVICE_BRIDGE(o)            (G_TYPE_CHECK_INSTANCE_CAST ((o), NET_TYPE_DEVICE_BRIDGE, NetDeviceBridge))
#define NET_DEVICE_BRIDGE_CLASS(k)      (G_TYPE_CHECK_CLASS_CAST((k), NET_TYPE_DEVICE_BRIDGE, NetDeviceBridgeClass))
#define NET_IS_DEVICE_BRIDGE(o)         (G_TYPE_CHECK_INSTANCE_TYPE ((o), NET_TYPE_DEVICE_BRIDGE))
#define NET_IS_DEVICE_BRIDGE_CLASS(k)   (G_TYPE_CHECK_CLASS_TYPE ((k), NET_TYPE_DEVICE_BRIDGE))
#define NET_DEVICE_BRIDGE_GET_CLASS(o)  (G_TYPE_INSTANCE_GET_CLASS ((o), NET_TYPE_DEVICE_BRIDGE, NetDeviceBridgeClass))

typedef struct _NetDeviceBridgePrivate   NetDeviceBridgePrivate;
typedef struct _NetDeviceBridge          NetDeviceBridge;
typedef struct _NetDeviceBridgeClass     NetDeviceBridgeClass;

struct _NetDeviceBridge
{
         NetVirtualDevice               parent;
         NetDeviceBridgePrivate          *priv;
};

struct _NetDeviceBridgeClass
{
        NetVirtualDeviceClass            parent_class;
};

GType            net_device_bridge_get_type      (void);

G_END_DECLS

#endif /* __NET_DEVICE_BRIDGE_H */

