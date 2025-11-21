/* cc-xkb-modifier-page.c
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

#include <glib/gi18n.h>
#include <adwaita.h>

#include "cc-xkb-modifier-page.h"

struct _CcXkbModifierPage
{
  AdwNavigationPage   parent_instance;

  AdwPreferencesPage *xkb_modifier_page;
  AdwPreferencesGroup *options_group;
  AdwPreferencesGroup *switch_group;
  AdwSwitchRow        *switch_row;

  GSettings      *input_source_settings;
  const CcXkbModifier *modifier;
  GSList         *radio_group;
};

G_DEFINE_TYPE (CcXkbModifierPage, cc_xkb_modifier_page, ADW_TYPE_NAVIGATION_PAGE)

#define RADIO_STORAGE_KEY "cc-xkb-option"

static const gchar*
get_translated_xkb_option_label (const CcXkbOption *option)
{
  g_assert (option != NULL);

  return g_dpgettext2 (NULL, "keyboard key", option->label);
}

static const CcXkbOption*
get_xkb_option_from_name (const CcXkbModifier *modifier, const gchar* name)
{
  const CcXkbOption *options = modifier->options;
  int i;

  for (i = 0; options[i].label && options[i].xkb_option; i++)
    {
      if (g_str_equal (name, options[i].xkb_option))
        return &options[i];
    }

  return NULL;
}

static gboolean
get_is_customized (CcXkbModifierPage *self)
{
  gboolean switch_active = adw_switch_row_get_active (self->switch_row);

  if (self->modifier->switch_inverted)
    return !switch_active;

  return switch_active;
}

static void
set_is_customized (CcXkbModifierPage *self,
                   gboolean           is_customized)
{
  gboolean switch_active = self->modifier->switch_inverted ? !is_customized : is_customized;

  adw_switch_row_set_active (self->switch_row, switch_active);
}

static GtkCheckButton *
get_radio_button_from_xkb_option_name (CcXkbModifierPage *self,
                                       const gchar       *name)
{
  CcXkbOption *option;
  GSList *l;

  for (l = self->radio_group; l != NULL; l = l->next)
    {
      option = g_object_get_data (l->data, RADIO_STORAGE_KEY);
      if (g_strcmp0 (option->xkb_option, name) == 0)
        return l->data;
    }

  return NULL;
}

static void
update_active_radio (CcXkbModifierPage *self)
{
  g_auto(GStrv) options = NULL;
  guint i;

  options = g_settings_get_strv (self->input_source_settings, "xkb-options");

  for (i = 0; options != NULL && options[i] != NULL; i++)
    {
      GtkCheckButton *radio;

      if (!g_str_has_prefix (options[i], self->modifier->prefix))
        continue;

      radio = get_radio_button_from_xkb_option_name (self, options[i]);

      if (!radio)
        continue;

      gtk_check_button_set_active (GTK_CHECK_BUTTON (radio), TRUE);
      set_is_customized (self, TRUE);
      return;
    }

  set_is_customized (self, FALSE);
}

static void
set_xkb_option (CcXkbModifierPage *self,
                gchar             *xkb_option)
{
  g_autoptr(GPtrArray) array = NULL;
  g_auto(GStrv) options = NULL;
  gboolean found;
  guint i;

  /* Either replace the existing "<modifier>:" option in the string
   * array, or add the option at the end
   */
  array = g_ptr_array_new ();
  options = g_settings_get_strv (self->input_source_settings, "xkb-options");
  found = FALSE;

  for (i = 0; options != NULL && options[i] != NULL; i++)
    {
      if (g_str_has_prefix (options[i], self->modifier->prefix))
        {
          if (!found && xkb_option != NULL)
            g_ptr_array_add (array, xkb_option);
          found = TRUE;
        }
      else
        {
          g_ptr_array_add (array, options[i]);
        }
    }

  if (!found && xkb_option != NULL)
    g_ptr_array_add (array, xkb_option);

  g_ptr_array_add (array, NULL);

  g_settings_set_strv (self->input_source_settings,
                       "xkb-options",
                       (const gchar * const *) array->pdata);
}

static void
on_active_radio_changed_cb (CcXkbModifierPage *self,
                            GtkCheckButton    *radio)
{
  CcXkbOption *option;

  if (!gtk_check_button_get_active (GTK_CHECK_BUTTON (radio)))
    return;

  if (!get_is_customized (self))
    return;

  option = g_object_get_data (G_OBJECT (radio), RADIO_STORAGE_KEY);
  set_xkb_option (self, option->xkb_option);
}

static void
on_xkb_options_changed_cb (CcXkbModifierPage *self)
{
  if (self->modifier == NULL)
    update_active_radio (self);
}

static gboolean
switch_row_changed_cb (CcXkbModifierPage *self)
{
  CcXkbOption *option;
  GSList *l;

  gtk_widget_set_sensitive (GTK_WIDGET (self->options_group), get_is_customized (self));

  if (get_is_customized (self))
    {
      for (l = self->radio_group; l != NULL; l = l->next)
        {
          if (gtk_check_button_get_active (l->data))
            {
              option = g_object_get_data (l->data, RADIO_STORAGE_KEY);
              set_xkb_option (self, option->xkb_option);
              break;
            }
        }
    }
  else
    {
      set_xkb_option (self, NULL);
    }

  return FALSE;
}

static const char *
get_layout_default_str (CcXkbModifierPage *self)
{
  GdkDisplay *display;
  GdkSeat *seat;
  GdkDevice *keyboard;
  GSList *l;
  int custom_event_code = 0;
  g_autoptr(GdkKeymapKey) keys = NULL;
  int n_keys;

  if (!g_str_equal (self->modifier->prefix, "lv3:"))
    return NULL;

  display = gdk_display_get_default ();
  if (!display)
    return NULL;

  seat = gdk_display_get_default_seat (display);
  if (!seat)
    return NULL;

  keyboard = gdk_seat_get_keyboard (seat);
  if (!keyboard)
    return NULL;

  for (l = self->radio_group; l != NULL; l = l->next)
    {
      if (gtk_check_button_get_active (l->data))
        {
          CcXkbOption *active_option = g_object_get_data (l->data, RADIO_STORAGE_KEY);
          custom_event_code = active_option->event_code;
          break;
        }
    }

  /* Find all currently active Level 3 shifts to see if anything other than Right Alt
     or the custom key is used, and if so, return NULL.
     It's not worth translating any other (very rare) Level 3 default option.
     Note that the GDK/X keycodes are offset by 8 compared to the kernel codes. */
  gdk_display_map_keyval (display, GDK_KEY_ISO_Level3_Shift, &keys, &n_keys);

  for (int i = 0; i < n_keys; i++)
    {
      /* Ignore keys from other layouts and the virtual Level 3 key */
      if (keys[i].group != gdk_device_get_active_layout_index (keyboard) || keys[i].keycode == 92)
        continue;

      if (custom_event_code && keys[i].keycode == custom_event_code + 8)
        continue;

      if (keys[i].keycode != KEY_RIGHTALT + 8)
        return NULL;
    }

  return g_dpgettext2 (NULL, "keyboard key", "Right Alt");
}

static void
cc_xkb_modifier_page_finalize (GObject *object)
{
  CcXkbModifierPage *self = (CcXkbModifierPage *)object;

  g_clear_object (&self->input_source_settings);
  g_clear_pointer (&self->radio_group, g_slist_free);

  G_OBJECT_CLASS (cc_xkb_modifier_page_parent_class)->finalize (object);
}

static void
cc_xkb_modifier_page_class_init (CcXkbModifierPageClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = cc_xkb_modifier_page_finalize;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/keyboard/cc-xkb-modifier-page.ui");

  gtk_widget_class_bind_template_child (widget_class, CcXkbModifierPage, xkb_modifier_page);
  gtk_widget_class_bind_template_child (widget_class, CcXkbModifierPage, options_group);
  gtk_widget_class_bind_template_child (widget_class, CcXkbModifierPage, switch_group);
  gtk_widget_class_bind_template_child (widget_class, CcXkbModifierPage, switch_row);

  gtk_widget_class_bind_template_callback (widget_class, switch_row_changed_cb);
}

static void
add_radio_buttons (CcXkbModifierPage *self)
{
  g_autoptr(GSList) group = NULL;
  GtkWidget *last_button = NULL;
  CcXkbOption *options = self->modifier->options;
  int i;

  for (i = 0; options[i].label && options[i].xkb_option; i++)
    {
      AdwActionRow *row;
      GtkWidget *radio_button;

      row = ADW_ACTION_ROW (adw_action_row_new ());
      adw_preferences_row_set_title (ADW_PREFERENCES_ROW (row),
                                     get_translated_xkb_option_label (&options[i]));

      radio_button = gtk_check_button_new ();
      gtk_widget_set_valign (radio_button, GTK_ALIGN_CENTER);
      gtk_check_button_set_group (GTK_CHECK_BUTTON (radio_button), GTK_CHECK_BUTTON (last_button));
      g_object_set_data (G_OBJECT (radio_button), RADIO_STORAGE_KEY, &options[i]);
      g_signal_connect_object (radio_button, "toggled", G_CALLBACK (on_active_radio_changed_cb),
                               self, G_CONNECT_SWAPPED);

      adw_action_row_add_prefix (row, radio_button);
      adw_action_row_set_activatable_widget (row, radio_button);

      adw_preferences_group_add (self->options_group, GTK_WIDGET (row));

      last_button = radio_button;
      group = g_slist_prepend (group, radio_button);
    }

  if (group != NULL)
    self->radio_group = g_steal_pointer (&group);
}

static void
cc_xkb_modifier_page_init (CcXkbModifierPage *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  self->modifier = NULL;

  self->input_source_settings = g_settings_new ("org.gnome.desktop.input-sources");
  g_signal_connect_object (self->input_source_settings,
                           "changed::xkb-options",
                           G_CALLBACK (on_xkb_options_changed_cb),
                           self, G_CONNECT_SWAPPED);
}

CcXkbModifierPage *
cc_xkb_modifier_page_new (GSettings *input_settings,
                          const CcXkbModifier *modifier)
{
  CcXkbModifierPage *self;
  const char *default_str;

  self = g_object_new (CC_TYPE_XKB_MODIFIER_PAGE, NULL);
  self->input_source_settings = g_object_ref (input_settings);

  self->modifier = modifier;

  add_radio_buttons (self);
  update_active_radio (self);

  default_str = get_layout_default_str (self);

  adw_navigation_page_set_title (ADW_NAVIGATION_PAGE (self), _(modifier->title));
  adw_preferences_row_set_title (ADW_PREFERENCES_ROW (self->switch_row), _(modifier->switch_label));
  adw_action_row_set_subtitle (ADW_ACTION_ROW (self->switch_row), default_str ? default_str : "");
  adw_preferences_page_set_description (self->xkb_modifier_page, _(modifier->description));

  gtk_widget_set_sensitive (GTK_WIDGET (self->options_group), get_is_customized (self));

  return self;
}

gboolean
xcb_modifier_transform_binding_to_label (GValue   *value,
                                         GVariant *variant,
                                         gpointer  user_data)
{
  const CcXkbModifier *modifier = user_data;
  const CcXkbOption *entry = NULL;
  g_autofree const char **items = g_variant_get_strv (variant, NULL);
  guint i;

  for (i = 0; items != NULL && items[i] != NULL; i++)
    {
      entry = get_xkb_option_from_name (modifier, items[i]);
      if (entry != NULL)
        break;
    }

  if (entry == NULL)
    {
      g_value_set_string (value, _(modifier->unset_label));
      return TRUE;
    }

  g_value_set_string (value, get_translated_xkb_option_label (entry));
  return TRUE;
}
