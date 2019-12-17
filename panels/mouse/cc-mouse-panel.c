/*
 * Copyright (C) 2010 Intel, Inc
 * Copyright (C) 2012 Red Hat, Inc.
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
 * Authors: Thomas Wood <thomas.wood@intel.com>
 *          Rodrigo Moya <rodrigo@gnome.org>
 *          Ondrej Holy <oholy@redhat.com>
 *
 */

#include <gdesktop-enums.h>
#include <gtk/gtk.h>

#include "cc-mouse-caps-helper.h"
#include "cc-mouse-panel.h"
#include "cc-mouse-resources.h"
#include "cc-mouse-test.h"
#include "gsd-device-manager.h"
#include "gsd-input-helper.h"
#include "list-box-helper.h"

struct _CcMousePanel
{
  CcPanel            parent_instance;

  GtkListBoxRow     *edge_scrolling_row;
  GtkSwitch         *edge_scrolling_switch;
  GtkListBox        *general_listbox;
  GtkFrame          *mouse_frame;
  GtkListBox        *mouse_listbox;
  GtkSwitch         *mouse_natural_scrolling_switch;
  GtkScale          *mouse_speed_scale;
  CcMouseTest       *mouse_test;
  GtkRadioButton    *primary_button_left;
  GtkRadioButton    *primary_button_right;
  GtkScrolledWindow *scrolled_window;
  GtkStack          *stack;
  GtkListBoxRow     *tap_to_click_row;
  GtkSwitch         *tap_to_click_switch;
  GtkButton         *test_button;
  GtkFrame          *touchpad_frame;
  GtkListBox        *touchpad_listbox;
  GtkListBoxRow     *touchpad_natural_scrolling_row;
  GtkSwitch         *touchpad_natural_scrolling_switch;
  GtkListBoxRow     *touchpad_speed_row;
  GtkScale          *touchpad_speed_scale;
  GtkSwitch         *touchpad_toggle_switch;
  GtkListBoxRow     *two_finger_scrolling_row;
  GtkSwitch         *two_finger_scrolling_switch;

  GSettings         *mouse_settings;
  GSettings         *gsd_mouse_settings;
  GSettings         *touchpad_settings;

  gboolean           have_mouse;
  gboolean           have_touchpad;
  gboolean           have_touchscreen;
  gboolean           have_synaptics;

  gboolean           left_handed;
  GtkGesture        *left_gesture;
  GtkGesture        *right_gesture;
};

CC_PANEL_REGISTER (CcMousePanel, cc_mouse_panel)

static void
setup_touchpad_options (CcMousePanel *self)
{
  gboolean edge_scroll_enabled;
  gboolean two_finger_scroll_enabled;
  gboolean have_two_finger_scrolling;
  gboolean have_edge_scrolling;
  gboolean have_tap_to_click;

  if (self->have_synaptics || !self->have_touchpad) {
    gtk_widget_hide (GTK_WIDGET (self->touchpad_frame));
    return;
  }

  cc_touchpad_check_capabilities (&have_two_finger_scrolling, &have_edge_scrolling, &have_tap_to_click);

  gtk_widget_show (GTK_WIDGET (self->touchpad_frame));

  gtk_widget_set_visible (GTK_WIDGET (self->two_finger_scrolling_row), have_two_finger_scrolling);
  gtk_widget_set_visible (GTK_WIDGET (self->edge_scrolling_row), have_edge_scrolling);
  gtk_widget_set_visible (GTK_WIDGET (self->tap_to_click_row), have_tap_to_click);

  edge_scroll_enabled = g_settings_get_boolean (self->touchpad_settings, "edge-scrolling-enabled");
  two_finger_scroll_enabled = g_settings_get_boolean (self->touchpad_settings, "two-finger-scrolling-enabled");
  if (edge_scroll_enabled && two_finger_scroll_enabled)
  {
    /* You cunning user set both, but you can only have one set in that UI */
    gtk_switch_set_active (self->edge_scrolling_switch, FALSE);
  }
}

static void
two_finger_scrolling_changed_event (CcMousePanel *self,
                                    gboolean      state)
{
  /* Updating the setting will cause the "state" of the switch to be updated. */
  g_settings_set_boolean (self->touchpad_settings, "two-finger-scrolling-enabled", state);

  /* Disable edge scrolling if two-finger scrolling is enabled */
  if (state && gtk_widget_get_visible (GTK_WIDGET (self->edge_scrolling_row)))
    gtk_switch_set_active (self->edge_scrolling_switch, FALSE);
}

static void
edge_scrolling_changed_event (CcMousePanel *self,
                              gboolean      state)
{
  /* Updating the setting will cause the "state" of the switch to be updated. */
  g_settings_set_boolean (self->touchpad_settings, "edge-scrolling-enabled", state);

  /* Disable two-finger scrolling if edge scrolling is enabled */
  if (state && gtk_widget_get_visible (GTK_WIDGET (self->two_finger_scrolling_row)))
    gtk_switch_set_active (self->two_finger_scrolling_switch, FALSE);
}

static gboolean
get_touchpad_enabled (GSettings *settings)
{
  GDesktopDeviceSendEvents send_events;

  send_events = g_settings_get_enum (settings, "send-events");

  return send_events == G_DESKTOP_DEVICE_SEND_EVENTS_ENABLED;
}

static gboolean
show_touchpad_enabling_switch (CcMousePanel *self)
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
handle_secondary_button (CcMousePanel   *self,
                         GtkRadioButton *button,
                         GtkGesture     *gesture)
{
  gtk_gesture_single_set_touch_only (GTK_GESTURE_SINGLE (gesture), FALSE);
  gtk_gesture_single_set_exclusive (GTK_GESTURE_SINGLE (gesture), TRUE);
  gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (gesture), GDK_BUTTON_SECONDARY);
  g_signal_connect_swapped (gesture, "pressed", G_CALLBACK (gtk_button_clicked), button);
  gtk_event_controller_set_propagation_phase (GTK_EVENT_CONTROLLER (gesture), GTK_PHASE_BUBBLE);
}

/* Set up the property editors in the dialog. */
static void
setup_dialog (CcMousePanel *self)
{
  GtkRadioButton *button;

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
  self->right_gesture = gtk_gesture_multi_press_new (GTK_WIDGET (button));
  handle_secondary_button (self, button, self->right_gesture);
  button = self->primary_button_left;
  self->left_gesture = gtk_gesture_multi_press_new (GTK_WIDGET (button));
  handle_secondary_button (self, button, self->left_gesture);

  g_settings_bind (self->mouse_settings, "natural-scroll",
       self->mouse_natural_scrolling_switch, "active",
       G_SETTINGS_BIND_DEFAULT);

  gtk_list_box_set_header_func (self->general_listbox, cc_list_box_update_header_func, NULL, NULL);
  gtk_list_box_set_header_func (self->touchpad_listbox, cc_list_box_update_header_func, NULL, NULL);

  /* Mouse section */
  gtk_widget_set_visible (GTK_WIDGET (self->mouse_frame), self->have_mouse);

  g_settings_bind (self->mouse_settings, "speed",
                   gtk_range_get_adjustment (GTK_RANGE (self->mouse_speed_scale)), "value",
                   G_SETTINGS_BIND_DEFAULT);

  gtk_list_box_set_header_func (self->mouse_listbox, cc_list_box_update_header_func, NULL, NULL);

  /* Touchpad section */
  gtk_widget_set_visible (GTK_WIDGET (self->touchpad_toggle_switch), show_touchpad_enabling_switch (self));

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
device_changed (CcMousePanel *self)
{
  self->have_touchpad = touchpad_is_present ();

  setup_touchpad_options (self);

  self->have_mouse = mouse_is_present ();
  gtk_widget_set_visible (GTK_WIDGET (self->mouse_frame), self->have_mouse);
  gtk_widget_set_visible (GTK_WIDGET (self->touchpad_toggle_switch), show_touchpad_enabling_switch (self));
}

static void
on_content_size_changed (CcMousePanel  *self,
                         GtkAllocation *allocation)
{
  if (allocation->height < 490)
  {
    gtk_scrolled_window_set_policy (self->scrolled_window,
                                    GTK_POLICY_NEVER, GTK_POLICY_NEVER);
  }
  else
  {
    gtk_scrolled_window_set_policy (self->scrolled_window,
                                    GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_min_content_height (self->scrolled_window, 490);
  }
}

static void
cc_mouse_panel_dispose (GObject *object)
{
  CcMousePanel *self = CC_MOUSE_PANEL (object);

  g_clear_object (&self->mouse_settings);
  g_clear_object (&self->gsd_mouse_settings);
  g_clear_object (&self->touchpad_settings);
  g_clear_object (&self->right_gesture);
  g_clear_object (&self->left_gesture);

  G_OBJECT_CLASS (cc_mouse_panel_parent_class)->dispose (object);
}

static const char *
cc_mouse_panel_get_help_uri (CcPanel *panel)
{
  return "help:gnome-help/mouse";
}

static void
test_button_toggled_cb (CcMousePanel *self)
{
  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (self->test_button)))
    gtk_stack_set_visible_child (self->stack, GTK_WIDGET (self->mouse_test));
  else
    gtk_stack_set_visible_child (self->stack, GTK_WIDGET (self->scrolled_window));
}

static void
cc_mouse_panel_constructed (GObject *object)
{
  CcMousePanel *self = CC_MOUSE_PANEL (object);
  CcShell *shell;

  G_OBJECT_CLASS (cc_mouse_panel_parent_class)->constructed (object);

  /* Add test area button to shell header. */
  shell = cc_panel_get_shell (CC_PANEL (self));
  cc_shell_embed_widget_in_header (shell, GTK_WIDGET (self->test_button), GTK_POS_RIGHT);
}

static void
cc_mouse_panel_init (CcMousePanel *self)
{
  GsdDeviceManager  *device_manager;

  g_resources_register (cc_mouse_get_resource ());

  cc_mouse_test_get_type ();
  gtk_widget_init_template (GTK_WIDGET (self));

  self->mouse_settings = g_settings_new ("org.gnome.desktop.peripherals.mouse");
  self->gsd_mouse_settings = g_settings_new ("org.gnome.settings-daemon.peripherals.mouse");
  self->touchpad_settings = g_settings_new ("org.gnome.desktop.peripherals.touchpad");

  device_manager = gsd_device_manager_get ();
  g_signal_connect_object (device_manager, "device-added",
                           G_CALLBACK (device_changed), self, G_CONNECT_SWAPPED);
  g_signal_connect_object (device_manager, "device-removed",
                           G_CALLBACK (device_changed), self, G_CONNECT_SWAPPED);

  self->have_mouse = mouse_is_present ();
  self->have_touchpad = touchpad_is_present ();
  self->have_touchscreen = touchscreen_is_present ();
  self->have_synaptics = cc_synaptics_check ();
  if (self->have_synaptics)
    g_warning ("Detected synaptics X driver, please migrate to libinput");

  setup_dialog (self);
}

static void
cc_mouse_panel_class_init (CcMousePanelClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  CcPanelClass *panel_class = CC_PANEL_CLASS (klass);

  panel_class->get_help_uri = cc_mouse_panel_get_help_uri;

  object_class->dispose = cc_mouse_panel_dispose;
  object_class->constructed = cc_mouse_panel_constructed;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/mouse/cc-mouse-panel.ui");

  gtk_widget_class_bind_template_child (widget_class, CcMousePanel, edge_scrolling_row);
  gtk_widget_class_bind_template_child (widget_class, CcMousePanel, edge_scrolling_switch);
  gtk_widget_class_bind_template_child (widget_class, CcMousePanel, general_listbox);
  gtk_widget_class_bind_template_child (widget_class, CcMousePanel, mouse_frame);
  gtk_widget_class_bind_template_child (widget_class, CcMousePanel, mouse_listbox);
  gtk_widget_class_bind_template_child (widget_class, CcMousePanel, mouse_natural_scrolling_switch);
  gtk_widget_class_bind_template_child (widget_class, CcMousePanel, mouse_speed_scale);
  gtk_widget_class_bind_template_child (widget_class, CcMousePanel, mouse_test);
  gtk_widget_class_bind_template_child (widget_class, CcMousePanel, primary_button_left);
  gtk_widget_class_bind_template_child (widget_class, CcMousePanel, primary_button_right);
  gtk_widget_class_bind_template_child (widget_class, CcMousePanel, scrolled_window);
  gtk_widget_class_bind_template_child (widget_class, CcMousePanel, stack);
  gtk_widget_class_bind_template_child (widget_class, CcMousePanel, tap_to_click_row);
  gtk_widget_class_bind_template_child (widget_class, CcMousePanel, tap_to_click_switch);
  gtk_widget_class_bind_template_child (widget_class, CcMousePanel, test_button);
  gtk_widget_class_bind_template_child (widget_class, CcMousePanel, touchpad_frame);
  gtk_widget_class_bind_template_child (widget_class, CcMousePanel, touchpad_listbox);
  gtk_widget_class_bind_template_child (widget_class, CcMousePanel, touchpad_natural_scrolling_row);
  gtk_widget_class_bind_template_child (widget_class, CcMousePanel, touchpad_natural_scrolling_switch);
  gtk_widget_class_bind_template_child (widget_class, CcMousePanel, touchpad_speed_row);
  gtk_widget_class_bind_template_child (widget_class, CcMousePanel, touchpad_speed_scale);
  gtk_widget_class_bind_template_child (widget_class, CcMousePanel, touchpad_toggle_switch);
  gtk_widget_class_bind_template_child (widget_class, CcMousePanel, two_finger_scrolling_row);
  gtk_widget_class_bind_template_child (widget_class, CcMousePanel, two_finger_scrolling_switch);

  gtk_widget_class_bind_template_callback (widget_class, edge_scrolling_changed_event);
  gtk_widget_class_bind_template_callback (widget_class, on_content_size_changed);
  gtk_widget_class_bind_template_callback (widget_class, test_button_toggled_cb);
  gtk_widget_class_bind_template_callback (widget_class, two_finger_scrolling_changed_event);
}
