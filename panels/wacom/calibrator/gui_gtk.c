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

#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <gtk/gtk.h>
#include <cairo.h>

#include "calibrator.h"
#include "gui_gtk.h"

/* Timeout parameters */
const int time_step = 100;  /* in milliseconds */
const int max_time = 15000; /* 5000 = 5 sec */

/* Clock appereance */
const int cross_lines = 25;
const int cross_circle = 4;
const int clock_radius = 50;
const int clock_line_width = 10;

/* Text printed on screen */
const int font_size = 16;
#define HELP_LINES (sizeof help_text / sizeof help_text[0])
const char *help_text[] = {
    "Touchscreen Calibration",
    "Press the point, use a stylus to increase precision.",
    "",
    "(To abort, press any key or wait)"
};

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
    double text_height;
    double text_width;
    double x;
    double y;
    cairo_text_extents_t extent;

    resize_display(calib_area);

    /* Print the text */
    cairo_set_font_size(cr, font_size);
    text_height = -1;
    text_width = -1;
    for (i = 0; i != HELP_LINES; i++)
    {
        cairo_text_extents(cr, help_text[i], &extent);
        text_width = MAX(text_width, extent.width);
        text_height = MAX(text_height, extent.height);
    }
    text_height += 2;

    x = (calib_area->display_width - text_width) / 2;
    y = (calib_area->display_height - text_height) / 2 - 60;
    cairo_set_line_width(cr, 2);
    cairo_rectangle(cr, x - 10, y - (HELP_LINES*text_height) - 10,
            text_width + 20, (HELP_LINES*text_height) + 20);

    /* Print help lines */
    y -= 3;
    for (i = HELP_LINES-1; i != -1; i--)
    {
        cairo_text_extents(cr, help_text[i], &extent);
        cairo_move_to(cr, x + (text_width-extent.width)/2, y);
        cairo_show_text(cr, help_text[i]);
        y -= text_height;
    }
    cairo_stroke(cr);

    /* Draw the points */
    for (i = 0; i <= calib_area->calibrator->num_clicks; i++)
    {
        /* set color: already clicked or not */
        if (i < calib_area->calibrator->num_clicks)
            cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
        else
            cairo_set_source_rgb(cr, 0.8, 0.0, 0.0);

        cairo_set_line_width(cr, 1);
        cairo_move_to(cr, calib_area->X[i] - cross_lines, calib_area->Y[i]);
        cairo_rel_line_to(cr, cross_lines*2, 0);
        cairo_move_to(cr, calib_area->X[i], calib_area->Y[i] - cross_lines);
        cairo_rel_line_to(cr, 0, cross_lines*2);
        cairo_stroke(cr);

        cairo_arc(cr, calib_area->X[i], calib_area->Y[i], cross_circle, 0.0, 2.0 * M_PI);
        cairo_stroke(cr);
    }

    /* Draw the clock background */
    cairo_arc(cr, calib_area->display_width/2, calib_area->display_height/2, clock_radius/2, 0.0, 2.0 * M_PI);
    cairo_set_source_rgb(cr, 0.5, 0.5, 0.5);
    cairo_fill_preserve(cr);
    cairo_stroke(cr);

    cairo_set_line_width(cr, clock_line_width);
    cairo_arc(cr, calib_area->display_width/2, calib_area->display_height/2, (clock_radius - clock_line_width)/2,
         3/2.0*M_PI, (3/2.0*M_PI) + ((double)calib_area->time_elapsed/(double)max_time) * 2*M_PI);
    cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
    cairo_stroke(cr);


    /* Draw the message (if any) */
    if (calib_area->message != NULL)
    {
        /* Frame the message */
        cairo_set_font_size(cr, font_size);
        cairo_text_extents(cr, calib_area->message, &extent);
        text_width = extent.width;
        text_height = extent.height;

        x = (calib_area->display_width - text_width) / 2;
        y = (calib_area->display_height - text_height + clock_radius) / 2 + 60;
        cairo_set_line_width(cr, 2);
        cairo_rectangle(cr, x - 10, y - text_height - 10,
                text_width + 20, text_height + 25);

        /* Print the message */
        cairo_move_to(cr, x, y);
        cairo_show_text(cr, calib_area->message);
        cairo_stroke(cr);
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

    calib_area->time_elapsed += time_step;
    if (calib_area->time_elapsed > max_time || parent == NULL)
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
        rect.x = calib_area->display_width/2 - clock_radius - clock_line_width;
        rect.y = calib_area->display_height/2 - clock_radius - clock_line_width;
        rect.width = 2 * clock_radius + 1 + 2 * clock_line_width;
        rect.height = 2 * clock_radius + 1 + 2 * clock_line_width;
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
    calib_area->anim_id = g_timeout_add(time_step, (GSourceFunc)on_timer_signal, calib_area);

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
