/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*-
 *
 * Copyright (C) 2012 Richard Hughes <richard@hughsie.com>
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

#ifndef CC_COLOR_DEVICE_H
#define CC_COLOR_DEVICE_H

#include <gtk/gtk.h>
#include <colord.h>

#define CC_TYPE_COLOR_DEVICE            (cc_color_device_get_type())
#define CC_COLOR_DEVICE(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), CC_TYPE_COLOR_DEVICE, CcColorDevice))
#define CC_COLOR_DEVICE_CLASS(cls)      (G_TYPE_CHECK_CLASS_CAST((cls), CC_TYPE_COLOR_DEVICE, CcColorDeviceClass))
#define CC_IS_COLOR_DEVICE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), CC_TYPE_COLOR_DEVICE))
#define CC_IS_COLOR_DEVICE_CLASS(cls)   (G_TYPE_CHECK_CLASS_TYPE((cls), CC_TYPE_COLOR_DEVICE))
#define CC_COLOR_DEVICE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), CC_TYPE_COLOR_DEVICE, CcColorDeviceClass))

G_BEGIN_DECLS

typedef struct _CcColorDevice             CcColorDevice;
typedef struct _CcColorDeviceClass        CcColorDeviceClass;
typedef struct _CcColorDevicePrivate      CcColorDevicePrivate;

struct _CcColorDevice
{
        GtkListBoxRow            parent;

        /*< private >*/
        CcColorDevicePrivate    *priv;
};

struct _CcColorDeviceClass
{
        GtkListBoxRowClass       parent_class;
        void            (*expanded_changed) (CcColorDevice  *color_device,
                                             gboolean        expanded);
};

GType        cc_color_device_get_type      (void);
GtkWidget   *cc_color_device_new           (CdDevice       *device);
CdDevice    *cc_color_device_get_device    (CcColorDevice  *color_device);
const gchar *cc_color_device_get_sortable  (CcColorDevice  *color_device);
void         cc_color_device_set_expanded  (CcColorDevice  *color_device,
                                            gboolean        expanded);

G_END_DECLS

#endif /* CC_COLOR_DEVICE_H */

