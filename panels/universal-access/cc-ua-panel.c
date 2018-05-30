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

#include "zoom-options.h"

#define WID(w) GTK_WIDGET (gtk_builder_get_object (self->builder, w))

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

/* application settings */
#define APPLICATION_SETTINGS         "org.gnome.desktop.a11y.applications"
#define KEY_SCREEN_KEYBOARD_ENABLED  "screen-keyboard-enabled"
#define KEY_SCREEN_MAGNIFIER_ENABLED "screen-magnifier-enabled"
#define KEY_SCREEN_READER_ENABLED    "screen-reader-enabled"

/* wm settings */
#define WM_SETTINGS                  "org.gnome.desktop.wm.preferences"
#define KEY_VISUAL_BELL_ENABLED      "visual-bell"
#define KEY_VISUAL_BELL_TYPE         "visual-bell-type"
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

  GtkBuilder *builder;

  GSettings *wm_settings;
  GSettings *a11y_settings;
  GSettings *interface_settings;
  GSettings *kb_settings;
  GSettings *mouse_settings;
  GSettings *kb_desktop_settings;
  GSettings *application_settings;
  GSettings *gsd_mouse_settings;

  ZoomOptions *zoom_options;

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

  g_clear_object (&self->builder);
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

  g_clear_object (&self->zoom_options);

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
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  CcPanelClass *panel_class = CC_PANEL_CLASS (klass);

  panel_class->get_help_uri = cc_ua_panel_get_help_uri;

  object_class->dispose = cc_ua_panel_dispose;
}

/* zoom options dialog */
static void
zoom_options_launch (CcUaPanel *self)
{
  if (self->zoom_options == NULL)
    {
      GtkWindow *window = GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (self)));
      self->zoom_options = zoom_options_new (window);
    }

  gtk_window_present_with_time (GTK_WINDOW (self->zoom_options), GDK_CURRENT_TIME);
}

/* cursor size dialog */
static void
cursor_size_toggled (GtkWidget *button,
                     CcUaPanel *self)
{
  guint cursor_size;

  if (!gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button)))
    return;

  cursor_size = GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (button), "cursor-size"));
  g_settings_set_int (self->interface_settings, KEY_MOUSE_CURSOR_SIZE, cursor_size);
  g_debug ("Setting cursor size to %d", cursor_size);
}

static void
cursor_size_setup (CcUaPanel *self)
{
  guint cursor_sizes[] = { 24, 32, 48, 64, 96 };
  guint current_cursor_size, i;
  GtkWidget *grid;
  GtkSizeGroup *size_group;
  GtkWidget *last_radio_button = NULL;

  grid = WID ("cursor_size_grid");
  gtk_style_context_add_class (gtk_widget_get_style_context (grid), "linked");

  current_cursor_size = g_settings_get_int (self->interface_settings,
                                            KEY_MOUSE_CURSOR_SIZE);
  size_group = gtk_size_group_new (GTK_SIZE_GROUP_BOTH);

  for (i = 0; i < G_N_ELEMENTS(cursor_sizes); i++)
    {
      GtkWidget *image, *button;
      char *cursor_image_name;

      cursor_image_name = g_strdup_printf ("/org/gnome/control-center/universal-access/left_ptr_%dpx.png", cursor_sizes[i]);
      image = gtk_image_new_from_resource (cursor_image_name);
      g_free (cursor_image_name);

      button = gtk_radio_button_new_from_widget (GTK_RADIO_BUTTON (last_radio_button));
      last_radio_button = button;
      gtk_toggle_button_set_mode (GTK_TOGGLE_BUTTON (button), FALSE);
      g_object_set_data (G_OBJECT (button), "cursor-size", GUINT_TO_POINTER (cursor_sizes[i]));

      gtk_container_add (GTK_CONTAINER (button), image);
      gtk_grid_attach (GTK_GRID (grid), button, i, 0, 1, 1);
      gtk_size_group_add_widget (size_group, button);

      g_signal_connect (button, "toggled",
                        G_CALLBACK (cursor_size_toggled), self);

      if (current_cursor_size == cursor_sizes[i])
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), TRUE);
    }

  gtk_widget_show_all (grid);
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
keynav_failed (GtkWidget *list, GtkDirectionType direction, CcUaPanel *self)
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
  g_signal_connect (list, "keynav-failed", G_CALLBACK (keynav_failed), self);

  self->sections = g_list_append (self->sections, list);
  self->sections_reverse = g_list_prepend (self->sections_reverse, list);
}

static void
cc_ua_panel_init_status (CcUaPanel *self)
{
  GtkWidget *box;

  box = GTK_WIDGET (gtk_builder_get_object (self->builder, "section_status"));
  self->sections_reverse = g_list_prepend (self->sections_reverse, box);

  g_settings_bind (self->a11y_settings, KEY_ALWAYS_SHOW_STATUS,
                   WID ("switch_status"), "active",
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
activate_row (CcUaPanel *self, GtkListBoxRow *row)
{
  GtkWidget *dialog;
  const gchar *dialog_id;
  const gchar *widget_name;

  /* Check switches to toggle */
  widget_name = gtk_buildable_get_name (GTK_BUILDABLE (row));
  if (widget_name)
    {
      if (!g_strcmp0 (widget_name, "row_highcontrast"))
        {
          toggle_switch (WID ("value_highcontrast"));
          return;
        }
      if (!g_strcmp0 (widget_name, "row_large_text"))
        {
          toggle_switch (WID ("value_large_text"));
          return;
        }
      if (!g_strcmp0 (widget_name, "row_screen_keyboard"))
        {
          toggle_switch (WID ("screen_keyboard_switch"));
          return;
        }
      if (!g_strcmp0 (widget_name, "row_mouse_keys"))
        {
          toggle_switch (WID ("mouse_keys_switch"));
          return;
        }
    }

  /* Check dialog to open */
  dialog_id = (const gchar *)g_object_get_data (G_OBJECT (row), "dialog-id");
  if (g_strcmp0 (dialog_id, "zoom") == 0)
    {
      zoom_options_launch (self);
      return;
    }

  dialog = (GtkWidget *)g_object_get_data (G_OBJECT (row), "dialog");
  if (dialog == NULL)
    return;

  gtk_window_set_transient_for (GTK_WINDOW (dialog),
                                GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (self))));
  gtk_window_present (GTK_WINDOW (dialog));
}

static void
cc_ua_panel_init_seeing (CcUaPanel *self)
{
  GtkWidget *list;
  GtkWidget *dialog;

  list = WID ("list_seeing");
  add_section (list, self);

  add_separators (GTK_LIST_BOX (list));

  g_signal_connect_swapped (list, "row-activated",
                            G_CALLBACK (activate_row), self);

  g_settings_bind_with_mapping (self->interface_settings, KEY_GTK_THEME,
                                WID ("value_highcontrast"),
                                "active", G_SETTINGS_BIND_DEFAULT,
                                get_contrast_mapping,
                                set_contrast_mapping,
                                self,
                                NULL);

  /* large text */

  g_settings_bind_with_mapping (self->interface_settings, KEY_TEXT_SCALING_FACTOR,
                                WID ("value_large_text"),
                                "active", G_SETTINGS_BIND_DEFAULT,
                                get_large_text_mapping,
                                set_large_text_mapping,
                                self->interface_settings,
                                NULL);

  /* cursor size */

  cursor_size_setup (self);

  g_settings_bind_with_mapping (self->interface_settings, KEY_MOUSE_CURSOR_SIZE,
                                WID ("value_cursor_size"),
                                "label", G_SETTINGS_BIND_GET,
                                cursor_size_label_mapping_get,
                                NULL, NULL, NULL);

  dialog = WID ("cursor_size_dialog");
  self->toplevels = g_slist_prepend (self->toplevels, dialog);

  g_object_set_data (G_OBJECT (WID ("row_cursor_size")), "dialog", dialog);
  g_signal_connect (dialog, "delete-event",
                    G_CALLBACK (gtk_widget_hide_on_delete), NULL);

  /* zoom */

  g_settings_bind_with_mapping (self->application_settings, "screen-magnifier-enabled",
                                WID ("value_zoom"),
                                "label", G_SETTINGS_BIND_GET,
                                on_off_label_mapping_get,
                                NULL, NULL, NULL);

  g_object_set_data (G_OBJECT (WID ("row_zoom")), "dialog-id", "zoom");

  /* screen reader */

  g_settings_bind_with_mapping (self->application_settings, "screen-reader-enabled",
                                WID ("value_screen_reader"), "label",
                                G_SETTINGS_BIND_GET,
                                on_off_label_mapping_get,
                                NULL, NULL, NULL);

  g_settings_bind (self->application_settings, "screen-reader-enabled",
                   WID ("screen_reader_switch"), "active",
                   G_SETTINGS_BIND_DEFAULT);

  dialog = WID ("screen_reader_dialog");
  self->toplevels = g_slist_prepend (self->toplevels, dialog);

  g_object_set_data (G_OBJECT (WID ("row_screen_reader")), "dialog", dialog);
  g_signal_connect (dialog, "delete-event",
                    G_CALLBACK (gtk_widget_hide_on_delete), NULL);

  /* sound keys */

  g_settings_bind_with_mapping (self->kb_settings, KEY_TOGGLEKEYS_ENABLED,
                                WID ("value_sound_keys"), "label",
                                G_SETTINGS_BIND_GET,
                                on_off_label_mapping_get,
                                NULL, NULL, NULL);

  g_settings_bind (self->kb_settings, KEY_TOGGLEKEYS_ENABLED,
                   WID ("sound_keys_switch"), "active",
                   G_SETTINGS_BIND_DEFAULT);

  dialog = WID ("sound_keys_dialog");
  self->toplevels = g_slist_prepend (self->toplevels, dialog);

  g_object_set_data (G_OBJECT (WID ("row_sound_keys")), "dialog", dialog);
  g_signal_connect (dialog, "delete-event",
                    G_CALLBACK (gtk_widget_hide_on_delete), NULL);
}

/* hearing/sound section */
static void
visual_bell_type_notify_cb (GSettings   *settings,
                            const gchar *key,
                            CcUaPanel   *self)
{
  GtkWidget *widget;
  GDesktopVisualBellType type;

  type = g_settings_get_enum (self->wm_settings, KEY_VISUAL_BELL_TYPE);

  if (type == G_DESKTOP_VISUAL_BELL_FRAME_FLASH)
    widget = WID ("visual_alerts_window_radio");
  else
    widget = WID ("visual_alerts_screen_radio");

  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), TRUE);
}

static void
visual_bell_type_toggle_cb (GtkWidget *button,
                            CcUaPanel *panel)
{
  gboolean frame_flash;
  GDesktopVisualBellType type;

  frame_flash = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button));

  if (frame_flash)
    type = G_DESKTOP_VISUAL_BELL_FRAME_FLASH;
  else
    type = G_DESKTOP_VISUAL_BELL_FULLSCREEN_FLASH;
  g_settings_set_enum (panel->wm_settings, KEY_VISUAL_BELL_TYPE, type);
}

static void
test_flash (GtkButton *button,
            gpointer   data)
{
  GtkWidget *toplevel = gtk_widget_get_toplevel (GTK_WIDGET (button));
  gdk_window_beep (gtk_widget_get_window (toplevel));
}

static void
cc_ua_panel_init_hearing (CcUaPanel *self)
{
  GtkWidget *list;
  GtkWidget *dialog;

  list = WID ("list_hearing");
  add_section (list, self);

  add_separators (GTK_LIST_BOX (list));

  g_signal_connect_swapped (list, "row-activated",
                            G_CALLBACK (activate_row), self);

  /* set the initial visual bell values */
  visual_bell_type_notify_cb (NULL, NULL, self);

  /* and listen */
  g_settings_bind (self->wm_settings, KEY_VISUAL_BELL_ENABLED,
                   WID ("visual_alerts_switch"), "active",
                   G_SETTINGS_BIND_DEFAULT);

  g_settings_bind_with_mapping (self->wm_settings, KEY_VISUAL_BELL_ENABLED,
                                WID ("value_visual_alerts"),
                                "label", G_SETTINGS_BIND_GET,
                                on_off_label_mapping_get,
                                NULL, NULL, NULL);

  g_object_bind_property (WID ("visual_alerts_switch"), "active",
                          WID ("visual_alerts_window_radio"), "sensitive",
                          G_BINDING_SYNC_CREATE);
  g_object_bind_property (WID ("visual_alerts_switch"), "active",
                          WID ("visual_alerts_screen_radio"), "sensitive",
                          G_BINDING_SYNC_CREATE);

  g_signal_connect (self->wm_settings, "changed::" KEY_VISUAL_BELL_TYPE,
                    G_CALLBACK (visual_bell_type_notify_cb), self);
  g_signal_connect (WID ("visual_alerts_window_radio"),
                    "toggled", G_CALLBACK (visual_bell_type_toggle_cb), self);

  dialog = WID ("visual_alerts_dialog");
  self->toplevels = g_slist_prepend (self->toplevels, dialog);

  g_object_set_data (G_OBJECT (WID ("row_visual_alerts")), "dialog", dialog);

  g_signal_connect (dialog, "delete-event",
                    G_CALLBACK (gtk_widget_hide_on_delete), NULL);

  g_signal_connect (WID ("visual_alerts_test_button"),
                    "clicked", G_CALLBACK (test_flash), NULL);
}

/* typing/keyboard section */
static void
on_repeat_keys_toggled (GSettings *settings, const gchar *key, CcUaPanel *self)
{
  gboolean on;

  on = g_settings_get_boolean (settings, KEY_REPEAT_KEYS);

  gtk_label_set_text (GTK_LABEL (WID ("value_repeat_keys")), on ? _("On") : _("Off"));

  gtk_widget_set_sensitive (WID ("repeat-keys-delay-grid"), on);
  gtk_widget_set_sensitive (WID ("repeat-keys-speed-grid"), on);
}

static void
on_cursor_blinking_toggled (GSettings *settings, const gchar *key, CcUaPanel *self)
{
  gboolean on;

  on = g_settings_get_boolean (settings, KEY_CURSOR_BLINKING);

  gtk_label_set_text (GTK_LABEL (WID ("value_row_cursor_blinking")), on ? _("On") : _("Off"));
}

static void
update_accessx_label (GSettings *settings, const gchar *key, CcUaPanel *self)
{
  gboolean on;

  on = g_settings_get_boolean (settings, KEY_STICKYKEYS_ENABLED) ||
       g_settings_get_boolean (settings, KEY_SLOWKEYS_ENABLED) ||
       g_settings_get_boolean (settings, KEY_BOUNCEKEYS_ENABLED);

  gtk_label_set_text (GTK_LABEL (WID ("value_accessx")), on ? _("On") : _("Off"));
}

static void
cc_ua_panel_init_keyboard (CcUaPanel *self)
{
  GtkWidget *list;
  GtkWidget *w;
  GtkWidget *sw;
  GtkWidget *dialog;

  list = WID ("list_typing");
  add_section (list, self);

  add_separators (GTK_LIST_BOX (list));

  g_signal_connect_swapped (list, "row-activated",
                            G_CALLBACK (activate_row), self);

  /* on-screen keyboard */
  sw = WID ("screen_keyboard_switch");
  g_settings_bind (self->application_settings, KEY_SCREEN_KEYBOARD_ENABLED,
                   sw, "active",
                   G_SETTINGS_BIND_DEFAULT);

  /* Repeat keys */
  g_signal_connect (self->kb_desktop_settings, "changed",
                   G_CALLBACK (on_repeat_keys_toggled), self);

  dialog = WID ("repeat_keys_dialog");
  self->toplevels = g_slist_prepend (self->toplevels, dialog);

  g_object_set_data (G_OBJECT (WID ("row_repeat_keys")), "dialog", dialog);

  g_signal_connect (dialog, "delete-event",
                    G_CALLBACK (gtk_widget_hide_on_delete), NULL);

  sw = WID ("repeat_keys_switch");
  g_settings_bind (self->kb_desktop_settings, KEY_REPEAT_KEYS,
                   sw, "active",
                   G_SETTINGS_BIND_DEFAULT);
  on_repeat_keys_toggled (self->kb_desktop_settings, NULL, self);

  g_settings_bind (self->kb_desktop_settings, "delay",
                   gtk_range_get_adjustment (GTK_RANGE (WID ("repeat_keys_delay_scale"))), "value",
                   G_SETTINGS_BIND_DEFAULT);
  g_settings_bind (self->kb_desktop_settings, "repeat-interval",
                   gtk_range_get_adjustment (GTK_RANGE (WID ("repeat_keys_speed_scale"))), "value",
                   G_SETTINGS_BIND_DEFAULT);

  /* Cursor Blinking */
  g_signal_connect (self->interface_settings, "changed",
                    G_CALLBACK (on_cursor_blinking_toggled), self);

  dialog = WID ("cursor_blinking_dialog");
  self->toplevels = g_slist_prepend (self->toplevels, dialog);

  g_object_set_data (G_OBJECT (WID ("row_cursor_blinking")), "dialog", dialog);

  g_signal_connect (dialog, "delete-event",
                    G_CALLBACK (gtk_widget_hide_on_delete), NULL);

  sw = WID ("cursor_blinking_switch");
  g_settings_bind (self->interface_settings, KEY_CURSOR_BLINKING,
                   sw, "active",
                   G_SETTINGS_BIND_DEFAULT);
  on_cursor_blinking_toggled (self->interface_settings, NULL, self);

  g_settings_bind (self->interface_settings, KEY_CURSOR_BLINKING_TIME,
                   gtk_range_get_adjustment (GTK_RANGE (WID ("cursor_blinking_scale"))), "value",
                   G_SETTINGS_BIND_DEFAULT);


  /* accessx */
  g_signal_connect (self->kb_settings, "changed",
                    G_CALLBACK (update_accessx_label), self);
  update_accessx_label (self->kb_settings, NULL, self);

  /* enable shortcuts */
  sw = WID ("typing_keyboard_toggle_switch");
  g_settings_bind (self->kb_settings, KEY_KEYBOARD_TOGGLE,
                   sw, "active",
                   G_SETTINGS_BIND_DEFAULT);

  /* sticky keys */
  sw = WID ("typing_stickykeys_switch");
  g_settings_bind (self->kb_settings, KEY_STICKYKEYS_ENABLED,
                   sw, "active",
                   G_SETTINGS_BIND_DEFAULT);

  w = WID ("typing_stickykeys_disable_two_keys_check");
  g_settings_bind (self->kb_settings, KEY_STICKYKEYS_TWO_KEY_OFF,
                   w, "active",
                   G_SETTINGS_BIND_NO_SENSITIVITY);
  g_object_bind_property (sw, "active", w, "sensitive", G_BINDING_SYNC_CREATE);

  w = WID ("typing_stickykeys_beep_modifier_check");
  g_settings_bind (self->kb_settings, KEY_STICKYKEYS_MODIFIER_BEEP,
                   w, "active",
                   G_SETTINGS_BIND_NO_SENSITIVITY);
  g_object_bind_property (sw, "active", w, "sensitive", G_BINDING_SYNC_CREATE);

  /* slow keys */
  sw = WID ("typing_slowkeys_switch");
  g_settings_bind (self->kb_settings, KEY_SLOWKEYS_ENABLED,
                   sw, "active",
                   G_SETTINGS_BIND_DEFAULT);

  w = WID ("typing_slowkeys_delay_scale");
  g_settings_bind (self->kb_settings, KEY_SLOWKEYS_DELAY,
                   gtk_range_get_adjustment (GTK_RANGE (w)), "value",
                   G_SETTINGS_BIND_DEFAULT);
  w = WID ("typing_slowkeys_delay_box");
  g_object_bind_property (sw, "active", w, "sensitive", G_BINDING_SYNC_CREATE);

  w = WID ("typing_slowkeys_beep_pressed_check");
  g_settings_bind (self->kb_settings, KEY_SLOWKEYS_BEEP_PRESS,
                   w, "active",
                   G_SETTINGS_BIND_DEFAULT);
  g_object_bind_property (sw, "active", w, "sensitive", G_BINDING_SYNC_CREATE);

  w = WID ("typing_slowkeys_beep_accepted_check");
  g_settings_bind (self->kb_settings, KEY_SLOWKEYS_BEEP_ACCEPT,
                   w, "active",
                   G_SETTINGS_BIND_DEFAULT);
  g_object_bind_property (sw, "active", w, "sensitive", G_BINDING_SYNC_CREATE);

  w = WID ("typing_slowkeys_beep_rejected_check");
  g_settings_bind (self->kb_settings, KEY_SLOWKEYS_BEEP_REJECT,
                   w, "active",
                   G_SETTINGS_BIND_DEFAULT);
  g_object_bind_property (sw, "active", w, "sensitive", G_BINDING_SYNC_CREATE);

  /* bounce keys */
  sw = WID ("typing_bouncekeys_switch");
  g_settings_bind (self->kb_settings, KEY_BOUNCEKEYS_ENABLED,
                   sw, "active",
                   G_SETTINGS_BIND_DEFAULT);

  w = WID ("typing_bouncekeys_delay_scale");
  g_settings_bind (self->kb_settings, KEY_BOUNCEKEYS_DELAY,
                   gtk_range_get_adjustment (GTK_RANGE (w)), "value",
                   G_SETTINGS_BIND_DEFAULT);
  w = WID ("typing_bouncekeys_delay_box");
  g_object_bind_property (sw, "active", w, "sensitive", G_BINDING_SYNC_CREATE);

  w = WID ("typing_bouncekeys_beep_rejected_check");
  g_settings_bind (self->kb_settings, KEY_BOUNCEKEYS_BEEP_REJECT,
                   w, "active",
                   G_SETTINGS_BIND_NO_SENSITIVITY);
  g_object_bind_property (sw, "active", w, "sensitive", G_BINDING_SYNC_CREATE);

  dialog = WID ("typing_dialog");
  self->toplevels = g_slist_prepend (self->toplevels, dialog);

  g_object_set_data (G_OBJECT (WID ("row_accessx")), "dialog", dialog);

  g_signal_connect (dialog, "delete-event",
                    G_CALLBACK (gtk_widget_hide_on_delete), NULL);
}

/* mouse/pointing & clicking section */
static void
update_click_assist_label (GSettings *settings, const gchar *key, CcUaPanel *self)
{
  gboolean on;

  on = g_settings_get_boolean (settings, KEY_SECONDARY_CLICK_ENABLED) ||
       g_settings_get_boolean (settings, KEY_DWELL_CLICK_ENABLED);

  gtk_label_set_text (GTK_LABEL (WID ("value_click_assist")), on ? _("On") : _("Off"));
}


static void
cc_ua_panel_init_mouse (CcUaPanel *self)
{
  GtkWidget *list;
  GtkWidget *dialog;
  GtkWidget *sw;
  GtkWidget *w;

  list = WID ("list_pointing");
  add_section (list, self);

  add_separators (GTK_LIST_BOX (list));

  g_signal_connect_swapped (list, "row-activated",
                            G_CALLBACK (activate_row), self);

  g_settings_bind (self->kb_settings, KEY_MOUSEKEYS_ENABLED,
                   WID ("mouse_keys_switch"), "active",
                   G_SETTINGS_BIND_DEFAULT);

  g_signal_connect (self->mouse_settings, "changed",
                    G_CALLBACK (update_click_assist_label), self);
  update_click_assist_label (self->mouse_settings, NULL, self);

  /* simulated secondary click */
  sw = WID ("pointing_secondary_click_switch");
  g_settings_bind (self->mouse_settings, KEY_SECONDARY_CLICK_ENABLED,
                   sw, "active",
                   G_SETTINGS_BIND_DEFAULT);

  w = WID ("pointing_secondary_click_delay_scale");
  g_settings_bind (self->mouse_settings, KEY_SECONDARY_CLICK_TIME,
                   gtk_range_get_adjustment (GTK_RANGE (w)), "value",
                   G_SETTINGS_BIND_DEFAULT);
  w = WID ("pointing_secondary_click_delay_box");
  g_object_bind_property (sw, "active", w, "sensitive", G_BINDING_SYNC_CREATE);

  /* dwell click */
  sw = WID ("pointing_hover_click_switch");
  g_settings_bind (self->mouse_settings, KEY_DWELL_CLICK_ENABLED,
                   sw, "active",
                   G_SETTINGS_BIND_DEFAULT);

  w = WID ("pointing_dwell_delay_scale");
  g_settings_bind (self->mouse_settings, KEY_DWELL_TIME,
                   gtk_range_get_adjustment (GTK_RANGE (w)), "value",
                   G_SETTINGS_BIND_DEFAULT);
  w = WID ("pointing_dwell_delay_box");
  g_object_bind_property (sw, "active", w, "sensitive", G_BINDING_SYNC_CREATE);

  w = WID ("pointing_dwell_threshold_scale");
  g_settings_bind (self->mouse_settings, KEY_DWELL_THRESHOLD,
                   gtk_range_get_adjustment (GTK_RANGE (w)), "value",
                   G_SETTINGS_BIND_DEFAULT);
  w = WID ("pointing_dwell_threshold_box");
  g_object_bind_property (sw, "active", w, "sensitive", G_BINDING_SYNC_CREATE);

  dialog = WID ("pointing_dialog");
  self->toplevels = g_slist_prepend (self->toplevels, dialog);

  g_object_set_data (G_OBJECT (WID ("row_click_assist")), "dialog", dialog);

  g_settings_bind (self->gsd_mouse_settings, "double-click",
                   gtk_range_get_adjustment (GTK_RANGE (WID ("scale_double_click_delay"))), "value",
                   G_SETTINGS_BIND_DEFAULT);

  gtk_scale_add_mark (GTK_SCALE (WID ("scale_double_click_delay")), 400, GTK_POS_BOTTOM, NULL);

  g_signal_connect (dialog, "delete-event",
                    G_CALLBACK (gtk_widget_hide_on_delete), NULL);
}

static void
cc_ua_panel_init (CcUaPanel *self)
{
  GtkWidget *panel;
  GtkWidget *content;

  g_resources_register (cc_universal_access_get_resource ());

  self->interface_settings = g_settings_new (INTERFACE_SETTINGS);
  self->a11y_settings = g_settings_new (A11Y_SETTINGS);
  self->wm_settings = g_settings_new (WM_SETTINGS);
  self->kb_settings = g_settings_new (KEYBOARD_SETTINGS);
  self->kb_desktop_settings = g_settings_new (KEYBOARD_DESKTOP_SETTINGS);
  self->mouse_settings = g_settings_new (MOUSE_SETTINGS);
  self->gsd_mouse_settings = g_settings_new (GSD_MOUSE_SETTINGS);
  self->application_settings = g_settings_new (APPLICATION_SETTINGS);

  self->builder = gtk_builder_new ();
  gtk_builder_add_from_resource (self->builder,

                                 "/org/gnome/control-center/universal-access/uap.ui",
                                 NULL);

  cc_ua_panel_init_status (self);
  cc_ua_panel_init_seeing (self);
  cc_ua_panel_init_hearing (self);
  cc_ua_panel_init_keyboard (self);
  cc_ua_panel_init_mouse (self);

  panel = WID ("universal_access_panel");
  content = WID ("universal_access_content");

  gtk_scrolled_window_set_min_content_height (GTK_SCROLLED_WINDOW (panel), SCROLL_HEIGHT);

  self->focus_adjustment = gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW (panel));
  gtk_container_set_focus_vadjustment (GTK_CONTAINER (content), self->focus_adjustment);

  gtk_container_add (GTK_CONTAINER (self), panel);
}
