/* -*- mode: C; c-basic-offset: 4 -*-
 * themus - utilities for GNOME themes
 * Copyright (C) 2003  Andrew Sobala <aes@gnome.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include <gdk/gdk.h>
#include <gnome-theme-info.h>
#include <theme-thumbnail.h>

#define PAD_PIXELS 4

/* This function courtesy of James Henstridge and fontilus */
static void
save_pixbuf(GdkPixbuf *pixbuf, gchar *filename)
{
    guchar *buffer;
    gint p_width, p_height, p_rowstride;
    gint i, j;
    gint trim_left, trim_right, trim_top, trim_bottom;
    GdkPixbuf *subpixbuf;

    buffer      = gdk_pixbuf_get_pixels(pixbuf);
    p_width     = gdk_pixbuf_get_width(pixbuf);
    p_height    = gdk_pixbuf_get_height(pixbuf);
    p_rowstride = gdk_pixbuf_get_rowstride(pixbuf);

    for (i = 0; i < p_width; i++) {
	gboolean seen_pixel = FALSE;

	for (j = 0; j < p_height; j++) {
	    gint offset = j * p_rowstride + 3*i;

	    seen_pixel = (buffer[offset]   != 0xff ||
			  buffer[offset+1] != 0xff ||
			  buffer[offset+2] != 0xff);
	    if (seen_pixel)
		break;
	}
	if (seen_pixel)
	    break;
    }
    trim_left = MIN(p_width, i);
    trim_left = MAX(trim_left - PAD_PIXELS, 0);

    for (i = p_width-1; i >= trim_left; i--) {
	gboolean seen_pixel = FALSE;

	for (j = 0; j < p_height; j++) {
	    gint offset = j * p_rowstride + 3*i;

	    seen_pixel = (buffer[offset]   != 0xff ||
			  buffer[offset+1] != 0xff ||
			  buffer[offset+2] != 0xff);
	    if (seen_pixel)
		break;
	}
	if (seen_pixel)
	    break;
    }
    trim_right = MAX(trim_left, i);
    trim_right = MIN(trim_right + PAD_PIXELS, p_width-1);

    for (j = 0; j < p_height; j++) {
	gboolean seen_pixel = FALSE;

	for (i = 0; i < p_width; i++) {
	    gint offset = j * p_rowstride + 3*i;

	    seen_pixel = (buffer[offset]   != 0xff ||
			  buffer[offset+1] != 0xff ||
			  buffer[offset+2] != 0xff);
	    if (seen_pixel)
		break;
	}
	if (seen_pixel)
	    break;
    }
    trim_top = MIN(p_height, j);
    trim_top = MAX(trim_top - PAD_PIXELS, 0);

    for (j = p_height-1; j >= trim_top; j--) {
	gboolean seen_pixel = FALSE;

	for (i = 0; i < p_width; i++) {
	    gint offset = j * p_rowstride + 3*i;

	    seen_pixel = (buffer[offset]   != 0xff ||
			  buffer[offset+1] != 0xff ||
			  buffer[offset+2] != 0xff);
	    if (seen_pixel)
		break;
	}
	if (seen_pixel)
	    break;
    }
    trim_bottom = MAX(trim_top, j);
    trim_bottom = MIN(trim_bottom + PAD_PIXELS, p_height-1);

    subpixbuf = gdk_pixbuf_new_subpixbuf(pixbuf, trim_left, trim_top,
					 trim_right - trim_left,
					 trim_bottom - trim_top);
    gdk_pixbuf_save(subpixbuf, filename, "png", NULL, NULL);
    gdk_pixbuf_unref(subpixbuf);
}

int
main(int argc, char **argv)
{
	GdkPixbuf *pixbuf;
	GnomeThemeMetaInfo *theme;
	GnomeVFSURI *uri;

	theme_thumbnail_factory_init (argc, argv);

	if (argc != 3) {
		g_printerr("usage: gnome-theme-thumbnailer theme output-image\n");
		return 1;
	}

	if (!gnome_vfs_init()) {
		g_printerr("could not initialise gnome-vfs\n");
		return 1;
	}
	
	gnome_theme_init (FALSE);
	uri = gnome_vfs_uri_new (argv[1]);
	theme = gnome_theme_read_meta_theme (uri);
	gnome_vfs_uri_unref (uri);

	pixbuf = generate_theme_thumbnail (theme, TRUE);

	save_pixbuf(pixbuf, argv[2]);
	gdk_pixbuf_unref(pixbuf);

	return 0;
}
