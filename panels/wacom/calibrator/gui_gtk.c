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

typedef struct
{
    struct Calib* calibrator;
    double X[4], Y[4];
    int display_width, display_height;
    int time_elapsed;

    const char* message;

    guint anim_id;

    GtkWidget *window;
} CalibArea;


/* Window parameters */
#define WINDOW_OPACITY		0.9

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
set_display_size(CalibArea *calib_area,
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
resize_display(CalibArea *calib_area)
{
    /* check that screensize did not change (if no manually specified geometry) */
    GtkAllocation allocation;
    gtk_widget_get_allocation(calib_area->window, &allocation);
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
    CalibArea *calib_area = (CalibArea*)data;
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
    y = (calib_area->display_height - logical_rect.height) / 2  - logical_rect.height - 40 + logical_rect.y;
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
draw_message(CalibArea *calib_area,
             const char       *msg)
{
    calib_area->message = msg;
}

static void
redraw(CalibArea *calib_area)
{
    GdkWindow *win = gtk_widget_get_window(calib_area->window);
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
    CalibArea *calib_area = (CalibArea*)data;
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
        gtk_widget_destroy (calib_area->window);
        return FALSE;
    }

    /* Force a redraw */
    redraw(calib_area);

    return FALSE;
}

static gboolean
on_key_release_event(GtkWidget   *widget,
                     GdkEventKey *event,
                     gpointer     data)
{
    CalibArea *calib_area = (CalibArea*)data;

    if (event->type != GDK_KEY_RELEASE)
        return FALSE;
    if (event->keyval != GDK_KEY_Escape)
        return FALSE;

    gtk_widget_destroy (calib_area->window);
    return FALSE;
}

static gboolean
on_timer_signal(CalibArea *calib_area)
{
    GdkWindow *win;

    calib_area->time_elapsed += TIME_STEP;
    if (calib_area->time_elapsed > MAX_TIME)
    {
        gtk_widget_destroy (calib_area->window);
        return FALSE;
    }

    /* Update clock */
    win = gtk_widget_get_window (calib_area->window);
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

static CalibArea*
calibration_area_new (struct Calib *c)
{
    CalibArea *calib_area;
    GdkWindow *window;
    GdkRGBA black;

    calib_area = g_new0 (CalibArea, 1);
    calib_area->calibrator = c;

    /* Set up the window */
    calib_area->window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
    gtk_widget_set_app_paintable (GTK_WIDGET (calib_area->window), TRUE);

    /* Black background */
    gdk_rgba_parse (&black, "rgb(0,0,0)");
    gtk_window_set_opacity (GTK_WINDOW (calib_area->window), WINDOW_OPACITY);

    gtk_widget_realize (calib_area->window);
    window = gtk_widget_get_window (calib_area->window);
    gdk_window_set_background_rgba (window, &black);

    /* Listen for mouse events */
    gtk_widget_add_events (calib_area->window, GDK_KEY_RELEASE_MASK | GDK_BUTTON_PRESS_MASK);
    gtk_widget_set_can_focus (calib_area->window, TRUE);
    gtk_window_fullscreen (GTK_WINDOW (calib_area->window));

    /* Connect callbacks */
    g_signal_connect (calib_area->window, "draw",
		      G_CALLBACK(draw), calib_area);
    g_signal_connect (calib_area->window, "button-press-event",
		      G_CALLBACK(on_button_press_event), calib_area);
    g_signal_connect (calib_area->window, "key-release-event",
		      G_CALLBACK(on_key_release_event), calib_area);
    g_signal_connect (calib_area->window, "destroy",
		      G_CALLBACK (gtk_main_quit), NULL);

    /* FIXME */
#if 0
    /* parse geometry string */
    if (c->geometry != NULL)
    {
        int gw,gh;
        int res = sscanf(c->geometry,"%dx%d",&gw,&gh);
        if (res != 2)
            c->geometry = NULL;
        else
            set_display_size(calib_area, gw, gh );\
    }
    if (c->geometry == NULL)
    {
        GtkAllocation allocation;
        gtk_widget_get_allocation(calib_area->drawing_area, &allocation);
        set_display_size(calib_area, allocation.width, allocation.height);
    }
#endif

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
    CalibArea *calib_area;
    GdkScreen *screen;
    GdkRectangle rect;

    g_debug ("Current calibration: %d, %d, %d, %d\n",
	     c->old_axis.x_min,
	     c->old_axis.y_min,
	     c->old_axis.x_max,
	     c->old_axis.y_max);

    /* Which monitor are we on? */
    screen = gdk_screen_get_default();
    /*int num_monitors = screen->get_n_monitors(); TODO, multiple monitors?*/
    gdk_screen_get_monitor_geometry(screen, 0, &rect);

    calib_area = calibration_area_new (c);

    gtk_window_move (GTK_WINDOW (calib_area->window), rect.x, rect.y);
    gtk_window_set_default_size (GTK_WINDOW (calib_area->window), rect.width, rect.height);

    gtk_widget_show_all(calib_area->window);

    gtk_main();

    g_source_remove (calib_area->anim_id);

    success = finish(calib_area->calibrator, calib_area->display_width, calib_area->display_height, new_axis, swap);

    g_free (calib_area);

    if (success)
	    g_debug ("Final calibration: %d, %d, %d, %d\n",
		     new_axis->x_min,
		     new_axis->y_min,
		     new_axis->x_max,
		     new_axis->y_max);
    else
	    g_debug ("Calibration was aborted or timed out");

   return success;
}
