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

#pragma once

#include <glib-object.h>

#include "net-device.h"

G_BEGIN_DECLS

#define NET_TYPE_DEVICE_SIMPLE (net_device_simple_get_type ())
G_DECLARE_DERIVABLE_TYPE (NetDeviceSimple, net_device_simple, NET, DEVICE_SIMPLE, NetDevice)

struct _NetDeviceSimpleClass
{
        NetDeviceClass           parent_class;

        char                    *(*get_speed)  (NetDeviceSimple *device_simple);
};

char *net_device_simple_get_speed              (NetDeviceSimple *device_simple);

void  net_device_simple_add_row                (NetDeviceSimple *device_simple,
                                                const char      *label,
                                                const char      *property_name);

void  net_device_simple_set_show_separator     (NetDeviceSimple *device_simple,
                                                gboolean         show_separator);

G_END_DECLS
