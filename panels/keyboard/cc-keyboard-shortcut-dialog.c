/* cc-keyboard-shortcut-dialog.c
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
#define HANDY_USE_UNSTABLE_API
#include <handy.h>

#include "cc-keyboard-shortcut-dialog.h"
#include "cc-keyboard-item.h"
#include "cc-keyboard-manager.h"
#include "cc-keyboard-shortcut-editor.h"
#include "cc-keyboard-shortcut-row.h"
#include "cc-list-row.h"
#include "cc-util.h"
#include "list-box-helper.h"
#include "keyboard-shortcuts.h"

#define SHORTCUT_DELIMITERS "+ "

typedef struct {
  gchar          *section_title;
  gchar          *section_id;
  guint           modified_count;
  GtkLabel       *modified_label;
} SectionRowData;

typedef struct {
  CcKeyboardItem *item;
  gchar          *section_title;
  gchar          *section_id;
  SectionRowData *section_data;
} ShortcutRowData;

struct _CcKeyboardShortcutDialog
{
  GtkDialog  parent_instance;

  CcKeyboardManager  *manager;
  GtkWidget          *shortcut_editor;

  GHashTable         *sections;
  GtkListBoxRow      *section_row;

  GtkHeaderBar       *headerbar;
  GtkStack           *stack;
  GtkListBox         *section_listbox;
  GtkListBox         *shortcut_listbox;
  GtkScrolledWindow  *section_scrolled_window;
  GtkScrolledWindow  *shortcut_scrolled_window;
  GtkRevealer        *back_revealer;
  GtkRevealer        *reset_all_revealer;
  GtkRevealer        *search_revealer;
  GtkSearchEntry     *search_entry;
  GtkWidget          *empty_search_placeholder;
};

G_DEFINE_TYPE (CcKeyboardShortcutDialog, cc_keyboard_shortcut_dialog, GTK_TYPE_DIALOG)

static SectionRowData*
section_row_data_new (const gchar *section_id,
                      const gchar *section_title,
                      GtkLabel    *modified_label)
{
  SectionRowData *data;

  data = g_new0 (SectionRowData, 1);
  data->section_id = g_strdup (section_id);
  data->section_title = g_strdup (section_title);
  data->modified_count = 0;
  data->modified_label = modified_label;

  return data;
}

static void
section_row_data_free (SectionRowData *data)
{
  g_free (data->section_id);
  g_free (data->section_title);
  g_free (data);
}

static ShortcutRowData*
shortcut_row_data_new (CcKeyboardItem *item,
                       const gchar    *section_id,
                       const gchar    *section_title,
                       SectionRowData *section_data)
{
  ShortcutRowData *data;

  data = g_new0 (ShortcutRowData, 1);
  data->item = g_object_ref (item);
  data->section_id = g_strdup (section_id);
  data->section_title = g_strdup (section_title);
  data->section_data = section_data;

  return data;
}

static void
shortcut_row_data_free (ShortcutRowData *data)
{
  g_object_unref (data->item);
  g_free (data->section_id);
  g_free (data->section_title);
  g_free (data);
}

static HdyActionRow*
add_section (CcKeyboardShortcutDialog *self,
             const gchar     *section_id,
             const gchar     *section_title)
{
  GtkWidget *icon, *modified_label;
  HdyActionRow *row;

  row = g_object_new (HDY_TYPE_ACTION_ROW,
                      "visible", 1,
                      "title", _(section_title),
                      NULL);

  icon = g_object_new (GTK_TYPE_IMAGE,
                        "visible", 1,
                        "icon_name", "go-next-symbolic",
                        NULL);
  gtk_style_context_add_class (gtk_widget_get_style_context (icon), "dim-label");
  hdy_action_row_add_action (row, icon);

  modified_label = g_object_new (GTK_TYPE_LABEL,
                                  "visible", 1,
                                  NULL);
  gtk_style_context_add_class (gtk_widget_get_style_context (modified_label), "dim-label");
  hdy_action_row_add_action (row, modified_label);

  g_object_set_data_full (G_OBJECT (row),
                          "data",
                          section_row_data_new (section_id, section_title, GTK_LABEL (modified_label)),
                          (GDestroyNotify)section_row_data_free);

  g_hash_table_insert (self->sections, g_strdup (section_id), row);
  gtk_container_add (GTK_CONTAINER (self->section_listbox), GTK_WIDGET (row));

  return row;
}
 
static void
add_item (CcKeyboardShortcutDialog *self,
          CcKeyboardItem  *item,
          const gchar     *section_id,
          const gchar     *section_title)
{
  GtkWidget *row;
  HdyActionRow *section_row;
  SectionRowData *section_data;

  section_row = g_hash_table_lookup (self->sections, section_id);
  if (section_row == NULL)
    section_row = add_section (self, section_id, section_title);

  section_data = g_object_get_data (G_OBJECT (section_row), "data");

  row = GTK_WIDGET (cc_keyboard_shortcut_row_new (item, self->manager, CC_KEYBOARD_SHORTCUT_EDITOR (self->shortcut_editor)));

  g_object_set_data_full (G_OBJECT (row),
                          "data",
                          shortcut_row_data_new (item, section_id, section_title, section_data),
                          (GDestroyNotify)shortcut_row_data_free);

  gtk_container_add (GTK_CONTAINER (self->shortcut_listbox), row);
}

static void
update_modified_counts (CcKeyboardShortcutDialog *self)
{
  GList *l, *sections, *shortcuts;
  SectionRowData *section_data;
  ShortcutRowData *shortcut_data;
  gchar *modified_text;
 
  sections = gtk_container_get_children (GTK_CONTAINER (self->section_listbox));
  shortcuts = gtk_container_get_children (GTK_CONTAINER (self->shortcut_listbox));

  for (l = sections; l != NULL; l = l->next)
    {
      section_data = g_object_get_data (G_OBJECT (l->data), "data");
      section_data->modified_count = 0;
    }

  for (l = shortcuts; l != NULL; l = l->next)
    {
      shortcut_data = g_object_get_data (G_OBJECT (l->data), "data");
      if (!cc_keyboard_item_is_value_default (shortcut_data->item))
        shortcut_data->section_data->modified_count++;
    }

  for (l = sections; l != NULL; l = l->next)
    {
      section_data = g_object_get_data (G_OBJECT (l->data), "data");
      if (section_data->modified_count > 0)
        {
          modified_text = g_strdup_printf ("%d %s", section_data->modified_count, _("modified"));
          gtk_label_set_text (section_data->modified_label, modified_text);
          g_free (modified_text);
        }
      else
        {
          gtk_label_set_text (section_data->modified_label, "");
        }
    }

  g_list_free (shortcuts);
  g_list_free (sections);
}

static void
show_section_list (CcKeyboardShortcutDialog *self)
{
  self->section_row = NULL;

  gtk_stack_set_visible_child (self->stack, GTK_WIDGET (self->section_scrolled_window));
  gtk_header_bar_set_title (self->headerbar, _("Keyboard Shortcuts"));
  gtk_revealer_set_reveal_child (self->reset_all_revealer, TRUE);
  gtk_revealer_set_reveal_child (self->back_revealer, FALSE);
  gtk_revealer_set_reveal_child (self->search_revealer, TRUE);

  update_modified_counts (self);
}

static void
show_shortcut_list (CcKeyboardShortcutDialog *self)
{
  SectionRowData *section_data;
  gchar *title = "";

   if (self->section_row != NULL)
    {
      section_data = g_object_get_data (G_OBJECT (self->section_row), "data");
      title = _(section_data->section_title);
    }
  else
    {
      title = _("Keyboard Shortcuts");
    }

  gtk_list_box_invalidate_filter (self->shortcut_listbox);
  gtk_stack_set_visible_child (self->stack, GTK_WIDGET (self->shortcut_scrolled_window));
  gtk_header_bar_set_title (self->headerbar, title);
  gtk_revealer_set_reveal_child (self->reset_all_revealer, FALSE);
  gtk_revealer_set_reveal_child (self->back_revealer, TRUE);
  gtk_revealer_set_reveal_child (self->search_revealer, self->section_row == NULL);

}

static void
section_row_activated (GtkWidget                *button,
                       GtkListBoxRow            *row,
                       CcKeyboardShortcutDialog *self)
{
  self->section_row = row;
  show_shortcut_list (self);
}

static void
back_button_clicked_cb (CcKeyboardShortcutDialog *self)
{
  show_section_list (self);
}

static void
reset_all_shortcuts_cb (GtkWidget *widget,
                        gpointer   user_data)
{
  CcKeyboardShortcutDialog *self;
  ShortcutRowData *data;

  self = user_data;

  data = g_object_get_data (G_OBJECT (widget), "data");

  /* Don't reset custom shortcuts */
  if (cc_keyboard_item_get_item_type (data->item) == CC_KEYBOARD_ITEM_TYPE_GSETTINGS_PATH)
    return;

  /* cc_keyboard_manager_reset_shortcut() already resets conflicting shortcuts,
   * so no other check is needed here. */
  cc_keyboard_manager_reset_shortcut (self->manager, data->item);
}

static void
reset_all_clicked_cb (CcKeyboardShortcutDialog *self)
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
      gtk_container_foreach (GTK_CONTAINER (self->shortcut_listbox),
                             reset_all_shortcuts_cb,
                             self);
    }

  gtk_widget_destroy (dialog);
}

static void
search_entry_cb (CcKeyboardShortcutDialog *self)
{
  if (gtk_entry_get_text_length (GTK_ENTRY (self->search_entry)) == 0 && self->section_row == NULL)
    show_section_list (self);
  else if (gtk_stack_get_visible_child (self->stack) != GTK_WIDGET (self->shortcut_scrolled_window))
    show_shortcut_list (self);
  else
    gtk_list_box_invalidate_filter (self->shortcut_listbox);
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

static gboolean
shortcut_filter_function (GtkListBoxRow *row,
                          gpointer       userdata)
{
  CcKeyboardShortcutDialog *self = userdata;
  SectionRowData  *section_data;
  ShortcutRowData *data;
  CcKeyboardItem *item;
  gboolean retval;
  g_autofree gchar *search = NULL;
  g_autofree gchar *name = NULL;
  g_auto(GStrv) terms = NULL;
  guint i;

  if (self->section_row != NULL)
  {
    section_data = g_object_get_data (G_OBJECT (self->section_row), "data");
    data = g_object_get_data (G_OBJECT (row), "data");
    if (strcmp (data->section_id, section_data->section_id) != 0)
      return FALSE;
  }

  if (gtk_entry_get_text_length (GTK_ENTRY (self->search_entry)) == 0)
    return TRUE;

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

  return TRUE;
}

static void
shortcut_header_function (GtkListBoxRow *row,
                          GtkListBoxRow *before,
                          gpointer       user_data)
{
  CcKeyboardShortcutDialog *self;
  gboolean add_header;
  ShortcutRowData *data, *before_data;

  data = g_object_get_data (G_OBJECT (row), "data");

  self = user_data;
  add_header = FALSE;

  if (before)
    {
      before_data = g_object_get_data (G_OBJECT (before), "data");
      add_header = g_strcmp0 (before_data->section_id, data->section_id) != 0;
    }
  else
    {
      add_header = TRUE;
    }

  if (self->section_row != NULL)
    add_header = FALSE;

  if (add_header)
    {
      GtkWidget *box, *label, *separator;
      g_autofree gchar *markup = NULL;

      box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
      gtk_widget_show (box);
      gtk_widget_set_margin_top (box, before ? 18 : 6);

      if (before)
        {
          separator = gtk_separator_new (GTK_ORIENTATION_HORIZONTAL);
          gtk_widget_show (separator);
          gtk_container_add (GTK_CONTAINER (box), separator);
        }

      markup = g_strdup_printf ("<b>%s</b>", _(data->section_title));
      label = g_object_new (GTK_TYPE_LABEL,
                            "label", markup,
                            "use-markup", TRUE,
                            "xalign", 0.0,
                            "margin-start", 6,
                            NULL);
      gtk_widget_show (label);
      gtk_container_add (GTK_CONTAINER (box), label);

      separator = gtk_separator_new (GTK_ORIENTATION_HORIZONTAL);
      gtk_widget_show (separator);
      gtk_container_add (GTK_CONTAINER (box), separator);

      gtk_list_box_row_set_header (row, box);
    }
  else
    {
      GtkWidget *separator = gtk_separator_new (GTK_ORIENTATION_HORIZONTAL);
      gtk_widget_show (separator);

      gtk_list_box_row_set_header (row, separator);
    }
}

static void
cc_keyboard_shortcut_dialog_class_init (CcKeyboardShortcutDialogClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/keyboard/cc-keyboard-shortcut-dialog.ui");

  gtk_widget_class_bind_template_child (widget_class, CcKeyboardShortcutDialog, headerbar);
  gtk_widget_class_bind_template_child (widget_class, CcKeyboardShortcutDialog, stack);
  gtk_widget_class_bind_template_child (widget_class, CcKeyboardShortcutDialog, section_listbox);
  gtk_widget_class_bind_template_child (widget_class, CcKeyboardShortcutDialog, shortcut_listbox);
  gtk_widget_class_bind_template_child (widget_class, CcKeyboardShortcutDialog, section_scrolled_window);
  gtk_widget_class_bind_template_child (widget_class, CcKeyboardShortcutDialog, shortcut_scrolled_window);
  gtk_widget_class_bind_template_child (widget_class, CcKeyboardShortcutDialog, back_revealer);
  gtk_widget_class_bind_template_child (widget_class, CcKeyboardShortcutDialog, reset_all_revealer);
  gtk_widget_class_bind_template_child (widget_class, CcKeyboardShortcutDialog, search_revealer);
  gtk_widget_class_bind_template_child (widget_class, CcKeyboardShortcutDialog, search_entry);
  gtk_widget_class_bind_template_child (widget_class, CcKeyboardShortcutDialog, empty_search_placeholder);

  gtk_widget_class_bind_template_callback (widget_class, section_row_activated);
  gtk_widget_class_bind_template_callback (widget_class, back_button_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, reset_all_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, search_entry_cb);
}

static void
cc_keyboard_shortcut_dialog_init (CcKeyboardShortcutDialog *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  self->manager = cc_keyboard_manager_new ();

  self->shortcut_editor = cc_keyboard_shortcut_editor_new (self->manager);

  self->sections = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
  self->section_row = NULL;

  g_signal_connect_object (self->manager,
                           "shortcut-added",
                           G_CALLBACK (add_item),
                           self,
                           G_CONNECT_SWAPPED);

  cc_keyboard_manager_load_shortcuts (self->manager);

  gtk_list_box_set_header_func (self->section_listbox, cc_list_box_update_header_func, NULL, NULL);

  gtk_list_box_set_filter_func (self->shortcut_listbox,
                                shortcut_filter_function,
                                self,
                                NULL);
  gtk_list_box_set_header_func (self->shortcut_listbox,
                                shortcut_header_function,
                                self,
                                NULL);

  gtk_list_box_set_placeholder (GTK_LIST_BOX (self->shortcut_listbox), self->empty_search_placeholder);

  show_section_list (self);
}

GtkWidget*
cc_keyboard_shortcut_dialog_new (void)
{
  return g_object_new (CC_TYPE_KEYBOARD_SHORTCUT_DIALOG,
                       "use-header-bar", 1,
                       NULL);
}