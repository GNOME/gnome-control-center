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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Bastien Nocera <hadess@hadess.net>
 *
 */

#ifndef __GSD_WACOM_DEVICE_MANAGER_H
#define __GSD_WACOM_DEVICE_MANAGER_H

#include <glib-object.h>
#include "gsd-enums.h"

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

typedef enum {
	WACOM_STYLUS_TYPE_UNKNOWN,
	WACOM_STYLUS_TYPE_GENERAL,
	WACOM_STYLUS_TYPE_INKING,
	WACOM_STYLUS_TYPE_AIRBRUSH,
	WACOM_STYLUS_TYPE_CLASSIC,
	WACOM_STYLUS_TYPE_MARKER,
	WACOM_STYLUS_TYPE_STROKE,
	WACOM_STYLUS_TYPE_PUCK
} GsdWacomStylusType;

GType            gsd_wacom_stylus_get_type       (void);
GSettings      * gsd_wacom_stylus_get_settings   (GsdWacomStylus *stylus);
const char     * gsd_wacom_stylus_get_name       (GsdWacomStylus *stylus);
const char     * gsd_wacom_stylus_get_icon_name  (GsdWacomStylus *stylus);
GsdWacomDevice * gsd_wacom_stylus_get_device     (GsdWacomStylus *stylus);
gboolean         gsd_wacom_stylus_get_has_eraser (GsdWacomStylus *stylus);
guint            gsd_wacom_stylus_get_num_buttons(GsdWacomStylus *stylus);
int              gsd_wacom_stylus_get_id         (GsdWacomStylus *stylus);
GsdWacomStylusType gsd_wacom_stylus_get_stylus_type (GsdWacomStylus *stylus);

/* Tablet Buttons */
typedef enum {
	WACOM_TABLET_BUTTON_TYPE_NORMAL,
	WACOM_TABLET_BUTTON_TYPE_STRIP,
	WACOM_TABLET_BUTTON_TYPE_RING,
	WACOM_TABLET_BUTTON_TYPE_HARDCODED
} GsdWacomTabletButtonType;

/*
 * Positions of the buttons on the tablet in default right-handed mode
 * (ie with no rotation applied).
 */
typedef enum {
	WACOM_TABLET_BUTTON_POS_UNDEF = 0,
	WACOM_TABLET_BUTTON_POS_LEFT,
	WACOM_TABLET_BUTTON_POS_RIGHT,
	WACOM_TABLET_BUTTON_POS_TOP,
	WACOM_TABLET_BUTTON_POS_BOTTOM
} GsdWacomTabletButtonPos;

#define MAX_GROUP_ID 4

#define GSD_WACOM_NO_LED -1

typedef struct
{
	char                     *name;
	char                     *id;
	GSettings                *settings;
	GsdWacomTabletButtonType  type;
	GsdWacomTabletButtonPos   pos;
	int                       group_id, idx;
	int                       status_led;
	int                       has_oled;
} GsdWacomTabletButton;

void                  gsd_wacom_tablet_button_free (GsdWacomTabletButton *button);
GsdWacomTabletButton *gsd_wacom_tablet_button_copy (GsdWacomTabletButton *button);

/* Device types to apply a setting to */
typedef enum {
	WACOM_TYPE_INVALID =     0,
        WACOM_TYPE_STYLUS  =     (1 << 1),
        WACOM_TYPE_ERASER  =     (1 << 2),
        WACOM_TYPE_CURSOR  =     (1 << 3),
        WACOM_TYPE_PAD     =     (1 << 4),
        WACOM_TYPE_TOUCH   =     (1 << 5),
        WACOM_TYPE_ALL     =     WACOM_TYPE_STYLUS | WACOM_TYPE_ERASER | WACOM_TYPE_CURSOR | WACOM_TYPE_PAD | WACOM_TYPE_TOUCH
} GsdWacomDeviceType;

/* We use -1 for entire screen when setting/getting monitor value */
#define GSD_WACOM_SET_ALL_MONITORS -1

GType gsd_wacom_device_get_type     (void);

void     gsd_wacom_device_set_display         (GsdWacomDevice    *device,
                                               int                monitor);
gint     gsd_wacom_device_get_display_monitor (GsdWacomDevice *device);
GsdWacomRotation gsd_wacom_device_get_display_rotation (GsdWacomDevice *device);

GsdWacomDevice * gsd_wacom_device_new              (GdkDevice *device);
GList          * gsd_wacom_device_list_styli       (GsdWacomDevice *device);
const char     * gsd_wacom_device_get_name         (GsdWacomDevice *device);
const char     * gsd_wacom_device_get_layout_path  (GsdWacomDevice *device);
const char     * gsd_wacom_device_get_path         (GsdWacomDevice *device);
const char     * gsd_wacom_device_get_icon_name    (GsdWacomDevice *device);
const char     * gsd_wacom_device_get_tool_name    (GsdWacomDevice *device);
gboolean         gsd_wacom_device_reversible       (GsdWacomDevice *device);
gboolean         gsd_wacom_device_is_screen_tablet (GsdWacomDevice *device);
gboolean         gsd_wacom_device_is_isd           (GsdWacomDevice *device);
gboolean         gsd_wacom_device_is_fallback      (GsdWacomDevice *device);
gint             gsd_wacom_device_get_num_strips   (GsdWacomDevice *device);
gint             gsd_wacom_device_get_num_rings    (GsdWacomDevice *device);
GSettings      * gsd_wacom_device_get_settings     (GsdWacomDevice *device);
void             gsd_wacom_device_set_current_stylus (GsdWacomDevice *device,
						      int             stylus_id);
GsdWacomStylus * gsd_wacom_device_get_stylus_for_type (GsdWacomDevice     *device,
						       GsdWacomStylusType  type);

GsdWacomDeviceType gsd_wacom_device_get_device_type (GsdWacomDevice *device);
gint           * gsd_wacom_device_get_area          (GsdWacomDevice *device);
gint           * gsd_wacom_device_get_default_area  (GsdWacomDevice *device);
const char     * gsd_wacom_device_type_to_string    (GsdWacomDeviceType type);
GList          * gsd_wacom_device_get_buttons       (GsdWacomDevice *device);
GsdWacomTabletButton *gsd_wacom_device_get_button   (GsdWacomDevice   *device,
						     int               button,
						     GtkDirectionType *dir);
int gsd_wacom_device_get_num_modes                  (GsdWacomDevice   *device,
						     int               group_id);
int gsd_wacom_device_get_current_mode               (GsdWacomDevice   *device,
						     int               group_id);
int gsd_wacom_device_set_next_mode                  (GsdWacomDevice       *device,
						     GsdWacomTabletButton *button);
GdkDevice      * gsd_wacom_device_get_gdk_device    (GsdWacomDevice   *device);

GsdWacomRotation gsd_wacom_device_rotation_name_to_type (const char *rotation);
const char     * gsd_wacom_device_rotation_type_to_name (GsdWacomRotation type);


/* Helper and debug functions */
GsdWacomDevice * gsd_wacom_device_create_fake (GsdWacomDeviceType  type,
					       const char         *name,
					       const char         *tool_name);

GList * gsd_wacom_device_create_fake_cintiq   (void);
GList * gsd_wacom_device_create_fake_bt       (void);
GList * gsd_wacom_device_create_fake_x201     (void);
GList * gsd_wacom_device_create_fake_intuos4  (void);
GList * gsd_wacom_device_create_fake_h610pro  (void);

G_END_DECLS

#endif /* __GSD_WACOM_DEVICE_MANAGER_H */
