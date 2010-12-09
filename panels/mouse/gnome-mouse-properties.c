/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2001 Red Hat, Inc.
 * Copyright (C) 2001 Ximian, Inc.
 *
 * Written by: Jonathon Blandford <jrb@redhat.com>,
 *             Bradford Hovinen <hovinen@ximian.com>,
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

#include <config.h>

#include <glib/gi18n.h>
#include <string.h>
#include <gdk/gdkx.h>
#include <gnome-settings-daemon/gsd-enums.h>
#include <math.h>

#include "gnome-mouse-properties.h"
#include "gsd-input-helper.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>

#include <X11/Xatom.h>
#include <X11/extensions/XInput.h>

#define WID(x) (GtkWidget*) gtk_builder_get_object (dialog, x)

enum
{
	DOUBLE_CLICK_TEST_OFF,
	DOUBLE_CLICK_TEST_MAYBE,
	DOUBLE_CLICK_TEST_ON
};

/* State in testing the double-click speed. Global for a great deal of
 * convenience
 */
static gint double_click_state = DOUBLE_CLICK_TEST_OFF;
static GSettings *mouse_settings = NULL;
static GSettings *touchpad_settings = NULL;

/* Double Click handling */

struct test_data_t
{
	gint *timeout_id;
	GtkWidget *image;
};

/* Timeout for the double click test */

static gboolean
test_maybe_timeout (struct test_data_t *data)
{
	double_click_state = DOUBLE_CLICK_TEST_OFF;

	gtk_image_set_from_icon_name (GTK_IMAGE (data->image), "face-plain", GTK_ICON_SIZE_DIALOG);

	*data->timeout_id = 0;

	return FALSE;
}

/* Callback issued when the user clicks the double click testing area. */

static gboolean
event_box_button_press_event (GtkWidget   *widget,
			      GdkEventButton *event,
			      GtkBuilder   *dialog)
{
	gint                       double_click_time;
	static struct test_data_t  data;
	static gint                test_on_timeout_id     = 0;
	static gint                test_maybe_timeout_id  = 0;
	static guint32             double_click_timestamp = 0;
	GtkWidget                 *image;

	if (event->type != GDK_BUTTON_PRESS)
		return FALSE;

	image = g_object_get_data (G_OBJECT (widget), "image");

	double_click_time = g_settings_get_int (mouse_settings, "double-click");

	if (test_maybe_timeout_id != 0)
		g_source_remove  (test_maybe_timeout_id);
	if (test_on_timeout_id != 0)
		g_source_remove (test_on_timeout_id);

	switch (double_click_state) {
	case DOUBLE_CLICK_TEST_OFF:
		double_click_state = DOUBLE_CLICK_TEST_MAYBE;
		data.image = image;
		data.timeout_id = &test_maybe_timeout_id;
		test_maybe_timeout_id = g_timeout_add (double_click_time, (GSourceFunc) test_maybe_timeout, &data);
		break;
	case DOUBLE_CLICK_TEST_MAYBE:
		if (event->time - double_click_timestamp < double_click_time) {
			double_click_state = DOUBLE_CLICK_TEST_ON;
			data.image = image;
			data.timeout_id = &test_on_timeout_id;
			test_on_timeout_id = g_timeout_add (2500, (GSourceFunc) test_maybe_timeout, &data);
		}
		break;
	case DOUBLE_CLICK_TEST_ON:
		double_click_state = DOUBLE_CLICK_TEST_OFF;
		break;
	}

	double_click_timestamp = event->time;

	switch (double_click_state) {
	case DOUBLE_CLICK_TEST_ON:
		gtk_image_set_from_icon_name (GTK_IMAGE (image), "face-laugh", GTK_ICON_SIZE_DIALOG);
		break;
	case DOUBLE_CLICK_TEST_MAYBE:
		gtk_image_set_from_icon_name (GTK_IMAGE (image), "face-smile", GTK_ICON_SIZE_DIALOG);
		break;
	case DOUBLE_CLICK_TEST_OFF:
		gtk_image_set_from_icon_name (GTK_IMAGE (image), "face-plain", GTK_ICON_SIZE_DIALOG);
		break;
	}

	return TRUE;
}

static void
orientation_radio_button_release_event (GtkWidget   *widget,
				        GdkEventButton *event)
{
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), TRUE);
}

static void
scrollmethod_changed_event (GtkToggleButton *button, GtkBuilder *dialog)
{
	GsdTouchpadScrollMethod method;
	GtkToggleButton *disabled = GTK_TOGGLE_BUTTON (WID ("scroll_disabled_radio"));

	gtk_widget_set_sensitive (WID ("horiz_scroll_toggle"),
				  !gtk_toggle_button_get_active (disabled));

	if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (WID ("scroll_edge_radio"))))
		method = GSD_TOUCHPAD_SCROLL_METHOD_EDGE_SCROLLING;
	else if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (WID ("scroll_twofinger_radio"))))
		method = GSD_TOUCHPAD_SCROLL_METHOD_TWO_FINGER_SCROLLING;
	else
		method = GSD_TOUCHPAD_SCROLL_METHOD_DISABLED;

	g_settings_set_enum (touchpad_settings, "scroll-method", method);
}

static void
synaptics_check_capabilities (GtkBuilder *dialog)
{
	int numdevices, i;
	XDeviceInfo *devicelist;
	Atom realtype, prop;
	int realformat;
	unsigned long nitems, bytes_after;
	unsigned char *data;

	prop = XInternAtom (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()), "Synaptics Capabilities", True);
	if (!prop)
		return;

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
		if ((XGetDeviceProperty (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()), device, prop, 0, 2, False,
					 XA_INTEGER, &realtype, &realformat, &nitems,
					 &bytes_after, &data) == Success) && (realtype != None)) {
			/* Property data is booleans for has_left, has_middle, has_right, has_double, has_triple.
			 * Newer drivers (X.org/kerrnel) will also include has_pressure and has_width. */
			if (!data[0]) {
				gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (WID ("tap_to_click_toggle")), TRUE);
				gtk_widget_set_sensitive (WID ("tap_to_click_toggle"), FALSE);
			}

			/* Disable two finger scrolling unless the hardware supports
			 * double touch or can emulate it based on finger width. */
			if (!(data[3] ||(nitems >= 6 && data[5])))
				gtk_widget_set_sensitive (WID ("scroll_twofinger_radio"), FALSE);

			XFree (data);
		}
		gdk_error_trap_pop_ignored ();

		XCloseDevice (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()), device);
	}
	XFreeDeviceList (devicelist);
}

/* Set up the property editors in the dialog. */
static void
setup_dialog (GtkBuilder *dialog)
{
	GtkRadioButton    *radio;

	/* Orientation radio buttons */
	radio = GTK_RADIO_BUTTON (WID ("left_handed_radio"));
	g_settings_bind (mouse_settings, "left-handed", radio, "active", G_SETTINGS_BIND_DEFAULT);

	/* explicitly connect to button-release so that you can change orientation with either button */
	g_signal_connect (WID ("right_handed_radio"), "button_release_event",
		G_CALLBACK (orientation_radio_button_release_event), NULL);
	g_signal_connect (WID ("left_handed_radio"), "button_release_event",
		G_CALLBACK (orientation_radio_button_release_event), NULL);

	/* Locate pointer toggle */
	g_settings_bind (mouse_settings, "locate-pointer",
			 WID ("locate_pointer_toggle"), "active",
			 G_SETTINGS_BIND_DEFAULT);

	/* Double-click time */
	g_settings_bind (mouse_settings, "double-click",
			 gtk_range_get_adjustment (GTK_RANGE (WID ("delay_scale"))), "value",
			 G_SETTINGS_BIND_DEFAULT);
	gtk_image_set_from_icon_name (GTK_IMAGE (WID ("double_click_image")), "face-plain", GTK_ICON_SIZE_DIALOG);
	g_object_set_data (G_OBJECT (WID ("double_click_eventbox")), "image", WID ("double_click_image"));
	g_signal_connect (WID ("double_click_eventbox"), "button_press_event",
			  G_CALLBACK (event_box_button_press_event), dialog);

	/* speed */
	g_settings_bind (mouse_settings, "motion-acceleration",
			 gtk_range_get_adjustment (GTK_RANGE (WID ("accel_scale"))), "value",
			 G_SETTINGS_BIND_DEFAULT);
	g_settings_bind (mouse_settings, "motion-threshold",
			 gtk_range_get_adjustment (GTK_RANGE (WID ("sensitivity_scale"))), "value",
			 G_SETTINGS_BIND_DEFAULT);

	/* DnD threshold */
	g_settings_bind (mouse_settings, "drag-threshold",
			 gtk_range_get_adjustment (GTK_RANGE (WID ("drag_threshold_scale"))), "value",
			 G_SETTINGS_BIND_DEFAULT);

	/* Trackpad page */
	if (touchpad_is_present () == FALSE)
		gtk_notebook_remove_page (GTK_NOTEBOOK (WID ("prefs_widget")), -1);
	else {
		g_settings_bind (touchpad_settings, "disable-while-typing",
				 WID ("disable_w_typing_toggle"), "active",
				 G_SETTINGS_BIND_DEFAULT);
		g_settings_bind (touchpad_settings, "tap-to-click",
				 WID ("tap_to_click_toggle"), "active",
				 G_SETTINGS_BIND_DEFAULT);
		g_settings_bind (touchpad_settings, "horiz-scroll-enabled",
				 WID ("horiz_scroll_toggle"), "active",
				 G_SETTINGS_BIND_DEFAULT);
		g_settings_bind (touchpad_settings, "motion-acceleration",
				 gtk_range_get_adjustment (GTK_RANGE (WID ("touchpad_accel_scale"))), "value",
				 G_SETTINGS_BIND_DEFAULT);
		g_settings_bind (touchpad_settings, "motion-threshold",
				 gtk_range_get_adjustment (GTK_RANGE (WID ("touchpad_sensitivity_scale"))), "value",
				 G_SETTINGS_BIND_DEFAULT);

		g_signal_connect (WID ("scroll_disabled_radio"), "toggled",
				  G_CALLBACK (scrollmethod_changed_event), dialog);
		g_signal_connect (WID ("scroll_edge_radio"), "toggled",
				  G_CALLBACK (scrollmethod_changed_event), dialog);
		g_signal_connect (WID ("scroll_twofinger_radio"), "toggled",
				  G_CALLBACK (scrollmethod_changed_event), dialog);

		synaptics_check_capabilities (dialog);
		scrollmethod_changed_event (GTK_TOGGLE_BUTTON (WID ("scroll_disabled_radio")), dialog);
	}

}

/* Construct the dialog */

static void
create_dialog (GtkBuilder *dialog)
{
	GtkSizeGroup *size_group;

	size_group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);
	gtk_size_group_add_widget (size_group, WID ("acceleration_label"));
	gtk_size_group_add_widget (size_group, WID ("sensitivity_label"));
	gtk_size_group_add_widget (size_group, WID ("threshold_label"));
	gtk_size_group_add_widget (size_group, WID ("timeout_label"));

	size_group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);
	gtk_size_group_add_widget (size_group, WID ("acceleration_fast_label"));
	gtk_size_group_add_widget (size_group, WID ("sensitivity_high_label"));
	gtk_size_group_add_widget (size_group, WID ("threshold_large_label"));
	gtk_size_group_add_widget (size_group, WID ("timeout_long_label"));

	size_group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);
	gtk_size_group_add_widget (size_group, WID ("acceleration_slow_label"));
	gtk_size_group_add_widget (size_group, WID ("sensitivity_low_label"));
	gtk_size_group_add_widget (size_group, WID ("threshold_small_label"));
	gtk_size_group_add_widget (size_group, WID ("timeout_short_label"));

	size_group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);
	gtk_size_group_add_widget (size_group, WID ("simulated_delay_label"));
	gtk_size_group_add_widget (size_group, WID ("dwell_delay_label"));
	gtk_size_group_add_widget (size_group, WID ("dwell_threshold_label"));

	size_group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);
	gtk_size_group_add_widget (size_group, WID ("simulated_delay_short_label"));
	gtk_size_group_add_widget (size_group, WID ("dwell_delay_short_label"));
	gtk_size_group_add_widget (size_group, WID ("dwell_threshold_small_label"));

	size_group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);
	gtk_size_group_add_widget (size_group, WID ("simulated_delay_long_label"));
	gtk_size_group_add_widget (size_group, WID ("dwell_delay_long_label"));
	gtk_size_group_add_widget (size_group, WID ("dwell_threshold_large_label"));

}

/* Callback issued when a button is clicked on the dialog */

static void
dialog_response_cb (GtkDialog *dialog, gint response_id, gpointer user_data)
{
/*
	if (response_id == GTK_RESPONSE_HELP)
		capplet_help (GTK_WINDOW (dialog),
			      "goscustperiph-5");
	else
		gtk_main_quit ();
*/
}

GtkWidget *
gnome_mouse_properties_init (GtkBuilder *dialog)
{
	GtkWidget      *dialog_win, *w;
	gchar *start_page = NULL;

	mouse_settings = g_settings_new ("org.gnome.settings-daemon.peripherals.mouse");
	touchpad_settings = g_settings_new ("org.gnome.settings-daemon.peripherals.touchpad");

	create_dialog (dialog);

	if (dialog) {
		setup_dialog (dialog);

		dialog_win = WID ("mouse_properties_dialog");
		g_signal_connect (dialog_win, "response",
				  G_CALLBACK (dialog_response_cb), NULL);

		if (start_page != NULL) {
			gchar *page_name;

			page_name = g_strconcat (start_page, "_vbox", NULL);
			g_free (start_page);

			w = WID (page_name);
			if (w != NULL) {
				GtkNotebook *nb;
				gint pindex;

				nb = GTK_NOTEBOOK (WID ("prefs_widget"));
				pindex = gtk_notebook_page_num (nb, w);
				if (pindex != -1)
					gtk_notebook_set_current_page (nb, pindex);
			}
			g_free (page_name);
		}

	} else {
		dialog_win = NULL;
	}

	return dialog_win;
}
