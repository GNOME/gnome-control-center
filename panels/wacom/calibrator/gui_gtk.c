/*
 * Copyright (c) 2009 Tias Guns
 * Copyright (c) 2009 Soren Hauberg
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "config.h"

#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <cairo.h>

#include "calibrator.h"
#include "gui_gtk.h"

/* Timeout parameters */
#define TIME_STEP		100   /* in milliseconds */
#define MAX_TIME		15000 /* 5000 = 5 sec */

/* Clock appereance */
#define CROSS_LINES		47
#define CROSS_CIRCLE		7
#define CROSS_CIRCLE2		27
#define CLOCK_RADIUS		50
#define CLOCK_LINE_WIDTH	10

/* Text printed on screen */
#define HELP_TEXT_TITLE N_("Screen Calibration")
#define HELP_TEXT_MAIN  N_("Please tap the target markers as they appear on screen to calibrate the tablet.")

static void
set_display_size(struct CalibArea *calib_area,
                 int               width,
                 int               height)
{
    int delta_x;
    int delta_y;

    calib_area->display_width = width;
    calib_area->display_height = height;

    /* Compute absolute circle centers */
    delta_x = calib_area->display_width/NUM_BLOCKS;
    delta_y = calib_area->display_height/NUM_BLOCKS;

    calib_area->X[UL] = delta_x;
    calib_area->Y[UL] = delta_y;

    calib_area->X[UR] = calib_area->display_width - delta_x - 1;
    calib_area->Y[UR] = delta_y;

    calib_area->X[LL] = delta_x;
    calib_area->Y[LL] = calib_area->display_height - delta_y - 1;

    calib_area->X[LR] = calib_area->display_width - delta_x - 1;
    calib_area->Y[LR] = calib_area->display_height - delta_y - 1;

    /* reset calibration if already started */
    reset(calib_area->calibrator);
}

static void
resize_display(struct CalibArea *calib_area)
{
    /* check that screensize did not change (if no manually specified geometry) */
    GtkAllocation allocation;
    gtk_widget_get_allocation(calib_area->drawing_area, &allocation);
    if (calib_area->calibrator->geometry == NULL &&
        (calib_area->display_width != allocation.width ||
         calib_area->display_height != allocation.height ))
    {
        set_display_size(calib_area, allocation.width, allocation.height);
    }
}

static void
draw(GtkWidget *widget, cairo_t *cr, gpointer data)
{
    struct CalibArea *calib_area = (struct CalibArea*)data;
    int i;
    double x;
    double y;
    PangoLayout *layout;
    PangoRectangle logical_rect;
    GtkStyleContext *context;
    char *markup;

    resize_display(calib_area);

    context = gtk_widget_get_style_context (widget);

    /* Black background and reset the operator */
    cairo_set_operator (cr, CAIRO_OPERATOR_CLEAR);
    cairo_paint (cr);
    cairo_set_operator (cr, CAIRO_OPERATOR_OVER);

    /* Print the text */
    cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);

    layout = pango_layout_new (gtk_widget_get_pango_context (widget));
    pango_layout_set_alignment (layout, PANGO_ALIGN_CENTER);
    markup = g_strdup_printf ("<big><b>%s</b></big>\n<big>%s</big>",
			      _(HELP_TEXT_TITLE),
			      _(HELP_TEXT_MAIN));
    pango_layout_set_markup (layout, markup, -1);
    g_free (markup);

    pango_layout_get_pixel_extents (layout, NULL, &logical_rect);

    x = (calib_area->display_width - logical_rect.width) / 2 + logical_rect.x;
    y = (calib_area->display_height - logical_rect.height) / 2  - logical_rect.height - 20 + logical_rect.y;
    cairo_set_line_width(cr, 2);
    cairo_rectangle(cr, x - 10 - 0.5, y - 10 - 0.5,
            logical_rect.width + 20 + 1, logical_rect.height + 20 + 1);

    /* Print help lines */
    gtk_render_layout (context, cr,
		       x + logical_rect.x,
		       y + logical_rect.y,
		       layout);
    g_object_unref (layout);

    /* Draw the points */
    i = calib_area->calibrator->num_clicks;

    cairo_set_line_width(cr, 1);
    cairo_move_to(cr, calib_area->X[i] - CROSS_LINES, calib_area->Y[i] - 0.5);
    cairo_rel_line_to(cr, CROSS_LINES*2, 0);
    cairo_move_to(cr, calib_area->X[i] - 0.5, calib_area->Y[i] - CROSS_LINES);
    cairo_rel_line_to(cr, 0, CROSS_LINES*2);
    cairo_stroke(cr);

    cairo_set_line_width(cr, 2);
    cairo_arc(cr, calib_area->X[i] - 0.5, calib_area->Y[i] - 0.5, CROSS_CIRCLE, 0.0, 2.0 * M_PI);
    cairo_stroke(cr);

    cairo_set_line_width(cr, 5);
    cairo_arc(cr, calib_area->X[i] - 0.5, calib_area->Y[i] - 0.5, CROSS_CIRCLE2, 0.0, 2.0 * M_PI);
    cairo_stroke(cr);

    /* Draw the clock background */
    cairo_arc(cr, calib_area->display_width/2, calib_area->display_height/2, CLOCK_RADIUS/2, 0.0, 2.0 * M_PI);
    cairo_set_source_rgb(cr, 0.5, 0.5, 0.5);
    cairo_fill_preserve(cr);
    cairo_stroke(cr);

    cairo_set_line_width(cr, CLOCK_LINE_WIDTH);
    cairo_arc(cr, calib_area->display_width/2, calib_area->display_height/2, (CLOCK_RADIUS - CLOCK_LINE_WIDTH)/2,
         3/2.0*M_PI, (3/2.0*M_PI) + ((double)calib_area->time_elapsed/(double)MAX_TIME) * 2*M_PI);
    cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
    cairo_stroke(cr);

    /* Draw the message (if any) */
    if (calib_area->message != NULL)
    {
        cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);

        /* Frame the message */
        layout = pango_layout_new (gtk_widget_get_pango_context (widget));
        pango_layout_set_alignment (layout, PANGO_ALIGN_CENTER);
        pango_layout_set_text (layout, calib_area->message, -1);
        pango_layout_get_pixel_extents (layout, NULL, &logical_rect);

        x = (calib_area->display_width - logical_rect.width) / 2 + logical_rect.x;
        y = (calib_area->display_height - logical_rect.height + CLOCK_RADIUS) / 2 + 60 + logical_rect.y;
        cairo_set_line_width(cr, 2);
        cairo_rectangle(cr, x - 10 - 0.5 , y - logical_rect.height - 10 - 0.5,
                logical_rect.width + 20 + 1, logical_rect.height + 25 + 1);

        /* Print the message */
	gtk_render_layout (context, cr,
			   x + logical_rect.x,
			   y + logical_rect.y,
			   layout);
	g_object_unref (layout);
    }
}

static void
draw_message(struct CalibArea *calib_area,
             const char       *msg)
{
    calib_area->message = msg;
}

static void
redraw(struct CalibArea *calib_area)
{
    GdkWindow *win = gtk_widget_get_window(calib_area->drawing_area);
    if (win)
    {
        GdkRectangle rect;
        rect.x = 0;
        rect.y = 0;
        rect.width = calib_area->display_width;
        rect.height = calib_area->display_height;
        gdk_window_invalidate_rect(win, &rect, FALSE);
    }
}

static gboolean
on_button_press_event(GtkWidget      *widget,
                      GdkEventButton *event,
                      gpointer        data)
{
    struct CalibArea *calib_area = (struct CalibArea*)data;
    gboolean success;

    /* Handle click */
    calib_area->time_elapsed = 0;
    success = add_click(calib_area->calibrator, (int)event->x_root, (int)event->y_root);

    if (!success && calib_area->calibrator->num_clicks == 0)
        draw_message(calib_area, "Mis-click detected, restarting...");
    else
        draw_message(calib_area, NULL);

    /* Are we done yet? */
    if (calib_area->calibrator->num_clicks >= 4)
    {
        GtkWidget *parent = gtk_widget_get_parent(calib_area->drawing_area);
        if (parent)
            gtk_widget_destroy(parent);
        return TRUE;
    }

    /* Force a redraw */
    redraw(calib_area);

    return TRUE;
}

static gboolean
on_key_press_event(GtkWidget   *widget,
                   GdkEventKey *event,
                   gpointer     data)
{
    struct CalibArea *calib_area = (struct CalibArea*)data;
    GtkWidget *parent = gtk_widget_get_parent(calib_area->drawing_area);
    if (parent)
        gtk_widget_destroy(parent);
    return TRUE;
}

static gboolean
on_timer_signal(struct CalibArea *calib_area)
{
    GdkWindow *win;
    GtkWidget *parent = gtk_widget_get_parent(calib_area->drawing_area);

    calib_area->time_elapsed += TIME_STEP;
    if (calib_area->time_elapsed > MAX_TIME || parent == NULL)
    {
        if (parent)
            gtk_widget_destroy(parent);
        return FALSE;
    }

    /* Update clock */
    win = gtk_widget_get_window(calib_area->drawing_area);
    if (win)
    {
        GdkRectangle rect;
        rect.x = calib_area->display_width/2 - CLOCK_RADIUS - CLOCK_LINE_WIDTH;
        rect.y = calib_area->display_height/2 - CLOCK_RADIUS - CLOCK_LINE_WIDTH;
        rect.width = 2 * CLOCK_RADIUS + 1 + 2 * CLOCK_LINE_WIDTH;
        rect.height = 2 * CLOCK_RADIUS + 1 + 2 * CLOCK_LINE_WIDTH;
        gdk_window_invalidate_rect(win, &rect, FALSE);
    }

    return TRUE;
}

static struct CalibArea*
CalibrationArea_(struct Calib *c)
{
    struct CalibArea *calib_area;
    const char *geo = c->geometry;

    calib_area = g_new0 (struct CalibArea, 1);
    calib_area->calibrator = c;
    calib_area->drawing_area = gtk_drawing_area_new();

    /* Listen for mouse events */
    gtk_widget_add_events(calib_area->drawing_area, GDK_KEY_PRESS_MASK | GDK_BUTTON_PRESS_MASK);
    gtk_widget_set_can_focus(calib_area->drawing_area, TRUE);

    /* Connect callbacks */
    g_signal_connect(calib_area->drawing_area, "draw", G_CALLBACK(draw), calib_area);
    g_signal_connect(calib_area->drawing_area, "button-press-event", G_CALLBACK(on_button_press_event), calib_area);
    g_signal_connect(calib_area->drawing_area, "key-press-event", G_CALLBACK(on_key_press_event), calib_area);

    /* parse geometry string */
    if (geo != NULL)
    {
        int gw,gh;
        int res = sscanf(geo,"%dx%d",&gw,&gh);
        if (res != 2)
            geo = NULL;
        else
            set_display_size(calib_area, gw, gh );\
    }
    if (geo == NULL)
    {
        GtkAllocation allocation;
        gtk_widget_get_allocation(calib_area->drawing_area, &allocation);
        set_display_size(calib_area, allocation.width, allocation.height);
    }

    /* Setup timer for animation */
    calib_area->anim_id = g_timeout_add(TIME_STEP, (GSourceFunc)on_timer_signal, calib_area);

    return calib_area;
}

/**
 * Creates the windows and other objects required to do calibration
 * under GTK and then starts the main loop. When the main loop exits,
 * the calibration will be calculated (if possible) and this function
 * will then return ('TRUE' if successful, 'FALSE' otherwise).
 */
gboolean
run_gui(struct Calib *c,
        XYinfo       *new_axis,
        gboolean         *swap)
{
    gboolean success;
    struct CalibArea *calib_area = CalibrationArea_(c);
    GdkRGBA black;
    GdkWindow *window;

    g_debug ("Current calibration: %d, %d, %d, %d\n",
	     c->old_axis.x_min,
	     c->old_axis.y_min,
	     c->old_axis.x_max,
	     c->old_axis.y_max);

    GdkScreen *screen = gdk_screen_get_default();
    GtkWidget *win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    GdkRectangle rect;
    /*int num_monitors = screen->get_n_monitors(); TODO, multiple monitors?*/

    g_signal_connect(G_OBJECT(win), "destroy", G_CALLBACK(gtk_main_quit), NULL);

    gdk_screen_get_monitor_geometry(screen, 0, &rect);

    /* when no window manager: explicitely take size of full screen */
    gtk_window_move(GTK_WINDOW(win), rect.x, rect.y);
    gtk_window_set_default_size(GTK_WINDOW(win), rect.width, rect.height);

    /* in case of window manager: set as full screen to hide window decorations */
    gtk_window_fullscreen(GTK_WINDOW(win));

    gtk_container_add(GTK_CONTAINER(win), calib_area->drawing_area);

    /* Black background */
    gdk_rgba_parse (&black, "000");

    gtk_widget_realize (calib_area->drawing_area);
    window = gtk_widget_get_window (calib_area->drawing_area);
    gdk_window_set_background_rgba (window, &black);

    gtk_widget_realize (win);
    window = gtk_widget_get_window (win);
    gdk_window_set_background_rgba (window, &black);

    gtk_widget_show_all(win);

    gtk_main();

    g_source_remove (calib_area->anim_id);

    success = finish(calib_area->calibrator, calib_area->display_width, calib_area->display_height, new_axis, swap);

    g_debug ("Final calibration: %d, %d, %d, %d\n",
	     new_axis->x_min,
	     new_axis->y_min,
	     new_axis->x_max,
	     new_axis->y_max);

   return success;
}
