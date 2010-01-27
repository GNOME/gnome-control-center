/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2002 Jonathan Blandford <jrb@gnome.org>
 * Copyright (C) 2007 Jens Granseuer <jensgr@gmx.net>
 * Copyright (C) 2010 Red Hat, Inc.
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 */

#include "config.h"

#include <stdlib.h>
#include <stdio.h>

#include <gtk/gtk.h>
#include <gio/gio.h>
#include <glib/gi18n-lib.h>
#include <gconf/gconf-client.h>

#ifdef HAVE_XFT2
#include <gdk/gdkx.h>
#include <X11/Xft/Xft.h>
#endif /* HAVE_XFT2 */

#include "cc-font-common.h"

#ifdef HAVE_XFT2

/*
 * Code for displaying previews of font rendering with various Xft options
 */

static void
sample_size_request (GtkWidget      *darea,
                     GtkRequisition *requisition)
{
        GdkPixbuf *pixbuf = g_object_get_data (G_OBJECT (darea), "sample-pixbuf");

        requisition->width = gdk_pixbuf_get_width (pixbuf) + 2;
        requisition->height = gdk_pixbuf_get_height (pixbuf) + 2;
}

static void
sample_expose (GtkWidget      *darea,
               GdkEventExpose *expose)
{
        GdkPixbuf *pixbuf = g_object_get_data (G_OBJECT (darea), "sample-pixbuf");
        int        width = gdk_pixbuf_get_width (pixbuf);
        int        height = gdk_pixbuf_get_height (pixbuf);

        int        x = (darea->allocation.width - width) / 2;
        int        y = (darea->allocation.height - height) / 2;

        gdk_draw_rectangle (darea->window, darea->style->white_gc, TRUE,
                            0, 0,
                            darea->allocation.width, darea->allocation.height);
        gdk_draw_rectangle (darea->window, darea->style->black_gc, FALSE,
                            0, 0,
                            darea->allocation.width - 1, darea->allocation.height - 1);

        gdk_draw_pixbuf (darea->window, NULL, pixbuf, 0, 0, x, y, width, height,
                         GDK_RGB_DITHER_NORMAL, 0, 0);
}

static XftFont *
open_pattern (FcPattern   *pattern,
              Antialiasing antialiasing,
              Hinting      hinting)
{
#ifdef FC_HINT_STYLE
        static const int hintstyles[] = {
                FC_HINT_NONE, FC_HINT_SLIGHT, FC_HINT_MEDIUM, FC_HINT_FULL
        };
#endif /* FC_HINT_STYLE */

        FcPattern *res_pattern;
        FcResult   result;
        XftFont   *font;

        Display   *xdisplay = gdk_x11_get_default_xdisplay ();
        int        screen = gdk_x11_get_default_screen ();

        res_pattern = XftFontMatch (xdisplay, screen, pattern, &result);
        if (res_pattern == NULL)
                return NULL;

        FcPatternDel (res_pattern, FC_HINTING);
        FcPatternAddBool (res_pattern, FC_HINTING, hinting != HINT_NONE);

#ifdef FC_HINT_STYLE
        FcPatternDel (res_pattern, FC_HINT_STYLE);
        FcPatternAddInteger (res_pattern, FC_HINT_STYLE, hintstyles[hinting]);
#endif /* FC_HINT_STYLE */

        FcPatternDel (res_pattern, FC_ANTIALIAS);
        FcPatternAddBool (res_pattern, FC_ANTIALIAS, antialiasing != ANTIALIAS_NONE);

        FcPatternDel (res_pattern, FC_RGBA);
        FcPatternAddInteger (res_pattern, FC_RGBA,
                             antialiasing == ANTIALIAS_RGBA ? FC_RGBA_RGB : FC_RGBA_NONE);

        FcPatternDel (res_pattern, FC_DPI);
        FcPatternAddInteger (res_pattern, FC_DPI, 96);

        font = XftFontOpenPattern (xdisplay, res_pattern);
        if (!font)
                FcPatternDestroy (res_pattern);

        return font;
}

void
setup_font_sample (GtkWidget   *darea,
                   Antialiasing antialiasing,
                   Hinting      hinting)
{
        const char  *string1 = "abcfgop AO ";
        const char  *string2 = "abcfgop";

        XftColor     black, white;
        XRenderColor rendcolor;

        Display     *xdisplay = gdk_x11_get_default_xdisplay ();

        GdkColormap *colormap = gdk_rgb_get_colormap ();
        Colormap     xcolormap = GDK_COLORMAP_XCOLORMAP (colormap);

        GdkVisual   *visual = gdk_colormap_get_visual (colormap);
        Visual      *xvisual = GDK_VISUAL_XVISUAL (visual);

        FcPattern   *pattern;
        XftFont     *font1, *font2;
        XGlyphInfo   extents1 = { 0 };
        XGlyphInfo   extents2 = { 0 };
        GdkPixmap   *pixmap;
        XftDraw     *draw;
        GdkPixbuf   *tmp_pixbuf, *pixbuf;

        int          width, height;
        int          ascent, descent;

        pattern = FcPatternBuild (NULL,
                                  FC_FAMILY, FcTypeString, "Serif",
                                  FC_SLANT, FcTypeInteger, FC_SLANT_ROMAN,
                                  FC_SIZE, FcTypeDouble, 18.,
                                  NULL);
        font1 = open_pattern (pattern, antialiasing, hinting);
        FcPatternDestroy (pattern);

        pattern = FcPatternBuild (NULL,
                                  FC_FAMILY, FcTypeString, "Serif",
                                  FC_SLANT, FcTypeInteger, FC_SLANT_ITALIC,
                                  FC_SIZE, FcTypeDouble, 20.,
                                  NULL);
        font2 = open_pattern (pattern, antialiasing, hinting);
        FcPatternDestroy (pattern);

        ascent = 0;
        descent = 0;
        if (font1) {
                XftTextExtentsUtf8 (xdisplay, font1, (unsigned char *) string1,
                                    strlen (string1), &extents1);
                ascent = MAX (ascent, font1->ascent);
                descent = MAX (descent, font1->descent);
        }

        if (font2) {
                XftTextExtentsUtf8 (xdisplay, font2, (unsigned char *) string2,
                                    strlen (string2), &extents2);
                ascent = MAX (ascent, font2->ascent);
                descent = MAX (descent, font2->descent);
        }

        width = extents1.xOff + extents2.xOff + 4;
        height = ascent + descent + 2;

        pixmap = gdk_pixmap_new (NULL, width, height, visual->depth);

        draw = XftDrawCreate (xdisplay, GDK_DRAWABLE_XID (pixmap), xvisual, xcolormap);

        rendcolor.red = 0;
        rendcolor.green = 0;
        rendcolor.blue = 0;
        rendcolor.alpha = 0xffff;
        XftColorAllocValue (xdisplay, xvisual, xcolormap, &rendcolor, &black);

        rendcolor.red = 0xffff;
        rendcolor.green = 0xffff;
        rendcolor.blue = 0xffff;
        rendcolor.alpha = 0xffff;
        XftColorAllocValue (xdisplay, xvisual, xcolormap, &rendcolor, &white);
        XftDrawRect (draw, &white, 0, 0, width, height);
        if (font1)
                XftDrawStringUtf8 (draw,
                                   &black,
                                   font1,
                                   2, 2 + ascent,
                                   (unsigned char *) string1,
                                   strlen (string1));
        if (font2)
                XftDrawStringUtf8 (draw,
                                   &black,
                                   font2,
                                   2 + extents1.xOff,
                                   2 + ascent,
                                   (unsigned char *) string2,
                                   strlen (string2));

        XftDrawDestroy (draw);

        if (font1)
                XftFontClose (xdisplay, font1);
        if (font2)
                XftFontClose (xdisplay, font2);

        tmp_pixbuf = gdk_pixbuf_get_from_drawable (NULL, pixmap, colormap, 0, 0, 0, 0, width, height);
        pixbuf = gdk_pixbuf_scale_simple (tmp_pixbuf, 1 * width, 1 * height, GDK_INTERP_TILES);

        g_object_unref (pixmap);
        g_object_unref (tmp_pixbuf);

        g_object_set_data_full (G_OBJECT (darea),
                                "sample-pixbuf",
                                pixbuf,
                                (GDestroyNotify) g_object_unref);

        g_signal_connect (darea, "size_request", G_CALLBACK (sample_size_request), NULL);
        g_signal_connect (darea, "expose_event", G_CALLBACK (sample_expose), NULL);
}

#endif
