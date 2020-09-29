/*
 * Copyright © 2013 Red Hat, Inc.
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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Carlos Garnacho <carlosg@gnome.org>
 *         (based on previous work by Joaquim Rocha, Tias Guns and Soren Hauberg)
 */

#include "config.h"

#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <glib/gi18n.h>
#include <gdk/gdkx.h>
#include <gtk/gtk.h>

#include "calibrator.h"
#include "calibrator-gui.h"
#include "cc-clock.h"

struct CalibArea
{
  struct Calib calibrator;
  XYinfo       axis;
  gboolean     swap;
  gboolean     success;
  GdkDevice   *device;

  double X[4], Y[4];
  int display_width, display_height;

  GtkWidget  *window;
  GtkBuilder *builder;
  GtkWidget  *error_revealer;
  GtkWidget  *clock;
  GtkCssProvider *style_provider;

  FinishCallback callback;
  gpointer       user_data;
};

/* Timeout parameters */
#define MAX_TIME                15000 /* 15000 = 15 sec */
#define END_TIME                750   /*  750 = 0.75 sec */

static void
set_display_size (CalibArea *calib_area,
                  int        width,
                  int        height)
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
  reset (&calib_area->calibrator);
}

static void
calib_area_notify_finish (CalibArea *area)
{
  gtk_widget_hide (area->window);

  (*area->callback) (area, area->user_data);
}

static gboolean
on_delete_event (GtkWidget *widget,
                 GdkEvent  *event,
                 CalibArea *area)
{
  calib_area_notify_finish (area);
  return TRUE;
}

static gboolean
calib_area_finish_idle_cb (CalibArea *area)
{
  calib_area_notify_finish (area);
  return FALSE;
}

static void
set_success (CalibArea *area)
{
  GtkWidget *stack;

  stack = GTK_WIDGET (gtk_builder_get_object (area->builder, "stack"));
  gtk_stack_set_visible_child_name (GTK_STACK (stack), "page1");
}

static void
set_calibration_status (CalibArea *area)
{
  area->success = finish (&area->calibrator, &area->axis, &area->swap);

  if (area->success)
    {
      set_success (area);
      g_timeout_add (END_TIME,
                     (GSourceFunc) calib_area_finish_idle_cb,
                     area);
    }
  else
    {
      g_idle_add ((GSourceFunc) calib_area_finish_idle_cb, area);
    }
}

static void
show_error_message (CalibArea *area)
{
  gtk_revealer_set_reveal_child (GTK_REVEALER (area->error_revealer), TRUE);
}

static void
hide_error_message (CalibArea *area)
{
  gtk_revealer_set_reveal_child (GTK_REVEALER (area->error_revealer), FALSE);
}

static void
set_active_target (CalibArea *area,
                   int        n_target)
{
  GtkWidget *targets[] = {
    GTK_WIDGET (gtk_builder_get_object (area->builder, "target1")),
    GTK_WIDGET (gtk_builder_get_object (area->builder, "target2")),
    GTK_WIDGET (gtk_builder_get_object (area->builder, "target3")),
    GTK_WIDGET (gtk_builder_get_object (area->builder, "target4")),
  };
  int i;

  for (i = 0; i < G_N_ELEMENTS (targets); i++)
    gtk_widget_set_sensitive (targets[i], i == n_target);
}

static void
on_gesture_press (GtkGestureMultiPress *gesture,
                  guint                 n_press,
                  gdouble               x,
                  gdouble               y,
                  CalibArea            *area)
{
  gint num_clicks;
  gboolean success;
  GdkDevice *source;
  GdkEvent *event;

  if (area->success)
    return;

  event = gtk_get_current_event ();
  source = gdk_event_get_source_device ((GdkEvent *) event);
  gdk_event_free (event);

  /* Check matching device if a device was provided */
  if (area->device && area->device != source)
    {
      g_debug ("Ignoring input from device %s",
	       gdk_device_get_name (source));
      return;
    }

  /* Handle click */
  /* FIXME: reset clock */
  success = add_click(&area->calibrator,
                      (int) x,
                      (int) y);

  num_clicks = area->calibrator.num_clicks;

  if (!success && num_clicks == 0)
    show_error_message (area);
  else
    hide_error_message (area);

  /* Are we done yet? */
  if (num_clicks >= 4)
    {
      set_calibration_status (area);
      return;
    }

  set_active_target (area, num_clicks);
}

static gboolean
on_key_release_event (GtkWidget   *widget,
                      GdkEventKey *event,
                      CalibArea   *area)
{
  if (area->success ||
      event->keyval != GDK_KEY_Escape)
    return GDK_EVENT_PROPAGATE;

  calib_area_notify_finish (area);
  return GDK_EVENT_STOP;
}

static gboolean
on_focus_out_event (GtkWidget *widget,
                    GdkEvent  *event,
                    CalibArea *area)
{
  if (area->success)
    return FALSE;

  /* If the calibrator window loses focus, simply bail out... */
  calib_area_notify_finish (area);

  return FALSE;
}

static void
on_clock_finished (CcClock   *clock,
                   CalibArea *area)
{
  set_calibration_status (area);
}

static void
on_title_revealed (CalibArea *area)
{
  GtkWidget *revealer;

  revealer = GTK_WIDGET (gtk_builder_get_object (area->builder, "subtitle_revealer"));
  gtk_revealer_set_reveal_child (GTK_REVEALER (revealer), TRUE);
}

static gboolean
on_fullscreen (GtkWindow           *window,
               GdkEventWindowState *event,
               CalibArea           *area)
{
  GtkWidget *revealer;

  if ((event->changed_mask & GDK_WINDOW_STATE_FULLSCREEN) == 0)
    return FALSE;

  revealer = GTK_WIDGET (gtk_builder_get_object (area->builder, "title_revealer"));
  g_signal_connect_swapped (revealer, "notify::child-revealed",
                            G_CALLBACK (on_title_revealed),
                            area);
  gtk_revealer_set_reveal_child (GTK_REVEALER (revealer), TRUE);

  set_active_target (area, 0);

  return FALSE;
}

static void
on_size_allocate (GtkWidget     *widget,
                  GtkAllocation *allocation,
                  CalibArea     *area)
{
  set_display_size (area, allocation->width, allocation->height);
}

/**
 * Creates the windows and other objects required to do calibration
 * under GTK. When the window is closed (timed out, calibration finished
 * or user cancellation), callback will be called, where you should call
 * calib_area_finish().
 */
CalibArea *
calib_area_new (GdkScreen      *screen,
                int             n_monitor,
                GdkDevice      *device,
                FinishCallback  callback,
                gpointer        user_data,
                int             threshold_doubleclick,
                int             threshold_misclick)
{
  CalibArea *calib_area;
  GdkRectangle rect;
  GdkVisual *visual;
  GdkMonitor *monitor;
#ifndef FAKE_AREA
  GdkWindow *window;
  g_autoptr(GdkCursor) cursor = NULL;
#endif /* FAKE_AREA */
  GtkGesture *press;

  g_return_val_if_fail (callback, NULL);

  g_type_ensure (CC_TYPE_CLOCK);

  calib_area = g_new0 (CalibArea, 1);
  calib_area->callback = callback;
  calib_area->user_data = user_data;
  calib_area->device = device;
  calib_area->calibrator.threshold_doubleclick = threshold_doubleclick;
  calib_area->calibrator.threshold_misclick = threshold_misclick;

  calib_area->builder = gtk_builder_new_from_resource ("/org/gnome/control-center/wacom/calibrator/calibrator.ui");
  calib_area->window = GTK_WIDGET (gtk_builder_get_object (calib_area->builder, "window"));
  calib_area->error_revealer = GTK_WIDGET (gtk_builder_get_object (calib_area->builder, "error_revealer"));
  calib_area->clock = GTK_WIDGET (gtk_builder_get_object (calib_area->builder, "clock"));
  calib_area->style_provider = gtk_css_provider_new ();
  gtk_css_provider_load_from_resource (calib_area->style_provider, "/org/gnome/control-center/wacom/calibrator/calibrator.css");
  gtk_style_context_add_provider_for_screen (gtk_widget_get_screen (calib_area->window),
                                             GTK_STYLE_PROVIDER (calib_area->style_provider),
                                             GTK_STYLE_PROVIDER_PRIORITY_USER);

  cc_clock_set_duration (CC_CLOCK (calib_area->clock), MAX_TIME);
  g_signal_connect (calib_area->clock, "finished",
                    G_CALLBACK (on_clock_finished), calib_area);

#ifndef FAKE_AREA
  /* No cursor */
  gtk_widget_realize (calib_area->window);
  window = gtk_widget_get_window (calib_area->window);
  cursor = gdk_cursor_new_for_display (gdk_display_get_default (), GDK_BLANK_CURSOR);
  gdk_window_set_cursor (window, cursor);

  gtk_widget_set_can_focus (calib_area->window, TRUE);
  gtk_window_set_keep_above (GTK_WINDOW (calib_area->window), TRUE);
#endif /* FAKE_AREA */

  /* Move to correct screen */
  if (screen == NULL)
    screen = gdk_screen_get_default ();
  monitor = gdk_display_get_monitor (gdk_screen_get_display (screen), n_monitor);
  gdk_monitor_get_geometry (monitor, &rect);

  calib_area->calibrator.geometry = rect;

  g_signal_connect (calib_area->window,
                    "key-release-event",
                    G_CALLBACK (on_key_release_event),
                    calib_area);
  g_signal_connect (calib_area->window,
                    "delete-event",
                    G_CALLBACK (on_delete_event),
                    calib_area);
  g_signal_connect (calib_area->window,
                    "focus-out-event",
                    G_CALLBACK(on_focus_out_event),
                    calib_area);
  g_signal_connect (calib_area->window,
                    "window-state-event",
                    G_CALLBACK (on_fullscreen),
                    calib_area);
  g_signal_connect (calib_area->window,
                    "size-allocate",
                    G_CALLBACK (on_size_allocate),
                    calib_area);

  press = gtk_gesture_multi_press_new (calib_area->window);
  gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (press), GDK_BUTTON_PRIMARY);
  g_signal_connect (press, "pressed",
                    G_CALLBACK (on_gesture_press), calib_area);

  gtk_window_fullscreen_on_monitor (GTK_WINDOW (calib_area->window), screen, n_monitor);

  visual = gdk_screen_get_rgba_visual (screen);
  if (visual != NULL)
    gtk_widget_set_visual (GTK_WIDGET (calib_area->window), visual);

  gtk_widget_show (calib_area->window);

  return calib_area;
}

/* Finishes the calibration. Note that CalibArea
 * needs to be destroyed with calib_area_free() afterwards */
gboolean
calib_area_finish (CalibArea *area)
{
  g_return_val_if_fail (area != NULL, FALSE);

  if (area->success)
    g_debug ("Final calibration: %f, %f, %f, %f\n",
             area->axis.x_min,
             area->axis.y_min,
             area->axis.x_max,
             area->axis.y_max);
  else
    g_debug ("Calibration was aborted or timed out");

  return area->success;
}

void
calib_area_free (CalibArea *area)
{
  g_return_if_fail (area != NULL);

  gtk_style_context_remove_provider_for_screen (gtk_widget_get_screen (area->window),
                                                GTK_STYLE_PROVIDER (area->style_provider));
  gtk_widget_destroy (area->window);
  g_free (area);
}

void
calib_area_get_display_size (CalibArea *area, gint *width, gint *height)
{
  g_return_if_fail (area != NULL);

  *width = area->display_width;
  *height = area->display_height;
}

void
calib_area_get_axis (CalibArea *area,
                     XYinfo    *new_axis,
                     gboolean  *swap_xy)
{
  g_return_if_fail (area != NULL);

  *new_axis = area->axis;
  *swap_xy  = area->swap;
}

void
calib_area_get_padding (CalibArea *area,
                        XYinfo    *padding)
{
  g_return_if_fail (area != NULL);

  /* min/max values are monitor coordinates scaled to be between
   * 0 and 1, padding starts at 0 on "the edge", and positive
   * values grow towards the center of the rectangle.
   */
  padding->x_min = area->axis.x_min;
  padding->y_min = area->axis.y_min;
  padding->x_max = 1 - area->axis.x_max;
  padding->y_max = 1 - area->axis.y_max;
}
