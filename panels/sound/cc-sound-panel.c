/*
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

#include "cc-list-row.h"
#include "cc-alert-chooser-page.h"
#include "cc-balance-slider.h"
#include "cc-device-combo-row.h"
#include "cc-fade-slider.h"
#include "cc-level-bar.h"
#include "cc-output-test-window.h"
#include "cc-profile-combo-row.h"
#include "cc-sound-panel.h"
#include "cc-sound-resources.h"
#include "cc-subwoofer-slider.h"
#include "cc-volume-levels-page.h"
#include "cc-volume-slider.h"

struct _CcSoundPanel
{
  CcPanel            parent_instance;

  AdwPreferencesGroup *output_group;
  CcLevelBar          *output_level_bar;
  CcDeviceComboRow    *output_device_combo_row;
  CcProfileComboRow   *output_profile_combo_row;
  CcVolumeSlider      *output_volume_slider;
  CcBalanceSlider     *balance_slider;
  AdwPreferencesRow   *fade_row;
  CcFadeSlider        *fade_slider;
  AdwPreferencesRow   *subwoofer_row;
  CcSubwooferSlider   *subwoofer_slider;
  AdwPreferencesGroup *output_no_devices_group;
  AdwPreferencesGroup *input_group;
  CcLevelBar          *input_level_bar;
  CcDeviceComboRow    *input_device_combo_row;
  CcProfileComboRow   *input_profile_combo_row;
  CcVolumeSlider      *input_volume_slider;
  AdwPreferencesGroup *input_no_devices_group;
  CcListRow           *alert_sound_row;

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
update_alert_sound_label (CcSoundPanel *self)
{
  const gchar *alert_name = get_selected_alert_display_name ();
  cc_list_row_set_secondary_label (self->alert_sound_row, alert_name);
}

static void
allow_amplified_changed_cb (CcSoundPanel *self)
{
  cc_volume_slider_set_is_amplified (self->output_volume_slider,
                                     g_settings_get_boolean (self->sound_settings, "allow-volume-above-100-percent"));
}

static void
set_output_stream (CcSoundPanel   *self,
                   GvcMixerStream *stream)
{
  GvcChannelMap *map = NULL;
  gboolean can_fade = FALSE, has_lfe = FALSE;

  cc_volume_slider_set_stream (self->output_volume_slider, stream, CC_STREAM_TYPE_OUTPUT);
  cc_level_bar_set_stream (self->output_level_bar, stream);

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
}

static void
set_input_stream (CcSoundPanel   *self,
                  GvcMixerStream *stream)
{
  cc_volume_slider_set_stream (self->input_volume_slider, stream, CC_STREAM_TYPE_INPUT);
  cc_level_bar_set_stream (self->input_level_bar, stream);
}

static void
output_device_update_cb (CcSoundPanel *self,
                         guint         id)
{
  GvcMixerUIDevice *device;
  gboolean has_multi_profiles;
  GvcMixerStream *stream = NULL;

  device = cc_device_combo_row_get_device (self->output_device_combo_row);

  gtk_widget_set_visible (GTK_WIDGET (self->output_group), device != NULL);
  gtk_widget_set_visible (GTK_WIDGET (self->output_no_devices_group), device == NULL);

  cc_profile_combo_row_set_device (self->output_profile_combo_row, self->mixer_control, device);
  has_multi_profiles = (cc_profile_combo_row_get_profile_count (self->output_profile_combo_row) > 1);
  gtk_widget_set_visible (GTK_WIDGET (self->output_profile_combo_row),
                          has_multi_profiles);

  if (device)
    stream = gvc_mixer_control_get_stream_from_device (self->mixer_control, device);

  set_output_stream (self, stream);
}

static void
input_device_update_cb (CcSoundPanel *self,
                        guint         id)
{
  GvcMixerUIDevice *device;
  gboolean has_multi_profiles;
  GvcMixerStream *stream = NULL;

  device = cc_device_combo_row_get_device (self->input_device_combo_row);

  gtk_widget_set_visible (GTK_WIDGET (self->input_group), device != NULL);
  gtk_widget_set_visible (GTK_WIDGET (self->input_no_devices_group), device == NULL);

  cc_profile_combo_row_set_device (self->input_profile_combo_row, self->mixer_control, device);
  has_multi_profiles = (cc_profile_combo_row_get_profile_count (self->input_profile_combo_row) > 1);
  gtk_widget_set_visible (GTK_WIDGET (self->input_profile_combo_row),
                          has_multi_profiles);

  if (device)
    stream = gvc_mixer_control_get_stream_from_device (self->mixer_control, device);

  set_input_stream (self, stream);
}

static void
test_output_configuration_button_clicked_cb (CcSoundPanel *self)
{
  GvcMixerUIDevice *device;
  GvcMixerStream *stream = NULL;
  CcOutputTestWindow *window;

  device = cc_device_combo_row_get_device (self->output_device_combo_row);
  if (device != NULL)
    stream = gvc_mixer_control_get_stream_from_device (self->mixer_control, device);

  window = cc_output_test_window_new (stream);
  adw_dialog_present (ADW_DIALOG (window), GTK_WIDGET (self));
}

static void
volume_levels_activated_cb (CcSoundPanel *self)
{
  CcVolumeLevelsPage *volume_levels;

  volume_levels = cc_volume_levels_page_new (self->mixer_control);

  cc_panel_push_subpage (CC_PANEL (self), ADW_NAVIGATION_PAGE (volume_levels));
}

static void
alert_sound_activated_cb (CcSoundPanel *self)
{
  AdwNavigationPage *page;

  page = ADW_NAVIGATION_PAGE (cc_alert_chooser_page_new ());

  g_signal_connect_object (page, "hiding",
                           G_CALLBACK (update_alert_sound_label),
                           self, G_CONNECT_SWAPPED);

  cc_panel_push_subpage (CC_PANEL (self), page);
}

static const char *
cc_sound_panel_get_help_uri (CcPanel *panel)
{
  return "help:gnome-help/media#sound";
}

static void
cc_sound_panel_finalize (GObject *object)
{
  CcSoundPanel *self = CC_SOUND_PANEL (object);

  g_clear_object (&self->mixer_control);
  g_clear_object (&self->sound_settings);

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

  gtk_widget_class_bind_template_child (widget_class, CcSoundPanel, output_group);
  gtk_widget_class_bind_template_child (widget_class, CcSoundPanel, output_level_bar);
  gtk_widget_class_bind_template_child (widget_class, CcSoundPanel, output_device_combo_row);
  gtk_widget_class_bind_template_child (widget_class, CcSoundPanel, output_profile_combo_row);
  gtk_widget_class_bind_template_child (widget_class, CcSoundPanel, output_volume_slider);
  gtk_widget_class_bind_template_child (widget_class, CcSoundPanel, balance_slider);
  gtk_widget_class_bind_template_child (widget_class, CcSoundPanel, fade_row);
  gtk_widget_class_bind_template_child (widget_class, CcSoundPanel, fade_slider);
  gtk_widget_class_bind_template_child (widget_class, CcSoundPanel, subwoofer_row);
  gtk_widget_class_bind_template_child (widget_class, CcSoundPanel, subwoofer_slider);
  gtk_widget_class_bind_template_child (widget_class, CcSoundPanel, output_no_devices_group);
  gtk_widget_class_bind_template_child (widget_class, CcSoundPanel, input_group);
  gtk_widget_class_bind_template_child (widget_class, CcSoundPanel, input_level_bar);
  gtk_widget_class_bind_template_child (widget_class, CcSoundPanel, input_device_combo_row);
  gtk_widget_class_bind_template_child (widget_class, CcSoundPanel, input_profile_combo_row);
  gtk_widget_class_bind_template_child (widget_class, CcSoundPanel, input_volume_slider);
  gtk_widget_class_bind_template_child (widget_class, CcSoundPanel, input_no_devices_group);
  gtk_widget_class_bind_template_child (widget_class, CcSoundPanel, alert_sound_row);

  gtk_widget_class_bind_template_callback (widget_class, test_output_configuration_button_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, volume_levels_activated_cb);
  gtk_widget_class_bind_template_callback (widget_class, alert_sound_activated_cb);

  g_type_ensure (CC_TYPE_BALANCE_SLIDER);
  g_type_ensure (CC_TYPE_DEVICE_COMBO_ROW);
  g_type_ensure (CC_TYPE_FADE_SLIDER);
  g_type_ensure (CC_TYPE_LEVEL_BAR);
  g_type_ensure (CC_TYPE_PROFILE_COMBO_ROW);
  g_type_ensure (CC_TYPE_SUBWOOFER_SLIDER);
  g_type_ensure (CC_TYPE_VOLUME_SLIDER);
  g_type_ensure (CC_TYPE_LIST_ROW);
}

static void
cc_sound_panel_init (CcSoundPanel *self)
{
  g_resources_register (cc_sound_get_resource ());

  gtk_widget_init_template (GTK_WIDGET (self));

  self->sound_settings = g_settings_new (KEY_SOUNDS_SCHEMA);
  g_signal_connect_object (self->sound_settings,
                           "changed::allow-volume-above-100-percent",
                           G_CALLBACK (allow_amplified_changed_cb),
                           self,
                           G_CONNECT_SWAPPED);
  allow_amplified_changed_cb (self);

  self->mixer_control = gvc_mixer_control_new ("GNOME Settings");

  cc_volume_slider_set_mixer_control (self->input_volume_slider, self->mixer_control);
  cc_volume_slider_set_mixer_control (self->output_volume_slider, self->mixer_control);
  cc_subwoofer_slider_set_mixer_control (self->subwoofer_slider, self->mixer_control);
  cc_device_combo_row_set_mixer_control (self->input_device_combo_row, self->mixer_control, FALSE);
  cc_device_combo_row_set_mixer_control (self->output_device_combo_row, self->mixer_control, TRUE);

  g_signal_connect_object (self->mixer_control,
                           "active-input-update",
                           G_CALLBACK (input_device_update_cb),
                           self,
                           G_CONNECT_SWAPPED);
  g_signal_connect_object (self->mixer_control,
                           "active-output-update",
                           G_CALLBACK (output_device_update_cb),
                           self,
                           G_CONNECT_SWAPPED);

  gvc_mixer_control_open (self->mixer_control);

  update_alert_sound_label (self);
}
