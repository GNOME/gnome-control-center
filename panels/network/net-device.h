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

#ifndef __NET_DEVICE_H
#define __NET_DEVICE_H

#include <glib-object.h>

#include <NetworkManager.h>
#include "net-object.h"

G_BEGIN_DECLS

#define NET_TYPE_DEVICE          (net_device_get_type ())
#define NET_DEVICE(o)            (G_TYPE_CHECK_INSTANCE_CAST ((o), NET_TYPE_DEVICE, NetDevice))
#define NET_DEVICE_CLASS(k)      (G_TYPE_CHECK_CLASS_CAST((k), NET_TYPE_DEVICE, NetDeviceClass))
#define NET_IS_DEVICE(o)         (G_TYPE_CHECK_INSTANCE_TYPE ((o), NET_TYPE_DEVICE))
#define NET_IS_DEVICE_CLASS(k)   (G_TYPE_CHECK_CLASS_TYPE ((k), NET_TYPE_DEVICE))
#define NET_DEVICE_GET_CLASS(o)  (G_TYPE_INSTANCE_GET_CLASS ((o), NET_TYPE_DEVICE, NetDeviceClass))

typedef struct _NetDevicePrivate         NetDevicePrivate;
typedef struct _NetDevice                NetDevice;
typedef struct _NetDeviceClass           NetDeviceClass;

struct _NetDevice
{
         NetObject               parent;
};

struct _NetDeviceClass
{
        NetObjectClass               parent_class;

        NMConnection * (*get_find_connection) (NetDevice *device);
};

GType            net_device_get_type                    (void);
NetDevice       *net_device_new                         (void);
NMDevice        *net_device_get_nm_device               (NetDevice      *device);
NMConnection    *net_device_get_find_connection         (NetDevice      *device);

GSList          *net_device_get_valid_connections       (NetDevice      *device);

G_END_DECLS

#endif /* __NET_DEVICE_H */

