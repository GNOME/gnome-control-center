/* -*- mode: C; c-basic-offset: 4 -*-
 * fontilus - a collection of font utilities for GNOME
 * Copyright (C) 2002-2003  James Henstridge <james@daa.com.au>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <stdio.h>

#include <ft2build.h>
#include FT_FREETYPE_H

#include <libgnomevfs/gnome-vfs.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

const gchar *
get_ft_error(FT_Error error)
{
#undef __FTERRORS_H__
#define FT_ERRORDEF(e,v,s) case e: return s;
#define FT_ERROR_START_LIST
#define FT_ERROR_END_LIST
    switch (error) {
#include FT_ERRORS_H
    default:
	return "unknown";
    }
}

#define FONT_SIZE 64
#define PAD_PIXELS 4

FT_Error FT_New_Face_From_URI(FT_Library library,
			      const gchar *uri,
			      FT_Long face_index,
			      FT_Face *aface);

static void
draw_bitmap(GdkPixbuf *pixbuf, FT_Bitmap *bitmap, gint off_x, gint off_y)
{
    guchar *buffer;
    gint p_width, p_height, p_rowstride;
    gint i, j;

    buffer      = gdk_pixbuf_get_pixels(pixbuf);
    p_width     = gdk_pixbuf_get_width(pixbuf);
    p_height    = gdk_pixbuf_get_height(pixbuf);
    p_rowstride = gdk_pixbuf_get_rowstride(pixbuf);

    for (j = 0; j < bitmap->rows; j++) {
	if (j + off_y < 0 || j + off_y >= p_height)
	    continue;
	for (i = 0; i < bitmap->width; i++) {
	    guchar pixel;
	    gint pos;

	    if (i + off_x < 0 || i + off_x >= p_width)
		continue;
	    switch (bitmap->pixel_mode) {
	    case ft_pixel_mode_mono:
		pixel = bitmap->buffer[j * bitmap->pitch + i/8];
		pixel = 255 - ((pixel >> (7 - i % 8)) & 0x1) * 255;
		break;
	    case ft_pixel_mode_grays:
		pixel = 255 - bitmap->buffer[j*bitmap->pitch + i];
		break;
	    default:
		pixel = 255;
	    }
	    pos = (j + off_y) * p_rowstride + 3 * (i + off_x);
	    buffer[pos]   = pixel;
	    buffer[pos+1] = pixel;
	    buffer[pos+2] = pixel;
	}
    }
}

static void
draw_char(GdkPixbuf *pixbuf, FT_Face face, FT_UInt glyph_index,
	  gint *pen_x, gint *pen_y)
{
    FT_Error error;
    FT_GlyphSlot slot;

    slot = face->glyph;

    error = FT_Load_Glyph(face, glyph_index, FT_LOAD_DEFAULT);
    if (error) {
	g_printerr("could not load glyph index '%ud': %s\n", glyph_index,
		   get_ft_error(error));
	return;
    }

    error = FT_Render_Glyph(slot, ft_render_mode_normal);
    if (error) {
	g_printerr("could not render glyph index '%ud': %s\n", glyph_index,
		   get_ft_error(error));
	return;
    }

    draw_bitmap(pixbuf, &slot->bitmap,
		*pen_x + slot->bitmap_left,
		*pen_y - slot->bitmap_top);

    *pen_x += slot->advance.x >> 6;
}

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
    FT_Error error;
    FT_Library library;
    FT_Face face;
    FT_UInt glyph_index1, glyph_index2;
    FT_ULong charcode;
    gchar *uri;
    GdkPixbuf *pixbuf;
    guchar *buffer;
    gint i, len, pen_x, pen_y;

    if (argc != 3) {
	g_printerr("usage: thumbnailer fontfile output-image\n");
	return 1;
    }

    if (!gnome_vfs_init()) {
	g_printerr("could not initialise gnome-vfs\n");
	return 1;
    }

    error = FT_Init_FreeType(&library);
    if (error) {
	g_printerr("could not initialise freetype: %s\n", get_ft_error(error));
	return 1;
    }

    uri = gnome_vfs_make_uri_from_shell_arg (argv[1]);
    error = FT_New_Face_From_URI(library, uri, 0, &face);
    if (error) {
	g_printerr("could not load face '%s': %s\n", uri,
		   get_ft_error(error));
	return 1;
    }
    g_free (uri);

    error = FT_Set_Pixel_Sizes(face, 0, FONT_SIZE);
    if (error) {
	g_printerr("could not set pixel size: %s\n", get_ft_error(error));
	/* return 1; */
    }

    for (i = 0; i < face->num_charmaps; i++) {
	if (face->charmaps[i]->encoding == ft_encoding_latin_1 ||
	    face->charmaps[i]->encoding == ft_encoding_unicode ||
	    face->charmaps[i]->encoding == ft_encoding_apple_roman) {
	    error = FT_Set_Charmap(face, face->charmaps[i]);
	    if (error) {
		g_printerr("could not set charmap: %s\n", get_ft_error(error));
		/* return 1; */
	    }
	    break;
	}
    }

    pixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, FALSE, 8,
			    FONT_SIZE*3, FONT_SIZE*1.5);
    if (!pixbuf) {
	g_printerr("could not create pixbuf\n");
	return 1;
    }
    buffer = gdk_pixbuf_get_pixels(pixbuf);
    len = gdk_pixbuf_get_rowstride(pixbuf) * gdk_pixbuf_get_height(pixbuf);
    for (i = 0; i < len; i++)
	buffer[i] = 255;

    pen_x = FONT_SIZE/2;
    pen_y = FONT_SIZE;

    glyph_index1 = FT_Get_Char_Index (face, 'A');
    glyph_index2 = FT_Get_Char_Index (face, 'a');

    /* if the glyphs for those letters don't exist, pick some other
     * glyphs. */
    if (glyph_index1 == 0) glyph_index1 = MIN (65, face->num_glyphs-1);
    if (glyph_index2 == 0) glyph_index2 = MIN (97, face->num_glyphs-1);

    draw_char(pixbuf, face, glyph_index1, &pen_x, &pen_y);
    draw_char(pixbuf, face, glyph_index2, &pen_x, &pen_y);

    save_pixbuf(pixbuf, argv[2]);
    gdk_pixbuf_unref(pixbuf);

    /* freeing the face causes a crash I haven't tracked down yet */
    error = FT_Done_Face(face);
    if (error) {
	g_printerr("could not unload face: %s\n", get_ft_error(error));
	return 1;
    }
    error = FT_Done_FreeType(library);
    if (error) {
	g_printerr("could not finalise freetype library: %s\n",
		   get_ft_error(error));
	return 1;
    }

    return 0;
}
