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

#include <gio/gio.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <string.h>
#define GMENU_I_KNOW_THIS_IS_UNSTABLE
#include <gnome-menus/gmenu-tree.h>

#include "cc-panel.h"

#define W(b,x) GTK_WIDGET (gtk_builder_get_object (b, x))

typedef struct
{
  GtkBuilder *builder;
  GtkWidget  *notebook;
  GtkWidget  *window;

  GSList *icon_views;

  gchar  *current_title;

  GtkListStore *store;
  GtkTreeModel *filter;
  gchar *filter_string;

} ShellData;

void item_activated_cb (GtkIconView *icon_view, GtkTreePath *path, ShellData *data);

static GHashTable *panels = NULL;

static void
load_panel_plugins (void)
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

  panels = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_object_unref);

  if (ep == NULL)
    {
      g_debug ("Registering extension point");
      ep = g_io_extension_point_register (CC_PANEL_EXTENSION_POINT_NAME);
    }

  /* load all modules */
  g_debug ("Loading all modules in %s", EXTENSIONSDIR);
  modules = g_io_modules_load_all_in_directory (EXTENSIONSDIR);

  g_debug ("Loaded %d modules", g_list_length (modules));

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
      g_hash_table_insert (panels, g_strdup (id), g_object_ref (panel));
      g_debug ("id: '%s'", id);
      g_free (id);
    }

  /* unload all modules; the module our instantiated authority is in won't be unloaded because
   * we've instantiated a reference to a type in this module
   */
  g_list_foreach (modules, (GFunc) g_type_module_unuse, NULL);
  g_list_free (modules);
}

gboolean
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

void
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

gboolean
model_filter_func (GtkTreeModel *model,
                   GtkTreeIter  *iter,
                   ShellData    *data)
{
  gchar *name;
  gchar *needle, *haystack;

  gtk_tree_model_get (model, iter, 0, &name, -1);

  if (!data->filter_string)
    return FALSE;

  if (!name)
    return FALSE;

  needle = g_utf8_casefold (data->filter_string, -1);
  haystack = g_utf8_casefold (name, -1);

  if (strstr (haystack, needle))
    return TRUE;
  else
    return FALSE;
}

void
fill_model (ShellData *data)
{
  GSList *list, *l;
  GMenuTreeDirectory *d;
  GMenuTree *t;
  GtkWidget *vbox, *w;

  vbox = W (data->builder, "main-vbox");

  t = gmenu_tree_lookup (MENUDIR "/gnomecc.menu", 0);

  d = gmenu_tree_get_root_directory (t);

  list = gmenu_tree_directory_get_contents (d);

  data->store = gtk_list_store_new (3, G_TYPE_STRING, G_TYPE_STRING,
                                    GDK_TYPE_PIXBUF);
  data->filter = gtk_tree_model_filter_new (GTK_TREE_MODEL (data->store),
                                            NULL);
  gtk_tree_model_filter_set_visible_func (GTK_TREE_MODEL_FILTER (data->filter),
                                          (GtkTreeModelFilterVisibleFunc)
                                            model_filter_func,
                                          data, NULL);
  w = (GtkWidget *) gtk_builder_get_object (data->builder, "search-view");
  gtk_icon_view_set_model (GTK_ICON_VIEW (w), GTK_TREE_MODEL (data->filter));
  gtk_icon_view_set_pixbuf_column (GTK_ICON_VIEW (w), 2);
  gtk_icon_view_set_text_column (GTK_ICON_VIEW (w), 0);
  gtk_icon_view_set_item_width (GTK_ICON_VIEW (w), 120);
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
          GtkListStore *store;
          GtkWidget *header, *iconview;
          GSList *foo, *f;
          const gchar *dir_name;
          gchar *header_name;

          foo = gmenu_tree_directory_get_contents (l->data);
          dir_name = gmenu_tree_directory_get_name (l->data);

          store = gtk_list_store_new (4,
                                      G_TYPE_STRING,
                                      G_TYPE_STRING,
                                      G_TYPE_STRING,
                                      GDK_TYPE_PIXBUF);

          iconview = gtk_icon_view_new_with_model (GTK_TREE_MODEL (store));
          gtk_icon_view_set_pixbuf_column (GTK_ICON_VIEW (iconview), 3);
          gtk_icon_view_set_text_column (GTK_ICON_VIEW (iconview), 0);
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
                  const gchar *icon = gmenu_tree_entry_get_icon (f->data);
                  const gchar *name = gmenu_tree_entry_get_name (f->data);
                  const gchar *id = gmenu_tree_entry_get_desktop_file_id (f->data);
                  const gchar *exec = gmenu_tree_entry_get_exec (f->data);
                  GdkPixbuf *pixbuf = NULL;

                  pixbuf = gtk_icon_theme_load_icon (gtk_icon_theme_get_default (),
                                                     icon, 32,
                                                     GTK_ICON_LOOKUP_FORCE_SIZE,
                                                     NULL);

                  if (err)
                    {
                      g_warning ("Could not load icon: %s", err->message);
                      g_error_free (err);
                    }

                  gtk_list_store_insert_with_values (store, NULL, 0,
                                                     0, name,
                                                     1, exec,
                                                     2, id,
                                                     3, pixbuf,
                                                     -1);

                  gtk_list_store_insert_with_values (data->store, NULL, 0,
                                                     0, name,
                                                     1, exec,
                                                     2, pixbuf,
                                                     -1);
                }
            }
        }
    }

}

void
item_activated_cb (GtkIconView *icon_view,
                   GtkTreePath *path,
                   ShellData   *data)
{
  GtkTreeModel *model;
  GtkTreeIter iter = {0,};
  gchar *name, *exec, *id, *markup;
  GtkWidget *notebook;
  static gint index = -1;
  CcPanel *panel;

  notebook = data->notebook;
  if (index >= 0)
    gtk_notebook_remove_page (GTK_NOTEBOOK (notebook), index);

  /* get exec */
  model = gtk_icon_view_get_model (icon_view);

  gtk_tree_model_get_iter (model, &iter, path);

  gtk_tree_model_get (model, &iter, 0, &name, 1, &exec, 2, &id, -1);

  g_debug ("activated id: '%s'", id);

  g_free (data->current_title);
  data->current_title = name;

  /* first look for a panel module */
  panel = g_hash_table_lookup (panels, id);
  if (panel != NULL)
    {
      gtk_container_set_border_width (GTK_CONTAINER (panel), 12);
      index = gtk_notebook_append_page (GTK_NOTEBOOK (notebook), GTK_WIDGET (panel), NULL);
      gtk_widget_show_all (GTK_WIDGET (panel));
      gtk_notebook_set_current_page (GTK_NOTEBOOK (notebook), index);
      gtk_widget_show (W (data->builder, "home-button"));
      gtk_window_set_title (GTK_WINDOW (data->window), data->current_title);
    }
  else
    {
      /* start app directly */
      g_debug ("Panel module not found for %s", id);
      g_spawn_command_line_async (exec, NULL);
    }

  g_free (id);
  g_free (exec);
}

static void
home_button_clicked_cb (GtkButton *button,
                        ShellData *data)
{
  int        page;
  GtkWidget *widget;

  page = gtk_notebook_get_current_page (GTK_NOTEBOOK (data->notebook));
  gtk_notebook_set_current_page (GTK_NOTEBOOK (data->notebook), 0);
  widget = gtk_notebook_get_nth_page (GTK_NOTEBOOK (data->notebook), page);
  gtk_widget_hide (widget);
  gtk_notebook_remove_page (GTK_NOTEBOOK (data->notebook), page);

  gtk_window_set_title (GTK_WINDOW (data->window), "System Settings");

  gtk_widget_hide (GTK_WIDGET (button));
}

void
search_entry_changed_cb (GtkEntry  *entry,
                         ShellData *data)
{
  g_free (data->filter_string);
  data->filter_string = g_strdup (gtk_entry_get_text (entry));

  if (!g_strcmp0 (data->filter_string, ""))
    {
      home_button_clicked_cb (GTK_BUTTON (gtk_builder_get_object (data->builder,
                                                                  "home-button")),
                                          data);
    }
  else
    {
      gtk_tree_model_filter_refilter (GTK_TREE_MODEL_FILTER (data->filter));
      gtk_notebook_set_current_page (GTK_NOTEBOOK (data->notebook), 1);
    }
}

gboolean
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

int
main (int argc, char **argv)
{
  ShellData *data;
  guint ret;
  GtkWidget *widget;

  gtk_init (&argc, &argv);

  data = g_new0 (ShellData, 1);

  data->builder = gtk_builder_new ();

  ret = gtk_builder_add_from_file (data->builder, UIDIR "/shell.ui", NULL);
  if (ret == 0)
    {
      g_error ("Unable to load UI");
    }

  data->window = W (data->builder, "main-window");
  g_signal_connect (data->window, "delete-event", G_CALLBACK (gtk_main_quit),
                    NULL);

  data->notebook = W (data->builder, "notebook");

  fill_model (data);


  g_signal_connect (gtk_builder_get_object (data->builder, "home-button"),
                    "clicked", G_CALLBACK (home_button_clicked_cb), data);

  widget = (GtkWidget*) gtk_builder_get_object (data->builder, "search-entry");

  g_signal_connect (widget, "changed", G_CALLBACK (search_entry_changed_cb),
                    data);
  g_signal_connect (widget, "key-press-event",
                    G_CALLBACK (search_entry_key_press_event_cb), data);

  load_panel_plugins ();

  gtk_widget_show_all (data->window);

  gtk_main ();

  g_free (data->filter_string);
  g_free (data->current_title);
  g_free (data);

  return 0;
}
