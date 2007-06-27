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
#include "theme-installer.h"
#include "theme-save.h"
#include "theme-util.h"

#include <glib/gi18n.h>
#include <libwindow-settings/gnome-wm-manager.h>
#include <string.h>

#define CUSTOM_THEME_NAME "__custom__"

static void
theme_thumbnail_done_cb (GdkPixbuf *pixbuf, gchar *theme_name, AppearanceData *data)
{
  /* find item in model and update thumbnail */
  if (pixbuf) {
    GtkTreeIter iter;
    GtkTreeModel *model = GTK_TREE_MODEL (data->theme_store);

    if (theme_find_in_model (model, theme_name, &iter))
      gtk_list_store_set (data->theme_store, &iter, COL_THUMBNAIL, pixbuf, -1);

    g_object_unref (pixbuf);
  }
}

static void
theme_thumbnail_generate (GnomeThemeMetaInfo *info, AppearanceData *data)
{
  generate_meta_theme_thumbnail_async (info,
      (ThemeThumbnailFunc) theme_thumbnail_done_cb, data, NULL);
}

static void
theme_changed_on_disk_cb (GnomeThemeType       type,
			  gpointer             theme,
			  GnomeThemeChangeType change_type,
			  GnomeThemeElement    element,
			  AppearanceData       *data)
{
  if (type == GNOME_THEME_TYPE_METATHEME) {
    GnomeThemeMetaInfo *meta = theme;

    if (change_type == GNOME_THEME_CHANGE_CREATED) {
      gtk_list_store_insert_with_values (data->theme_store, NULL, 0,
          COL_LABEL, meta->readable_name,
          COL_NAME, meta->name,
          COL_THUMBNAIL, data->theme_icon,
          -1);
      theme_thumbnail_generate (meta, data);

    } else if (change_type == GNOME_THEME_CHANGE_DELETED) {
      GtkTreeIter iter;

      if (theme_find_in_model (GTK_TREE_MODEL (data->theme_store), meta->name, &iter))
        gtk_list_store_remove (data->theme_store, &iter);

    } else if (change_type == GNOME_THEME_CHANGE_CHANGED) {
      theme_thumbnail_generate (meta, data);
    }
  }
}

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

/* Find out if the lockdown key has been set. Currently returns false on error... */
static gboolean
is_locked_down (GConfClient *client)
{
  return gconf_client_get_bool (client, LOCKDOWN_KEY, NULL);
}

static void
theme_load_from_gconf (GConfClient *client, GnomeThemeMetaInfo *theme)
{
  g_free (theme->gtk_theme_name);
  theme->gtk_theme_name = gconf_client_get_string (client, GTK_THEME_KEY, NULL);

  g_free (theme->gtk_color_scheme);
  theme->gtk_color_scheme = gconf_client_get_string (client, COLOR_SCHEME_KEY, NULL);

  g_free (theme->metacity_theme_name);
  theme->metacity_theme_name = gconf_client_get_string (client, METACITY_THEME_KEY, NULL);

  g_free (theme->icon_theme_name);
  theme->icon_theme_name = gconf_client_get_string (client, ICON_THEME_KEY, NULL);
}

static gchar *
theme_get_selected_name (GtkIconView *icon_view, AppearanceData *data)
{
  gchar *name = NULL;
  GList *selected = gtk_icon_view_get_selected_items (icon_view);

  if (selected) {
    GtkTreePath *path = selected->data;
    GtkTreeModel *model = gtk_icon_view_get_model (icon_view);
    GtkTreeIter iter;

    if (gtk_tree_model_get_iter (model, &iter, path))
      gtk_tree_model_get (model, &iter, COL_NAME, &name, -1);

    g_list_foreach (selected, (GFunc) gtk_tree_path_free, NULL);
    g_list_free (selected);
  }

  return name;
}

static const GnomeThemeMetaInfo *
theme_get_selected (GtkIconView *icon_view, AppearanceData *data)
{
  GnomeThemeMetaInfo *theme = NULL;
  gchar *name = theme_get_selected_name (icon_view, data);

  if (name != NULL) {
    if (!strcmp (name, data->theme_custom->name)) {
      theme = data->theme_custom;
    } else {
      theme = gnome_theme_meta_info_find (name);
    }

    g_free (name);
  }

  return theme;
}

static void
theme_select_iter (GtkIconView *icon_view, GtkTreeIter *iter)
{
  GtkTreePath *path;

  path = gtk_tree_model_get_path (gtk_icon_view_get_model (icon_view), iter);
  gtk_icon_view_select_path (icon_view, path);
  gtk_icon_view_scroll_to_path (icon_view, path, FALSE, 0.5, 0.0);
  gtk_tree_path_free (path);
}

static void
theme_select_name (GtkIconView *icon_view, const gchar *theme)
{
  GtkTreeIter iter;
  GtkTreeModel *model = gtk_icon_view_get_model (icon_view);

  if (theme_find_in_model (model, theme, &iter))
    theme_select_iter (icon_view, &iter);
}

static gboolean
theme_is_equal (const GnomeThemeMetaInfo *a, const GnomeThemeMetaInfo *b)
{
  gboolean a_set, b_set;

  if (!(a->gtk_theme_name && b->gtk_theme_name) ||
      strcmp (a->gtk_theme_name, b->gtk_theme_name))
    return FALSE;

  a_set = a->gtk_color_scheme && strcmp (a->gtk_color_scheme, "");
  b_set = b->gtk_color_scheme && strcmp (b->gtk_color_scheme, "");
  if ((a_set != b_set) ||
      (a_set && strcmp (a->gtk_color_scheme, b->gtk_color_scheme)))
    return FALSE;

  if (!(a->icon_theme_name && b->icon_theme_name) ||
      strcmp (a->icon_theme_name, b->icon_theme_name))
    return FALSE;

  if (!(a->metacity_theme_name && b->metacity_theme_name) ||
      strcmp (a->metacity_theme_name, b->metacity_theme_name))
    return FALSE;

  return TRUE;
}

static void
theme_set_custom_from_theme (const GnomeThemeMetaInfo *info, AppearanceData *data)
{
  GnomeThemeMetaInfo *custom = data->theme_custom;
  GtkIconView *icon_view = GTK_ICON_VIEW (glade_xml_get_widget (data->xml, "theme_list"));
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
  if (!theme_find_in_model (model, custom->name, &iter)) {
    GtkTreeIter child;

    gtk_list_store_insert_with_values (data->theme_store, &child, 0,
        COL_LABEL, custom->readable_name,
        COL_NAME, custom->name,
        COL_THUMBNAIL, data->theme_icon,
        -1);
    gtk_tree_model_sort_convert_child_iter_to_iter (
        GTK_TREE_MODEL_SORT (model), &iter, &child);
  }

  path = gtk_tree_model_get_path (model, &iter);
  gtk_icon_view_select_path (icon_view, path);
  gtk_icon_view_scroll_to_path (icon_view, path, FALSE, 0.5, 0.0);
  gtk_tree_path_free (path);

  /* update the theme thumbnail */
  theme_thumbnail_generate (custom, data);
}

/** GUI Callbacks **/

static void
theme_selection_changed_cb (GtkWidget *icon_view, AppearanceData *data)
{
  GList *selection;
  GnomeThemeMetaInfo *theme = NULL;
  gboolean is_custom = FALSE;

  selection = gtk_icon_view_get_selected_items (GTK_ICON_VIEW (icon_view));

  if (selection) {
    GtkTreeModel *model;
    GtkTreeIter iter;
    gchar *name;

    model = gtk_icon_view_get_model (GTK_ICON_VIEW (icon_view));
    gtk_tree_model_get_iter (model, &iter, selection->data);
    gtk_tree_model_get (model, &iter, COL_NAME, &name, -1);

    is_custom = !strcmp (name, CUSTOM_THEME_NAME);

    if (is_custom)
      theme = data->theme_custom;
    else
      theme = gnome_theme_meta_info_find (name);

    if (theme)
      gnome_meta_theme_set (theme);

    g_free (name);
    g_list_foreach (selection, (GFunc) gtk_tree_path_free, NULL);
    g_list_free (selection);
  }

  gtk_widget_set_sensitive (glade_xml_get_widget (data->xml, "theme_delete"),
			    gnome_theme_is_writable (theme, GNOME_THEME_TYPE_METATHEME));
  gtk_widget_set_sensitive (glade_xml_get_widget (data->xml, "theme_save"), is_custom);
}

static void
theme_custom_cb (GtkWidget *button, AppearanceData *data)
{
  GtkWidget *w, *parent;

  /* select the "custom" metatheme */
  w = glade_xml_get_widget (data->xml, "theme_list");
  theme_set_custom_from_theme (theme_get_selected (GTK_ICON_VIEW (w), data), data);

  w = glade_xml_get_widget (data->xml, "theme_details");
  parent = glade_xml_get_widget (data->xml, "appearance_window");
  gtk_window_set_transient_for (GTK_WINDOW (w), GTK_WINDOW (parent));
  gtk_widget_show_all (w);
}

static void
theme_save_cb (GtkWidget *button, AppearanceData *data)
{
  theme_save_dialog_run (data->theme_custom, data);
}

static void
theme_install_cb (GtkWidget *button, AppearanceData *data)
{
  gnome_theme_installer_run (
      GTK_WINDOW (glade_xml_get_widget (data->xml, "appearance_window")), NULL);
}

static void
theme_delete_cb (GtkWidget *button, AppearanceData *data)
{
  GtkIconView *icon_view = GTK_ICON_VIEW (glade_xml_get_widget (data->xml, "theme_list"));
  GList *selected = gtk_icon_view_get_selected_items (icon_view);

  if (selected) {
    GtkTreePath *path = selected->data;
    GtkTreeModel *model = gtk_icon_view_get_model (icon_view);
    GtkTreeIter iter;
    gchar *name = NULL;

    if (gtk_tree_model_get_iter (model, &iter, path))
      gtk_tree_model_get (model, &iter, COL_NAME, &name, -1);

    if (name != NULL &&
        strcmp (name, data->theme_custom->name) &&
        theme_delete (name, THEME_TYPE_META)) {
      /* remove theme from the model, too */
      GtkTreeIter child;

      if (gtk_tree_model_iter_next (model, &iter) ||
          theme_model_iter_last (model, &iter))
        theme_select_iter (icon_view, &iter);

      gtk_tree_model_get_iter (model, &iter, path);
      gtk_tree_model_sort_convert_iter_to_child_iter (
          GTK_TREE_MODEL_SORT (model), &child, &iter);
      gtk_list_store_remove (data->theme_store, &child);
    }

    g_list_foreach (selected, (GFunc) gtk_tree_path_free, NULL);
    g_list_free (selected);
    g_free (name);
  }
}

static void
theme_details_changed_cb (AppearanceData *data)
{
  GnomeThemeMetaInfo *gconf_theme;
  const GnomeThemeMetaInfo *selected;
  GtkIconView *icon_view;
  gboolean done = FALSE;

  /* load new state from gconf */
  gconf_theme = gnome_theme_meta_info_new ();
  theme_load_from_gconf (data->client, gconf_theme);

  /* check if it's our currently selected theme */
  icon_view = GTK_ICON_VIEW (glade_xml_get_widget (data->xml, "theme_list"));
  selected = theme_get_selected (icon_view, data);

  if (!selected || !(done = theme_is_equal (selected, gconf_theme))) {
    /* look for a matching metatheme */
    GList *theme_list, *l;

    theme_list = gnome_theme_meta_info_find_all ();

    for (l = theme_list; l; l = l->next) {
      GnomeThemeMetaInfo *info = l->data;

      if (theme_is_equal (gconf_theme, info)) {
        theme_select_name (icon_view, info->name);
        done = TRUE;
        break;
      }
    }
    g_list_free (theme_list);
  }

  if (!done)
    /* didn't find a match, set or update custom */
    theme_set_custom_from_theme (gconf_theme, data);

  gnome_theme_meta_info_free (gconf_theme);
}

static void
theme_color_scheme_changed_cb (GObject *settings,
                               GParamSpec *pspec,
                               AppearanceData *data)
{
  theme_details_changed_cb (data);
}

static void
theme_gconf_changed (GConfClient *client,
                     guint conn_id,
                     GConfEntry *entry,
                     AppearanceData *data)
{
  theme_details_changed_cb (data);
}

static void
theme_postinit (GtkIconView *icon_view, AppearanceData *data)
{
  /* connect to individual gconf changes; we only do that now to make sure
   * we don't receive any signals before the widgets have been realized */
  gconf_client_add_dir (data->client, "/apps/metacity/general", GCONF_CLIENT_PRELOAD_NONE, NULL);
  gconf_client_add_dir (data->client, "/desktop/gnome/interface", GCONF_CLIENT_PRELOAD_NONE, NULL);
  gconf_client_notify_add (data->client, GTK_THEME_KEY, (GConfClientNotifyFunc) theme_gconf_changed, data, NULL, NULL);
  gconf_client_notify_add (data->client, METACITY_THEME_KEY, (GConfClientNotifyFunc) theme_gconf_changed, data, NULL, NULL);
  gconf_client_notify_add (data->client, ICON_THEME_KEY, (GConfClientNotifyFunc) theme_gconf_changed, data, NULL, NULL);

  g_signal_connect (gtk_settings_get_default (), "notify::gtk-color-scheme", (GCallback) theme_color_scheme_changed_cb, data);
}

static gint
theme_list_sort_func (GtkTreeModel *model,
                      GtkTreeIter *a,
                      GtkTreeIter *b,
                      gpointer user_data)
{
  const gchar *a_name = NULL, *a_label = NULL;
  const gchar *b_name = NULL, *b_label = NULL;

  gtk_tree_model_get (model, a, COL_NAME, &a_name, COL_LABEL, &a_label, -1);
  gtk_tree_model_get (model, b, COL_NAME, &b_name, COL_LABEL, &b_label, -1);

  if (!strcmp (a_name, CUSTOM_THEME_NAME))
    return -1;
  if (!strcmp (b_name, CUSTOM_THEME_NAME))
    return 1;

  return strcmp (a_label, b_label);
}

void
themes_init (AppearanceData *data)
{
  GtkWidget *w, *del_button;
  GList *theme_list, *l;
  GtkListStore *theme_store;
  GtkTreeModel *sort_model;
  GnomeThemeMetaInfo *meta_theme = NULL;

  /* initialise some stuff */
  gnome_theme_init (NULL);
  gnome_wm_manager_init ();

  data->theme_save_dialog = NULL;
  data->theme_custom = gnome_theme_meta_info_new ();
  data->theme_icon = gdk_pixbuf_new_from_file (GNOMECC_PIXMAP_DIR "/theme-thumbnailing.png", NULL);
  data->theme_store = theme_store =
      gtk_list_store_new (NUM_COLS, GDK_TYPE_PIXBUF, G_TYPE_STRING, G_TYPE_STRING);

  del_button = glade_xml_get_widget (data->xml, "theme_delete");

  /* set up theme list */
  theme_list = gnome_theme_meta_info_find_all ();
  gnome_theme_info_register_theme_change ((ThemeChangedCallback) theme_changed_on_disk_cb, data);

  data->theme_custom->name = g_strdup (CUSTOM_THEME_NAME);
  data->theme_custom->readable_name = g_strdup_printf ("<b>%s</b>", _("Custom"));
  theme_load_from_gconf (data->client, data->theme_custom);

  for (l = theme_list; l; l = l->next) {
    GnomeThemeMetaInfo *info = l->data;

    gtk_list_store_insert_with_values (theme_store, NULL, 0,
        COL_LABEL, info->readable_name,
        COL_NAME, info->name,
        COL_THUMBNAIL, data->theme_icon,
        -1);

    theme_thumbnail_generate (info, data);

    if (!meta_theme && theme_is_equal (data->theme_custom, info))
      meta_theme = info;
  }
  g_list_free (theme_list);

  if (!meta_theme) {
    /* add custom theme */
    meta_theme = data->theme_custom;

    gtk_list_store_insert_with_values (theme_store, NULL, 0,
        COL_LABEL, meta_theme->readable_name,
        COL_NAME, meta_theme->name,
        COL_THUMBNAIL, data->theme_icon,
        -1);

    theme_thumbnail_generate (meta_theme, data);
  }

  w = glade_xml_get_widget (data->xml, "theme_list");
  gtk_icon_view_set_selection_mode (GTK_ICON_VIEW (w), GTK_SELECTION_BROWSE);
  sort_model = gtk_tree_model_sort_new_with_model (GTK_TREE_MODEL (theme_store));
  gtk_icon_view_set_model (GTK_ICON_VIEW (w), GTK_TREE_MODEL (sort_model));
  gtk_tree_sortable_set_sort_func (GTK_TREE_SORTABLE (sort_model), COL_LABEL, theme_list_sort_func, NULL, NULL);
  gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (sort_model), COL_LABEL, GTK_SORT_ASCENDING);
  g_signal_connect (w, "selection-changed", (GCallback) theme_selection_changed_cb, data);
  g_signal_connect_after (w, "realize", (GCallback) theme_select_name, meta_theme->name);
  g_signal_connect_after (w, "realize", (GCallback) theme_postinit, data);

  w = glade_xml_get_widget (data->xml, "theme_install");
  gtk_button_set_image (GTK_BUTTON (w),
                        gtk_image_new_from_stock (GTK_STOCK_OPEN, GTK_ICON_SIZE_BUTTON));

  /* connect button signals */
  g_signal_connect (w, "clicked", (GCallback) theme_install_cb, data);
  g_signal_connect (glade_xml_get_widget (data->xml, "theme_save"), "clicked", (GCallback) theme_save_cb, data);
  g_signal_connect (glade_xml_get_widget (data->xml, "theme_custom"), "clicked", (GCallback) theme_custom_cb, data);
  g_signal_connect (del_button, "clicked", (GCallback) theme_delete_cb, data);

  if (is_locked_down (data->client)) {
    /* FIXME: determine what needs disabling */
  }
}

void
themes_shutdown (AppearanceData *data)
{
  gnome_theme_meta_info_free (data->theme_custom);

  if (data->theme_icon)
    g_object_unref (data->theme_icon);
  if (data->theme_save_dialog)
    gtk_widget_destroy (data->theme_save_dialog);
}
