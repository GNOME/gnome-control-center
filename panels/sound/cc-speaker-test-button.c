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

#include "cc-sound-resources.h"
#include "cc-speaker-test-button.h"

struct _CcSpeakerTestButton
{
  GtkDialog parent_instance;

  GtkImage *image;
  GtkLabel *label;

  pa_channel_position_t position;
};

G_DEFINE_TYPE (CcSpeakerTestButton, cc_speaker_test_button, GTK_TYPE_BUTTON)

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
                                             pa_channel_position_t position)
{
  g_return_if_fail (CC_IS_SPEAKER_TEST_BUTTON (self));

  self->position = position;
  gtk_label_set_label (self->label, pa_channel_position_to_pretty_string (position));
}
