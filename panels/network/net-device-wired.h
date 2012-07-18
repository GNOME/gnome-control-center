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

#ifndef __NET_DEVICE_WIRED_H
#define __NET_DEVICE_WIRED_H

#include <glib-object.h>

#include "net-device.h"

G_BEGIN_DECLS

#define NET_TYPE_DEVICE_WIRED          (net_device_wired_get_type ())
#define NET_DEVICE_WIRED(o)            (G_TYPE_CHECK_INSTANCE_CAST ((o), NET_TYPE_DEVICE_WIRED, NetDeviceWired))
#define NET_DEVICE_WIRED_CLASS(k)      (G_TYPE_CHECK_CLASS_CAST((k), NET_TYPE_DEVICE_WIRED, NetDeviceWiredClass))
#define NET_IS_DEVICE_WIRED(o)         (G_TYPE_CHECK_INSTANCE_TYPE ((o), NET_TYPE_DEVICE_WIRED))
#define NET_IS_DEVICE_WIRED_CLASS(k)   (G_TYPE_CHECK_CLASS_TYPE ((k), NET_TYPE_DEVICE_WIRED))
#define NET_DEVICE_WIRED_GET_CLASS(o)  (G_TYPE_INSTANCE_GET_CLASS ((o), NET_TYPE_DEVICE_WIRED, NetDeviceWiredClass))

typedef struct _NetDeviceWiredPrivate   NetDeviceWiredPrivate;
typedef struct _NetDeviceWired          NetDeviceWired;
typedef struct _NetDeviceWiredClass     NetDeviceWiredClass;

struct _NetDeviceWired
{
         NetDevice                       parent;
         NetDeviceWiredPrivate          *priv;
};

struct _NetDeviceWiredClass
{
        NetDeviceClass                   parent_class;
};

GType            net_device_wired_get_type      (void);

G_END_DECLS

#endif /* __NET_DEVICE_WIRED_H */

