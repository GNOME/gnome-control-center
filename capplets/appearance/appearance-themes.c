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
  THEME_NAME_COLUMN,
  THEME_PIXBUF_COLUMN,
};

/* Theme functions */
void theme_changed_func (gpointer uri, AppearanceData *data);

/* GUI Callbacks */
void theme_save_cb (GtkWidget *button, AppearanceData *data);
void theme_open_cb (GtkWidget *button, AppearanceData *data);
void theme_delete_cb (GtkWidget *button, AppearanceData *data);
void theme_activated_cb (GtkWidget *icon_view, GtkTreePath *path, AppearanceData *data);

void
themes_init (AppearanceData *data)
{
  GtkWidget *w;
  GList *theme_list, *l;
  GtkListStore *theme_store;

  /* initialise some stuff */
  gnome_theme_init (NULL);
  gnome_wm_manager_init ();

  /* connect button signals */
  g_signal_connect (G_OBJECT (glade_xml_get_widget (data->xml, "theme_save")), "clicked", (GCallback) theme_save_cb, data);
  g_signal_connect (G_OBJECT (glade_xml_get_widget (data->xml, "theme_open")), "clicked", (GCallback) theme_open_cb, data);
  g_signal_connect (G_OBJECT (glade_xml_get_widget (data->xml, "theme_delete")), "clicked", (GCallback) theme_delete_cb, data);

  /* connect theme list signals */
  g_signal_connect (G_OBJECT (glade_xml_get_widget (data->xml, "theme_list")), "item-activated", (GCallback) theme_activated_cb, data);

  /* set up theme list */
  theme_store = gtk_list_store_new (2, G_TYPE_STRING, GDK_TYPE_PIXBUF);

  /* Temporary measure to fill the themes list.
   * This should be replace with asynchronous calls.
   */
  theme_list = gnome_theme_meta_info_find_all ();
  gnome_theme_info_register_theme_change ((GFunc)theme_changed_func, data);
  for (l = theme_list; l; l = g_list_next (l))
  {
    gchar *name = ((GnomeThemeMetaInfo *)l->data)->readable_name;
    if (name)
    {
      GdkPixbuf *pixbuf = generate_theme_thumbnail (l->data, TRUE);
      gtk_list_store_insert_with_values (theme_store, NULL, 0,
          THEME_NAME_COLUMN, name,
          THEME_PIXBUF_COLUMN, pixbuf,
          -1);
    }
  }

  w = glade_xml_get_widget (data->xml, "theme_list");
  gtk_icon_view_set_model (GTK_ICON_VIEW (w), GTK_TREE_MODEL (theme_store));

  w = glade_xml_get_widget (data->xml, "theme_open");
  gtk_button_set_image (GTK_BUTTON (w),
                        gtk_image_new_from_stock ("gtk-open", GTK_ICON_SIZE_BUTTON));
}

/** Theme Callbacks **/

void
theme_changed_func (gpointer uri, AppearanceData *data)
{
  /* TODO: add/change/remove themes from the model as appropriate */
}


/** GUI Callbacks **/

void
theme_activated_cb (GtkWidget *icon_view, GtkTreePath *path, AppearanceData *data)
{
  GtkTreeModel *model;
  GtkTreeIter iter;
  GnomeThemeMetaInfo *theme = NULL;
  gchar *name;

  model = gtk_icon_view_get_model (GTK_ICON_VIEW (icon_view));
  gtk_tree_model_get_iter (model, &iter, path);
  gtk_tree_model_get (model, &iter, THEME_NAME_COLUMN, &name, -1);

  theme = gnome_theme_meta_info_find (name);
  if (theme)
    gnome_meta_theme_set (theme);
  g_free (theme);
  g_free (name);
}

void
theme_save_cb (GtkWidget *button, AppearanceData *data)
{
  /* TODO: Save the current settings as a new theme */
}

void
theme_open_cb (GtkWidget *button, AppearanceData *data)
{
  /* TODO: Install a new theme */
}

void
theme_delete_cb (GtkWidget *button, AppearanceData *data)
{
  /* TODO: Delete the selected theme */
}
