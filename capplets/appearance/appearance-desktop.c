/*
 * Copyright (C) 2007 The GNOME Foundation
 * Written by Rodney Dawes <dobey@ximian.com>
 *            Denis Washington <denisw@svn.gnome.org>
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
#include "gnome-wp-info.h"
#include "gnome-wp-item.h"
#include "gnome-wp-xml.h"
#include "wp-cellrenderer.h"
#include <glib/gi18n.h>
#include <string.h>
#include <gconf/gconf-client.h>
#include <libgnomeui/gnome-thumbnail.h>
#define GNOME_DESKTOP_USE_UNSTABLE_API
#include <libgnomeui/gnome-bg.h>

enum {
  TARGET_URI_LIST,
  TARGET_BGIMAGE
};

static const GtkTargetEntry drop_types[] = {
  { "text/uri-list", 0, TARGET_URI_LIST },
  { "property/bgimage", 0, TARGET_BGIMAGE }
};

static const GtkTargetEntry drag_types[] = {
  {"text/uri-list", GTK_TARGET_OTHER_WIDGET, TARGET_URI_LIST}
};

static void
select_item (AppearanceData *data,
             GnomeWPItem * item,
             gboolean scroll)
{
  GtkTreePath *path;

  g_return_if_fail (data != NULL);

  if (item == NULL)
    return;

  path = gtk_tree_row_reference_get_path (item->rowref);

  gtk_icon_view_select_path (data->wp_view, path);

  if (scroll)
    gtk_icon_view_scroll_to_path (data->wp_view, path, FALSE, 0.5, 0.0);

  gtk_tree_path_free (path);
}

static GnomeWPItem *
get_selected_item (AppearanceData *data,
                   GtkTreeIter *iter)
{
  GnomeWPItem *item = NULL;
  GList *selected;

  selected = gtk_icon_view_get_selected_items (data->wp_view);

  if (selected != NULL)
  {
    GtkTreeIter sel_iter;
    gchar *wpfile;

    gtk_tree_model_get_iter (data->wp_model, &sel_iter,
                             selected->data);

    g_list_foreach (selected, (GFunc) gtk_tree_path_free, NULL);
    g_list_free (selected);

    if (iter)
      *iter = sel_iter;

    gtk_tree_model_get (data->wp_model, &sel_iter, 2, &wpfile, -1);

    item = g_hash_table_lookup (data->wp_hash, wpfile);
    g_free (wpfile);
  }

  return item;
}

static gboolean predicate (gpointer key, gpointer value, gpointer data)
{
  GnomeBG *bg = data;
  GnomeWPItem *item = value;

  return item->bg == bg;
}

static void on_item_changed (GnomeBG *bg, AppearanceData *data) {
  GtkTreeModel *model;
  GtkTreeIter iter;
  GtkTreePath *path;
  GnomeWPItem *item;

  item = g_hash_table_find (data->wp_hash, predicate, bg);

  if (!item)
    return;

  model = gtk_tree_row_reference_get_model (item->rowref);
  path = gtk_tree_row_reference_get_path (item->rowref);

  if (gtk_tree_model_get_iter (model, &iter, path)) {
    GdkPixbuf *pixbuf;

    g_signal_handlers_block_by_func (bg, G_CALLBACK (on_item_changed), data);

    pixbuf = gnome_wp_item_get_thumbnail (item, data->thumb_factory);
    if (pixbuf) {
      gtk_list_store_set (GTK_LIST_STORE (data->wp_model), &iter,
                          0, pixbuf,
                         -1);
      g_object_unref (pixbuf);
    }

    g_signal_handlers_unblock_by_func (bg, G_CALLBACK (on_item_changed), data);
  }
}

static void
wp_props_load_wallpaper (gchar *key,
                         GnomeWPItem *item,
                         AppearanceData *data)
{
  GtkTreeIter iter;
  GtkTreePath *path;
  GdkPixbuf *pixbuf;

  if (item->deleted == TRUE)
    return;

  gtk_list_store_append (GTK_LIST_STORE (data->wp_model), &iter);

  pixbuf = gnome_wp_item_get_thumbnail (item, data->thumb_factory);
  gnome_wp_item_update_description (item);

  gtk_list_store_set (GTK_LIST_STORE (data->wp_model), &iter,
                      0, pixbuf,
                      1, item->description,
                      2, item->filename,
                      -1);

  if (pixbuf != NULL)
    g_object_unref (pixbuf);

  path = gtk_tree_model_get_path (data->wp_model, &iter);
  item->rowref = gtk_tree_row_reference_new (data->wp_model, path);
  g_signal_connect (item->bg, "changed", G_CALLBACK (on_item_changed), data);
  gtk_tree_path_free (path);
}

static GnomeWPItem *
wp_add_image (AppearanceData *data,
              const gchar *filename)
{
  GnomeWPItem *item;

  item = g_hash_table_lookup (data->wp_hash, filename);

  if (item != NULL)
  {
    if (item->deleted)
    {
      item->deleted = FALSE;
      wp_props_load_wallpaper (item->filename, item, data);
    }
  }
  else
  {
    item = gnome_wp_item_new (filename, data->wp_hash, data->thumb_factory);

    if (item != NULL)
    {
      wp_props_load_wallpaper (item->filename, item, data);
    }
  }

  return item;
}

static void
wp_add_images (AppearanceData *data,
               GSList *images)
{
  GdkWindow *window;
  GdkCursor *cursor;
  GnomeWPItem *item;

  window = glade_xml_get_widget (data->xml, "appearance_window")->window;

  item = NULL;
  cursor = gdk_cursor_new_for_display (gdk_display_get_default (),
                                       GDK_WATCH);
  gdk_window_set_cursor (window, cursor);
  gdk_cursor_unref (cursor);

  while (images != NULL)
  {
    gchar *uri = images->data;

    item = wp_add_image (data, uri);
    images = g_slist_remove (images, uri);
    g_free (uri);
  }

  gdk_window_set_cursor (window, NULL);

  if (item != NULL)
  {
    select_item (data, item, TRUE);
  }
}

static void
wp_option_menu_set (AppearanceData *data,
                    int value,
                    gboolean shade_type)
{
  if (shade_type)
  {
    gtk_combo_box_set_active (GTK_COMBO_BOX (data->wp_color_menu),
                              value);

    if (value == GNOME_BG_COLOR_SOLID)
      gtk_widget_hide (data->wp_scpicker);
    else
      gtk_widget_show (data->wp_scpicker);
  }
  else
  {
    gtk_combo_box_set_active (GTK_COMBO_BOX (data->wp_style_menu),
                              value);
  }
}

static void
wp_set_sensitivities (AppearanceData *data)
{
  GnomeWPItem *item;
  gchar *filename = NULL;

  item = get_selected_item (data, NULL);

  if (item != NULL)
    filename = item->filename;

  if (!gconf_client_key_is_writable (data->client, WP_OPTIONS_KEY, NULL)
      || (filename && !strcmp (filename, "(none)")))
    gtk_widget_set_sensitive (data->wp_style_menu, FALSE);
  else
    gtk_widget_set_sensitive (data->wp_style_menu, TRUE);

  if (!gconf_client_key_is_writable (data->client, WP_SHADING_KEY, NULL))
    gtk_widget_set_sensitive (data->wp_color_menu, FALSE);
  else
    gtk_widget_set_sensitive (data->wp_color_menu, TRUE);

  if (!gconf_client_key_is_writable (data->client, WP_PCOLOR_KEY, NULL))
    gtk_widget_set_sensitive (data->wp_pcpicker, FALSE);
  else
    gtk_widget_set_sensitive (data->wp_pcpicker, TRUE);

  if (!gconf_client_key_is_writable (data->client, WP_SCOLOR_KEY, NULL))
    gtk_widget_set_sensitive (data->wp_scpicker, FALSE);
  else
    gtk_widget_set_sensitive (data->wp_scpicker, TRUE);

  if (!filename || !strcmp (filename, "(none)"))
    gtk_widget_set_sensitive (data->wp_rem_button, FALSE);
  else
    gtk_widget_set_sensitive (data->wp_rem_button, TRUE);
}

static void
wp_scale_type_changed (GtkComboBox *combobox,
                       AppearanceData *data)
{
  GnomeWPItem *item;
  GtkTreeIter iter;
  GdkPixbuf *pixbuf;

  item = get_selected_item (data, &iter);

  if (item == NULL)
    return;

  item->options = gtk_combo_box_get_active (GTK_COMBO_BOX (data->wp_style_menu));

  pixbuf = gnome_wp_item_get_thumbnail (item, data->thumb_factory);
  gtk_list_store_set (GTK_LIST_STORE (data->wp_model),
                      &iter,
                      0, pixbuf,
                      -1);
  if (pixbuf != NULL)
    g_object_unref (pixbuf);

  if (gconf_client_key_is_writable (data->client, WP_OPTIONS_KEY, NULL))
    gconf_client_set_string (data->client, WP_OPTIONS_KEY,
                             wp_item_option_to_string (item->options), NULL);
}

static void
wp_shade_type_changed (GtkWidget *combobox,
                       AppearanceData *data)
{
  GnomeWPItem *item;
  GtkTreeIter iter;
  GdkPixbuf *pixbuf;

  item = get_selected_item (data, &iter);

  if (item == NULL)
    return;

  item->shade_type = gtk_combo_box_get_active (GTK_COMBO_BOX (data->wp_color_menu));

  pixbuf = gnome_wp_item_get_thumbnail (item, data->thumb_factory);
  gtk_list_store_set (GTK_LIST_STORE (data->wp_model), &iter,
                      0, pixbuf,
                      -1);
  if (pixbuf != NULL)
    g_object_unref (pixbuf);

  if (gconf_client_key_is_writable (data->client, WP_SHADING_KEY, NULL))
    gconf_client_set_string (data->client, WP_SHADING_KEY,
                             wp_item_shading_to_string (item->shade_type), NULL);
}

static void
wp_color_changed (AppearanceData *data,
                  gboolean update)
{
  GnomeWPItem *item;

  item = get_selected_item (data, NULL);

  if (item == NULL)
    return;

  gtk_color_button_get_color (GTK_COLOR_BUTTON (data->wp_pcpicker), item->pcolor);
  gtk_color_button_get_color (GTK_COLOR_BUTTON (data->wp_scpicker), item->scolor);

  if (update)
  {
    gchar *pcolor, *scolor;

    pcolor = gdk_color_to_string (item->pcolor);
    scolor = gdk_color_to_string (item->scolor);
    gconf_client_set_string (data->client, WP_PCOLOR_KEY, pcolor, NULL);
    gconf_client_set_string (data->client, WP_SCOLOR_KEY, scolor, NULL);
    g_free (pcolor);
    g_free (scolor);
  }

  wp_shade_type_changed (NULL, data);
}

static void
wp_scolor_changed (GtkWidget *widget,
                   AppearanceData *data)
{
  wp_color_changed (data, TRUE);
}

static void
wp_remove_wallpaper (GtkWidget *widget,
                     AppearanceData *data)
{
  GnomeWPItem *item;
  GtkTreeIter iter;
  GtkTreePath *path;

  item = get_selected_item (data, &iter);

  if (item)
  {
    item->deleted = TRUE;

    if (gtk_list_store_remove (GTK_LIST_STORE (data->wp_model), &iter))
      path = gtk_tree_model_get_path (data->wp_model, &iter);
    else
      path = gtk_tree_path_new_first ();

    gtk_icon_view_select_path (data->wp_view, path);
    gtk_tree_path_free (path);
  }
}

static void
wp_uri_changed (const gchar *uri,
                AppearanceData *data)
{
  GnomeWPItem *item, *selected;

  item = g_hash_table_lookup (data->wp_hash, uri);
  selected = get_selected_item (data, NULL);

  if (selected != NULL && strcmp (selected->filename, uri) != 0)
  {
    if (item == NULL)
      item = wp_add_image (data, uri);

    select_item (data, item, TRUE);
  }
}

static void
wp_file_changed (GConfClient *client, guint id,
                 GConfEntry *entry,
                 AppearanceData *data)
{
  const gchar *uri;
  gchar *wpfile;

  uri = gconf_value_get_string (entry->value);

  if (g_utf8_validate (uri, -1, NULL) && g_file_test (uri, G_FILE_TEST_EXISTS))
    wpfile = g_strdup (uri);
  else
    wpfile = g_filename_from_utf8 (uri, -1, NULL, NULL, NULL);

  wp_uri_changed (wpfile, data);

  g_free (wpfile);
}

static void
wp_options_changed (GConfClient *client, guint id,
                    GConfEntry *entry,
                    AppearanceData *data)
{
  GnomeWPItem *item;
  const gchar *option;

  option = gconf_value_get_string (entry->value);

  /* "none" means we don't use a background image */
  if (option == NULL || !strcmp (option, "none"))
  {
    /* temporarily disconnect so we don't override settings when
     * updating the selection */
    data->wp_update_gconf = FALSE;
    wp_uri_changed ("(none)", data);
    data->wp_update_gconf = TRUE;
    return;
  }

  item = get_selected_item (data, NULL);

  if (item != NULL)
  {
    item->options = wp_item_string_to_option (option);
    wp_option_menu_set (data, item->options, FALSE);
  }
}

static void
wp_shading_changed (GConfClient *client, guint id,
                    GConfEntry *entry,
                    AppearanceData *data)
{
  GnomeWPItem *item;

  wp_set_sensitivities (data);

  item = get_selected_item (data, NULL);

  if (item != NULL)
  {
    item->shade_type = wp_item_string_to_shading (gconf_value_get_string (entry->value));
    wp_option_menu_set (data, item->shade_type, TRUE);
  }
}

static void
wp_color1_changed (GConfClient *client, guint id,
                   GConfEntry *entry,
                   AppearanceData *data)
{
  GdkColor color;
  const gchar *colorhex;

  colorhex = gconf_value_get_string (entry->value);

  gdk_color_parse (colorhex, &color);

  gtk_color_button_set_color (GTK_COLOR_BUTTON (data->wp_pcpicker), &color);

  wp_color_changed (data, FALSE);
}

static void
wp_color2_changed (GConfClient *client, guint id,
                   GConfEntry *entry,
                   AppearanceData *data)
{
  GdkColor color;
  const gchar *colorhex;

  wp_set_sensitivities (data);

  colorhex = gconf_value_get_string (entry->value);

  gdk_color_parse (colorhex, &color);

  gtk_color_button_set_color (GTK_COLOR_BUTTON (data->wp_scpicker), &color);

  wp_color_changed (data, FALSE);
}

static gboolean
wp_props_wp_set (AppearanceData *data, GnomeWPItem *item)
{
  GConfChangeSet *cs;
  gchar *pcolor, *scolor;

  cs = gconf_change_set_new ();

  if (!strcmp (item->filename, "(none)"))
  {
    gconf_change_set_set_string (cs, WP_OPTIONS_KEY, "none");
    gconf_change_set_set_string (cs, WP_FILE_KEY, "");
  }
  else
  {
    gchar *uri;

    if (g_utf8_validate (item->filename, -1, NULL))
      uri = g_strdup (item->filename);
    else
      uri = g_filename_to_utf8 (item->filename, -1, NULL, NULL, NULL);

    if (uri == NULL) {
      g_warning ("Failed to convert filename to UTF-8: %s", item->filename);
    } else {
      gconf_change_set_set_string (cs, WP_FILE_KEY, uri);
      g_free (uri);
    }

    gconf_change_set_set_string (cs, WP_OPTIONS_KEY,
                                 wp_item_option_to_string (item->options));
  }

  gconf_change_set_set_string (cs, WP_SHADING_KEY,
                               wp_item_shading_to_string (item->shade_type));

  pcolor = gdk_color_to_string (item->pcolor);
  scolor = gdk_color_to_string (item->scolor);
  gconf_change_set_set_string (cs, WP_PCOLOR_KEY, pcolor);
  gconf_change_set_set_string (cs, WP_SCOLOR_KEY, scolor);
  g_free (pcolor);
  g_free (scolor);

  gconf_client_commit_change_set (data->client, cs, TRUE, NULL);

  gconf_change_set_unref (cs);

  return FALSE;
}

static void
wp_props_wp_selected (GtkTreeSelection *selection,
                      AppearanceData *data)
{
  GnomeWPItem *item;

  item = get_selected_item (data, NULL);

  if (item != NULL)
  {
    wp_set_sensitivities (data);

    if (strcmp (item->filename, "(none)") != 0)
      wp_option_menu_set (data, item->options, FALSE);

    wp_option_menu_set (data, item->shade_type, TRUE);

    gtk_color_button_set_color (GTK_COLOR_BUTTON (data->wp_pcpicker),
                                item->pcolor);
    gtk_color_button_set_color (GTK_COLOR_BUTTON (data->wp_scpicker),
                                item->scolor);

    if (data->wp_update_gconf)
      wp_props_wp_set (data, item);
  }
  else
  {
    gtk_widget_set_sensitive (data->wp_rem_button, FALSE);
  }
}

static void
wp_file_open_dialog (GtkWidget *widget,
                     AppearanceData *data)
{
  GSList *files;

  switch (gtk_dialog_run (GTK_DIALOG (data->wp_filesel)))
  {
  case GTK_RESPONSE_OK:
    files = gtk_file_chooser_get_filenames (data->wp_filesel);
    wp_add_images (data, files);
  case GTK_RESPONSE_CANCEL:
  default:
    gtk_widget_hide (GTK_WIDGET (data->wp_filesel));
    break;
  }
}

static void
wp_drag_received (GtkWidget *widget,
                  GdkDragContext *context,
                  gint x, gint y,
                  GtkSelectionData *selection_data,
                  guint info, guint time,
                  AppearanceData *data)
{
  if (info == TARGET_URI_LIST || info == TARGET_BGIMAGE)
  {
    GList * uris;
    GSList * realuris = NULL;

    uris = gnome_vfs_uri_list_parse ((gchar *) selection_data->data);

    if (uris != NULL && uris->data != NULL)
    {
      GdkWindow *window;
      GdkCursor *cursor;

      window = glade_xml_get_widget (data->xml, "appearance_window")->window;

      cursor = gdk_cursor_new_for_display (gdk_display_get_default (),
             GDK_WATCH);
      gdk_window_set_cursor (window, cursor);
      gdk_cursor_unref (cursor);

      for (; uris != NULL; uris = uris->next)
      {
        realuris = g_slist_append (realuris,
            g_strdup (gnome_vfs_uri_get_path (uris->data)));
      }

      wp_add_images (data, realuris);
      gdk_window_set_cursor (window, NULL);
    }
    gnome_vfs_uri_list_free (uris);
  }
}

static void
wp_drag_get_data (GtkWidget *widget,
		  GdkDragContext *context,
		  GtkSelectionData *selection_data,
		  guint type, guint time,
		  AppearanceData *data)
{
  if (type == TARGET_URI_LIST) {
    GnomeWPItem *item = get_selected_item (data, NULL);

    if (item != NULL) {
      gchar *uris[] = { item->filename, NULL };
      gtk_selection_data_set_uris (selection_data, uris);
    }
  }
}

static gboolean
wp_view_tooltip_cb (GtkWidget  *widget,
                    gint x,
                    gint y,
                    gboolean keyboard_mode,
                    GtkTooltip *tooltip,
                    AppearanceData *data)
{
  GtkTreeIter iter;
  gchar *wpfile;
  GnomeWPItem *item;

  if (gtk_icon_view_get_tooltip_context (data->wp_view,
                                         &x, &y,
                                         keyboard_mode,
                                         NULL,
                                         NULL,
                                         &iter))
    {
      gtk_tree_model_get (data->wp_model, &iter, 2, &wpfile, -1);
      item = g_hash_table_lookup (data->wp_hash, wpfile);
      g_free (wpfile);

      gtk_tooltip_set_markup (tooltip, item->description);

      return TRUE;
    }

  return FALSE;
}

static gint
wp_list_sort (GtkTreeModel *model,
              GtkTreeIter *a, GtkTreeIter *b,
              AppearanceData *data)
{
  gchar *foo, *bar;
  gchar *desca, *descb;
  gint retval;

  gtk_tree_model_get (model, a, 1, &desca, 2, &foo, -1);
  gtk_tree_model_get (model, b, 1, &descb, 2, &bar, -1);

  if (!strcmp (foo, "(none)"))
  {
    retval =  -1;
  }
  else if (!strcmp (bar, "(none)"))
  {
    retval =  1;
  }
  else
  {
    retval = g_utf8_collate (desca, descb);
  }

  g_free (desca);
  g_free (descb);
  g_free (foo);
  g_free (bar);

  return retval;
}

static void
wp_update_preview (GtkFileChooser *chooser,
                   AppearanceData *data)
{
  gchar *uri;

  uri = gtk_file_chooser_get_preview_uri (chooser);

  if (uri)
  {
    GdkPixbuf *pixbuf = NULL;
    gchar *mime_type;

    mime_type = gnome_vfs_get_mime_type (uri);

    if (mime_type)
    {
      pixbuf = gnome_thumbnail_factory_generate_thumbnail (data->thumb_factory,
                                                           uri,
                                                           mime_type);
      g_free (mime_type);
    }

    if (pixbuf != NULL)
    {
      gtk_image_set_from_pixbuf (GTK_IMAGE (data->wp_image), pixbuf);
      g_object_unref (pixbuf);
    }
    else
    {
      gtk_image_set_from_stock (GTK_IMAGE (data->wp_image),
                                "gtk-dialog-question",
                                GTK_ICON_SIZE_DIALOG);
    }
  }

  gtk_file_chooser_set_preview_widget_active (chooser, TRUE);
}

static gboolean
wp_load_stuffs (void *user_data)
{
  AppearanceData *data;
  gchar *imagepath, *uri, *style;
  GnomeWPItem *item;

  data = (AppearanceData *) user_data;

  gnome_wp_xml_load_list (data);
  g_hash_table_foreach (data->wp_hash, (GHFunc) wp_props_load_wallpaper,
                        data);

  style = gconf_client_get_string (data->client,
                                   WP_OPTIONS_KEY,
                                   NULL);
  if (style == NULL)
    style = g_strdup ("none");

  uri = gconf_client_get_string (data->client,
                                 WP_FILE_KEY,
                                 NULL);
  if (uri == NULL)
    uri = g_strdup ("(none)");

  if (g_utf8_validate (uri, -1, NULL) && g_file_test (uri, G_FILE_TEST_EXISTS))
    imagepath = g_strdup (uri);
  else
    imagepath = g_filename_from_utf8 (uri, -1, NULL, NULL, NULL);

  g_free (uri);

  item = g_hash_table_lookup (data->wp_hash, imagepath);

  if (item != NULL)
  {
    /* update with the current gconf settings */
    gnome_wp_item_update (item);

    if (strcmp (style, "none") != 0)
    {
      if (item->deleted == TRUE)
      {
        item->deleted = FALSE;
        wp_props_load_wallpaper (item->filename, item, data);
      }

      select_item (data, item, FALSE);
    }
  }
  else if (strcmp (style, "none") != 0)
  {
    item = wp_add_image (data, imagepath);
    select_item (data, item, FALSE);
  }

  item = g_hash_table_lookup (data->wp_hash, "(none)");
  if (item == NULL)
  {
    item = gnome_wp_item_new ("(none)", data->wp_hash, data->thumb_factory);
    if (item != NULL)
    {
      wp_props_load_wallpaper (item->filename, item, data);
    }
  }
  else
  {
    if (item->deleted == TRUE)
    {
      item->deleted = FALSE;
      wp_props_load_wallpaper (item->filename, item, data);
    }

    if (!strcmp (style, "none"))
    {
      select_item (data, item, FALSE);
      wp_option_menu_set (data, GNOME_BG_PLACEMENT_SCALED, FALSE);
    }
  }
  g_free (imagepath);
  g_free (style);

  if (data->wp_uris) {
    wp_add_images (data, data->wp_uris);
    data->wp_uris = NULL;
  }

  return FALSE;
}

static void
wp_select_after_realize (GtkWidget *widget,
                         AppearanceData *data)
{
  GnomeWPItem *item = get_selected_item (data, NULL);

  if (item == NULL)
    item = g_hash_table_lookup (data->wp_hash, "(none)");

  select_item (data, item, TRUE);
}

void
desktop_init (AppearanceData *data,
	      const gchar **uris)
{
  GtkWidget *add_button;
  GtkCellRenderer *cr;
  GtkFileFilter *filter;
  const gchar *pictures;
  const gchar *start_dir;

  g_object_set (gtk_settings_get_default (), "gtk-tooltip-timeout", 500, NULL);

  data->wp_update_gconf = TRUE;

  data->wp_uris = NULL;
  if (uris != NULL) {
    while (*uris != NULL) {
      data->wp_uris = g_slist_append (data->wp_uris, g_strdup (*uris));
      uris++;
    }
  }

  data->wp_hash = g_hash_table_new (g_str_hash, g_str_equal);

  gconf_client_add_dir (data->client, WP_KEYBOARD_PATH,
      GCONF_CLIENT_PRELOAD_ONELEVEL, NULL);
  gconf_client_add_dir (data->client, WP_PATH_KEY,
      GCONF_CLIENT_PRELOAD_ONELEVEL, NULL);

  gconf_client_notify_add (data->client,
                           WP_FILE_KEY,
                           (GConfClientNotifyFunc) wp_file_changed,
                           data, NULL, NULL);
  gconf_client_notify_add (data->client,
                           WP_OPTIONS_KEY,
                           (GConfClientNotifyFunc) wp_options_changed,
                           data, NULL, NULL);
  gconf_client_notify_add (data->client,
                           WP_SHADING_KEY,
                           (GConfClientNotifyFunc) wp_shading_changed,
                           data, NULL, NULL);
  gconf_client_notify_add (data->client,
                           WP_PCOLOR_KEY,
                           (GConfClientNotifyFunc) wp_color1_changed,
                           data, NULL, NULL);
  gconf_client_notify_add (data->client,
                           WP_SCOLOR_KEY,
                           (GConfClientNotifyFunc) wp_color2_changed,
                           data, NULL, NULL);

  data->wp_model = GTK_TREE_MODEL (gtk_list_store_new (3, GDK_TYPE_PIXBUF,
                                                 G_TYPE_STRING,
                                                 G_TYPE_STRING));

  data->wp_view = GTK_ICON_VIEW (glade_xml_get_widget (data->xml, "wp_view"));
  gtk_icon_view_set_model (data->wp_view, GTK_TREE_MODEL (data->wp_model));

  g_signal_connect_after (data->wp_view, "realize",
                          (GCallback) wp_select_after_realize, data);

  gtk_cell_layout_clear (GTK_CELL_LAYOUT (data->wp_view));

  cr = cell_renderer_wallpaper_new ();
  g_object_set (cr, "xpad", 5, "ypad", 5, NULL);

  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (data->wp_view), cr, TRUE);
  gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (data->wp_view), cr,
                                  "pixbuf", 0,
                                  NULL);

  gtk_tree_sortable_set_sort_func (GTK_TREE_SORTABLE (data->wp_model), 2,
                                   (GtkTreeIterCompareFunc) wp_list_sort,
                                   data, NULL);

  gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (data->wp_model),
                                        2, GTK_SORT_ASCENDING);

  gtk_drag_dest_set (GTK_WIDGET (data->wp_view), GTK_DEST_DEFAULT_ALL, drop_types,
                     G_N_ELEMENTS (drop_types), GDK_ACTION_COPY | GDK_ACTION_MOVE);
  g_signal_connect (data->wp_view, "drag_data_received",
                    (GCallback) wp_drag_received, data);

  gtk_drag_source_set (GTK_WIDGET (data->wp_view), GDK_BUTTON1_MASK,
                       drag_types, G_N_ELEMENTS (drag_types), GDK_ACTION_COPY);
  g_signal_connect (data->wp_view, "drag-data-get",
		    (GCallback) wp_drag_get_data, data);

  data->wp_style_menu = glade_xml_get_widget (data->xml, "wp_style_menu");

  g_signal_connect (data->wp_style_menu, "changed",
                    (GCallback) wp_scale_type_changed, data);

  data->wp_color_menu = glade_xml_get_widget (data->xml, "wp_color_menu");

  g_signal_connect (data->wp_color_menu, "changed",
                    (GCallback) wp_shade_type_changed, data);

  data->wp_scpicker = glade_xml_get_widget (data->xml, "wp_scpicker");

  g_signal_connect (data->wp_scpicker, "color-set",
                    (GCallback) wp_scolor_changed, data);

  data->wp_pcpicker = glade_xml_get_widget (data->xml, "wp_pcpicker");

  g_signal_connect (data->wp_pcpicker, "color-set",
                    (GCallback) wp_scolor_changed, data);

  add_button = glade_xml_get_widget (data->xml, "wp_add_button");
  gtk_button_set_image (GTK_BUTTON (add_button),
                        gtk_image_new_from_stock ("gtk-add", GTK_ICON_SIZE_BUTTON));

  g_signal_connect (add_button, "clicked",
                    (GCallback) wp_file_open_dialog, data);

  data->wp_rem_button = glade_xml_get_widget (data->xml, "wp_rem_button");

  g_signal_connect (data->wp_rem_button, "clicked",
                    (GCallback) wp_remove_wallpaper, data);

  g_idle_add (wp_load_stuffs, data);

  g_signal_connect (data->wp_view, "selection-changed",
                    (GCallback) wp_props_wp_selected, data);
  g_signal_connect (data->wp_view, "query-tooltip",
                    (GCallback) wp_view_tooltip_cb, data);

  wp_set_sensitivities (data);

  data->wp_filesel = GTK_FILE_CHOOSER (
  		     gtk_file_chooser_dialog_new_with_backend (_("Add Wallpaper"),
                     GTK_WINDOW (glade_xml_get_widget (data->xml, "appearance_window")),
                     GTK_FILE_CHOOSER_ACTION_OPEN,
                     "gtk+",
                     GTK_STOCK_CANCEL,
                     GTK_RESPONSE_CANCEL,
                     GTK_STOCK_OPEN,
                     GTK_RESPONSE_OK,
                     NULL));

  gtk_dialog_set_default_response (GTK_DIALOG (data->wp_filesel), GTK_RESPONSE_OK);
  gtk_file_chooser_set_select_multiple (data->wp_filesel, TRUE);
  gtk_file_chooser_set_use_preview_label (data->wp_filesel, FALSE);

  start_dir = g_get_home_dir ();

  if (g_file_test ("/usr/share/backgrounds", G_FILE_TEST_IS_DIR)) {
    gtk_file_chooser_add_shortcut_folder (data->wp_filesel,
                                          "/usr/share/backgrounds", NULL);
    start_dir = "/usr/share/backgrounds";
  }

  pictures = g_get_user_special_dir (G_USER_DIRECTORY_PICTURES);
  if (pictures != NULL && g_file_test (pictures, G_FILE_TEST_IS_DIR)) {
    gtk_file_chooser_add_shortcut_folder (data->wp_filesel, pictures, NULL);
    start_dir = pictures;
  }

  gtk_file_chooser_set_current_folder (data->wp_filesel, start_dir);

  filter = gtk_file_filter_new ();
  gtk_file_filter_add_pixbuf_formats (filter);
  gtk_file_filter_set_name (filter, _("Images"));
  gtk_file_chooser_add_filter (data->wp_filesel, filter);

  filter = gtk_file_filter_new ();
  gtk_file_filter_set_name (filter, _("All files"));
  gtk_file_filter_add_pattern (filter, "*");
  gtk_file_chooser_add_filter (data->wp_filesel, filter);

  data->wp_image = gtk_image_new ();
  gtk_file_chooser_set_preview_widget (data->wp_filesel, data->wp_image);
  gtk_widget_set_size_request (data->wp_image, 128, -1);

  gtk_widget_show (data->wp_image);

  g_signal_connect (data->wp_filesel, "update-preview",
                    (GCallback) wp_update_preview, data);
}

void
desktop_shutdown (AppearanceData *data)
{
  gnome_wp_xml_save_list (data);
  g_slist_foreach (data->wp_uris, (GFunc) g_free, NULL);
  g_slist_free (data->wp_uris);
  g_object_ref_sink (data->wp_filesel);
  g_object_unref (data->wp_filesel);
}
