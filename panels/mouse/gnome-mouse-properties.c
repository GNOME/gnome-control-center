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

#include <gdesktop-enums.h>

#include "gnome-mouse-properties.h"
#include "gsd-input-helper.h"
#include "gsd-device-manager.h"
#include "list-box-helper.h"
#include "cc-mouse-caps-helper.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>

#define WID(x) (GtkWidget *) gtk_builder_get_object (d->builder, x)

#define CC_MOUSE_PROPERTIES_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), CC_TYPE_MOUSE_PROPERTIES, CcMousePropertiesPrivate))

struct _CcMousePropertiesPrivate
{
	GtkBuilder *builder;

	GSettings *mouse_settings;
	GSettings *gsd_mouse_settings;
	GSettings *touchpad_settings;

	GsdDeviceManager *device_manager;
	guint device_added_id;
	guint device_removed_id;

	gboolean have_mouse;
	gboolean have_touchpad;
	gboolean have_touchscreen;
	gboolean have_synaptics;

	gboolean left_handed;
	GtkGesture *left_gesture;
	GtkGesture *right_gesture;

	gboolean changing_scroll;
};

G_DEFINE_TYPE (CcMouseProperties, cc_mouse_properties, GTK_TYPE_BIN);

static void
setup_touchpad_options (CcMousePropertiesPrivate *d)
{
	gboolean edge_scroll_enabled;
	gboolean two_finger_scroll_enabled;
	gboolean have_two_finger_scrolling;
	gboolean have_edge_scrolling;
	gboolean have_tap_to_click;

	gtk_widget_set_visible (WID ("touchpad-frame"), !d->have_synaptics);
	if (d->have_synaptics)
		return;

	gtk_widget_set_visible (WID ("touchpad-frame"), d->have_touchpad);
	if (!d->have_touchpad)
		return;

	cc_touchpad_check_capabilities (&have_two_finger_scrolling, &have_edge_scrolling, &have_tap_to_click);

	gtk_widget_show_all (WID ("touchpad-frame"));

	gtk_widget_set_visible (WID ("two-finger-scrolling-row"), have_two_finger_scrolling);
	gtk_widget_set_visible (WID ("edge-scrolling-row"), have_edge_scrolling);
	gtk_widget_set_visible (WID ("tap-to-click-row"), have_tap_to_click);

	edge_scroll_enabled = g_settings_get_boolean (d->touchpad_settings, "edge-scrolling-enabled");
	two_finger_scroll_enabled = g_settings_get_boolean (d->touchpad_settings, "two-finger-scrolling-enabled");
	if (edge_scroll_enabled && two_finger_scroll_enabled) {
		/* You cunning user set both, but you can only have one set in that UI */
		d->changing_scroll = TRUE;
		gtk_switch_set_active (GTK_SWITCH (WID ("two-finger-scrolling-switch")), two_finger_scroll_enabled);
		d->changing_scroll = FALSE;
		gtk_switch_set_active (GTK_SWITCH (WID ("edge-scrolling-switch")), FALSE);
	} else {
		d->changing_scroll = TRUE;
		gtk_switch_set_active (GTK_SWITCH (WID ("edge-scrolling-switch")), edge_scroll_enabled);
		gtk_switch_set_active (GTK_SWITCH (WID ("two-finger-scrolling-switch")), two_finger_scroll_enabled);
		d->changing_scroll = FALSE;
	}
}

static void
two_finger_scrolling_changed_event (GtkSwitch *button,
				    gboolean   state,
				    gpointer user_data)
{
	CcMousePropertiesPrivate *d = user_data;

	if (d->changing_scroll)
		return;

	g_settings_set_boolean (d->touchpad_settings, "two-finger-scrolling-enabled", state);
	gtk_switch_set_state (button, state);

	if (state && gtk_widget_get_visible (WID ("edge-scrolling-row"))) {
		/* Disable edge scrolling if two-finger scrolling is enabled */
		gtk_switch_set_state (GTK_SWITCH (WID ("edge-scrolling-switch")), FALSE);
	}
}

static void
edge_scrolling_changed_event (GtkSwitch *button,
			      gboolean   state,
			      gpointer user_data)
{
	CcMousePropertiesPrivate *d = user_data;

	if (d->changing_scroll)
		return;

	g_settings_set_boolean (d->touchpad_settings, "edge-scrolling-enabled", state);
	gtk_switch_set_state (button, state);

	if (state && gtk_widget_get_visible (WID ("two-finger-scrolling-row"))) {
		/* Disable two-finger scrolling if edge scrolling is enabled */
		gtk_switch_set_state (GTK_SWITCH (WID ("two-finger-scrolling-switch")), FALSE);
	}
}

static gboolean
get_touchpad_enabled (GSettings *settings)
{
        GDesktopDeviceSendEvents send_events;

        send_events = g_settings_get_enum (settings, "send-events");

        return send_events == G_DESKTOP_DEVICE_SEND_EVENTS_ENABLED;
}

static gboolean
show_touchpad_enabling_switch (CcMousePropertiesPrivate *d)
{
	if (!d->have_touchpad)
		return FALSE;

	g_debug ("Should we show the touchpad disable switch: have_mouse: %s have_touchscreen: %s\n",
		 d->have_mouse ? "true" : "false",
		 d->have_touchscreen ? "true" : "false");

	/* Let's show the button when a mouse or touchscreen is present */
	if (d->have_mouse || d->have_touchscreen)
		return TRUE;

	/* Let's also show when the touchpad is disabled. */
	if (!get_touchpad_enabled (d->touchpad_settings))
		return TRUE;

	return FALSE;
}

static gboolean
touchpad_enabled_get_mapping (GValue    *value,
                              GVariant  *variant,
                              gpointer   user_data)
{
        gboolean enabled;

        enabled = g_strcmp0 (g_variant_get_string (variant, NULL), "enabled") == 0;
        g_value_set_boolean (value, enabled);

        return TRUE;
}

static GVariant *
touchpad_enabled_set_mapping (const GValue              *value,
                              const GVariantType        *type,
                              gpointer                   user_data)
{
        gboolean enabled;

        enabled = g_value_get_boolean (value);

        return g_variant_new_string (enabled ? "enabled" : "disabled");
}

static void
handle_secondary_button (CcMousePropertiesPrivate *d,
			 GtkWidget                *button,
			 GtkGesture               *gesture)
{
	gtk_gesture_single_set_touch_only (GTK_GESTURE_SINGLE (gesture), FALSE);
	gtk_gesture_single_set_exclusive (GTK_GESTURE_SINGLE (gesture), TRUE);
	gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (gesture), GDK_BUTTON_SECONDARY);
	g_signal_connect_swapped (gesture, "pressed", G_CALLBACK (gtk_button_clicked), button);
	gtk_event_controller_set_propagation_phase (GTK_EVENT_CONTROLLER (gesture), GTK_PHASE_BUBBLE);

}

/* Set up the property editors in the dialog. */
static void
setup_dialog (CcMousePropertiesPrivate *d)
{
	GtkWidget *button;

	d->left_handed = g_settings_get_boolean (d->mouse_settings, "left-handed");
	button = WID (d->left_handed ? "primary-button-right" : "primary-button-left");
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), TRUE);

	g_settings_bind (d->mouse_settings, "left-handed",
			 WID ("primary-button-left"), "active",
			 G_SETTINGS_BIND_DEFAULT | G_SETTINGS_BIND_INVERT_BOOLEAN);
	g_settings_bind (d->mouse_settings, "left-handed",
			 WID ("primary-button-right"), "active",
			 G_SETTINGS_BIND_DEFAULT);

	/* Allow changing orientation with either button */
	button = WID ("primary-button-right");
	d->right_gesture = gtk_gesture_multi_press_new (button);
	handle_secondary_button (d, button, d->right_gesture);
	button = WID ("primary-button-left");
	d->left_gesture = gtk_gesture_multi_press_new (button);
	handle_secondary_button (d, button, d->left_gesture);

	g_settings_bind (d->mouse_settings, "natural-scroll",
			 WID ("mouse-natural-scrolling-switch"), "active",
			 G_SETTINGS_BIND_DEFAULT);

	gtk_list_box_set_header_func (GTK_LIST_BOX (WID ("general-listbox")), cc_list_box_update_header_func, NULL, NULL);
	gtk_list_box_set_header_func (GTK_LIST_BOX (WID ("touchpad-listbox")), cc_list_box_update_header_func, NULL, NULL);

	/* Mouse section */
	gtk_widget_set_visible (WID ("mouse-frame"), d->have_mouse);

	g_settings_bind (d->mouse_settings, "speed",
			 gtk_range_get_adjustment (GTK_RANGE (WID ("mouse-speed-scale"))), "value",
			 G_SETTINGS_BIND_DEFAULT);

	gtk_list_box_set_header_func (GTK_LIST_BOX (WID ("mouse-listbox")), cc_list_box_update_header_func, NULL, NULL);

	/* Touchpad section */
	gtk_widget_set_visible (WID ("touchpad-toggle-switch"),
				show_touchpad_enabling_switch (d));

	g_settings_bind_with_mapping (d->touchpad_settings, "send-events",
				      WID ("touchpad-toggle-switch"), "active",
				      G_SETTINGS_BIND_DEFAULT,
				      touchpad_enabled_get_mapping,
				      touchpad_enabled_set_mapping,
				      NULL, NULL);
	g_settings_bind_with_mapping (d->touchpad_settings, "send-events",
				      WID ("touchpad-options-listbox"), "sensitive",
				      G_SETTINGS_BIND_GET,
				      touchpad_enabled_get_mapping,
				      touchpad_enabled_set_mapping,
				      NULL, NULL);

	g_settings_bind (d->touchpad_settings, "natural-scroll",
                         WID ("touchpad-natural-scrolling-switch"), "active",
                         G_SETTINGS_BIND_DEFAULT);

	g_settings_bind (d->touchpad_settings, "speed",
			 gtk_range_get_adjustment (GTK_RANGE (WID ("touchpad-speed-scale"))), "value",
			 G_SETTINGS_BIND_DEFAULT);

	g_settings_bind (d->touchpad_settings, "tap-to-click",
			 WID ("tap-to-click-switch"), "active",
			 G_SETTINGS_BIND_DEFAULT);

	setup_touchpad_options (d);

	g_signal_connect (WID ("edge-scrolling-switch"), "state-set",
			  G_CALLBACK (edge_scrolling_changed_event), d);
	g_signal_connect (WID ("two-finger-scrolling-switch"), "state-set",
			  G_CALLBACK (two_finger_scrolling_changed_event), d);

	gtk_list_box_set_header_func (GTK_LIST_BOX (WID ("touchpad-options-listbox")), cc_list_box_update_header_func, NULL, NULL);
}

/* Callback issued when a button is clicked on the dialog */
static void
device_changed (GsdDeviceManager *device_manager,
		GsdDevice *device,
		CcMousePropertiesPrivate *d)
{
	d->have_touchpad = touchpad_is_present ();

	setup_touchpad_options (d);

	d->have_mouse = mouse_is_present ();
	gtk_widget_set_visible (WID ("mouse-frame"), d->have_mouse);
	gtk_widget_set_visible (WID ("touchpad-toggle-switch"),
				show_touchpad_enabling_switch (d));
}

static void
on_content_size_changed (GtkWidget     *widget,
			  GtkAllocation *allocation,
			  gpointer       data)
{
	if (allocation->height < 490) {
		gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (widget),
						GTK_POLICY_NEVER, GTK_POLICY_NEVER);
	} else {
		gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (widget),
						GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
		gtk_scrolled_window_set_min_content_height (GTK_SCROLLED_WINDOW (widget), 490);
	}
}

static void
cc_mouse_properties_finalize (GObject *object)
{
	CcMousePropertiesPrivate *d = CC_MOUSE_PROPERTIES (object)->priv;

	g_clear_object (&d->mouse_settings);
	g_clear_object (&d->gsd_mouse_settings);
	g_clear_object (&d->touchpad_settings);
	g_clear_object (&d->builder);
	g_clear_object (&d->right_gesture);
	g_clear_object (&d->left_gesture);

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
	g_autoptr(GError) error = NULL;

	d->builder = gtk_builder_new ();
	gtk_builder_add_from_resource (d->builder,
				       "/org/gnome/control-center/mouse/gnome-mouse-properties.ui",
				       &error);

	d->mouse_settings = g_settings_new ("org.gnome.desktop.peripherals.mouse");
	d->gsd_mouse_settings = g_settings_new ("org.gnome.settings-daemon.peripherals.mouse");
	d->touchpad_settings = g_settings_new ("org.gnome.desktop.peripherals.touchpad");

	d->device_manager = gsd_device_manager_get ();
	d->device_added_id = g_signal_connect (d->device_manager, "device-added",
					       G_CALLBACK (device_changed), d);
	d->device_removed_id = g_signal_connect (d->device_manager, "device-removed",
						 G_CALLBACK (device_changed), d);

	d->have_mouse = mouse_is_present ();
	d->have_touchpad = touchpad_is_present ();
	d->have_touchscreen = touchscreen_is_present ();
	d->have_synaptics = cc_synaptics_check ();
	if (d->have_synaptics)
		g_warning ("Detected synaptics X driver, please migrate to libinput");

	d->changing_scroll = FALSE;

	gtk_container_add (GTK_CONTAINER (object), WID ("scrolled-window"));

	setup_dialog (d);

	g_signal_connect (WID ("scrolled-window"), "size-allocate", G_CALLBACK (on_content_size_changed), NULL);
}

GtkWidget *
cc_mouse_properties_new (void)
{
	return (GtkWidget *) g_object_new (CC_TYPE_MOUSE_PROPERTIES, NULL);
}
