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
 * Author: Joaquim Rocha <jrocha@redhat.com>
 *         (based on previous work by Tias Guns and Soren Hauberg)
 */

#include "config.h"

#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <glib/gi18n.h>
#include <gdk/gdkx.h>
#include <gtk/gtk.h>
#include <cairo.h>
#include <clutter-gtk/clutter-gtk.h>
#include <clutter/clutter.h>

#include "calibrator.h"
#include "calibrator-gui.h"
#include "cc-clock-actor.h"
#include "cc-target-actor.h"

struct CalibArea
{
  struct Calib calibrator;
  XYinfo       axis;
  gboolean     swap;
  gboolean     success;
  GdkDevice   *device;

  double X[4], Y[4];
  int display_width, display_height;

  GtkWidget         *window;
  ClutterActor      *stage;
  ClutterActor      *action_layer;
  ClutterActor      *clock;
  ClutterActor      *target;
  ClutterActor      *success_image;
  ClutterActor      *text_title_holder;
  ClutterActor      *helper_text_title;
  ClutterActor      *text_body_holder;
  ClutterActor      *helper_text_body;
  ClutterActor      *error_text;
  ClutterTransition *clock_timeline;
  ClutterTransition *error_msg_timeline;
  ClutterTransition *helper_msg_timeline;
  GdkPixbuf         *icon_success;

  FinishCallback callback;
  gpointer       user_data;
};

#define TARGET_SHOW_ANIMATION_DURATION 500
#define TARGET_HIDE_ANIMATION_DURATION 200

#define COLOR_GRAY 127

/* Window parameters */
#define WINDOW_OPACITY          0.9

/* Timeout parameters */
#define MAX_TIME                15000 /* 15000 = 15 sec */
#define END_TIME                750   /*  750 = 0.75 sec */

/* Text printed on screen */
#define HELP_TEXT_TITLE            N_("Screen Calibration")
#define HELP_TEXT_MAIN             N_("Please tap the target markers as they " \
                                      "appear on screen to calibrate the tablet.")
#define HELP_TEXT_ANIMATION_DURATION 300

#define ERROR_MESSAGE                    N_("Mis-click detected, restarting…")
#define ERROR_MESSAGE_ANIMATION_DURATION 500

#define ICON_SUCCESS    "emblem-ok-symbolic"
#define ICON_SIZE       300

static void
set_display_size(CalibArea *calib_area,
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
  reset(&calib_area->calibrator);
}

static void
resize_display(CalibArea *calib_area)
{
  gfloat width, height;

  clutter_actor_get_size (calib_area->stage, &width, &height);
  if (calib_area->display_width != width ||
      calib_area->display_height != height)
    {
      gint i = calib_area->calibrator.num_clicks;
      set_display_size(calib_area, width, height);
      cc_target_actor_move_center (CC_TARGET_ACTOR (calib_area->target),
                                   calib_area->X[i],
                                   calib_area->Y[i]);
    }
}

static void
on_allocation_changed (ClutterActor          *actor,
                       ClutterActorBox       *box,
                       ClutterAllocationFlags flags,
                       CalibArea             *area)
{
  if (!gtk_widget_is_visible (area->window))
    return;

  resize_display (area);
}

static void
calib_area_notify_finish (CalibArea *area)
{
  clutter_timeline_stop (CLUTTER_TIMELINE (area->clock_timeline));

  if (area->error_msg_timeline)
    clutter_timeline_stop (CLUTTER_TIMELINE (area->error_msg_timeline));
  if (area->helper_msg_timeline)
    clutter_timeline_stop (CLUTTER_TIMELINE (area->helper_msg_timeline));

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
  ClutterImage *image;
  GdkPixbuf *icon = area->icon_success;

  if (icon == NULL)
    return;

  image = CLUTTER_IMAGE (clutter_actor_get_content (area->success_image));
  clutter_image_set_data (image,
                          gdk_pixbuf_get_pixels (icon),
                          gdk_pixbuf_get_has_alpha (icon)
                          ? COGL_PIXEL_FORMAT_RGBA_8888
                          : COGL_PIXEL_FORMAT_RGB_888,
                          gdk_pixbuf_get_width (icon),
                          gdk_pixbuf_get_height (icon),
                          gdk_pixbuf_get_rowstride (icon),
                          NULL);
  clutter_actor_set_size (area->success_image,
                          gdk_pixbuf_get_width (icon),
                          gdk_pixbuf_get_height (icon));

  clutter_actor_show (area->success_image);
  clutter_actor_hide (area->action_layer);
}

static void
set_calibration_status (CalibArea *area)
{
  GtkIconTheme *icon_theme;
  GtkIconInfo  *icon_info;
  GdkRGBA       white;

  icon_theme = gtk_icon_theme_get_default ();
  icon_info = gtk_icon_theme_lookup_icon (icon_theme,
                                          ICON_SUCCESS,
                                          ICON_SIZE,
                                          GTK_ICON_LOOKUP_USE_BUILTIN);
  if (icon_info == NULL)
    {
      g_warning ("Failed to find icon \"%s\"", ICON_SUCCESS);
      goto out;
    }

  gdk_rgba_parse (&white, "White");
  area->icon_success = gtk_icon_info_load_symbolic (icon_info,
                                                    &white,
                                                    NULL,
                                                    NULL,
                                                    NULL,
                                                    NULL,
                                                    NULL);
  g_object_unref (icon_info);

  if (!area->icon_success)
    g_warning ("Failed to load icon \"%s\"", ICON_SUCCESS);

 out:
  area->success = finish (&area->calibrator, &area->axis, &area->swap);
  if (area->success && area->icon_success)
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

static ClutterTransition *
get_error_message_transition (CalibArea *area)
{
  ClutterTransition *transition;

  clutter_actor_show (area->error_text);
  transition = clutter_property_transition_new ("opacity");
  clutter_timeline_set_progress_mode (CLUTTER_TIMELINE (transition),
                                      CLUTTER_EASE_OUT);
  clutter_timeline_set_duration (CLUTTER_TIMELINE (transition),
                                 ERROR_MESSAGE_ANIMATION_DURATION);
  clutter_transition_set_animatable (transition,
                                     CLUTTER_ANIMATABLE (area->error_text));
  clutter_transition_set_from (transition, G_TYPE_UINT, 0);
  clutter_transition_set_to (transition, G_TYPE_UINT, 255);

  return transition;
}

static void
show_error_message (CalibArea *area)
{
  ClutterTransition *transition;
  clutter_actor_show (area->error_text);
  transition = get_error_message_transition (area);
  clutter_timeline_start (CLUTTER_TIMELINE (transition));

  g_clear_object (&area->error_msg_timeline);
  area->error_msg_timeline = transition;
}

static void
on_error_message_transparent (ClutterTimeline *timeline,
                              CalibArea       *area)
{
  clutter_actor_hide (area->error_text);
}

static void
hide_error_message (CalibArea *area)
{
  ClutterTransition *transition;
  transition = get_error_message_transition (area);
  clutter_transition_set_from (transition, G_TYPE_UINT, 255);
  clutter_transition_set_to (transition, G_TYPE_UINT, 0);
  g_signal_connect (CLUTTER_TIMELINE (transition),
                    "completed",
                    G_CALLBACK (on_error_message_transparent),
                    area);
  clutter_timeline_start (CLUTTER_TIMELINE (transition));

  g_clear_object (&area->error_msg_timeline);
  area->error_msg_timeline = transition;
}

static gboolean
on_button_press_event (GtkWidget      *widget,
                       GdkEventButton *event,
                       CalibArea      *area)
{
  gint num_clicks;
  gboolean success;
  GdkDevice *source;

  if (area->success)
    return FALSE;

  if (event->type != GDK_BUTTON_PRESS ||
      event->button != GDK_BUTTON_PRIMARY)
    return FALSE;

  source = gdk_event_get_source_device ((GdkEvent *) event);

  /* Check matching device if a device was provided */
  if (area->device && area->device != source)
    {
      g_debug ("Ignoring input from device %s",
	       gdk_device_get_name (source));
      return FALSE;
    }

  /* Handle click */
  clutter_timeline_stop (CLUTTER_TIMELINE (area->clock_timeline));
  clutter_timeline_start (CLUTTER_TIMELINE (area->clock_timeline));
  success = add_click(&area->calibrator,
                      (int) event->x,
                      (int) event->y);

  num_clicks = area->calibrator.num_clicks;

  if (!success && num_clicks == 0)
    show_error_message (area);
  else
    {
      gboolean visible;
      g_object_get (area->error_text, "visible", &visible, NULL);

      if (visible)
        hide_error_message (area);
    }

  /* Are we done yet? */
  if (num_clicks >= 4)
    {
      set_calibration_status (area);
      return FALSE;
    }

  cc_target_actor_move_center (CC_TARGET_ACTOR (area->target),
                               area->X[num_clicks],
                               area->Y[num_clicks]);

  return FALSE;
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
on_timeout (ClutterTimeline *timeline,
            CalibArea       *area)
{
  set_calibration_status (area);
}

static void
show_helper_text_body (CalibArea *area)
{
  ClutterTransition *transition;
  gfloat height;

  height = clutter_actor_get_height (area->helper_text_body);
  clutter_actor_show (area->helper_text_body);

  transition = clutter_property_transition_new ("y");
  clutter_timeline_set_progress_mode (CLUTTER_TIMELINE (transition),
                                      CLUTTER_EASE_OUT);
  clutter_timeline_set_duration (CLUTTER_TIMELINE (transition),
                                 HELP_TEXT_ANIMATION_DURATION);
  clutter_transition_set_animatable (transition,
                                     CLUTTER_ANIMATABLE (area->helper_text_body));
  clutter_transition_set_from (transition, G_TYPE_FLOAT, -height);
  clutter_transition_set_to (transition, G_TYPE_FLOAT, 0.0);
  clutter_timeline_start (CLUTTER_TIMELINE (transition));

  g_clear_object (&area->helper_msg_timeline);
  area->helper_msg_timeline = transition;
}

static void
on_helper_text_title_shown (ClutterTimeline *timelines,
                            CalibArea       *area)
{
  show_helper_text_body (area);
}

static void
show_helper_text_title (CalibArea *area)
{
  ClutterTransition *transition;

  gfloat height = clutter_actor_get_height (area->helper_text_title);
  clutter_actor_set_y (area->helper_text_title,
                       - clutter_actor_get_height (area->helper_text_title));
  clutter_actor_show (area->helper_text_title);

  transition = clutter_property_transition_new ("y");
  clutter_timeline_set_progress_mode (CLUTTER_TIMELINE (transition),
                                      CLUTTER_EASE_OUT);
  clutter_timeline_set_duration (CLUTTER_TIMELINE (transition),
                                 HELP_TEXT_ANIMATION_DURATION);
  clutter_transition_set_animatable (transition,
                                     CLUTTER_ANIMATABLE (area->helper_text_title));
  clutter_transition_set_from (transition, G_TYPE_FLOAT, -height);
  clutter_transition_set_to (transition, G_TYPE_FLOAT, 0.0);

  g_signal_connect (CLUTTER_TIMELINE (transition),
                    "completed",
                    G_CALLBACK (on_helper_text_title_shown),
                    area);

  clutter_timeline_start (CLUTTER_TIMELINE (transition));

  g_clear_object (&area->helper_msg_timeline);
  area->helper_msg_timeline = transition;
}

static gboolean
on_fullscreen (GtkWindow           *window,
               GdkEventWindowState *event,
               CalibArea           *area)
{
  ClutterRect rect;

  if ((event->changed_mask & GDK_WINDOW_STATE_FULLSCREEN) == 0)
    return FALSE;

  /* Protect against window state multiple changes*/
  if (clutter_actor_is_visible (area->action_layer))
    return FALSE;

  clutter_actor_show (area->action_layer);
  clutter_actor_show (area->clock);

  rect.origin.x = 0;
  rect.origin.y = 0;
  clutter_actor_get_size (area->helper_text_title,
                          &rect.size.width,
                          &rect.size.height);
  g_object_set (area->text_title_holder, "clip-rect", &rect, NULL);

  clutter_actor_get_size (area->helper_text_body,
                          &rect.size.width,
                          &rect.size.height);
  g_object_set (area->text_body_holder, "clip-rect", &rect, NULL);
  clutter_actor_set_y (area->helper_text_body,
                       - clutter_actor_get_height (area->helper_text_body));

  show_helper_text_title (area);
  return FALSE;
}

static void
set_up_stage (CalibArea *calib_area, ClutterActor *stage)
{
  ClutterPoint anchor;
  ClutterColor color;
  ClutterContent *success_content;
  gfloat height;
  gchar *markup;

  calib_area->stage = stage;
  calib_area->action_layer = clutter_actor_new ();
  calib_area->clock = cc_clock_actor_new ();
  calib_area->target = cc_target_actor_new ();
  calib_area->text_title_holder = clutter_actor_new ();
  calib_area->helper_text_title = clutter_text_new ();
  calib_area->text_body_holder = clutter_actor_new ();
  calib_area->helper_text_body = clutter_text_new ();
  calib_area->error_text = clutter_text_new ();
  calib_area->success_image = clutter_actor_new ();

  clutter_stage_set_use_alpha (CLUTTER_STAGE (stage), TRUE);

  clutter_actor_hide (calib_area->target);

  /* bind the action layer's geometry to the stage's */
  clutter_actor_add_constraint (calib_area->action_layer,
                                clutter_bind_constraint_new (stage,
                                                             CLUTTER_BIND_SIZE,
                                                             0));
  clutter_actor_add_child (stage, calib_area->action_layer);

  g_signal_connect (stage,
                    "allocation-changed",
                    G_CALLBACK (on_allocation_changed),
                    calib_area);

  clutter_color_from_string (&color, "#000");
  color.alpha = WINDOW_OPACITY * 255;
  clutter_actor_set_background_color (stage, &color);

  clutter_actor_add_child (calib_area->action_layer, calib_area->clock);
  clutter_actor_add_constraint (calib_area->clock,
                                clutter_align_constraint_new (stage,
                                                              CLUTTER_ALIGN_BOTH,
                                                              0.5));

  clutter_actor_add_child (calib_area->action_layer, calib_area->target);

  /* set the helper text */
  anchor.x =  0;
  g_object_set (calib_area->text_title_holder, "pivot-point", &anchor, NULL);

  clutter_actor_add_child (calib_area->action_layer,
                           calib_area->text_title_holder);
  clutter_actor_add_child (calib_area->text_title_holder,
                           calib_area->helper_text_title);
  height = clutter_actor_get_height (calib_area->clock);
  clutter_actor_add_constraint (calib_area->text_title_holder,
                                clutter_bind_constraint_new (calib_area->clock,
                                                             CLUTTER_BIND_Y,
                                                             height * 1.5));
  clutter_actor_add_constraint (calib_area->text_title_holder,
                                clutter_align_constraint_new (stage,
                                                              CLUTTER_ALIGN_X_AXIS,
                                                              .5));

  clutter_text_set_line_alignment (CLUTTER_TEXT (calib_area->helper_text_title),
                                   PANGO_ALIGN_CENTER);

  color.red = COLOR_GRAY;
  color.green = COLOR_GRAY;
  color.blue = COLOR_GRAY;
  color.alpha = 255;

  markup = g_strdup_printf ("<big><b>%s</b></big>",
                            _(HELP_TEXT_TITLE));
  clutter_text_set_markup (CLUTTER_TEXT (calib_area->helper_text_title), markup);
  clutter_text_set_color (CLUTTER_TEXT (calib_area->helper_text_title), &color);
  g_free (markup);

  g_object_set (calib_area->text_body_holder, "pivot-point", &anchor, NULL);

  clutter_actor_add_child (calib_area->action_layer,
                           calib_area->text_body_holder);
  clutter_actor_add_child (calib_area->text_body_holder,
                           calib_area->helper_text_body);
  height = clutter_actor_get_height (calib_area->helper_text_title);
  clutter_actor_add_constraint (calib_area->text_body_holder,
                                clutter_bind_constraint_new (calib_area->text_title_holder,
                                                             CLUTTER_BIND_Y,
                                                             height * 1.2));
  clutter_actor_add_constraint (calib_area->text_body_holder,
                                clutter_align_constraint_new (stage,
                                                              CLUTTER_ALIGN_X_AXIS,
                                                              .5));

  clutter_text_set_line_alignment (CLUTTER_TEXT (calib_area->helper_text_body),
                                   PANGO_ALIGN_CENTER);
  markup = g_strdup_printf ("<span foreground=\"white\"><big>%s</big></span>",
                            _(HELP_TEXT_MAIN));
  clutter_text_set_markup (CLUTTER_TEXT (calib_area->helper_text_body), markup);
  g_free (markup);

  /* set the error text */
  g_object_set (calib_area->error_text, "pivot-point", &anchor, NULL);

  clutter_actor_add_child (calib_area->action_layer, calib_area->error_text);
  height = clutter_actor_get_height (calib_area->helper_text_body);
  clutter_actor_add_constraint (calib_area->error_text,
                                clutter_bind_constraint_new (calib_area->text_title_holder,
                                                             CLUTTER_BIND_Y,
                                                             height * 3));
  clutter_actor_add_constraint (calib_area->error_text,
                                clutter_align_constraint_new (stage,
                                                              CLUTTER_ALIGN_X_AXIS,
                                                              .5));

  clutter_text_set_line_alignment (CLUTTER_TEXT (calib_area->error_text),
                                   PANGO_ALIGN_CENTER);
  markup = g_strdup_printf ("<span foreground=\"white\"><big>"
                            "<b>%s</b></big></span>",
                            ERROR_MESSAGE);
  clutter_text_set_markup (CLUTTER_TEXT (calib_area->error_text), markup);
  g_free (markup);

  clutter_actor_hide (calib_area->error_text);

  /* configure success image */
  success_content = clutter_image_new ();
  clutter_actor_set_content (calib_area->success_image,
                             success_content);
  g_object_unref (success_content);
  clutter_actor_add_child (stage, calib_area->success_image);
  clutter_actor_add_constraint (calib_area->success_image,
                                clutter_align_constraint_new (stage,
                                                              CLUTTER_ALIGN_BOTH,
                                                              .5));

  /* animate clock */
  calib_area->clock_timeline = clutter_property_transition_new ("angle");
  clutter_timeline_set_progress_mode (CLUTTER_TIMELINE (calib_area->clock_timeline),
                                      CLUTTER_LINEAR);
  clutter_timeline_set_duration (CLUTTER_TIMELINE (calib_area->clock_timeline),
                                 MAX_TIME);
  clutter_transition_set_animatable (calib_area->clock_timeline,
                                     CLUTTER_ANIMATABLE (calib_area->clock));
  clutter_transition_set_from (calib_area->clock_timeline, G_TYPE_FLOAT, .0);
  clutter_transition_set_to (calib_area->clock_timeline, G_TYPE_FLOAT, 360.0);
  clutter_timeline_set_repeat_count (CLUTTER_TIMELINE (calib_area->clock_timeline),
                                     -1);
  clutter_timeline_start (CLUTTER_TIMELINE (calib_area->clock_timeline));
  g_signal_connect (CLUTTER_TIMELINE (calib_area->clock_timeline),
                    "completed",
                    G_CALLBACK (on_timeout),
                    calib_area);
}

/**
 * Creates the windows and other objects required to do calibration
 * under GTK. When the window is closed (timed out, calibration finished
 * or user cancellation), callback will be called, where you should call
 * calib_area_finish().
 */
CalibArea *
calib_area_new (GdkScreen      *screen,
                int             monitor,
                GdkDevice      *device,
                FinishCallback  callback,
                gpointer        user_data,
                XYinfo         *old_axis,
                int             threshold_doubleclick,
                int             threshold_misclick)
{
  CalibArea *calib_area;
  GdkRectangle rect;
  GdkVisual *visual;
#ifndef FAKE_AREA
  GdkWindow *window;
  GdkCursor *cursor;
#endif /* FAKE_AREA */
  GtkWidget *clutter_embed;
  ClutterActor *stage;

  g_return_val_if_fail (old_axis, NULL);
  g_return_val_if_fail (callback, NULL);

  g_debug ("Current calibration: %f, %f, %f, %f\n",
           old_axis->x_min,
           old_axis->y_min,
           old_axis->x_max,
           old_axis->y_max);

  calib_area = g_new0 (CalibArea, 1);
  calib_area->callback = callback;
  calib_area->user_data = user_data;
  calib_area->device = device;
  calib_area->calibrator.old_axis.x_min = old_axis->x_min;
  calib_area->calibrator.old_axis.x_max = old_axis->x_max;
  calib_area->calibrator.old_axis.y_min = old_axis->y_min;
  calib_area->calibrator.old_axis.y_max = old_axis->y_max;
  calib_area->calibrator.threshold_doubleclick = threshold_doubleclick;
  calib_area->calibrator.threshold_misclick = threshold_misclick;

  calib_area->window = gtk_window_new (GTK_WINDOW_TOPLEVEL);

#ifndef FAKE_AREA
  /* No cursor */
  gtk_widget_realize (calib_area->window);
  window = gtk_widget_get_window (calib_area->window);
  cursor = gdk_cursor_new_for_display (gdk_display_get_default (), GDK_BLANK_CURSOR);
  gdk_window_set_cursor (window, cursor);
  g_object_unref (cursor);

  gtk_widget_set_can_focus (calib_area->window, TRUE);
  gtk_window_set_keep_above (GTK_WINDOW (calib_area->window), TRUE);
#endif /* FAKE_AREA */

  /* Set up the embedded stage */
  clutter_embed = gtk_clutter_embed_new ();
  gtk_container_add (GTK_CONTAINER (calib_area->window),
                     clutter_embed);

  stage = gtk_clutter_embed_get_stage (GTK_CLUTTER_EMBED (clutter_embed));

  /* Move to correct screen */
  if (screen == NULL)
    screen = gdk_screen_get_default ();
  gdk_screen_get_monitor_geometry (screen, monitor, &rect);

  calib_area->calibrator.geometry = rect;

  set_up_stage (calib_area, stage);

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
                    "button-press-event",
                    G_CALLBACK (on_button_press_event),
                    calib_area);

  gtk_window_fullscreen_on_monitor (GTK_WINDOW (calib_area->window), screen, monitor);

  visual = gdk_screen_get_rgba_visual (screen);
  if (visual != NULL)
    gtk_widget_set_visual (GTK_WIDGET (calib_area->window), visual);

  gtk_widget_show_all (calib_area->window);
  clutter_actor_hide (calib_area->action_layer);

  return calib_area;
}

/* Finishes the calibration. Note that CalibArea
 * needs to be destroyed with calib_area_free() afterwards */
gboolean
calib_area_finish (CalibArea *area,
                   XYinfo    *new_axis,
                   gboolean  *swap_xy)
{
  g_return_val_if_fail (area != NULL, FALSE);

  *new_axis = area->axis;
  *swap_xy  = area->swap;

  if (area->success)
    g_debug ("Final calibration: %f, %f, %f, %f\n",
             new_axis->x_min,
             new_axis->y_min,
             new_axis->x_max,
             new_axis->y_max);
  else
    g_debug ("Calibration was aborted or timed out");

  return area->success;
}

void
calib_area_free (CalibArea *area)
{
  g_return_if_fail (area != NULL);

  g_clear_object (&area->icon_success);
  g_clear_object (&area->clock_timeline);
  g_clear_object (&area->error_msg_timeline);
  g_clear_object (&area->helper_msg_timeline);
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
