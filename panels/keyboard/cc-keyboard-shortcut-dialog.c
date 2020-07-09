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

#include "cc-keyboard-shortcut-dialog.h"
#include "cc-keyboard-item.h"
#include "cc-keyboard-manager.h"
#include "cc-keyboard-shortcut-editor.h"
#include "cc-keyboard-shortcut-row.h"
#include "cc-list-row.h"
#include "list-box-helper.h"

struct _CcKeyboardShortcutDialog
{
  GtkDialog  parent_instance;

  CcKeyboardManager  *manager;
  GtkWidget          *shortcut_editor;

  GHashTable         *categories;
  gchar              *category;
  GtkListBoxRow      *section_row;

  GtkHeaderBar       *headerbar;
  GtkStack           *stack;
  GtkListBox         *category_listbox;
  GtkListBox         *shortcut_listbox;
  GtkScrolledWindow  *category_scrolled_window;
  GtkScrolledWindow  *shortcut_scrolled_window;
  GtkRevealer        *back_revealer;
  GtkSearchBar       *search_bar;
};

G_DEFINE_TYPE (CcKeyboardShortcutDialog, cc_keyboard_shortcut_dialog, GTK_TYPE_DIALOG)

static void
add_item (CcKeyboardShortcutDialog *self,
          CcKeyboardItem  *item,
          const gchar     *section_id,
          const gchar     *section_title)
{
  GtkWidget *row;
  CcListRow *category_row;

  category_row = g_hash_table_lookup (self->categories, section_id);
  if (category_row == NULL)
    {
      category_row = g_object_new (CC_TYPE_LIST_ROW,
                                   "visible", 1,
                                   "title", section_title,
                                   "icon-name", "go-next-symbolic",
                                   NULL);

      g_object_set_data_full (G_OBJECT (category_row),
                              "section_id",
                              g_strdup (section_id),
                              g_free);
      g_object_set_data_full (G_OBJECT (category_row),
                              "section_title",
                              g_strdup (section_title),
                              g_free);

      g_hash_table_insert (self->categories, g_strdup (section_id), category_row);
      gtk_container_add (GTK_CONTAINER (self->category_listbox), GTK_WIDGET (category_row));
    }

  row = GTK_WIDGET(cc_keyboard_shortcut_row_new(item, self->manager, CC_KEYBOARD_SHORTCUT_EDITOR (self->shortcut_editor)));

  g_object_set_data_full (G_OBJECT (row),
                          "section_id",
                          g_strdup (section_id),
                          g_free);
  g_object_set_data_full (G_OBJECT (row),
                          "section_title",
                          g_strdup (section_title),
                          g_free);

  gtk_container_add (GTK_CONTAINER (self->shortcut_listbox), row);
}

static void
category_row_activated (GtkWidget                *button,
                        GtkListBoxRow            *row,
                        CcKeyboardShortcutDialog *self)
{
  gchar *section_title;

  self->section_row = row; 
  section_title = g_object_get_data (G_OBJECT (row), "section_title");

  gtk_list_box_invalidate_filter (self->shortcut_listbox);
  gtk_stack_set_visible_child (self->stack, GTK_WIDGET (self->shortcut_scrolled_window));
  gtk_header_bar_set_subtitle (self->headerbar, _(section_title));
  gtk_revealer_set_reveal_child (self->back_revealer, TRUE);
}

static void
back_button_clicked_cb (CcKeyboardShortcutDialog *self)
{
  self->section_row = NULL;
  gtk_stack_set_visible_child (self->stack, GTK_WIDGET (self->category_scrolled_window));
  gtk_header_bar_set_subtitle (self->headerbar, "");
  gtk_revealer_set_reveal_child (self->back_revealer, FALSE);
}

static void
reset_all_clicked_cb (CcKeyboardShortcutDialog *self)
{
}

static void
search_entry_cb (CcKeyboardShortcutDialog *self)
{
  gtk_list_box_invalidate_filter (self->shortcut_listbox);
  gtk_stack_set_visible_child (self->stack, GTK_WIDGET (self->shortcut_scrolled_window));
  // TODO
}

static gboolean
shortcut_filter_function (GtkListBoxRow *row,
                          gpointer       userdata)
{
  CcKeyboardShortcutDialog *self = userdata;
  gchar *section_id, *row_section_id;

  if (self->section_row != NULL)
  {
    section_id = g_object_get_data (G_OBJECT (self->section_row), "section_id");
    row_section_id = g_object_get_data (G_OBJECT (row), "section_id");
    if (strcmp (section_id, row_section_id) != 0)
      return FALSE;
  }

  return TRUE;
}

static void
shortcut_header_function (GtkListBoxRow *row,
                          GtkListBoxRow *before,
                          gpointer       user_data)
{
  CcKeyboardShortcutDialog *self;
  gboolean add_header;
  gchar *section_id, *section_title, *before_section_id;

  section_id = g_object_get_data (G_OBJECT (row), "section_id");
  section_title = g_object_get_data (G_OBJECT (row), "section_title");

  self = user_data;
  add_header = FALSE;

  // XXX
  //if (row == self->add_shortcut_row)
  //  {
  //  }
  if (before)
    {
      before_section_id = g_object_get_data (G_OBJECT (before), "section_id");
      add_header = g_strcmp0 (before_section_id, section_id) != 0;
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

      if (before)
        {
          separator = gtk_separator_new (GTK_ORIENTATION_HORIZONTAL);
          gtk_widget_show (separator);
          gtk_container_add (GTK_CONTAINER (box), separator);
        }

      markup = g_strdup_printf ("<b>%s</b>", _(section_title));
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
      GtkWidget *separator = gtk_separator_new (GTK_ORIENTATION_HORIZONTAL);
      gtk_widget_show (separator);

      gtk_list_box_row_set_header (row, separator);
    }
}

static void
cc_keyboard_shortcut_dialog_class_init (CcKeyboardShortcutDialogClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/keyboard/cc-keyboard-shortcut-dialog.ui");

  gtk_widget_class_bind_template_child (widget_class, CcKeyboardShortcutDialog, headerbar);
  gtk_widget_class_bind_template_child (widget_class, CcKeyboardShortcutDialog, stack);
  gtk_widget_class_bind_template_child (widget_class, CcKeyboardShortcutDialog, category_listbox);
  gtk_widget_class_bind_template_child (widget_class, CcKeyboardShortcutDialog, shortcut_listbox);
  gtk_widget_class_bind_template_child (widget_class, CcKeyboardShortcutDialog, category_scrolled_window);
  gtk_widget_class_bind_template_child (widget_class, CcKeyboardShortcutDialog, shortcut_scrolled_window);
  gtk_widget_class_bind_template_child (widget_class, CcKeyboardShortcutDialog, back_revealer);
  gtk_widget_class_bind_template_child (widget_class, CcKeyboardShortcutDialog, search_bar);

  gtk_widget_class_bind_template_callback (widget_class, category_row_activated);
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

  self->categories = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
  self->section_row = NULL;

  g_signal_connect_object (self->manager,
                           "shortcut-added",
                           G_CALLBACK (add_item),
                           self,
                           G_CONNECT_SWAPPED);

  cc_keyboard_manager_load_shortcuts (self->manager);

  gtk_list_box_set_header_func (self->category_listbox, cc_list_box_update_header_func, NULL, NULL);

  gtk_list_box_set_filter_func (self->shortcut_listbox,
                                shortcut_filter_function,
                                self,
                                NULL);
  gtk_list_box_set_header_func (self->shortcut_listbox,
                                shortcut_header_function,
                                self,
                                NULL);

  gtk_stack_set_visible_child (self->stack, GTK_WIDGET (self->category_scrolled_window));
}

GtkWidget*
cc_keyboard_shortcut_dialog_new (void)
{
  return g_object_new (CC_TYPE_KEYBOARD_SHORTCUT_DIALOG,
                       "use-header-bar", 1,
                       NULL);
}