/* vim: set sw=8: -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * gnomecc-rounded-rect.c: A rectangle with rounded corners
 *
 * Copyright (C) 2004 Novell Inc
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */

#include <config.h>
#include "gnomecc-rounded-rect.h"
#include "gnomecc-rounded-rect-pixbuf.h"
#include <glib-object.h>
#include <math.h>

struct _GnomeccRoundedRect {
	GnomeCanvasRect base;
};
typedef GnomeCanvasRectClass GnomeccRoundedRectClass;

#define GNOMECC_ROUNDED_RECT_CLASS(k)	 (G_TYPE_CHECK_CLASS_CAST ((k), GNOMECC_TYPE_ROUNDED_RECT, GnomeccRoundedRectClass))
#define GNOMECC_IS_ROUNDED_RECT_CLASS(k) (G_TYPE_CHECK_CLASS_TYPE ((k), GNOMECC_TYPE_ROUNDED_RECT))

G_DEFINE_TYPE (GnomeccRoundedRect, gnomecc_rounded_rect, GNOME_TYPE_CANVAS_RECT);

/*************************************************************************
 * Adapted from nautilus/libnautilus-private/nautilus-icon-canvas-item.c
 */
/* clear the corners of the selection pixbuf by copying the corners of the passed-in pixbuf */
static void
gnomecc_rounded_rect_draw (GnomeCanvasItem *item, GdkDrawable *drawable,
			   int x, int y, int width, int height)
{
	static GdkPixbuf *corner_pixbuf = NULL;
	GnomeCanvasRE const *re = GNOME_CANVAS_RE (item);
	int dest_width, dest_height, src_width, src_height;
	int dx, dy;
	int corner_size = 5;
	double affine[6];

	GNOME_CANVAS_ITEM_CLASS (gnomecc_rounded_rect_parent_class)->draw (item, drawable, x, y, width, height);

	if (corner_pixbuf == NULL)
		corner_pixbuf = gdk_pixbuf_new_from_inline (-1,
			gnomecc_rounded_rect_frame, FALSE, NULL);
	src_width = gdk_pixbuf_get_width (corner_pixbuf);
	src_height = gdk_pixbuf_get_height (corner_pixbuf);

	gnome_canvas_item_i2c_affine (item, affine);

	dest_width = fabs (re->x2 - re->x1);
	dest_height = fabs (re->y2 - re->y1);
	dx = affine[4] - x;
	dy = affine[5] - y;
	
	/* draw top left corner */
	gdk_draw_pixbuf (drawable, NULL, corner_pixbuf,
			 0, 0,
			 dx, dy,
			 corner_size, corner_size,
			 GDK_RGB_DITHER_NORMAL, 0, 0);
	/* draw top right corner */
	gdk_draw_pixbuf (drawable, NULL, corner_pixbuf,
			 src_width - corner_size, 0,
			 dx + dest_width - corner_size, dy,
			 corner_size, corner_size,
			 GDK_RGB_DITHER_NORMAL, 0, 0);
	/* draw bottom left corner */
	gdk_draw_pixbuf (drawable, NULL, corner_pixbuf,
			 0, src_height - corner_size,
			 dx, dy + dest_height - corner_size,
			 corner_size, corner_size,
			 GDK_RGB_DITHER_NORMAL, 0, 0);
	/* draw bottom right corner */
	gdk_draw_pixbuf (drawable, NULL, corner_pixbuf,
			 src_width - corner_size, src_height - corner_size,
			 dx + dest_width - corner_size, dy + dest_height - corner_size,
			 corner_size, corner_size,
			 GDK_RGB_DITHER_NORMAL, 0, 0);
}

static void
gnomecc_rounded_rect_class_init (GnomeccRoundedRectClass *klass)
{
	GnomeCanvasItemClass	*item_klass = (GnomeCanvasItemClass *) klass;
	item_klass->draw	= gnomecc_rounded_rect_draw;
}
static void
gnomecc_rounded_rect_init (GnomeccRoundedRect *rr)
{
}
