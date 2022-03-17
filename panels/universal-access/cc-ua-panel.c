/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*-
 *
 * Copyright (C) 2010 Intel, Inc
 * Copyright (C) 2008 William Jon McCann <jmccann@redhat.com>
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
 *
 */

#include <config.h>

#include <math.h>
#include <glib/gi18n-lib.h>
#include <gdesktop-enums.h>

#include "cc-ua-panel.h"
#include "cc-ua-resources.h"
#include "cc-cursor-blinking-dialog.h"
#include "cc-cursor-size-dialog.h"
#include "cc-pointing-dialog.h"
#include "cc-repeat-keys-dialog.h"
#include "cc-typing-dialog.h"
#include "cc-visual-alerts-dialog.h"
#include "cc-zoom-options-dialog.h"

#define DPI_FACTOR_LARGE 1.25
#define DPI_FACTOR_NORMAL 1.0
#define HIGH_CONTRAST_THEME     "HighContrast"

/* shell settings */
#define A11Y_SETTINGS                "org.gnome.desktop.a11y"
#define KEY_ALWAYS_SHOW_STATUS       "always-show-universal-access-status"

/* a11y interface settings */
#define A11Y_INTERFACE_SETTINGS      "org.gnome.desktop.a11y.interface"
#define KEY_HIGH_CONTRAST            "high-contrast"

/* interface settings */
#define INTERFACE_SETTINGS           "org.gnome.desktop.interface"
#define KEY_TEXT_SCALING_FACTOR      "text-scaling-factor"
#define KEY_GTK_THEME                "gtk-theme"
#define KEY_ICON_THEME               "icon-theme"
#define KEY_CURSOR_BLINKING          "cursor-blink"
#define KEY_CURSOR_BLINKING_TIME     "cursor-blink-time"
#define KEY_MOUSE_CURSOR_SIZE        "cursor-size"
#define KEY_LOCATE_POINTER           "locate-pointer"
#define KEY_ENABLE_ANIMATIONS        "enable-animations"

/* application settings */
#define APPLICATION_SETTINGS         "org.gnome.desktop.a11y.applications"
#define KEY_SCREEN_KEYBOARD_ENABLED  "screen-keyboard-enabled"
#define KEY_SCREEN_MAGNIFIER_ENABLED "screen-magnifier-enabled"
#define KEY_SCREEN_READER_ENABLED    "screen-reader-enabled"

/* wm settings */
#define WM_SETTINGS                  "org.gnome.desktop.wm.preferences"
#define KEY_VISUAL_BELL_ENABLED      "visual-bell"

/* keyboard settings */
#define KEYBOARD_SETTINGS            "org.gnome.desktop.a11y.keyboard"
#define KEY_KEYBOARD_TOGGLE          "enable"
#define KEY_STICKYKEYS_ENABLED       "stickykeys-enable"
#define KEY_STICKYKEYS_TWO_KEY_OFF   "stickykeys-two-key-off"
#define KEY_STICKYKEYS_MODIFIER_BEEP "stickykeys-modifier-beep"
#define KEY_SLOWKEYS_ENABLED         "slowkeys-enable"
#define KEY_SLOWKEYS_DELAY           "slowkeys-delay"
#define KEY_SLOWKEYS_BEEP_PRESS      "slowkeys-beep-press"
#define KEY_SLOWKEYS_BEEP_ACCEPT     "slowkeys-beep-accept"
#define KEY_SLOWKEYS_BEEP_REJECT     "slowkeys-beep-reject"
#define KEY_BOUNCEKEYS_ENABLED       "bouncekeys-enable"
#define KEY_BOUNCEKEYS_DELAY         "bouncekeys-delay"
#define KEY_BOUNCEKEYS_BEEP_REJECT   "bouncekeys-beep-reject"
#define KEY_MOUSEKEYS_ENABLED        "mousekeys-enable"
#define KEY_TOGGLEKEYS_ENABLED       "togglekeys-enable"

/* keyboard desktop settings */
#define KEYBOARD_DESKTOP_SETTINGS    "org.gnome.desktop.peripherals.keyboard"
#define KEY_REPEAT_KEYS              "repeat"

/* mouse settings */
#define MOUSE_SETTINGS               "org.gnome.desktop.a11y.mouse"
#define KEY_SECONDARY_CLICK_ENABLED  "secondary-click-enabled"
#define KEY_SECONDARY_CLICK_TIME     "secondary-click-time"
#define KEY_DWELL_CLICK_ENABLED      "dwell-click-enabled"
#define KEY_DWELL_TIME               "dwell-time"
#define KEY_DWELL_THRESHOLD          "dwell-threshold"

#define MOUSE_PERIPHERAL_SETTINGS    "org.gnome.desktop.peripherals.mouse"
#define KEY_DOUBLE_CLICK_DELAY       "double-click"

struct _CcUaPanel
{
  CcPanel    parent_instance;

  GtkLabel          *accessx_label;
  AdwActionRow      *accessx_row;
  GtkLabel          *click_assist_label;
  AdwActionRow      *click_assist_row;
  GtkLabel          *cursor_blinking_label;
  AdwActionRow      *cursor_blinking_row;
  GtkLabel          *cursor_size_label;
  AdwActionRow      *cursor_size_row;
  GtkScale          *double_click_delay_scale;
  GtkSwitch         *enable_animations_switch;
  GtkSwitch         *highcontrast_enable_switch;
  GtkSwitch         *large_text_enable_switch;
  GtkSwitch         *locate_pointer_enable_switch;
  GtkSwitch         *mouse_keys_enable_switch;
  GtkLabel          *repeat_keys_label;
  AdwActionRow      *repeat_keys_row;
  GtkSwitch         *screen_keyboard_enable_switch;
  GtkSwitch         *screen_reader_switch;
  AdwActionRow      *screen_reader_row;
  GtkSwitch         *show_status_switch;
  GtkSwitch         *sound_keys_switch;
  AdwActionRow      *sound_keys_row;
  GtkLabel          *visual_alerts_label;
  AdwActionRow      *visual_alerts_row;
  GtkLabel          *zoom_label;
  AdwActionRow     *zoom_row;

  GSettings *wm_settings;
  GSettings *a11y_settings;
  GSettings *a11y_interface_settings;
  GSettings *interface_settings;
  GSettings *kb_settings;
  GSettings *mouse_settings;
  GSettings *kb_desktop_settings;
  GSettings *application_settings;
  GSettings *gds_mouse_settings;
};

CC_PANEL_REGISTER (CcUaPanel, cc_ua_panel)

static void
run_dialog (CcUaPanel *self, GtkDialog *dialog)
{
  GtkNative *native = gtk_widget_get_native (GTK_WIDGET (self));

  gtk_window_set_transient_for (GTK_WINDOW (dialog), GTK_WINDOW (native));
  gtk_window_present (GTK_WINDOW (dialog));
}

static void
activate_row (CcUaPanel *self, AdwActionRow *row)
{
  if (row == self->zoom_row)
    {
      run_dialog (self, GTK_DIALOG (cc_zoom_options_dialog_new ()));
    }
  else if (row == self->cursor_size_row)
    {
      run_dialog (self, GTK_DIALOG (cc_cursor_size_dialog_new ()));
    }
  else if (row == self->visual_alerts_row)
    {
      run_dialog (self, GTK_DIALOG (cc_visual_alerts_dialog_new ()));
    }
  else if (row == self->repeat_keys_row)
    {
      run_dialog (self, GTK_DIALOG (cc_repeat_keys_dialog_new ()));
    }
  else if (row == self->cursor_blinking_row)
    {
      run_dialog (self, GTK_DIALOG (cc_cursor_blinking_dialog_new ()));
    }
  else if (row == self->accessx_row)
    {
      run_dialog (self, GTK_DIALOG (cc_typing_dialog_new ()));
    }
  else if (row == self->click_assist_row)
    {
      run_dialog (self, GTK_DIALOG (cc_pointing_dialog_new ()));
    }
}

static void
cc_ua_panel_dispose (GObject *object)
{
  CcUaPanel *self = CC_UA_PANEL (object);

  g_clear_object (&self->wm_settings);
  g_clear_object (&self->a11y_settings);
  g_clear_object (&self->a11y_interface_settings);
  g_clear_object (&self->interface_settings);
  g_clear_object (&self->kb_settings);
  g_clear_object (&self->mouse_settings);
  g_clear_object (&self->kb_desktop_settings);
  g_clear_object (&self->application_settings);
  g_clear_object (&self->gds_mouse_settings);

  G_OBJECT_CLASS (cc_ua_panel_parent_class)->dispose (object);
}

static const char *
cc_ua_panel_get_help_uri (CcPanel *panel)
{
  return "help:gnome-help/a11y";
}

static void
cc_ua_panel_class_init (CcUaPanelClass *klass)
{ 
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  CcPanelClass *panel_class = CC_PANEL_CLASS (klass);

  panel_class->get_help_uri = cc_ua_panel_get_help_uri;

  object_class->dispose = cc_ua_panel_dispose;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/universal-access/cc-ua-panel.ui");

  gtk_widget_class_bind_template_child (widget_class, CcUaPanel, accessx_label);
  gtk_widget_class_bind_template_child (widget_class, CcUaPanel, accessx_row);
  gtk_widget_class_bind_template_child (widget_class, CcUaPanel, click_assist_label);
  gtk_widget_class_bind_template_child (widget_class, CcUaPanel, click_assist_row);
  gtk_widget_class_bind_template_child (widget_class, CcUaPanel, cursor_blinking_label);
  gtk_widget_class_bind_template_child (widget_class, CcUaPanel, cursor_blinking_row);
  gtk_widget_class_bind_template_child (widget_class, CcUaPanel, cursor_size_label);
  gtk_widget_class_bind_template_child (widget_class, CcUaPanel, cursor_size_row);
  gtk_widget_class_bind_template_child (widget_class, CcUaPanel, double_click_delay_scale);
  gtk_widget_class_bind_template_child (widget_class, CcUaPanel, enable_animations_switch);
  gtk_widget_class_bind_template_child (widget_class, CcUaPanel, highcontrast_enable_switch);
  gtk_widget_class_bind_template_child (widget_class, CcUaPanel, large_text_enable_switch);
  gtk_widget_class_bind_template_child (widget_class, CcUaPanel, locate_pointer_enable_switch);
  gtk_widget_class_bind_template_child (widget_class, CcUaPanel, mouse_keys_enable_switch);
  gtk_widget_class_bind_template_child (widget_class, CcUaPanel, repeat_keys_label);
  gtk_widget_class_bind_template_child (widget_class, CcUaPanel, repeat_keys_row);
  gtk_widget_class_bind_template_child (widget_class, CcUaPanel, screen_keyboard_enable_switch);
  gtk_widget_class_bind_template_child (widget_class, CcUaPanel, screen_reader_switch);
  gtk_widget_class_bind_template_child (widget_class, CcUaPanel, screen_reader_row);
  gtk_widget_class_bind_template_child (widget_class, CcUaPanel, show_status_switch);
  gtk_widget_class_bind_template_child (widget_class, CcUaPanel, sound_keys_switch);
  gtk_widget_class_bind_template_child (widget_class, CcUaPanel, sound_keys_row);
  gtk_widget_class_bind_template_child (widget_class, CcUaPanel, visual_alerts_label);
  gtk_widget_class_bind_template_child (widget_class, CcUaPanel, visual_alerts_row);
  gtk_widget_class_bind_template_child (widget_class, CcUaPanel, zoom_label);
  gtk_widget_class_bind_template_child (widget_class, CcUaPanel, zoom_row);
  gtk_widget_class_bind_template_callback (widget_class, activate_row);
}

/* seeing section */

static gboolean
is_large_factor (gdouble factor)
{
  return (factor > DPI_FACTOR_NORMAL);
}

static gboolean
get_large_text_mapping (GValue   *value,
                        GVariant *variant,
                        gpointer  user_data)
{
  gdouble factor;

  factor = g_variant_get_double (variant);
  g_value_set_boolean (value, is_large_factor (factor));

  return TRUE;
}

static GVariant *
set_large_text_mapping (const GValue       *value,
                        const GVariantType *expected_type,
                        gpointer            user_data)
{
  gboolean large;
  GSettings *settings = user_data;
  GVariant *ret = NULL;

  large = g_value_get_boolean (value);
  if (large)
    ret = g_variant_new_double (DPI_FACTOR_LARGE);
  else
    g_settings_reset (settings, KEY_TEXT_SCALING_FACTOR);

  return ret;
}

static gboolean
get_contrast_mapping (GValue   *value,
                      GVariant *variant,
                      gpointer  user_data)
{
  gboolean hc;

  hc = g_variant_get_boolean (variant);
  g_value_set_boolean (value, hc);

  return TRUE;
}

static GVariant *
set_contrast_mapping (const GValue       *value,
                      const GVariantType *expected_type,
                      gpointer            user_data)
{
  gboolean hc;
  CcUaPanel *self = user_data;

  hc = g_value_get_boolean (value);
  if (hc)
    {
      g_settings_set_string (self->interface_settings, KEY_GTK_THEME, HIGH_CONTRAST_THEME);
    }
  else
    {
      g_settings_reset (self->interface_settings, KEY_GTK_THEME);
      g_settings_reset (self->interface_settings, KEY_ICON_THEME);
    }

  return g_variant_new_boolean (hc);
}

static gboolean
on_off_label_mapping_get (GValue   *value,
                          GVariant *variant,
                          gpointer  user_data)
{
  g_value_set_string (value, g_variant_get_boolean (variant) ? _("On") : _("Off"));

  return TRUE;
}

static gboolean
cursor_size_label_mapping_get (GValue   *value,
                               GVariant *variant,
                               gpointer  user_data)
{
  char *label;
  int cursor_size;

  cursor_size = g_variant_get_int32 (variant);

  switch (cursor_size)
    {
      case 24:
        /* translators: the labels will read:
         * Cursor Size: Default */
        label = g_strdup (C_("cursor size", "Default"));
        break;
      case 32:
        label = g_strdup (C_("cursor size", "Medium"));
        break;
      case 48:
        label = g_strdup (C_("cursor size", "Large"));
        break;
      case 64:
        label = g_strdup (C_("cursor size", "Larger"));
        break;
      case 96:
        label = g_strdup (C_("cursor size", "Largest"));
        break;
      default:
        label = g_strdup_printf (g_dngettext (GETTEXT_PACKAGE,
                                              "%d pixel",
                                              "%d pixels",
                                              cursor_size),
                                 cursor_size);
        break;
    }

  g_value_take_string (value, label);

  return TRUE;
}

static void
cc_ua_panel_init_status (CcUaPanel *self)
{
  g_settings_bind (self->a11y_settings, KEY_ALWAYS_SHOW_STATUS,
                   self->show_status_switch, "active",
                   G_SETTINGS_BIND_DEFAULT);
}

static void
cc_ua_panel_init_seeing (CcUaPanel *self)
{
  g_settings_bind_with_mapping (self->a11y_interface_settings, KEY_HIGH_CONTRAST,
                                self->highcontrast_enable_switch,
                                "active", G_SETTINGS_BIND_DEFAULT,
                                get_contrast_mapping,
                                set_contrast_mapping,
                                self,
                                NULL);

  /* enable animation */
  g_settings_bind (self->interface_settings, KEY_ENABLE_ANIMATIONS,
                   self->enable_animations_switch, "active",
                   G_SETTINGS_BIND_DEFAULT);


  /* large text */

  g_settings_bind_with_mapping (self->interface_settings, KEY_TEXT_SCALING_FACTOR,
                                self->large_text_enable_switch,
                                "active", G_SETTINGS_BIND_DEFAULT,
                                get_large_text_mapping,
                                set_large_text_mapping,
                                self->interface_settings,
                                NULL);

  /* screen reader */

  g_settings_bind (self->application_settings, "screen-reader-enabled",
                   self->screen_reader_switch, "active",
                   G_SETTINGS_BIND_DEFAULT);

  /* sound keys */

  g_settings_bind (self->kb_settings, KEY_TOGGLEKEYS_ENABLED,
                   self->sound_keys_switch, "active",
                   G_SETTINGS_BIND_DEFAULT);

  /* cursor size */

  g_settings_bind_with_mapping (self->interface_settings, KEY_MOUSE_CURSOR_SIZE, // FIXME
                                self->cursor_size_label,
                                "label", G_SETTINGS_BIND_GET,
                                cursor_size_label_mapping_get,
                                NULL, NULL, NULL);

  /* zoom */

  g_settings_bind_with_mapping (self->application_settings, "screen-magnifier-enabled",
                                self->zoom_label,
                                "label", G_SETTINGS_BIND_GET,
                                on_off_label_mapping_get,
                                NULL, NULL, NULL);
}

/* hearing/sound section */

static void
cc_ua_panel_init_hearing (CcUaPanel *self)
{
  g_settings_bind_with_mapping (self->wm_settings, KEY_VISUAL_BELL_ENABLED,
                                self->visual_alerts_label,
                                "label", G_SETTINGS_BIND_GET,
                                on_off_label_mapping_get,
                                NULL, NULL, NULL);
}

/* typing/keyboard section */
static void
on_repeat_keys_toggled (CcUaPanel *self)
{
  gboolean on;

  on = g_settings_get_boolean (self->kb_desktop_settings, KEY_REPEAT_KEYS);

  gtk_label_set_text (self->repeat_keys_label, on ? _("On") : _("Off"));
}

static void
on_cursor_blinking_toggled (CcUaPanel *self)
{
  gboolean on;

  on = g_settings_get_boolean (self->interface_settings, KEY_CURSOR_BLINKING);

  gtk_label_set_text (self->cursor_blinking_label, on ? _("On") : _("Off"));
}

static void
update_accessx_label (CcUaPanel *self)
{
  gboolean on;

  on = g_settings_get_boolean (self->kb_settings, KEY_STICKYKEYS_ENABLED) ||
       g_settings_get_boolean (self->kb_settings, KEY_SLOWKEYS_ENABLED) ||
       g_settings_get_boolean (self->kb_settings, KEY_BOUNCEKEYS_ENABLED);

  gtk_label_set_text (self->accessx_label, on ? _("On") : _("Off"));
}

static void
cc_ua_panel_init_keyboard (CcUaPanel *self)
{
  /* on-screen keyboard */
  g_settings_bind (self->application_settings, KEY_SCREEN_KEYBOARD_ENABLED,
                   self->screen_keyboard_enable_switch, "active",
                   G_SETTINGS_BIND_DEFAULT);

  /* Repeat keys */
  g_signal_connect_object (self->kb_desktop_settings, "changed",
                           G_CALLBACK (on_repeat_keys_toggled), self, G_CONNECT_SWAPPED);
  on_repeat_keys_toggled (self);

  /* Cursor Blinking */
  g_signal_connect_object (self->interface_settings, "changed",
                           G_CALLBACK (on_cursor_blinking_toggled), self, G_CONNECT_SWAPPED);
  on_cursor_blinking_toggled (self);

  /* accessx */
  g_signal_connect_object (self->kb_settings, "changed",
                           G_CALLBACK (update_accessx_label), self, G_CONNECT_SWAPPED);
  update_accessx_label (self);
}

/* mouse/pointing & clicking section */
static void
update_click_assist_label (CcUaPanel *self)
{
  gboolean on;

  on = g_settings_get_boolean (self->mouse_settings, KEY_SECONDARY_CLICK_ENABLED) ||
       g_settings_get_boolean (self->mouse_settings, KEY_DWELL_CLICK_ENABLED);

  gtk_label_set_text (self->click_assist_label, on ? _("On") : _("Off"));
}


static void
cc_ua_panel_init_mouse (CcUaPanel *self)
{
  g_settings_bind (self->kb_settings, KEY_MOUSEKEYS_ENABLED,
                   self->mouse_keys_enable_switch, "active",
                   G_SETTINGS_BIND_DEFAULT);

  g_settings_bind (self->interface_settings, KEY_LOCATE_POINTER,
                   self->locate_pointer_enable_switch, "active",
                   G_SETTINGS_BIND_DEFAULT);

  g_signal_connect_object (self->mouse_settings, "changed",
                           G_CALLBACK (update_click_assist_label), self, G_CONNECT_SWAPPED);
  update_click_assist_label (self);

  g_settings_bind (self->gds_mouse_settings, "double-click",
                   gtk_range_get_adjustment (GTK_RANGE (self->double_click_delay_scale)), "value",
                   G_SETTINGS_BIND_DEFAULT);

  gtk_scale_add_mark (GTK_SCALE (self->double_click_delay_scale), 400, GTK_POS_BOTTOM, NULL);
}

static void
cc_ua_panel_init (CcUaPanel *self)
{
  g_resources_register (cc_universal_access_get_resource ());

  gtk_widget_init_template (GTK_WIDGET (self));

  self->interface_settings = g_settings_new (INTERFACE_SETTINGS);
  self->a11y_interface_settings = g_settings_new (A11Y_INTERFACE_SETTINGS);
  self->a11y_settings = g_settings_new (A11Y_SETTINGS);
  self->wm_settings = g_settings_new (WM_SETTINGS);
  self->kb_settings = g_settings_new (KEYBOARD_SETTINGS);
  self->kb_desktop_settings = g_settings_new (KEYBOARD_DESKTOP_SETTINGS);
  self->mouse_settings = g_settings_new (MOUSE_SETTINGS);
  self->gds_mouse_settings = g_settings_new (MOUSE_PERIPHERAL_SETTINGS);
  self->application_settings = g_settings_new (APPLICATION_SETTINGS);

  cc_ua_panel_init_status (self);
  cc_ua_panel_init_seeing (self);
  cc_ua_panel_init_hearing (self);
  cc_ua_panel_init_keyboard (self);
  cc_ua_panel_init_mouse (self);
}
