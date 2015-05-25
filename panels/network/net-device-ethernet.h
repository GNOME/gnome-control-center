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

#ifndef __NET_DEVICE_ETHERNET_H
#define __NET_DEVICE_ETHERNET_H

#include <glib-object.h>

#include "net-device-simple.h"

G_BEGIN_DECLS

#define NET_TYPE_DEVICE_ETHERNET          (net_device_ethernet_get_type ())
#define NET_DEVICE_ETHERNET(o)            (G_TYPE_CHECK_INSTANCE_CAST ((o), NET_TYPE_DEVICE_ETHERNET, NetDeviceEthernet))
#define NET_DEVICE_ETHERNET_CLASS(k)      (G_TYPE_CHECK_CLASS_CAST((k), NET_TYPE_DEVICE_ETHERNET, NetDeviceEthernetClass))
#define NET_IS_DEVICE_ETHERNET(o)         (G_TYPE_CHECK_INSTANCE_TYPE ((o), NET_TYPE_DEVICE_ETHERNET))
#define NET_IS_DEVICE_ETHERNET_CLASS(k)   (G_TYPE_CHECK_CLASS_TYPE ((k), NET_TYPE_DEVICE_ETHERNET))
#define NET_DEVICE_ETHERNET_GET_CLASS(o)  (G_TYPE_INSTANCE_GET_CLASS ((o), NET_TYPE_DEVICE_ETHERNET, NetDeviceEthernetClass))

typedef struct _NetDeviceEthernetPrivate   NetDeviceEthernetPrivate;
typedef struct _NetDeviceEthernet          NetDeviceEthernet;
typedef struct _NetDeviceEthernetClass     NetDeviceEthernetClass;

struct _NetDeviceEthernet
{
        NetDeviceSimple parent;

        GtkBuilder *builder;

        GtkWidget *list;
        GtkWidget *scrolled_window;
        GtkWidget *details;
        GtkWidget *details_button;
        GtkWidget *add_profile_button;
        gboolean   updating_device;

        GHashTable *connections;
};

struct _NetDeviceEthernetClass
{
        NetDeviceSimpleClass parent_class;
};

GType net_device_ethernet_get_type (void);

G_END_DECLS

#endif /* __NET_DEVICE_ETHERNET_H */

