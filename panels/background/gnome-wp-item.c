/*
 *  Authors: Rodney Dawes <dobey@ximian.com>
 *
 *  Copyright 2003-2006 Novell, Inc. (www.novell.com)
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of version 2 of the GNU General Public License
 *  as published by the Free Software Foundation
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Street #330, Boston, MA 02111-1307, USA.
 *
 */

#include <config.h>

#include <glib/gi18n.h>
#include <string.h>
#include "gnome-wp-item.h"

static struct {
  GDesktopBackgroundStyle value;
  gchar *string;
} options_lookup[] = {
  { G_DESKTOP_BACKGROUND_STYLE_CENTERED, "centered" },
  { G_DESKTOP_BACKGROUND_STYLE_STRETCHED, "stretched" },
  { G_DESKTOP_BACKGROUND_STYLE_SCALED, "scaled" },
  { G_DESKTOP_BACKGROUND_STYLE_ZOOM, "zoom" },
  { G_DESKTOP_BACKGROUND_STYLE_WALLPAPER, "wallpaper" },
  { G_DESKTOP_BACKGROUND_STYLE_SPANNED, "spanned" },
  { 0, NULL }
};

static struct {
  GDesktopBackgroundShading value;
  gchar *string;
} shade_lookup[] = {
  { G_DESKTOP_BACKGROUND_SHADING_SOLID, "solid" },
  { G_DESKTOP_BACKGROUND_SHADING_HORIZONTAL, "horizontal-gradient" },
  { G_DESKTOP_BACKGROUND_SHADING_VERTICAL, "vertical-gradient" },
  { 0, NULL }
};

const gchar *wp_item_option_to_string (GDesktopBackgroundStyle type)
{
  int i;

  for (i = 0; options_lookup[i].value != 0; i++) {
    if (options_lookup[i].value == type)
      return (const gchar *) options_lookup[i].string;
  }

  return "scaled";
}

const gchar *wp_item_shading_to_string (GDesktopBackgroundShading type)
{
  int i;

  for (i = 0; shade_lookup[i].value != 0; i++) {
    if (shade_lookup[i].value == type)
      return (const gchar *) shade_lookup[i].string;
  }

  return "solid";
}

GDesktopBackgroundStyle wp_item_string_to_option (const gchar *option)
{
  gint i;

  for (i = 0; options_lookup[i].value != 0; i++) {
    if (g_str_equal (options_lookup[i].string, option))
      return options_lookup[i].value;
  }

  return G_DESKTOP_BACKGROUND_STYLE_SCALED;
}

GDesktopBackgroundShading wp_item_string_to_shading (const gchar *shade_type)
{
  int i;

  for (i = 0; shade_lookup[i].value != 0; i++) {
    if (g_str_equal (shade_lookup[i].string, shade_type))
      return options_lookup[i].value;
  }
  return G_DESKTOP_BACKGROUND_SHADING_SOLID;
}

static void set_bg_properties (GnomeWPItem *item)
{
  if (!item->bg)
    return;

  if (item->filename)
    gnome_bg_set_filename (item->bg, item->filename);

  gnome_bg_set_color (item->bg, item->shade_type, item->pcolor, item->scolor);
  gnome_bg_set_placement (item->bg, item->options);
}

void gnome_wp_item_ensure_gnome_bg (GnomeWPItem *item)
{
  if (!item->bg)
    item->bg = gnome_bg_new ();

  g_object_set_data (G_OBJECT (item->bg), "gnome-wp-item", item);

  set_bg_properties (item);
}

void gnome_wp_item_update (GnomeWPItem *item) {
  GSettings *settings;
  GdkColor color1 = { 0, 0, 0, 0 }, color2 = { 0, 0, 0, 0 };
  gchar *s;

  settings = g_settings_new (WP_PATH_ID);

  item->options = g_settings_get_enum (settings, WP_OPTIONS_KEY);
  item->shade_type = g_settings_get_enum (settings,WP_SHADING_KEY);

  s = g_settings_get_string (settings, WP_PCOLOR_KEY);
  if (s != NULL) {
    gdk_color_parse (s, &color1);
    g_free (s);
  }

  s = g_settings_get_string (settings, WP_SCOLOR_KEY);
  if (s != NULL) {
    gdk_color_parse (s, &color2);
    g_free (s);
  }

  g_object_unref (settings);

  if (item->pcolor != NULL)
    gdk_color_free (item->pcolor);

  if (item->scolor != NULL)
    gdk_color_free (item->scolor);

  item->pcolor = gdk_color_copy (&color1);
  item->scolor = gdk_color_copy (&color2);

  set_bg_properties (item);
}

GnomeWPItem * gnome_wp_item_new (const gchar * filename,
				 GHashTable * wallpapers,
				 GFileInfo * file_info,
				 GnomeDesktopThumbnailFactory * thumbnails) {
  GnomeWPItem *item = g_new0 (GnomeWPItem, 1);

  item->filename = g_strdup (filename);
  item->fileinfo = gnome_wp_info_new (filename, file_info, thumbnails);

  if (item->fileinfo != NULL && item->fileinfo->mime_type != NULL &&
      (g_str_has_prefix (item->fileinfo->mime_type, "image/") ||
       strcmp (item->fileinfo->mime_type, "application/xml") == 0)) {

    if (g_utf8_validate (item->fileinfo->name, -1, NULL))
      item->name = g_strdup (item->fileinfo->name);
    else
      item->name = g_filename_to_utf8 (item->fileinfo->name, -1, NULL,
				       NULL, NULL);

    gnome_wp_item_update (item);
    gnome_wp_item_ensure_gnome_bg (item);
    gnome_wp_item_update_size (item, NULL);

    if (wallpapers)
      g_hash_table_insert (wallpapers, item->filename, item);
  } else {
    gnome_wp_item_free (item);
    item = NULL;
  }

  return item;
}

void gnome_wp_item_free (GnomeWPItem * item) {
  if (item == NULL) {
    return;
  }

  g_free (item->name);
  g_free (item->filename);
  g_free (item->size);

  if (item->pcolor != NULL)
    gdk_color_free (item->pcolor);

  if (item->scolor != NULL)
    gdk_color_free (item->scolor);

  gnome_wp_info_free (item->fileinfo);
  if (item->bg)
    g_object_unref (item->bg);

  gtk_tree_row_reference_free (item->rowref);

  g_free (item->source_url);

  g_free (item);
}

static GEmblem *
get_slideshow_icon (void)
{
	GIcon *themed;
	GEmblem *emblem;

	themed = g_themed_icon_new ("slideshow-emblem");
	emblem = g_emblem_new_with_origin (themed, G_EMBLEM_ORIGIN_DEVICE);
	g_object_unref (themed);
	return emblem;
}

GIcon * gnome_wp_item_get_frame_thumbnail (GnomeWPItem * item,
					       GnomeDesktopThumbnailFactory * thumbs,
                                               int width,
                                               int height,
                                               gint frame) {
  GdkPixbuf *pixbuf = NULL;
  GIcon *icon = NULL;

  set_bg_properties (item);

  if (frame >= 0)
    pixbuf = gnome_bg_create_frame_thumbnail (item->bg, thumbs, gdk_screen_get_default (), width, height, frame);
  else
    pixbuf = gnome_bg_create_thumbnail (item->bg, thumbs, gdk_screen_get_default(), width, height);

  if (pixbuf && frame != -2 && gnome_bg_changes_with_time (item->bg))
    {
      GEmblem *emblem;

      emblem = get_slideshow_icon ();
      icon = g_emblemed_icon_new (G_ICON (pixbuf), emblem);
      g_object_unref (emblem);
      g_object_unref (pixbuf);
    }
  else
    {
      icon = G_ICON (pixbuf);
    }

  gnome_bg_get_image_size (item->bg, thumbs, width, height, &item->width, &item->height);

  return icon;
}


GIcon * gnome_wp_item_get_thumbnail (GnomeWPItem * item,
					 GnomeDesktopThumbnailFactory * thumbs,
                                         gint width,
                                         gint height) {
  return gnome_wp_item_get_frame_thumbnail (item, thumbs, width, height, -1);
}

void gnome_wp_item_update_size (GnomeWPItem * item,
				GnomeDesktopThumbnailFactory * thumbs) {
  g_free (item->size);
  item->size = NULL;

  if (!strcmp (item->filename, "(none)")) {
    item->size = g_strdup ("");
  } else {
    if (gnome_bg_has_multiple_sizes (item->bg) || gnome_bg_changes_with_time (item->bg))
      item->size = g_strdup (_("multiple sizes"));
    else {
      if (thumbs != NULL && (item->width <= 0 || item->height <= 0)) {
        gnome_bg_get_image_size (item->bg, thumbs, 1, 1, &item->width, &item->height);
      }
      if (item->width > 0 && item->height > 0) {
        /* translators: 100 × 100px
         * Note that this is not an "x", but U+00D7 MULTIPLICATION SIGN */
        item->size = g_strdup_printf (_("%d \303\227 %d"),
				      item->width,
				      item->height);
      } else {
        item->size = g_strdup ("");
      }
    }
  }
}
