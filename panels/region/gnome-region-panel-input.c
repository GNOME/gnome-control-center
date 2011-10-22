/*
 * Copyright (C) 2011 Red Hat, Inc.
 *
 * Written by: Matthias Clasen <mclasen@redhat.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

#include <config.h>

#include <string.h>

#include <ibus.h>

#include "gnome-region-panel-input.h"

#define WID(s) GTK_WIDGET(gtk_builder_get_object (builder, s))


/* another ibus bus: ibusutil.h is not included in ibus.h */
#define IBUS_COMPILATION
#include <ibusutil.h>
#undef IBUS_COMPILATION

static void
get_active_engines (GtkListStore *store)
{
  IBusBus *bus;
  GList *list, *l;
  IBusEngineDesc *desc;
  GType type;
  const gchar *name;
  GtkTreeIter iter;

  bus = ibus_bus_new ();

  /* work around a bug in IBus which forgets to register
   * its types properly
   */
  type = ibus_engine_desc_get_type ();

  list = ibus_bus_list_active_engines (bus);

  for (l = list; l; l = l->next)
    {
      desc = (IBusEngineDesc *)l->data;

      name = ibus_engine_desc_get_name (desc);
      if (g_str_has_prefix (name, "xkb:layout:default:#"))
        continue;

      gtk_list_store_append (store, &iter);
      gtk_list_store_set (store, &iter,
                          0, name,
                          1, ibus_get_language_name (ibus_engine_desc_get_language (desc)),
                          2, ibus_engine_desc_get_layout (desc),
                          3, g_str_has_prefix (name, "xkb:layout:"),
                          -1);
    }

  g_list_free (list);
}

static gboolean
get_selected_iter (GtkBuilder    *builder,
                   GtkTreeModel **model,
                   GtkTreeIter   *iter)
{
  GtkTreeSelection *selection;

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (WID ("active_input_sources")));

  return gtk_tree_selection_get_selected (selection, model, iter);
}

static gint
find_selected_layout_idx (GtkBuilder *builder)
{
  GtkTreeIter iter;
  GtkTreeModel *model;
  GtkTreePath *path;
  gint idx;

  if (!get_selected_iter (builder, &model, &iter))
    return -1;

  path = gtk_tree_model_get_path (model, &iter);
  if (path == NULL)
    return -1;

  idx = gtk_tree_path_get_indices (path)[0];
  gtk_tree_path_free (path);

  return idx;
}

static void
update_button_sensitivity (GtkBuilder *builder)
{
  GtkWidget *remove_button;
  GtkWidget *up_button;
  GtkWidget *down_button;
  GtkWidget *show_button;
  GtkTreeView *tv;
  GtkTreeSelection *selection;
  gint n_active;
  gint index;

  remove_button = WID("input_source_remove");
  show_button = WID("input_source_show");
  up_button = WID("input_source_move_up");
  down_button = WID("input_source_move_down");

  tv = GTK_TREE_VIEW (WID ("active_input_sources"));
  selection = gtk_tree_view_get_selection (tv);

  n_active = gtk_tree_model_iter_n_children (gtk_tree_view_get_model (tv), NULL);
  index = find_selected_layout_idx (builder);

  gtk_widget_set_sensitive (remove_button, index >= 0 && n_active > 1);
  gtk_widget_set_sensitive (show_button, index >= 0);
  gtk_widget_set_sensitive (up_button, index > 0);
  gtk_widget_set_sensitive (down_button, index >= 0 && index < n_active - 1);
}

static void
set_selected_path (GtkBuilder  *builder,
                   GtkTreePath *path)
{
  GtkTreeSelection *selection;

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (WID ("active_input_sources")));

  gtk_tree_selection_select_path (selection, path);
}

static void
add_input (GtkButton *button, gpointer data)
{
  GtkBuilder *builder = data;

  g_debug ("add an input source");
}

static void
remove_selected_input (GtkButton *button, gpointer data)
{
  GtkBuilder *builder = data;
  GtkTreeModel *model;
  GtkTreeIter iter;

  g_debug ("remove selected input source");

  if (get_selected_iter (builder, &model, &iter) == FALSE)
    return;

  gtk_list_store_remove (GTK_LIST_STORE (model), &iter);
  update_button_sensitivity (builder);
  /* update_layouts_list (model, dialog); */
}

static void
move_selected_input_up (GtkButton *button, gpointer data)
{
  GtkBuilder *builder = data;
  GtkTreeModel *model;
  GtkTreeIter iter, prev;
  GtkTreePath *path;

  g_debug ("move selected input source up");

  if (!get_selected_iter (builder, &model, &iter))
    return;

  prev = iter;
  if (!gtk_tree_model_iter_previous (model, &prev))
    return;

  path = gtk_tree_model_get_path (model, &prev);

  gtk_list_store_swap (GTK_LIST_STORE (model), &iter, &prev);
  update_button_sensitivity (builder);
  /* update_layouts_list (model, dialog); */
  set_selected_path (builder, path);

  gtk_tree_path_free (path);
}

static void
move_selected_input_down (GtkButton *button, gpointer data)
{
  GtkBuilder *builder = data;
  GtkTreeModel *model;
  GtkTreeIter iter, next;
  GtkTreePath *path;

  g_debug ("move selected input source down");

  if (!get_selected_iter (builder, &model, &iter))
    return;

  next = iter;
  if (!gtk_tree_model_iter_next (model, &next))
    return;

  path = gtk_tree_model_get_path (model, &next);

  gtk_list_store_swap (GTK_LIST_STORE (model), &iter, &next);
  update_button_sensitivity (builder);
  /* update_layouts_list (model, dialog); */
  set_selected_path (builder, path);

  gtk_tree_path_free (path);
}

static void
show_selected_layout (GtkButton *button, gpointer data)
{
  GtkBuilder *builder = data;

  g_debug ("show selected layout");
}

void
setup_input_tabs (GtkBuilder *builder)
{
  GtkWidget *treeview;
  GtkTreeViewColumn *column;
  GtkCellRenderer *cell;
  GtkListStore *store;
  GtkTreeSelection *selection;

  /* set up the list of active inputs */
  treeview = WID("active_input_sources");
  column = gtk_tree_view_column_new ();
  cell = gtk_cell_renderer_text_new ();
  gtk_tree_view_column_pack_start (column, cell, TRUE);
  gtk_tree_view_column_add_attribute (column, cell, "text", 1);
  gtk_tree_view_append_column (GTK_TREE_VIEW (treeview), column);

  /* id, name, layout, xkb? */
  store = gtk_list_store_new (4, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_BOOLEAN);

  get_active_engines (store);

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview));
  g_signal_connect_swapped (selection, "changed",
                            G_CALLBACK (update_button_sensitivity), builder);

  gtk_tree_view_set_model (GTK_TREE_VIEW (treeview), GTK_TREE_MODEL (store));

  /* set up the buttons */
  g_signal_connect (WID("input_source_add"), "clicked",
                    G_CALLBACK (add_input), builder);
  g_signal_connect (WID("input_source_remove"), "clicked",
                    G_CALLBACK (remove_selected_input), builder);
  g_signal_connect (WID("input_source_move_up"), "clicked",
                    G_CALLBACK (move_selected_input_up), builder);
  g_signal_connect (WID("input_source_move_down"), "clicked",
                    G_CALLBACK (move_selected_input_down), builder);
  g_signal_connect (WID("input_source_show"), "clicked",
                    G_CALLBACK (show_selected_layout), builder);
}
