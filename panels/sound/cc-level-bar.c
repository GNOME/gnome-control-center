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
#include "gvc-level-bar.h"
#include "gvc-mixer-stream-private.h"

struct _CcLevelBar {
  GtkBox           parent_instance;

  GtkWidget       *legacy_level_bar;
  GtkAdjustment   *peak_adjustment;

  GvcMixerStream  *stream;
  pa_stream       *level_stream;
  gdouble          last_input_peak;
};

G_DEFINE_TYPE (CcLevelBar, cc_level_bar, GTK_TYPE_BOX)

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

  gtk_adjustment_set_value (self->peak_adjustment, value);
}

static void
read_cb (pa_stream *stream,
         size_t     length,
         void      *userdata)
{
  CcLevelBar *self = userdata;
  const void *data;
  gdouble     value;

  if (pa_stream_peek (stream, &data, &length) < 0) {
    g_warning ("Failed to read data from stream");
    return;
  }

  if (!data) {
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

  if (pa_stream_is_suspended (stream)) {
    g_debug ("Stream suspended");
    gtk_adjustment_set_value (self->peak_adjustment, 0);
  }
}

static void
cc_level_bar_dispose (GObject *object)
{
  CcLevelBar *self = CC_LEVEL_BAR (object);

  // FIXME: Disconnect callbacks
  g_clear_object (&self->stream);
  g_clear_pointer (&self->level_stream, pa_stream_unref);

  G_OBJECT_CLASS (cc_level_bar_parent_class)->dispose (object);
}

void
cc_level_bar_class_init (CcLevelBarClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = cc_level_bar_dispose;
}

void
cc_level_bar_init (CcLevelBar *self)
{
  self->legacy_level_bar = gvc_level_bar_new ();
  gtk_widget_show (self->legacy_level_bar);
  gtk_widget_set_hexpand (self->legacy_level_bar, TRUE);
  gtk_container_add (GTK_CONTAINER (self), self->legacy_level_bar);
  self->peak_adjustment = gvc_level_bar_get_peak_adjustment (GVC_LEVEL_BAR (self->legacy_level_bar));
}

void
cc_level_bar_set_stream (CcLevelBar     *self,
                         GvcMixerStream *stream)
{
  pa_context       *context;
  pa_sample_spec    sample_spec;
  pa_proplist      *proplist;
  pa_buffer_attr    attr;
  g_autofree gchar *device = NULL;

  g_return_if_fail (CC_IS_LEVEL_BAR (self));

  // FIXME: Disconnect callbacks
  g_clear_object (&self->stream);
  g_clear_pointer (&self->level_stream, pa_stream_unref);

  self->stream = g_object_ref (stream);

  context = gvc_mixer_stream_get_pa_context (stream);

  if (pa_context_get_server_protocol_version (context) < 13)
    return;

  sample_spec.channels = 1;
  sample_spec.format = PA_SAMPLE_FLOAT32;
  sample_spec.rate = 25;

  proplist = pa_proplist_new ();
  pa_proplist_sets (proplist, PA_PROP_APPLICATION_ID, "org.gnome.VolumeControl");
  self->level_stream = pa_stream_new_with_proplist (context, "Peak detect", &sample_spec, NULL, proplist);
  pa_proplist_free (proplist);
  if (self->level_stream == NULL) {
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
                                                     PA_STREAM_ADJUST_LATENCY)) < 0) {
    g_warning ("Failed to connect monitoring stream");
  }
}
