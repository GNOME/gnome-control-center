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

#include "cc-illustrated-row.h"
#include "cc-split-row.h"
#include "cc-list-row-info-button.h"

#include "cc-mouse-caps-helper.h"
#include "cc-mouse-panel.h"
#include "cc-mouse-resources.h"
#include "cc-mouse-test.h"
#include "gsd-device-manager.h"
#include "gsd-input-helper.h"

struct _CcMousePanel
{
  CcPanel            parent_instance;

  GtkSwitch         *mouse_accel_switch;
  AdwPreferencesGroup *mouse_group;
  CcSplitRow        *mouse_scroll_direction_row;
  GtkScale          *mouse_speed_scale;
  GtkWindow         *mouse_test;

  AdwToggleGroup    *primary_toggle_group;
  CcSplitRow        *two_finger_push_row;
  GtkStack          *title_stack;
  CcIllustratedRow  *tap_to_click_row;
  GtkSwitch         *tap_to_click_switch;
  AdwViewStackPage  *touchpad_stack_page;
  CcSplitRow        *touchpad_scroll_direction_row;
  CcSplitRow        *touchpad_scroll_method_row;
  GtkScale          *touchpad_speed_scale;
  AdwSwitchRow      *touchpad_toggle_row;
  AdwSwitchRow      *touchpad_typing_row;

  GtkSwitch         *pointingstick_accel_switch;
  AdwViewStackPage  *pointingstick_stack_page;
  GtkScale          *pointingstick_speed_scale;
  GtkWindow         *pointingstick_test;

  GSettings         *mouse_settings;
  GSettings         *touchpad_settings;
  GSettings         *pointingstick_settings;

  gboolean           have_mouse;
  gboolean           have_touchpad;
  gboolean           have_touchscreen;
  gboolean           have_pointingstick;
  gboolean           have_synaptics;

  GtkGesture        *left_gesture;
  GtkGesture        *right_gesture;
};

CC_PANEL_REGISTER (CcMousePanel, cc_mouse_panel)


static void
setup_title_stack (CcMousePanel *self)
{
  gboolean have_pointingstick;
  gboolean have_touchpad;

  have_touchpad = self->have_touchpad && !self->have_synaptics;
  have_pointingstick = self->have_pointingstick;

  if (have_touchpad) {
    gboolean have_two_finger_scrolling;
    gboolean have_edge_scrolling;
    gboolean have_tap_to_click;

    cc_touchpad_check_capabilities (&have_two_finger_scrolling, &have_edge_scrolling, &have_tap_to_click);

    gtk_widget_set_visible (GTK_WIDGET (self->touchpad_scroll_method_row), have_two_finger_scrolling);
    gtk_widget_set_visible (GTK_WIDGET (self->tap_to_click_row), have_tap_to_click);
  }

  adw_view_stack_page_set_visible (self->touchpad_stack_page, have_touchpad);
  adw_view_stack_page_set_visible (self->pointingstick_stack_page, have_pointingstick);
  gtk_stack_set_visible_child_name (self->title_stack,
                                    have_pointingstick || have_touchpad ? "switcher" : "title");

}

static void
on_primary_button_changed_cb (CcMousePanel *self)
{
  const char *active_name;

  active_name = adw_toggle_group_get_active_name (self->primary_toggle_group);

  if (!active_name)
    return;

  if (g_strcmp0 (active_name, "left") == 0)
    g_settings_set_boolean (self->mouse_settings, "left-handed", FALSE);
  else
    g_settings_set_boolean (self->mouse_settings, "left-handed", TRUE);
}

static void
on_touchpad_scroll_method_changed_cb (CcMousePanel *self)
{
  gboolean two_finger;

  two_finger = cc_split_row_get_use_default (self->touchpad_scroll_method_row);

  if (g_settings_get_boolean (self->touchpad_settings,
                              "two-finger-scrolling-enabled") == two_finger)
    return;

  g_settings_set_boolean (self->touchpad_settings, "two-finger-scrolling-enabled", two_finger);
}

static gboolean
get_touchpad_enabled (GSettings *settings)
{
  GDesktopDeviceSendEvents send_events;

  send_events = g_settings_get_enum (settings, "send-events");

  return send_events == G_DESKTOP_DEVICE_SEND_EVENTS_ENABLED;
}

static gboolean
can_disable_touchpad (CcMousePanel *self)
{
  if (!self->have_touchpad)
    return FALSE;

  g_debug ("Should we show the row to enable touchpad: have_mouse: %s have_touchscreen: %s\n",
     self->have_mouse ? "true" : "false",
     self->have_touchscreen ? "true" : "false");

  /* Let's show the button when a mouse, touchscreen or pointing stick is present */
  if (self->have_mouse || self->have_touchscreen || self->have_pointingstick)
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

static gboolean
click_method_get_mapping (GValue    *value,
                          GVariant  *variant,
                          gpointer   user_data)
{
  gboolean is_default;

  is_default = g_strcmp0 (g_variant_get_string (variant, NULL), "fingers") == 0;
  g_value_set_boolean (value, is_default);

  return TRUE;
}

static GVariant *
click_method_set_mapping (const GValue       *value,
                          const GVariantType *type,
                          gpointer            user_data)
{
  gboolean is_default;

  is_default = g_value_get_boolean (value);

  return g_variant_new_string (is_default ? "fingers" : "areas");
}

static void
primary_toggle_right_click_pressed_cb (CcMousePanel *self,
                                       gint          n_press,
                                       double        x,
                                       double        y)
{
  double primary_toggle_group_width;

  primary_toggle_group_width = gtk_widget_get_width (GTK_WIDGET (self->primary_toggle_group));

  if (x < primary_toggle_group_width / 2)
    adw_toggle_group_set_active_name (self->primary_toggle_group, "left");
  else
    adw_toggle_group_set_active_name (self->primary_toggle_group, "right");
}

static gboolean
mouse_accel_get_mapping (GValue    *value,
                         GVariant  *variant,
                         gpointer   user_data)
{
    gboolean enabled;

    enabled = g_strcmp0 (g_variant_get_string (variant, NULL), "flat") != 0;
    g_value_set_boolean (value, enabled);

    return TRUE;
}

static GVariant *
mouse_accel_set_mapping (const GValue       *value,
                         const GVariantType *type,
                         gpointer            user_data)
{
    return g_variant_new_string (g_value_get_boolean (value) ? "default" : "flat");
}

static gboolean
pointingstick_accel_get_mapping (GValue    *value,
                                 GVariant  *variant,
                                 gpointer   user_data)
{
    gboolean enabled;

    enabled = g_strcmp0 (g_variant_get_string (variant, NULL), "flat") != 0;
    g_value_set_boolean (value, enabled);

    return TRUE;
}

static GVariant *
pointingstick_accel_set_mapping (const GValue       *value,
                                 const GVariantType *type,
                                 gpointer            user_data)
{
    return g_variant_new_string (g_value_get_boolean (value) ? "default" : "flat");
}

/* Set up the property editors in the dialog. */
static void
setup_dialog (CcMousePanel *self)
{
  const char *active_toggle_name;
  gboolean left_handed;

  self->mouse_test = GTK_WINDOW (cc_mouse_test_new ());

  left_handed = g_settings_get_boolean (self->mouse_settings, "left-handed");
  active_toggle_name = left_handed ? "right" : "left";
  adw_toggle_group_set_active_name (self->primary_toggle_group, active_toggle_name);

  g_settings_bind (self->mouse_settings, "natural-scroll",
                   self->mouse_scroll_direction_row, "use-default",
                   G_SETTINGS_BIND_INVERT_BOOLEAN);

  /* Mouse section */
  gtk_widget_set_visible (GTK_WIDGET (self->mouse_group), self->have_mouse);

  g_settings_bind (self->mouse_settings, "speed",
                   gtk_range_get_adjustment (GTK_RANGE (self->mouse_speed_scale)), "value",
                   G_SETTINGS_BIND_DEFAULT);

  g_settings_bind_with_mapping (self->mouse_settings, "accel-profile",
                                self->mouse_accel_switch, "active",
                                G_SETTINGS_BIND_DEFAULT,
                                mouse_accel_get_mapping,
                                mouse_accel_set_mapping,
                                NULL, NULL);

  /* Touchpad section */
  gtk_widget_set_visible (GTK_WIDGET (self->touchpad_toggle_row), can_disable_touchpad (self));

  g_settings_bind_with_mapping (self->touchpad_settings, "send-events",
                                self->touchpad_toggle_row, "active",
                                G_SETTINGS_BIND_DEFAULT,
                                touchpad_enabled_get_mapping,
                                touchpad_enabled_set_mapping,
                                NULL, NULL);

  g_settings_bind_with_mapping (self->touchpad_settings, "send-events",
                                self->touchpad_typing_row, "sensitive",
                                G_SETTINGS_BIND_GET,
                                touchpad_enabled_get_mapping,
                                touchpad_enabled_set_mapping,
                                NULL, NULL);

  g_settings_bind (self->touchpad_settings, "natural-scroll",
                   self->touchpad_scroll_direction_row, "use-default",
                   G_SETTINGS_BIND_INVERT_BOOLEAN |
                   G_SETTINGS_BIND_NO_SENSITIVITY);

  g_settings_bind (self->touchpad_settings, "speed",
                   gtk_range_get_adjustment (GTK_RANGE (self->touchpad_speed_scale)), "value",
                   G_SETTINGS_BIND_DEFAULT |
                   G_SETTINGS_BIND_NO_SENSITIVITY);

  g_settings_bind (self->touchpad_settings, "tap-to-click",
                   self->tap_to_click_switch, "active",
                   G_SETTINGS_BIND_DEFAULT |
                   G_SETTINGS_BIND_NO_SENSITIVITY);

  g_settings_bind_with_mapping (self->touchpad_settings, "click-method",
                                self->two_finger_push_row, "use-default",
                                G_SETTINGS_BIND_DEFAULT,
                                click_method_get_mapping,
                                click_method_set_mapping,
                                NULL, NULL);

  g_settings_bind (self->touchpad_settings, "two-finger-scrolling-enabled",
                   self->touchpad_scroll_method_row, "use-default",
                   G_SETTINGS_BIND_DEFAULT |
                   G_SETTINGS_BIND_NO_SENSITIVITY);

  g_settings_bind (self->touchpad_settings, "edge-scrolling-enabled",
                   self->touchpad_scroll_method_row, "use-default",
                   G_SETTINGS_BIND_INVERT_BOOLEAN |
                   G_SETTINGS_BIND_NO_SENSITIVITY);

  g_settings_bind (self->touchpad_settings, "disable-while-typing",
                   self->touchpad_typing_row, "active",
                   G_SETTINGS_BIND_DEFAULT);

  /* Pointing stick section */
  g_settings_bind (self->pointingstick_settings, "speed",
                   gtk_range_get_adjustment (GTK_RANGE (self->pointingstick_speed_scale)), "value",
                   G_SETTINGS_BIND_DEFAULT);

  g_settings_bind_with_mapping (self->pointingstick_settings, "accel-profile",
                                self->pointingstick_accel_switch, "active",
                                G_SETTINGS_BIND_DEFAULT,
                                pointingstick_accel_get_mapping,
                                pointingstick_accel_set_mapping,
                                NULL, NULL);

  setup_title_stack (self);
}

/* Callback issued when a button is clicked on the dialog */
static void
device_changed (CcMousePanel *self)
{
  self->have_touchpad = touchpad_is_present ();
  self->have_pointingstick = pointingstick_is_present ();

  setup_title_stack (self);

  self->have_mouse = mouse_is_present ();
  gtk_widget_set_visible (GTK_WIDGET (self->mouse_group), self->have_mouse);
  gtk_widget_set_visible (GTK_WIDGET (self->touchpad_toggle_row), can_disable_touchpad (self));
}

static void
cc_mouse_panel_direction_changed (GtkWidget        *widget,
                                  GtkTextDirection  previous_direction)
{
  CcMousePanel *self = CC_MOUSE_PANEL (widget);

  gtk_widget_set_direction (GTK_WIDGET (self->primary_toggle_group), GTK_TEXT_DIR_LTR);

  GTK_WIDGET_CLASS (cc_mouse_panel_parent_class)->direction_changed (widget, previous_direction);
}

static void
cc_mouse_panel_dispose (GObject *object)
{
  CcMousePanel *self = CC_MOUSE_PANEL (object);

  g_clear_object (&self->mouse_settings);
  g_clear_object (&self->touchpad_settings);
  g_clear_pointer (&self->mouse_test, gtk_window_destroy);

  G_OBJECT_CLASS (cc_mouse_panel_parent_class)->dispose (object);
}

static const char *
cc_mouse_panel_get_help_uri (CcPanel *panel)
{
  return "help:gnome-help/mouse";
}

static void
test_button_row_activated_cb (CcMousePanel *self)
{
  CcShell *shell = cc_panel_get_shell (CC_PANEL (self));

  gtk_window_set_transient_for (self->mouse_test,
                                GTK_WINDOW (cc_shell_get_toplevel (shell)));
  gtk_window_present (self->mouse_test);
}

static void
cc_mouse_panel_init (CcMousePanel *self)
{
  GsdDeviceManager  *device_manager;

  g_resources_register (cc_mouse_get_resource ());

  gtk_widget_init_template (GTK_WIDGET (self));

  self->mouse_settings = g_settings_new ("org.gnome.desktop.peripherals.mouse");
  self->touchpad_settings = g_settings_new ("org.gnome.desktop.peripherals.touchpad");
  self->pointingstick_settings = g_settings_new ("org.gnome.desktop.peripherals.pointingstick");

  device_manager = gsd_device_manager_get ();
  g_signal_connect_object (device_manager, "device-added",
                           G_CALLBACK (device_changed), self, G_CONNECT_SWAPPED);
  g_signal_connect_object (device_manager, "device-removed",
                           G_CALLBACK (device_changed), self, G_CONNECT_SWAPPED);

  self->have_mouse = mouse_is_present ();
  self->have_touchpad = touchpad_is_present ();
  self->have_touchscreen = touchscreen_is_present ();
  self->have_pointingstick = pointingstick_is_present ();
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
  widget_class->direction_changed = cc_mouse_panel_direction_changed;

  g_type_ensure (CC_TYPE_ILLUSTRATED_ROW);
  g_type_ensure (CC_TYPE_SPLIT_ROW);
  g_type_ensure (CC_TYPE_LIST_ROW_INFO_BUTTON);
  g_type_ensure (CC_TYPE_MOUSE_TEST);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/mouse/cc-mouse-panel.ui");

  gtk_widget_class_bind_template_child (widget_class, CcMousePanel, mouse_accel_switch);
  gtk_widget_class_bind_template_child (widget_class, CcMousePanel, mouse_group);
  gtk_widget_class_bind_template_child (widget_class, CcMousePanel, mouse_scroll_direction_row);
  gtk_widget_class_bind_template_child (widget_class, CcMousePanel, mouse_speed_scale);
  gtk_widget_class_bind_template_child (widget_class, CcMousePanel, pointingstick_accel_switch);
  gtk_widget_class_bind_template_child (widget_class, CcMousePanel, pointingstick_stack_page);
  gtk_widget_class_bind_template_child (widget_class, CcMousePanel, pointingstick_speed_scale);
  gtk_widget_class_bind_template_child (widget_class, CcMousePanel, primary_toggle_group);
  gtk_widget_class_bind_template_child (widget_class, CcMousePanel, title_stack);
  gtk_widget_class_bind_template_child (widget_class, CcMousePanel, tap_to_click_row);
  gtk_widget_class_bind_template_child (widget_class, CcMousePanel, tap_to_click_switch);
  gtk_widget_class_bind_template_child (widget_class, CcMousePanel, touchpad_scroll_direction_row);
  gtk_widget_class_bind_template_child (widget_class, CcMousePanel, touchpad_scroll_method_row);
  gtk_widget_class_bind_template_child (widget_class, CcMousePanel, touchpad_stack_page);
  gtk_widget_class_bind_template_child (widget_class, CcMousePanel, touchpad_speed_scale);
  gtk_widget_class_bind_template_child (widget_class, CcMousePanel, touchpad_toggle_row);
  gtk_widget_class_bind_template_child (widget_class, CcMousePanel, touchpad_typing_row);
  gtk_widget_class_bind_template_child (widget_class, CcMousePanel, two_finger_push_row);

  gtk_widget_class_bind_template_callback (widget_class, on_primary_button_changed_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_touchpad_scroll_method_changed_cb);
  gtk_widget_class_bind_template_callback (widget_class, primary_toggle_right_click_pressed_cb);
  gtk_widget_class_bind_template_callback (widget_class, test_button_row_activated_cb);
}
