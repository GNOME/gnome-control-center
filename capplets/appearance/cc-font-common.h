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

#define GTK_FONT_KEY           "/desktop/gnome/interface/font_name"
#define DESKTOP_FONT_KEY       "/apps/nautilus/preferences/desktop_font"

#define METACITY_DIR "/apps/metacity/general"
#define WINDOW_TITLE_FONT_KEY METACITY_DIR "/titlebar_font"
#define WINDOW_TITLE_USES_SYSTEM_KEY METACITY_DIR "/titlebar_uses_system_font"
#define MONOSPACE_FONT_KEY "/desktop/gnome/interface/monospace_font_name"
#define DOCUMENT_FONT_KEY "/desktop/gnome/interface/document_font_name"

#define FONT_RENDER_DIR "/desktop/gnome/font_rendering"
#define FONT_ANTIALIASING_KEY FONT_RENDER_DIR "/antialiasing"
#define FONT_HINTING_KEY      FONT_RENDER_DIR "/hinting"
#define FONT_RGBA_ORDER_KEY   FONT_RENDER_DIR "/rgba_order"
#define FONT_DPI_KEY          FONT_RENDER_DIR "/dpi"


/* X servers sometimes lie about the screen's physical dimensions, so we cannot
 * compute an accurate DPI value.  When this happens, the user gets fonts that
 * are too huge or too tiny.  So, we see what the server returns:  if it reports
 * something outside of the range [DPI_LOW_REASONABLE_VALUE,
 * DPI_HIGH_REASONABLE_VALUE], then we assume that it is lying and we use
 * DPI_FALLBACK instead.
 *
 * See get_dpi_from_gconf_or_server() below, and also
 * https://bugzilla.novell.com/show_bug.cgi?id=217790
 */
#define DPI_FALLBACK 96
#define DPI_LOW_REASONABLE_VALUE 50
#define DPI_HIGH_REASONABLE_VALUE 500

#define MAX_FONT_POINT_WITHOUT_WARNING 32
#define MAX_FONT_SIZE_WITHOUT_WARNING MAX_FONT_POINT_WITHOUT_WARNING*1024

typedef enum {
        ANTIALIAS_NONE,
        ANTIALIAS_GRAYSCALE,
        ANTIALIAS_RGBA
} Antialiasing;

typedef enum {
        HINT_NONE,
        HINT_SLIGHT,
        HINT_MEDIUM,
        HINT_FULL
} Hinting;

typedef enum {
        RGBA_RGB,
        RGBA_BGR,
        RGBA_VRGB,
        RGBA_VBGR
} RgbaOrder;

void setup_font_sample (GtkWidget   *drawing_area,
                        Antialiasing antialiasing,
                        Hinting      hinting);
