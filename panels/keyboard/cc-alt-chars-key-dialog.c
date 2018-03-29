/* cc-alt-chars-key-dialog.c
 *
 * Copyright 2019 Bastien Nocera <hadess@hadess.net>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "cc-alt-chars-key-dialog.h"

struct _CcAltCharsKeyDialog
{
  GtkDialog       parent_instance;

  GSettings      *input_source_settings;

  GtkRadioButton *radiobutton_rightctrl;
  GtkRadioButton *radiobutton_menukey;
  GtkRadioButton *radiobutton_leftsuper;
  GtkRadioButton *radiobutton_rightsuper;
  GtkRadioButton *radiobutton_leftalt;
  GtkRadioButton *radiobutton_rightalt;
};

G_DEFINE_TYPE (CcAltCharsKeyDialog, cc_alt_chars_key_dialog, GTK_TYPE_DIALOG)

static GtkRadioButton *
get_radio_button_from_xkb_option_name (CcAltCharsKeyDialog *self,
                                       const gchar         *name)
{
  if (g_str_equal (name, "lv3:switch"))
    return self->radiobutton_rightctrl;
  else if (g_str_equal (name, "lv3:menu_switch"))
    return self->radiobutton_menukey;
  else if (g_str_equal (name, "lv3:lwin_switch"))
    return self->radiobutton_leftsuper;
  else if (g_str_equal (name, "lv3:rwin_switch"))
    return self->radiobutton_rightsuper;
  else if (g_str_equal (name, "lv3:lalt_switch"))
    return self->radiobutton_leftalt;
  else if (g_str_equal (name, "lv3:ralt_switch"))
    return self->radiobutton_rightalt;

  return NULL;
}

static const gchar *
get_xkb_option_name_from_radio_button (CcAltCharsKeyDialog *self,
                                       GtkRadioButton      *radio)
{
  if (radio == self->radiobutton_rightctrl)
    return "lv3:switch";
  else if (radio == self->radiobutton_menukey)
    return "lv3:menu_switch";
  else if (radio == self->radiobutton_leftsuper)
    return "lv3:lwin_switch";
  else if (radio == self->radiobutton_rightsuper)
    return "lv3:rwin_switch";
  else if (radio == self->radiobutton_leftalt)
    return "lv3:lalt_switch";
  else if (radio == self->radiobutton_rightalt)
    return "lv3:ralt_switch";

  return NULL;
}

static void
update_active_radio (CcAltCharsKeyDialog *self)
{
  g_auto(GStrv) options = NULL;
  guint i;

  options = g_settings_get_strv (self->input_source_settings, "xkb-options");

  for (i = 0; options != NULL && options[i] != NULL; i++)
    {
      GtkRadioButton *radio;

      if (!g_str_has_prefix (options[i], "lv3:"))
        continue;

      radio = get_radio_button_from_xkb_option_name (self, options[i]);

      if (!radio)
        continue;

      g_message ("Found radio for %s", options[i]);
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (radio), TRUE);
      return;
    }

  /* Fallback to Right Alt as default */
  g_message ("Using fallback");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (self->radiobutton_rightalt), TRUE);
}

static void
on_active_lv3_changed_cb (GtkRadioButton      *radio,
                          CcAltCharsKeyDialog *self)
{
  g_autoptr(GPtrArray) array = NULL;
  g_auto(GStrv) options = NULL;
  gboolean found;
  guint i;

  if (!gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (radio)))
    return;

  /* Either replace the existing "lv3:" option in the string
   * array, or add the option at the end
   */
  array = g_ptr_array_new ();
  options = g_settings_get_strv (self->input_source_settings, "xkb-options");
  found = FALSE;

  for (i = 0; options != NULL && options[i] != NULL; i++)
    {
      if (g_str_has_prefix (options[i], "lv3:"))
        {
          g_ptr_array_add (array, (gchar *)get_xkb_option_name_from_radio_button (self, radio));
          found = TRUE;
        }
      else
        {
          g_ptr_array_add (array, options[i]);
        }
    }

  if (!found)
    g_ptr_array_add (array, (gchar *)get_xkb_option_name_from_radio_button (self, radio));

  g_ptr_array_add (array, NULL);

  g_settings_set_strv (self->input_source_settings,
                       "xkb-options",
                       (const gchar * const *) array->pdata);
}

static void
on_xkb_options_changed_cb (GSettings           *settings,
                           const gchar         *key,
                           CcAltCharsKeyDialog *self)
{
  update_active_radio (self);
}

static void
cc_alt_chars_key_dialog_finalize (GObject *object)
{
  CcAltCharsKeyDialog *self = (CcAltCharsKeyDialog *)object;

  g_clear_object (&self->input_source_settings);

  G_OBJECT_CLASS (cc_alt_chars_key_dialog_parent_class)->finalize (object);
}

static void
cc_alt_chars_key_dialog_class_init (CcAltCharsKeyDialogClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = cc_alt_chars_key_dialog_finalize;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/keyboard/cc-alt-chars-key-dialog.ui");

  gtk_widget_class_bind_template_child (widget_class, CcAltCharsKeyDialog, radiobutton_rightctrl);
  gtk_widget_class_bind_template_child (widget_class, CcAltCharsKeyDialog, radiobutton_menukey);
  gtk_widget_class_bind_template_child (widget_class, CcAltCharsKeyDialog, radiobutton_leftsuper);
  gtk_widget_class_bind_template_child (widget_class, CcAltCharsKeyDialog, radiobutton_rightsuper);
  gtk_widget_class_bind_template_child (widget_class, CcAltCharsKeyDialog, radiobutton_leftalt);
  gtk_widget_class_bind_template_child (widget_class, CcAltCharsKeyDialog, radiobutton_rightalt);

  gtk_widget_class_bind_template_callback (widget_class, on_active_lv3_changed_cb);
}

static void
cc_alt_chars_key_dialog_init (CcAltCharsKeyDialog *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  self->input_source_settings = g_settings_new ("org.gnome.desktop.input-sources");
  g_signal_connect (self->input_source_settings,
                    "changed::xkb-options",
                    G_CALLBACK (on_xkb_options_changed_cb),
                    self);
  update_active_radio (self);
}

GtkWidget *
cc_alt_chars_key_dialog_new (GSettings *input_settings)
{
  CcAltCharsKeyDialog *self;

  self = g_object_new (CC_TYPE_ALT_CHARS_KEY_DIALOG,
                       "use-header-bar", 1,
                       NULL);
  self->input_source_settings = g_object_ref (input_settings);

  return (GtkWidget *)self;
}
