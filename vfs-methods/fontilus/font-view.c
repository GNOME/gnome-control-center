/* -*- mode: C; c-basic-offset: 4 -*- */
#include <ft2build.h>
#include FT_FREETYPE_H
#include <X11/Xft/Xft.h>
#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include <libgnomevfs/gnome-vfs.h>

#ifndef _
#  define _(s) (s)
#endif

FT_Error FT_New_Face_From_URI(FT_Library library,
			      const gchar *uri,
			      FT_Long face_index,
			      FT_Face *aface);

/* height is a little more than needed to display all sizes */
#define FONTAREA_WIDTH 500
#define FONTAREA_HEIGHT 236

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
    XftDrawString8(draw, colour, font, 5, *pos_y + font->ascent,
		   (gchar *)text, textlen);
    XftFontClose(xdisplay, font);
 end:
    *pos_y += pixel_size;
}

static GdkPixmap *
create_text_pixmap(GtkWidget *drawing_area, FT_Face face)
{
    gint i, pos_y, textlen;
    GdkPixmap *pixmap;
    gchar *text;

    Display *xdisplay;
    Drawable xdrawable;
    Visual *xvisual;
    Colormap xcolormap;
    XftDraw *draw;
    XftColor colour;

    text = _("ABCDEFGHIJKLMNOPQRSTUVWXYZ 0123456789");
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
    /* bitmap fonts */
    if (!FT_IS_SCALABLE(face)) {
	for (i = 0; i < face->num_fixed_sizes; i++) {
	    draw_text(xdisplay, draw, face, face->available_sizes[i].width,
		      &colour, text, textlen, &pos_y);
	}
    } else {
	static const gint sizes[] = { 8, 10, 12, 18, 24, 36, 48, 72 };

	for (i = 0; i < G_N_ELEMENTS(sizes); i++) {
	    draw_text(xdisplay, draw, face, sizes[i],
		      &colour, text, textlen, &pos_y);
	}
    }

    return pixmap;
}

static void
add_row(GtkWidget *table, gint *row_p, const gchar *name, const gchar *value)
{
    gchar *bold_name;
    GtkWidget *name_w, *value_w;

    bold_name = g_strconcat("<b>", name, "</b>", NULL);
    name_w = gtk_label_new(bold_name);
    g_free(bold_name);
    gtk_misc_set_alignment(GTK_MISC(name_w), 1.0, 0.5);
    gtk_label_set_use_markup(GTK_LABEL(name_w), TRUE);

    value_w = gtk_label_new(value);
    gtk_misc_set_alignment(GTK_MISC(value_w), 0.0, 0.5);
    gtk_label_set_selectable(GTK_LABEL(value_w), TRUE);

    gtk_table_attach(GTK_TABLE(table), name_w, 0, 1, *row_p, *row_p + 1,
		     GTK_FILL, GTK_FILL, 0, 0);
    gtk_table_attach(GTK_TABLE(table), value_w, 1, 2, *row_p, *row_p + 1,
		     GTK_FILL|GTK_EXPAND, GTK_FILL, 0, 0);

    (*row_p)++;
}

static void
add_face_info(GtkWidget *table, gint *row_p, const gchar *uri, FT_Face face)
{
    gchar *filename;
    GnomeVFSFileInfo *file_info;

    add_row(table, row_p, _("Name:"), face->family_name);

    if (face->style_name)
	add_row(table, row_p, _("Style:"), face->style_name);

    filename = gnome_vfs_get_local_path_from_uri(uri);
    add_row(table, row_p, _("File name:"), filename ? filename : uri);
    g_free(filename);

    file_info = gnome_vfs_file_info_new();
    if (gnome_vfs_get_file_info
	(uri, file_info, GNOME_VFS_FILE_INFO_GET_MIME_TYPE) == GNOME_VFS_OK) {

	if ((file_info->valid_fields&GNOME_VFS_FILE_INFO_FIELDS_MIME_TYPE)!=0){
	    gchar *type = gnome_vfs_mime_get_description(file_info->mime_type);

	    add_row(table, row_p, _("Type:"),
		    type ? type : file_info->mime_type);
	    g_free(type);
	}

	if ((file_info->valid_fields & GNOME_VFS_FILE_INFO_FIELDS_SIZE) != 0) {
	    gchar *size;
	    size = gnome_vfs_format_file_size_for_display(file_info->size);
	    add_row(table, row_p, _("Size:"), size);
	    g_free(size);
	}
    }
    gnome_vfs_file_info_unref(file_info);
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
