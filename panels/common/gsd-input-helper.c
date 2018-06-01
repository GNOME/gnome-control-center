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
 *
 */

#include "config.h"

#include <string.h>

#include <gdk/gdk.h>
#include <gdk/gdkx.h>

#include <sys/types.h>
#include <X11/Xatom.h>
#include <X11/extensions/XInput2.h>

#include "gsd-input-helper.h"
#include "gsd-device-manager.h"

#define ABS_MT_X "Abs MT Position X"
#define ABS_MT_Y "Abs MT Position Y"
#define ABS_X "Abs X"
#define ABS_Y "Abs Y"

typedef gboolean (* InfoIdentifyFunc) (XDeviceInfo *device_info);
typedef gboolean (* DeviceIdentifyFunc) (XDevice *xdevice);

gboolean
device_set_property (XDevice        *xdevice,
                     const char     *device_name,
                     PropertyHelper *property)
{
        int rc, i;
        Atom prop;
        Atom realtype;
        int realformat;
        unsigned long nitems, bytes_after;
        unsigned char *data;

        prop = XInternAtom (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()),
                            property->name, False);
        if (!prop)
                return FALSE;

        gdk_error_trap_push ();

        rc = XGetDeviceProperty (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()),
                                 xdevice, prop, 0, property->nitems, False,
                                 AnyPropertyType, &realtype, &realformat, &nitems,
                                 &bytes_after, &data);

        if (rc != Success ||
            realtype != property->type ||
            realformat != property->format ||
            nitems < property->nitems) {
                gdk_error_trap_pop_ignored ();
                g_warning ("Error reading property \"%s\" for \"%s\"", property->name, device_name);
                return FALSE;
        }

        for (i = 0; i < nitems; i++) {
                switch (property->format) {
                        case 8:
                                data[i] = property->data.c[i];
                                break;
                        case 32:
                                ((long*)data)[i] = property->data.i[i];
                                break;
                }
        }

        XChangeDeviceProperty (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()),
                               xdevice, prop, realtype, realformat,
                               PropModeReplace, data, nitems);

        XFree (data);

        if (gdk_error_trap_pop ()) {
                g_warning ("Error in setting \"%s\" for \"%s\"", property->name, device_name);
                return FALSE;
        }

        return TRUE;
}

static gboolean
supports_xinput_devices_with_opcode (int *opcode)
{
        gint op_code, event, error;
        gboolean retval;

        retval = XQueryExtension (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()),
				  "XInputExtension",
				  &op_code,
				  &event,
				  &error);
	if (opcode)
		*opcode = op_code;

	return retval;
}

gboolean
supports_xtest (void)
{
        gint op_code, event, error;
        gboolean retval;

        retval = XQueryExtension (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()),
				  "XTEST",
				  &op_code,
				  &event,
				  &error);

	return retval;
}

gboolean
supports_xinput2_devices (int *opcode)
{
        int major, minor;

        if (supports_xinput_devices_with_opcode (opcode) == FALSE)
                return FALSE;

        gdk_error_trap_push ();

        major = 2;
        minor = 3;

        if (XIQueryVersion (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()), &major, &minor) != Success) {
                gdk_error_trap_pop_ignored ();
                    return FALSE;
        }
        gdk_error_trap_pop_ignored ();

        if ((major * 1000 + minor) < (2000))
                return FALSE;

        return TRUE;
}

static gboolean
device_type_is_present (GsdDeviceType type)
{
        g_autoptr(GList) l = gsd_device_manager_list_devices (gsd_device_manager_get (),
                                                              type);
        return l != NULL;
}

gboolean
touchscreen_is_present (void)
{
        return device_type_is_present (GSD_DEVICE_TYPE_TOUCHSCREEN);
}

gboolean
touchpad_is_present (void)
{
        return device_type_is_present (GSD_DEVICE_TYPE_TOUCHPAD);
}

gboolean
mouse_is_present (void)
{
        return device_type_is_present (GSD_DEVICE_TYPE_MOUSE);
}

char *
xdevice_get_device_node (int deviceid)
{
        Atom           prop;
        Atom           act_type;
        int            act_format;
        unsigned long  nitems, bytes_after;
        unsigned char *data;
        char          *ret;

        gdk_display_sync (gdk_display_get_default ());

        prop = XInternAtom (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()), "Device Node", False);
        if (!prop)
                return NULL;

        gdk_error_trap_push ();

        if (!XIGetProperty (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()),
                            deviceid, prop, 0, 1000, False,
                            AnyPropertyType, &act_type, &act_format,
                            &nitems, &bytes_after, &data) == Success) {
                gdk_error_trap_pop_ignored ();
                return NULL;
        }
        if (gdk_error_trap_pop ())
                goto out;

        if (nitems == 0)
                goto out;

        if (act_type != XA_STRING)
                goto out;

        /* Unknown string format */
        if (act_format != 8)
                goto out;

        ret = g_strdup ((char *) data);

        XFree (data);
        return ret;

out:
        XFree (data);
        return NULL;
}

#define TOOL_ID_FORMAT_SIZE 32
static int
get_id_for_index (guchar *data,
		  guint   idx)
{
	guchar *ptr;
	int id;

	ptr = data;
	ptr += TOOL_ID_FORMAT_SIZE / 8 * idx;

	id = *((int32_t*)ptr);
	id = id & 0xfffff;

	return id;
}


#define STYLUS_DEVICE_ID        0x02
#define ERASER_DEVICE_ID        0x0A

int
xdevice_get_last_tool_id (int  deviceid)
{
        Atom           prop;
        Atom           act_type;
        int            act_format;
        unsigned long  nitems, bytes_after;
        unsigned char *data;
        int            id;

        id = -1;

        gdk_display_sync (gdk_display_get_default ());

        prop = XInternAtom (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()), WACOM_SERIAL_IDS_PROP, False);
        if (!prop)
                return -1;

        data = NULL;

        gdk_error_trap_push ();

        if (XIGetProperty (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()),
                            deviceid, prop, 0, 1000, False,
                            AnyPropertyType, &act_type, &act_format,
                            &nitems, &bytes_after, &data) != Success) {
                gdk_error_trap_pop_ignored ();
                goto out;
        }

        if (gdk_error_trap_pop ())
                goto out;

	if (nitems != 4 && nitems != 5)
		goto out;

	if (act_type != XA_INTEGER)
		goto out;

	if (act_format != TOOL_ID_FORMAT_SIZE)
		goto out;

	/* item 0 = tablet ID
	 * item 1 = old device serial number (== last tool in proximity)
	 * item 2 = old hardware serial number (including tool ID)
	 * item 3 = current serial number (0 if no tool in proximity)
	 * item 4 = current tool ID (since Feb 2012)
	 *
	 * Get the current tool ID first, if available, then the old one */
	id = 0x0;
	if (nitems == 5)
		id = get_id_for_index (data, 4);
	if (id == 0x0)
		id = get_id_for_index (data, 2);

	/* That means that no tool was set down yet */
	if (id == STYLUS_DEVICE_ID ||
	    id == ERASER_DEVICE_ID)
		id = 0x0;

out:
        if (data != NULL)
                XFree (data);
        return id;
}

gboolean
set_device_enabled (int device_id,
                    gboolean enabled)
{
        Atom prop;
        guchar value;

        prop = XInternAtom (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()), "Device Enabled", False);
        if (!prop)
                return FALSE;

        gdk_error_trap_push ();

        value = enabled ? 1 : 0;
        XIChangeProperty (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()),
                          device_id, prop, XA_INTEGER, 8, PropModeReplace, &value, 1);

        if (gdk_error_trap_pop ())
                return FALSE;

        return TRUE;
}

const char *
xdevice_get_wacom_tool_type (int deviceid)
{
        unsigned long nitems, bytes_after;
        unsigned char *data = NULL;
        Atom prop, realtype, tool;
        GdkDisplay *display;
        int realformat, rc;
        const gchar *ret = NULL;

        gdk_error_trap_push ();

        display = gdk_display_get_default ();
        prop = gdk_x11_get_xatom_by_name ("Wacom Tool Type");

        rc = XIGetProperty (GDK_DISPLAY_XDISPLAY (display),
                            deviceid, prop, 0, 1, False,
                            XA_ATOM, &realtype, &realformat, &nitems,
                            &bytes_after, &data);

        gdk_error_trap_pop_ignored ();

        if (rc != Success || nitems == 0)
                return NULL;

        if (realtype == XA_ATOM) {
                tool = *((Atom*) data);
                ret = gdk_x11_get_xatom_name (tool);
        }

        XFree (data);

        return ret;
}

void
xdevice_close (XDevice *xdevice)
{
    gdk_error_trap_push ();
    XCloseDevice (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()), xdevice);
    gdk_error_trap_pop_ignored();
}

gboolean
xdevice_get_dimensions (int    deviceid,
                        guint *width,
                        guint *height)
{
        GdkDisplay *display = gdk_display_get_default ();
        XIDeviceInfo *info;
        guint *value, w, h;
        int i, n_info;

        gdk_error_trap_push ();

        info = XIQueryDevice (GDK_DISPLAY_XDISPLAY (display), deviceid, &n_info);
        *width = *height = w = h = 0;

        if (gdk_error_trap_pop ())
                return FALSE;

        if (!info)
                return FALSE;

        for (i = 0; i < info->num_classes; i++) {
                XIValuatorClassInfo *valuator_info;

                if (info->classes[i]->type != XIValuatorClass)
                        continue;

                valuator_info = (XIValuatorClassInfo *) info->classes[i];

                if (valuator_info->label == gdk_x11_get_xatom_by_name_for_display (display, ABS_X) ||
                    valuator_info->label == gdk_x11_get_xatom_by_name_for_display (display, ABS_MT_X))
                        value = &w;
                else if (valuator_info->label == gdk_x11_get_xatom_by_name_for_display (display, ABS_Y) ||
                         valuator_info->label == gdk_x11_get_xatom_by_name_for_display (display, ABS_MT_Y))
                        value = &h;
                else
                        continue;

                *value = (valuator_info->max -  valuator_info->min) * 1000 / valuator_info->resolution;
        }

        *width = w;
        *height = h;

        XIFreeDeviceInfo (info);

        return (w != 0 && h != 0);
}
