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

#include "gnome-wp-utils.h"
#include <string.h>

GdkPixbuf * gnome_wp_pixbuf_new_gradient (GtkOrientation orientation,
					  GdkColor * c1,
					  GdkColor * c2,
					  gint width, gint height) {
  GdkPixbuf * pixbuf;
  gint i, j;
  gint dr, dg, db;
  gint gs1;
  gint vc = ((orientation == GTK_ORIENTATION_HORIZONTAL) || (c1 == c2));
  guchar * b, * row, * d;
  int rowstride;

  pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB, FALSE, 8, width, height);
  d = gdk_pixbuf_get_pixels (pixbuf);
  rowstride = gdk_pixbuf_get_rowstride (pixbuf);
  
  dr = c2->red - c1->red;
  dg = c2->green - c1->green;
  db = c2->blue - c1->blue;

  gs1 = (orientation == GTK_ORIENTATION_VERTICAL) ? height - 1 : width - 1;

  row = g_new (unsigned char, rowstride);

  if (vc) {
    b = row;
    for (j = 0; j < width; j++) {
      *b++ = (c1->red + (j * dr) / gs1) >> 8;
      *b++ = (c1->green + (j * dg) / gs1) >> 8;
      *b++ = (c1->blue + (j * db) / gs1) >> 8;
    }
  }

  for (i = 0; i < height; i++) {
    if (!vc) {
      unsigned char cr, cg, cb;
      cr = (c1->red + (i * dr) / gs1) >> 8;
      cg = (c1->green + (i * dg) / gs1) >> 8;
      cb = (c1->blue + (i * db) / gs1) >> 8;
      b = row;
      for (j = 0; j < width; j++) {
	*b++ = cr;
	*b++ = cg;
	*b++ = cb;
      }
    }
    memcpy (d, row, width * 3);
    d += rowstride;
  }
  g_free (row);

  return pixbuf;
}

GdkPixbuf * gnome_wp_pixbuf_new_solid (GdkColor * color,
				       gint width, gint height) {
  GdkPixbuf * pixbuf;
  gint j, rowstride;
  guchar * b, * d, * row;

  pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB, FALSE, 8, width, height);
  d = gdk_pixbuf_get_pixels (pixbuf);
  rowstride = gdk_pixbuf_get_rowstride (pixbuf);

  row = g_new (unsigned char, rowstride);

  b = row;
  for (j = 0; j < width; j++) {
    *b++ = color->red >> 8;
    *b++ = color->green >> 8;
    *b++ = color->blue >> 8;
  }

  for (j = 0; j < height; j++) {
    memcpy (d, row, width * 3);
    d += rowstride;
  }

  g_free (row);

  return pixbuf;
}

GdkPixbuf * gnome_wp_pixbuf_tile (GdkPixbuf * src_pixbuf,
				  GdkPixbuf * dest_pixbuf,
				  gint scaled_width,
				  gint scaled_height) {
  GdkPixbuf * tmpbuf;
  gdouble cx, cy;
  gint dwidth, dheight;
  gint swidth, sheight;
  guint alpha = 255;

  if (dest_pixbuf == NULL) {
    return gdk_pixbuf_copy (src_pixbuf);
  }

  tmpbuf = gdk_pixbuf_scale_simple (src_pixbuf, scaled_width, scaled_height,
				    GDK_INTERP_BILINEAR);

  swidth = gdk_pixbuf_get_width (tmpbuf);
  sheight = gdk_pixbuf_get_height (tmpbuf);

  dwidth = gdk_pixbuf_get_width (dest_pixbuf);
  dheight = gdk_pixbuf_get_height (dest_pixbuf);

  for (cy = 0; cy < dheight; cy += sheight) {
    for (cx = 0; cx < dwidth; cx += swidth) {
      gdk_pixbuf_composite (tmpbuf, dest_pixbuf, cx, cy,
			    MIN (swidth, dwidth - cx),
			    MIN (sheight, dheight - cy),
			    cx, cy, 1.0, 1.0,
			    GDK_INTERP_BILINEAR, alpha);
    }
  }
  g_object_unref (tmpbuf);

  return gdk_pixbuf_copy (dest_pixbuf);
}

GdkPixbuf * gnome_wp_pixbuf_center (GdkPixbuf * src_pixbuf,
				    GdkPixbuf * dest_pixbuf,
				    gint scaled_width,
				    gint scaled_height) {
  GdkPixbuf * tmpbuf;
  gint ox, oy, cx, cy;
  gint dwidth, dheight;
  gint swidth, sheight;
  gint cwidth, cheight;
  guint alpha = 255;

  if (dest_pixbuf == NULL) {
    return gdk_pixbuf_copy (src_pixbuf);
  }

  ox = cx = oy = cy = 0;

  tmpbuf = gdk_pixbuf_scale_simple (src_pixbuf, scaled_width, scaled_height,
				    GDK_INTERP_BILINEAR);

  swidth = gdk_pixbuf_get_width (tmpbuf);
  sheight = gdk_pixbuf_get_height (tmpbuf);

  dwidth = gdk_pixbuf_get_width (dest_pixbuf);
  dheight = gdk_pixbuf_get_height (dest_pixbuf);

  if (dwidth > swidth) {
    ox = (dwidth - swidth) / 2;
    cx = 0;
    cwidth = swidth;
  } else {
    cx = (swidth - dwidth) / 2;
    oy = 0;
    cwidth = dwidth;
  }

  if (dheight > sheight) {
    oy = ((dheight - sheight) / 2);
    cy = 0;
    cheight = sheight;
  } else {
    cy = (sheight - dheight) / 2;
    oy = 0;
    cheight = dheight;
  }

  gdk_pixbuf_composite (tmpbuf, dest_pixbuf, ox, oy,
			cwidth, cheight,
			ox - cx, oy - cy, 1.0, 1.0,
			GDK_INTERP_BILINEAR, alpha);

  g_object_unref (tmpbuf);
  return gdk_pixbuf_copy (dest_pixbuf);
}

