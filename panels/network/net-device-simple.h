/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2011-2012 Richard Hughes <richard@hughsie.com>
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

#ifndef __NET_DEVICE_SIMPLE_H
#define __NET_DEVICE_SIMPLE_H

#include <glib-object.h>

#include "net-device.h"

G_BEGIN_DECLS

#define NET_TYPE_DEVICE_SIMPLE          (net_device_simple_get_type ())
#define NET_DEVICE_SIMPLE(o)            (G_TYPE_CHECK_INSTANCE_CAST ((o), NET_TYPE_DEVICE_SIMPLE, NetDeviceSimple))
#define NET_DEVICE_SIMPLE_CLASS(k)      (G_TYPE_CHECK_CLASS_CAST((k), NET_TYPE_DEVICE_SIMPLE, NetDeviceSimpleClass))
#define NET_IS_DEVICE_SIMPLE(o)         (G_TYPE_CHECK_INSTANCE_TYPE ((o), NET_TYPE_DEVICE_SIMPLE))
#define NET_IS_DEVICE_SIMPLE_CLASS(k)   (G_TYPE_CHECK_CLASS_TYPE ((k), NET_TYPE_DEVICE_SIMPLE))
#define NET_DEVICE_SIMPLE_GET_CLASS(o)  (G_TYPE_INSTANCE_GET_CLASS ((o), NET_TYPE_DEVICE_SIMPLE, NetDeviceSimpleClass))

typedef struct _NetDeviceSimplePrivate   NetDeviceSimplePrivate;
typedef struct _NetDeviceSimple          NetDeviceSimple;
typedef struct _NetDeviceSimpleClass     NetDeviceSimpleClass;

struct _NetDeviceSimple
{
         NetDevice               parent;
         NetDeviceSimplePrivate *priv;
};

struct _NetDeviceSimpleClass
{
        NetDeviceClass           parent_class;

        char                    *(*get_speed)  (NetDeviceSimple *device_simple);
};

GType net_device_simple_get_type               (void);

char *net_device_simple_get_speed              (NetDeviceSimple *device_simple);

void  net_device_simple_add_row                (NetDeviceSimple *device_simple,
                                                const char      *label,
                                                const char      *property_name);

void  net_device_simple_set_show_separator     (NetDeviceSimple *device_simple,
                                                gboolean         show_separator);

G_END_DECLS

#endif /* __NET_DEVICE_SIMPLE_H */

