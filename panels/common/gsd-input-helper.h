/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2010 Bastien Nocera <hadess@hadess.net>
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
 */

#pragma once

G_BEGIN_DECLS

#include <glib.h>

#include <X11/extensions/XInput.h>
#include <X11/extensions/XIproto.h>

#define WACOM_SERIAL_IDS_PROP "Wacom Serial IDs"

/* Generic property setting code. Fill up the struct property with the property
 * data and pass it into device_set_property together with the device to be
 * changed.  Note: doesn't cater for non-zero offsets yet, but we don't have
 * any settings that require that.
 */
typedef struct {
        const char *name;       /* property name */
        gint nitems;            /* number of items in data */
        gint format;            /* CARD8 or CARD32 sized-items */
        gint type;              /* Atom representing data type */
        union {
                const gchar *c; /* 8 bit data */
                const gint *i;  /* 32 bit data */
        } data;
} PropertyHelper;

gboolean  supports_xinput2_devices (int *opcode);
gboolean  supports_xtest           (void);

gboolean set_device_enabled       (int device_id,
                                   gboolean enabled);

gboolean  touchpad_is_present     (void);
gboolean  touchscreen_is_present  (void);
gboolean  mouse_is_present        (void);

gboolean  device_set_property     (XDevice                *xdevice,
                                   const char             *device_name,
                                   PropertyHelper         *property);

char *    xdevice_get_device_node  (int                     deviceid);
int       xdevice_get_last_tool_id (int                     deviceid);
gboolean  xdevice_get_dimensions   (int                     deviceid,
                                    guint                  *width,
                                    guint                  *height);
void      xdevice_close      (XDevice                *xdevice);

const char * xdevice_get_wacom_tool_type (int               deviceid);

G_END_DECLS
