/*
 * Copyright (C) 2014 Red Hat, Inc
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
 */

#include "list-box-helper.h"

#define MAX_ROWS_VISIBLE 5

struct _CcListBoxRow
{
  GtkListBoxRow parent_instance;
};
G_DEFINE_TYPE (CcListBoxRow, cc_list_box_row, GTK_TYPE_LIST_BOX_ROW)
enum
{
  BOX_ROW_ACTIVATED,
  LAST_BOX_ROW_SIGNAL
};
static guint cc_list_box_row_signals[LAST_BOX_ROW_SIGNAL] = { 0 };
static void
cc_list_box_row_class_init (CcListBoxRowClass *klass)
{
  cc_list_box_row_signals[BOX_ROW_ACTIVATED] =
    g_signal_new ("activated",
                  CC_TYPE_LIST_BOX_ROW,
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL,
                  G_TYPE_NONE, 0);
}
static void cc_list_box_row_init (CcListBoxRow *self) {}

struct _CcListBox
{
  GtkListBox parent_instance;
};
G_DEFINE_TYPE (CcListBox, cc_list_box, GTK_TYPE_LIST_BOX)
static void
cc_list_box_row_activated (GtkListBox *box, GtkListBoxRow *row)
{
  if (CC_IS_LIST_BOX_ROW (row))
    g_signal_emit (row, cc_list_box_row_signals[BOX_ROW_ACTIVATED], 0);
}
static void
cc_list_box_class_init (CcListBoxClass *klass)
{
  GtkListBoxClass *parent_class = GTK_LIST_BOX_CLASS (klass);
  parent_class->row_activated = cc_list_box_row_activated;
}
static void cc_list_box_init (CcListBox *self) {}


void
cc_list_box_update_header_func (GtkListBoxRow *row,
                                GtkListBoxRow *before,
                                gpointer user_data)
{
  GtkWidget *current;

  if (before == NULL)
    {
      gtk_list_box_row_set_header (row, NULL);
      return;
    }

  current = gtk_list_box_row_get_header (row);
  if (current == NULL)
    {
      current = gtk_separator_new (GTK_ORIENTATION_HORIZONTAL);
      gtk_widget_show (current);
      gtk_list_box_row_set_header (row, current);
    }
}

void
cc_list_box_adjust_scrolling (GtkListBox *listbox)
{
  GtkWidget *scrolled_window;
  g_autoptr(GList) children = NULL;
  guint n_rows, num_max_rows;

  scrolled_window = g_object_get_data (G_OBJECT (listbox), "cc-scrolling-scrolled-window");
  if (!scrolled_window)
    return;

  children = gtk_container_get_children (GTK_CONTAINER (listbox));
  n_rows = g_list_length (children);

  num_max_rows = GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (listbox), "cc-max-rows-visible"));

  if (n_rows >= num_max_rows)
    {
      gint total_row_height = 0;
      GList *l;
      guint i;

      for (l = children, i = 0; l != NULL && i < num_max_rows; l = l->next, i++) {
        gint row_height;
        gtk_widget_get_preferred_height (GTK_WIDGET (l->data), &row_height, NULL);
        total_row_height += row_height;
      }

      gtk_scrolled_window_set_min_content_height (GTK_SCROLLED_WINDOW (scrolled_window), total_row_height);
      gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window),
                                      GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    }
  else
    {
      gtk_scrolled_window_set_min_content_height (GTK_SCROLLED_WINDOW (scrolled_window), -1);
      gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window),
                                      GTK_POLICY_NEVER, GTK_POLICY_NEVER);
    }
}

void
cc_list_box_setup_scrolling (GtkListBox *listbox,
                             guint       num_max_rows)
{
  GtkWidget *parent;
  GtkWidget *scrolled_window;

  parent = gtk_widget_get_parent (GTK_WIDGET (listbox));
  scrolled_window = gtk_scrolled_window_new (NULL, NULL);
  gtk_widget_show (scrolled_window);

  g_object_ref (listbox);
  gtk_container_remove (GTK_CONTAINER (parent), GTK_WIDGET (listbox));
  gtk_container_add (GTK_CONTAINER (scrolled_window), GTK_WIDGET (listbox));
  g_object_unref (listbox);

  gtk_container_add (GTK_CONTAINER (parent), scrolled_window);

  if (num_max_rows == 0)
    num_max_rows = MAX_ROWS_VISIBLE;

  g_object_set_data (G_OBJECT (listbox), "cc-scrolling-scrolled-window", scrolled_window);
  g_object_set_data (G_OBJECT (listbox), "cc-max-rows-visible", GUINT_TO_POINTER (num_max_rows));
}
