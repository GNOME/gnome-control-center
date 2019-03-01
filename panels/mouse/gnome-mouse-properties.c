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

struct _CcMouseProperties
{
	GtkBin parent_instance;

	GtkWidget *edge_scrolling_row;
	GtkWidget *edge_scrolling_switch;
	GtkWidget *general_listbox;
	GtkWidget *mouse_frame;
	GtkWidget *mouse_listbox;
	GtkWidget *mouse_natural_scrolling_switch;
	GtkWidget *mouse_speed_scale;
	GtkWidget *primary_button_left;
	GtkWidget *primary_button_right;
	GtkWidget *scrolled_window;
	GtkWidget *tap_to_click_row;
	GtkWidget *tap_to_click_switch;
	GtkWidget *touchpad_frame;
	GtkWidget *touchpad_listbox;
	GtkWidget *touchpad_natural_scrolling_row;
	GtkWidget *touchpad_natural_scrolling_switch;
	GtkWidget *touchpad_speed_row;
	GtkWidget *touchpad_speed_scale;
	GtkWidget *touchpad_toggle_switch;
	GtkWidget *two_finger_scrolling_row;
	GtkWidget *two_finger_scrolling_switch;

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
};

G_DEFINE_TYPE (CcMouseProperties, cc_mouse_properties, GTK_TYPE_BIN);

static void
setup_touchpad_options (CcMouseProperties *self)
{
	gboolean edge_scroll_enabled;
	gboolean two_finger_scroll_enabled;
	gboolean have_two_finger_scrolling;
	gboolean have_edge_scrolling;
	gboolean have_tap_to_click;

	if (self->have_synaptics || !self->have_touchpad) {
		gtk_widget_hide (self->touchpad_frame);
		return;
	}

	cc_touchpad_check_capabilities (&have_two_finger_scrolling, &have_edge_scrolling, &have_tap_to_click);

	gtk_widget_show (self->touchpad_frame);

	gtk_widget_set_visible (self->two_finger_scrolling_row, have_two_finger_scrolling);
	gtk_widget_set_visible (self->edge_scrolling_row, have_edge_scrolling);
	gtk_widget_set_visible (self->tap_to_click_row, have_tap_to_click);

	edge_scroll_enabled = g_settings_get_boolean (self->touchpad_settings, "edge-scrolling-enabled");
	two_finger_scroll_enabled = g_settings_get_boolean (self->touchpad_settings, "two-finger-scrolling-enabled");
	if (edge_scroll_enabled && two_finger_scroll_enabled) {
		/* You cunning user set both, but you can only have one set in that UI */
		gtk_switch_set_active (GTK_SWITCH (self->edge_scrolling_switch), FALSE);
	}
}

static void
two_finger_scrolling_changed_event (CcMouseProperties *self,
				    gboolean           state)
{
	/* Updating the setting will cause the "state" of the switch to be updated. */
	g_settings_set_boolean (self->touchpad_settings, "two-finger-scrolling-enabled", state);

	if (state && gtk_widget_get_visible (self->edge_scrolling_row)) {
		/* Disable edge scrolling if two-finger scrolling is enabled */
		gtk_switch_set_active (GTK_SWITCH (self->edge_scrolling_switch), FALSE);
	}
}

static void
edge_scrolling_changed_event (CcMouseProperties *self,
			      gboolean           state)
{
	/* Updating the setting will cause the "state" of the switch to be updated. */
	g_settings_set_boolean (self->touchpad_settings, "edge-scrolling-enabled", state);

	if (state && gtk_widget_get_visible (self->two_finger_scrolling_row)) {
		/* Disable two-finger scrolling if edge scrolling is enabled */
		gtk_switch_set_active (GTK_SWITCH (self->two_finger_scrolling_switch), FALSE);
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
show_touchpad_enabling_switch (CcMouseProperties *self)
{
	if (!self->have_touchpad)
		return FALSE;

	g_debug ("Should we show the touchpad disable switch: have_mouse: %s have_touchscreen: %s\n",
		 self->have_mouse ? "true" : "false",
		 self->have_touchscreen ? "true" : "false");

	/* Let's show the button when a mouse or touchscreen is present */
	if (self->have_mouse || self->have_touchscreen)
		return TRUE;

	/* Let's also show when the touchpad is disabled. */
	if (!get_touchpad_enabled (self->touchpad_settings))
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
handle_secondary_button (CcMouseProperties *self,
			 GtkWidget         *button,
			 GtkGesture        *gesture)
{
	gtk_gesture_single_set_touch_only (GTK_GESTURE_SINGLE (gesture), FALSE);
	gtk_gesture_single_set_exclusive (GTK_GESTURE_SINGLE (gesture), TRUE);
	gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (gesture), GDK_BUTTON_SECONDARY);
	g_signal_connect_swapped (gesture, "pressed", G_CALLBACK (gtk_button_clicked), button);
	gtk_event_controller_set_propagation_phase (GTK_EVENT_CONTROLLER (gesture), GTK_PHASE_BUBBLE);

}

/* Set up the property editors in the dialog. */
static void
setup_dialog (CcMouseProperties *self)
{
	GtkWidget *button;

	self->left_handed = g_settings_get_boolean (self->mouse_settings, "left-handed");
	button = self->left_handed ? self->primary_button_right : self->primary_button_left;
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), TRUE);

	g_settings_bind (self->mouse_settings, "left-handed",
			 self->primary_button_left, "active",
			 G_SETTINGS_BIND_DEFAULT | G_SETTINGS_BIND_INVERT_BOOLEAN);
	g_settings_bind (self->mouse_settings, "left-handed",
			 self->primary_button_right, "active",
			 G_SETTINGS_BIND_DEFAULT);

	/* Allow changing orientation with either button */
	button = self->primary_button_right;
	self->right_gesture = gtk_gesture_multi_press_new (button);
	handle_secondary_button (self, button, self->right_gesture);
	button = self->primary_button_left;
	self->left_gesture = gtk_gesture_multi_press_new (button);
	handle_secondary_button (self, button, self->left_gesture);

	g_settings_bind (self->mouse_settings, "natural-scroll",
			 self->mouse_natural_scrolling_switch, "active",
			 G_SETTINGS_BIND_DEFAULT);

	gtk_list_box_set_header_func (GTK_LIST_BOX (self->general_listbox), cc_list_box_update_header_func, NULL, NULL);
	gtk_list_box_set_header_func (GTK_LIST_BOX (self->touchpad_listbox), cc_list_box_update_header_func, NULL, NULL);

	/* Mouse section */
	gtk_widget_set_visible (self->mouse_frame, self->have_mouse);

	g_settings_bind (self->mouse_settings, "speed",
			 gtk_range_get_adjustment (GTK_RANGE (self->mouse_speed_scale)), "value",
			 G_SETTINGS_BIND_DEFAULT);

	gtk_list_box_set_header_func (GTK_LIST_BOX (self->mouse_listbox), cc_list_box_update_header_func, NULL, NULL);

	/* Touchpad section */
	gtk_widget_set_visible (self->touchpad_toggle_switch,
				show_touchpad_enabling_switch (self));

	g_settings_bind_with_mapping (self->touchpad_settings, "send-events",
				      self->touchpad_toggle_switch, "active",
				      G_SETTINGS_BIND_DEFAULT,
				      touchpad_enabled_get_mapping,
				      touchpad_enabled_set_mapping,
				      NULL, NULL);
	g_settings_bind_with_mapping (self->touchpad_settings, "send-events",
				      self->touchpad_natural_scrolling_row, "sensitive",
				      G_SETTINGS_BIND_GET,
				      touchpad_enabled_get_mapping,
				      touchpad_enabled_set_mapping,
				      NULL, NULL);
	g_settings_bind_with_mapping (self->touchpad_settings, "send-events",
				      self->touchpad_speed_row, "sensitive",
				      G_SETTINGS_BIND_GET,
				      touchpad_enabled_get_mapping,
				      touchpad_enabled_set_mapping,
				      NULL, NULL);
	g_settings_bind_with_mapping (self->touchpad_settings, "send-events",
				      self->tap_to_click_row, "sensitive",
				      G_SETTINGS_BIND_GET,
				      touchpad_enabled_get_mapping,
				      touchpad_enabled_set_mapping,
				      NULL, NULL);
	g_settings_bind_with_mapping (self->touchpad_settings, "send-events",
				      self->two_finger_scrolling_row, "sensitive",
				      G_SETTINGS_BIND_GET,
				      touchpad_enabled_get_mapping,
				      touchpad_enabled_set_mapping,
				      NULL, NULL);
	g_settings_bind_with_mapping (self->touchpad_settings, "send-events",
				      self->edge_scrolling_row, "sensitive",
				      G_SETTINGS_BIND_GET,
				      touchpad_enabled_get_mapping,
				      touchpad_enabled_set_mapping,
				      NULL, NULL);

	g_settings_bind (self->touchpad_settings, "natural-scroll",
                         self->touchpad_natural_scrolling_switch, "active",
                         G_SETTINGS_BIND_DEFAULT);

	g_settings_bind (self->touchpad_settings, "speed",
			 gtk_range_get_adjustment (GTK_RANGE (self->touchpad_speed_scale)), "value",
			 G_SETTINGS_BIND_DEFAULT);

	g_settings_bind (self->touchpad_settings, "tap-to-click",
			 self->tap_to_click_switch, "active",
			 G_SETTINGS_BIND_DEFAULT);

	g_settings_bind (self->touchpad_settings, "two-finger-scrolling-enabled",
                         self->two_finger_scrolling_switch, "state",
                         G_SETTINGS_BIND_GET);

	g_settings_bind (self->touchpad_settings, "edge-scrolling-enabled",
                         self->edge_scrolling_switch, "state",
                         G_SETTINGS_BIND_GET);

	setup_touchpad_options (self);
}

/* Callback issued when a button is clicked on the dialog */
static void
device_changed (GsdDeviceManager *device_manager,
		GsdDevice *device,
		CcMouseProperties *self)
{
	self->have_touchpad = touchpad_is_present ();

	setup_touchpad_options (self);

	self->have_mouse = mouse_is_present ();
	gtk_widget_set_visible (self->mouse_frame, self->have_mouse);
	gtk_widget_set_visible (self->touchpad_toggle_switch,
				show_touchpad_enabling_switch (self));
}

static void
on_content_size_changed (CcMouseProperties *self,
			 GtkAllocation     *allocation)
{
	if (allocation->height < 490) {
		gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (self->scrolled_window),
						GTK_POLICY_NEVER, GTK_POLICY_NEVER);
	} else {
		gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (self->scrolled_window),
						GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
		gtk_scrolled_window_set_min_content_height (GTK_SCROLLED_WINDOW (self->scrolled_window), 490);
	}
}

static void
cc_mouse_properties_finalize (GObject *object)
{
	CcMouseProperties *self = CC_MOUSE_PROPERTIES (object);

	g_clear_object (&self->mouse_settings);
	g_clear_object (&self->gsd_mouse_settings);
	g_clear_object (&self->touchpad_settings);
	g_clear_object (&self->right_gesture);
	g_clear_object (&self->left_gesture);

	if (self->device_manager != NULL) {
		g_signal_handler_disconnect (self->device_manager, self->device_added_id);
		self->device_added_id = 0;
		g_signal_handler_disconnect (self->device_manager, self->device_removed_id);
		self->device_removed_id = 0;
		self->device_manager = NULL;
	}

	G_OBJECT_CLASS (cc_mouse_properties_parent_class)->finalize (object);
}

static void
cc_mouse_properties_class_init (CcMousePropertiesClass *klass)
{
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = cc_mouse_properties_finalize;

	gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/mouse/gnome-mouse-properties.ui");

	gtk_widget_class_bind_template_child (widget_class, CcMouseProperties, edge_scrolling_row);
	gtk_widget_class_bind_template_child (widget_class, CcMouseProperties, edge_scrolling_switch);
	gtk_widget_class_bind_template_child (widget_class, CcMouseProperties, general_listbox);
	gtk_widget_class_bind_template_child (widget_class, CcMouseProperties, mouse_frame);
	gtk_widget_class_bind_template_child (widget_class, CcMouseProperties, mouse_listbox);
	gtk_widget_class_bind_template_child (widget_class, CcMouseProperties, mouse_natural_scrolling_switch);
	gtk_widget_class_bind_template_child (widget_class, CcMouseProperties, mouse_speed_scale);
	gtk_widget_class_bind_template_child (widget_class, CcMouseProperties, primary_button_left);
	gtk_widget_class_bind_template_child (widget_class, CcMouseProperties, primary_button_right);
	gtk_widget_class_bind_template_child (widget_class, CcMouseProperties, scrolled_window);
	gtk_widget_class_bind_template_child (widget_class, CcMouseProperties, tap_to_click_row);
	gtk_widget_class_bind_template_child (widget_class, CcMouseProperties, tap_to_click_switch);
	gtk_widget_class_bind_template_child (widget_class, CcMouseProperties, touchpad_frame);
	gtk_widget_class_bind_template_child (widget_class, CcMouseProperties, touchpad_listbox);
	gtk_widget_class_bind_template_child (widget_class, CcMouseProperties, touchpad_natural_scrolling_row);
	gtk_widget_class_bind_template_child (widget_class, CcMouseProperties, touchpad_natural_scrolling_switch);
	gtk_widget_class_bind_template_child (widget_class, CcMouseProperties, touchpad_speed_row);
	gtk_widget_class_bind_template_child (widget_class, CcMouseProperties, tap_to_click_row);
	gtk_widget_class_bind_template_child (widget_class, CcMouseProperties, two_finger_scrolling_row);
	gtk_widget_class_bind_template_child (widget_class, CcMouseProperties, edge_scrolling_row);
	gtk_widget_class_bind_template_child (widget_class, CcMouseProperties, touchpad_speed_scale);
	gtk_widget_class_bind_template_child (widget_class, CcMouseProperties, touchpad_toggle_switch);
	gtk_widget_class_bind_template_child (widget_class, CcMouseProperties, two_finger_scrolling_row);
	gtk_widget_class_bind_template_child (widget_class, CcMouseProperties, two_finger_scrolling_switch);

	gtk_widget_class_bind_template_callback (widget_class, edge_scrolling_changed_event);
	gtk_widget_class_bind_template_callback (widget_class, two_finger_scrolling_changed_event);
	gtk_widget_class_bind_template_callback (widget_class, on_content_size_changed);
}

static void
cc_mouse_properties_init (CcMouseProperties *self)
{
	g_autoptr(GError) error = NULL;

	gtk_widget_init_template (GTK_WIDGET (self));

	self->mouse_settings = g_settings_new ("org.gnome.desktop.peripherals.mouse");
	self->gsd_mouse_settings = g_settings_new ("org.gnome.settings-daemon.peripherals.mouse");
	self->touchpad_settings = g_settings_new ("org.gnome.desktop.peripherals.touchpad");

	self->device_manager = gsd_device_manager_get ();
	self->device_added_id = g_signal_connect (self->device_manager, "device-added",
					       G_CALLBACK (device_changed), self);
	self->device_removed_id = g_signal_connect (self->device_manager, "device-removed",
						 G_CALLBACK (device_changed), self);

	self->have_mouse = mouse_is_present ();
	self->have_touchpad = touchpad_is_present ();
	self->have_touchscreen = touchscreen_is_present ();
	self->have_synaptics = cc_synaptics_check ();
	if (self->have_synaptics)
		g_warning ("Detected synaptics X driver, please migrate to libinput");

	setup_dialog (self);
}

GtkWidget *
cc_mouse_properties_new (void)
{
	return (GtkWidget *) g_object_new (CC_TYPE_MOUSE_PROPERTIES, NULL);
}
