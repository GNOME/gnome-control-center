/*
 * Copyright (C) 2010 Intel, Inc
 * Copyright (C) 2016 Endless, Inc
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
 *
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

#include "keyboard-shortcuts.h"

#include "cc-util.h"

#define SHORTCUT_DELIMITERS "+ "

typedef struct {
  CcKeyboardItem *item;
  gchar          *section_title;
  gchar          *section_id;
} RowData;

struct _CcKeyboardPanel
{
  CcPanel             parent_instance;

  /* Search */
  GtkWidget          *empty_search_placeholder;
  GtkWidget          *reset_button;
  GtkWidget          *search_bar;
  GtkWidget          *search_button;
  GtkWidget          *search_entry;
  guint               search_bar_handler_id;

  /* Shortcuts */
  GtkWidget          *shortcuts_listbox;
  GtkListBoxRow      *add_shortcut_row;
  GtkSizeGroup       *accelerator_sizegroup;

  /* Alternate characters key */
  CcAltCharsKeyDialog *alt_chars_key_dialog;
  GSettings           *input_source_settings;
  GtkWidget           *value_alternate_chars;

  /* Custom shortcut dialog */
  GtkWidget          *shortcut_editor;

  GRegex             *pictures_regex;

  CcKeyboardManager  *manager;
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

/* RowData functions */
static RowData *
row_data_new (CcKeyboardItem *item,
              const gchar    *section_id,
              const gchar    *section_title)
{
  RowData *data;

  data = g_new0 (RowData, 1);
  data->item = g_object_ref (item);
  data->section_id = g_strdup (section_id);
  data->section_title = g_strdup (section_title);

  return data;
}

static void
row_data_free (RowData *data)
{
  g_object_unref (data->item);
  g_free (data->section_id);
  g_free (data->section_title);
  g_free (data);
}

static void
reset_all_shortcuts_cb (GtkWidget *widget,
                        gpointer   user_data)
{
  CcKeyboardPanel *self;
  RowData *data;

  self = user_data;

  if (widget == (GtkWidget *) self->add_shortcut_row)
    return;

  data = g_object_get_data (G_OBJECT (widget), "data");

  /* Don't reset custom shortcuts */
  if (cc_keyboard_item_get_item_type (data->item) == CC_KEYBOARD_ITEM_TYPE_GSETTINGS_PATH)
    return;

  /* cc_keyboard_manager_reset_shortcut() already resets conflicting shortcuts,
   * so no other check is needed here. */
  cc_keyboard_manager_reset_shortcut (self->manager, data->item);
}

static void
reset_all_clicked_cb (CcKeyboardPanel *self)
{
  GtkWidget *dialog, *toplevel, *button;
  guint response;

  toplevel = gtk_widget_get_toplevel (GTK_WIDGET (self));
  dialog = gtk_message_dialog_new (GTK_WINDOW (toplevel),
                                   GTK_DIALOG_MODAL | GTK_DIALOG_USE_HEADER_BAR | GTK_DIALOG_DESTROY_WITH_PARENT,
                                   GTK_MESSAGE_WARNING,
                                   GTK_BUTTONS_NONE,
                                   _("Reset All Shortcuts?"));

  gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
                                            _("Resetting the shortcuts may affect your custom shortcuts. "
                                              "This cannot be undone."));

  gtk_dialog_add_buttons (GTK_DIALOG (dialog),
                          _("Cancel"), GTK_RESPONSE_CANCEL,
                          _("Reset All"), GTK_RESPONSE_ACCEPT,
                          NULL);

  gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_CANCEL);

  /* Make the "Reset All" button destructive */
  button = gtk_dialog_get_widget_for_response (GTK_DIALOG (dialog), GTK_RESPONSE_ACCEPT);
  gtk_style_context_add_class (gtk_widget_get_style_context (button), "destructive-action");

  /* Reset shortcuts if accepted */
  response = gtk_dialog_run (GTK_DIALOG (dialog));

  if (response == GTK_RESPONSE_ACCEPT)
    {
      gtk_container_foreach (GTK_CONTAINER (self->shortcuts_listbox),
                             reset_all_shortcuts_cb,
                             self);
    }

  gtk_widget_destroy (dialog);
}

static void
add_item (CcKeyboardPanel *self,
          CcKeyboardItem  *item,
          const gchar     *section_id,
          const gchar     *section_title)
{
  GtkWidget *row;

  row = GTK_WIDGET(cc_keyboard_shortcut_row_new(item,
			                        self->manager,
						CC_KEYBOARD_SHORTCUT_EDITOR (self->shortcut_editor),
		                                self->accelerator_sizegroup));
  g_object_set_data_full (G_OBJECT (row),
                          "data",
                          row_data_new (item, section_id, section_title),
                          (GDestroyNotify) row_data_free);
  gtk_container_add (GTK_CONTAINER (self->shortcuts_listbox), row);
}

static void
remove_item (CcKeyboardPanel *self,
             CcKeyboardItem  *item)
{
  GList *children, *l;

  children = gtk_container_get_children (GTK_CONTAINER (self->shortcuts_listbox));

  for (l = children; l != NULL; l = l->next)
    {
      RowData *row_data;

      row_data = g_object_get_data (l->data, "data");

      if (row_data->item == item)
        {
          gtk_container_remove (GTK_CONTAINER (self->shortcuts_listbox), l->data);
          break;
        }
    }

  g_list_free (children);
}

static gboolean
strv_contains_prefix_or_match (gchar       **strv,
                               const gchar  *prefix)
{
  guint i;

  const struct {
    const gchar *key;
    const gchar *untranslated;
    const gchar *synonym;
  } key_aliases[] =
    {
      { "ctrl",   "Ctrl",  "ctrl" },
      { "win",    "Super", "super" },
      { "option",  NULL,   "alt" },
      { "command", NULL,   "super" },
      { "apple",   NULL,   "super" },
    };

  for (i = 0; strv[i]; i++)
    {
      if (g_str_has_prefix (strv[i], prefix))
        return TRUE;
    }

  for (i = 0; i < G_N_ELEMENTS (key_aliases); i++)
    {
      g_autofree gchar *alias = NULL;
      const gchar *synonym;

      if (!g_str_has_prefix (key_aliases[i].key, prefix))
        continue;

      if (key_aliases[i].untranslated)
        {
          const gchar *translated_label;

          /* Steal GTK+'s translation */
          translated_label = g_dpgettext2 ("gtk30", "keyboard label", key_aliases[i].untranslated);
          alias = g_utf8_strdown (translated_label, -1);
        }

      synonym = key_aliases[i].synonym;

      /* If a translation or synonym of the key is in the accelerator, and we typed
       * the key, also consider that a prefix */
      if ((alias && g_strv_contains ((const gchar * const *) strv, alias)) ||
          (synonym && g_strv_contains ((const gchar * const *) strv, synonym)))
        {
          return TRUE;
        }
    }

  return FALSE;
}

static gboolean
search_match_shortcut (CcKeyboardItem *item,
                       const gchar    *search)
{
  GStrv shortcut_tokens, search_tokens;
  g_autofree gchar *normalized_accel = NULL;
  g_autofree gchar *accel = NULL;
  gboolean match;
  guint i;
  GList *key_combos, *l;
  CcKeyCombo *combo;

  key_combos = cc_keyboard_item_get_key_combos (item);
  for (l = key_combos; l != NULL; l = l->next)
    {
      combo = l->data;

      if (is_empty_binding (combo))
        continue;

      match = TRUE;
      accel = convert_keysym_state_to_string (combo);
      normalized_accel = cc_util_normalize_casefold_and_unaccent (accel);

      shortcut_tokens = g_strsplit_set (normalized_accel, SHORTCUT_DELIMITERS, -1);
      search_tokens = g_strsplit_set (search, SHORTCUT_DELIMITERS, -1);

      for (i = 0; search_tokens[i] != NULL; i++)
        {
          const gchar *token;

          /* Strip leading and trailing whitespaces */
          token = g_strstrip (search_tokens[i]);

          if (g_utf8_strlen (token, -1) == 0)
            continue;

          match = match && strv_contains_prefix_or_match (shortcut_tokens, token);

          if (!match)
            break;
        }

      g_strfreev (shortcut_tokens);
      g_strfreev (search_tokens);

      if (match)
        return TRUE;
    }

  return FALSE;
}

static gint
sort_function (GtkListBoxRow *a,
               GtkListBoxRow *b,
               gpointer       user_data)
{
  CcKeyboardPanel *self;
  RowData *a_data, *b_data;
  gint retval;

  self = user_data;

  if (a == self->add_shortcut_row)
    return 1;

  if (b == self->add_shortcut_row)
    return -1;

  a_data = g_object_get_data (G_OBJECT (a), "data");
  b_data = g_object_get_data (G_OBJECT (b), "data");

  /* Put custom shortcuts below everything else */
  if (cc_keyboard_item_get_item_type (a_data->item) == CC_KEYBOARD_ITEM_TYPE_GSETTINGS_PATH)
    return 1;
  else if (cc_keyboard_item_get_item_type (b_data->item) == CC_KEYBOARD_ITEM_TYPE_GSETTINGS_PATH)
    return -1;

  retval = g_strcmp0 (a_data->section_title, b_data->section_title);

  if (retval != 0)
    return retval;

  return g_strcmp0 (cc_keyboard_item_get_description (a_data->item), cc_keyboard_item_get_description (b_data->item));
}

static void
header_function (GtkListBoxRow *row,
                 GtkListBoxRow *before,
                 gpointer       user_data)
{
  CcKeyboardPanel *self;
  gboolean add_header;
  RowData *data;

  self = user_data;
  add_header = FALSE;

  /* The + row always has a separator */
  if (row == self->add_shortcut_row)
    {
      GtkWidget *separator = gtk_separator_new (GTK_ORIENTATION_HORIZONTAL);
      gtk_widget_show (separator);

      gtk_list_box_row_set_header (row, separator);

      return;
    }

  data = g_object_get_data (G_OBJECT (row), "data");

  if (before)
    {
      RowData *before_data = g_object_get_data (G_OBJECT (before), "data");

      if (before_data)
        add_header = g_strcmp0 (before_data->section_id, data->section_id) != 0;
    }
  else
    {
      add_header = TRUE;
    }

  if (add_header)
    {
      GtkWidget *box, *label, *separator;
      g_autofree gchar *markup = NULL;

      box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
      gtk_widget_show (box);
      gtk_widget_set_margin_top (box, before ? 18 : 6);

      markup = g_strdup_printf ("<b>%s</b>", _(data->section_title));
      label = g_object_new (GTK_TYPE_LABEL,
                            "label", markup,
                            "use-markup", TRUE,
                            "xalign", 0.0,
                            "margin-start", 6,
                            NULL);
      gtk_widget_show (label);
      gtk_style_context_add_class (gtk_widget_get_style_context (label), "dim-label");
      gtk_container_add (GTK_CONTAINER (box), label);

      separator = gtk_separator_new (GTK_ORIENTATION_HORIZONTAL);
      gtk_widget_show (separator);
      gtk_container_add (GTK_CONTAINER (box), separator);

      gtk_list_box_row_set_header (row, box);
    }
  else
    {
      gtk_list_box_row_set_header (row, NULL);
    }
}

static gboolean
filter_function (GtkListBoxRow *row,
                 gpointer       user_data)
{
  CcKeyboardPanel *self = user_data;
  CcKeyboardItem *item;
  RowData *data;
  gboolean retval;
  g_autofree gchar *search = NULL;
  g_autofree gchar *name = NULL;
  g_auto(GStrv) terms = NULL;
  guint i;

  if (gtk_entry_get_text_length (GTK_ENTRY (self->search_entry)) == 0)
    return TRUE;

  /* When searching, the '+' row is always hidden */
  if (row == self->add_shortcut_row)
    return FALSE;

  data = g_object_get_data (G_OBJECT (row), "data");
  item = data->item;
  name = cc_util_normalize_casefold_and_unaccent (cc_keyboard_item_get_description (item));
  search = cc_util_normalize_casefold_and_unaccent (gtk_entry_get_text (GTK_ENTRY (self->search_entry)));
  terms = g_strsplit (search, " ", -1);

  for (i = 0; terms && terms[i]; i++)
    {
      retval = strstr (name, terms[i]) || search_match_shortcut (item, terms[i]);
      if (!retval)
        break;
    }

  return retval;
}

static void
shortcut_row_activated (GtkWidget       *button,
                        GtkListBoxRow   *row,
                        CcKeyboardPanel *self)
{
  CcKeyboardShortcutEditor *editor;

  editor = CC_KEYBOARD_SHORTCUT_EDITOR (self->shortcut_editor);

  if (row != self->add_shortcut_row)
    {
      RowData *data = g_object_get_data (G_OBJECT (row), "data");

      cc_keyboard_shortcut_editor_set_mode (editor, CC_SHORTCUT_EDITOR_EDIT);
      cc_keyboard_shortcut_editor_set_item (editor, data->item);
    }
  else
    {
      cc_keyboard_shortcut_editor_set_mode (editor, CC_SHORTCUT_EDITOR_CREATE);
      cc_keyboard_shortcut_editor_set_item (editor, NULL);
    }

  gtk_widget_show (self->shortcut_editor);
}

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
  g_clear_object (&self->accelerator_sizegroup);
  g_clear_object (&self->input_source_settings);

  cc_keyboard_option_clear_all ();

  if (self->search_bar_handler_id != 0)
    {
      window = cc_shell_get_toplevel (cc_panel_get_shell (CC_PANEL (self)));
      g_signal_handler_disconnect (window, self->search_bar_handler_id);
    }

  G_OBJECT_CLASS (cc_keyboard_panel_parent_class)->finalize (object);
}

static void
cc_keyboard_panel_constructed (GObject *object)
{
  CcKeyboardPanel *self = CC_KEYBOARD_PANEL (object);
  GtkWindow *toplevel;
  CcShell *shell;

  G_OBJECT_CLASS (cc_keyboard_panel_parent_class)->constructed (object);

  /* Setup the dialog's transient parent */
  shell = cc_panel_get_shell (CC_PANEL (self));
  toplevel = GTK_WINDOW (cc_shell_get_toplevel (shell));
  gtk_window_set_transient_for (GTK_WINDOW (self->shortcut_editor), toplevel);

  cc_shell_embed_widget_in_header (shell, self->reset_button, GTK_POS_LEFT);
  cc_shell_embed_widget_in_header (shell, self->search_button, GTK_POS_RIGHT);

  self->search_bar_handler_id =
    g_signal_connect_swapped (toplevel,
                              "key-press-event",
                              G_CALLBACK (gtk_search_bar_handle_event),
                              self->search_bar);
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
  object_class->constructed = cc_keyboard_panel_constructed;

  g_object_class_override_property (object_class, PROP_PARAMETERS, "parameters");

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/keyboard/cc-keyboard-panel.ui");

  gtk_widget_class_bind_template_child (widget_class, CcKeyboardPanel, add_shortcut_row);
  gtk_widget_class_bind_template_child (widget_class, CcKeyboardPanel, empty_search_placeholder);
  gtk_widget_class_bind_template_child (widget_class, CcKeyboardPanel, reset_button);
  gtk_widget_class_bind_template_child (widget_class, CcKeyboardPanel, search_bar);
  gtk_widget_class_bind_template_child (widget_class, CcKeyboardPanel, search_button);
  gtk_widget_class_bind_template_child (widget_class, CcKeyboardPanel, search_entry);
  gtk_widget_class_bind_template_child (widget_class, CcKeyboardPanel, shortcuts_listbox);
  gtk_widget_class_bind_template_child (widget_class, CcKeyboardPanel, value_alternate_chars);

  gtk_widget_class_bind_template_callback (widget_class, reset_all_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, shortcut_row_activated);
  gtk_widget_class_bind_template_callback (widget_class, alternate_chars_activated);
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

  /* Shortcut manager */
  self->manager = cc_keyboard_manager_new ();

  /* Shortcut editor dialog */
  self->shortcut_editor = cc_keyboard_shortcut_editor_new (self->manager);

  /* Use a sizegroup to make the accelerator labels the same width */
  self->accelerator_sizegroup = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);

  g_signal_connect_object (self->manager,
                           "shortcut-added",
                           G_CALLBACK (add_item),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (self->manager,
                           "shortcut-removed",
                           G_CALLBACK (remove_item),
                           self,
                           G_CONNECT_SWAPPED);

  cc_keyboard_manager_load_shortcuts (self->manager);

  /* Setup the shortcuts shortcuts_listbox */
  gtk_list_box_set_sort_func (GTK_LIST_BOX (self->shortcuts_listbox),
                              sort_function,
                              self,
                              NULL);

  gtk_list_box_set_header_func (GTK_LIST_BOX (self->shortcuts_listbox),
                                header_function,
                                self,
                                NULL);

  gtk_list_box_set_filter_func (GTK_LIST_BOX (self->shortcuts_listbox),
                                filter_function,
                                self,
                                NULL);

  gtk_list_box_set_placeholder (GTK_LIST_BOX (self->shortcuts_listbox), self->empty_search_placeholder);
}

