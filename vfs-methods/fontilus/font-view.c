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

#ifdef ENABLE_NLS
#  include <libintl.h>
#  define _(String) gettext(String)
#  define N_(String) gettext_noop(String)
#else
#  define _(String) (String)
#  define N_(String) (String)
#  define textdomain(String) (String)
#  define bindtextdomain(Domain,Directory) (Domain)
#  define bind_textdomain_codeset(Domain,Codeset) (Domain)
#endif

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_TYPE1_TABLES_H
#include FT_SFNT_NAMES_H
#include FT_TRUETYPE_IDS_H
#include <X11/Xft/Xft.h>
#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include <libgnomevfs/gnome-vfs.h>
#include <libgnomevfs/gnome-vfs-mime-handlers.h>

FT_Error FT_New_Face_From_URI(FT_Library library,
			      const gchar *uri,
			      FT_Long face_index,
			      FT_Face *aface);

/* height is a little more than needed to display all sizes */
#define FONTAREA_WIDTH 500
#define FONTAREA_HEIGHT 262

static void
draw_text(Display *xdisplay, XftDraw *draw, FT_Face face, gint pixel_size,
	  XftColor *colour, const gchar *text, gint textlen, gint *pos_y)
{
    FcPattern *pattern;
    XftFont *font;

    pattern = FcPatternBuild(NULL,
			     FC_FT_FACE, FcTypeFTFace, face,
			     FC_PIXEL_SIZE, FcTypeDouble, (double)pixel_size,
			     NULL);
    font = XftFontOpenPattern(xdisplay, pattern);
    FcPatternDestroy(pattern);
    if (!font) {
	g_printerr("could not load Xft face\n");
	goto end;
    }

    // g_message("target size=%d, ascent=%d, descent=%d, height=%d",
    //      pixel_size, font->ascent, font->descent, font->height);

    XftDrawString8(draw, colour, font, 5, *pos_y + font->ascent,
		   (gchar *)text, textlen);
    XftFontClose(xdisplay, font);
 end:
    *pos_y += pixel_size;
}

static GdkPixmap *
create_text_pixmap(GtkWidget *drawing_area, FT_Face face)
{
    gint i, pos_y, textlen, alpha_size;
    GdkPixmap *pixmap;
    gchar *text;

    Display *xdisplay;
    Drawable xdrawable;
    Visual *xvisual;
    Colormap xcolormap;
    XftDraw *draw;
    XftColor colour;

    text = _("The quick brown fox jumps over the lazy dog. 0123456789");
    textlen = strlen(text);

    /* create pixmap */
    gtk_widget_set_size_request(drawing_area, FONTAREA_WIDTH, FONTAREA_HEIGHT);
    gtk_widget_realize(drawing_area);
    pixmap = gdk_pixmap_new(drawing_area->window,
			    FONTAREA_WIDTH, FONTAREA_HEIGHT, -1);
    if (!pixmap)
	return NULL;
    gdk_draw_rectangle(pixmap, drawing_area->style->white_gc,
		       TRUE, 0, 0, FONTAREA_WIDTH, FONTAREA_HEIGHT);

    /* create the XftDraw */
    xdisplay = GDK_PIXMAP_XDISPLAY(pixmap);
    xdrawable = GDK_DRAWABLE_XID(pixmap);
    xvisual = GDK_VISUAL_XVISUAL(gdk_drawable_get_visual(pixmap));
    xcolormap = GDK_COLORMAP_XCOLORMAP(gdk_drawable_get_colormap(pixmap));

    draw = XftDrawCreate(xdisplay, xdrawable, xvisual, xcolormap);
    XftColorAllocName(xdisplay, xvisual, xcolormap, "black", &colour);

    pos_y = 4;

    if (FT_IS_SCALABLE(face)) {
	alpha_size = 24;
    } else {
	alpha_size = face->available_sizes[0].height;
	for (i = 0; i < face->num_fixed_sizes; i++) {
	    if (face->available_sizes[i].height <= 24)
		alpha_size = face->available_sizes[i].height;
	    else
		break;
	}
    }
    draw_text(xdisplay, draw, face, alpha_size, &colour,
	      "abcdefghijklmnopqrstuvwxyz", 26, &pos_y);
    draw_text(xdisplay, draw, face, alpha_size, &colour,
	      "ABCDEFGHIJKLMNOPQRSTUVWXYZ", 26, &pos_y);
    draw_text(xdisplay, draw, face, alpha_size, &colour,
	      "0123456789.:,;(*!?')", 20, &pos_y);

    pos_y += 8;

    /* bitmap fonts */
    if (!FT_IS_SCALABLE(face)) {
	for (i = 0; i < face->num_fixed_sizes; i++) {
	    draw_text(xdisplay, draw, face, face->available_sizes[i].height,
		      &colour, text, textlen, &pos_y);
	}
    } else {
	static const gint sizes[] = { 8, 10, 12, 24, 48, 72 };

	for (i = 0; i < G_N_ELEMENTS(sizes); i++) {
	    draw_text(xdisplay, draw, face, sizes[i],
		      &colour, text, textlen, &pos_y);
	}
    }

    return pixmap;
}

static void
add_row(GtkWidget *table, gint *row_p,
	const gchar *name, const gchar *value, gboolean multiline)
{
    gchar *bold_name;
    GtkWidget *name_w, *value_w;

    bold_name = g_strconcat("<b>", name, "</b>", NULL);
    name_w = gtk_label_new(bold_name);
    g_free(bold_name);
    gtk_misc_set_alignment(GTK_MISC(name_w), 1.0, 0.0);
    gtk_label_set_use_markup(GTK_LABEL(name_w), TRUE);

    if (multiline) {
	GtkWidget *textview;
	GtkTextBuffer *buffer;

	textview = gtk_text_view_new();
	gtk_text_view_set_editable(GTK_TEXT_VIEW(textview), FALSE);
	gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(textview), FALSE);
	gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(textview), GTK_WRAP_WORD);
	buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(textview));
	gtk_text_buffer_set_text(buffer, value, -1);

	value_w = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(value_w),
			GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(value_w),
					    GTK_SHADOW_IN);
	gtk_widget_set_size_request(value_w, -1, 50);
	gtk_container_add(GTK_CONTAINER(value_w), textview);
    } else {
	value_w = gtk_label_new(value);
	gtk_misc_set_alignment(GTK_MISC(value_w), 0.0, 0.5);
	gtk_label_set_selectable(GTK_LABEL(value_w), TRUE);
    }

    gtk_table_attach(GTK_TABLE(table), name_w, 0, 1, *row_p, *row_p + 1,
		     GTK_FILL, GTK_FILL, 0, 0);
    gtk_table_attach(GTK_TABLE(table), value_w, 1, 2, *row_p, *row_p + 1,
		     GTK_FILL|GTK_EXPAND, GTK_FILL, 0, 0);

    (*row_p)++;
}

static void
add_face_info(GtkWidget *table, gint *row_p, const gchar *uri, FT_Face face)
{
    GnomeVFSFileInfo *file_info;
    PS_FontInfoRec ps_info;

    add_row(table, row_p, _("Name:"), face->family_name, FALSE);

    if (face->style_name)
	add_row(table, row_p, _("Style:"), face->style_name, FALSE);

    file_info = gnome_vfs_file_info_new();
    if (gnome_vfs_get_file_info
	(uri, file_info, GNOME_VFS_FILE_INFO_GET_MIME_TYPE) == GNOME_VFS_OK) {

	if ((file_info->valid_fields&GNOME_VFS_FILE_INFO_FIELDS_MIME_TYPE)!=0){
	    const gchar *type = gnome_vfs_mime_get_description(file_info->mime_type);

	    add_row(table, row_p, _("Type:"),
		    type ? type : file_info->mime_type, FALSE);
	}

	if ((file_info->valid_fields & GNOME_VFS_FILE_INFO_FIELDS_SIZE) != 0) {
	    gchar *size;
	    size = gnome_vfs_format_file_size_for_display(file_info->size);
	    add_row(table, row_p, _("Size:"), size, FALSE);
	    g_free(size);
	}
    }
    gnome_vfs_file_info_unref(file_info);

    if (FT_IS_SFNT(face)) {
	gint i, len;
	gchar *version = NULL, *copyright = NULL, *description = NULL;

	len = FT_Get_Sfnt_Name_Count(face);
	for (i = 0; i < len; i++) {
	    FT_SfntName sname;

	    if (FT_Get_Sfnt_Name(face, i, &sname) != 0)
		continue;

	    /* only handle the unicode names for US langid */
	    if (!(sname.platform_id == TT_PLATFORM_MICROSOFT &&
		  sname.encoding_id == TT_MS_ID_UNICODE_CS &&
		  sname.language_id == TT_MS_LANGID_ENGLISH_UNITED_STATES))
		continue;

	    switch (sname.name_id) {
	    case TT_NAME_ID_COPYRIGHT:
		g_free(copyright);
		copyright = g_convert(sname.string, sname.string_len,
				      "UTF-8", "UTF-16BE", NULL, NULL, NULL);
		break;
	    case TT_NAME_ID_VERSION_STRING:
		g_free(version);
		version = g_convert(sname.string, sname.string_len,
				    "UTF-8", "UTF-16BE", NULL, NULL, NULL);
		break;
	    case TT_NAME_ID_DESCRIPTION:
		g_free(description);
		description = g_convert(sname.string, sname.string_len,
					"UTF-8", "UTF-16BE", NULL, NULL, NULL);
		break;
	    default:
		break;
	    }
	}
	if (version) {
	    add_row(table, row_p, _("Version:"), version, FALSE);
	    g_free(version);
	}
	if (copyright) {
	    add_row(table, row_p, _("Copyright:"), copyright, TRUE);
	    g_free(copyright);
	}
	if (description) {
	    add_row(table, row_p, _("Description:"), description, TRUE);
	    g_free(description);
	}
    } else if (FT_Get_PS_Font_Info(face, &ps_info) == 0) {
	if (ps_info.version && g_utf8_validate(ps_info.version, -1, NULL))
	    add_row(table, row_p, _("Version:"), ps_info.version, FALSE);
	if (ps_info.notice && g_utf8_validate(ps_info.notice, -1, NULL))
	    add_row(table, row_p, _("Copyright:"), ps_info.notice, TRUE);
    }
}

static gboolean
expose_event(GtkWidget *widget, GdkEventExpose *event, GdkPixmap *pixmap)
{
    gdk_draw_drawable(widget->window,
		      widget->style->fg_gc[GTK_WIDGET_STATE (widget)],
		      pixmap,
		      event->area.x, event->area.y,
		      event->area.x, event->area.y,
		      event->area.width, event->area.height);
    return FALSE;
}

int
main(int argc, char **argv)
{
    FT_Error error;
    FT_Library library;
    FT_Face face;
    gchar *title;
    gint row;
    GtkWidget *window, *vbox, *table, *frame, *drawing_area;
    GdkPixmap *pixmap;
    GdkColor white = { 0, 0xffff, 0xffff, 0xffff };

    bindtextdomain(GETTEXT_PACKAGE, FONTILUS_LOCALEDIR);
    bind_textdomain_codeset(GETTEXT_PACKAGE, "UTF-8");
    textdomain(GETTEXT_PACKAGE);

    gtk_init(&argc, &argv);

    if (argc != 2) {
	g_printerr("usage: foo fontfile\n");
	return 1;
    }

    if (!gnome_vfs_init()) {
	g_printerr("could not initialise gnome-vfs\n");
	return 1;
    }

    if (!XftInitFtLibrary()) {
	g_printerr("could not initialise freetype library\n");
	return 1;
    }

    error = FT_Init_FreeType(&library);
    if (error) {
	g_printerr("could not initialise freetype\n");
	return 1;
    }  

    error = FT_New_Face_From_URI(library, argv[1], 0, &face);
    if (error) {
	g_printerr("could not load face '%s'\n", argv[1]);
	return 1;
    }

    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    title = g_strconcat(face->family_name,
			face->style_name ? ", " : "",
			face->style_name, NULL);
    gtk_window_set_title(GTK_WINDOW(window), title);
    g_free(title);
    gtk_window_set_resizable(GTK_WINDOW(window), FALSE);

    vbox = gtk_vbox_new(FALSE, 0);
    gtk_container_add(GTK_CONTAINER(window), vbox);

    table = gtk_table_new(1, 2, FALSE);
    gtk_container_set_border_width(GTK_CONTAINER(table), 5);
    gtk_box_pack_start(GTK_BOX(vbox), table, TRUE, TRUE, 0);

    row = 0;
    add_face_info(table, &row, argv[1], face);

    gtk_table_set_col_spacings(GTK_TABLE(table), 8);
    gtk_table_set_row_spacings(GTK_TABLE(table), 2);

    frame = gtk_frame_new(NULL);
    gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_IN);
    gtk_box_pack_start(GTK_BOX(vbox), frame, FALSE, FALSE, 0);

    drawing_area = gtk_drawing_area_new();
    gtk_widget_modify_bg(drawing_area, GTK_STATE_NORMAL, &white);
    gtk_container_add(GTK_CONTAINER(frame), drawing_area);

    pixmap = create_text_pixmap(drawing_area, face);

    g_signal_connect(drawing_area, "expose_event",
		     G_CALLBACK(expose_event), pixmap);
    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    gtk_widget_show_all(window);

    gtk_main();
    return 0;
}
