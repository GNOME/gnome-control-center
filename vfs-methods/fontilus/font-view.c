/* -*- mode: C; c-basic-offset: 4 -*- */
#include <ft2build.h>
#include FT_FREETYPE_H
#include <X11/Xft/Xft.h>
#include <gtk/gtk.h>
#include <gdk/gdkx.h>

static XftFont *font;
static XftDraw *draw = NULL;
static XftColor colour;

FT_Error FT_New_Face_From_URI(FT_Library library,
			      const gchar *uri,
			      FT_Long face_index,
			      FT_Face *aface);

static void
realize(GtkWidget *widget)
{
    Display *xdisplay;
    Drawable xdrawable;
    Visual *xvisual;
    Colormap xcolormap;

    xdisplay = GDK_WINDOW_XDISPLAY(widget->window);
    xdrawable = GDK_DRAWABLE_XID(widget->window);
    xvisual = GDK_VISUAL_XVISUAL(gdk_drawable_get_visual(widget->window));
    xcolormap = GDK_COLORMAP_XCOLORMAP(gdk_drawable_get_colormap(widget->window));

    draw = XftDrawCreate(xdisplay, xdrawable, xvisual, xcolormap);
    if (!draw) {
	g_warning("could not create Xft drawable");
	return;
    }
    XftColorAllocName(xdisplay, xvisual, xcolormap, "black", &colour);
}

static gboolean
expose_event(GtkWidget *widget, GdkEventExpose *event)
{
    XftDrawString8(draw, &colour, font, 50, 50, "Foo bar", 7);
}

int
main(int argc, char **argv)
{
    FT_Error error;
    FT_Library library;
    FT_Face face;
    FcPattern *pattern;
    GtkWidget *window;
    GtkWidget *drawing_area;
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

    pattern = FcPatternBuild(NULL,
			     FC_FT_FACE, FcTypeFTFace, face,
			     FC_PIXEL_SIZE, FcTypeDouble, 48.0,
			     NULL);
    font = XftFontOpenPattern(GDK_DISPLAY(), pattern);
    if (!font) {
	g_printerr("could not load face\n");
	return 1;
    }

    g_message("ascent=%d, descent=%d, height=%d",
	      font->ascent, font->descent, font->height);

    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "Foo");

    drawing_area = gtk_drawing_area_new();
    gtk_widget_modify_bg(drawing_area, GTK_STATE_NORMAL, &white);
    gtk_widget_set_double_buffered(drawing_area, FALSE);
    gtk_widget_set_size_request(drawing_area, 300, 300);
    gtk_container_add(GTK_CONTAINER(window), drawing_area);

    g_signal_connect_after(drawing_area, "realize",
			   G_CALLBACK(realize), NULL);
    g_signal_connect(drawing_area, "expose_event",
		     G_CALLBACK(expose_event), NULL);
    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    gtk_widget_show_all(window);

    gtk_main();
    return 0;
}
