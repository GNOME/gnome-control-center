/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2018 Canonical Ltd.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include "cc-level-bar.h"
#include "cc-sound-enums.h"
#include "gvc-mixer-stream-private.h"

struct _CcLevelBar
{
  GtkWidget             parent_instance;

  CcStreamType          type;
  pa_stream            *level_stream;
  gdouble               last_input_peak;

  gdouble               value;
};

G_DEFINE_TYPE (CcLevelBar, cc_level_bar, GTK_TYPE_WIDGET)

#define LED_WIDTH   12
#define LED_HEIGHT  3
#define LED_SPACING 4

#define DECAY_STEP .15

static void
set_peak (CcLevelBar *self,
          gdouble     value)
{
  if (value < 0)
     value = 0;
  if (value > 1)
     value = 1;

  if (self->last_input_peak >= DECAY_STEP &&
      value < self->last_input_peak - DECAY_STEP)
    value = self->last_input_peak - DECAY_STEP;
  self->last_input_peak = value;

  self->value = value;
  gtk_widget_queue_draw (GTK_WIDGET (self));
}

static void
read_cb (pa_stream *stream,
         size_t     length,
         void      *userdata)
{
  CcLevelBar *self = userdata;
  const void *data;
  gdouble value;

  if (pa_stream_peek (stream, &data, &length) < 0)
    {
      g_warning ("Failed to read data from stream");
      return;
    }

  if (!data)
    {
      pa_stream_drop (stream);
      return;
    }

  assert (length > 0);
  assert (length % sizeof (float) == 0);

  value = ((const float *) data)[length / sizeof (float) -1];

  pa_stream_drop (stream);

  set_peak (self, value);
}

static void
suspended_cb (pa_stream *stream,
              void      *userdata)
{
  CcLevelBar *self = userdata;

  if (pa_stream_is_suspended (stream))
    {
      g_debug ("Stream suspended");
      self->value = 0.0;
      gtk_widget_queue_draw (GTK_WIDGET (self));
    }
}

static void
cc_level_bar_get_preferred_height (GtkWidget *widget,
                                   gint      *minimum,
                                   gint      *natural)
{
  *minimum = *natural = LED_HEIGHT;
}

static void
set_source_blend (cairo_t *cr, GdkRGBA *a, GdkRGBA *b, gdouble f)
{
  cairo_set_source_rgb (cr,
                        (1.0 - f) * a->red   + f * b->red,
                        (1.0 - f) * a->green + f * b->green,
                        (1.0 - f) * a->blue  + f * b->blue);
}

static gboolean
cc_level_bar_draw (GtkWidget *widget,
                   cairo_t   *cr)
{
  CcLevelBar *self = CC_LEVEL_BAR (widget);
  GtkAllocation allocation;
  GdkRGBA inactive_color, active_color;
  int i, n_leds;
  double level;
  double spacing, x_offset = 0.0;

  gtk_widget_get_allocation (widget, &allocation);

  n_leds = allocation.width / (LED_WIDTH + LED_SPACING);
  spacing = (double) (allocation.width - (n_leds * LED_WIDTH)) / (n_leds - 1);
  level = self->value * n_leds;

  gdk_rgba_parse (&inactive_color, "#C0C0C0");
  switch (self->type)
  {
  default:
  case CC_STREAM_TYPE_OUTPUT:
    gdk_rgba_parse (&active_color, "#4a90d9");
    break;
  case CC_STREAM_TYPE_INPUT:
    gdk_rgba_parse (&active_color, "#ff0000");
    break;
  }

  for (i = 0; i < n_leds; i++)
  {
    double led_level;

    led_level = level - i;
    if (led_level < 0.0)
      led_level = 0.0;
    else if (led_level > 1.0)
      led_level = 1.0;

    cairo_rectangle (cr,
                     x_offset, 0,
                     LED_WIDTH, allocation.height);
    set_source_blend (cr, &inactive_color, &active_color, led_level);
    cairo_fill (cr);
    x_offset += LED_WIDTH + spacing;
  }

  return FALSE;
}

static void
close_stream (pa_stream *stream)
{
  if (stream == NULL)
    return;

  /* Stop receiving data */
  pa_stream_set_read_callback (stream, NULL, NULL);
  pa_stream_set_suspended_callback (stream, NULL, NULL);

  /* Disconnect from the stream */
  pa_stream_disconnect (stream);
}

static void
cc_level_bar_dispose (GObject *object)
{
  CcLevelBar *self = CC_LEVEL_BAR (object);

  close_stream (self->level_stream);
  g_clear_pointer (&self->level_stream, pa_stream_unref);

  G_OBJECT_CLASS (cc_level_bar_parent_class)->dispose (object);
}

void
cc_level_bar_class_init (CcLevelBarClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = cc_level_bar_dispose;

  widget_class->get_preferred_height = cc_level_bar_get_preferred_height;
  widget_class->draw = cc_level_bar_draw;
}

void
cc_level_bar_init (CcLevelBar *self)
{
  gtk_widget_set_has_window (GTK_WIDGET (self), FALSE);
}

void
cc_level_bar_set_stream (CcLevelBar     *self,
                         GvcMixerStream *stream,
                         CcStreamType    type)
{
  pa_context *context;
  pa_sample_spec sample_spec;
  pa_proplist *proplist;
  pa_buffer_attr  attr;
  g_autofree gchar *device = NULL;

  g_return_if_fail (CC_IS_LEVEL_BAR (self));

  close_stream (self->level_stream);
  g_clear_pointer (&self->level_stream, pa_stream_unref);

  self->type = type;

  if (stream == NULL)
   {
     gtk_widget_queue_draw (GTK_WIDGET (self));
     return;
   }

  context = gvc_mixer_stream_get_pa_context (stream);

  if (pa_context_get_server_protocol_version (context) < 13)
    {
      g_warning ("Unsupported version of PulseAudio");
      return;
    }

  sample_spec.channels = 1;
  sample_spec.format = PA_SAMPLE_FLOAT32;
  sample_spec.rate = 25;

  proplist = pa_proplist_new ();
  pa_proplist_sets (proplist, PA_PROP_APPLICATION_ID, "org.gnome.VolumeControl");
  self->level_stream = pa_stream_new_with_proplist (context, "Peak detect", &sample_spec, NULL, proplist);
  pa_proplist_free (proplist);
  if (self->level_stream == NULL)
    {
      g_warning ("Failed to create monitoring stream");
      return;
    }

  pa_stream_set_read_callback (self->level_stream, read_cb, self);
  pa_stream_set_suspended_callback (self->level_stream, suspended_cb, self);

  memset (&attr, 0, sizeof (attr));
  attr.fragsize = sizeof (float);
  attr.maxlength = (uint32_t) -1;
  device = g_strdup_printf ("%u", gvc_mixer_stream_get_index (stream));
  if (pa_stream_connect_record (self->level_stream,
                                device,
                                &attr,
                                (pa_stream_flags_t) (PA_STREAM_DONT_MOVE |
                                                     PA_STREAM_PEAK_DETECT |
                                                     PA_STREAM_ADJUST_LATENCY)) < 0)
    {
      g_warning ("Failed to connect monitoring stream");
    }

  gtk_widget_queue_draw (GTK_WIDGET (self));
}
