/*
 *  Authors: Rodney Dawes <dobey@ximian.com>
 *
 *  Copyright 2003 Novell, Inc. (www.novell.com)
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

#include "gnome-wp-item.h"
#include "gnome-wp-utils.h"
#include <string.h>

void gnome_wp_item_free (GnomeWPItem * item) {
  if (item == NULL) {
    return;
  }

  g_free (item->name);
  g_free (item->filename);
  g_free (item->description);
  g_free (item->imguri);
  g_free (item->options);
  g_free (item->shade_type);

  g_free (item->pri_color);
  g_free (item->sec_color);

  gdk_color_free (item->pcolor);
  gdk_color_free (item->scolor);

  gnome_wp_info_free (item->fileinfo);
  gnome_wp_info_free (item->uriinfo);

  gtk_tree_row_reference_free (item->rowref);

  item = NULL;
}

GdkPixbuf * gnome_wp_item_get_thumbnail (GnomeWPItem * item,
					 GnomeThumbnailFactory * thumbs) {
  GdkPixbuf * pixbuf, * bgpixbuf;
  GdkPixbuf * scaled = NULL;
  gint w, h, ratio;
  gint bw, bh;

  /*
     Get the size of the screen and calculate our aspect ratio divisor
     We do this, so that images are thumbnailed as they would look on
     the screen in reality
  */
  w = gdk_screen_get_width (gdk_screen_get_default ());
  h = gdk_screen_get_height (gdk_screen_get_default ());
  ratio = h / 48;
  bw = w / ratio;
  bh = h / ratio;

  /*
     Create the pixbuf for the background colors, which will show up for
     oddly sized images, smaller images that are centered, or alpha images
  */
  if (!strcmp (item->shade_type, "solid")) {
    bgpixbuf = gnome_wp_pixbuf_new_solid (item->pcolor, w / ratio, h / ratio);
  } else if (!strcmp (item->shade_type, "vertical-gradient")) {
    bgpixbuf = gnome_wp_pixbuf_new_gradient (GTK_ORIENTATION_VERTICAL,
					     item->pcolor, item->scolor,
					     w / ratio, h / ratio);
  } else {
    bgpixbuf = gnome_wp_pixbuf_new_gradient (GTK_ORIENTATION_HORIZONTAL,
					     item->pcolor, item->scolor,
					     w / ratio, h / ratio);
  }

  /*
     Load up the thumbnail image using the thumbnail spec
     If the image doesn't exist, we create it
     If we are creating the thumbnail for "No Wallpaper", then we just copy
     the background colors pixbuf we created above, here
  */
  if (item->fileinfo->thumburi != NULL &&
      g_file_test (item->fileinfo->thumburi, G_FILE_TEST_EXISTS)) {
    pixbuf = gdk_pixbuf_new_from_file (item->fileinfo->thumburi, NULL);
  } else if (!strcmp (item->filename, "(none)")) {
    pixbuf = gdk_pixbuf_copy (bgpixbuf);
  } else {
    pixbuf = gnome_thumbnail_factory_generate_thumbnail (thumbs,
							 gnome_vfs_escape_path_string (item->filename),
							 item->fileinfo->mime_type);
    gnome_thumbnail_factory_save_thumbnail (thumbs, pixbuf,
					    gnome_vfs_escape_path_string (item->filename),
					    item->fileinfo->mtime);
  }

  if (pixbuf != NULL) {
    w = gdk_pixbuf_get_width (pixbuf);
    h = gdk_pixbuf_get_height (pixbuf);

    /*
       Handle images large and small. We default to 1, since images smaller
       than 64x48 don't need to be scaled down, and the tiled thumbnails
       will look correct for really small pattern images
    */
    if (h >= 48)
      ratio = h / 48;
    else if (w >= 64)
      ratio = w / 64;
    else
      ratio = 1;

    scaled = gnome_thumbnail_scale_down_pixbuf (pixbuf, w / ratio, h / ratio);

    if (!strcmp (item->options, "wallpaper")) {
      w = gdk_pixbuf_get_width (scaled);
      h = gdk_pixbuf_get_height (scaled);

      scaled = gnome_wp_pixbuf_tile (scaled, bgpixbuf);
    } else if (!strcmp (item->options, "centered")) {
      w = gdk_pixbuf_get_width (scaled);
      h = gdk_pixbuf_get_height (scaled);

      /*
	 This is for alpha centered images like gnome-logo-transparent.jpg
	 It's an ugly hack, that can potentially be removed when round() or
	 something like it decides to work
	 We scale it down again so that it looks proper, instead of the off-
	 center look that seems to appear without this hack
      */
      if (gdk_pixbuf_get_has_alpha (pixbuf) && (w > bw || h > bh))
	scaled = gnome_thumbnail_scale_down_pixbuf (scaled, w / 2, h / 2);

      scaled = gnome_wp_pixbuf_center (scaled, bgpixbuf);
    }
  } else {
    scaled = gdk_pixbuf_copy (bgpixbuf);
  }
  g_object_unref (pixbuf);
  g_object_unref (bgpixbuf);

  return scaled;
}

void gnome_wp_item_update_description (GnomeWPItem * item) {
  if (!strcmp (item->filename, "(none)")) {
    item->description = g_strdup_printf ("<b>%s</b>", item->name);
  } else {
    item->description = g_strdup_printf ("<b>%s</b>\n%s (%LuK)",
					 item->name,
					 gnome_vfs_mime_get_description (item->fileinfo->mime_type),
					 item->fileinfo->size / 1024);
  }
}
