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
#include <libgnomevfs/gnome-vfs-mime-handlers.h>

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
  gint rw, rh, sw, sh, bw, bh, pw, ph, th, tw;
  gdouble ratio;

  /*
     Get the size of the screen and calculate our aspect ratio divisor
     We do this, so that images are thumbnailed as they would look on
     the screen in reality
  */
  sw = gdk_screen_get_width (gdk_screen_get_default ());
  sh = gdk_screen_get_height (gdk_screen_get_default ());
  ratio = (gdouble) sw / (gdouble) 64;
  bw = sw / ratio;
  bh = sh / ratio;

  /*
     Create the pixbuf for the background colors, which will show up for
     oddly sized images, smaller images that are centered, or alpha images
  */
  if (!strcmp (item->shade_type, "solid")) {
    bgpixbuf = gnome_wp_pixbuf_new_solid (item->pcolor, bw, bh);
  } else if (!strcmp (item->shade_type, "vertical-gradient")) {
    bgpixbuf = gnome_wp_pixbuf_new_gradient (GTK_ORIENTATION_VERTICAL,
					     item->pcolor, item->scolor,
					     bw, bh);
  } else {
    bgpixbuf = gnome_wp_pixbuf_new_gradient (GTK_ORIENTATION_HORIZONTAL,
					     item->pcolor, item->scolor,
					     bw, bh);
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

  if (pixbuf != NULL && strcmp (item->filename, "(none)") != 0) {
    const gchar * w_val, * h_val;

    w_val = gdk_pixbuf_get_option (pixbuf, "tEXt::Thumb::Image::Width");
    h_val = gdk_pixbuf_get_option (pixbuf, "tEXt::Thumb::Image::Height");
    if (item->width <= 0 || item->height <= 0) {
      if (w_val && h_val) {
	item->width = rw = atoi (w_val);
	item->height = rh = atoi (h_val);
      } else {
	GdkPixbuf * tmpbuf = gdk_pixbuf_new_from_file (item->filename, NULL);
	
	item->width = rw = gdk_pixbuf_get_width (tmpbuf);
	item->height = rh = gdk_pixbuf_get_height (tmpbuf);
	
	g_object_unref (tmpbuf);
      }
    } else {
	rw = item->width;
	rh = item->height;
    }

    pw = gdk_pixbuf_get_width (pixbuf);
    ph = gdk_pixbuf_get_height (pixbuf);

    if (rw >= sw && rh >= sh) {
      ratio = (gdouble) rw / (gdouble) sw;
      tw = ratio * bw;
      th = ratio * bh;
    } else {
      if (pw > ph) {
	ratio = (gdouble) pw / (gdouble) (bw - 16);
      } else {
	ratio = (gdouble) ph / (gdouble) (bh - 16);
      }
      tw = pw / ratio;
      th = ph / ratio;
    }
    scaled = gnome_thumbnail_scale_down_pixbuf (pixbuf, tw, th);

    if (!strcmp (item->options, "wallpaper")) {
      pixbuf = gnome_wp_pixbuf_tile (scaled, bgpixbuf);
    } else if (!strcmp (item->options, "centered")) {
      pixbuf = gnome_wp_pixbuf_center (scaled, bgpixbuf);
    } else if (!strcmp (item->options, "stretched")) {
      scaled = gnome_thumbnail_scale_down_pixbuf (pixbuf, bw, bh);
      pixbuf = gnome_wp_pixbuf_center (scaled, bgpixbuf);
    } else if (!strcmp (item->options, "scaled")) {
      if (ph > bh && pw > bw) {
	if ((gdouble) ph * (gdouble) bw > (gdouble) pw * (gdouble) bh) {
	  tw = 0.5 + (gdouble) pw * (gdouble) bh / (gdouble) ph;
	  th = bh;
	} else {
	  th = 0.5 + (gdouble) ph * (gdouble) bw / (gdouble) pw;
	  tw = bw;
	}
	scaled = gnome_thumbnail_scale_down_pixbuf (pixbuf, tw, th);
      }
      pixbuf = gnome_wp_pixbuf_center (scaled, bgpixbuf);
    }
  }
  scaled = gdk_pixbuf_copy (pixbuf);

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
