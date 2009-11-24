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
 */

#include <gtk/gtk.h>
#define GMENU_I_KNOW_THIS_IS_UNSTABLE
#include <gnome-menus/gmenu-tree.h>

#define W(b,x) GTK_WIDGET (gtk_builder_get_object (b, x))


void
fill_model (GtkListStore *store)
{
  GSList *list, *l;
  GMenuTreeDirectory *d;
  GMenuTree *t;

  t = gmenu_tree_lookup ("/etc/xdg/menus/gnomecc.menu", 0);

  d = gmenu_tree_get_root_directory (t);

  list = gmenu_tree_directory_get_contents (d);

  for (l = list; l; l = l->next)
    {
      GMenuTreeItemType type;
      type = gmenu_tree_item_get_type (l->data);
      if (type == GMENU_TREE_ITEM_DIRECTORY)
        {
          GSList *foo, *f;
          foo = gmenu_tree_directory_get_contents (l->data);
          for (f = foo; f; f = f->next)
            {
              if (gmenu_tree_item_get_type (f->data)
                  == GMENU_TREE_ITEM_ENTRY)
                {
                  const gchar *icon = gmenu_tree_entry_get_icon (f->data);
                  const gchar *name = gmenu_tree_entry_get_name (f->data);
                  const gchar *exec = gmenu_tree_entry_get_exec (f->data);

                  gtk_list_store_insert_with_values (store, NULL, 0, 
                                                     0, name,
                                                     1, exec,
                                                     2, icon,
                                                     -1);
                }
            }
        }
    }

}

void
plug_added_cb (GtkSocket *socket,
               GtkBuilder *builder)
{
  GtkWidget *notebook;

  notebook = W (builder, "notebook");

  gtk_notebook_set_page (GTK_NOTEBOOK (notebook), 1);
}

void
item_activated_cb (GtkIconView *icon_view,
                   GtkTreePath *path,
                   GtkBuilder *builder)
{
  GtkTreeModel *model;
  GtkTreeIter iter = {0,};
  gchar *name, *exec, *command;
  GtkWidget *socket, *notebook;
  guint socket_id = 0;
  static gint index = -1;

  /* create new socket */
  socket = gtk_socket_new ();

  g_signal_connect (socket, "plug-added", G_CALLBACK (plug_added_cb), builder);

  notebook = W (builder, "notebook");
  if (index >= 0)
    gtk_notebook_remove_page (GTK_NOTEBOOK (notebook), index);
  index = gtk_notebook_append_page (GTK_NOTEBOOK (notebook), socket, NULL);

  gtk_widget_show (socket);

  socket_id = gtk_socket_get_id (GTK_SOCKET (socket));

  /* get exec */
  model = gtk_icon_view_get_model (icon_view);

  gtk_tree_model_get_iter (model, &iter, path);

  gtk_tree_model_get (model, &iter, 0, &name, 1, &exec, -1);

  gtk_label_set_text (GTK_LABEL (W (builder, "applet-label")),
                      name);
  gtk_widget_show (W (builder, "applet-label"));
  gtk_widget_show (W (builder, "arrow"));

  /* start app */
  command = g_strdup_printf ("%s --socket=%u", exec, socket_id);
  g_spawn_command_line_async (command, NULL);
  g_free (command);

  g_free (name);
  g_free (exec);
}

void
home_button_clicked_cb (GtkButton *button, GtkBuilder *builder)
{
  gtk_notebook_set_page (GTK_NOTEBOOK (W (builder, "notebook")), 0);
  gtk_widget_hide (W (builder, "applet-label"));
  gtk_widget_hide (W (builder, "arrow"));
}

int
main (int argc, char **argv)
{
  GtkBuilder *b;
  GtkWidget *window, *notebook;
  GdkColor color = {0, 32767, 32767, 32767};

  gtk_init (&argc, &argv);

  b = gtk_builder_new ();

  gtk_builder_add_from_file (b, "shell.ui", NULL);

  window = W (b, "main-window");
  g_signal_connect (window, "delete-event", G_CALLBACK (gtk_main_quit), NULL);

  fill_model (GTK_LIST_STORE (gtk_builder_get_object (b, "liststore")));

  notebook = W (b, "notebook");

  gtk_widget_modify_text (W (b,"search-entry"), GTK_STATE_NORMAL, &color);


  g_signal_connect (gtk_builder_get_object (b, "iconview"), "item-activated",
                    G_CALLBACK (item_activated_cb), b);
  g_signal_connect (gtk_builder_get_object (b, "home-button"), "clicked",
                    G_CALLBACK (home_button_clicked_cb), b);

  gtk_widget_show_all (window);

  gtk_main ();

  return 0;
}
