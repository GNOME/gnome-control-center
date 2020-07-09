/* cc-keyboard-shortcut-panel.c
 *
 * Copyright (C) 2010 Intel, Inc
 * Copyright (C) 2016 Endless, Inc
 * Copyright (C) 2020 System76, Inc.
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
 * Author: Thomas Wood <thomas.wood@intel.com>
 *         Georges Basile Stavracas Neto <gbsneto@gnome.org>
 *         Ian Douglas Scott <idscott@system76.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <glib/gi18n.h>

#include "cc-alt-chars-key-dialog.h"
#include "cc-keyboard-shortcut-row.h"
#include "cc-keyboard-item.h"
#include "cc-keyboard-manager.h"
#include "cc-keyboard-option.h"
#include "cc-keyboard-panel.h"
#include "cc-keyboard-resources.h"
#include "cc-keyboard-shortcut-editor.h"
#include "cc-keyboard-shortcut-dialog.h"

#include "keyboard-shortcuts.h"

#include "cc-util.h"

#define SHORTCUT_DELIMITERS "+ "

struct _CcKeyboardPanel
{
  CcPanel             parent_instance;

  /* Alternate characters key */
  CcAltCharsKeyDialog *alt_chars_key_dialog;
  GSettings           *input_source_settings;
  GtkWidget           *value_alternate_chars;

  GtkListBoxRow       *common_shortcuts_row;

  GRegex             *pictures_regex;
};

CC_PANEL_REGISTER (CcKeyboardPanel, cc_keyboard_panel)

enum {
  PROP_0,
  PROP_PARAMETERS
};

static const gchar* custom_css =
"button.reset-shortcut-button {"
"    padding: 0;"
"}";


#define DEFAULT_LV3_OPTION 5
static struct {
  const char *xkb_option;
  const char *label;
  const char *widget_name;
} lv3_xkb_options[] = {
  { "lv3:switch", NC_("keyboard key", "Right Ctrl"), "radiobutton_rightctrl" },
  { "lv3:menu_switch", NC_("keyboard key", "Menu Key"), "radiobutton_menukey" },
  { "lv3:lwin_switch", NC_("keyboard key", "Left Super"), "radiobutton_leftsuper" },
  { "lv3:rwin_switch", NC_("keyboard key", "Right Super"), "radiobutton_rightsuper" },
  { "lv3:lalt_switch", NC_("keyboard key", "Left Alt"), "radiobutton_leftalt" },
  { "lv3:ralt_switch", NC_("keyboard key", "Right Alt"), "radiobutton_rightalt" },
};

static void
alternate_chars_activated (GtkWidget       *button,
                           GtkListBoxRow   *row,
                           CcKeyboardPanel *self)
{
  GtkWindow *window;

  window = GTK_WINDOW (cc_shell_get_toplevel (cc_panel_get_shell (CC_PANEL (self))));

  gtk_window_set_transient_for (GTK_WINDOW (self->alt_chars_key_dialog), window);
  gtk_widget_show (GTK_WIDGET (self->alt_chars_key_dialog));
}

static gboolean
transform_binding_to_alt_chars (GValue   *value,
                                GVariant *variant,
                                gpointer  user_data)
{
  const char **items;
  guint i;

  items = g_variant_get_strv (variant, NULL);
  if (!items)
    goto bail;

  for (i = 0; items[i] != NULL; i++)
    {
      guint j;

      if (!g_str_has_prefix (items[i], "lv3:"))
        continue;

      for (j = 0; j < G_N_ELEMENTS (lv3_xkb_options); j++)
        {
          if (!g_str_equal (items[i], lv3_xkb_options[j].xkb_option))
            continue;

          g_value_set_string (value,
                              g_dpgettext2 (NULL, "keyboard key", lv3_xkb_options[j].label));
          return TRUE;
        }
    }

bail:
  g_value_set_string (value,
                      g_dpgettext2 (NULL, "keyboard key", lv3_xkb_options[DEFAULT_LV3_OPTION].label));
  return TRUE;
}

static void
keyboard_shortcuts_activated (GtkWidget       *button,
                              GtkListBoxRow   *row,
                              CcKeyboardPanel *self)
{
  GtkWindow *window;
  GtkWidget *shortcut_dialog;

  if (row == self->common_shortcuts_row)
    {
      window = GTK_WINDOW (cc_shell_get_toplevel (cc_panel_get_shell (CC_PANEL (self))));

      shortcut_dialog = cc_keyboard_shortcut_dialog_new ();
      gtk_window_set_transient_for (GTK_WINDOW (shortcut_dialog), window);
      gtk_widget_show (GTK_WIDGET (shortcut_dialog));
    }
}

static void
cc_keyboard_panel_set_property (GObject      *object,
                               guint         property_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  switch (property_id)
    {
    case PROP_PARAMETERS:
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static const char *
cc_keyboard_panel_get_help_uri (CcPanel *panel)
{
  return "help:gnome-help/keyboard";
}

static void
cc_keyboard_panel_finalize (GObject *object)
{
  CcKeyboardPanel *self = CC_KEYBOARD_PANEL (object);
  GtkWidget *window;

  g_clear_pointer (&self->pictures_regex, g_regex_unref);
  g_clear_object (&self->input_source_settings);

  cc_keyboard_option_clear_all ();

  G_OBJECT_CLASS (cc_keyboard_panel_parent_class)->finalize (object);
}

static void
cc_keyboard_panel_class_init (CcKeyboardPanelClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  CcPanelClass *panel_class = CC_PANEL_CLASS (klass);

  panel_class->get_help_uri = cc_keyboard_panel_get_help_uri;

  object_class->set_property = cc_keyboard_panel_set_property;
  object_class->finalize = cc_keyboard_panel_finalize;

  g_object_class_override_property (object_class, PROP_PARAMETERS, "parameters");

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/keyboard/cc-keyboard-panel.ui");

  gtk_widget_class_bind_template_child (widget_class, CcKeyboardPanel, value_alternate_chars);
  gtk_widget_class_bind_template_child (widget_class, CcKeyboardPanel, common_shortcuts_row);

  gtk_widget_class_bind_template_callback (widget_class, alternate_chars_activated);
  gtk_widget_class_bind_template_callback (widget_class, keyboard_shortcuts_activated);
}

static void
cc_keyboard_panel_init (CcKeyboardPanel *self)
{
  GtkCssProvider *provider;

  g_resources_register (cc_keyboard_get_resource ());

  gtk_widget_init_template (GTK_WIDGET (self));

  /* Custom CSS */
  provider = gtk_css_provider_new ();
  gtk_css_provider_load_from_data (provider, custom_css, -1, NULL);

  gtk_style_context_add_provider_for_screen (gdk_screen_get_default (),
                                             GTK_STYLE_PROVIDER (provider),
                                             GTK_STYLE_PROVIDER_PRIORITY_APPLICATION + 1);

  g_object_unref (provider);

  /* Alternate characters key */
  self->input_source_settings = g_settings_new ("org.gnome.desktop.input-sources");
  g_settings_bind_with_mapping (self->input_source_settings,
                                "xkb-options",
                                self->value_alternate_chars,
                                "label",
                                G_SETTINGS_BIND_GET,
                                transform_binding_to_alt_chars,
                                NULL,
                                self->value_alternate_chars,
                                NULL);

  self->alt_chars_key_dialog = cc_alt_chars_key_dialog_new (self->input_source_settings);
}