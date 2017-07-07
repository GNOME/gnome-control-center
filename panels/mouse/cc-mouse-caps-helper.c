/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright 2015  Red Hat, Inc,
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
 * Author: Felipe Borges <feborges@redhat.com>
 */

#include <gdk/gdkx.h>
#include <X11/Xatom.h>
#include <X11/extensions/XInput2.h>

#include "cc-mouse-caps-helper.h"

static gboolean
touchpad_check_capabilities_x11 (gboolean *have_two_finger_scrolling,
                                 gboolean *have_edge_scrolling,
                                 gboolean *have_tap_to_click)
{
        Display *display;
	GList *devicelist, *l;
	Atom realtype, prop_scroll_methods, prop_tapping_enabled;
	int realformat;
	unsigned long nitems, bytes_after;
	unsigned char *data;

        display = GDK_DISPLAY_XDISPLAY (gdk_display_get_default ());
	prop_scroll_methods = XInternAtom (display, "libinput Scroll Methods Available", False);
	prop_tapping_enabled = XInternAtom (display, "libinput Tapping Enabled", False);
	if (!prop_scroll_methods || !prop_tapping_enabled)
		return FALSE;

	*have_two_finger_scrolling = FALSE;
	*have_edge_scrolling = FALSE;
	*have_tap_to_click = FALSE;

        gdk_error_trap_push ();

	devicelist = gdk_seat_get_slaves (gdk_display_get_default_seat (gdk_display_get_default ()),
                                          GDK_SEAT_CAPABILITY_ALL_POINTING);
	for (l = devicelist; l != NULL; l = l->next) {
                GdkDevice *device = l->data;
                if (gdk_device_get_source (device) != GDK_SOURCE_TOUCHPAD)
			continue;

		/* xorg-x11-drv-libinput */
		if ((XIGetProperty (display, gdk_x11_device_get_id (device), prop_scroll_methods,
                                    0, 2, False, XA_INTEGER, &realtype, &realformat, &nitems,
                                    &bytes_after, &data) == Success) && (realtype != None)) {
			/* Property data is booleans for two-finger, edge, on-button scroll available. */

			if (data[0])
				*have_two_finger_scrolling = TRUE;

			if (data[1])
				*have_edge_scrolling = TRUE;

			XFree (data);
		}

		if ((XIGetProperty (display, gdk_x11_device_get_id (device), prop_tapping_enabled,
                                    0, 1, False, XA_INTEGER, &realtype, &realformat, &nitems,
                                    &bytes_after, &data) == Success) && (realtype != None)) {
			/* Property data is boolean for tapping enabled. */
			*have_tap_to_click = TRUE;

			XFree (data);
		}
	}
        g_list_free (devicelist);

        gdk_error_trap_pop_ignored ();

	return TRUE;
}

gboolean
cc_touchpad_check_capabilities (gboolean *have_two_finger_scrolling,
                                gboolean *have_edge_scrolling,
                                gboolean *have_tap_to_click)
{
	if (GDK_IS_X11_DISPLAY (gdk_display_get_default ()))
		return touchpad_check_capabilities_x11 (have_two_finger_scrolling,
                                                        have_edge_scrolling,
                                                        have_tap_to_click);
	/* else we unconditionally show all touchpad knobs */
        *have_two_finger_scrolling = TRUE;
        *have_edge_scrolling = TRUE;
        *have_tap_to_click = TRUE;
	return FALSE;
}

gboolean
cc_synaptics_check (void)
{
        Display *display;
        GList *devicelist, *l;
        Atom prop, realtype;
        int realformat;
        unsigned long nitems, bytes_after;
        unsigned char *data;
        gboolean have_synaptics = FALSE;

        if (!GDK_IS_X11_DISPLAY (gdk_display_get_default ()))
                return FALSE;

        display = GDK_DISPLAY_XDISPLAY (gdk_display_get_default ());
        prop = XInternAtom (display, "Synaptics Capabilities", False);

        gdk_error_trap_push ();

        devicelist = gdk_seat_get_slaves (gdk_display_get_default_seat (gdk_display_get_default ()),
                                          GDK_SEAT_CAPABILITY_ALL_POINTING);
        for (l = devicelist; l != NULL; l = l->next) {
                GdkDevice *device = l->data;

                if ((XIGetProperty (display, gdk_x11_device_get_id (device), prop,
                                    0, 2, False, XA_INTEGER, &realtype, &realformat, &nitems,
                                    &bytes_after, &data) == Success) && (realtype != None)) {
                        have_synaptics = TRUE;
                        XFree (data);
                }

                if (have_synaptics)
                        break;
        }
        g_list_free (devicelist);

        gdk_error_trap_pop_ignored ();

        return have_synaptics;
}
