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
#include "cc-clock-actor.h"

#define CLOCK_RADIUS                   50
#define CLOCK_LINE_WIDTH               10
#define CLOCK_LINE_PADDING             10
#define CLOCK_SIZE_EXTRA
#define ANGLE                          "angle"
#define EXTRA_SPACE                    2

struct _CcClockActor
{
  ClutterActor parent_instance;

  gfloat angle;
};

G_DEFINE_TYPE (CcClockActor, cc_clock_actor, CLUTTER_TYPE_ACTOR);

enum {
  PROP_0,
  PROP_ANGLE,
  N_PROPERTIES
};

static GParamSpec *obj_properties[N_PROPERTIES] = { NULL, };

static void
draw_clock (ClutterCairoTexture *texture,
            cairo_t             *cr,
            gint                 width,
            gint                 height,
            CcClockActor        *self)
{
  cairo_set_operator (cr, CAIRO_OPERATOR_CLEAR);
  cairo_paint (cr);
  cairo_set_operator (cr, CAIRO_OPERATOR_OVER);

  /* Draw the clock background */
  cairo_arc (cr, width / 2, height / 2, CLOCK_RADIUS / 2, 0.0, 2.0 * M_PI);
  cairo_set_source_rgb (cr, 0.5, 0.5, 0.5);
  cairo_fill_preserve (cr);
  cairo_stroke (cr);

  cairo_set_line_width (cr, CLOCK_LINE_WIDTH);

  cairo_arc (cr,
             width / 2,
             height / 2,
             (CLOCK_RADIUS - CLOCK_LINE_WIDTH - CLOCK_LINE_PADDING) / 2,
             3 * M_PI_2,
             3 * M_PI_2 + self->angle * M_PI / 180.0);
  cairo_set_source_rgb (cr, 1.0, 1.0, 1.0);
  cairo_stroke (cr);
}

static void
cc_clock_actor_set_property (GObject      *object,
                             guint         property_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  CcClockActor *self = CC_CLOCK_ACTOR (object);
  ClutterContent *content;
  content = clutter_actor_get_content (CLUTTER_ACTOR (self));

  switch (property_id)
    {
    case PROP_ANGLE:
      self->angle = g_value_get_float (value);
      if (content)
        clutter_content_invalidate (content);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
cc_clock_actor_get_property (GObject      *object,
                             guint         property_id,
                             GValue       *value,
                             GParamSpec   *pspec)
{
  CcClockActor *self = CC_CLOCK_ACTOR (object);

  switch (property_id)
    {
    case PROP_ANGLE:
      g_value_set_float (value, self->angle);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
cc_clock_actor_init (CcClockActor *self)
{
  self->angle = 0;

  ClutterContent *content;
  content = clutter_canvas_new ();
  /* Extra space is needed because when drawing without it,
     it will miss 1 pixel in each of the edges */
  clutter_canvas_set_size (CLUTTER_CANVAS (content),
                           CLOCK_RADIUS + EXTRA_SPACE,
                           CLOCK_RADIUS + EXTRA_SPACE);
  clutter_actor_set_content (CLUTTER_ACTOR (self), content);
  g_signal_connect (CLUTTER_CANVAS (content),
                    "draw",
                    G_CALLBACK (draw_clock),
                    self);
  g_object_unref (content);
}

static void
cc_clock_actor_get_preferred_width (ClutterActor *actor,
                                    gfloat        for_height,
                                    gfloat       *min_width_p,
                                    gfloat       *natural_width_p)
{
  *min_width_p = CLOCK_RADIUS + EXTRA_SPACE;
  *natural_width_p = CLOCK_RADIUS + EXTRA_SPACE;
}

static void
cc_clock_actor_get_preferred_height (ClutterActor *actor,
                                     gfloat        for_width,
                                     gfloat       *min_height_p,
                                     gfloat       *natural_height_p)
{
  *min_height_p = CLOCK_RADIUS + EXTRA_SPACE;
  *natural_height_p = CLOCK_RADIUS + EXTRA_SPACE;
}


static void
cc_clock_actor_class_init (CcClockActorClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  ClutterActorClass *clutter_actor_class = CLUTTER_ACTOR_CLASS (klass);

  gobject_class->set_property = cc_clock_actor_set_property;
  gobject_class->get_property = cc_clock_actor_get_property;

  clutter_actor_class->get_preferred_width = cc_clock_actor_get_preferred_width;
  clutter_actor_class->get_preferred_height = cc_clock_actor_get_preferred_height;

  obj_properties[PROP_ANGLE] =
    g_param_spec_float (ANGLE,
                        "The angle of the clock's progress",
                        "Set the angle of the clock's progress",
                        .0,
                        360.0,
                        .0,
                        G_PARAM_READWRITE |
                        G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (gobject_class,
                                     N_PROPERTIES,
                                     obj_properties);
}

ClutterActor *
cc_clock_actor_new (void)
{
  return g_object_new (CC_CLOCK_ACTOR_TYPE, NULL);
}
