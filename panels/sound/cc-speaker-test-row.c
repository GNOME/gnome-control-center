/* -*- mode: c; c-file-style: "gnu"; c-basic-offset: 2; indent-tabs-mode: nil; -*- */
/* cc-speaker-test-row.c
 *
 * Copyright 2019 Purism SPC
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Author(s):
 *   Mohammed Sadiq <sadiq@sadiqpk.org>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <gsound.h>
#include <pulse/pulseaudio.h>

#include "cc-sound-resources.h"
#include "cc-speaker-test-row.h"

struct _CcSpeakerTestRow
{
  GtkListBoxRow          parent_instance;

  GtkImage              *speaker_icon;
  GtkLabel              *speaker_label;
  GtkImage              *playing_icon;

  gchar                 *channel_name;
  GCancellable          *cancellable;
  GSoundContext         *context;
  pa_channel_position_t  position;
  gint                   event_index;
};


G_DEFINE_TYPE (CcSpeakerTestRow, cc_speaker_test_row, GTK_TYPE_LIST_BOX_ROW)

const gchar *test_sounds[] = {"audio-test-signal", "bell", NULL};

static gboolean play_test_sound (CcSpeakerTestRow *self);

static void
test_sound_play_finish_cb (GObject      *object,
                           GAsyncResult *result,
                           gpointer      userdata)
{
  g_autoptr(CcSpeakerTestRow) self = userdata;
  g_autoptr(GError) error = NULL;

  if (!gsound_context_play_full_finish (GSOUND_CONTEXT (object), result, &error))
    {
      if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        return;

      if (play_test_sound (self))
        return;

      g_warning ("Failed to play sound: %s", error->message);
    }

  gtk_widget_hide (GTK_WIDGET (self->playing_icon));
}

static gboolean
play_test_sound (CcSpeakerTestRow *self)
{
  const gchar *test_sound;

  /* Stop existing sound */
  g_cancellable_cancel (self->cancellable);
  g_clear_object (&self->cancellable);
  self->cancellable = g_cancellable_new ();

  if (self->event_index == -1)
    test_sound = self->channel_name;
  else
    test_sound = test_sounds[self->event_index];

  if (test_sound == NULL)
    return FALSE;

  self->event_index++;
  gtk_widget_show (GTK_WIDGET (self->playing_icon));

  gsound_context_play_full (self->context, self->cancellable,
                            test_sound_play_finish_cb, g_object_ref (self),
                            GSOUND_ATTR_MEDIA_ROLE, "test",
                            GSOUND_ATTR_MEDIA_NAME, pa_channel_position_to_pretty_string (self->position),
                            GSOUND_ATTR_CANBERRA_FORCE_CHANNEL, pa_channel_position_to_string (self->position),
                            GSOUND_ATTR_CANBERRA_ENABLE, "1",
                            GSOUND_ATTR_EVENT_ID, test_sound,
                            NULL);
  return TRUE;
}

static void
speaker_test_row_activated_cb (CcSpeakerTestRow *self,
                               GtkListBoxRow    *row)
{
  g_assert (CC_SPEAKER_TEST_ROW (self));

  if (!self->context || row != GTK_LIST_BOX_ROW (self))
    return;

  self->event_index = -1;
  play_test_sound (self);
}

static void
speaker_row_parent_changed_cb (CcSpeakerTestRow *self)
{
  GtkWidget *parent;

  g_assert (CC_SPEAKER_TEST_ROW (self));

  parent = gtk_widget_get_parent (GTK_WIDGET (self));

  if (!parent)
    return;

  g_return_if_fail (GTK_IS_LIST_BOX (parent));
  g_signal_connect_object (parent, "row-activated",
                           G_CALLBACK (speaker_test_row_activated_cb),
                           self, G_CONNECT_SWAPPED);
}

static void
cc_speaker_test_row_dispose (GObject *object)
{
  CcSpeakerTestRow *self = CC_SPEAKER_TEST_ROW (object);

  g_cancellable_cancel (self->cancellable);
  g_clear_object (&self->cancellable);
  g_clear_object (&self->context);
  g_clear_pointer (&self->channel_name, g_free);

  G_OBJECT_CLASS (cc_speaker_test_row_parent_class)->dispose (object);
}

void
cc_speaker_test_row_class_init (CcSpeakerTestRowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = cc_speaker_test_row_dispose;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/sound/cc-speaker-test-row.ui");

  gtk_widget_class_bind_template_child (widget_class, CcSpeakerTestRow, speaker_label);
  gtk_widget_class_bind_template_child (widget_class, CcSpeakerTestRow, speaker_icon);
  gtk_widget_class_bind_template_child (widget_class, CcSpeakerTestRow, playing_icon);
}

void
cc_speaker_test_row_init (CcSpeakerTestRow *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
  self->cancellable = g_cancellable_new ();

  g_signal_connect_object (self, "notify::parent",
                           G_CALLBACK (speaker_row_parent_changed_cb),
                           self, G_CONNECT_SWAPPED);
}

void
cc_speaker_test_row_set_channel_position (CcSpeakerTestRow      *self,
                                          GSoundContext         *context,
                                          pa_channel_position_t  position)
{
  const gchar *channel_name;
  g_return_if_fail (CC_IS_SPEAKER_TEST_ROW (self));

  g_set_object (&self->context, context);
  self->position = position;

  channel_name = pa_channel_position_to_string (position);
  g_free (self->channel_name);
  self->channel_name = g_strdup_printf ("audio-channel-%s", channel_name);
  gtk_image_set_from_icon_name (self->speaker_icon, self->channel_name, GTK_ICON_SIZE_BUTTON);

  channel_name = pa_channel_position_to_pretty_string (position);
  gtk_label_set_label (self->speaker_label, channel_name);
}
