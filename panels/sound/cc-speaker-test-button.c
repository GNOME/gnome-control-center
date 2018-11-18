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

#include <gsound.h>
#include <pulse/pulseaudio.h>

#include "cc-sound-resources.h"
#include "cc-speaker-test-button.h"

struct _CcSpeakerTestButton
{
  GtkDialog             parent_instance;

  GtkImage             *image;
  GtkLabel             *label;

  GCancellable         *cancellable;
  GSoundContext        *context;
  pa_channel_position_t position;
  gboolean              playing;
  gint                  event_index;
};

G_DEFINE_TYPE (CcSpeakerTestButton, cc_speaker_test_button, GTK_TYPE_BUTTON)

#define TEST_SOUND_ID 1

static gboolean
play_sound (CcSpeakerTestButton *self);

static const gchar *
get_icon_name (CcSpeakerTestButton *self)
{
  switch (self->position)
    {
  case PA_CHANNEL_POSITION_FRONT_LEFT:
    return self->playing ? "audio-speaker-left-testing" : "audio-speaker-left";
  case PA_CHANNEL_POSITION_FRONT_RIGHT:
    return self->playing ? "audio-speaker-right-testing" : "audio-speaker-right";
  case PA_CHANNEL_POSITION_FRONT_CENTER:
    return self->playing ? "audio-speaker-center-testing" : "audio-speaker-center";
  case PA_CHANNEL_POSITION_REAR_LEFT:
    return self->playing ? "audio-speaker-left-back-testing" : "audio-speaker-left-back";
  case PA_CHANNEL_POSITION_REAR_RIGHT:
    return self->playing ? "audio-speaker-right-back-testing" : "audio-speaker-right-back";
  case PA_CHANNEL_POSITION_REAR_CENTER:
    return self->playing ? "audio-speaker-center-back-testing" : "audio-speaker-center-back";
  case PA_CHANNEL_POSITION_LFE:
    return self->playing ? "audio-subwoofer-testing" : "audio-subwoofer";
  case PA_CHANNEL_POSITION_SIDE_LEFT:
    return self->playing ? "audio-speaker-left-side-testing" : "audio-speaker-left-side";
  case PA_CHANNEL_POSITION_SIDE_RIGHT:
    return self->playing ? "audio-speaker-right-side-testing" : "audio-speaker-right-side";
  case PA_CHANNEL_POSITION_FRONT_LEFT_OF_CENTER:
    return self->playing ? "audio-speaker-front-left-of-center-testing" : "audio-speaker-front-left-of-center";
  case PA_CHANNEL_POSITION_FRONT_RIGHT_OF_CENTER:
    return self->playing ? "audio-speaker-front-right-of-center-testing" : "audio-speaker-front-right-of-center";
  case PA_CHANNEL_POSITION_MONO:
    return self->playing ? "audio-speaker-mono-testing" : "audio-speaker-mono";
  default:
    return "audio-speakers";
  }
}

static void
update_icon (CcSpeakerTestButton *self)
{
  gtk_image_set_from_icon_name (self->image,
                                get_icon_name (self),
                                GTK_ICON_SIZE_DIALOG);
}

static GStrv
get_sound_events (CcSpeakerTestButton *self)
{
  switch (self->position)
    {
  case PA_CHANNEL_POSITION_FRONT_LEFT:
    return g_strsplit ("audio-channel-front-left;audio-test-signal;bell", ";", -1);
  case PA_CHANNEL_POSITION_FRONT_RIGHT:
    return g_strsplit ("audio-channel-front-right;audio-test-signal;bell", ";", -1);
  case PA_CHANNEL_POSITION_FRONT_CENTER:
    return g_strsplit ("audio-channel-front-center;audio-test-signal;bell", ";", -1);
  case PA_CHANNEL_POSITION_REAR_LEFT:
    return g_strsplit ("audio-channel-rear-left;audio-test-signal;bell", ";", -1);
  case PA_CHANNEL_POSITION_REAR_RIGHT:
    return g_strsplit ("audio-channel-rear-right;audio-test-signal;bell", ";", -1);
  case PA_CHANNEL_POSITION_REAR_CENTER:
    return g_strsplit ("audio-channel-rear-center;audio-test-signal;bell", ";", -1);
  case PA_CHANNEL_POSITION_LFE:
    return g_strsplit ("audio-channel-lfe;audio-test-signal;bell", ";", -1);
  case PA_CHANNEL_POSITION_SIDE_LEFT:
    return g_strsplit ("audio-channel-side-left;audio-test-signal;bell", ";", -1);
  case PA_CHANNEL_POSITION_SIDE_RIGHT:
    return g_strsplit ("audio-channel-side-right;audio-test-signal;bell", ";", -1);
  case PA_CHANNEL_POSITION_FRONT_LEFT_OF_CENTER:
    return g_strsplit ("audio-channel-front-left-of-center;audio-test-signal;bell", ";", -1);
  case PA_CHANNEL_POSITION_FRONT_RIGHT_OF_CENTER:
    return g_strsplit ("audio-channel-front-right-of-center;audio-test-signal;bell", ";", -1);
  case PA_CHANNEL_POSITION_MONO:
    return g_strsplit ("audio-channel-mono;audio-test-signal;bell", ";", -1);
  default:
    return g_strsplit ("audio-test-signal;bell", ";", -1);
  }
}

static void
finish_cb (GObject      *object,
           GAsyncResult *result,
           gpointer      userdata)
{
  CcSpeakerTestButton *self = userdata;
  g_autoptr(GError) error = NULL;

  if (!gsound_context_play_full_finish (GSOUND_CONTEXT (object), result, &error))
    {
      if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        return;

      if (play_sound (self))
        return;

      g_warning ("Failed to play sound: %s", error->message);
    }

  self->playing = FALSE;
  update_icon (self);
}

static gboolean
play_sound (CcSpeakerTestButton *self)
{
  g_auto(GStrv) events = NULL;

  /* Stop existing sound */
  g_cancellable_cancel (self->cancellable);
  g_clear_object (&self->cancellable);
  self->cancellable = g_cancellable_new ();

  events = get_sound_events (self);
  if (events[self->event_index] == NULL)
    return FALSE;

  gsound_context_play_full (self->context, self->cancellable, finish_cb, self,
                            GSOUND_ATTR_MEDIA_ROLE, "test",
                            GSOUND_ATTR_MEDIA_NAME, pa_channel_position_to_pretty_string (self->position),
                            GSOUND_ATTR_CANBERRA_FORCE_CHANNEL, pa_channel_position_to_string (self->position),
                            GSOUND_ATTR_CANBERRA_ENABLE, "1",
                            GSOUND_ATTR_EVENT_ID, events[self->event_index],
                            NULL);
  self->event_index++;

  return TRUE;
}

static void
clicked_cb (CcSpeakerTestButton *self)
{
  if (self->context == NULL)
    return;

  self->playing = TRUE;
  update_icon (self);

  /* Play the per-channel sound name or a generic sound */
  self->event_index = 0;
  play_sound (self);
}

static void
cc_speaker_test_button_dispose (GObject *object)
{
  CcSpeakerTestButton *self = CC_SPEAKER_TEST_BUTTON (object);

  g_cancellable_cancel (self->cancellable);
  g_clear_object (&self->cancellable);
  g_clear_object (&self->context);

  G_OBJECT_CLASS (cc_speaker_test_button_parent_class)->dispose (object);
}

void
cc_speaker_test_button_class_init (CcSpeakerTestButtonClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = cc_speaker_test_button_dispose;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/sound/cc-speaker-test-button.ui");

  gtk_widget_class_bind_template_child (widget_class, CcSpeakerTestButton, image);
  gtk_widget_class_bind_template_child (widget_class, CcSpeakerTestButton, label);

  gtk_widget_class_bind_template_callback (widget_class, clicked_cb);
}

void
cc_speaker_test_button_init (CcSpeakerTestButton *self)
{
  g_resources_register (cc_sound_get_resource ());

  gtk_widget_init_template (GTK_WIDGET (self));

  self->cancellable = g_cancellable_new ();

  update_icon (self);
}

CcSpeakerTestButton *
cc_speaker_test_button_new (void)
{
  return g_object_new (CC_TYPE_SPEAKER_TEST_BUTTON, NULL);
}

void
cc_speaker_test_button_set_channel_position (CcSpeakerTestButton  *self,
                                             GSoundContext        *context,
                                             pa_channel_position_t position)
{
  g_return_if_fail (CC_IS_SPEAKER_TEST_BUTTON (self));

  g_clear_object (&self->context);
  self->context = g_object_ref (context);
  self->position = position;
  gtk_label_set_label (self->label, pa_channel_position_to_pretty_string (position));
  update_icon (self);
}
