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

#include <glib/gi18n.h>
#include <math.h>
#include <pulse/pulseaudio.h>
#include <gvc-mixer-control.h>
#include <canberra-gtk.h>

#include "cc-sound-resources.h"
#include "cc-volume-slider.h"

struct _CcVolumeSlider
{
  GtkBox           parent_instance;

  GtkToggleButton *mute_button;
  GtkAdjustment   *volume_adjustment;
  GtkScale        *volume_scale;

  GvcMixerStream  *stream;
  guint            notify_volume_handler_id;
  guint            notify_is_muted_handler_id;
};

G_DEFINE_TYPE (CcVolumeSlider, cc_volume_slider, GTK_TYPE_BOX)

static void
volume_changed_cb (CcVolumeSlider *self)
{
  gdouble volume, rounded;

  if (self->stream == NULL)
    return;

  volume = gtk_adjustment_get_value (self->volume_adjustment);
  rounded = round (volume);

  gtk_toggle_button_set_active (self->mute_button, volume == 0.0);
  if (gvc_mixer_stream_set_volume (self->stream,
                                   (pa_volume_t) rounded))
    {
      gvc_mixer_stream_push_volume (self->stream);
    }
}

static void
mute_button_toggled_cb (CcVolumeSlider *self)
{
  if (self->stream == NULL)
    return;

  gvc_mixer_stream_change_is_muted (self->stream, gtk_toggle_button_get_active (self->mute_button));
}

static void
notify_volume_cb (CcVolumeSlider *self)
{
  g_signal_handlers_block_by_func (self->volume_adjustment, volume_changed_cb, self);
  if (gtk_toggle_button_get_active (self->mute_button))
    gtk_adjustment_set_value (self->volume_adjustment, 0.0);
  else
    gtk_adjustment_set_value (self->volume_adjustment, gvc_mixer_stream_get_volume (self->stream));
  g_signal_handlers_unblock_by_func (self->volume_adjustment, volume_changed_cb, self);
}

static void
notify_is_muted_cb (CcVolumeSlider *self)
{
  g_signal_handlers_block_by_func (self->mute_button, mute_button_toggled_cb, self);
  gtk_toggle_button_set_active (self->mute_button, gvc_mixer_stream_get_is_muted (self->stream));
  g_signal_handlers_unblock_by_func (self->mute_button, mute_button_toggled_cb, self);
  notify_volume_cb (self);
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
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = cc_volume_slider_dispose;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/sound/cc-volume-slider.ui");

  gtk_widget_class_bind_template_child (widget_class, CcVolumeSlider, mute_button);
  gtk_widget_class_bind_template_child (widget_class, CcVolumeSlider, volume_adjustment);
  gtk_widget_class_bind_template_child (widget_class, CcVolumeSlider, volume_scale);

  gtk_widget_class_bind_template_callback (widget_class, mute_button_toggled_cb);
  gtk_widget_class_bind_template_callback (widget_class, volume_changed_cb);
}

void
cc_volume_slider_init (CcVolumeSlider *self)
{
  gdouble vol_max_norm;

  g_resources_register (cc_sound_get_resource ());

  gtk_widget_init_template (GTK_WIDGET (self));

  vol_max_norm = gvc_mixer_control_get_vol_max_norm (NULL);
  gtk_adjustment_set_upper (self->volume_adjustment, vol_max_norm);
  gtk_adjustment_set_page_increment (self->volume_adjustment, vol_max_norm / 100.0);
}

void
cc_volume_slider_set_stream (CcVolumeSlider *self,
                             GvcMixerStream *stream)
{
  g_return_if_fail (CC_IS_VOLUME_SLIDER (self));

  if (self->stream != NULL)
    {
      g_signal_handler_disconnect (self->stream, self->notify_volume_handler_id);
      self->notify_volume_handler_id = 0;
      g_signal_handler_disconnect (self->stream, self->notify_is_muted_handler_id);
      self->notify_is_muted_handler_id = 0;
    }
  g_clear_object (&self->stream);

  if (stream != NULL)
    {
      self->stream = g_object_ref (stream);

      self->notify_volume_handler_id = g_signal_connect_object (stream,
                                                                "notify::volume",
                                                                G_CALLBACK (notify_volume_cb),
                                                                self, G_CONNECT_SWAPPED);
      self->notify_is_muted_handler_id = g_signal_connect_object (stream,
                                                                  "notify::is-muted",
                                                                  G_CALLBACK (notify_is_muted_cb),
                                                                  self, G_CONNECT_SWAPPED);
      notify_volume_cb (self);
      notify_is_muted_cb (self);
    }
}

void
cc_volume_slider_set_is_amplified (CcVolumeSlider *self,
                                   gboolean        is_amplified)
{
  g_return_if_fail (CC_IS_VOLUME_SLIDER (self));

  gtk_scale_clear_marks (self->volume_scale);
  if (is_amplified)
    {
      gtk_adjustment_set_upper (self->volume_adjustment, gvc_mixer_control_get_vol_max_amplified (NULL));
      gtk_scale_add_mark (self->volume_scale,
                          gvc_mixer_control_get_vol_max_norm (NULL),
                          GTK_POS_BOTTOM,
                          C_("volume", "100%"));
    }
  else
    {
      gtk_adjustment_set_upper (self->volume_adjustment, gvc_mixer_control_get_vol_max_norm (NULL));
    }
}
