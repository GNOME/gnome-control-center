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

#include <pulse/pulseaudio.h>
#include <gvc-mixer-control.h>

#include "cc-volume-slider.h"

struct _CcVolumeSlider
{
  GtkScale        parent_instance;

  GvcMixerStream *stream;
};

G_DEFINE_TYPE (CcVolumeSlider, cc_volume_slider, GTK_TYPE_SCALE)

static void
cc_volume_slider_dispose (GObject *object)
{
  CcVolumeSlider *self = CC_VOLUME_SLIDER (object);

  g_clear_object (&self->stream);

  G_OBJECT_CLASS (cc_volume_slider_parent_class)->dispose (object);
}

void
cc_volume_slider_class_init (CcVolumeSliderClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = cc_volume_slider_dispose;
}

void
cc_volume_slider_init (CcVolumeSlider *self)
{
  gtk_scale_set_draw_value (GTK_SCALE (self), FALSE);
}

static void
volume_cb (CcVolumeSlider *self)
{
  gtk_adjustment_set_value (gtk_range_get_adjustment (GTK_RANGE (self)), gvc_mixer_stream_get_volume (self->stream));
}

void
cc_volume_slider_set_stream (CcVolumeSlider *self,
                             GvcMixerStream *stream)
{
  // FIXME: disconnect
  g_clear_object (&self->stream);

  if (stream) {
    self->stream = g_object_ref (stream);

    g_signal_connect_object (stream,
                             "notify::volume",
                             G_CALLBACK (volume_cb),
                             self, G_CONNECT_SWAPPED);
    gtk_adjustment_set_upper (gtk_range_get_adjustment (GTK_RANGE (self)), gvc_mixer_control_get_vol_max_norm (NULL)); // FIXME: Handle amplified
    volume_cb (self);
  }
}
