/*
 * Copyright (c) 2009, 2010 Intel, Inc.
 * Copyright (c) 2010 Red Hat, Inc.
 *
 * The Control Center is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * The Control Center is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with the Control Center; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Author: Thomas Wood <thos@gnome.org>
 */

#include "config.h"

#include <glib/gi18n.h>
#include <gio/gio.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <string.h>
#define GMENU_I_KNOW_THIS_IS_UNSTABLE
#include <gmenu-tree.h>

#include "cc-panel.h"
#include "cc-shell.h"
#include "shell-search-renderer.h"

#define W(b,x) GTK_WIDGET (gtk_builder_get_object (b, x))

typedef struct
{
  GtkBuilder *builder;
  GtkWidget  *notebook;
  GtkWidget  *window;
  GtkWidget  *search_entry;

  GSList *icon_views;

  gchar  *current_title;
  CcPanel *current_panel;

  GtkListStore *store;
  GtkTreeModel *search_filter;
  GtkCellRenderer *search_renderer;
  gchar *filter_string;

  GHashTable *panels;

} ShellData;

enum
{
  OVERVIEW_PAGE,
  SEARCH_PAGE,
  CAPPLET_PAGE
};

enum
{
  COL_NAME,
  COL_EXEC,
  COL_ID,
  COL_PIXBUF,
  COL_CATEGORY,
  COL_SEARCH_TARGET,

  N_COLS
};

static void item_activated_cb (GtkIconView *icon_view, GtkTreePath *path, ShellData *data);

#ifdef RUN_IN_SOURCE_TREE
static GList *
load_panel_plugins_from_source (void)
{
  GDir *dir;
  GList *list;
  const char *name;

  g_message ("capplets!");

  dir = g_dir_open ("../capplets/", 0, NULL);
  if (dir == NULL)
    return NULL;

  while ((name = g_dir_read_name (dir)) != NULL)
    {
      char *path;
      GList *l;

      path = g_strconcat ("../capplets/", name, "/.libs", NULL);
      g_message ("loading modules in %s", path);
      l = g_io_modules_load_all_in_directory (path);
      g_free (path);

      if (l)
        list = g_list_concat (list, l);
    }
  g_dir_close (dir);

  return list;
}
#endif

static void
load_panel_plugins (ShellData *data)
{
  static volatile GType panel_type = G_TYPE_INVALID;
  static GIOExtensionPoint *ep = NULL;
  GList *modules;
  GList *panel_implementations;
  GList *l;

  /* make sure base type is registered */
  if (panel_type == G_TYPE_INVALID)
    {
      panel_type = g_type_from_name ("CcPanel");
    }

  data->panels = g_hash_table_new_full (g_str_hash, g_str_equal, g_free,
                                        g_object_unref);

  if (ep == NULL)
    {
      g_debug ("Registering extension point");
      ep = g_io_extension_point_register (CC_PANEL_EXTENSION_POINT_NAME);
    }

  /* load all modules */
  g_debug ("Loading all modules in %s", EXTENSIONSDIR);
  modules = g_io_modules_load_all_in_directory (EXTENSIONSDIR);

  g_debug ("Loaded %d modules", g_list_length (modules));

#ifdef RUN_IN_SOURCE_TREE
  if (g_list_length (modules) == 0)
    modules = load_panel_plugins_from_source ();
#endif

  /* find all extensions */
  panel_implementations = g_io_extension_point_get_extensions (ep);
  for (l = panel_implementations; l != NULL; l = l->next)
    {
      GIOExtension *extension;
      CcPanel *panel;
      char *id;

      extension = l->data;

      g_debug ("Found extension: %s %d", g_io_extension_get_name (extension), g_io_extension_get_priority (extension));
      panel = g_object_new (g_io_extension_get_type (extension), NULL);
      g_object_get (panel, "id", &id, NULL);
      g_hash_table_insert (data->panels, g_strdup (id), g_object_ref (panel));
      g_debug ("id: '%s'", id);
      g_free (id);
    }

  /* unload all modules; the module our instantiated authority is in won't be unloaded because
   * we've instantiated a reference to a type in this module
   */
  g_list_foreach (modules, (GFunc) g_type_module_unuse, NULL);
  g_list_free (modules);
}

static gboolean
button_release_cb (GtkWidget      *view,
                   GdkEventButton *event,
                   ShellData      *data)
{
  if (event->button == 1)
    {
      GList *selection;

      selection = gtk_icon_view_get_selected_items (GTK_ICON_VIEW (view));

      if (!selection)
        return FALSE;

      item_activated_cb (GTK_ICON_VIEW (view), selection->data, data);

      g_list_free (selection);
      return TRUE;
    }
  return FALSE;
}

static void
selection_changed_cb (GtkIconView *view,
                      ShellData   *data)
{
  GSList *iconviews, *l;
  GList *selection;

  /* don't clear other selections if this icon view does not have one */
  selection = gtk_icon_view_get_selected_items (view);
  if (!selection)
    return;
  else
    g_list_free (selection);

  iconviews = data->icon_views;

  for (l = iconviews; l; l = l->next)
    {
      GtkIconView *iconview = l->data;

      if (iconview != view)
        {
          if ((selection = gtk_icon_view_get_selected_items (iconview)))
            {
              gtk_icon_view_unselect_all (iconview);
              g_list_free (selection);
            }
        }
    }
}

static gboolean
model_filter_func (GtkTreeModel *model,
                   GtkTreeIter  *iter,
                   ShellData    *data)
{
  gchar *name, *target;
  gchar *needle, *haystack;
  gboolean result;

  gtk_tree_model_get (model, iter, COL_NAME, &name,
                      COL_SEARCH_TARGET, &target, -1);

  if (!data->filter_string || !name || !target)
    {
      g_free (name);
      g_free (target);
      return FALSE;
    }

  needle = g_utf8_casefold (data->filter_string, -1);
  haystack = g_utf8_casefold (target, -1);

  result = (strstr (haystack, needle) != NULL);

  g_free (name);
  g_free (target);
  g_free (haystack);
  g_free (needle);

  return result;
}

static gboolean
category_filter_func (GtkTreeModel *model,
                      GtkTreeIter  *iter,
                      gchar        *filter)
{
  gchar *category;
  gboolean result;

  gtk_tree_model_get (model, iter, COL_CATEGORY, &category, -1);

  result = (g_strcmp0 (category, filter) == 0);

  g_free (category);

  return result;
}

static void
fill_model (ShellData *data)
{
  GSList *list, *l;
  GMenuTreeDirectory *d;
  GMenuTree *tree;
  GtkWidget *vbox, *w;

  vbox = W (data->builder, "main-vbox");

  tree = gmenu_tree_lookup (MENUDIR "/gnomecc.menu", 0);

  if (!tree)
    {
      g_warning ("Could not find control center menu");
      return;
    }

  d = gmenu_tree_get_root_directory (tree);

  list = gmenu_tree_directory_get_contents (d);

  data->store = gtk_list_store_new (N_COLS, G_TYPE_STRING, G_TYPE_STRING,
                                    G_TYPE_STRING, GDK_TYPE_PIXBUF,
                                    G_TYPE_STRING, G_TYPE_STRING);

  data->search_filter = gtk_tree_model_filter_new (GTK_TREE_MODEL (data->store), NULL);

  gtk_tree_model_filter_set_visible_func (GTK_TREE_MODEL_FILTER (data->search_filter),
                                          (GtkTreeModelFilterVisibleFunc)
                                            model_filter_func,
                                          data, NULL);

  w = (GtkWidget *) gtk_builder_get_object (data->builder, "search-view");
  gtk_icon_view_set_model (GTK_ICON_VIEW (w), GTK_TREE_MODEL (data->search_filter));
  gtk_icon_view_set_pixbuf_column (GTK_ICON_VIEW (w), COL_PIXBUF);


  data->search_renderer = (GtkCellRenderer*) shell_search_renderer_new ();
  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (w), data->search_renderer, TRUE);
  gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (w), data->search_renderer,
                                 "title", COL_NAME);
  gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (w), data->search_renderer,
                                 "search-target", COL_SEARCH_TARGET);

  g_signal_connect (w, "item-activated",
                    G_CALLBACK (item_activated_cb), data);
  g_signal_connect (w, "button-release-event",
                    G_CALLBACK (button_release_cb), data);
  g_signal_connect (w, "selection-changed",
                    G_CALLBACK (selection_changed_cb), data);


  for (l = list; l; l = l->next)
    {
      GMenuTreeItemType type;
      type = gmenu_tree_item_get_type (l->data);
      if (type == GMENU_TREE_ITEM_DIRECTORY)
        {
          GtkTreeModel *filter;
          GtkWidget *header, *iconview;
          GSList *foo, *f;
          const gchar *dir_name;
          gchar *header_name;

          foo = gmenu_tree_directory_get_contents (l->data);
          dir_name = gmenu_tree_directory_get_name (l->data);

          filter = gtk_tree_model_filter_new (GTK_TREE_MODEL (data->store), NULL);
          gtk_tree_model_filter_set_visible_func (GTK_TREE_MODEL_FILTER (filter),
                                                  (GtkTreeModelFilterVisibleFunc) category_filter_func,
                                                  g_strdup (dir_name), g_free);

          iconview = gtk_icon_view_new_with_model (GTK_TREE_MODEL (filter));

          gtk_icon_view_set_pixbuf_column (GTK_ICON_VIEW (iconview), COL_PIXBUF);
          gtk_icon_view_set_text_column (GTK_ICON_VIEW (iconview), COL_NAME);
          gtk_icon_view_set_item_width (GTK_ICON_VIEW (iconview), 120);
          g_signal_connect (iconview, "item-activated",
                            G_CALLBACK (item_activated_cb), data);
          g_signal_connect (iconview, "button-release-event",
                            G_CALLBACK (button_release_cb), data);
          g_signal_connect (iconview, "selection-changed",
                            G_CALLBACK (selection_changed_cb), data);

          data->icon_views = g_slist_prepend (data->icon_views, iconview);

          header_name = g_strdup_printf ("<b>%s</b>", dir_name);

          header = g_object_new (GTK_TYPE_LABEL,
                                 "use-markup", TRUE,
                                 "label", header_name,
                                 "wrap", TRUE,
                                 "xalign", 0.0,
                                 "xpad", 6,
                                 NULL);

          gtk_box_pack_start (GTK_BOX (vbox), header, FALSE, TRUE, 3);
          gtk_box_pack_start (GTK_BOX (vbox), iconview, FALSE, TRUE, 0);


          for (f = foo; f; f = f->next)
            {
              if (gmenu_tree_item_get_type (f->data)
                  == GMENU_TREE_ITEM_ENTRY)
                {
                  GError *err = NULL;
                  gchar *search_target;
                  const gchar *icon = gmenu_tree_entry_get_icon (f->data);
                  const gchar *name = gmenu_tree_entry_get_name (f->data);
                  const gchar *id = gmenu_tree_entry_get_desktop_file_id (f->data);
                  const gchar *exec = gmenu_tree_entry_get_exec (f->data);
                  const gchar *comment = gmenu_tree_entry_get_comment (f->data);
                  GdkPixbuf *pixbuf = NULL;
                  char *icon2 = NULL;

                  if (icon != NULL && *icon == '/')
                    {
                      pixbuf = gdk_pixbuf_new_from_file_at_scale (icon, 32, 32, TRUE, &err);
                    }
                  else
                    {
                      if (icon2 == NULL && icon != NULL && g_str_has_suffix (icon, ".png"))
                        icon2 = g_strndup (icon, strlen (icon) - strlen (".png"));

                      pixbuf = gtk_icon_theme_load_icon (gtk_icon_theme_get_default (),
                                                         icon2 ? icon2 : icon, 32,
                                                         GTK_ICON_LOOKUP_FORCE_SIZE,
                                                         &err);
                    }

                  if (err)
                    {
                      g_warning ("Could not load icon '%s': %s", icon2 ? icon2 : icon,
                                       err->message);
                      g_error_free (err);
                    }

                  g_free (icon2);

                  search_target = g_strconcat (name, " - ", comment, NULL);

                  gtk_list_store_insert_with_values (data->store, NULL, 0,
                                                     COL_NAME, name,
                                                     COL_EXEC, exec,
                                                     COL_ID, id,
                                                     COL_PIXBUF, pixbuf,
                                                     COL_CATEGORY, dir_name,
                                                     COL_SEARCH_TARGET, search_target,
                                                     -1);

                  g_free (search_target);
                }
            }
        }
    }

}

static void
activate_panel (const gchar *id,
                const gchar *exec,
                ShellData   *data)
{
  CcPanel *panel;

  /* first look for a panel module */
  panel = g_hash_table_lookup (data->panels, id);
  if (panel != NULL)
    {
      data->current_panel = panel;
      gtk_container_set_border_width (GTK_CONTAINER (panel), 12);
      gtk_widget_show_all (GTK_WIDGET (panel));
      cc_panel_set_active (panel, TRUE);

      gtk_notebook_insert_page (GTK_NOTEBOOK (data->notebook), GTK_WIDGET (panel),
                                NULL, CAPPLET_PAGE);

      gtk_notebook_set_current_page (GTK_NOTEBOOK (data->notebook), CAPPLET_PAGE);

      gtk_label_set_text (GTK_LABEL (gtk_builder_get_object (data->builder,
                                                             "label-title")),
                          data->current_title);

      gtk_widget_show (GTK_WIDGET (gtk_builder_get_object (data->builder,
                                                           "title-alignment")));
    }
  else
    {
      /* start app directly */
      g_debug ("Panel module not found for %s", id);
      g_spawn_command_line_async (exec, NULL);
    }

}

static void
item_activated_cb (GtkIconView *icon_view,
                   GtkTreePath *path,
                   ShellData   *data)
{
  GtkTreeModel *model;
  GtkTreeIter iter;
  gchar *name, *exec, *id;

  /* get exec */
  model = gtk_icon_view_get_model (icon_view);

  /* get the iter and ensure it is valid */
  if (!gtk_tree_model_get_iter (model, &iter, path))
    return;

  gtk_tree_model_get (model, &iter, COL_NAME, &name, COL_EXEC, &exec, COL_ID, &id, -1);

  g_debug ("activated id: '%s'", id);

  g_free (data->current_title);
  data->current_title = name;

  activate_panel (id, exec, data);

  g_free (id);
  g_free (exec);
}

static void
shell_show_overview_page (ShellData *data)
{
  gtk_notebook_set_current_page (GTK_NOTEBOOK (data->notebook), OVERVIEW_PAGE);

  gtk_notebook_remove_page (GTK_NOTEBOOK (data->notebook), CAPPLET_PAGE);

  if (data->current_panel != NULL)
    cc_panel_set_active (data->current_panel, FALSE);

  gtk_label_set_text (GTK_LABEL (gtk_builder_get_object (data->builder, "label-title")), "");
  gtk_widget_hide (GTK_WIDGET (gtk_builder_get_object (data->builder, "title-alignment")));

  /* clear the search text */
  g_free (data->filter_string);
  data->filter_string = g_strdup ("");
  gtk_entry_set_text (GTK_ENTRY (data->search_entry), "");
}

static void
home_button_clicked_cb (GtkButton *button,
                        ShellData *data)
{
  shell_show_overview_page (data);
}

static void
search_entry_changed_cb (GtkEntry  *entry,
                         ShellData *data)
{

  /* if the entry text was set manually (not by the user) */
  if (!g_strcmp0 (data->filter_string, gtk_entry_get_text (entry)))
    return;

  g_free (data->filter_string);
  data->filter_string = g_strdup (gtk_entry_get_text (entry));

  g_object_set (data->search_renderer,
                "search-string", data->filter_string,
                NULL);

  if (!g_strcmp0 (data->filter_string, ""))
    {
      shell_show_overview_page (data);
    }
  else
    {
      gtk_tree_model_filter_refilter (GTK_TREE_MODEL_FILTER (data->search_filter));
      gtk_notebook_set_current_page (GTK_NOTEBOOK (data->notebook), SEARCH_PAGE);
    }
}

static gboolean
search_entry_key_press_event_cb (GtkEntry    *entry,
                                 GdkEventKey *event,
                                 ShellData   *data)
{
  if (event->keyval == GDK_Return)
    {
      GtkTreePath *path;

      path = gtk_tree_path_new_first ();
      item_activated_cb ((GtkIconView *) gtk_builder_get_object (data->builder,
                                                                 "search-view"),
                         path, data);
      gtk_tree_path_free (path);
      return TRUE;
    }

  if (event->keyval == GDK_Escape)
    {
      gtk_entry_set_text (entry, "");
      return TRUE;
    }

  return FALSE;
}

static void
notebook_switch_page_cb (GtkNotebook     *book,
                         GtkNotebookPage *page,
                         gint             page_num,
                         ShellData       *data)
{
  /* make sure the home button is shown on all pages except the overview page */

  if (page_num == OVERVIEW_PAGE)
    gtk_widget_hide (W (data->builder, "home-button"));
  else
    gtk_widget_show (W (data->builder, "home-button"));
}

static void
search_entry_clear_cb (GtkEntry *entry)
{
  gtk_entry_set_text (entry, "");
}


int
main (int argc, char **argv)
{
  ShellData *data;
  GtkWidget *widget;

  g_thread_init (NULL);
  gtk_init (&argc, &argv);

  data = g_new0 (ShellData, 1);

  data->builder = (GtkBuilder*) cc_shell_new ();

  if (!data->builder)
    {
      g_critical ("Could not build interface");
      return 1;
    }


  data->window = W (data->builder, "main-window");
  g_signal_connect (data->window, "delete-event", G_CALLBACK (gtk_main_quit),
                    NULL);

  data->notebook = W (data->builder, "notebook");

  g_signal_connect (data->notebook, "switch-page",
                    G_CALLBACK (notebook_switch_page_cb), data);

  fill_model (data);


  g_signal_connect (gtk_builder_get_object (data->builder, "home-button"),
                    "clicked", G_CALLBACK (home_button_clicked_cb), data);

  widget = (GtkWidget*) gtk_builder_get_object (data->builder, "search-entry");
  data->search_entry = widget;

  g_signal_connect (widget, "changed", G_CALLBACK (search_entry_changed_cb),
                    data);
  g_signal_connect (widget, "key-press-event",
                    G_CALLBACK (search_entry_key_press_event_cb), data);

  g_signal_connect (widget, "icon-release", G_CALLBACK (search_entry_clear_cb), data);

  load_panel_plugins (data);

  gtk_widget_show_all (data->window);

  if (argc == 2)
    {
      GtkTreeIter iter;
      gboolean iter_valid;
      gchar *start_id;
      gchar *name;

      start_id = argv[1];

      iter_valid = gtk_tree_model_get_iter_first (GTK_TREE_MODEL (data->store),
                                                  &iter);

      while (iter_valid)
        {
          gchar *id;

          /* find the details for this item */
          gtk_tree_model_get (GTK_TREE_MODEL (data->store), &iter,
                              COL_NAME, &name,
                              COL_ID, &id,
                              -1);
          if (id && !strcmp (id, start_id))
            {
              g_free (id);
              break;
            }
          else
            {
              g_free (id);
              g_free (name);

              name = NULL;
              id = NULL;
            }

          iter_valid = gtk_tree_model_iter_next (GTK_TREE_MODEL (data->store),
                                                 &iter);
        }

      g_free (data->current_title);
      data->current_title = name;

      activate_panel (start_id, start_id, data);
    }

  gtk_main ();

  g_free (data->filter_string);
  g_free (data->current_title);
  g_hash_table_destroy (data->panels);
  g_free (data);

  return 0;
}
