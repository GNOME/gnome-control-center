/*
 * Copyright (C) 2011 Red Hat, Inc.
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * Author: Bastien Nocera <hadess@hadess.net>
 *
 */

#ifndef __GSD_WACOM_DEVICE_MANAGER_H
#define __GSD_WACOM_DEVICE_MANAGER_H

#include <glib-object.h>

G_BEGIN_DECLS

#define GSD_TYPE_WACOM_DEVICE         (gsd_wacom_device_get_type ())
#define GSD_WACOM_DEVICE(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), GSD_TYPE_WACOM_DEVICE, GsdWacomDevice))
#define GSD_WACOM_DEVICE_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), GSD_TYPE_WACOM_DEVICE, GsdWacomDeviceClass))
#define GSD_IS_WACOM_DEVICE(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), GSD_TYPE_WACOM_DEVICE))
#define GSD_IS_WACOM_DEVICE_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), GSD_TYPE_WACOM_DEVICE))
#define GSD_WACOM_DEVICE_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), GSD_TYPE_WACOM_DEVICE, GsdWacomDeviceClass))

typedef struct GsdWacomDevicePrivate GsdWacomDevicePrivate;

typedef struct
{
        GObject                parent;
        GsdWacomDevicePrivate *priv;
} GsdWacomDevice;

typedef struct
{
        GObjectClass   parent_class;
} GsdWacomDeviceClass;

#define GSD_TYPE_WACOM_STYLUS         (gsd_wacom_stylus_get_type ())
#define GSD_WACOM_STYLUS(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), GSD_TYPE_WACOM_STYLUS, GsdWacomStylus))
#define GSD_WACOM_STYLUS_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), GSD_TYPE_WACOM_STYLUS, GsdWacomStylusClass))
#define GSD_IS_WACOM_STYLUS(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), GSD_TYPE_WACOM_STYLUS))
#define GSD_IS_WACOM_STYLUS_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), GSD_TYPE_WACOM_STYLUS))
#define GSD_WACOM_STYLUS_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), GSD_TYPE_WACOM_STYLUS, GsdWacomStylusClass))

typedef struct GsdWacomStylusPrivate GsdWacomStylusPrivate;

typedef struct
{
        GObject                parent;
        GsdWacomStylusPrivate *priv;
} GsdWacomStylus;

typedef struct
{
        GObjectClass   parent_class;
} GsdWacomStylusClass;

GType            gsd_wacom_stylus_get_type     (void);
GSettings      * gsd_wacom_stylus_get_settings (GsdWacomStylus *stylus);
const char     * gsd_wacom_stylus_get_name     (GsdWacomStylus *stylus);
const char     * gsd_wacom_stylus_get_icon_name(GsdWacomStylus *stylus);
GsdWacomDevice * gsd_wacom_stylus_get_device   (GsdWacomStylus *stylus);

/* Device types to apply a setting to */
typedef enum {
	WACOM_TYPE_INVALID =     0,
        WACOM_TYPE_STYLUS  =     (1 << 1),
        WACOM_TYPE_ERASER  =     (1 << 2),
        WACOM_TYPE_CURSOR  =     (1 << 3),
        WACOM_TYPE_PAD     =     (1 << 4),
        WACOM_TYPE_ALL     =     WACOM_TYPE_STYLUS | WACOM_TYPE_ERASER | WACOM_TYPE_CURSOR | WACOM_TYPE_PAD
} GsdWacomDeviceType;

GType gsd_wacom_device_get_type     (void);

GsdWacomDevice * gsd_wacom_device_new              (GdkDevice *device);
GList          * gsd_wacom_device_list_styli       (GsdWacomDevice *device);
const char     * gsd_wacom_device_get_name         (GsdWacomDevice *device);
const char     * gsd_wacom_device_get_icon_name    (GsdWacomDevice *device);
const char     * gsd_wacom_device_get_tool_name    (GsdWacomDevice *device);
gboolean         gsd_wacom_device_reversible       (GsdWacomDevice *device);
gboolean         gsd_wacom_device_is_screen_tablet (GsdWacomDevice *device);
GSettings      * gsd_wacom_device_get_settings     (GsdWacomDevice *device);

GsdWacomDeviceType gsd_wacom_device_get_device_type (GsdWacomDevice *device);
const char     * gsd_wacom_device_type_to_string   (GsdWacomDeviceType type);

/* Helper and debug functions */
GsdWacomDevice * gsd_wacom_device_create_fake (GsdWacomDeviceType  type,
					       const char         *name,
					       const char         *tool_name,
					       gboolean            reversible,
					       gboolean            is_screen_tablet,
					       const char         *icon_name,
					       guint               num_styli);

GList * gsd_wacom_device_create_fake_cintiq   (void);
GList * gsd_wacom_device_create_fake_bt       (void);

G_END_DECLS

#endif /* __GSD_WACOM_DEVICE_MANAGER_H */
