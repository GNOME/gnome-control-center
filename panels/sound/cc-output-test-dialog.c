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

#include "cc-output-test-dialog.h"
#include "cc-sound-resources.h"
#include "cc-speaker-test-button.h"

struct _CcOutputTestDialog
{
  GtkDialog            parent_instance;

  CcSpeakerTestButton *front_center_speaker_button;
  CcSpeakerTestButton *front_left_speaker_button;
  CcSpeakerTestButton *front_left_of_center_speaker_button;
  CcSpeakerTestButton *front_right_of_center_speaker_button;
  CcSpeakerTestButton *front_right_speaker_button;
  CcSpeakerTestButton *lfe_speaker_button;
  CcSpeakerTestButton *rear_center_speaker_button;
  CcSpeakerTestButton *rear_left_speaker_button;
  CcSpeakerTestButton *rear_right_speaker_button;
  CcSpeakerTestButton *side_left_speaker_button;
  CcSpeakerTestButton *side_right_speaker_button;

  GvcMixerUIDevice    *device;
};

G_DEFINE_TYPE (CcOutputTestDialog, cc_output_test_dialog, GTK_TYPE_DIALOG)

static void
cc_output_test_dialog_dispose (GObject *object)
{
  CcOutputTestDialog *self = CC_OUTPUT_TEST_DIALOG (object);

  g_clear_object (&self->device);

  G_OBJECT_CLASS (cc_output_test_dialog_parent_class)->dispose (object);
}

void
cc_output_test_dialog_class_init (CcOutputTestDialogClass *klass)
{
  GObjectClass   *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = cc_output_test_dialog_dispose;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/sound/cc-output-test-dialog.ui");

  gtk_widget_class_bind_template_child (widget_class, CcOutputTestDialog, front_center_speaker_button);
  gtk_widget_class_bind_template_child (widget_class, CcOutputTestDialog, front_left_speaker_button);
  gtk_widget_class_bind_template_child (widget_class, CcOutputTestDialog, front_left_of_center_speaker_button);
  gtk_widget_class_bind_template_child (widget_class, CcOutputTestDialog, front_right_of_center_speaker_button);
  gtk_widget_class_bind_template_child (widget_class, CcOutputTestDialog, front_right_speaker_button);
  gtk_widget_class_bind_template_child (widget_class, CcOutputTestDialog, lfe_speaker_button);
  gtk_widget_class_bind_template_child (widget_class, CcOutputTestDialog, rear_center_speaker_button);
  gtk_widget_class_bind_template_child (widget_class, CcOutputTestDialog, rear_left_speaker_button);
  gtk_widget_class_bind_template_child (widget_class, CcOutputTestDialog, rear_right_speaker_button);
  gtk_widget_class_bind_template_child (widget_class, CcOutputTestDialog, side_left_speaker_button);
  gtk_widget_class_bind_template_child (widget_class, CcOutputTestDialog, side_right_speaker_button);
}

void
cc_output_test_dialog_init (CcOutputTestDialog *self)
{
  g_resources_register (cc_sound_get_resource ());

  cc_speaker_test_button_get_type ();
  gtk_widget_init_template (GTK_WIDGET (self));

  cc_speaker_test_button_set_channel_position (self->front_left_speaker_button, PA_CHANNEL_POSITION_FRONT_LEFT);
  cc_speaker_test_button_set_channel_position (self->front_left_of_center_speaker_button, PA_CHANNEL_POSITION_FRONT_LEFT_OF_CENTER);
  cc_speaker_test_button_set_channel_position (self->front_center_speaker_button, PA_CHANNEL_POSITION_FRONT_CENTER);
  cc_speaker_test_button_set_channel_position (self->front_right_of_center_speaker_button, PA_CHANNEL_POSITION_FRONT_RIGHT_OF_CENTER);
  cc_speaker_test_button_set_channel_position (self->front_right_speaker_button, PA_CHANNEL_POSITION_FRONT_RIGHT);
  cc_speaker_test_button_set_channel_position (self->side_left_speaker_button, PA_CHANNEL_POSITION_SIDE_LEFT);
  cc_speaker_test_button_set_channel_position (self->side_right_speaker_button, PA_CHANNEL_POSITION_SIDE_RIGHT);
  cc_speaker_test_button_set_channel_position (self->lfe_speaker_button, PA_CHANNEL_POSITION_LFE);
  cc_speaker_test_button_set_channel_position (self->rear_left_speaker_button, PA_CHANNEL_POSITION_REAR_LEFT);
  cc_speaker_test_button_set_channel_position (self->rear_center_speaker_button, PA_CHANNEL_POSITION_REAR_CENTER);
  cc_speaker_test_button_set_channel_position (self->rear_right_speaker_button, PA_CHANNEL_POSITION_REAR_RIGHT);
}

CcOutputTestDialog *
cc_output_test_dialog_new (GvcMixerUIDevice *device)
{
  CcOutputTestDialog *self;

  self = g_object_new (cc_output_test_dialog_get_type (),
                       "use-header-bar", 1,
                       NULL);
  self->device = g_object_ref (device);

  return self;
}
