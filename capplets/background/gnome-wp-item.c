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

  w = gdk_screen_get_width (gdk_screen_get_default ());
  h = gdk_screen_get_height (gdk_screen_get_default ());
  ratio = h / 96;

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

  if (item->fileinfo->thumburi != NULL &&
      g_file_test (item->fileinfo->thumburi, G_FILE_TEST_EXISTS)) {
    pixbuf = gdk_pixbuf_new_from_file (item->fileinfo->thumburi, NULL);
  } else if (!strcmp (item->filename, "(none)")) {
    pixbuf = gdk_pixbuf_copy (bgpixbuf);
  } else {
    pixbuf = gnome_thumbnail_factory_generate_thumbnail (thumbs,
							 item->filename,
							 item->fileinfo->mime_type);
    gnome_thumbnail_factory_save_thumbnail (thumbs, pixbuf,
					    item->filename,
					    item->fileinfo->mtime);
  }

  if (pixbuf != NULL) {
    w = gdk_pixbuf_get_width (pixbuf);
    h = gdk_pixbuf_get_height (pixbuf);
    ratio = h / 48;

    if (ratio == 1)
      ratio = 2;

    scaled = gnome_thumbnail_scale_down_pixbuf (pixbuf,
						w / ratio, h / ratio);

    if (w == h) {
      item->options = g_strdup ("wallpaper");
      scaled = gnome_wp_pixbuf_tile (scaled, bgpixbuf);

      gnome_thumbnail_factory_save_thumbnail (thumbs, scaled,
					      item->filename,
					      item->fileinfo->mtime);

      w = gdk_pixbuf_get_width (scaled);
      h = gdk_pixbuf_get_height (scaled);
      ratio = h / 48;
      
      if (ratio == 1)
	ratio = 2;

      scaled = gnome_thumbnail_scale_down_pixbuf (scaled,
						  w / ratio, h / ratio);
    }

    if (!strcmp (item->options, "centered")) {
      scaled = gnome_wp_pixbuf_center (scaled, bgpixbuf);

      w = gdk_pixbuf_get_width (scaled);
      h = gdk_pixbuf_get_height (scaled);
      ratio = h / 48;
      
      if (ratio == 1)
	ratio = 2;

      scaled = gnome_thumbnail_scale_down_pixbuf (scaled,
						  w / ratio, h / ratio);
   }
  }
  g_object_unref (pixbuf);
  g_object_unref (bgpixbuf);

  return scaled;
}
