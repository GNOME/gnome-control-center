/* -*- mode: C; c-basic-offset: 4 -*- */
#include <ft2build.h>
#include FT_FREETYPE_H
#include <X11/Xft/Xft.h>
#include <gtk/gtk.h>
#include <gdk/gdkx.h>

FT_Error FT_New_Face_From_URI(FT_Library library,
			      const gchar *uri,
			      FT_Long face_index,
			      FT_Face *aface);

/* height is a little more than needed to display all sizes */
#define FONTAREA_WIDTH 500
#define FONTAREA_HEIGHT 236

static GdkPixmap *
create_text_pixmap(GtkWidget *drawing_area, FT_Face face)
{
    static const gint sizes[] = { 8, 10, 12, 18, 24, 36, 48, 72 };
    gint i, pos_y, textlen;
    GdkPixmap *pixmap;
    gchar *text;

    Display *xdisplay;
    Drawable xdrawable;
    Visual *xvisual;
    Colormap xcolormap;
    XftDraw *draw;
    XftColor colour;

    text = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
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
    for (i = 0; i < G_N_ELEMENTS(sizes); i++) {
	FcPattern *pattern;
	XftFont *font;

	pattern = FcPatternBuild(NULL,
				 FC_FT_FACE, FcTypeFTFace, face,
				 FC_PIXEL_SIZE, FcTypeDouble, (double)sizes[i],
				 NULL);
	font = XftFontOpenPattern(xdisplay, pattern);
	FcPatternDestroy(pattern);
	if (!font) {
	    g_printerr("could not load Xft face\n");
	    goto endloop;
	}
	XftDrawString8(draw, &colour, font, 5, pos_y + font->ascent,
		       text, textlen);
	XftFontClose(xdisplay, font);
    endloop:
	pos_y += sizes[i];
    }

    return pixmap;
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
    GtkWidget *window;
    GtkWidget *drawing_area;
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

    drawing_area = gtk_drawing_area_new();
    gtk_widget_modify_bg(drawing_area, GTK_STATE_NORMAL, &white);
    gtk_widget_set_double_buffered(drawing_area, FALSE);
    gtk_container_add(GTK_CONTAINER(window), drawing_area);

    pixmap = create_text_pixmap(drawing_area, face);

    g_signal_connect(drawing_area, "expose_event",
		     G_CALLBACK(expose_event), pixmap);
    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    gtk_widget_show_all(window);

    gtk_main();
    return 0;
}
