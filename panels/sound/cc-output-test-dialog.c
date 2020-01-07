/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * Copyright (C) 2018 Canonical Ltd.
 * Copyright 2020 Purism SPC
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
 *
 * Author(s):
 *   Robert Ancell <robert.ancell@canonical.com>
 *   Mohammed Sadiq <sadiq@sadiqpk.org>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <gsound.h>
#include <glib/gi18n.h>

#include "cc-output-test-dialog.h"
#include "cc-sound-resources.h"
#include "cc-speaker-test-row.h"
#include "list-box-helper.h"

struct _CcOutputTestDialog
{
  HdyDialog         parent_instance;

  GtkListBox       *speaker_list;
  CcSpeakerTestRow *front_center_row;
  CcSpeakerTestRow *front_left_row;
  CcSpeakerTestRow *front_left_of_center_row;
  CcSpeakerTestRow *front_right_of_center_row;
  CcSpeakerTestRow *front_right_row;
  CcSpeakerTestRow *lfe_row;  /* Subwoofer */
  CcSpeakerTestRow *rear_center_row;
  CcSpeakerTestRow *rear_left_row;
  CcSpeakerTestRow *rear_right_row;
  CcSpeakerTestRow *side_left_row;
  CcSpeakerTestRow *side_right_row;

  GvcMixerUIDevice *device;
  GSoundContext    *context;
};

G_DEFINE_TYPE (CcOutputTestDialog, cc_output_test_dialog, HDY_TYPE_DIALOG)

static void
cc_output_test_dialog_dispose (GObject *object)
{
  CcOutputTestDialog *self = CC_OUTPUT_TEST_DIALOG (object);

  g_clear_object (&self->device);
  g_clear_object (&self->context);

  G_OBJECT_CLASS (cc_output_test_dialog_parent_class)->dispose (object);
}

void
cc_output_test_dialog_class_init (CcOutputTestDialogClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = cc_output_test_dialog_dispose;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/sound/cc-output-test-dialog.ui");

  gtk_widget_class_bind_template_child (widget_class, CcOutputTestDialog, speaker_list);
  gtk_widget_class_bind_template_child (widget_class, CcOutputTestDialog, front_left_row);
  gtk_widget_class_bind_template_child (widget_class, CcOutputTestDialog, front_left_of_center_row);
  gtk_widget_class_bind_template_child (widget_class, CcOutputTestDialog, front_center_row);
  gtk_widget_class_bind_template_child (widget_class, CcOutputTestDialog, front_left_row);
  gtk_widget_class_bind_template_child (widget_class, CcOutputTestDialog, front_left_of_center_row);
  gtk_widget_class_bind_template_child (widget_class, CcOutputTestDialog, front_right_of_center_row);
  gtk_widget_class_bind_template_child (widget_class, CcOutputTestDialog, front_right_row);
  gtk_widget_class_bind_template_child (widget_class, CcOutputTestDialog, lfe_row);
  gtk_widget_class_bind_template_child (widget_class, CcOutputTestDialog, rear_center_row);
  gtk_widget_class_bind_template_child (widget_class, CcOutputTestDialog, rear_left_row);
  gtk_widget_class_bind_template_child (widget_class, CcOutputTestDialog, rear_right_row);
  gtk_widget_class_bind_template_child (widget_class, CcOutputTestDialog, side_left_row);
  gtk_widget_class_bind_template_child (widget_class, CcOutputTestDialog, side_right_row);

  g_type_ensure (CC_TYPE_SPEAKER_TEST_ROW);
}

void
cc_output_test_dialog_init (CcOutputTestDialog *self)
{
  GtkSettings *settings;
  g_autofree gchar *theme_name = NULL;

  gtk_widget_init_template (GTK_WIDGET (self));

  gtk_list_box_set_header_func (GTK_LIST_BOX (self->speaker_list),
                                cc_list_box_update_header_func,
                                NULL, NULL);

  self->context = gsound_context_new (NULL, NULL);
  gsound_context_set_driver (self->context, "pulse", NULL);
  gsound_context_set_attributes (self->context, NULL,
                                 GSOUND_ATTR_APPLICATION_ID, "org.gnome.VolumeControl",
                                 NULL);
  settings = gtk_settings_get_for_screen (gdk_screen_get_default ());
  g_object_get (G_OBJECT (settings),
                "gtk-sound-theme-name", &theme_name,
                NULL);
  if (theme_name != NULL)
     gsound_context_set_attributes (self->context, NULL,
                                    GSOUND_ATTR_CANBERRA_XDG_THEME_NAME, theme_name,
                                    NULL);

  cc_speaker_test_row_set_channel_position (self->front_left_row, self->context, PA_CHANNEL_POSITION_FRONT_LEFT);
  cc_speaker_test_row_set_channel_position (self->front_left_of_center_row, self->context, PA_CHANNEL_POSITION_FRONT_LEFT_OF_CENTER);
  cc_speaker_test_row_set_channel_position (self->front_center_row, self->context, PA_CHANNEL_POSITION_FRONT_CENTER);
  cc_speaker_test_row_set_channel_position (self->front_right_of_center_row, self->context, PA_CHANNEL_POSITION_FRONT_RIGHT_OF_CENTER);
  cc_speaker_test_row_set_channel_position (self->front_right_row, self->context, PA_CHANNEL_POSITION_FRONT_RIGHT);
  cc_speaker_test_row_set_channel_position (self->side_left_row, self->context, PA_CHANNEL_POSITION_SIDE_LEFT);
  cc_speaker_test_row_set_channel_position (self->side_right_row, self->context, PA_CHANNEL_POSITION_SIDE_RIGHT);
  cc_speaker_test_row_set_channel_position (self->lfe_row, self->context, PA_CHANNEL_POSITION_LFE);
  cc_speaker_test_row_set_channel_position (self->rear_left_row, self->context, PA_CHANNEL_POSITION_REAR_LEFT);
  cc_speaker_test_row_set_channel_position (self->rear_center_row, self->context, PA_CHANNEL_POSITION_REAR_CENTER);
  cc_speaker_test_row_set_channel_position (self->rear_right_row, self->context, PA_CHANNEL_POSITION_REAR_RIGHT);
}

CcOutputTestDialog *
cc_output_test_dialog_new (GvcMixerUIDevice *device,
                           GvcMixerStream   *stream)
{
  CcOutputTestDialog *self;
  const GvcChannelMap *map = NULL;
  g_autofree gchar *title = NULL;

  self = g_object_new (CC_TYPE_OUTPUT_TEST_DIALOG,
                       "use-header-bar", 1,
                       NULL);
  self->device = g_object_ref (device);

  title = g_strdup_printf (_("Testing %s"), gvc_mixer_ui_device_get_description (device));
  gtk_header_bar_set_title (GTK_HEADER_BAR (gtk_dialog_get_header_bar (GTK_DIALOG (self))), title);

  map = gvc_mixer_stream_get_channel_map (stream);
  gtk_widget_set_visible (GTK_WIDGET (self->front_left_row), gvc_channel_map_has_position (map, PA_CHANNEL_POSITION_FRONT_LEFT));
  gtk_widget_set_visible (GTK_WIDGET (self->front_left_of_center_row), gvc_channel_map_has_position (map, PA_CHANNEL_POSITION_FRONT_LEFT_OF_CENTER));
  gtk_widget_set_visible (GTK_WIDGET (self->front_center_row), gvc_channel_map_has_position (map, PA_CHANNEL_POSITION_FRONT_CENTER));
  gtk_widget_set_visible (GTK_WIDGET (self->front_right_of_center_row), gvc_channel_map_has_position (map, PA_CHANNEL_POSITION_FRONT_RIGHT_OF_CENTER));
  gtk_widget_set_visible (GTK_WIDGET (self->front_right_row), gvc_channel_map_has_position (map, PA_CHANNEL_POSITION_FRONT_RIGHT));
  gtk_widget_set_visible (GTK_WIDGET (self->side_left_row), gvc_channel_map_has_position (map, PA_CHANNEL_POSITION_SIDE_LEFT));
  gtk_widget_set_visible (GTK_WIDGET (self->side_right_row), gvc_channel_map_has_position (map, PA_CHANNEL_POSITION_SIDE_RIGHT));
  gtk_widget_set_visible (GTK_WIDGET (self->lfe_row), gvc_channel_map_has_position (map, PA_CHANNEL_POSITION_LFE));
  gtk_widget_set_visible (GTK_WIDGET (self->rear_left_row), gvc_channel_map_has_position (map, PA_CHANNEL_POSITION_REAR_LEFT));
  gtk_widget_set_visible (GTK_WIDGET (self->rear_center_row), gvc_channel_map_has_position (map, PA_CHANNEL_POSITION_REAR_CENTER));
  gtk_widget_set_visible (GTK_WIDGET (self->rear_right_row), gvc_channel_map_has_position (map, PA_CHANNEL_POSITION_REAR_RIGHT));

  /* Replace the center channel with a mono channel */
  if (gvc_channel_map_has_position (map, PA_CHANNEL_POSITION_MONO))
    {
      if (gvc_channel_map_has_position (map, PA_CHANNEL_POSITION_FRONT_CENTER))
        g_warning ("Testing output with both front center and mono channels - front center is hidden");
      cc_speaker_test_row_set_channel_position (self->front_center_row, self->context, PA_CHANNEL_POSITION_MONO);
      gtk_widget_set_visible (GTK_WIDGET (self->front_center_row), TRUE);
    }

  return self;
}
