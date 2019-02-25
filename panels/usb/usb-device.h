/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * Copyright (C) 2019 GNOME
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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Ludovico de Nittis <denittis@gnome.org>
 *
 */

#pragma once

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define USB_TYPE_DEVICE usb_device_get_type ()
G_DECLARE_FINAL_TYPE (UsbDevice, usb_device, USB, DEVICE, GObject);

UsbDevice *   usb_device_new (gboolean        authorized,
                              const char     *name,
                              const char     *product_id,
                              const char     *vendor);

gboolean      usb_device_get_authorization (UsbDevice *device);

gint          usb_device_set_authorization (UsbDevice *device,
                                            gboolean authorization);

const char *  usb_device_get_name (UsbDevice *dev);

const char *  usb_device_get_product_id (UsbDevice *dev);

const char *  usb_device_get_vendor (UsbDevice *dev);

G_END_DECLS

