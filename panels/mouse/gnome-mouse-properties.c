/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2001, 2012 Red Hat, Inc.
 * Copyright (C) 2001 Ximian, Inc.
 *
 * Written by: Jonathon Blandford <jrb@redhat.com>,
 *             Bradford Hovinen <hovinen@ximian.com>,
 *             Ondrej Holy <oholy@redhat.com>,
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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include <config.h>

#include <glib/gi18n.h>
#include <string.h>
#include <gdk/gdk.h>
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

#define WID(x) (GtkWidget *) gtk_builder_get_object (d->builder, x)

#define CC_MOUSE_PROPERTIES_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), CC_TYPE_MOUSE_PROPERTIES, CcMousePropertiesPrivate))

struct _CcMousePropertiesPrivate
{
	GtkBuilder *builder;

	GSettings *mouse_settings;
	GSettings *touchpad_settings;

	GdkDeviceManager *device_manager;
	guint device_added_id;
	guint device_removed_id;

	gboolean changing_scroll;
};

G_DEFINE_TYPE (CcMouseProperties, cc_mouse_properties, GTK_TYPE_ALIGNMENT);

static void
orientation_radio_button_release_event (GtkWidget   *widget,
				        GdkEventButton *event)
{
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), TRUE);
}

static void
setup_scrollmethod_radios (CcMousePropertiesPrivate *d)
{
        GsdTouchpadScrollMethod method;
        gboolean active;

        method = g_settings_get_enum (d->touchpad_settings, "scroll-method");
	active = (method == GSD_TOUCHPAD_SCROLL_METHOD_TWO_FINGER_SCROLLING);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (WID ("two_finger_scroll_toggle")), active);
}

static void
scrollmethod_changed_event (GtkToggleButton *button, CcMousePropertiesPrivate *d)
{
	GsdTouchpadScrollMethod method;

	if (d->changing_scroll)
		return;

	if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (WID ("two_finger_scroll_toggle"))))
		method = GSD_TOUCHPAD_SCROLL_METHOD_TWO_FINGER_SCROLLING;
	else
		method = GSD_TOUCHPAD_SCROLL_METHOD_EDGE_SCROLLING;

	g_settings_set_enum (d->touchpad_settings, "scroll-method", method);
	g_settings_set_boolean (d->touchpad_settings, "horiz-scroll-enabled", TRUE);
}

static void
synaptics_check_capabilities (CcMousePropertiesPrivate *d)
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
				gtk_widget_set_sensitive (WID ("tap_to_click_toggle"), FALSE);
			}

			/* Disable two finger scrolling unless the hardware supports
			 * double touch */
			if (!(data[3]))
				gtk_widget_set_sensitive (WID ("two_finger_scroll_toggle"), FALSE);

			XFree (data);
		}
		gdk_error_trap_pop_ignored ();

		XCloseDevice (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()), device);
	}
	XFreeDeviceList (devicelist);
}

static void
pointer_speed_scale_event (GtkRange *scale, CcMousePropertiesPrivate *d)
{
	gdouble value;
	GSettings *settings;
	GtkAdjustment *adjustment;

	if (GTK_WIDGET (scale) == WID ("pointer_speed_scale"))
		settings = d->mouse_settings;
	else
		settings = d->touchpad_settings;

	adjustment = gtk_range_get_adjustment (scale);
	value = gtk_adjustment_get_upper (adjustment) - gtk_range_get_value (scale) + 1;
	g_settings_set_int (settings, "motion-threshold", value);
}

static gboolean
show_touchpad_enabling_switch (GSettings *touchpad_settings)
{
	gboolean enabled;

	if (!touchpad_is_present())
		return FALSE;

	/* Lets show the button when the mouse/touchscreen is present */
	if (mouse_is_present() || touchscreen_is_present())
		return TRUE;
	
	/* Lets also show when touch pad is disabled. */
	enabled = g_settings_get_boolean (touchpad_settings,  "touchpad-enabled");
	if (!enabled)
		return TRUE;
	
	return FALSE;
}

/* Set up the property editors in the dialog. */
static void
setup_dialog (CcMousePropertiesPrivate *d)
{
	GtkRadioButton *radio;
	gboolean        touchpad_present, mouse_present;

	/* Orientation radio buttons */
	radio = GTK_RADIO_BUTTON (WID ("left_handed_radio"));
	g_settings_bind (d->mouse_settings, "left-handed", radio, "active", G_SETTINGS_BIND_DEFAULT);

	/* explicitly connect to button-release so that you can change orientation with either button */
	g_signal_connect (WID ("right_handed_radio"), "button_release_event",
		G_CALLBACK (orientation_radio_button_release_event), NULL);
	g_signal_connect (WID ("left_handed_radio"), "button_release_event",
		G_CALLBACK (orientation_radio_button_release_event), NULL);

	/* Double-click time */
	g_settings_bind (d->mouse_settings, "double-click",
			 gtk_range_get_adjustment (GTK_RANGE (WID ("double_click_scale"))), "value",
			 G_SETTINGS_BIND_DEFAULT);

	/* Mouse section */
	mouse_present = mouse_is_present ();
	gtk_widget_set_visible (WID ("mouse_vbox"), mouse_present);

	g_settings_bind (d->mouse_settings, "motion-acceleration",
			 gtk_range_get_adjustment (GTK_RANGE (WID ("pointer_speed_scale"))), "value",
			 G_SETTINGS_BIND_DEFAULT);

	g_signal_connect (WID ("pointer_speed_scale"), "value-changed",
			  G_CALLBACK (pointer_speed_scale_event), d);

	/* Trackpad page */
	touchpad_present = touchpad_is_present ();
	gtk_widget_set_visible (WID ("touchpad_vbox"), touchpad_present);
	gtk_widget_set_visible (WID ("touchpad_enabled_switch"), 
				show_touchpad_enabling_switch (d->touchpad_settings));

	g_settings_bind (d->touchpad_settings, "touchpad-enabled",
			 WID ("touchpad_enabled_switch"), "active",
			 G_SETTINGS_BIND_DEFAULT);
	g_settings_bind (d->touchpad_settings, "touchpad-enabled",
			 WID ("touchpad_options_box"), "sensitive",
			 G_SETTINGS_BIND_GET);

	g_settings_bind (d->touchpad_settings, "disable-while-typing",
			 WID ("disable_w_typing_toggle"), "active",
			 G_SETTINGS_BIND_DEFAULT);
	g_settings_bind (d->touchpad_settings, "tap-to-click",
			 WID ("tap_to_click_toggle"), "active",
			 G_SETTINGS_BIND_DEFAULT);
	g_settings_bind (d->touchpad_settings, "natural-scroll",
			 WID ("natural_scroll_toggle"), "active",
			 G_SETTINGS_BIND_DEFAULT);
	g_settings_bind (d->touchpad_settings, "motion-acceleration",
			 gtk_range_get_adjustment (GTK_RANGE (WID ("touchpad_pointer_speed_scale"))), "value",
			 G_SETTINGS_BIND_DEFAULT);

	g_signal_connect (WID ("touchpad_pointer_speed_scale"), "value-changed",
			  G_CALLBACK (pointer_speed_scale_event), d);

	if (touchpad_present) {
		synaptics_check_capabilities (d);
		setup_scrollmethod_radios (d);
	}

	g_signal_connect (WID ("two_finger_scroll_toggle"), "toggled",
			  G_CALLBACK (scrollmethod_changed_event), d);
}

/* Construct the dialog */

static void
create_dialog (CcMousePropertiesPrivate *d)
{
	GtkSizeGroup *size_group;

	size_group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);
	gtk_size_group_add_widget (size_group, WID ("primary_button_label"));
	gtk_size_group_add_widget (size_group, WID ("pointer_speed_label"));
	gtk_size_group_add_widget (size_group, WID ("double_click_label"));
	gtk_size_group_add_widget (size_group, WID ("touchpad_pointer_speed_label"));

	size_group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);
	gtk_size_group_add_widget (size_group, WID ("pointer_speed_fast_label"));
	gtk_size_group_add_widget (size_group, WID ("double_click_fast_label"));
	gtk_size_group_add_widget (size_group, WID ("touchpad_pointer_speed_fast_label"));

	size_group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);
	gtk_size_group_add_widget (size_group, WID ("pointer_speed_slow_label"));
	gtk_size_group_add_widget (size_group, WID ("double_click_slow_label"));
	gtk_size_group_add_widget (size_group, WID ("touchpad_pointer_speed_slow_label"));

	gtk_widget_set_direction (WID ("primary_button_box"), GTK_TEXT_DIR_LTR);
}

/* Callback issued when a button is clicked on the dialog */

static void
device_changed (GdkDeviceManager *device_manager,
		GdkDevice *device,
		CcMousePropertiesPrivate *d)
{
	gboolean present;

	present = touchpad_is_present ();
	gtk_widget_set_visible (WID ("touchpad_vbox"), present);

	if (present) {
		d->changing_scroll = TRUE;
		synaptics_check_capabilities (d);
		setup_scrollmethod_radios (d);
		d->changing_scroll = FALSE;
	}

	present = mouse_is_present ();
	gtk_widget_set_visible (WID ("mouse_vbox"), present);
	gtk_widget_set_visible (WID ("touchpad_enabled_switch"), 
				show_touchpad_enabling_switch (d->touchpad_settings));
}


static void
cc_mouse_properties_finalize (GObject *object)
{
	CcMousePropertiesPrivate *d = CC_MOUSE_PROPERTIES (object)->priv;

	g_clear_object (&d->mouse_settings);
	g_clear_object (&d->touchpad_settings);
	g_clear_object (&d->builder);

	if (d->device_manager != NULL) {
		g_signal_handler_disconnect (d->device_manager, d->device_added_id);
		d->device_added_id = 0;
		g_signal_handler_disconnect (d->device_manager, d->device_removed_id);
		d->device_removed_id = 0;
		d->device_manager = NULL;
	}

	G_OBJECT_CLASS (cc_mouse_properties_parent_class)->finalize (object);
}

static void
cc_mouse_properties_class_init (CcMousePropertiesClass *class)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->finalize = cc_mouse_properties_finalize;

	g_type_class_add_private (class, sizeof (CcMousePropertiesPrivate));
}

static void
cc_mouse_properties_init (CcMouseProperties *object)
{
	CcMousePropertiesPrivate *d = object->priv = CC_MOUSE_PROPERTIES_GET_PRIVATE (object);
	GError *error = NULL;

	d->builder = gtk_builder_new ();
	gtk_builder_add_from_resource (d->builder,
				       "/org/gnome/control-center/mouse/gnome-mouse-properties.ui",
				       &error);

	d->mouse_settings = g_settings_new ("org.gnome.settings-daemon.peripherals.mouse");
	d->touchpad_settings = g_settings_new ("org.gnome.settings-daemon.peripherals.touchpad");

	d->device_manager = gdk_display_get_device_manager (gdk_display_get_default ());
	d->device_added_id = g_signal_connect (d->device_manager, "device-added",
					       G_CALLBACK (device_changed), d);
	d->device_removed_id = g_signal_connect (d->device_manager, "device-removed",
						 G_CALLBACK (device_changed), d);

	d->changing_scroll = FALSE;

	gtk_container_add (GTK_CONTAINER (object), WID ("prefs_widget"));

	create_dialog (d);
	setup_dialog (d);
}

GtkWidget *
cc_mouse_properties_new (void)
{
	return (GtkWidget *) g_object_new (CC_TYPE_MOUSE_PROPERTIES, NULL);
}
