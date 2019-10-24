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

#pragma once

#include <gtk/gtk.h>
#include <NetworkManager.h>

G_BEGIN_DECLS

G_DECLARE_FINAL_TYPE (NetDeviceEthernet, net_device_ethernet, NET, DEVICE_ETHERNET, GtkBox)

NetDeviceEthernet *net_device_ethernet_new        (NMClient          *client,
                                                   NMDevice          *device);

NMDevice          *net_device_ethernet_get_device (NetDeviceEthernet *device);

void               net_device_ethernet_set_title  (NetDeviceEthernet *device,
                                                   const gchar       *title);

G_END_DECLS
