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
#include "gnome-wp-utils.h"
#include "gnome-wp-xml.h"
#include <glib/gi18n.h>
#include <string.h>
#include <gconf/gconf-client.h>
#include <libgnomeui/gnome-thumbnail.h>

typedef enum {
  GNOME_WP_SHADE_TYPE_SOLID,
  GNOME_WP_SHADE_TYPE_HORIZ,
  GNOME_WP_SHADE_TYPE_VERT
} GnomeWPShadeType;

typedef enum {
  GNOME_WP_SCALE_TYPE_CENTERED,
  GNOME_WP_SCALE_TYPE_STRETCHED,
  GNOME_WP_SCALE_TYPE_SCALED,
  GNOME_WP_SCALE_TYPE_ZOOM,
  GNOME_WP_SCALE_TYPE_TILED
} GnomeWPScaleType;

enum {
  TARGET_URI_LIST,
  TARGET_URL,
  TARGET_COLOR,
  TARGET_BGIMAGE,
  TARGET_BACKGROUND_RESET
};

static GtkTargetEntry drop_types[] = {
  {"text/uri-list", 0, TARGET_URI_LIST},
  /*  { "application/x-color", 0, TARGET_COLOR }, */
  { "property/bgimage", 0, TARGET_BGIMAGE },
  /*  { "x-special/gnome-reset-background", 0, TARGET_BACKGROUND_RESET }*/
};

static void
scroll_to_item (AppearanceData *data,
                GnomeWPItem * item)
{
  GtkTreePath *path;

  g_return_if_fail (data != NULL);

  if (item == NULL)
    return;

  path = gtk_tree_row_reference_get_path (item->rowref);

  gtk_tree_view_set_cursor (data->wp_tree, path, NULL, FALSE);
  gtk_tree_view_scroll_to_cell (data->wp_tree, path, NULL, TRUE, 0.5, 0.0);
  gtk_tree_path_free (path);
}

static void
wp_props_load_wallpaper (gchar *key,
                         GnomeWPItem *item,
                         AppearanceData *data)
{
  GtkTreeModel *model;
  GtkTreeIter iter;
  GtkTreePath *path;
  GdkPixbuf *pixbuf;

  if (item->deleted == TRUE)
    return;

  model = gtk_tree_view_get_model (data->wp_tree);

  gtk_list_store_append (GTK_LIST_STORE (model), &iter);

  pixbuf = gnome_wp_item_get_thumbnail (item, data->wp_thumbs);
  gnome_wp_item_update_description (item);

  if (pixbuf != NULL)
  {
    gtk_list_store_set (GTK_LIST_STORE (model), &iter,
                        0, pixbuf,
                        1, item->description,
                        2, item->filename,
                        -1);
    g_object_unref (pixbuf);
  }
  else
  {
    gtk_list_store_set (GTK_LIST_STORE (model), &iter,
                        1, item->description,
                        2, item->filename,
                        -1);
  }

  path = gtk_tree_model_get_path (model, &iter);
  item->rowref = gtk_tree_row_reference_new (model, path);
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
    item = gnome_wp_item_new (filename, data->wp_hash, data->wp_thumbs);

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

  window = (glade_xml_get_widget (data->xml, "appearance_window"))->window;

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
    scroll_to_item (data, item);
  }
}

static void
wp_option_menu_set (AppearanceData *data,
                    const gchar *value,
                    gboolean shade_type)
{
  if (shade_type)
  {
    if (!strcmp (value, "horizontal-gradient"))
    {
      gtk_combo_box_set_active (GTK_COMBO_BOX (data->wp_color_menu),
                                GNOME_WP_SHADE_TYPE_HORIZ);
      gtk_widget_show (data->wp_scpicker);
    }
    else if (!strcmp (value, "vertical-gradient"))
    {
      gtk_combo_box_set_active (GTK_COMBO_BOX (data->wp_color_menu),
                                GNOME_WP_SHADE_TYPE_VERT);
      gtk_widget_show (data->wp_scpicker);
    }
    else
    {
      gtk_combo_box_set_active (GTK_COMBO_BOX (data->wp_color_menu),
                                GNOME_WP_SHADE_TYPE_SOLID);
      gtk_widget_hide (data->wp_scpicker);
    }
  }
  else
  {
    if (!strcmp (value, "centered"))
    {
      gtk_combo_box_set_active (GTK_COMBO_BOX (data->wp_style_menu),
                                GNOME_WP_SCALE_TYPE_CENTERED);
    }
    else if (!strcmp (value, "stretched"))
    {
      gtk_combo_box_set_active (GTK_COMBO_BOX (data->wp_style_menu),
                                GNOME_WP_SCALE_TYPE_STRETCHED);
    }
    else if (!strcmp (value, "scaled"))
    {
      gtk_combo_box_set_active (GTK_COMBO_BOX (data->wp_style_menu),
                                GNOME_WP_SCALE_TYPE_SCALED);
    }
    else if (!strcmp (value, "zoom"))
    {
      gtk_combo_box_set_active (GTK_COMBO_BOX (data->wp_style_menu),
                                GNOME_WP_SCALE_TYPE_ZOOM);
    }
    else if (!strcmp (value, "none"))
    {
      gtk_combo_box_set_active (GTK_COMBO_BOX (data->wp_style_menu),
                                GNOME_WP_SCALE_TYPE_TILED);
    }
  }
}

static void
wp_set_sensitivities (AppearanceData *data)
{
  GtkTreeIter iter;
  GtkTreeModel *model;
  GtkTreeSelection *selection;
  GnomeWPItem *item;
  gchar *wpfile;
  gchar *filename = NULL;

  selection = gtk_tree_view_get_selection (data->wp_tree);

  if (gtk_tree_selection_get_selected (selection, &model, &iter))
  {
    gtk_tree_model_get (model, &iter, 2, &wpfile, -1);

    item = g_hash_table_lookup (data->wp_hash, wpfile);
    filename = item->filename;
    g_free (wpfile);
  }

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
  GnomeWPItem *item = NULL;
  GdkPixbuf *pixbuf;
  GtkTreeIter iter;
  GtkTreeModel *model;
  GtkTreeSelection *selection;
  gchar *wpfile;

  selection = gtk_tree_view_get_selection (data->wp_tree);

  if (gtk_tree_selection_get_selected (selection, &model, &iter))
  {
    gtk_tree_model_get (model, &iter, 2, &wpfile, -1);

    item = g_hash_table_lookup (data->wp_hash, wpfile);

    g_free (wpfile);
  }

  if (item == NULL)
    return;

  g_free (item->options);

  switch (gtk_combo_box_get_active (GTK_COMBO_BOX (data->wp_style_menu)))
  {
  case GNOME_WP_SCALE_TYPE_CENTERED:
    item->options = g_strdup ("centered");
    break;
  case GNOME_WP_SCALE_TYPE_STRETCHED:
    item->options = g_strdup ("stretched");
    break;
  case GNOME_WP_SCALE_TYPE_SCALED:
    item->options = g_strdup ("scaled");
    break;
  case GNOME_WP_SCALE_TYPE_ZOOM:
    item->options = g_strdup ("zoom");
    break;
  case GNOME_WP_SCALE_TYPE_TILED:
  default:
    item->options = g_strdup ("wallpaper");
    break;
  }

  pixbuf = gnome_wp_item_get_thumbnail (item, data->wp_thumbs);
  gtk_list_store_set (GTK_LIST_STORE (gtk_tree_view_get_model (data->wp_tree)),
                      &iter,
                      0, pixbuf,
                      -1);
  g_object_unref (pixbuf);

  gconf_client_set_string (data->client, WP_OPTIONS_KEY, item->options, NULL);
}

static void
wp_shade_type_changed (GtkWidget *combobox,
                       AppearanceData *data)
{
  GnomeWPItem *item = NULL;
  GtkTreeIter iter;
  GtkTreeModel *model;
  GtkTreeSelection *selection;
  gchar *wpfile;
  GdkPixbuf *pixbuf;

  selection = gtk_tree_view_get_selection (data->wp_tree);

  if (gtk_tree_selection_get_selected (selection, &model, &iter))
  {
    gtk_tree_model_get (model, &iter, 2, &wpfile, -1);

    item = g_hash_table_lookup (data->wp_hash, wpfile);

    g_free (wpfile);
  }

  if (item == NULL)
    return;

  g_free (item->shade_type);

  switch (gtk_combo_box_get_active (GTK_COMBO_BOX (data->wp_color_menu)))
  {
  case GNOME_WP_SHADE_TYPE_HORIZ:
    item->shade_type = g_strdup ("horizontal-gradient");
    gtk_widget_show (data->wp_scpicker);
    break;
  case GNOME_WP_SHADE_TYPE_VERT:
    item->shade_type = g_strdup ("vertical-gradient");
    gtk_widget_show (data->wp_scpicker);
    break;
  case GNOME_WP_SHADE_TYPE_SOLID:
  default:
    item->shade_type = g_strdup ("solid");
    gtk_widget_hide (data->wp_scpicker);
    break;
  }

  pixbuf = gnome_wp_item_get_thumbnail (item, data->wp_thumbs);
  gtk_list_store_set (GTK_LIST_STORE (model), &iter,
                      0, pixbuf,
                      -1);
  g_object_unref (pixbuf);
  gconf_client_set_string (data->client, WP_SHADING_KEY,
                           item->shade_type, NULL);
}

static void
wp_color_changed (AppearanceData *data,
                  gboolean update)
{
  GnomeWPItem *item = NULL;
  GtkTreeIter iter;
  GtkTreeModel *model;
  GtkTreeSelection *selection;
  gchar *wpfile;

  selection = gtk_tree_view_get_selection (data->wp_tree);

  if (gtk_tree_selection_get_selected (selection, &model, &iter))
  {
    gtk_tree_model_get (model, &iter, 2, &wpfile, -1);

    item = g_hash_table_lookup (data->wp_hash, wpfile);

    g_free (wpfile);
  }

  if (item == NULL)
    return;

  g_free (item->pri_color);

  gtk_color_button_get_color (GTK_COLOR_BUTTON (data->wp_pcpicker), item->pcolor);

  item->pri_color = g_strdup_printf ("#%02X%02X%02X",
                                     item->pcolor->red >> 8,
                                     item->pcolor->green >> 8,
                                     item->pcolor->blue >> 8);

  g_free (item->sec_color);

  gtk_color_button_get_color (GTK_COLOR_BUTTON (data->wp_pcpicker), item->scolor);

  item->sec_color = g_strdup_printf ("#%02X%02X%02X",
                                     item->scolor->red >> 8,
                                     item->scolor->green >> 8,
                                     item->scolor->blue >> 8);

  if (update)
  {
    gconf_client_set_string (data->client, WP_PCOLOR_KEY,
                             item->pri_color, NULL);
    gconf_client_set_string (data->client, WP_SCOLOR_KEY,
                             item->sec_color, NULL);
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
  GtkTreeIter iter;
  GtkTreeModel *model;
  GtkTreeSelection *selection;

  selection = gtk_tree_view_get_selection (data->wp_tree);

  if (gtk_tree_selection_get_selected (selection, &model, &iter))
  {
    GnomeWPItem *item;
    gchar *wpfile;
    GtkTreePath *path;

    gtk_tree_model_get (model, &iter, 2, &wpfile, -1);

    item = g_hash_table_lookup (data->wp_hash, wpfile);
    item->deleted = TRUE;
    g_free (wpfile);

    if (gtk_list_store_remove (GTK_LIST_STORE (model), &iter))
      path = gtk_tree_model_get_path (model, &iter);
    else
      path = gtk_tree_path_new_first ();

    gtk_tree_view_set_cursor (data->wp_tree, path, NULL, FALSE);
    gtk_tree_path_free (path);
  }
}

static void
wp_uri_changed (const gchar *uri,
                AppearanceData *data)
{
  GtkTreeSelection * selection;
  GtkTreeModel * model;
  GtkTreeIter iter;
  GnomeWPItem * item;
  gchar * selected;

  item = g_hash_table_lookup (data->wp_hash, uri);

  selection = gtk_tree_view_get_selection (data->wp_tree);

  if (gtk_tree_selection_get_selected (selection, &model, &iter))
  {
    gtk_tree_model_get (model, &iter, 2, &selected, -1);

    if (strcmp (selected, uri) != 0)
    {
      if (item == NULL)
        item = wp_add_image (data, uri);

      scroll_to_item (data, item);
    }

    g_free (selected);
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
  GtkTreeSelection *selection;
  GtkTreeModel *model;
  GtkTreeIter iter;
  GnomeWPItem *item;
  const gchar *option;
  gchar *wpfile;

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

  selection = gtk_tree_view_get_selection (data->wp_tree);

  if (gtk_tree_selection_get_selected (selection, &model, &iter))
  {
    gtk_tree_model_get (model, &iter, 2, &wpfile, -1);

    item = g_hash_table_lookup (data->wp_hash, wpfile);

    if (item != NULL)
    {
      g_free (item->options);
      item->options = g_strdup (option);
      wp_option_menu_set (data, item->options, FALSE);
    }

    g_free (wpfile);
  }
}

static void
wp_shading_changed (GConfClient *client, guint id,
                    GConfEntry *entry,
                    AppearanceData *data)
{
  GtkTreeSelection *selection;
  GtkTreeModel *model;
  GtkTreeIter iter;
  GnomeWPItem *item;
  gchar *wpfile;

  wp_set_sensitivities (data);

  selection = gtk_tree_view_get_selection (data->wp_tree);

  if (gtk_tree_selection_get_selected (selection, &model, &iter))
  {
    gtk_tree_model_get (model, &iter, 2, &wpfile, -1);

    item = g_hash_table_lookup (data->wp_hash, wpfile);

    if (item != NULL)
    {
      g_free (item->shade_type);
      item->shade_type = g_strdup (gconf_value_get_string (entry->value));
      wp_option_menu_set (data, item->shade_type, TRUE);
    }

    g_free (wpfile);
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
wp_props_wp_set (AppearanceData *data)
{
  GtkTreeIter iter;
  GtkTreeModel *model;
  GtkTreeSelection *selection;
  GnomeWPItem *item;
  GConfChangeSet *cs;
  gchar *wpfile;

  selection = gtk_tree_view_get_selection (data->wp_tree);

  if (gtk_tree_selection_get_selected (selection, &model, &iter))
  {
    gtk_tree_model_get (model, &iter, 2, &wpfile, -1);

    item = g_hash_table_lookup (data->wp_hash, wpfile);

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

      gconf_change_set_set_string (cs, WP_FILE_KEY, uri);
      g_free (uri);

      gconf_change_set_set_string (cs, WP_OPTIONS_KEY, item->options);
    }

    gconf_change_set_set_string (cs, WP_SHADING_KEY, item->shade_type);

    gconf_change_set_set_string (cs, WP_PCOLOR_KEY, item->pri_color);
    gconf_change_set_set_string (cs, WP_SCOLOR_KEY, item->sec_color);

    gconf_client_commit_change_set (data->client, cs, TRUE, NULL);

    gconf_change_set_unref (cs);

    g_free (wpfile);
  }

  return FALSE;
}

static void
wp_props_wp_selected (GtkTreeSelection *selection,
                      AppearanceData *data)
{
  GtkTreeIter iter;
  GtkTreeModel *model;
  GnomeWPItem *item;
  gchar *wpfile;

  if (gtk_tree_selection_get_selected (selection, &model, &iter))
  {
    gtk_tree_model_get (model, &iter, 2, &wpfile, -1);

    item = g_hash_table_lookup (data->wp_hash, wpfile);

    wp_set_sensitivities (data);

    if (strcmp (item->filename, "(none)") != 0)
      wp_option_menu_set (data, item->options, FALSE);

    wp_option_menu_set (data, item->shade_type, TRUE);

    gtk_color_button_set_color (GTK_COLOR_BUTTON (data->wp_pcpicker),
                                item->pcolor);
    gtk_color_button_set_color (GTK_COLOR_BUTTON (data->wp_scpicker),
                                item->scolor);

    g_free (wpfile);
  }
  else
  {
    gtk_widget_set_sensitive (data->wp_rem_button, FALSE);
  }

  if (data->wp_update_gconf)
    wp_props_wp_set (data);
}

static void
wp_file_open_dialog (GtkWidget *widget,
                     AppearanceData *data)
{
  GSList *files;

  switch (gtk_dialog_run (GTK_DIALOG (data->wp_filesel)))
  {
  case GTK_RESPONSE_OK:
    files = gtk_file_chooser_get_filenames (GTK_FILE_CHOOSER (data->wp_filesel));
    wp_add_images (data, files);
  case GTK_RESPONSE_CANCEL:
  default:
    gtk_widget_hide (data->wp_filesel);
    break;
  }
}

static void
wp_tree_delete_event (GtkWidget *widget,
                      GdkEvent *event,
                      AppearanceData *data)
{
  gnome_wp_xml_save_list (data);
  g_object_unref (data->wp_thumbs);
}

static void
wp_dragged_image (GtkWidget *widget,
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

      window = (glade_xml_get_widget (data->xml, "appearance_window"))->window;

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
      pixbuf = gnome_thumbnail_factory_generate_thumbnail (data->wp_thumbs,
                                                           uri,
                                                           mime_type);
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

    g_free (mime_type);
  }

  gtk_file_chooser_set_preview_widget_active (chooser, TRUE);
}

static gboolean
wp_load_stuffs (void *user_data)
{
  AppearanceData *data;
  gchar * imagepath, * style, * uri;
  GnomeWPItem * item;

  data = (AppearanceData *) user_data;

  gnome_wp_xml_load_list (data);
  g_hash_table_foreach (data->wp_hash, (GHFunc) wp_props_load_wallpaper,
                        data);

  /*gdk_window_set_cursor (data->window->window, NULL);*/

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

  if (item != NULL && strcmp (style, "none") != 0)
  {
    if (item->deleted == TRUE)
    {
      item->deleted = FALSE;
      wp_props_load_wallpaper (item->filename, item, data);
    }

    scroll_to_item (data, item);

    wp_option_menu_set (data, item->options, FALSE);
    wp_option_menu_set (data, item->shade_type, TRUE);

    gtk_color_button_set_color (GTK_COLOR_BUTTON (data->wp_pcpicker), item->pcolor);
    gtk_color_button_set_color (GTK_COLOR_BUTTON (data->wp_scpicker), item->scolor);

  }
  else if (strcmp (style, "none") != 0)
  {
    item = wp_add_image (data, imagepath);
    scroll_to_item (data, item);
  }

  item = g_hash_table_lookup (data->wp_hash, "(none)");
  if (item == NULL)
  {
    item = gnome_wp_item_new ("(none)", data->wp_hash, data->wp_thumbs);
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
      scroll_to_item (data, item);
    }

  }
  g_free (imagepath);
  g_free (style);

  return FALSE;
}

void
desktop_init (AppearanceData *data)
{
  GtkTreeViewColumn *column;
  GtkCellRenderer *renderer;
  GtkTreeSelection *selection;
  GtkFileFilter *filter;

  data->wp_hash = g_hash_table_new (g_str_hash, g_str_equal);

  data->wp_thumbs = gnome_thumbnail_factory_new (GNOME_THUMBNAIL_SIZE_NORMAL);

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

  g_signal_connect_after (G_OBJECT (glade_xml_get_widget (data->xml, "appearance_window")), "delete-event",
                          G_CALLBACK (wp_tree_delete_event), data);

  data->wp_tree = GTK_TREE_VIEW (glade_xml_get_widget (data->xml, "wp_tree"));
  gtk_tree_view_set_model (data->wp_tree, data->wp_model);

  column = gtk_tree_view_column_new ();
  renderer = gtk_cell_renderer_pixbuf_new ();
  gtk_tree_view_column_pack_start (column, renderer, FALSE);
  gtk_tree_view_column_set_attributes (column, renderer,
                                       "pixbuf", 0,
                                       NULL);

  renderer = gtk_cell_renderer_text_new ();
  gtk_tree_view_column_pack_start (column, renderer, FALSE);
  gtk_tree_view_column_set_attributes (column, renderer,
                                       "markup", 1,
                                       NULL);
  gtk_tree_view_column_set_spacing (column, 6);

  gtk_tree_view_append_column (data->wp_tree, column);

  gtk_tree_sortable_set_sort_func (GTK_TREE_SORTABLE (data->wp_model), 2,
                                   (GtkTreeIterCompareFunc) wp_list_sort,
                                   data, NULL);

  gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (data->wp_model),
                                        2, GTK_SORT_ASCENDING);

  gtk_drag_dest_set (GTK_WIDGET (data->wp_tree), GTK_DEST_DEFAULT_ALL, drop_types,
                     sizeof (drop_types) / sizeof (drop_types[0]),
                      GDK_ACTION_COPY | GDK_ACTION_MOVE);
  g_signal_connect (G_OBJECT (data->wp_tree), "drag_data_received",
                    G_CALLBACK (wp_dragged_image), data);

  data->wp_style_menu = glade_xml_get_widget (data->xml, "wp_style_menu");

  gtk_combo_box_append_text (GTK_COMBO_BOX (data->wp_style_menu), _("Centered"));
  gtk_combo_box_append_text (GTK_COMBO_BOX (data->wp_style_menu), _("Fill Screen"));
  gtk_combo_box_append_text (GTK_COMBO_BOX (data->wp_style_menu), _("Scaled"));
  gtk_combo_box_append_text (GTK_COMBO_BOX (data->wp_style_menu), _("Zoom"));
  gtk_combo_box_append_text (GTK_COMBO_BOX (data->wp_style_menu), _("Tiled"));

  g_signal_connect (G_OBJECT (data->wp_style_menu), "changed",
                    G_CALLBACK (wp_scale_type_changed), data);

  data->wp_color_menu = glade_xml_get_widget (data->xml, "wp_color_menu");

  gtk_combo_box_append_text (GTK_COMBO_BOX (data->wp_color_menu), _("Solid Color"));
  gtk_combo_box_append_text (GTK_COMBO_BOX (data->wp_color_menu), _("Horizontal Gradient"));
  gtk_combo_box_append_text (GTK_COMBO_BOX (data->wp_color_menu), _("Vertical Gradient"));

  g_signal_connect (G_OBJECT (data->wp_color_menu), "changed",
                    G_CALLBACK (wp_shade_type_changed), data);

  data->wp_scpicker = glade_xml_get_widget (data->xml, "wp_scpicker");

  g_signal_connect (G_OBJECT (data->wp_scpicker), "color-set",
                    G_CALLBACK (wp_scolor_changed), data);

  data->wp_pcpicker = glade_xml_get_widget (data->xml, "wp_pcpicker");

  g_signal_connect (G_OBJECT (data->wp_pcpicker), "color-set",
                    G_CALLBACK (wp_scolor_changed), data);

  data->wp_rem_button = glade_xml_get_widget (data->xml, "wp_rem_button");

  g_signal_connect (G_OBJECT (data->wp_rem_button), "clicked",
                    G_CALLBACK (wp_remove_wallpaper), data);

  g_signal_connect (G_OBJECT (glade_xml_get_widget (data->xml, "wp_add_button")), "clicked",
                    G_CALLBACK (wp_file_open_dialog), data);

  g_idle_add (wp_load_stuffs, data);

  selection = gtk_tree_view_get_selection (data->wp_tree);
  g_signal_connect (G_OBJECT (selection), "changed",
                    G_CALLBACK (wp_props_wp_selected), data);

  wp_set_sensitivities (data);

  data->wp_filesel = gtk_file_chooser_dialog_new_with_backend (_("Add Wallpaper"),
                     GTK_WINDOW (glade_xml_get_widget (data->xml, "appearance_window")),
                     GTK_FILE_CHOOSER_ACTION_OPEN,
                     "gtk+",
                     GTK_STOCK_CANCEL,
                     GTK_RESPONSE_CANCEL,
                     GTK_STOCK_OPEN,
                     GTK_RESPONSE_OK,
                     NULL);

  gtk_file_chooser_set_select_multiple (GTK_FILE_CHOOSER (data->wp_filesel),
                                        TRUE);
  gtk_file_chooser_set_use_preview_label (GTK_FILE_CHOOSER (data->wp_filesel),
                                          FALSE);

  filter = gtk_file_filter_new ();
  gtk_file_filter_add_pixbuf_formats (filter);
  gtk_file_filter_set_name (filter, _("Images"));
  gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (data->wp_filesel), filter);

  filter = gtk_file_filter_new ();
  gtk_file_filter_set_name (filter, _("All Files"));
  gtk_file_filter_add_pattern (filter, "*");
  gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (data->wp_filesel), filter);

  data->wp_image = gtk_image_new ();
  gtk_file_chooser_set_preview_widget (GTK_FILE_CHOOSER (data->wp_filesel),
                                       data->wp_image);
  gtk_widget_set_size_request (data->wp_image, 128, -1);

  gtk_widget_show (data->wp_image);

  g_signal_connect (data->wp_filesel, "update-preview",
                    G_CALLBACK (wp_update_preview), data);
}
