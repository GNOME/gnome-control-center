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
#include <gconf/gconf-client.h>
#include <string.h>
#include "gnome-wp-item.h"

static GConfEnumStringPair options_lookup[] = {
  { G_DESKTOP_BACKGROUND_STYLE_CENTERED, "centered" },
  { G_DESKTOP_BACKGROUND_STYLE_STRETCHED, "stretched" },
  { G_DESKTOP_BACKGROUND_STYLE_SCALED, "scaled" },
  { G_DESKTOP_BACKGROUND_STYLE_ZOOM, "zoom" },
  { G_DESKTOP_BACKGROUND_STYLE_WALLPAPER, "wallpaper" },
  { G_DESKTOP_BACKGROUND_STYLE_SPANNED, "spanned" },
  { 0, NULL }
};

static GConfEnumStringPair shade_lookup[] = {
  { G_DESKTOP_BACKGROUND_SHADING_SOLID, "solid" },
  { G_DESKTOP_BACKGROUND_SHADING_HORIZONTAL, "horizontal-gradient" },
  { G_DESKTOP_BACKGROUND_SHADING_VERTICAL, "vertical-gradient" },
  { 0, NULL }
};

const gchar *wp_item_option_to_string (GnomeBGPlacement type)
{
  return gconf_enum_to_string (options_lookup, type);
}

const gchar *wp_item_shading_to_string (GnomeBGColorType type)
{
  return gconf_enum_to_string (shade_lookup, type);
}

GnomeBGPlacement wp_item_string_to_option (const gchar *option)
{
  int i = GNOME_BG_PLACEMENT_SCALED;
  gconf_string_to_enum (options_lookup, option, &i);
  return i;
}

GnomeBGColorType wp_item_string_to_shading (const gchar *shade_type)
{
  int i = GNOME_BG_COLOR_SOLID;
  gconf_string_to_enum (shade_lookup, shade_type, &i);
  return i;
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
  if (!item->bg) {
    item->bg = gnome_bg_new ();

    g_object_set_data (G_OBJECT (item->bg), "gnome-wp-item", item);

    set_bg_properties (item);
  }
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
				 GnomeDesktopThumbnailFactory * thumbnails) {
  GnomeWPItem *item = g_new0 (GnomeWPItem, 1);

  item->filename = g_strdup (filename);
  item->fileinfo = gnome_wp_info_new (filename, thumbnails);

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
    gnome_wp_item_update_description (item);

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
  g_free (item->description);

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

static GdkPixbuf *
add_slideshow_frame (GdkPixbuf *pixbuf)
{
  GdkPixbuf *sheet, *sheet2;
  GdkPixbuf *tmp;
  gint w, h;

  w = gdk_pixbuf_get_width (pixbuf);
  h = gdk_pixbuf_get_height (pixbuf);

  sheet = gdk_pixbuf_new (GDK_COLORSPACE_RGB, FALSE, 8, w, h);
  gdk_pixbuf_fill (sheet, 0x00000000);
  sheet2 = gdk_pixbuf_new_subpixbuf (sheet, 1, 1, w - 2, h - 2);
  gdk_pixbuf_fill (sheet2, 0xffffffff);
  g_object_unref (sheet2);

  tmp = gdk_pixbuf_new (GDK_COLORSPACE_RGB, TRUE, 8, w + 6, h + 6);

  gdk_pixbuf_fill (tmp, 0x00000000);
  gdk_pixbuf_composite (sheet, tmp, 6, 6, w, h, 6.0, 6.0, 1.0, 1.0, GDK_INTERP_NEAREST, 255);
  gdk_pixbuf_composite (sheet, tmp, 3, 3, w, h, 3.0, 3.0, 1.0, 1.0, GDK_INTERP_NEAREST, 255);
  gdk_pixbuf_composite (pixbuf, tmp, 0, 0, w, h, 0.0, 0.0, 1.0, 1.0, GDK_INTERP_NEAREST, 255);

  g_object_unref (sheet);

  return tmp;
}

GdkPixbuf * gnome_wp_item_get_frame_thumbnail (GnomeWPItem * item,
					       GnomeDesktopThumbnailFactory * thumbs,
                                               int width,
                                               int height,
                                               gint frame) {
  GdkPixbuf *pixbuf = NULL;

  set_bg_properties (item);

  if (frame != -1)
    pixbuf = gnome_bg_create_frame_thumbnail (item->bg, thumbs, gdk_screen_get_default (), width, height, frame);
  else
    pixbuf = gnome_bg_create_thumbnail (item->bg, thumbs, gdk_screen_get_default(), width, height);

  if (pixbuf && gnome_bg_changes_with_time (item->bg))
    {
      GdkPixbuf *tmp;

      tmp = add_slideshow_frame (pixbuf);
      g_object_unref (pixbuf);
      pixbuf = tmp;
    }

  gnome_bg_get_image_size (item->bg, thumbs, width, height, &item->width, &item->height);

  return pixbuf;
}


GdkPixbuf * gnome_wp_item_get_thumbnail (GnomeWPItem * item,
					 GnomeDesktopThumbnailFactory * thumbs,
                                         gint width,
                                         gint height) {
  return gnome_wp_item_get_frame_thumbnail (item, thumbs, width, height, -1);
}

void gnome_wp_item_update_description (GnomeWPItem * item) {
  g_free (item->description);

  if (!strcmp (item->filename, "(none)")) {
    item->description = g_strdup (item->name);
  } else {
    gchar *description;
    gchar *size;
    gchar *dirname = g_path_get_dirname (item->filename);

    description = NULL;
    size = NULL;

    if (strcmp (item->fileinfo->mime_type, "application/xml") == 0)
      {
        if (gnome_bg_changes_with_time (item->bg))
          description = g_strdup (_("Slide Show"));
        else if (item->width > 0 && item->height > 0)
          description = g_strdup (_("Image"));
      }
    else
      description = g_content_type_get_description (item->fileinfo->mime_type);

    if (gnome_bg_has_multiple_sizes (item->bg))
      size = g_strdup (_("multiple sizes"));
    else if (item->width > 0 && item->height > 0) {
      /* translators: x pixel(s) by y pixel(s) */
      size = g_strdup_printf (_("%d %s by %d %s"),
                              item->width,
                              ngettext ("pixel", "pixels", item->width),
                              item->height,
                              ngettext ("pixel", "pixels", item->height));
    }

    if (description && size) {
      /* translators: <b>wallpaper name</b>
       * mime type, size
       * Folder: /path/to/file
       */
      item->description = g_markup_printf_escaped (_("<b>%s</b>\n"
                                                     "%s, %s\n"
                                                     "Folder: %s"),
                                                   item->name,
                                                   description,
                                                   size,
                                                   dirname);
    } else {
      /* translators: <b>wallpaper name</b>
       * Image missing
       * Folder: /path/to/file
       */
      item->description = g_markup_printf_escaped (_("<b>%s</b>\n"
                                                     "%s\n"
                                                     "Folder: %s"),
                                                   item->name,
                                                   _("Image missing"),
                                                   dirname);
    }

    g_free (size);
    g_free (dirname);
    g_free (description);
  }
}
