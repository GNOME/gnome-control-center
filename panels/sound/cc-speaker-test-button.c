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

#include <canberra.h>
#include <pulse/pulseaudio.h>

#include "cc-sound-resources.h"
#include "cc-speaker-test-button.h"

// FIXME: Use GSound?

struct _CcSpeakerTestButton
{
  GtkDialog             parent_instance;

  GtkImage             *image;
  GtkLabel             *label;

  ca_context           *context;
  pa_channel_position_t position;
};

G_DEFINE_TYPE (CcSpeakerTestButton, cc_speaker_test_button, GTK_TYPE_BUTTON)

#define TEST_SOUND_ID 1

static void
finish_cb (ca_context *c,
           uint32_t    id,
           int         error_code,
           void       *userdata)
{
  CcSpeakerTestButton *self = userdata;
   g_printerr ("finish\n");
}

static const gchar *
get_sound_name (CcSpeakerTestButton *self)
{
  switch (self->position) {
  case PA_CHANNEL_POSITION_FRONT_LEFT:
    return "audio-channel-front-left";
  case PA_CHANNEL_POSITION_FRONT_RIGHT:
    return "audio-channel-front-right";
  case PA_CHANNEL_POSITION_FRONT_CENTER:
    return "audio-channel-front-center";
  case PA_CHANNEL_POSITION_REAR_LEFT:
    return "audio-channel-rear-left";
  case PA_CHANNEL_POSITION_REAR_RIGHT:
    return "audio-channel-rear-right";
  case PA_CHANNEL_POSITION_REAR_CENTER:
    return "audio-channel-rear-center";
  case PA_CHANNEL_POSITION_LFE:
    return "audio-channel-lfe";
  case PA_CHANNEL_POSITION_SIDE_LEFT:
    return "audio-channel-side-left";
  case PA_CHANNEL_POSITION_SIDE_RIGHT:
    return "audio-channel-side-right";
  case PA_CHANNEL_POSITION_FRONT_LEFT_OF_CENTER:
    return "audio-channel-front-left-of-center";
  case PA_CHANNEL_POSITION_FRONT_RIGHT_OF_CENTER:
    return "audio-channel-front-right-of-center";
  case PA_CHANNEL_POSITION_MONO:
    return "audio-channel-mono";
  default:
    return NULL;
  }
}

static void
clicked_cb (CcSpeakerTestButton *self)
{
  ca_proplist *proplist;
  const gchar *name;
  int error;

  if (self->context == NULL)
    return;

  ca_context_cancel (self->context, TEST_SOUND_ID);

  ca_proplist_create (&proplist);
  ca_proplist_sets (proplist, CA_PROP_MEDIA_ROLE, "test");
  ca_proplist_sets (proplist, CA_PROP_MEDIA_NAME, pa_channel_position_to_pretty_string (self->position));
  ca_proplist_sets (proplist, CA_PROP_CANBERRA_FORCE_CHANNEL, pa_channel_position_to_string (self->position));
  ca_proplist_sets (proplist, CA_PROP_CANBERRA_ENABLE, "1");

  name = get_sound_name (self);
  if (name != NULL) {
    ca_proplist_sets (proplist, CA_PROP_EVENT_ID, name);
    error = ca_context_play_full (self->context, TEST_SOUND_ID, proplist, finish_cb, self);
    if (error == 0)
      return;
  }

  ca_proplist_sets (proplist, CA_PROP_EVENT_ID, "audio-test-signal");
  error = ca_context_play_full (self->context, TEST_SOUND_ID, proplist, finish_cb, self);
  if (error == 0)
    return;

  ca_proplist_sets (proplist, CA_PROP_EVENT_ID, "bell-window-system");
  error = ca_context_play_full (self->context, TEST_SOUND_ID, proplist, finish_cb, self);
  if (error == 0)
    return;

  g_warning ("Failed to play test sound: %s", ca_strerror (error));
}

static void
cc_speaker_test_button_dispose (GObject *object)
{
  CcSpeakerTestButton *self = CC_SPEAKER_TEST_BUTTON (object);

  G_OBJECT_CLASS (cc_speaker_test_button_parent_class)->dispose (object);
}

void
cc_speaker_test_button_class_init (CcSpeakerTestButtonClass *klass)
{
  GObjectClass   *object_class = G_OBJECT_CLASS (klass);
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
}

CcSpeakerTestButton *
cc_speaker_test_button_new (void)
{
  return g_object_new (cc_speaker_test_button_get_type (), NULL);
}

void
cc_speaker_test_button_set_channel_position (CcSpeakerTestButton  *self,
                                             ca_context           *context,
                                             pa_channel_position_t position)
{
  g_return_if_fail (CC_IS_SPEAKER_TEST_BUTTON (self));

  self->context = context;
  self->position = position;
  gtk_label_set_label (self->label, pa_channel_position_to_pretty_string (position));
}
