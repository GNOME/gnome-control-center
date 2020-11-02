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

#include "list-box-helper.h"
#include "cc-ua-panel.h"
#include "cc-ua-resources.h"
#include "cc-cursor-size-dialog.h"
#include "cc-repeat-keys-dialog.h"
#include "cc-sound-keys-dialog.h"
#include "cc-screen-reader-dialog.h"
#include "cc-visual-alerts-dialog.h"
#include "cc-zoom-options-dialog.h"

#define DPI_FACTOR_LARGE 1.25
#define DPI_FACTOR_NORMAL 1.0
#define HIGH_CONTRAST_THEME     "HighContrast"

/* shell settings */
#define A11Y_SETTINGS               "org.gnome.desktop.a11y"
#define KEY_ALWAYS_SHOW_STATUS       "always-show-universal-access-status"

/* interface settings */
#define INTERFACE_SETTINGS           "org.gnome.desktop.interface"
#define KEY_TEXT_SCALING_FACTOR      "text-scaling-factor"
#define KEY_GTK_THEME                "gtk-theme"
#define KEY_ICON_THEME               "icon-theme"
#define KEY_CURSOR_BLINKING          "cursor-blink"
#define KEY_CURSOR_BLINKING_TIME     "cursor-blink-time"
#define KEY_MOUSE_CURSOR_SIZE        "cursor-size"
#define KEY_LOCATE_POINTER           "locate-pointer"

/* application settings */
#define APPLICATION_SETTINGS         "org.gnome.desktop.a11y.applications"
#define KEY_SCREEN_KEYBOARD_ENABLED  "screen-keyboard-enabled"
#define KEY_SCREEN_MAGNIFIER_ENABLED "screen-magnifier-enabled"
#define KEY_SCREEN_READER_ENABLED    "screen-reader-enabled"

/* wm settings */
#define WM_SETTINGS                  "org.gnome.desktop.wm.preferences"
#define KEY_VISUAL_BELL_ENABLED      "visual-bell"
#define KEY_WM_THEME                 "theme"

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

/* gnome-settings-daemon settings */
#define GSD_MOUSE_SETTINGS           "org.gnome.settings-daemon.peripherals.mouse"
#define KEY_DOUBLE_CLICK_DELAY       "double-click"

#define SCROLL_HEIGHT 490

struct _CcUaPanel
{
  CcPanel    parent_instance;

  GtkDialog *cursor_blinking_dialog;
  GtkWidget *cursor_blinking_scale;
  GtkWidget *cursor_blinking_switch;
  GtkWidget *list_hearing;
  GtkWidget *list_pointing;
  GtkWidget *list_seeing;
  GtkWidget *list_typing;
  GtkWidget *mouse_keys_switch;
  GtkWidget *locate_pointer_switch;
  GtkDialog *pointing_dialog;
  GtkWidget *pointing_dwell_delay_box;
  GtkWidget *pointing_dwell_delay_scale;
  GtkWidget *pointing_dwell_threshold_box;
  GtkWidget *pointing_dwell_threshold_scale;
  GtkWidget *pointing_hover_click_switch;
  GtkWidget *pointing_secondary_click_delay_box;
  GtkWidget *pointing_secondary_click_delay_scale;
  GtkWidget *pointing_secondary_click_switch;
  GtkListBoxRow *row_accessx;
  GtkListBoxRow *row_click_assist;
  GtkListBoxRow *row_cursor_blinking;
  GtkListBoxRow *row_cursor_size;
  GtkListBoxRow *row_highcontrast;
  GtkListBoxRow *row_large_text;
  GtkListBoxRow *row_mouse_keys;
  GtkListBoxRow *row_repeat_keys;
  GtkListBoxRow *row_screen_keyboard;
  GtkListBoxRow *row_screen_reader;
  GtkListBoxRow *row_sound_keys;
  GtkListBoxRow *row_visual_alerts;
  GtkListBoxRow *row_zoom;
  GtkWidget *scale_double_click_delay;
  GtkWidget *screen_keyboard_switch;
  GtkWidget *section_status;
  GtkWidget *switch_status;
  GtkWidget *typing_bouncekeys_beep_rejected_check;
  GtkWidget *typing_bouncekeys_delay_box;
  GtkWidget *typing_bouncekeys_delay_scale;
  GtkWidget *typing_bouncekeys_switch;
  GtkDialog *typing_dialog;
  GtkWidget *typing_keyboard_toggle_switch;
  GtkWidget *typing_slowkeys_beep_accepted_check;
  GtkWidget *typing_slowkeys_beep_pressed_check;
  GtkWidget *typing_slowkeys_beep_rejected_check;
  GtkWidget *typing_slowkeys_delay_box;
  GtkWidget *typing_slowkeys_delay_scale;
  GtkWidget *typing_slowkeys_switch;
  GtkWidget *typing_stickykeys_beep_modifier_check;
  GtkWidget *typing_stickykeys_disable_two_keys_check;
  GtkWidget *typing_stickykeys_switch;
  GtkWidget *universal_access_content;
  GtkWidget *universal_access_panel;
  GtkWidget *value_accessx;
  GtkWidget *value_click_assist;
  GtkWidget *value_cursor_size;
  GtkWidget *value_highcontrast;
  GtkWidget *value_large_text;
  GtkWidget *value_repeat_keys;
  GtkWidget *value_row_cursor_blinking;
  GtkWidget *value_screen_reader;
  GtkWidget *value_sound_keys;
  GtkWidget *value_visual_alerts;
  GtkWidget *value_zoom;

  GSettings *wm_settings;
  GSettings *a11y_settings;
  GSettings *interface_settings;
  GSettings *kb_settings;
  GSettings *mouse_settings;
  GSettings *kb_desktop_settings;
  GSettings *application_settings;
  GSettings *gsd_mouse_settings;

  GtkAdjustment *focus_adjustment;

  GList *sections;
  GList *sections_reverse;

  GSList *toplevels;
};

CC_PANEL_REGISTER (CcUaPanel, cc_ua_panel)

static void
cc_ua_panel_dispose (GObject *object)
{
  CcUaPanel *self = CC_UA_PANEL (object);

  g_slist_free_full (self->toplevels, (GDestroyNotify)gtk_widget_destroy);
  self->toplevels = NULL;

  g_clear_object (&self->wm_settings);
  g_clear_object (&self->a11y_settings);
  g_clear_object (&self->interface_settings);
  g_clear_object (&self->kb_settings);
  g_clear_object (&self->mouse_settings);
  g_clear_object (&self->kb_desktop_settings);
  g_clear_object (&self->application_settings);
  g_clear_object (&self->gsd_mouse_settings);

  g_clear_pointer (&self->sections, g_list_free);
  g_clear_pointer (&self->sections_reverse, g_list_free);

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

  gtk_widget_class_bind_template_child (widget_class, CcUaPanel, cursor_blinking_dialog);
  gtk_widget_class_bind_template_child (widget_class, CcUaPanel, cursor_blinking_scale);
  gtk_widget_class_bind_template_child (widget_class, CcUaPanel, cursor_blinking_switch);
  gtk_widget_class_bind_template_child (widget_class, CcUaPanel, list_hearing);
  gtk_widget_class_bind_template_child (widget_class, CcUaPanel, list_pointing);
  gtk_widget_class_bind_template_child (widget_class, CcUaPanel, list_seeing);
  gtk_widget_class_bind_template_child (widget_class, CcUaPanel, list_typing);
  gtk_widget_class_bind_template_child (widget_class, CcUaPanel, mouse_keys_switch);
  gtk_widget_class_bind_template_child (widget_class, CcUaPanel, locate_pointer_switch);
  gtk_widget_class_bind_template_child (widget_class, CcUaPanel, pointing_dialog);
  gtk_widget_class_bind_template_child (widget_class, CcUaPanel, pointing_dwell_delay_box);
  gtk_widget_class_bind_template_child (widget_class, CcUaPanel, pointing_dwell_delay_scale);
  gtk_widget_class_bind_template_child (widget_class, CcUaPanel, pointing_dwell_threshold_box);
  gtk_widget_class_bind_template_child (widget_class, CcUaPanel, pointing_dwell_threshold_scale);
  gtk_widget_class_bind_template_child (widget_class, CcUaPanel, pointing_hover_click_switch);
  gtk_widget_class_bind_template_child (widget_class, CcUaPanel, pointing_secondary_click_delay_box);
  gtk_widget_class_bind_template_child (widget_class, CcUaPanel, pointing_secondary_click_delay_scale);
  gtk_widget_class_bind_template_child (widget_class, CcUaPanel, pointing_secondary_click_switch);
  gtk_widget_class_bind_template_child (widget_class, CcUaPanel, row_accessx);
  gtk_widget_class_bind_template_child (widget_class, CcUaPanel, row_click_assist);
  gtk_widget_class_bind_template_child (widget_class, CcUaPanel, row_cursor_blinking);
  gtk_widget_class_bind_template_child (widget_class, CcUaPanel, row_cursor_size);
  gtk_widget_class_bind_template_child (widget_class, CcUaPanel, row_repeat_keys);
  gtk_widget_class_bind_template_child (widget_class, CcUaPanel, row_screen_reader);
  gtk_widget_class_bind_template_child (widget_class, CcUaPanel, row_sound_keys);
  gtk_widget_class_bind_template_child (widget_class, CcUaPanel, row_visual_alerts);
  gtk_widget_class_bind_template_child (widget_class, CcUaPanel, row_zoom);
  gtk_widget_class_bind_template_child (widget_class, CcUaPanel, scale_double_click_delay);
  gtk_widget_class_bind_template_child (widget_class, CcUaPanel, screen_keyboard_switch);
  gtk_widget_class_bind_template_child (widget_class, CcUaPanel, section_status);
  gtk_widget_class_bind_template_child (widget_class, CcUaPanel, switch_status);
  gtk_widget_class_bind_template_child (widget_class, CcUaPanel, typing_bouncekeys_beep_rejected_check);
  gtk_widget_class_bind_template_child (widget_class, CcUaPanel, typing_bouncekeys_delay_box);
  gtk_widget_class_bind_template_child (widget_class, CcUaPanel, typing_bouncekeys_delay_scale);
  gtk_widget_class_bind_template_child (widget_class, CcUaPanel, typing_bouncekeys_switch);
  gtk_widget_class_bind_template_child (widget_class, CcUaPanel, typing_dialog);
  gtk_widget_class_bind_template_child (widget_class, CcUaPanel, typing_keyboard_toggle_switch);
  gtk_widget_class_bind_template_child (widget_class, CcUaPanel, typing_slowkeys_beep_accepted_check);
  gtk_widget_class_bind_template_child (widget_class, CcUaPanel, typing_slowkeys_beep_pressed_check);
  gtk_widget_class_bind_template_child (widget_class, CcUaPanel, typing_slowkeys_beep_rejected_check);
  gtk_widget_class_bind_template_child (widget_class, CcUaPanel, typing_slowkeys_delay_box);
  gtk_widget_class_bind_template_child (widget_class, CcUaPanel, typing_slowkeys_delay_scale);
  gtk_widget_class_bind_template_child (widget_class, CcUaPanel, typing_slowkeys_switch);
  gtk_widget_class_bind_template_child (widget_class, CcUaPanel, typing_stickykeys_beep_modifier_check);
  gtk_widget_class_bind_template_child (widget_class, CcUaPanel, typing_stickykeys_disable_two_keys_check);
  gtk_widget_class_bind_template_child (widget_class, CcUaPanel, typing_stickykeys_switch);
  gtk_widget_class_bind_template_child (widget_class, CcUaPanel, universal_access_content);
  gtk_widget_class_bind_template_child (widget_class, CcUaPanel, universal_access_panel);
  gtk_widget_class_bind_template_child (widget_class, CcUaPanel, value_accessx);
  gtk_widget_class_bind_template_child (widget_class, CcUaPanel, value_click_assist);
  gtk_widget_class_bind_template_child (widget_class, CcUaPanel, value_cursor_size);
  gtk_widget_class_bind_template_child (widget_class, CcUaPanel, value_highcontrast);
  gtk_widget_class_bind_template_child (widget_class, CcUaPanel, value_large_text);
  gtk_widget_class_bind_template_child (widget_class, CcUaPanel, value_repeat_keys);
  gtk_widget_class_bind_template_child (widget_class, CcUaPanel, value_row_cursor_blinking);
  gtk_widget_class_bind_template_child (widget_class, CcUaPanel, value_screen_reader);
  gtk_widget_class_bind_template_child (widget_class, CcUaPanel, value_sound_keys);
  gtk_widget_class_bind_template_child (widget_class, CcUaPanel, value_visual_alerts);
  gtk_widget_class_bind_template_child (widget_class, CcUaPanel, value_zoom);
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
  const char *theme;
  gboolean hc;

  theme = g_variant_get_string (variant, NULL);
  hc = (g_strcmp0 (theme, HIGH_CONTRAST_THEME) == 0);
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
  GVariant *ret = NULL;

  hc = g_value_get_boolean (value);
  if (hc)
    {
      ret = g_variant_new_string (HIGH_CONTRAST_THEME);
      g_settings_set_string (self->interface_settings, KEY_ICON_THEME, HIGH_CONTRAST_THEME);

      g_settings_set_string (self->wm_settings, KEY_WM_THEME, HIGH_CONTRAST_THEME);
    }
  else
    {
      g_settings_reset (self->interface_settings, KEY_GTK_THEME);
      g_settings_reset (self->interface_settings, KEY_ICON_THEME);

      g_settings_reset (self->wm_settings, KEY_WM_THEME);
    }

  return ret;
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
add_separators (GtkListBox *list)
{
  gtk_list_box_set_header_func (list, cc_list_box_update_header_func, NULL, NULL);
}

static gboolean
keynav_failed (CcUaPanel *self, GtkDirectionType direction, GtkWidget *list)
{
  GList *item, *sections;
  gdouble value, lower, upper, page;

  /* Find the list in the list of GtkListBoxes */
  if (direction == GTK_DIR_DOWN)
    sections = self->sections;
  else
    sections = self->sections_reverse;

  item = g_list_find (sections, list);
  g_assert (item);
  if (item->next)
    {
      gtk_widget_child_focus (GTK_WIDGET (item->next->data), direction);
      return TRUE;
    }

  value = gtk_adjustment_get_value (self->focus_adjustment);
  lower = gtk_adjustment_get_lower (self->focus_adjustment);
  upper = gtk_adjustment_get_upper (self->focus_adjustment);
  page  = gtk_adjustment_get_page_size (self->focus_adjustment);

  if (direction == GTK_DIR_UP && value > lower)
    {
      gtk_adjustment_set_value (self->focus_adjustment, lower);
      return TRUE;
    }
  else if (direction == GTK_DIR_DOWN && value < upper - page)
    {
      gtk_adjustment_set_value (self->focus_adjustment, upper - page);
      return TRUE;
    }

  return FALSE;
}

static void
add_section (GtkWidget *list, CcUaPanel *self)
{
  g_signal_connect_object (list, "keynav-failed", G_CALLBACK (keynav_failed), self, G_CONNECT_SWAPPED);

  self->sections = g_list_append (self->sections, list);
  self->sections_reverse = g_list_prepend (self->sections_reverse, list);
}

static void
cc_ua_panel_init_status (CcUaPanel *self)
{
  GtkWidget *box;

  box = GTK_WIDGET (self->section_status);
  self->sections_reverse = g_list_prepend (self->sections_reverse, box);

  g_settings_bind (self->a11y_settings, KEY_ALWAYS_SHOW_STATUS,
                   self->switch_status, "active",
                   G_SETTINGS_BIND_DEFAULT);
}

static void
toggle_switch (GtkWidget *sw)
{
  gboolean active;

  active = gtk_switch_get_active (GTK_SWITCH (sw));
  gtk_switch_set_active (GTK_SWITCH (sw), !active);
}

static void
run_dialog (CcUaPanel *self, GtkDialog *dialog)
{
  gtk_window_set_transient_for (GTK_WINDOW (dialog),
                                GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (self))));
  gtk_dialog_run (GTK_DIALOG (dialog));
  gtk_widget_destroy (GTK_WIDGET (dialog));
}

static void
show_dialog (CcUaPanel *self, GtkDialog *dialog)
{
  gtk_window_set_transient_for (GTK_WINDOW (dialog),
                                GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (self))));
  gtk_window_present (GTK_WINDOW (dialog));
}

static void
activate_row (CcUaPanel *self, GtkListBoxRow *row)
{
  if (row == self->row_highcontrast)
    {
      toggle_switch (self->value_highcontrast);
    }
  else if (row == self->row_large_text)
    {
      toggle_switch (self->value_large_text);
    }
  else if (row == self->row_screen_keyboard)
    {
      toggle_switch (self->screen_keyboard_switch);
    }
  else if (row == self->row_mouse_keys)
    {
      toggle_switch (self->mouse_keys_switch);
    }
  else if (row == self->row_zoom)
    {
      run_dialog (self, GTK_DIALOG (cc_zoom_options_dialog_new ()));
    }
  else if (row == self->row_cursor_size)
    {
      run_dialog (self, GTK_DIALOG (cc_cursor_size_dialog_new ()));
    }
  else if (row == self->row_screen_reader)
    {
      run_dialog (self, GTK_DIALOG (cc_screen_reader_dialog_new ()));
    }
  else if (row == self->row_sound_keys)
    {
      run_dialog (self, GTK_DIALOG (cc_sound_keys_dialog_new ()));
    }
  else if (row == self->row_visual_alerts)
    {
      run_dialog (self, GTK_DIALOG (cc_visual_alerts_dialog_new ()));
    }
  else if (row == self->row_repeat_keys)
    {
      run_dialog (self, GTK_DIALOG (cc_repeat_keys_dialog_new ()));
    }
  else if (row == self->row_cursor_blinking)
    {
      show_dialog (self, self->cursor_blinking_dialog);
    }
  else if (row == self->row_accessx)
    {
      show_dialog (self, self->typing_dialog);
    }
  else if (row == self->row_click_assist)
    {
      show_dialog (self, self->pointing_dialog);
    }
}

static void
cc_ua_panel_init_seeing (CcUaPanel *self)
{
  add_section (self->list_seeing, self);

  add_separators (GTK_LIST_BOX (self->list_seeing));

  g_signal_connect_object (self->list_seeing, "row-activated",
                           G_CALLBACK (activate_row), self, G_CONNECT_SWAPPED);

  g_settings_bind_with_mapping (self->interface_settings, KEY_GTK_THEME,
                                self->value_highcontrast,
                                "active", G_SETTINGS_BIND_DEFAULT,
                                get_contrast_mapping,
                                set_contrast_mapping,
                                self,
                                NULL);

  /* large text */

  g_settings_bind_with_mapping (self->interface_settings, KEY_TEXT_SCALING_FACTOR,
                                self->value_large_text,
                                "active", G_SETTINGS_BIND_DEFAULT,
                                get_large_text_mapping,
                                set_large_text_mapping,
                                self->interface_settings,
                                NULL);

  /* cursor size */

  g_settings_bind_with_mapping (self->interface_settings, KEY_MOUSE_CURSOR_SIZE, // FIXME
                                self->value_cursor_size,
                                "label", G_SETTINGS_BIND_GET,
                                cursor_size_label_mapping_get,
                                NULL, NULL, NULL);

  /* zoom */

  g_settings_bind_with_mapping (self->application_settings, "screen-magnifier-enabled",
                                self->value_zoom,
                                "label", G_SETTINGS_BIND_GET,
                                on_off_label_mapping_get,
                                NULL, NULL, NULL);

  /* screen reader */

  g_settings_bind_with_mapping (self->application_settings, "screen-reader-enabled",
                                self->value_screen_reader, "label",
                                G_SETTINGS_BIND_GET,
                                on_off_label_mapping_get,
                                NULL, NULL, NULL);

  /* sound keys */

  g_settings_bind_with_mapping (self->kb_settings, KEY_TOGGLEKEYS_ENABLED,
                                self->value_sound_keys, "label",
                                G_SETTINGS_BIND_GET,
                                on_off_label_mapping_get,
                                NULL, NULL, NULL);
}

/* hearing/sound section */

static void
cc_ua_panel_init_hearing (CcUaPanel *self)
{
  add_section (self->list_hearing, self);

  add_separators (GTK_LIST_BOX (self->list_hearing));

  g_signal_connect_object (self->list_hearing, "row-activated",
                           G_CALLBACK (activate_row), self, G_CONNECT_SWAPPED);

  g_settings_bind_with_mapping (self->wm_settings, KEY_VISUAL_BELL_ENABLED,
                                self->value_visual_alerts,
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

  gtk_label_set_text (GTK_LABEL (self->value_repeat_keys), on ? _("On") : _("Off"));
}

static void
on_cursor_blinking_toggled (CcUaPanel *self)
{
  gboolean on;

  on = g_settings_get_boolean (self->interface_settings, KEY_CURSOR_BLINKING);

  gtk_label_set_text (GTK_LABEL (self->value_row_cursor_blinking), on ? _("On") : _("Off"));
}

static void
update_accessx_label (CcUaPanel *self)
{
  gboolean on;

  on = g_settings_get_boolean (self->kb_settings, KEY_STICKYKEYS_ENABLED) ||
       g_settings_get_boolean (self->kb_settings, KEY_SLOWKEYS_ENABLED) ||
       g_settings_get_boolean (self->kb_settings, KEY_BOUNCEKEYS_ENABLED);

  gtk_label_set_text (GTK_LABEL (self->value_accessx), on ? _("On") : _("Off"));
}

static void
cc_ua_panel_init_keyboard (CcUaPanel *self)
{
  GtkWidget *list;
  GtkWidget *w;
  GtkWidget *sw;

  list = self->list_typing;
  add_section (list, self);

  add_separators (GTK_LIST_BOX (list));

  g_signal_connect_object (list, "row-activated",
                           G_CALLBACK (activate_row), self, G_CONNECT_SWAPPED);

  /* on-screen keyboard */
  g_settings_bind (self->application_settings, KEY_SCREEN_KEYBOARD_ENABLED,
                   self->screen_keyboard_switch, "active",
                   G_SETTINGS_BIND_DEFAULT);

  /* Repeat keys */
  g_signal_connect_object (self->kb_desktop_settings, "changed",
                           G_CALLBACK (on_repeat_keys_toggled), self, G_CONNECT_SWAPPED);
  on_repeat_keys_toggled (self);

  /* Cursor Blinking */
  g_signal_connect_object (self->interface_settings, "changed",
                           G_CALLBACK (on_cursor_blinking_toggled), self, G_CONNECT_SWAPPED);

  self->toplevels = g_slist_prepend (self->toplevels, self->cursor_blinking_dialog);

  g_signal_connect (self->cursor_blinking_dialog, "delete-event",
                    G_CALLBACK (gtk_widget_hide_on_delete), NULL);

  sw = self->cursor_blinking_switch;
  g_settings_bind (self->interface_settings, KEY_CURSOR_BLINKING,
                   sw, "active",
                   G_SETTINGS_BIND_DEFAULT);
  on_cursor_blinking_toggled (self);

  g_settings_bind (self->interface_settings, KEY_CURSOR_BLINKING_TIME,
                   gtk_range_get_adjustment (GTK_RANGE (self->cursor_blinking_scale)), "value",
                   G_SETTINGS_BIND_DEFAULT);


  /* accessx */
  g_signal_connect_object (self->kb_settings, "changed",
                           G_CALLBACK (update_accessx_label), self, G_CONNECT_SWAPPED);
  update_accessx_label (self);

  /* enable shortcuts */
  sw = self->typing_keyboard_toggle_switch;
  g_settings_bind (self->kb_settings, KEY_KEYBOARD_TOGGLE,
                   sw, "active",
                   G_SETTINGS_BIND_DEFAULT);

  /* sticky keys */
  sw = self->typing_stickykeys_switch;
  g_settings_bind (self->kb_settings, KEY_STICKYKEYS_ENABLED,
                   sw, "active",
                   G_SETTINGS_BIND_DEFAULT);

  w = self->typing_stickykeys_disable_two_keys_check;
  g_settings_bind (self->kb_settings, KEY_STICKYKEYS_TWO_KEY_OFF,
                   w, "active",
                   G_SETTINGS_BIND_NO_SENSITIVITY);
  g_object_bind_property (sw, "active", w, "sensitive", G_BINDING_SYNC_CREATE);

  w = self->typing_stickykeys_beep_modifier_check;
  g_settings_bind (self->kb_settings, KEY_STICKYKEYS_MODIFIER_BEEP,
                   w, "active",
                   G_SETTINGS_BIND_NO_SENSITIVITY);
  g_object_bind_property (sw, "active", w, "sensitive", G_BINDING_SYNC_CREATE);

  /* slow keys */
  sw = self->typing_slowkeys_switch;
  g_settings_bind (self->kb_settings, KEY_SLOWKEYS_ENABLED,
                   sw, "active",
                   G_SETTINGS_BIND_DEFAULT);

  w = self->typing_slowkeys_delay_scale;
  g_settings_bind (self->kb_settings, KEY_SLOWKEYS_DELAY,
                   gtk_range_get_adjustment (GTK_RANGE (w)), "value",
                   G_SETTINGS_BIND_DEFAULT);
  w = self->typing_slowkeys_delay_box;
  g_object_bind_property (sw, "active", w, "sensitive", G_BINDING_SYNC_CREATE);

  w = self->typing_slowkeys_beep_pressed_check;
  g_settings_bind (self->kb_settings, KEY_SLOWKEYS_BEEP_PRESS,
                   w, "active",
                   G_SETTINGS_BIND_DEFAULT);
  g_object_bind_property (sw, "active", w, "sensitive", G_BINDING_SYNC_CREATE);

  w = self->typing_slowkeys_beep_accepted_check;
  g_settings_bind (self->kb_settings, KEY_SLOWKEYS_BEEP_ACCEPT,
                   w, "active",
                   G_SETTINGS_BIND_DEFAULT);
  g_object_bind_property (sw, "active", w, "sensitive", G_BINDING_SYNC_CREATE);

  w = self->typing_slowkeys_beep_rejected_check;
  g_settings_bind (self->kb_settings, KEY_SLOWKEYS_BEEP_REJECT,
                   w, "active",
                   G_SETTINGS_BIND_DEFAULT);
  g_object_bind_property (sw, "active", w, "sensitive", G_BINDING_SYNC_CREATE);

  /* bounce keys */
  sw = self->typing_bouncekeys_switch;
  g_settings_bind (self->kb_settings, KEY_BOUNCEKEYS_ENABLED,
                   sw, "active",
                   G_SETTINGS_BIND_DEFAULT);

  w = self->typing_bouncekeys_delay_scale;
  g_settings_bind (self->kb_settings, KEY_BOUNCEKEYS_DELAY,
                   gtk_range_get_adjustment (GTK_RANGE (w)), "value",
                   G_SETTINGS_BIND_DEFAULT);
  w = self->typing_bouncekeys_delay_box;
  g_object_bind_property (sw, "active", w, "sensitive", G_BINDING_SYNC_CREATE);

  w = self->typing_bouncekeys_beep_rejected_check;
  g_settings_bind (self->kb_settings, KEY_BOUNCEKEYS_BEEP_REJECT,
                   w, "active",
                   G_SETTINGS_BIND_NO_SENSITIVITY);
  g_object_bind_property (sw, "active", w, "sensitive", G_BINDING_SYNC_CREATE);

  self->toplevels = g_slist_prepend (self->toplevels, self->typing_dialog);

  g_signal_connect (self->typing_dialog, "delete-event",
                    G_CALLBACK (gtk_widget_hide_on_delete), NULL);
}

/* mouse/pointing & clicking section */
static void
update_click_assist_label (CcUaPanel *self)
{
  gboolean on;

  on = g_settings_get_boolean (self->mouse_settings, KEY_SECONDARY_CLICK_ENABLED) ||
       g_settings_get_boolean (self->mouse_settings, KEY_DWELL_CLICK_ENABLED);

  gtk_label_set_text (GTK_LABEL (self->value_click_assist), on ? _("On") : _("Off"));
}


static void
cc_ua_panel_init_mouse (CcUaPanel *self)
{
  GtkWidget *list;
  GtkWidget *sw;
  GtkWidget *w;

  list = self->list_pointing;
  add_section (list, self);

  add_separators (GTK_LIST_BOX (list));

  g_signal_connect_object (list, "row-activated",
                           G_CALLBACK (activate_row), self, G_CONNECT_SWAPPED);

  g_settings_bind (self->kb_settings, KEY_MOUSEKEYS_ENABLED,
                   self->mouse_keys_switch, "active",
                   G_SETTINGS_BIND_DEFAULT);

  g_settings_bind (self->interface_settings, KEY_LOCATE_POINTER,
                   self->locate_pointer_switch, "active",
                   G_SETTINGS_BIND_DEFAULT);

  g_signal_connect_object (self->mouse_settings, "changed",
                           G_CALLBACK (update_click_assist_label), self, G_CONNECT_SWAPPED);
  update_click_assist_label (self);

  /* simulated secondary click */
  sw = self->pointing_secondary_click_switch;
  g_settings_bind (self->mouse_settings, KEY_SECONDARY_CLICK_ENABLED,
                   sw, "active",
                   G_SETTINGS_BIND_DEFAULT);

  w = self->pointing_secondary_click_delay_scale;
  g_settings_bind (self->mouse_settings, KEY_SECONDARY_CLICK_TIME,
                   gtk_range_get_adjustment (GTK_RANGE (w)), "value",
                   G_SETTINGS_BIND_DEFAULT);
  w = self->pointing_secondary_click_delay_box;
  g_object_bind_property (sw, "active", w, "sensitive", G_BINDING_SYNC_CREATE);

  /* dwell click */
  sw = self->pointing_hover_click_switch;
  g_settings_bind (self->mouse_settings, KEY_DWELL_CLICK_ENABLED,
                   sw, "active",
                   G_SETTINGS_BIND_DEFAULT);

  w = self->pointing_dwell_delay_scale;
  g_settings_bind (self->mouse_settings, KEY_DWELL_TIME,
                   gtk_range_get_adjustment (GTK_RANGE (w)), "value",
                   G_SETTINGS_BIND_DEFAULT);
  w = self->pointing_dwell_delay_box;
  g_object_bind_property (sw, "active", w, "sensitive", G_BINDING_SYNC_CREATE);

  w = self->pointing_dwell_threshold_scale;
  g_settings_bind (self->mouse_settings, KEY_DWELL_THRESHOLD,
                   gtk_range_get_adjustment (GTK_RANGE (w)), "value",
                   G_SETTINGS_BIND_DEFAULT);
  w = self->pointing_dwell_threshold_box;
  g_object_bind_property (sw, "active", w, "sensitive", G_BINDING_SYNC_CREATE);

  self->toplevels = g_slist_prepend (self->toplevels, self->pointing_dialog);

  g_settings_bind (self->gsd_mouse_settings, "double-click",
                   gtk_range_get_adjustment (GTK_RANGE (self->scale_double_click_delay)), "value",
                   G_SETTINGS_BIND_DEFAULT);

  gtk_scale_add_mark (GTK_SCALE (self->scale_double_click_delay), 400, GTK_POS_BOTTOM, NULL);

  g_signal_connect (self->pointing_dialog, "delete-event",
                    G_CALLBACK (gtk_widget_hide_on_delete), NULL);
}

static void
cc_ua_panel_init (CcUaPanel *self)
{
  g_resources_register (cc_universal_access_get_resource ());

  gtk_widget_init_template (GTK_WIDGET (self));

  self->interface_settings = g_settings_new (INTERFACE_SETTINGS);
  self->a11y_settings = g_settings_new (A11Y_SETTINGS);
  self->wm_settings = g_settings_new (WM_SETTINGS);
  self->kb_settings = g_settings_new (KEYBOARD_SETTINGS);
  self->kb_desktop_settings = g_settings_new (KEYBOARD_DESKTOP_SETTINGS);
  self->mouse_settings = g_settings_new (MOUSE_SETTINGS);
  self->gsd_mouse_settings = g_settings_new (GSD_MOUSE_SETTINGS);
  self->application_settings = g_settings_new (APPLICATION_SETTINGS);

  cc_ua_panel_init_status (self);
  cc_ua_panel_init_seeing (self);
  cc_ua_panel_init_hearing (self);
  cc_ua_panel_init_keyboard (self);
  cc_ua_panel_init_mouse (self);

  gtk_scrolled_window_set_min_content_height (GTK_SCROLLED_WINDOW (self->universal_access_panel), SCROLL_HEIGHT);

  self->focus_adjustment = gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW (self->universal_access_panel));
  gtk_container_set_focus_vadjustment (GTK_CONTAINER (self->universal_access_content), self->focus_adjustment);
}
