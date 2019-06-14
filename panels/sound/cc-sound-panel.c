/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2008 Red Hat, Inc.
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

#include "config.h"

#include <libintl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include <glib/gi18n-lib.h>
#include <glib.h>
#include <gtk/gtk.h>
#include <pulse/pulseaudio.h>
#include <gvc-mixer-control.h>

#include "list-box-helper.h"
#include "cc-alert-chooser.h"
#include "cc-balance-slider.h"
#include "cc-device-combo-box.h"
#include "cc-fade-slider.h"
#include "cc-level-bar.h"
#include "cc-output-test-dialog.h"
#include "cc-profile-combo-box.h"
#include "cc-sound-panel.h"
#include "cc-sound-resources.h"
#include "cc-stream-list-box.h"
#include "cc-subwoofer-slider.h"
#include "cc-volume-slider.h"

struct _CcSoundPanel
{
  CcPanel            parent_instance;

  CcBalanceSlider   *balance_slider;
  GtkListBoxRow     *fade_row;
  CcFadeSlider      *fade_slider;
  CcDeviceComboBox  *input_device_combo_box;
  CcLevelBar        *input_level_bar;
  GtkListBox        *input_list_box;
  CcProfileComboBox *input_profile_combo_box;
  GtkListBoxRow     *input_profile_row;
  CcVolumeSlider    *input_volume_slider;
  GtkSizeGroup      *label_size_group;
  GtkBox            *main_box;
  CcDeviceComboBox  *output_device_combo_box;
  GtkListStore      *output_device_model;
  CcLevelBar        *output_level_bar;
  GtkListBox        *output_list_box;
  CcProfileComboBox *output_profile_combo_box;
  GtkListBoxRow     *output_profile_row;
  CcVolumeSlider    *output_volume_slider;
  CcStreamListBox   *stream_list_box;
  GtkListBoxRow     *subwoofer_row;
  CcSubwooferSlider *subwoofer_slider;

  GvcMixerControl   *mixer_control;
  GSettings         *sound_settings;
};

CC_PANEL_REGISTER (CcSoundPanel, cc_sound_panel)

enum
{
  PROP_0,
  PROP_PARAMETERS
};

#define KEY_SOUNDS_SCHEMA "org.gnome.desktop.sound"

static void
allow_amplified_changed_cb (CcSoundPanel *self)
{
  cc_volume_slider_set_is_amplified (self->output_volume_slider,
                                     g_settings_get_boolean (self->sound_settings, "allow-volume-above-100-percent"));
}

static void
output_device_changed_cb (CcSoundPanel *self)
{
  GvcMixerUIDevice *device;
  GvcMixerStream *stream = NULL;
  GvcChannelMap *map = NULL;
  gboolean can_fade = FALSE, has_lfe = FALSE;

  device = cc_device_combo_box_get_device (self->output_device_combo_box);
  cc_profile_combo_box_set_device (self->output_profile_combo_box, self->mixer_control, device);
  gtk_widget_set_visible (GTK_WIDGET (self->output_profile_row),
                          cc_profile_combo_box_get_profile_count (self->output_profile_combo_box) > 1);

  if (device != NULL)
    stream = gvc_mixer_control_get_stream_from_device (self->mixer_control, device);

  cc_volume_slider_set_stream (self->output_volume_slider, stream, CC_STREAM_TYPE_OUTPUT);
  cc_level_bar_set_stream (self->output_level_bar, stream, CC_STREAM_TYPE_OUTPUT);

  if (stream != NULL)
    {
      map = (GvcChannelMap *) gvc_mixer_stream_get_channel_map (stream);
      can_fade = gvc_channel_map_can_fade (map);
      has_lfe = gvc_channel_map_has_lfe (map);
    }
  cc_fade_slider_set_channel_map (self->fade_slider, map);
  cc_balance_slider_set_channel_map (self->balance_slider, map);
  cc_subwoofer_slider_set_channel_map (self->subwoofer_slider, map);

  gtk_widget_set_visible (GTK_WIDGET (self->fade_row), can_fade);
  gtk_widget_set_visible (GTK_WIDGET (self->subwoofer_row), has_lfe);

  if (device != NULL)
    gvc_mixer_control_change_output (self->mixer_control, device);
}

static void
input_device_changed_cb (CcSoundPanel *self)
{
  GvcMixerUIDevice *device;
  GvcMixerStream *stream = NULL;

  device = cc_device_combo_box_get_device (self->input_device_combo_box);
  cc_profile_combo_box_set_device (self->input_profile_combo_box, self->mixer_control, device);
  gtk_widget_set_visible (GTK_WIDGET (self->input_profile_row),
                          cc_profile_combo_box_get_profile_count (self->input_profile_combo_box) > 1);

  if (device != NULL)
    stream = gvc_mixer_control_get_stream_from_device (self->mixer_control, device);

  cc_volume_slider_set_stream (self->input_volume_slider, stream, CC_STREAM_TYPE_INPUT);
  cc_level_bar_set_stream (self->input_level_bar, stream, CC_STREAM_TYPE_INPUT);

  if (device != NULL)
    gvc_mixer_control_change_input (self->mixer_control, device);
}

static void
test_output_configuration_button_clicked_cb (CcSoundPanel *self)
{
  GvcMixerUIDevice *device;
  GvcMixerStream *stream = NULL;
  CcOutputTestDialog *dialog;

  device = cc_device_combo_box_get_device (self->output_device_combo_box);
  if (device != NULL)
    stream = gvc_mixer_control_get_stream_from_device (self->mixer_control, device);

  dialog = cc_output_test_dialog_new (device, stream);
  gtk_window_set_transient_for (GTK_WINDOW (dialog), GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (self))));
  gtk_dialog_run (GTK_DIALOG (dialog));
  gtk_widget_destroy (GTK_WIDGET (dialog));
}

static const char *
cc_sound_panel_get_help_uri (CcPanel *panel)
{
  return "help:gnome-help/media#sound";
}

static void
cc_sound_panel_finalize (GObject *object)
{
  CcSoundPanel *panel = CC_SOUND_PANEL (object);

  g_clear_object (&panel->mixer_control);
  g_clear_object (&panel->sound_settings);

  G_OBJECT_CLASS (cc_sound_panel_parent_class)->finalize (object);
}

static void
cc_sound_panel_class_init (CcSoundPanelClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  CcPanelClass *panel_class = CC_PANEL_CLASS (klass);

  panel_class->get_help_uri = cc_sound_panel_get_help_uri;

  object_class->finalize = cc_sound_panel_finalize;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/sound/cc-sound-panel.ui");

  gtk_widget_class_bind_template_child (widget_class, CcSoundPanel, balance_slider);
  gtk_widget_class_bind_template_child (widget_class, CcSoundPanel, fade_row);
  gtk_widget_class_bind_template_child (widget_class, CcSoundPanel, fade_slider);
  gtk_widget_class_bind_template_child (widget_class, CcSoundPanel, input_device_combo_box);
  gtk_widget_class_bind_template_child (widget_class, CcSoundPanel, input_level_bar);
  gtk_widget_class_bind_template_child (widget_class, CcSoundPanel, input_list_box);
  gtk_widget_class_bind_template_child (widget_class, CcSoundPanel, input_profile_combo_box);
  gtk_widget_class_bind_template_child (widget_class, CcSoundPanel, input_profile_row);
  gtk_widget_class_bind_template_child (widget_class, CcSoundPanel, input_volume_slider);
  gtk_widget_class_bind_template_child (widget_class, CcSoundPanel, label_size_group);
  gtk_widget_class_bind_template_child (widget_class, CcSoundPanel, main_box);
  gtk_widget_class_bind_template_child (widget_class, CcSoundPanel, output_device_combo_box);
  gtk_widget_class_bind_template_child (widget_class, CcSoundPanel, output_level_bar);
  gtk_widget_class_bind_template_child (widget_class, CcSoundPanel, output_list_box);
  gtk_widget_class_bind_template_child (widget_class, CcSoundPanel, output_profile_combo_box);
  gtk_widget_class_bind_template_child (widget_class, CcSoundPanel, output_profile_row);
  gtk_widget_class_bind_template_child (widget_class, CcSoundPanel, output_volume_slider);
  gtk_widget_class_bind_template_child (widget_class, CcSoundPanel, stream_list_box);
  gtk_widget_class_bind_template_child (widget_class, CcSoundPanel, subwoofer_row);
  gtk_widget_class_bind_template_child (widget_class, CcSoundPanel, subwoofer_slider);

  gtk_widget_class_bind_template_callback (widget_class, input_device_changed_cb);
  gtk_widget_class_bind_template_callback (widget_class, output_device_changed_cb);
  gtk_widget_class_bind_template_callback (widget_class, test_output_configuration_button_clicked_cb);

  g_type_ensure (CC_TYPE_ALERT_CHOOSER);
  g_type_ensure (CC_TYPE_BALANCE_SLIDER);
  g_type_ensure (CC_TYPE_DEVICE_COMBO_BOX);
  g_type_ensure (CC_TYPE_FADE_SLIDER);
  g_type_ensure (CC_TYPE_LEVEL_BAR);
  g_type_ensure (CC_TYPE_PROFILE_COMBO_BOX);
  g_type_ensure (CC_TYPE_STREAM_LIST_BOX);
  g_type_ensure (CC_TYPE_SUBWOOFER_SLIDER);
  g_type_ensure (CC_TYPE_VOLUME_SLIDER);
}

static void
cc_sound_panel_init (CcSoundPanel *self)
{
  g_resources_register (cc_sound_get_resource ());

  gtk_widget_init_template (GTK_WIDGET (self));

  gtk_list_box_set_header_func (self->input_list_box,
                                cc_list_box_update_header_func,
                                NULL, NULL);
  gtk_list_box_set_header_func (self->output_list_box,
                                cc_list_box_update_header_func,
                                NULL, NULL);
  gtk_list_box_set_header_func (GTK_LIST_BOX (self->stream_list_box),
                                cc_list_box_update_header_func,
                                NULL, NULL);

  self->sound_settings = g_settings_new (KEY_SOUNDS_SCHEMA);
  g_signal_connect_object (self->sound_settings,
                           "changed::allow-volume-above-100-percent",
                           G_CALLBACK (allow_amplified_changed_cb),
                           self,
                           G_CONNECT_SWAPPED);
  allow_amplified_changed_cb (self);

  self->mixer_control = gvc_mixer_control_new ("GNOME Settings");
  gvc_mixer_control_open (self->mixer_control);

  cc_stream_list_box_set_mixer_control (self->stream_list_box, self->mixer_control);
  cc_device_combo_box_set_mixer_control (self->input_device_combo_box, self->mixer_control, FALSE);
  cc_device_combo_box_set_mixer_control (self->output_device_combo_box, self->mixer_control, TRUE);
}
