/* -*- mode: C; c-basic-offset: 4 -*- */

#include <ft2build.h>
#include FT_FREETYPE_H

#include <libgnomevfs/gnome-vfs.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

FT_Error FT_New_URI_Face(FT_Library library,
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
	    pixel = 255 - bitmap->buffer[j*bitmap->pitch + i];
	    pos = (j + off_y) * p_rowstride + 3 * (i + off_x);
	    buffer[pos]   = pixel;
	    buffer[pos+1] = pixel;
	    buffer[pos+2] = pixel;
	}
    }
}

int
main(int argc, char **argv)
{
    FT_Error error;
    FT_Library library;
    FT_Face face;
    FT_GlyphSlot slot;
    GdkPixbuf *pixbuf, *pixbuf2;
    guchar *buffer;
    gint width, height, i, pen_x, pen_y, max_width, max_height;

    if (argc != 3) {
	g_message("eek: bad args");
	return 1;
    }

    if (!gnome_vfs_init()) {
	g_message("eek: gnome-vfs init");
	return 1;
    }

    error = FT_Init_FreeType(&library);
    if (error) {
	g_message("eek: library");
	return 1;
    }

    error = FT_New_URI_Face(library, argv[1], 0, &face);
    if (error) {
	g_message("eek: face");
	return 1;
    }

    slot = face->glyph;

    error = FT_Set_Pixel_Sizes(face, 0, 64);
    if (error) {
	g_message("eek: set pixel size");
	return 1;
    }
    error = FT_Load_Char(face, 'A', FT_LOAD_RENDER);
    if (error) {
	g_message("eek: load char");
	return 1;
    }

    g_assert(slot->bitmap.pixel_mode == ft_pixel_mode_grays);

    width   = slot->bitmap.width;
    height  = slot->bitmap.rows;

    pixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, FALSE, 8, width*3, height*1.5);
    buffer = gdk_pixbuf_get_pixels(pixbuf);

    for (i = gdk_pixbuf_get_rowstride(pixbuf) *
	     gdk_pixbuf_get_height(pixbuf); i >= 0; i--)
	buffer[i] = 255;

    pen_x = 0;
    pen_y = slot->bitmap_top;
    max_height = height;
    max_width = pen_x + slot->bitmap_left + width;

    draw_bitmap(pixbuf, &slot->bitmap,
		pen_x + slot->bitmap_left, pen_y - slot->bitmap_top);

    pen_x += slot->advance.x >> 6;

    error = FT_Load_Char(face, 'a', FT_LOAD_RENDER);
    if (error) {
	g_message("ekk: load char 2");
	return 1;
    }

    max_height = MAX(max_height, pen_y + slot->bitmap.rows -
		     slot->bitmap_top);
    max_width = pen_x + slot->bitmap_left + slot->bitmap.width;

    draw_bitmap(pixbuf, &slot->bitmap,
		pen_x + slot->bitmap_left, pen_y - slot->bitmap_top);

    pixbuf2 = gdk_pixbuf_new_subpixbuf(pixbuf, 0,0, max_width,max_height);

    gdk_pixbuf_save(pixbuf2, argv[2], "png", NULL, NULL);
    gdk_pixbuf_unref(pixbuf2);
    gdk_pixbuf_unref(pixbuf);

    /* freeing the face causes a crash I haven't tracked down yet */
#if 0
    error = FT_Done_Face(face);
    if (error) {
	g_message("eek: done face");
	return 1;
    }
    error = FT_Done_FreeType(library);
    if (error) {
	g_message("eek: done library");
	return 1;
    }
#endif

    return 0;
}
