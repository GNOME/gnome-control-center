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

#include "config.h"

#include <glib-object.h>
#include <glib/gi18n.h>

#include <nm-client.h>
#include <nm-device.h>
#include <nm-device-ethernet.h>
#include <nm-remote-connection.h>

#include "panel-common.h"

#include "net-device-ethernet.h"

G_DEFINE_TYPE (NetDeviceEthernet, net_device_ethernet, NET_TYPE_DEVICE_SIMPLE)

static char *
device_ethernet_get_speed (NetDeviceSimple *device_simple)
{
        NMDevice *nm_device;
        guint speed;

        nm_device = net_device_get_nm_device (NET_DEVICE (device_simple));

        speed = nm_device_ethernet_get_speed (NM_DEVICE_ETHERNET (nm_device));
        if (speed > 0) {
                /* Translators: network device speed */
                return g_strdup_printf (_("%d Mb/s"), speed);
        } else
                return NULL;
}

static void
net_device_ethernet_class_init (NetDeviceEthernetClass *klass)
{
        NetDeviceSimpleClass *simple_class = NET_DEVICE_SIMPLE_CLASS (klass);

        simple_class->get_speed = device_ethernet_get_speed;
}

static void
net_device_ethernet_init (NetDeviceEthernet *device_ethernet)
{
}
