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

#include <glib-object.h>

#include "usb-device.h"

G_BEGIN_DECLS

/* UsbStore - database for devices */
#define USB_TYPE_STORE usb_store_get_type ()
G_DECLARE_FINAL_TYPE (UsbStore, usb_store, USB, STORE, GObject);

UsbStore *  usb_store_new (const char *path);

gboolean    usb_store_put_device (UsbStore  *store,
                                  UsbDevice *device,
                                  GError   **error);

UsbDevice * usb_store_get_device (UsbStore    *store,
                                  const gchar *vendor,
                                  const gchar *product_id,
                                  GError     **error);

gboolean    usb_store_del_device (UsbStore  *store,
                                  UsbDevice *device,
                                  GError   **error);

G_END_DECLS

