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

#include "cc-mouse-caps-helper.h"

static gboolean
synaptics_check_capabilities_x11 (gboolean *have_two_finger_scrolling,
                                  gboolean *have_tap_to_click)
{
	int numdevices, i;
	XDeviceInfo *devicelist;
	Atom realtype, prop_capabilities, prop_scroll_methods, prop_tapping_enabled;
	int realformat;
	unsigned long nitems, bytes_after;
	unsigned char *data;

	prop_capabilities = XInternAtom (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()), "Synaptics Capabilities", False);
	prop_scroll_methods = XInternAtom (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()), "libinput Scroll Methods Available", False);
	prop_tapping_enabled = XInternAtom (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()), "libinput Tapping Enabled", False);
	if (!prop_capabilities || !prop_scroll_methods || !prop_tapping_enabled)
		return FALSE;

	*have_two_finger_scrolling = FALSE;
	*have_tap_to_click = FALSE;

	devicelist = XListInputDevices (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()), &numdevices);
	for (i = 0; i < numdevices; i++) {
		if (devicelist[i].use != IsXExtensionPointer)
			continue;

		gdk_error_trap_push ();
		XDevice *device = XOpenDevice (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()),
					       devicelist[i].id);
		if (gdk_error_trap_pop ())
			continue;

		gdk_error_trap_push ();

		/* xorg-x11-drv-synaptics */
		if ((XGetDeviceProperty (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()), device, prop_capabilities,
					 0, 2, False, XA_INTEGER, &realtype, &realformat, &nitems,
					 &bytes_after, &data) == Success) && (realtype != None)) {
			/* Property data is booleans for has_left, has_middle, has_right, has_double, has_triple.
			 * Newer drivers (X.org/kerrnel) will also include has_pressure and has_width. */

			/* Set tap_to_click_toggle sensitive only if the device has hardware buttons */
			if (data[0])
				*have_tap_to_click = TRUE;

			/* Set two_finger_scroll_toggle sensitive if the hardware supports double touch */
			if (data[3])
				*have_two_finger_scrolling = TRUE;

			XFree (data);
		}

		/* xorg-x11-drv-libinput */
		if ((XGetDeviceProperty (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()), device, prop_scroll_methods,
					 0, 2, False, XA_INTEGER, &realtype, &realformat, &nitems,
					 &bytes_after, &data) == Success) && (realtype != None)) {
			/* Property data is booleans for two-finger, edge, on-button scroll available. */

			if (data[0] && data[1])
				*have_two_finger_scrolling = TRUE;

			XFree (data);
		}

		if ((XGetDeviceProperty (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()), device, prop_tapping_enabled,
					0, 1, False, XA_INTEGER, &realtype, &realformat, &nitems,
					&bytes_after, &data) == Success) && (realtype != None)) {
			/* Property data is boolean for tapping enabled. */
			*have_tap_to_click = TRUE;

			XFree (data);
		}

		gdk_error_trap_pop_ignored ();

		XCloseDevice (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()), device);
	}
	XFreeDeviceList (devicelist);

	return TRUE;
}

gboolean
synaptics_check_capabilities (gboolean *have_two_finger_scrolling,
                              gboolean *have_tap_to_click)
{
	if (GDK_IS_X11_DISPLAY (gdk_display_get_default ()))
		return synaptics_check_capabilities_x11 (have_two_finger_scrolling,
							 have_tap_to_click);
	/* else we unconditionally show all touchpad knobs */
	return FALSE;
}
