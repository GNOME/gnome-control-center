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
#include <gnome.h>
#include <string.h>
#include "gnome-wp-item.h"

static GConfEnumStringPair options_lookup[] = {
  { GNOME_BG_PLACEMENT_CENTERED, "centered" },
  { GNOME_BG_PLACEMENT_FILL_SCREEN, "stretched" },
  { GNOME_BG_PLACEMENT_SCALED, "scaled" },
  { GNOME_BG_PLACEMENT_ZOOMED, "zoomed" },
  { GNOME_BG_PLACEMENT_TILED, "wallpaper" },
  { 0, NULL }
};

static GConfEnumStringPair shade_lookup[] = {
  { GNOME_BG_COLOR_SOLID, "solid" },
  { GNOME_BG_COLOR_H_GRADIENT, "horizontal-gradient" },
  { GNOME_BG_COLOR_V_GRADIENT, "vertical-gradient" },
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
  if (item->filename)
    gnome_bg_set_uri (item->bg, item->filename);

  gnome_bg_set_color (item->bg, item->shade_type, item->pcolor, item->scolor);
  gnome_bg_set_placement (item->bg, item->options);
}

void gnome_wp_item_ensure_gnome_bg (GnomeWPItem *item)
{
  if (!item->bg) {
    item->bg = gnome_bg_new ();

    set_bg_properties (item);
  }
}

void gnome_wp_item_update (GnomeWPItem *item) {
  GConfClient *client;
  GdkColor color1 = { 0, 0, 0, 0 }, color2 = { 0, 0, 0, 0 };
  gchar *s;

  client = gconf_client_get_default ();

  s = gconf_client_get_string (client, WP_OPTIONS_KEY, NULL);
  item->options = wp_item_string_to_option (s);
  g_free (s);

  s = gconf_client_get_string (client, WP_SHADING_KEY, NULL);
  item->shade_type = wp_item_string_to_shading (s);
  g_free (s);

  s = gconf_client_get_string (client, WP_PCOLOR_KEY, NULL);
  if (s != NULL) {
    gdk_color_parse (s, &color1);
    g_free (s);
  }

  s = gconf_client_get_string (client, WP_SCOLOR_KEY, NULL);
  if (s != NULL) {
    gdk_color_parse (s, &color2);
    g_free (s);
  }

  g_object_unref (client);

  if (item->pcolor != NULL)
    gdk_color_free (item->pcolor);

  if (item->scolor != NULL)
    gdk_color_free (item->scolor);

  item->pcolor = gdk_color_copy (&color1);
  item->scolor = gdk_color_copy (&color2);
}

GnomeWPItem * gnome_wp_item_new (const gchar * filename,
				 GHashTable * wallpapers,
				 GnomeThumbnailFactory * thumbnails) {
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
    gnome_wp_item_update_description (item);
    gnome_wp_item_ensure_gnome_bg (item);

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

  g_free (item);
}

#define LIST_IMAGE_WIDTH 108

GdkPixbuf * gnome_wp_item_get_thumbnail (GnomeWPItem * item,
					 GnomeThumbnailFactory * thumbs) {
  GdkPixbuf *pixbuf;
  double aspect =
    (double)gdk_screen_get_height (gdk_screen_get_default()) /
    gdk_screen_get_width (gdk_screen_get_default());

  set_bg_properties (item);

  pixbuf = gnome_bg_create_thumbnail (item->bg, thumbs, gdk_screen_get_default(), LIST_IMAGE_WIDTH, LIST_IMAGE_WIDTH * aspect);

  gnome_bg_get_image_size (item->bg, thumbs, &item->width, &item->height);

  return pixbuf;
}

void gnome_wp_item_update_description (GnomeWPItem * item) {
  g_free (item->description);

  if (!strcmp (item->filename, "(none)")) {
    item->description = g_strdup (item->name);
  } else {
    const gchar *description;
    gchar *dirname = g_path_get_dirname (item->filename);

    if (strcmp (item->fileinfo->mime_type, "application/xml") == 0)
      description = _("Slide Show");
    else
      description = g_content_type_get_description (item->fileinfo->mime_type);

    /* translators: <b>wallpaper name</b>
     * mime type, x pixel(s) by y pixel(s)
     * Folder: /path/to/file
     */
    item->description = g_markup_printf_escaped (_("<b>%s</b>\n"
                                   "%s, %d %s by %d %s\n"
                                   "Folder: %s"),
                                 item->name,
                                 description,
                                 item->width,
                                 ngettext ("pixel", "pixels", item->width),
                                 item->height,
                                 ngettext ("pixel", "pixels", item->height),
                                 dirname);
    g_free (dirname);
  }
}
