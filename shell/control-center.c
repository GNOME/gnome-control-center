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

#include "control-center.h"

#include <glib/gi18n.h>
#include <gio/gio.h>
#include <gio/gdesktopappinfo.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <string.h>
#define GMENU_I_KNOW_THIS_IS_UNSTABLE
#include <gmenu-tree.h>

#include "cc-panel.h"
#include "cc-shell.h"
#include "shell-search-renderer.h"
#include "cc-shell-category-view.h"
#include "cc-shell-model.h"

#include <unique/unique.h>

#define W(b,x) GTK_WIDGET (gtk_builder_get_object (b, x))

enum
{
  CC_SHELL_RAISE_COMMAND = 1
};

typedef struct
{
  GtkBuilder *builder;
  GtkWidget  *notebook;
  GtkWidget  *window;
  GtkWidget  *search_entry;

  gchar  *current_title;

  GtkListStore *store;

  GtkTreeModel *search_filter;
  GtkWidget *search_view;
  GtkCellRenderer *search_renderer;
  gchar *filter_string;

  GHashTable *panels;

  guint32 last_time;

} ShellData;


static void
activate_panel (const gchar *name,
                const gchar *id,
                const gchar *desktop_file,
                ShellData   *data)
{
  if (cc_shell_set_panel (CC_SHELL (data->builder), id))
    {
      gtk_label_set_text (GTK_LABEL (W (data->builder, "label-title")), name);
      gtk_widget_show (W (data->builder, "title-alignment"));
    }
  else
    {
      /* start app directly */
      g_debug ("Panel module not found for %s", id);

      GAppInfo *appinfo;
      GError *err = NULL;
      GdkAppLaunchContext *ctx;
      GKeyFile *key_file;

      key_file = g_key_file_new ();
      g_key_file_load_from_file (key_file, desktop_file, 0, &err);

      if (err)
        {
          g_warning ("Error starting \"%s\": %s", id, err->message);

          g_error_free (err);
          err = NULL;
          return;
        }

      appinfo = (GAppInfo*) g_desktop_app_info_new_from_keyfile (key_file);

      g_key_file_free (key_file);


      ctx = gdk_app_launch_context_new ();
      gdk_app_launch_context_set_screen (ctx, gdk_screen_get_default ());
      gdk_app_launch_context_set_timestamp (ctx, data->last_time);

      g_app_info_launch (appinfo, NULL, G_APP_LAUNCH_CONTEXT (ctx), &err);

      g_object_unref (appinfo);
      g_object_unref (ctx);

      if (err)
        {
          g_warning ("Error starting \"%s\": %s", id, err->message);
          g_error_free (err);
          err = NULL;
        }
    }
}

static void
item_activated_cb (CcShellCategoryView *view,
                   gchar               *name,
                   gchar               *id,
                   gchar               *desktop_file,
                   ShellData           *data)
{
  activate_panel (name, id, desktop_file, data);
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
setup_search (ShellData *data)
{
  GtkWidget *search_scrolled, *search_view;

  g_return_if_fail (data->store != NULL);

  /* create the search filter */
  data->search_filter = gtk_tree_model_filter_new (GTK_TREE_MODEL (data->store),
                                                   NULL);

  gtk_tree_model_filter_set_visible_func (GTK_TREE_MODEL_FILTER (data->search_filter),
                                          (GtkTreeModelFilterVisibleFunc)
                                            model_filter_func,
                                          data, NULL);

  /* set up the search view */
  data->search_view = search_view = cc_shell_item_view_new ();
  gtk_icon_view_set_orientation (GTK_ICON_VIEW (search_view),
                                 GTK_ORIENTATION_HORIZONTAL);
  gtk_icon_view_set_spacing (GTK_ICON_VIEW (search_view), 6);
  gtk_icon_view_set_model (GTK_ICON_VIEW (search_view),
                           GTK_TREE_MODEL (data->search_filter));
  gtk_icon_view_set_pixbuf_column (GTK_ICON_VIEW (search_view), COL_PIXBUF);

  search_scrolled = W (data->builder, "search-scrolled-window");
  gtk_container_add (GTK_CONTAINER (search_scrolled), search_view);


  /* add the custom renderer */
  data->search_renderer = (GtkCellRenderer*) shell_search_renderer_new ();
  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (search_view),
                              data->search_renderer, TRUE);
  gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (search_view),
                                 data->search_renderer,
                                 "title", COL_NAME);
  gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (search_view),
                                 data->search_renderer,
                                 "search-target", COL_SEARCH_TARGET);

  /* connect the activated signal */
  g_signal_connect (search_view, "desktop-item-activated",
                    G_CALLBACK (item_activated_cb), data);
}

static void
fill_model (ShellData *data)
{
  GSList *list, *l;
  GMenuTreeDirectory *d;
  GMenuTree *tree;
  GtkWidget *vbox;

  vbox = W (data->builder, "main-vbox");

  gtk_widget_modify_bg (vbox->parent, GTK_STATE_NORMAL,
                        &vbox->style->base[GTK_STATE_NORMAL]);
  gtk_widget_modify_fg (vbox->parent, GTK_STATE_NORMAL,
                        &vbox->style->text[GTK_STATE_NORMAL]);

  tree = gmenu_tree_lookup (MENUDIR "/gnomecc.menu", 0);

  if (!tree)
    {
      g_warning ("Could not find control center menu");
      return;
    }

  d = gmenu_tree_get_root_directory (tree);

  list = gmenu_tree_directory_get_contents (d);

  data->store = (GtkListStore *) cc_shell_model_new ();



  for (l = list; l; l = l->next)
    {
      GMenuTreeItemType type;
      type = gmenu_tree_item_get_type (l->data);

      if (type == GMENU_TREE_ITEM_DIRECTORY)
        {
          GtkTreeModel *filter;
          GtkWidget *categoryview;
          GSList *contents, *f;
          const gchar *dir_name;

          contents = gmenu_tree_directory_get_contents (l->data);
          dir_name = gmenu_tree_directory_get_name (l->data);

          /* create new category view for this category */
          filter = gtk_tree_model_filter_new (GTK_TREE_MODEL (data->store),
                                              NULL);
          gtk_tree_model_filter_set_visible_func (GTK_TREE_MODEL_FILTER (filter),
                                                  (GtkTreeModelFilterVisibleFunc) category_filter_func,
                                                  g_strdup (dir_name), g_free);

          categoryview = cc_shell_category_view_new (dir_name, filter);
          gtk_box_pack_start (GTK_BOX (vbox), categoryview, FALSE, TRUE, 6);
          g_signal_connect (cc_shell_category_view_get_item_view (CC_SHELL_CATEGORY_VIEW (categoryview)),
                            "desktop-item-activated",
                            G_CALLBACK (item_activated_cb), data);
          gtk_widget_show (categoryview);

          /* add the items from this category to the model */
          for (f = contents; f; f = f->next)
            {
              if (gmenu_tree_item_get_type (f->data) == GMENU_TREE_ITEM_ENTRY)
                {
                  cc_shell_model_add_item (CC_SHELL_MODEL (data->store),
                                           dir_name,
                                           f->data);
                }
            }
        }
    }

}



static void
shell_show_overview_page (ShellData *data)
{
  gtk_notebook_set_current_page (GTK_NOTEBOOK (data->notebook), OVERVIEW_PAGE);

  gtk_notebook_remove_page (GTK_NOTEBOOK (data->notebook), CAPPLET_PAGE);

  cc_shell_set_panel (CC_SHELL (data->builder), NULL);

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

      gtk_label_set_text (GTK_LABEL (gtk_builder_get_object (data->builder, "label-title")), "");
      gtk_widget_hide (GTK_WIDGET (gtk_builder_get_object (data->builder, "title-alignment")));
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

      data->last_time = event->time;

      gtk_icon_view_item_activated (GTK_ICON_VIEW (data->search_view), path);

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

  if (page_num == CAPPLET_PAGE)
    gtk_widget_hide (W (data->builder, "search-entry"));
  else
    gtk_widget_show (W (data->builder, "search-entry"));
}

static void
search_entry_clear_cb (GtkEntry *entry)
{
  gtk_entry_set_text (entry, "");
}

static UniqueResponse
message_received (UniqueApp         *app,
                  gint               command,
                  UniqueMessageData *message_data,
                  guint              time_,
                  GtkWindow         *window)
{
  gtk_window_present (window);

  return GTK_RESPONSE_OK;
}

int
main (int argc, char **argv)
{
  ShellData *data;
  GtkWidget *widget;
  UniqueApp *unique;

  g_thread_init (NULL);
  gtk_init (&argc, &argv);

  unique = unique_app_new_with_commands ("org.gnome.ControlCenter",
                                         NULL,
                                         "raise",
                                         CC_SHELL_RAISE_COMMAND,
                                         NULL);

  if (unique_app_is_running (unique))
    {
      unique_app_send_message (unique, 1, NULL);
      return 0;
    }

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
  setup_search (data);


  g_signal_connect (gtk_builder_get_object (data->builder, "home-button"),
                    "clicked", G_CALLBACK (home_button_clicked_cb), data);

  widget = (GtkWidget*) gtk_builder_get_object (data->builder, "search-entry");
  data->search_entry = widget;

  g_signal_connect (widget, "changed", G_CALLBACK (search_entry_changed_cb),
                    data);
  g_signal_connect (widget, "key-press-event",
                    G_CALLBACK (search_entry_key_press_event_cb), data);

  g_signal_connect (widget, "icon-release", G_CALLBACK (search_entry_clear_cb), data);

  gtk_widget_show_all (data->window);

  g_signal_connect (unique, "message-received", G_CALLBACK (message_received),
                    data->window);


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

      activate_panel (name, start_id, NULL, data);
    }

  gtk_main ();

  g_free (data->filter_string);
  g_free (data->current_title);
  g_free (data);

  return 0;
}
