/*
 *  Authors: Rodney Dawes <dobey@ximian.com>
 *
 *  Copyright 2003-2005 Novell, Inc. (www.novell.com)
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

#include <gconf/gconf-client.h>
#include <gnome.h>
#include <string.h>
#include <libgnomevfs/gnome-vfs-mime-handlers.h>

#include "gnome-wp-item.h"
#include "gnome-wp-utils.h"

GnomeWPItem * gnome_wp_item_new (const gchar * filename,
				 GHashTable * wallpapers,
				 GnomeThumbnailFactory * thumbnails) {
  GnomeWPItem * item = NULL;
  GdkColor color1, color2;
  GConfClient * client;

  client = gconf_client_get_default ();

  item = g_new0 (GnomeWPItem, 1);
  
  item->filename = gnome_vfs_unescape_string_for_display (filename);

  item->fileinfo = gnome_wp_info_new (item->filename, thumbnails);

  item->shade_type = gconf_client_get_string (client, WP_SHADING_KEY, NULL);
  item->pri_color = gconf_client_get_string (client, WP_PCOLOR_KEY, NULL);
  item->sec_color = gconf_client_get_string (client, WP_SCOLOR_KEY, NULL);

  gdk_color_parse (item->pri_color, &color1);
  gdk_color_parse (item->sec_color, &color2);
     
  item->pcolor = gdk_color_copy (&color1);
  item->scolor = gdk_color_copy (&color2);

  if (item->fileinfo != NULL &&
      !strncmp (item->fileinfo->mime_type, "image/", strlen ("image/"))) {
    if (item->name == NULL) {
      item->name = g_strdup (item->fileinfo->name);
    }
    item->options = gconf_client_get_string (client, WP_OPTIONS_KEY, NULL);

    if (!strcmp (item->options, "none")) {
      item->options = g_strdup ("scaled");
    }
    gnome_wp_item_update_description (item);
     
    g_hash_table_insert (wallpapers, g_strdup (item->filename), item);
  } else {
    gnome_wp_item_free (item);
    item = NULL;
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
  g_free (item->imguri);
  g_free (item->options);
  g_free (item->shade_type);

  g_free (item->pri_color);
  g_free (item->sec_color);

  if (item->pcolor != NULL)
    gdk_color_free (item->pcolor);

  if (item->scolor != NULL)
    gdk_color_free (item->scolor);

  gnome_wp_info_free (item->fileinfo);
  gnome_wp_info_free (item->uriinfo);

  gtk_tree_row_reference_free (item->rowref);

  item = NULL;
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

#define LIST_IMAGE_WIDTH 64

GdkPixbuf * gnome_wp_item_get_thumbnail (GnomeWPItem * item,
					 GnomeThumbnailFactory * thumbs) {
  GdkPixbuf * pixbuf, * bgpixbuf;
  GdkPixbuf * tmpbuf;
  GdkPixbuf * scaled = NULL;
  gint sw, sh, bw, bh, pw, ph, tw, th;
  gdouble ratio;

  sw = sh = bw = bh = pw = ph = tw = th = 0;

  /*
     Get the size of the screen and calculate our aspect ratio divisor
     We do this, so that images are thumbnailed as they would look on
     the screen in reality
  */
  sw = gdk_screen_get_width (gdk_screen_get_default ());
  sh = gdk_screen_get_height (gdk_screen_get_default ());
  ratio = (gdouble) sw / (gdouble) LIST_IMAGE_WIDTH;
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
    return bgpixbuf;
  } else {
    pixbuf = gnome_thumbnail_factory_generate_thumbnail (thumbs,
							 gnome_vfs_escape_path_string (item->filename),
							 item->fileinfo->mime_type);
    gnome_thumbnail_factory_save_thumbnail (thumbs, pixbuf,
					    gnome_vfs_escape_path_string (item->filename),
					    item->fileinfo->mtime);
    g_object_unref (pixbuf);
    pixbuf = gdk_pixbuf_new_from_file (item->fileinfo->thumburi, NULL);
  }

  if (pixbuf != NULL) {
    const gchar * w_val, * h_val;

    w_val = gdk_pixbuf_get_option (pixbuf, "tEXt::Thumb::Image::Width");
    h_val = gdk_pixbuf_get_option (pixbuf, "tEXt::Thumb::Image::Height");
    if (item->width <= 0 || item->height <= 0) {
      if (w_val && h_val) {
	item->width = atoi (w_val);
	item->height = atoi (h_val);
      } else {
	gchar ** keys = NULL;
	gchar ** vals = NULL;

	tmpbuf = gdk_pixbuf_new_from_file (item->filename, NULL);

	item->width = gdk_pixbuf_get_width (tmpbuf);
	item->height = gdk_pixbuf_get_height (tmpbuf);

	collect_save_options (pixbuf, &keys, &vals, item->width, item->height);
	gdk_pixbuf_savev (pixbuf, item->fileinfo->thumburi, "png",
			  keys, vals, NULL);

	g_object_unref (tmpbuf);
	g_strfreev (keys);
	g_strfreev (vals);
      }
    }

    pw = gdk_pixbuf_get_width (pixbuf);
    ph = gdk_pixbuf_get_height (pixbuf);

    if (item->width <= bw && item->height <= bh)
      ratio = 1.0;

    tw = item->width / ratio;
    th = item->height / ratio;

    if (!strcmp (item->options, "wallpaper")) {
      scaled = gnome_wp_pixbuf_tile (pixbuf, bgpixbuf, tw, th);
    } else if (!strcmp (item->options, "centered")) {
      scaled = gnome_wp_pixbuf_center (pixbuf, bgpixbuf, tw, th);
    } else if (!strcmp (item->options, "stretched")) {
      scaled = gnome_wp_pixbuf_center (pixbuf, bgpixbuf, bw, bh);
    } else if (!strcmp (item->options, "scaled")) {
      if ((gdouble) ph * (gdouble) bw > (gdouble) pw * (gdouble) bh) {
	tw = 0.5 + (gdouble) pw * (gdouble) bh / (gdouble) ph;
	th = bh;
      } else {
	th = 0.5 + (gdouble) ph * (gdouble) bw / (gdouble) pw;
	tw = bw;
      }
      scaled = gnome_wp_pixbuf_center (pixbuf, bgpixbuf, tw, th);
    }
  }

  g_object_unref (pixbuf);
  g_object_unref (bgpixbuf);

  return scaled;
}

void gnome_wp_item_update_description (GnomeWPItem * item) {

  if (!strcmp (item->filename, "(none)")) {
    item->description = g_strdup_printf ("<b>%s</b>", item->name);
  } else {
    gchar * info;

    info = g_strdup_printf ("%s, %d %s x %d %s",
			    gnome_vfs_mime_get_description (item->fileinfo->mime_type),
			    item->width,
			    ngettext ("pixel", "pixels", item->width),
			    item->height,
			    ngettext ("pixel", "pixels", item->height));

    item->description = g_markup_printf_escaped ("<b>%s</b>\n"
						 "%s",
						 item->name,
						 info);

    g_free (info);
  }
}
