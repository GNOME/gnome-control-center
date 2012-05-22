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

#include <glib.h>
#include <glib/gi18n.h>

#define GNOME_DESKTOP_USE_UNSTABLE_API
#include <libgnome-desktop/gnome-xkb-info.h>

#include "gnome-region-panel-input.h"

#define WID(s) GTK_WIDGET(gtk_builder_get_object (builder, s))

#define GNOME_DESKTOP_INPUT_SOURCES_DIR "org.gnome.desktop.input-sources"

#define KEY_INPUT_SOURCES "sources"

#define INPUT_SOURCE_TYPE_XKB "xkb"

enum {
  NAME_COLUMN,
  TYPE_COLUMN,
  ID_COLUMN,
  N_COLUMNS
};

static GSettings *input_sources_settings = NULL;
static gulong input_sources_settings_changed_id = 0;
static GnomeXkbInfo *xkb_info = NULL;

static GtkWidget *input_chooser_new          (GtkWindow     *main_window,
                                              GtkListStore  *active_sources);
static gboolean   input_chooser_get_selected (GtkWidget     *chooser,
                                              GtkTreeModel **model,
                                              GtkTreeIter   *iter);

static gboolean
add_source_to_table (GtkTreeModel *model,
                     GtkTreePath  *path,
                     GtkTreeIter  *iter,
                     gpointer      data)
{
  GHashTable *hash = data;
  gchar *type;
  gchar *id;

  gtk_tree_model_get (model, iter,
                      TYPE_COLUMN, &type,
                      ID_COLUMN, &id,
                      -1);

  g_hash_table_add (hash, g_strconcat (type, id, NULL));

  g_free (type);
  g_free (id);

  return FALSE;
}

static void
populate_model (GtkListStore *store,
                GtkListStore *active_sources_store)
{
  GHashTable *active_sources_table;
  GtkTreeIter iter;
  const gchar *name;
  GList *sources, *tmp;
  gchar *source_id = NULL;

  active_sources_table = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

  gtk_tree_model_foreach (GTK_TREE_MODEL (active_sources_store),
                          add_source_to_table,
                          active_sources_table);

  sources = gnome_xkb_info_get_all_layouts (xkb_info);

  for (tmp = sources; tmp; tmp = tmp->next)
    {
      g_free (source_id);
      source_id = g_strconcat (INPUT_SOURCE_TYPE_XKB, tmp->data, NULL);

      if (g_hash_table_contains (active_sources_table, source_id))
        continue;

      gnome_xkb_info_get_layout_info (xkb_info, (const gchar *)tmp->data,
                                      &name, NULL, NULL, NULL);

      gtk_list_store_append (store, &iter);
      gtk_list_store_set (store, &iter,
                          NAME_COLUMN, name,
                          TYPE_COLUMN, INPUT_SOURCE_TYPE_XKB,
                          ID_COLUMN, tmp->data,
                          -1);
    }
  g_free (source_id);

  g_list_free (sources);
  g_hash_table_destroy (active_sources_table);
}

static void
populate_with_active_sources (GtkListStore *store)
{
  GVariant *sources;
  GVariantIter iter;
  const gchar *name;
  const gchar *type;
  const gchar *id;
  GtkTreeIter tree_iter;

  sources = g_settings_get_value (input_sources_settings, KEY_INPUT_SOURCES);

  g_variant_iter_init (&iter, sources);
  while (g_variant_iter_next (&iter, "(&s&s)", &type, &id))
    {
      if (!g_str_equal (type, INPUT_SOURCE_TYPE_XKB))
        {
          g_warning ("Unknown input source type '%s'", type);
          continue;
        }

      gnome_xkb_info_get_layout_info (xkb_info, id, &name, NULL, NULL, NULL);

      if (!name)
        {
          g_warning ("Couldn't find XKB input source '%s'", id);
          continue;
        }

      gtk_list_store_append (store, &tree_iter);
      gtk_list_store_set (store, &tree_iter,
                          NAME_COLUMN, name,
                          TYPE_COLUMN, type,
                          ID_COLUMN, id,
                          -1);
    }

  g_variant_unref (sources);
}

static void
update_configuration (GtkTreeModel *model)
{
  GtkTreeIter iter;
  gchar *type;
  gchar *id;
  GVariantBuilder builder;

  g_variant_builder_init (&builder, G_VARIANT_TYPE ("a(ss)"));

  gtk_tree_model_get_iter_first (model, &iter);
  do {
    gtk_tree_model_get (model, &iter,
                        TYPE_COLUMN, &type,
                        ID_COLUMN, &id,
                        -1);
    g_variant_builder_add (&builder, "(ss)", type, id);
    g_free (type);
    g_free (id);
  } while (gtk_tree_model_iter_next (model, &iter));

  g_signal_handler_block (input_sources_settings, input_sources_settings_changed_id);
  g_settings_set_value (input_sources_settings, KEY_INPUT_SOURCES, g_variant_builder_end (&builder));
  g_signal_handler_unblock (input_sources_settings, input_sources_settings_changed_id);
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
  gint n_active;
  gint index;

  remove_button = WID("input_source_remove");
  show_button = WID("input_source_show");
  up_button = WID("input_source_move_up");
  down_button = WID("input_source_move_down");

  tv = GTK_TREE_VIEW (WID ("active_input_sources"));

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
chooser_response (GtkWidget *chooser, gint response_id, gpointer data)
{
  GtkBuilder *builder = data;

  if (response_id == GTK_RESPONSE_OK)
    {
      GtkTreeModel *model;
      GtkTreeIter iter;

      if (input_chooser_get_selected (chooser, &model, &iter))
        {
          GtkTreeView *my_tv;
          GtkListStore *my_model;
          GtkTreeIter my_iter;
          gchar *name;
          gchar *type;
          gchar *id;

          gtk_tree_model_get (model, &iter,
                              NAME_COLUMN, &name,
                              TYPE_COLUMN, &type,
                              ID_COLUMN, &id,
                              -1);

          my_tv = GTK_TREE_VIEW (WID ("active_input_sources"));
          my_model = GTK_LIST_STORE (gtk_tree_view_get_model (my_tv));

          gtk_list_store_append (my_model, &my_iter);

          gtk_list_store_set (my_model, &my_iter,
                              NAME_COLUMN, name,
                              TYPE_COLUMN, type,
                              ID_COLUMN, id,
                              -1);
          g_free (name);
          g_free (type);
          g_free (id);

          gtk_tree_selection_select_iter (gtk_tree_view_get_selection (my_tv), &my_iter);

          update_button_sensitivity (builder);
          update_configuration (GTK_TREE_MODEL (my_model));
        }
      else
        {
          g_debug ("nothing selected, nothing added");
        }
    }

  gtk_widget_destroy (GTK_WIDGET (chooser));
}

static void
add_input (GtkButton *button, gpointer data)
{
  GtkBuilder *builder = data;
  GtkWidget *chooser;
  GtkWidget *toplevel;
  GtkWidget *treeview;
  GtkListStore *active_sources;

  g_debug ("add an input source");

  toplevel = gtk_widget_get_toplevel (WID ("region_notebook"));
  treeview = WID ("active_input_sources");
  active_sources = GTK_LIST_STORE (gtk_tree_view_get_model (GTK_TREE_VIEW (treeview)));

  chooser = input_chooser_new (GTK_WINDOW (toplevel), active_sources);
  g_signal_connect (chooser, "response",
                    G_CALLBACK (chooser_response), builder);
}

static void
remove_selected_input (GtkButton *button, gpointer data)
{
  GtkBuilder *builder = data;
  GtkTreeModel *model;
  GtkTreeIter iter;
  GtkTreePath *path;

  g_debug ("remove selected input source");

  if (get_selected_iter (builder, &model, &iter) == FALSE)
    return;

  path = gtk_tree_model_get_path (model, &iter);
  gtk_list_store_remove (GTK_LIST_STORE (model), &iter);

  if (!gtk_tree_model_get_iter (model, &iter, path))
    gtk_tree_path_prev (path);

  set_selected_path (builder, path);

  gtk_tree_path_free (path);

  update_button_sensitivity (builder);
  update_configuration (model);
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
  set_selected_path (builder, path);
  gtk_tree_path_free (path);

  update_button_sensitivity (builder);
  update_configuration (model);
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
  set_selected_path (builder, path);
  gtk_tree_path_free (path);

  update_button_sensitivity (builder);
  update_configuration (model);
}

static void
show_selected_layout (GtkButton *button, gpointer data)
{
  GtkBuilder *builder = data;
  GtkTreeModel *model;
  GtkTreeIter iter;
  gchar *id;
  gchar *kbd_viewer_args;
  const gchar *xkb_layout;
  const gchar *xkb_variant;

  g_debug ("show selected layout");

  if (!get_selected_iter (builder, &model, &iter))
    return;

  gtk_tree_model_get (model, &iter,
                      ID_COLUMN, &id,
                      -1);

  gnome_xkb_info_get_layout_info (xkb_info, id, NULL, NULL, &xkb_layout, &xkb_variant);

  if (!xkb_layout || !xkb_layout[0])
    {
      g_warning ("Couldn't find XKB input source '%s'", id);
      goto exit;
    }

  if (xkb_variant[0])
    kbd_viewer_args = g_strdup_printf ("gkbd-keyboard-display -l \"%s\t%s\"",
                                       xkb_layout, xkb_variant);
  else
    kbd_viewer_args = g_strdup_printf ("gkbd-keyboard-display -l %s",
                                       xkb_layout);

  g_spawn_command_line_async (kbd_viewer_args, NULL);

  g_free (kbd_viewer_args);
 exit:
  g_free (id);
}

static gboolean
go_to_shortcuts (GtkLinkButton *button,
                 CcRegionPanel *panel)
{
  CcShell *shell;
  const gchar *argv[] = { "shortcuts", NULL };
  GError *error = NULL;

  shell = cc_panel_get_shell (CC_PANEL (panel));
  if (!cc_shell_set_active_panel_from_id (shell, "keyboard", argv, &error))
    {
      g_warning ("Failed to activate Keyboard panel: %s", error->message);
      g_error_free (error);
    }

  return TRUE;
}

static void
input_sources_changed (GSettings  *settings,
                       gchar      *key,
                       GtkBuilder *builder)
{
  GtkWidget *treeview;
  GtkTreeModel *store;

  treeview = WID("active_input_sources");
  store = gtk_tree_view_get_model (GTK_TREE_VIEW (treeview));
  gtk_list_store_clear (GTK_LIST_STORE (store));
  populate_with_active_sources (GTK_LIST_STORE (store));
}

void
setup_input_tabs (GtkBuilder    *builder,
                  CcRegionPanel *panel)
{
  GtkWidget *treeview;
  GtkTreeViewColumn *column;
  GtkCellRenderer *cell;
  GtkListStore *store;
  GtkTreeSelection *selection;
  gchar *previous = NULL;
  gchar *next = NULL;
  GtkWidget *label;

  input_sources_settings = g_settings_new (GNOME_DESKTOP_INPUT_SOURCES_DIR);
  xkb_info = gnome_xkb_info_new ();

  /* set up the list of active inputs */
  treeview = WID("active_input_sources");
  column = gtk_tree_view_column_new ();
  cell = gtk_cell_renderer_text_new ();
  gtk_tree_view_column_pack_start (column, cell, TRUE);
  gtk_tree_view_column_add_attribute (column, cell, "text", NAME_COLUMN);
  gtk_tree_view_append_column (GTK_TREE_VIEW (treeview), column);

  store = gtk_list_store_new (3, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);

  populate_with_active_sources (store);

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

  /* use an em dash is no shortcut */
  if (!previous)
    previous = g_strdup ("\342\200\224");
  if (!next)
    next = g_strdup ("\342\200\224");

  label = WID("prev-source-shortcut-label");
  gtk_label_set_label (GTK_LABEL (label), previous);
  label = WID("next-source-shortcut-label");
  gtk_label_set_label (GTK_LABEL (label), next);

  g_free (previous);
  g_free (next);

  g_signal_connect (WID("jump-to-shortcuts"), "activate-link",
                    G_CALLBACK (go_to_shortcuts), panel);

  input_sources_settings_changed_id = g_signal_connect (G_OBJECT (input_sources_settings),
                                                        "changed::" KEY_INPUT_SOURCES,
                                                        G_CALLBACK (input_sources_changed),
                                                        builder);
}

static void
filter_clear (GtkEntry             *entry,
              GtkEntryIconPosition  icon_pos,
              GdkEvent             *event,
              gpointer              user_data)
{
  gtk_entry_set_text (entry, "");
}

static gchar **search_pattern_list;

static void
filter_changed (GtkBuilder *builder)
{
  GtkTreeModelFilter *filtered_model;
  GtkTreeView *tree_view;
  GtkTreeSelection *selection;
  GtkTreeIter selected_iter;
  GtkWidget *filter_entry;
  const gchar *pattern;
  gchar *upattern;

  filter_entry = WID ("input_source_filter");
  pattern = gtk_entry_get_text (GTK_ENTRY (filter_entry));
  upattern = g_utf8_strup (pattern, -1);
  if (!g_strcmp0 (pattern, ""))
    g_object_set (G_OBJECT (filter_entry),
                  "secondary-icon-name", "edit-find-symbolic",
                  "secondary-icon-activatable", FALSE,
                  "secondary-icon-sensitive", FALSE,
                  NULL);
  else
    g_object_set (G_OBJECT (filter_entry),
                  "secondary-icon-name", "edit-clear-symbolic",
                  "secondary-icon-activatable", TRUE,
                  "secondary-icon-sensitive", TRUE,
                  NULL);

  if (search_pattern_list != NULL)
    g_strfreev (search_pattern_list);

  search_pattern_list = g_strsplit (upattern, " ", -1);
  g_free (upattern);

  filtered_model = GTK_TREE_MODEL_FILTER (gtk_builder_get_object (builder, "filtered_input_source_model"));
  gtk_tree_model_filter_refilter (filtered_model);

  tree_view = GTK_TREE_VIEW (WID ("filtered_input_source_list"));
  selection = gtk_tree_view_get_selection (tree_view);
  if (gtk_tree_selection_get_selected (selection, NULL, &selected_iter))
    {
      GtkTreePath *path = gtk_tree_model_get_path (GTK_TREE_MODEL (filtered_model),
                                                   &selected_iter);
      gtk_tree_view_scroll_to_cell (tree_view, path, NULL, TRUE, 0.5, 0.5);
      gtk_tree_path_free (path);
    }
  else
    {
      GtkTreeIter iter;
      if (gtk_tree_model_get_iter_first (GTK_TREE_MODEL (filtered_model), &iter))
        gtk_tree_selection_select_iter (selection, &iter);
    }
}

static void
selection_changed (GtkTreeSelection *selection,
                   GtkBuilder       *builder)
{
  gtk_widget_set_sensitive (WID ("ok-button"),
                            gtk_tree_selection_get_selected (selection, NULL, NULL));
}

static void
row_activated (GtkTreeView       *tree_view,
               GtkTreePath       *path,
               GtkTreeViewColumn *column,
               GtkBuilder        *builder)
{
  GtkWidget *add_button;
  GtkWidget *dialog;

  add_button = WID ("ok-button");
  dialog = WID ("input_source_chooser");
  if (gtk_widget_is_sensitive (add_button))
    gtk_dialog_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);
}

static void
entry_activated (GtkBuilder *builder,
                 gpointer    data)
{
  row_activated (NULL, NULL, NULL, builder);
}

static gboolean
filter_func (GtkTreeModel *model,
             GtkTreeIter  *iter,
             gpointer      data)
{
  gchar *name = NULL;
  gchar **pattern;
  gboolean rv = TRUE;

  if (search_pattern_list == NULL || search_pattern_list[0] == NULL)
    return TRUE;

  gtk_tree_model_get (model, iter,
                      NAME_COLUMN, &name,
                      -1);

  pattern = search_pattern_list;
  do {
    gboolean is_pattern_found = FALSE;
    gchar *udesc = g_utf8_strup (name, -1);
    if (udesc != NULL && g_strstr_len (udesc, -1, *pattern))
      {
        is_pattern_found = TRUE;
      }
    g_free (udesc);

    if (!is_pattern_found)
      {
        rv = FALSE;
        break;
      }

  } while (*++pattern != NULL);

  g_free (name);

  return rv;
}

static GtkWidget *
input_chooser_new (GtkWindow    *main_window,
                   GtkListStore *active_sources)
{
  GtkBuilder *builder;
  GtkWidget *chooser;
  GtkWidget *filtered_list;
  GtkWidget *filter_entry;
  GtkTreeViewColumn *visible_column;
  GtkTreeSelection *selection;
  GtkListStore *model;
  GtkTreeModelFilter *filtered_model;
  GtkTreeIter iter;

  builder = gtk_builder_new ();
  gtk_builder_add_from_file (builder,
                             GNOMECC_UI_DIR "/gnome-region-panel-input-chooser.ui",
                             NULL);
  chooser = WID ("input_source_chooser");
  g_object_set_data_full (G_OBJECT (chooser), "builder", builder, g_object_unref);

  filtered_list = WID ("filtered_input_source_list");
  filter_entry = WID ("input_source_filter");

  g_object_set_data (G_OBJECT (chooser),
                     "filtered_input_source_list", filtered_list);
  visible_column =
    gtk_tree_view_column_new_with_attributes ("Input Sources",
                                              gtk_cell_renderer_text_new (),
                                              "text", NAME_COLUMN,
                                              NULL);

  gtk_window_set_transient_for (GTK_WINDOW (chooser), main_window);

  gtk_tree_view_append_column (GTK_TREE_VIEW (filtered_list),
                               visible_column);
  /* We handle searching ourselves, thank you. */
  gtk_tree_view_set_enable_search (GTK_TREE_VIEW (filtered_list), FALSE);
  gtk_tree_view_set_search_column (GTK_TREE_VIEW (filtered_list), -1);

  g_signal_connect_swapped (G_OBJECT (filter_entry), "activate",
                            G_CALLBACK (entry_activated), builder);
  g_signal_connect_swapped (G_OBJECT (filter_entry), "notify::text",
                            G_CALLBACK (filter_changed), builder);

  g_signal_connect (G_OBJECT (filter_entry), "icon-release",
                    G_CALLBACK (filter_clear), NULL);

  filtered_model = GTK_TREE_MODEL_FILTER (gtk_builder_get_object (builder, "filtered_input_source_model"));
  model = GTK_LIST_STORE (gtk_builder_get_object (builder, "input_source_model"));

  populate_model (model, active_sources);

  gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (model),
                                        NAME_COLUMN, GTK_SORT_ASCENDING);

  gtk_tree_model_filter_set_visible_func (filtered_model,
                                          filter_func,
                                          NULL, NULL);

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (filtered_list));

  g_signal_connect (G_OBJECT (selection), "changed",
                    G_CALLBACK (selection_changed), builder);

  if (gtk_tree_model_get_iter_first (GTK_TREE_MODEL (filtered_model), &iter))
    gtk_tree_selection_select_iter (selection, &iter);

  g_signal_connect (G_OBJECT (filtered_list), "row-activated",
                    G_CALLBACK (row_activated), builder);

  gtk_widget_grab_focus (filter_entry);

  gtk_widget_show (chooser);

  return chooser;
}

static gboolean
input_chooser_get_selected (GtkWidget     *dialog,
                            GtkTreeModel **model,
                            GtkTreeIter   *iter)
{
  GtkWidget *tv;
  GtkTreeSelection *selection;

  tv = g_object_get_data (G_OBJECT (dialog), "filtered_input_source_list");
  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (tv));

  return gtk_tree_selection_get_selected (selection, model, iter);
}
