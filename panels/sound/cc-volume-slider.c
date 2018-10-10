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

#include <math.h>
#include <pulse/pulseaudio.h>
#include <gvc-mixer-control.h>
#include <canberra-gtk.h>

#include "cc-sound-resources.h"
#include "cc-volume-slider.h"

struct _CcVolumeSlider
{
  GtkBox          parent_instance;

  GtkAdjustment  *volume_adjustment;
  GtkScale       *volume_scale;

  GvcMixerStream *stream;
};

G_DEFINE_TYPE (CcVolumeSlider, cc_volume_slider, GTK_TYPE_BOX)

static void
changed_cb (CcVolumeSlider *self)
{
   gdouble volume, rounded;

   if (self->stream == NULL)
     return;

   volume = gtk_adjustment_get_value (self->volume_adjustment);
   rounded = round (volume);

   if (volume == 0.0)
     gvc_mixer_stream_set_is_muted (self->stream, TRUE);
   if (gvc_mixer_stream_set_volume (self->stream, (pa_volume_t) rounded) != FALSE)
     gvc_mixer_stream_push_volume (self->stream);
}

static void
volume_cb (CcVolumeSlider *self)
{
  g_signal_handlers_block_by_func (self->volume_adjustment, changed_cb, self);
  gtk_adjustment_set_value (self->volume_adjustment, gvc_mixer_stream_get_volume (self->stream));
  g_signal_handlers_unblock_by_func (self->volume_adjustment, changed_cb, self);
}

static void
mute_button_toggled_cb (CcVolumeSlider *self)
{
}

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
  GObjectClass   *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = cc_volume_slider_dispose;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/sound/cc-volume-slider.ui");

  gtk_widget_class_bind_template_child (widget_class, CcVolumeSlider, volume_adjustment);
  gtk_widget_class_bind_template_child (widget_class, CcVolumeSlider, volume_scale);

  gtk_widget_class_bind_template_callback (widget_class, changed_cb);
  gtk_widget_class_bind_template_callback (widget_class, mute_button_toggled_cb);
}

void
cc_volume_slider_init (CcVolumeSlider *self)
{
  g_resources_register (cc_sound_get_resource ());

  gtk_widget_init_template (GTK_WIDGET (self));

  ca_gtk_widget_disable_sounds (GTK_WIDGET (self->volume_scale), FALSE);
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
    gtk_adjustment_set_upper (self->volume_adjustment, gvc_mixer_control_get_vol_max_norm (NULL)); // FIXME: Handle amplified
    volume_cb (self);
  }
}
