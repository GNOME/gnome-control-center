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
 */

#include <math.h>
#include "cc-target-actor.h"

#define CROSS_LINES                    47
#define TARGET_DIMENSION               (CROSS_LINES * 2)
#define CROSS_CIRCLE                   7
#define CROSS_CIRCLE2                  27
#define TARGET_SHOW_ANIMATION_DURATION 500
#define TARGET_HIDE_ANIMATION_DURATION 200

struct _CcTargetActor
{
  ClutterActor parent_instance;

  gdouble pos_x;
  gdouble pos_y;
};

G_DEFINE_TYPE (CcTargetActor, cc_target_actor, CLUTTER_TYPE_ACTOR);

static ClutterTransition *
get_target_show_animation (ClutterActor *target, const gchar *property)
{
  ClutterTransition *transition;

  transition = clutter_property_transition_new (property);
  clutter_timeline_set_progress_mode (CLUTTER_TIMELINE (transition),
                                      CLUTTER_EASE_OUT_BACK);
  clutter_timeline_set_duration (CLUTTER_TIMELINE (transition),
                                 TARGET_SHOW_ANIMATION_DURATION);
  clutter_transition_set_animatable (transition,
                                     CLUTTER_ANIMATABLE (target));
  clutter_transition_set_from (transition, G_TYPE_FLOAT, 0.0);
  clutter_transition_set_to (transition, G_TYPE_FLOAT, 1.0);

  return transition;
}

static ClutterTransition *
get_target_hide_animation (ClutterActor *target, const gchar *property)
{
  ClutterTransition *transition;

  transition = get_target_show_animation (target, property);
  clutter_timeline_set_progress_mode (CLUTTER_TIMELINE (transition),
                                      CLUTTER_EASE_OUT);
  clutter_timeline_set_duration (CLUTTER_TIMELINE (transition),
                                 TARGET_HIDE_ANIMATION_DURATION);
  clutter_transition_set_from (transition, G_TYPE_FLOAT, 1.0);
  clutter_transition_set_to (transition, G_TYPE_FLOAT, 0.0);

  return transition;
}

static void
show_target (CcTargetActor *self)
{
  ClutterTransition *transition;

  transition = get_target_show_animation (CLUTTER_ACTOR (self), "scale-x");
  clutter_timeline_start (CLUTTER_TIMELINE (transition));

  transition = get_target_show_animation (CLUTTER_ACTOR (self), "scale-y");
  clutter_timeline_start (CLUTTER_TIMELINE (transition));
}

static void
on_target_animation_complete (ClutterTimeline *timeline,
                              CcTargetActor   *self)
{
  clutter_actor_show (CLUTTER_ACTOR (self));
  clutter_actor_set_position (CLUTTER_ACTOR (self),
                              self->pos_x,
                              self->pos_y);

  show_target (self);
}

static void
draw_target (ClutterCairoTexture *texture,
             cairo_t             *cr,
             gint                 width,
             gint                 height,
             gpointer             data)
{
  cairo_set_operator (cr, CAIRO_OPERATOR_CLEAR);
  cairo_paint (cr);

  cairo_set_operator (cr, CAIRO_OPERATOR_OVER);
  cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);

  cairo_set_line_width(cr, 1);
  cairo_move_to(cr, 0, height / 2.0);
  cairo_rel_line_to(cr, width, 0);
  cairo_move_to(cr, width / 2.0, 0);
  cairo_rel_line_to(cr, 0, height * 2);
  cairo_stroke(cr);

  cairo_set_line_width(cr, 2);
  cairo_arc(cr, width / 2.0, height / 2.0, CROSS_CIRCLE, 0.0, 2.0 * M_PI);
  cairo_stroke(cr);

  cairo_set_line_width(cr, 5);
  cairo_arc(cr, width / 2.0, height / 2.0, CROSS_CIRCLE2, 0.0, 2.0 * M_PI);
  cairo_stroke(cr);
}

static void
cc_target_actor_init (CcTargetActor *self)
{
  ClutterContent *content;
  ClutterPoint anchor;

  self->pos_x = .0;
  self->pos_y = .0;

  content = clutter_canvas_new ();
  clutter_actor_set_content (CLUTTER_ACTOR (self), content);
  g_object_unref (content);

  clutter_canvas_set_size (CLUTTER_CANVAS (content),
                           CROSS_LINES * 2,
                           CROSS_LINES * 2);
  g_signal_connect (CLUTTER_CANVAS (content),
                    "draw",
                    G_CALLBACK (draw_target),
                    self);
  clutter_content_invalidate (content);

  anchor.x = .5;
  anchor.y = .5;

  g_object_set (self, "pivot-point", &anchor, NULL);
}

static void
cc_target_actor_get_preferred_width (ClutterActor *actor,
                           gfloat        for_height,
                           gfloat       *min_width_p,
                           gfloat       *natural_width_p)
{
  *min_width_p = CROSS_LINES * 2;
  *natural_width_p = CROSS_LINES * 2;
}

static void
cc_target_actor_get_preferred_height (ClutterActor *actor,
                            gfloat        for_width,
                            gfloat       *min_height_p,
                            gfloat       *natural_height_p)
{
  *min_height_p = CROSS_LINES * 2;
  *natural_height_p = CROSS_LINES * 2;
}


static void
cc_target_actor_class_init (CcTargetActorClass *klass)
{
  ClutterActorClass *clutter_actor_class = CLUTTER_ACTOR_CLASS (klass);

  clutter_actor_class->get_preferred_width = cc_target_actor_get_preferred_width;
  clutter_actor_class->get_preferred_height = cc_target_actor_get_preferred_height;
}


/* Move the _center_ of the target to be at (x,y) */
void
cc_target_actor_move_center (CcTargetActor *self, gdouble x, gdouble y)
{
  g_return_if_fail (CC_IS_TARGET_ACTOR (self));

  ClutterTransition *transition;
  gboolean target_visible;

  self->pos_x = x - (TARGET_DIMENSION / 2);
  self->pos_y = y - (TARGET_DIMENSION / 2);

  g_object_get (self, "visible", &target_visible, NULL);

  if (target_visible)
    {
      transition = get_target_hide_animation (CLUTTER_ACTOR (self), "scale-x");
      clutter_timeline_start (CLUTTER_TIMELINE (transition));
      transition = get_target_hide_animation (CLUTTER_ACTOR (self), "scale-y");
      clutter_timeline_start (CLUTTER_TIMELINE (transition));

      g_signal_connect (CLUTTER_TIMELINE (transition),
                        "completed",
                        G_CALLBACK (on_target_animation_complete),
                        self);
    }
  else
    {
      clutter_actor_show (CLUTTER_ACTOR (self));

      clutter_actor_set_position (CLUTTER_ACTOR (self), self->pos_x, self->pos_y);

      show_target (self);
    }
}

ClutterActor *
cc_target_actor_new (void)
{
  return g_object_new (CC_TARGET_ACTOR_TYPE, NULL);
}
