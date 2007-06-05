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
#include "theme-thumbnail.h"
#include "gnome-theme-apply.h"

#include <libwindow-settings/gnome-wm-manager.h>
#include <string.h>

#define GTK_THEME_KEY "/desktop/gnome/interface/gtk_theme"
#define METACITY_THEME_KEY "/apps/metacity/general/theme"
#define ICON_THEME_KEY "/desktop/gnome/interface/icon_theme"
#define COLOR_SCHEME_KEY "/desktop/gnome/interface/gtk_color_scheme"

#define CUSTOM_THEME_NAME "__custom__"

enum {
  COL_LABEL,
  COL_THUMBNAIL,
  COL_NAME,
  NUM_COLS
};

static void theme_thumbnail_done_cb (GdkPixbuf *pixbuf, AppearanceData *data);

static gchar *
get_default_string_from_key (GConfClient *client, const char *key)
{
  GConfValue *value;
  gchar *str = NULL;

  value = gconf_client_get_default_from_schema (client, key, NULL);

  if (value) {
    if (value->type == GCONF_VALUE_STRING)
      str = gconf_value_to_string (value);
    gconf_value_free (value);
  }

  return str;
}

static gboolean
find_in_model (GtkTreeModel *model, const gchar *value, gint column, GtkTreeIter *hit)
{
  GtkTreeIter iter;
  gboolean valid;
  gchar *test;

  if (!value)
    return FALSE;

  for (valid = gtk_tree_model_get_iter_first (model, &iter); valid;
       valid = gtk_tree_model_iter_next (model, &iter))
  {
    gtk_tree_model_get (model, &iter, column, &test, -1);

    if (test) {
      gint cmp = strcmp (test, value);
      g_free (test);

      if (!cmp) {
      	if (hit)
          *hit = iter;
        return TRUE;
      }
    }
  }

  return FALSE;
}

static void
theme_load_from_gconf (GConfClient *client, GnomeThemeMetaInfo *theme)
{
  gchar *s;

  s = gconf_client_get_string (client, GTK_THEME_KEY, NULL);
  if (s != NULL) {
    g_free (theme->gtk_theme_name);
    theme->gtk_theme_name = s;
  }

  s = gconf_client_get_string (client, COLOR_SCHEME_KEY, NULL);
  if (s != NULL) {
    g_free (theme->gtk_color_scheme);
    theme->gtk_color_scheme = s;
  }

  s = gconf_client_get_string (client, METACITY_THEME_KEY, NULL);
  if (s != NULL) {
    g_free (theme->metacity_theme_name);
    theme->metacity_theme_name = s;
  }

  s = gconf_client_get_string (client, ICON_THEME_KEY, NULL);
  if (s != NULL) {
    g_free (theme->icon_theme_name);
    theme->icon_theme_name = s;
  }
}

static gboolean
theme_thumbnail_generate (AppearanceData *data)
{
  generate_theme_thumbnail_async (data->theme_queue->data, TRUE,
      (ThemeThumbnailFunc) theme_thumbnail_done_cb, data, NULL);
  return FALSE;
}

static void
theme_queue_for_thumbnail (GnomeThemeMetaInfo *theme, AppearanceData *data)
{
  if (data->theme_queue == NULL)
    g_idle_add ((GSourceFunc) theme_thumbnail_generate, data);

  data->theme_queue = g_slist_append (data->theme_queue, theme);
}

static const GnomeThemeMetaInfo *
theme_get_selected (GtkIconView *icon_view, AppearanceData *data)
{
  GnomeThemeMetaInfo *theme = NULL;
  GList *selected = gtk_icon_view_get_selected_items (icon_view);

  if (selected) {
    GtkTreePath *path = selected->data;
    GtkTreeModel *model = gtk_icon_view_get_model (icon_view);
    GtkTreeIter iter;
    gchar *name;

    if (gtk_tree_model_get_iter (model, &iter, path)) {
      gtk_tree_model_get (model, &iter, COL_NAME, &name, -1);

      if (!strcmp (name, data->theme_custom->name)) {
      	theme = data->theme_custom;
      } else {
      	theme = gnome_theme_meta_info_find (name);
      }
    }

    g_free (name);
    g_list_foreach (selected, (GFunc) gtk_tree_path_free, NULL);
    g_list_free (selected);
  }

  return theme;
}

static void
theme_set_custom_from_selected (GtkIconView *icon_view, AppearanceData *data)
{
  GnomeThemeMetaInfo *custom = data->theme_custom;
  const GnomeThemeMetaInfo *info = theme_get_selected (icon_view, data);
  GtkTreeModel *model;
  GtkTreeIter iter;
  GtkTreePath *path;

  if (info == custom)
    return;

  /* if info is not NULL, we'll copy those theme settings over */
  if (info != NULL) {
    g_free (custom->gtk_theme_name);
    g_free (custom->icon_theme_name);
    g_free (custom->metacity_theme_name);
    g_free (custom->gtk_color_scheme);
    custom->gtk_theme_name = NULL;
    custom->icon_theme_name = NULL;
    custom->metacity_theme_name = NULL;
    custom->gtk_color_scheme = NULL;

    if (info->gtk_theme_name)
      custom->gtk_theme_name = g_strdup (info->gtk_theme_name);
    else
      custom->gtk_theme_name = get_default_string_from_key (data->client, GTK_THEME_KEY);

    if (info->icon_theme_name)
      custom->icon_theme_name = g_strdup (info->icon_theme_name);
    else
      custom->icon_theme_name = get_default_string_from_key (data->client, ICON_THEME_KEY);

    if (info->metacity_theme_name)
      custom->metacity_theme_name = g_strdup (info->metacity_theme_name);
    else
      custom->metacity_theme_name = get_default_string_from_key (data->client, METACITY_THEME_KEY);

    if (info->gtk_color_scheme)
      custom->gtk_color_scheme = g_strdup (info->gtk_color_scheme);
    else
      custom->gtk_color_scheme = get_default_string_from_key (data->client, COLOR_SCHEME_KEY);
  }

  /* select the custom theme */
  model = gtk_icon_view_get_model (icon_view);
  if (!find_in_model (model, custom->name, COL_NAME, &iter)) {
    GtkTreeIter child;

    gtk_list_store_insert_with_values (data->theme_store, &child, 0,
        COL_LABEL, custom->readable_name,
        COL_NAME, custom->name,
        -1);
    gtk_tree_model_sort_convert_child_iter_to_iter (
        GTK_TREE_MODEL_SORT (model), &iter, &child);
  }

  path = gtk_tree_model_get_path (model, &iter);
  gtk_icon_view_select_path (icon_view, path);
  gtk_icon_view_scroll_to_path (icon_view, path, FALSE, 0.5, 0.0);
  gtk_tree_path_free (path);

  /* update the theme thumbnail */
  theme_queue_for_thumbnail (custom, data);
}

static void
theme_remove_custom (GtkIconView *icon_view, AppearanceData *data)
{
  GtkTreeIter iter;

  if (find_in_model (GTK_TREE_MODEL (data->theme_store), CUSTOM_THEME_NAME, COL_NAME, &iter))
    gtk_list_store_remove (data->theme_store, &iter);
}

/** Theme Callbacks **/

static void
theme_changed_func (gpointer uri, AppearanceData *data)
{
  /* TODO: add/change/remove themes from the model as appropriate */
}

static void
theme_thumbnail_done_cb (GdkPixbuf *pixbuf, AppearanceData *data)
{
  GnomeThemeMetaInfo *info = data->theme_queue->data;

  g_return_if_fail (info != NULL);

  /* find item in model and update thumbnail */
  if (pixbuf) {
    GtkTreeIter iter;
    GtkTreeModel *model = GTK_TREE_MODEL (data->theme_store);

    if (find_in_model (model, info->name, COL_NAME, &iter))
      gtk_list_store_set (data->theme_store, &iter, COL_THUMBNAIL, pixbuf, -1);
  }

  data->theme_queue = g_slist_remove (data->theme_queue, info);

  if (data->theme_queue)
    /* we can't call theme_thumbnail_generate directly since the thumbnail
     * factory hasn't yet cleaned up at this point */
    g_idle_add ((GSourceFunc) theme_thumbnail_generate, data);
}

/** GUI Callbacks **/

static void
theme_selection_changed_cb (GtkWidget *icon_view, AppearanceData *data)
{
  GList *selection;

  selection = gtk_icon_view_get_selected_items (GTK_ICON_VIEW (icon_view));

  if (selection) {
    GtkTreeModel *model;
    GtkTreeIter iter;
    GnomeThemeMetaInfo *theme = NULL;
    gchar *name;

    model = gtk_icon_view_get_model (GTK_ICON_VIEW (icon_view));
    gtk_tree_model_get_iter (model, &iter, selection->data);
    gtk_tree_model_get (model, &iter, COL_NAME, &name, -1);

    if (!strcmp (name, CUSTOM_THEME_NAME))
      theme = data->theme_custom;
    else
      theme = gnome_theme_meta_info_find (name);

    if (theme)
      gnome_meta_theme_set (theme);

    g_free (name);
    g_list_foreach (selection, (GFunc) gtk_tree_path_free, NULL);
    g_list_free (selection);
  }
}

static void
theme_custom_cb (GtkWidget *button, AppearanceData *data)
{
  GtkWidget *w, *parent, *icon_view;

  /* select the "custom" metatheme */
  icon_view = glade_xml_get_widget (data->xml, "theme_list");
  theme_set_custom_from_selected (GTK_ICON_VIEW (icon_view), data);

  w = glade_xml_get_widget (data->xml, "theme_details");
  parent = glade_xml_get_widget (data->xml, "appearance_window");
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

static void
theme_details_changed_cb (GtkWidget *widget, AppearanceData *data)
{
  /* load new state from gconf */
  theme_load_from_gconf (data->client, data->theme_custom);

  /* regenerate the thumbnail image for theme */
  theme_queue_for_thumbnail (data->theme_custom, data);
}

static void
theme_select_after_realize (GtkIconView *icon_view,
                            const gchar *theme)
{
  GtkTreeIter iter;
  GtkTreeModel *model = gtk_icon_view_get_model (icon_view);

  if (find_in_model (model, theme, COL_NAME, &iter)) {
    GtkTreePath *path = gtk_tree_model_get_path (model, &iter);
    gtk_icon_view_select_path (icon_view, path);
    gtk_icon_view_scroll_to_path (icon_view, path, FALSE, 0.5, 0.0);
    gtk_tree_path_free (path);
  }
}

static gboolean
theme_is_equal (const GnomeThemeMetaInfo *a, const GnomeThemeMetaInfo *b)
{
  if (!(a->gtk_theme_name && b->gtk_theme_name) ||
      strcmp (a->gtk_theme_name, b->gtk_theme_name))
    return FALSE;

  if (!(a->gtk_color_scheme && b->gtk_color_scheme) ||
      strcmp (a->gtk_color_scheme, b->gtk_color_scheme))
    return FALSE;

  if (!(a->icon_theme_name && b->icon_theme_name) ||
      strcmp (a->icon_theme_name, b->icon_theme_name))
    return FALSE;

  if (!(a->metacity_theme_name && b->metacity_theme_name) ||
      strcmp (a->metacity_theme_name, b->metacity_theme_name))
    return FALSE;

  return TRUE;
}

void
themes_init (AppearanceData *data)
{
  GtkWidget *w;
  GList *theme_list, *l;
  GtkListStore *theme_store;
  GtkTreeModel *sort_model;
  gchar *meta_theme = NULL;
  GdkPixbuf *temp;

  /* initialise some stuff */
  gnome_theme_init (NULL);
  gnome_wm_manager_init ();

  data->theme_queue = NULL;
  data->theme_custom = gnome_theme_meta_info_new ();
  data->theme_store = theme_store =
      gtk_list_store_new (NUM_COLS, G_TYPE_STRING, GDK_TYPE_PIXBUF, G_TYPE_STRING);

  /* set up theme list */
  theme_list = gnome_theme_meta_info_find_all ();
  gnome_theme_info_register_theme_change ((GFunc) theme_changed_func, data);

  data->theme_custom->name = g_strdup (CUSTOM_THEME_NAME);
  data->theme_custom->readable_name = g_strdup ("Custom"); /* FIXME: translate */
  theme_load_from_gconf (data->client, data->theme_custom);

  temp = gdk_pixbuf_new_from_file (GNOMECC_PIXMAP_DIR "/theme-thumbnailing.png", NULL);

  for (l = theme_list; l; l = l->next) {
    GnomeThemeMetaInfo *info = l->data;

    data->theme_queue = g_slist_prepend (data->theme_queue, info);

    gtk_list_store_insert_with_values (theme_store, NULL, 0,
        COL_LABEL, info->readable_name,
        COL_NAME, info->name,
        COL_THUMBNAIL, temp,
        -1);

    if (!meta_theme && theme_is_equal (data->theme_custom, info))
      meta_theme = info->name;
  }

  if (!meta_theme) {
    /* add custom theme */
    meta_theme = data->theme_custom->name;
    data->theme_queue = g_slist_prepend (data->theme_queue, data->theme_custom);

    gtk_list_store_insert_with_values (theme_store, NULL, 0,
        COL_LABEL, data->theme_custom->readable_name,
        COL_NAME, meta_theme,
        COL_THUMBNAIL, temp,
        -1);
  }

  if (temp)
    g_object_unref (temp);

  w = glade_xml_get_widget (data->xml, "theme_list");
  g_signal_connect (w, "selection-changed", (GCallback) theme_selection_changed_cb, data);
  gtk_icon_view_set_selection_mode (GTK_ICON_VIEW (w), GTK_SELECTION_BROWSE);
  sort_model = gtk_tree_model_sort_new_with_model (GTK_TREE_MODEL (theme_store));
  gtk_icon_view_set_model (GTK_ICON_VIEW (w), GTK_TREE_MODEL (sort_model));
  gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (sort_model), COL_LABEL, GTK_SORT_ASCENDING);

  g_signal_connect_after (w, "realize", (GCallback) theme_select_after_realize, meta_theme);

  w = glade_xml_get_widget (data->xml, "theme_install");
  gtk_button_set_image (GTK_BUTTON (w),
                        gtk_image_new_from_stock (GTK_STOCK_OPEN, GTK_ICON_SIZE_BUTTON));

  /* connect button signals */
  g_signal_connect (w, "clicked", (GCallback) theme_install_cb, data);
  g_signal_connect (glade_xml_get_widget (data->xml, "theme_custom"), "clicked", (GCallback) theme_custom_cb, data);
  g_signal_connect (glade_xml_get_widget (data->xml, "theme_delete"), "clicked", (GCallback) theme_delete_cb, data);

  /* connect list signals in the details window */
  g_signal_connect_after (glade_xml_get_widget (data->xml, "gtk_themes_list"), "cursor-changed", (GCallback) theme_details_changed_cb, data);
  g_signal_connect_after (glade_xml_get_widget (data->xml, "window_themes_list"), "cursor-changed", (GCallback) theme_details_changed_cb, data);
  g_signal_connect_after (glade_xml_get_widget (data->xml, "icon_themes_list"), "cursor-changed", (GCallback) theme_details_changed_cb, data);
  /* FIXME: need to connect to color scheme stuff, too... */

  theme_thumbnail_generate (data);
}

void
themes_shutdown (AppearanceData *data)
{
  gnome_theme_meta_info_free (data->theme_custom);
  g_slist_free (data->theme_queue);
}
