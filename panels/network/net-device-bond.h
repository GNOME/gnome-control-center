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

#ifndef __NET_DEVICE_BOND_H
#define __NET_DEVICE_BOND_H

#include <glib-object.h>

#include "net-device-simple.h"

G_BEGIN_DECLS

#define NET_TYPE_DEVICE_BOND          (net_device_bond_get_type ())
#define NET_DEVICE_BOND(o)            (G_TYPE_CHECK_INSTANCE_CAST ((o), NET_TYPE_DEVICE_BOND, NetDeviceBond))
#define NET_DEVICE_BOND_CLASS(k)      (G_TYPE_CHECK_CLASS_CAST((k), NET_TYPE_DEVICE_BOND, NetDeviceBondClass))
#define NET_IS_DEVICE_BOND(o)         (G_TYPE_CHECK_INSTANCE_TYPE ((o), NET_TYPE_DEVICE_BOND))
#define NET_IS_DEVICE_BOND_CLASS(k)   (G_TYPE_CHECK_CLASS_TYPE ((k), NET_TYPE_DEVICE_BOND))
#define NET_DEVICE_BOND_GET_CLASS(o)  (G_TYPE_INSTANCE_GET_CLASS ((o), NET_TYPE_DEVICE_BOND, NetDeviceBondClass))

typedef struct _NetDeviceBondPrivate   NetDeviceBondPrivate;
typedef struct _NetDeviceBond          NetDeviceBond;
typedef struct _NetDeviceBondClass     NetDeviceBondClass;

struct _NetDeviceBond
{
         NetDeviceSimple                parent;
         NetDeviceBondPrivate          *priv;
};

struct _NetDeviceBondClass
{
        NetDeviceSimpleClass             parent_class;
};

GType            net_device_bond_get_type      (void);

G_END_DECLS

#endif /* __NET_DEVICE_BOND_H */

