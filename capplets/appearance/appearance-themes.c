/*
 * Copyright (C) 2007 The GNOME Foundation
 * Written by Thomas Wood <thos@gnome.org>
 * All Rights Reserved
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
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "appearance.h"
#include "gnome-theme-info.h"
#include "theme-thumbnail.h"
#include "gnome-theme-apply.h"
#include <libwindow-settings/gnome-wm-manager.h>

enum {
  THEME_DISPLAY_NAME_COLUMN,
  THEME_PIXBUF_COLUMN,
  THEME_NAME_COLUMN,
};

struct theme_thumbnail_func_data {
  GnomeThemeMetaInfo *info;
  GtkListStore *store;
};

/* Theme functions */
static void theme_changed_func (gpointer uri, AppearanceData *data);
static void theme_thumbnail_func (GdkPixbuf *pixbuf, struct theme_thumbnail_func_data *data);

/* GUI Callbacks */
static void theme_custom_cb (GtkWidget *button, AppearanceData *data);
static void theme_install_cb (GtkWidget *button, AppearanceData *data);
static void theme_delete_cb (GtkWidget *button, AppearanceData *data);
static void theme_selection_changed_cb (GtkWidget *icon_view, AppearanceData *data);

void
themes_init (AppearanceData *data)
{
  GtkWidget *w;
  GList *theme_list, *l;
  GtkListStore *theme_store;
  GtkTreeModel *sort_model;

  /* initialise some stuff */
  gnome_theme_init (NULL);
  gnome_wm_manager_init ();

  /* connect button signals */
  g_signal_connect (G_OBJECT (glade_xml_get_widget (data->xml, "theme_custom")), "clicked", (GCallback) theme_custom_cb, data);
  g_signal_connect (G_OBJECT (glade_xml_get_widget (data->xml, "theme_install")), "clicked", (GCallback) theme_install_cb, data);
  g_signal_connect (G_OBJECT (glade_xml_get_widget (data->xml, "theme_delete")), "clicked", (GCallback) theme_delete_cb, data);

  /* connect theme list signals */
  g_signal_connect (G_OBJECT (glade_xml_get_widget (data->xml, "theme_list")), "selection-changed", (GCallback) theme_selection_changed_cb, data);

  /* set up theme list */
  theme_store = gtk_list_store_new (3, G_TYPE_STRING, GDK_TYPE_PIXBUF, G_TYPE_STRING);

  /* Temporary measure to fill the themes list.
   * This should be replace with asynchronous calls.
   */
  theme_list = gnome_theme_meta_info_find_all ();
  gnome_theme_info_register_theme_change ((GFunc)theme_changed_func, data);
  for (l = theme_list; l; l = g_list_next (l))
  {
    struct theme_thumbnail_func_data *tdata;

    tdata = g_new0 (struct theme_thumbnail_func_data, 1);
    tdata->info = (GnomeThemeMetaInfo *)l->data;
    tdata->store = theme_store;

    generate_theme_thumbnail_async (tdata->info, (ThemeThumbnailFunc) theme_thumbnail_func, tdata, NULL);
  }
  g_list_free (theme_list);

  w = glade_xml_get_widget (data->xml, "theme_list");
  sort_model = gtk_tree_model_sort_new_with_model (GTK_TREE_MODEL (theme_store));
  gtk_icon_view_set_model (GTK_ICON_VIEW (w), GTK_TREE_MODEL (sort_model));
  gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (sort_model), THEME_DISPLAY_NAME_COLUMN, GTK_SORT_ASCENDING);

  w = glade_xml_get_widget (data->xml, "theme_install");
  gtk_button_set_image (GTK_BUTTON (w),
                        gtk_image_new_from_stock ("gtk-open", GTK_ICON_SIZE_BUTTON));
}

/** Theme Callbacks **/

static void
theme_changed_func (gpointer uri, AppearanceData *data)
{
  /* TODO: add/change/remove themes from the model as appropriate */
}


static void
theme_thumbnail_func (GdkPixbuf *pixbuf, struct theme_thumbnail_func_data *data)
{
  /* TODO: we should probably have already added the item, and only be updating
   * the thumbnail here */

  gtk_list_store_insert_with_values (data->store, NULL, 0,
      THEME_DISPLAY_NAME_COLUMN, data->info->readable_name,
      THEME_NAME_COLUMN, data->info->name,
      THEME_PIXBUF_COLUMN, pixbuf,
      -1);
  gnome_theme_meta_info_free (data->info);
  g_free (data);
}

/** GUI Callbacks **/

static void
theme_selection_changed_cb (GtkWidget *icon_view, AppearanceData *data)
{
  GtkTreeModel *model;
  GtkTreeIter iter;
  GnomeThemeMetaInfo *theme = NULL;
  gchar *name;
  GList *selection;

  selection = gtk_icon_view_get_selected_items (GTK_ICON_VIEW (icon_view));

  model = gtk_icon_view_get_model (GTK_ICON_VIEW (icon_view));
  gtk_tree_model_get_iter (model, &iter, selection->data);
  gtk_tree_model_get (model, &iter, THEME_NAME_COLUMN, &name, -1);

  theme = gnome_theme_meta_info_find (name);
  if (theme)
    gnome_meta_theme_set (theme);

  g_free (theme);
  g_free (name);
  g_list_foreach (selection, (GFunc) gtk_tree_path_free, NULL);
  g_list_free (selection);
}

static void
theme_custom_cb (GtkWidget *button, AppearanceData *data)
{
  GtkWidget *w, *parent;
  w = glade_xml_get_widget (data->xml, "theme_details");
  parent = glade_xml_get_widget (data->xml, "appearance_window");
  g_signal_connect_swapped (glade_xml_get_widget (data->xml, "theme_close_button"), "clicked", (GCallback) gtk_widget_hide, w);

  gtk_window_set_transient_for (GTK_WINDOW (w), GTK_WINDOW (parent));
  gtk_widget_show_all (w);
}

static void
theme_install_cb (GtkWidget *button, AppearanceData *data)
{
  /* TODO: Install a new theme */
}

static void
theme_delete_cb (GtkWidget *button, AppearanceData *data)
{
  /* TODO: Delete the selected theme */
}
