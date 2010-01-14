/*
 * Copyright (c) 2009 Intel, Inc.
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

#include <gtk/gtk.h>
#define GMENU_I_KNOW_THIS_IS_UNSTABLE
#include <gnome-menus/gmenu-tree.h>

#define W(b,x) GTK_WIDGET (gtk_builder_get_object (b, x))


typedef struct
{
  GtkBuilder *builder;
  GtkWidget  *notebook;
  GtkWidget  *window;

  GSList *icon_views;
} ShellData;

void item_activated_cb (GtkIconView *icon_view, GtkTreePath *path, ShellData *data);


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

void
fill_model (ShellData *data)
{
  GSList *list, *l;
  GMenuTreeDirectory *d;
  GMenuTree *t;
  GtkWidget *vbox;

  vbox = W (data->builder, "main-vbox");

  t = gmenu_tree_lookup (MENUDIR "/gnomecc.menu", 0);

  d = gmenu_tree_get_root_directory (t);

  list = gmenu_tree_directory_get_contents (d);

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

          store = gtk_list_store_new (3, G_TYPE_STRING, G_TYPE_STRING,
                                      GDK_TYPE_PIXBUF);

          iconview = gtk_icon_view_new_with_model (GTK_TREE_MODEL (store));
          gtk_icon_view_set_pixbuf_column (GTK_ICON_VIEW (iconview), 2);
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
                                                     2, pixbuf,
                                                     -1);
                }
            }
        }
    }

}

static gboolean
switch_after_delay (ShellData *data)
{
  gtk_notebook_set_current_page (GTK_NOTEBOOK (data->notebook), 1);

  gtk_widget_show (W (data->builder, "home-button"));

  return FALSE;
}

void
plug_added_cb (GtkSocket  *socket,
               ShellData  *data)
{
  GtkWidget *notebook;
  GSList *l;

  notebook = W (data->builder, "notebook");

  /* FIXME: this shouldn't be necassary if the capplet doesn't add to the socket
   * until it is fully ready */
  g_timeout_add (100, (GSourceFunc) switch_after_delay, data);

  /* make sure no items are selected when the user switches back to the icon
   * views */
  for (l = data->icon_views; l; l = l->next)
      gtk_icon_view_unselect_all (GTK_ICON_VIEW (l->data));
}

void
item_activated_cb (GtkIconView *icon_view,
                   GtkTreePath *path,
                   ShellData   *data)
{
  GtkTreeModel *model;
  GtkTreeIter iter = {0,};
  gchar *name, *exec, *command;
  GtkWidget *socket, *notebook;
  guint socket_id = 0;
  static gint index = -1;

  /* create new socket */
  socket = gtk_socket_new ();

  g_signal_connect (socket, "plug-added", G_CALLBACK (plug_added_cb), data);

  notebook = data->notebook;
  if (index >= 0)
    gtk_notebook_remove_page (GTK_NOTEBOOK (notebook), index);
  index = gtk_notebook_append_page (GTK_NOTEBOOK (notebook), socket, NULL);

  gtk_widget_show (socket);

  socket_id = gtk_socket_get_id (GTK_SOCKET (socket));

  /* get exec */
  model = gtk_icon_view_get_model (icon_view);

  gtk_tree_model_get_iter (model, &iter, path);

  gtk_tree_model_get (model, &iter, 0, &name, 1, &exec, -1);

  gtk_window_set_title (GTK_WINDOW (data->window), name);

  /* start app */
  command = g_strdup_printf ("%s --socket=%u", exec, socket_id);
  g_spawn_command_line_async (command, NULL);
  g_free (command);

  g_free (name);
  g_free (exec);
}

void
home_button_clicked_cb (GtkButton *button,
                        ShellData *data)
{
  gtk_notebook_set_current_page (GTK_NOTEBOOK (data->notebook), 0);
  gtk_window_set_title (GTK_WINDOW (data->window), "System Settings");

  gtk_widget_hide (GTK_WIDGET (button));
}

int
main (int argc, char **argv)
{
  ShellData *data;
  guint ret;
  GdkColor color = {0, 32767, 32767, 32767};

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


  gtk_widget_modify_text (W (data->builder,"search-entry"), GTK_STATE_NORMAL,
                          &color);

  g_signal_connect (gtk_builder_get_object (data->builder, "home-button"),
                    "clicked", G_CALLBACK (home_button_clicked_cb), data);

  gtk_widget_show_all (data->window);

  gtk_main ();

  g_free (data);

  return 0;
}
