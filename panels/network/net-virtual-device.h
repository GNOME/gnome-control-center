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

#ifndef __NET_VIRTUAL_DEVICE_H
#define __NET_VIRTUAL_DEVICE_H

#include <glib-object.h>

#include "net-device.h"

G_BEGIN_DECLS

#define NET_TYPE_VIRTUAL_DEVICE          (net_virtual_device_get_type ())
#define NET_VIRTUAL_DEVICE(o)            (G_TYPE_CHECK_INSTANCE_CAST ((o), NET_TYPE_VIRTUAL_DEVICE, NetVirtualDevice))
#define NET_VIRTUAL_DEVICE_CLASS(k)      (G_TYPE_CHECK_CLASS_CAST((k), NET_TYPE_VIRTUAL_DEVICE, NetVirtualDeviceClass))
#define NET_IS_VIRTUAL_DEVICE(o)         (G_TYPE_CHECK_INSTANCE_TYPE ((o), NET_TYPE_VIRTUAL_DEVICE))
#define NET_IS_VIRTUAL_DEVICE_CLASS(k)   (G_TYPE_CHECK_CLASS_TYPE ((k), NET_TYPE_VIRTUAL_DEVICE))
#define NET_VIRTUAL_DEVICE_GET_CLASS(o)  (G_TYPE_INSTANCE_GET_CLASS ((o), NET_TYPE_VIRTUAL_DEVICE, NetVirtualDeviceClass))

typedef struct _NetVirtualDevicePrivate   NetVirtualDevicePrivate;
typedef struct _NetVirtualDevice          NetVirtualDevice;
typedef struct _NetVirtualDeviceClass     NetVirtualDeviceClass;

struct _NetVirtualDevice
{
	NetDevice                parent;
	NetVirtualDevicePrivate *priv;
};

struct _NetVirtualDeviceClass
{
	NetDeviceClass           parent_class;

        /* signals */
        void (*device_set)   (NetVirtualDevice *virtual_device,
                              NMDevice         *nm_device);
        void (*device_unset) (NetVirtualDevice *virtual_device,
                              NMDevice         *nm_device);
};

GType net_virtual_device_get_type (void);

void  net_virtual_device_add_row  (NetVirtualDevice *virtual_device,
                                   const char       *label_string,
                                   const char       *property_name);

G_END_DECLS

#endif /* __NET_VIRTUAL_DEVICE_H */

