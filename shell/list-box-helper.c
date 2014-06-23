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

void
cc_list_box_update_header_func (GtkListBoxRow *row,
                                GtkListBoxRow *before,
                                gpointer user_data)
{
  GtkWidget *current;

  if (before == NULL)
    return;

  current = gtk_list_box_row_get_header (row);
  if (current == NULL)
    {
      current = gtk_separator_new (GTK_ORIENTATION_HORIZONTAL);
      gtk_widget_show (current);
      gtk_list_box_row_set_header (row, current);
    }
}

void
cc_list_box_adjust_scrolling (GtkScrolledWindow *scrolled_window)
{
  GtkWidget *listbox;
  GtkWidget *parent;
  GList *children;
  guint n_rows;

  listbox = gtk_bin_get_child (GTK_BIN (scrolled_window));
  parent = gtk_widget_get_parent (GTK_WIDGET (scrolled_window));
  children = gtk_container_get_children (GTK_CONTAINER (listbox));
  n_rows = g_list_length (children);
  g_list_free (children);

  if (n_rows >= MAX_ROWS_VISIBLE)
    {
      gint height;

      gtk_widget_get_preferred_height (parent, NULL, &height);
      gtk_widget_set_size_request (parent, -1, height);

      gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window),
                                      GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    }
  else
    {
      gtk_widget_set_size_request (parent, -1, -1);
      gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window),
                                      GTK_POLICY_NEVER, GTK_POLICY_NEVER);
    }
}
