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

#ifndef _gui_gtk_h
#define _gui_gtk_h

#include <gtk/gtk.h>

#include "calibrator.h"

struct CalibArea
{
    struct Calib* calibrator;
    double X[4], Y[4];
    int display_width, display_height;
    int time_elapsed;

    const char* message;

    GtkWidget *drawing_area;
};

struct CalibArea* CalibrationArea_      (struct Calib     *c);
void              set_display_size      (struct CalibArea *calib_area,
                                         int               width,
                                         int               height);
void              resize_display        (struct CalibArea *calib_area);
bool              on_expose_event       (GtkWidget        *widget,
                                         GdkEventExpose   *event,
                                         gpointer data);
void              draw                  (GtkWidget        *widget,
                                         cairo_t          *cr,
                                         gpointer          data);
void              redraw                (struct CalibArea *calib_area);
bool              on_timer_signal       (struct CalibArea *calib_area);
bool              on_button_press_event (GtkWidget        *widget,
                                         GdkEventButton   *event,
                                         gpointer          data);
void              draw_message          (struct CalibArea *calib_area,
                                         const char       *msg);
bool              on_key_press_event    (GtkWidget        *widget,
                                         GdkEventKey      *event,
                                         gpointer          data);
bool              run_gui               (struct Calib     *c,
                                         XYinfo           *new_axys,
                                         bool             *swap);

#endif /* _gui_gtk_h */
