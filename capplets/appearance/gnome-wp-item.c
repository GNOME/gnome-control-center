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
#include <libgnomevfs/gnome-vfs-mime-handlers.h>
#define GNOME_DESKTOP_USE_UNSTABLE_API
#include <libgnomeui/gnome-bg.h>

#include "gnome-wp-item.h"
#include "gnome-wp-utils.h"

static void set_bg_properties (GnomeWPItem *item)
{
  GnomeBGColorType color;
  GnomeBGPlacement placement;

  color = GNOME_BG_COLOR_SOLID;

  if (item->shade_type) {
    if (!strcmp (item->shade_type, "horizontal-gradient")) {
      color = GNOME_BG_COLOR_H_GRADIENT;
    } else if (!strcmp (item->shade_type, "vertical-gradient")) {
      color = GNOME_BG_COLOR_V_GRADIENT;
    }
  }

  placement = GNOME_BG_PLACEMENT_TILED;

  if (item->options) {
    if (!strcmp (item->options, "centered")) {
      placement = GNOME_BG_PLACEMENT_CENTERED;
    } else if (!strcmp (item->options, "stretched")) {
      placement = GNOME_BG_PLACEMENT_FILL_SCREEN;
    } else if (!strcmp (item->options, "scaled")) {
      placement = GNOME_BG_PLACEMENT_SCALED;
    } else if (!strcmp (item->options, "zoom")) {
      placement = GNOME_BG_PLACEMENT_ZOOMED;
    }
  }

  if (item->filename)
    gnome_bg_set_uri (item->bg, item->filename);

  gnome_bg_set_color (item->bg, color, item->pcolor, item->scolor);
  gnome_bg_set_placement (item->bg, placement);
}

void gnome_wp_item_ensure_gnome_bg (GnomeWPItem *item)
{
  if (!item->bg) {
    item->bg = gnome_bg_new ();

    set_bg_properties (item);
  }
}

GnomeWPItem * gnome_wp_item_new (const gchar * filename,
				 GHashTable * wallpapers,
				 GnomeThumbnailFactory * thumbnails) {
  GnomeWPItem * item = NULL;
  GdkColor color1 = { 0, 0, 0, 0 }, color2 = { 0, 0, 0, 0 };
  gchar * col_str;
  GConfClient * client;

  client = gconf_client_get_default ();

  item = g_new0 (GnomeWPItem, 1);

  item->filename = gnome_vfs_unescape_string_for_display (filename);

  item->fileinfo = gnome_wp_info_new (item->filename, thumbnails);

  if (item->fileinfo != NULL &&
      (g_str_has_prefix (item->fileinfo->mime_type, "image/") ||
       strcmp (item->fileinfo->mime_type, "application/xml") == 0)) {
    if (item->name == NULL) {
      if (g_utf8_validate (item->fileinfo->name, -1, NULL))
	item->name = g_strdup (item->fileinfo->name);
      else
	item->name = g_filename_to_utf8 (item->fileinfo->name, -1, NULL,
					 NULL, NULL);
    }

    item->options = gconf_client_get_string (client, WP_OPTIONS_KEY, NULL);
    if (item->options == NULL || !strcmp (item->options, "none")) {
      g_free (item->options);
      item->options = g_strdup ("scaled");
    }

    item->shade_type = gconf_client_get_string (client, WP_SHADING_KEY, NULL);
    if (item->shade_type == NULL)
      item->shade_type = g_strdup ("solid");

    col_str = gconf_client_get_string (client, WP_PCOLOR_KEY, NULL);
    if (col_str != NULL) {
      gdk_color_parse (col_str, &color1);
      g_free (col_str);
    }

    col_str = gconf_client_get_string (client, WP_SCOLOR_KEY, NULL);
    if (col_str != NULL) {
      gdk_color_parse (col_str, &color2);
      g_free (col_str);
    }

    item->pcolor = gdk_color_copy (&color1);
    item->scolor = gdk_color_copy (&color2);

    gnome_wp_item_update_description (item);

    g_hash_table_insert (wallpapers, item->filename, item);
  } else {
    gnome_wp_item_free (item);
    item = NULL;
  }

  if (item) {
    gnome_wp_item_ensure_gnome_bg (item);
  }

  g_object_unref (client);

  return item;
}

void gnome_wp_item_free (GnomeWPItem * item) {
  if (item == NULL) {
    return;
  }

  g_free (item->name);
  g_free (item->filename);
  g_free (item->description);
  g_free (item->options);
  g_free (item->shade_type);

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

static void collect_save_options (GdkPixbuf * pixbuf,
				  gchar *** keys,
				  gchar *** vals,
				  gint width,
				  gint height) {
  gchar ** options;
  gint n, count;

  count = 0;

  options = g_object_get_qdata (G_OBJECT (pixbuf),
				g_quark_from_static_string ("gdk_pixbuf_options"));
  if (options) {
    for (n = 0; options[2 * n]; n++) {
      ++count;

      *keys = g_realloc (*keys, sizeof (gchar *) * (count + 1));
      *vals = g_realloc (*vals, sizeof (gchar *) * (count + 1));

      (*keys)[count - 1] = g_strdup (options[2 * n]);
      (*vals)[count - 1] = g_strdup (options[2 * n + 1]);

      (*keys)[count] = NULL;
      (*vals)[count] = NULL;
    }
  }
  ++count;

  *keys = g_realloc (*keys, sizeof (gchar *) * (count + 1));
  *vals = g_realloc (*vals, sizeof (gchar *) * (count + 1));

  (*keys)[count - 1] = g_strdup ("tEXt::Thumb::Image::Width");
  (*vals)[count - 1] = g_strdup_printf ("%d", width);

  (*keys)[count] = NULL;
  (*vals)[count] = NULL;

  ++count;

  *keys = g_realloc (*keys, sizeof (gchar *) * (count + 1));
  *vals = g_realloc (*vals, sizeof (gchar *) * (count + 1));

  (*keys)[count - 1] = g_strdup ("tEXt::Thumb::Image::Height");
  (*vals)[count - 1] = g_strdup_printf ("%d", height);

  (*keys)[count] = NULL;
  (*vals)[count] = NULL;
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
      description = gnome_vfs_mime_get_description (item->fileinfo->mime_type);

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
