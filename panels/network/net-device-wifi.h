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

#ifndef __NET_DEVICE_WIFI_H
#define __NET_DEVICE_WIFI_H

#include <glib-object.h>

#include "net-device.h"

G_BEGIN_DECLS

#define NET_TYPE_DEVICE_WIFI          (net_device_wifi_get_type ())
#define NET_DEVICE_WIFI(o)            (G_TYPE_CHECK_INSTANCE_CAST ((o), NET_TYPE_DEVICE_WIFI, NetDeviceWifi))
#define NET_DEVICE_WIFI_CLASS(k)      (G_TYPE_CHECK_CLASS_CAST((k), NET_TYPE_DEVICE_WIFI, NetDeviceWifiClass))
#define NET_IS_DEVICE_WIFI(o)         (G_TYPE_CHECK_INSTANCE_TYPE ((o), NET_TYPE_DEVICE_WIFI))
#define NET_IS_DEVICE_WIFI_CLASS(k)   (G_TYPE_CHECK_CLASS_TYPE ((k), NET_TYPE_DEVICE_WIFI))
#define NET_DEVICE_WIFI_GET_CLASS(o)  (G_TYPE_INSTANCE_GET_CLASS ((o), NET_TYPE_DEVICE_WIFI, NetDeviceWifiClass))

typedef struct _NetDeviceWifiPrivate   NetDeviceWifiPrivate;
typedef struct _NetDeviceWifi          NetDeviceWifi;
typedef struct _NetDeviceWifiClass     NetDeviceWifiClass;

struct _NetDeviceWifi
{
         NetDevice                       parent;
         NetDeviceWifiPrivate           *priv;
};

struct _NetDeviceWifiClass
{
        NetDeviceClass                   parent_class;
};

GType            net_device_wifi_get_type          (void) G_GNUC_CONST;
GtkWidget       *net_device_wifi_get_header_widget (NetDeviceWifi *device_wifi);
GtkWidget       *net_device_wifi_get_title_widget  (NetDeviceWifi *device_wifi);
void             net_device_wifi_request_scan      (NetDeviceWifi *device_wifi);

G_END_DECLS

#endif /* __NET_DEVICE_WIFI_H */

