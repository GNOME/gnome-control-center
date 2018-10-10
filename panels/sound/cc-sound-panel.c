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
#include <gvc-mixer-sink.h>
#include <gvc-mixer-source.h>

#include "list-box-helper.h"
#include "cc-input-device-combo-box.h"
#include "cc-input-test-dialog.h"
#include "cc-output-device-combo-box.h"
#include "cc-output-test-dialog.h"
#include "cc-sound-panel.h"
#include "cc-sound-resources.h"
#include "cc-stream-row.h"
#include "cc-volume-slider.h"
#include "gvc-mixer-dialog.h"
#include "gvc-speaker-test.h"

struct _CcSoundPanel {
  CcPanel                 parent_instance;

  CcInputDeviceComboBox  *input_device_combobox;
  GtkListBox             *input_listbox;
  CcVolumeSlider         *input_volume_slider;
  GtkSizeGroup           *label_size_group;
  GtkBox                 *main_box;
  GtkListStore           *output_configuration_model;
  CcOutputDeviceComboBox *output_device_combobox;
  GtkListStore           *output_device_model;
  GtkListBox             *output_listbox;
  CcVolumeSlider         *output_volume_slider;
  GtkListBox             *stream_listbox;
  GtkAdjustment          *subwoofer_volume_adjustment;

  GvcMixerControl        *mixer_control;
  GvcMixerDialog         *dialog;
  GtkWidget              *connecting_label;
};

CC_PANEL_REGISTER (CcSoundPanel, cc_sound_panel)

enum {
  PROP_0,
  PROP_PARAMETERS
};

static void
output_device_changed_cb (CcSoundPanel *self)
{
  GvcMixerUIDevice *device;
  GvcMixerStream *stream = NULL;

  device = cc_output_device_combo_box_get_device (self->output_device_combobox);
  if (device != NULL)
    stream = gvc_mixer_control_get_stream_from_device (self->mixer_control, device);

  cc_volume_slider_set_stream (self->output_volume_slider, stream);
}

static void
input_device_changed_cb (CcSoundPanel *self)
{
  GvcMixerUIDevice *device;
  GvcMixerStream *stream = NULL;

  device = cc_input_device_combo_box_get_device (self->input_device_combobox);
  if (device != NULL)
    stream = gvc_mixer_control_get_stream_from_device (self->mixer_control, device);

  cc_volume_slider_set_stream (self->input_volume_slider, stream);
}

static void
stream_added_cb (CcSoundPanel *self,
                 guint         id)
{
  GvcMixerStream *stream;
  const gchar *app_id;
  CcStreamRow *row;

  stream = gvc_mixer_control_lookup_stream_id (self->mixer_control, id);
  if (stream == NULL)
    return;

  app_id = gvc_mixer_stream_get_application_id (stream);

  /* Skip mater volume controls */
  if (g_strcmp0 (app_id, "org.gnome.VolumeControl") == 0 ||
      g_strcmp0 (app_id, "org.PulseAudio.pavucontrol") == 0)
    return;

  /* Skip streams that aren't volume controls */
  if (GVC_IS_MIXER_SOURCE (stream) ||
      GVC_IS_MIXER_SINK (stream) ||
      gvc_mixer_stream_is_virtual (stream) ||
      gvc_mixer_stream_is_event_stream (stream))
    return;

  row = cc_stream_row_new (self->label_size_group, stream);
  gtk_widget_show (GTK_WIDGET (row));
  gtk_container_add (GTK_CONTAINER (self->stream_listbox), GTK_WIDGET (row));
}

static void
stream_removed_cb (CcSoundPanel *self,
                   guint         id)
{
  // FIXME
}

static void
test_output_configuration_button_clicked_cb (CcSoundPanel *self)
{
  CcOutputTestDialog *dialog;

  dialog = cc_output_test_dialog_new ();
  gtk_window_set_transient_for (GTK_WINDOW (dialog), GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (self))));
  gtk_dialog_run (GTK_DIALOG (dialog));
  gtk_widget_destroy (GTK_WIDGET (dialog));
}

static void
test_input_device_button_clicked_cb (CcSoundPanel *self)
{
  CcInputTestDialog *dialog;

  dialog = cc_input_test_dialog_new ();
  gtk_window_set_transient_for (GTK_WINDOW (dialog), GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (self))));
  gtk_dialog_run (GTK_DIALOG (dialog));
  gtk_widget_destroy (GTK_WIDGET (dialog));
}

static void
cc_sound_panel_set_property (GObject      *object,
                             guint         property_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  CcSoundPanel *self = CC_SOUND_PANEL (object);

  switch (property_id) {
  case PROP_PARAMETERS: {
    GVariant *parameters;

    parameters = g_value_get_variant (value);
    if (parameters && g_variant_n_children (parameters) > 0) {
      g_autoptr(GVariant) v = NULL;
      g_variant_get_child (parameters, 0, "v", &v);
      gvc_mixer_dialog_set_page (self->dialog, g_variant_get_string (v, NULL));
    }
    break;
  }
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }
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

  panel->dialog = NULL;
  panel->connecting_label = NULL;
  g_clear_object (&panel->mixer_control);

  G_OBJECT_CLASS (cc_sound_panel_parent_class)->finalize (object);
}

static void
cc_sound_panel_class_init (CcSoundPanelClass *klass)
{
  GObjectClass   *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  CcPanelClass   *panel_class = CC_PANEL_CLASS (klass);

  panel_class->get_help_uri = cc_sound_panel_get_help_uri;

  object_class->finalize = cc_sound_panel_finalize;
  object_class->set_property = cc_sound_panel_set_property;

  g_object_class_override_property (object_class, PROP_PARAMETERS, "parameters");

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/sound/cc-sound-panel.ui");

  gtk_widget_class_bind_template_child (widget_class, CcSoundPanel, input_device_combobox);
  gtk_widget_class_bind_template_child (widget_class, CcSoundPanel, input_listbox);
  gtk_widget_class_bind_template_child (widget_class, CcSoundPanel, input_volume_slider);
  gtk_widget_class_bind_template_child (widget_class, CcSoundPanel, label_size_group);
  gtk_widget_class_bind_template_child (widget_class, CcSoundPanel, main_box);
  gtk_widget_class_bind_template_child (widget_class, CcSoundPanel, output_configuration_model);
  gtk_widget_class_bind_template_child (widget_class, CcSoundPanel, output_device_combobox);
  gtk_widget_class_bind_template_child (widget_class, CcSoundPanel, output_listbox);
  gtk_widget_class_bind_template_child (widget_class, CcSoundPanel, output_volume_slider);
  gtk_widget_class_bind_template_child (widget_class, CcSoundPanel, stream_listbox);
  gtk_widget_class_bind_template_child (widget_class, CcSoundPanel, subwoofer_volume_adjustment);

  gtk_widget_class_bind_template_callback (widget_class, input_device_changed_cb);
  gtk_widget_class_bind_template_callback (widget_class, output_device_changed_cb);
  gtk_widget_class_bind_template_callback (widget_class, test_output_configuration_button_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, test_input_device_button_clicked_cb);
}

static void
cc_sound_panel_init (CcSoundPanel *self)
{
  g_resources_register (cc_sound_get_resource ());

  gtk_widget_init_template (GTK_WIDGET (self));

  gtk_list_box_set_header_func (self->input_listbox,
                                cc_list_box_update_header_func,
                                NULL, NULL);
  gtk_list_box_set_header_func (self->output_listbox,
                                cc_list_box_update_header_func,
                                NULL, NULL);
  gtk_list_box_set_header_func (self->stream_listbox,
                                cc_list_box_update_header_func,
                                NULL, NULL);

  gtk_icon_theme_append_search_path (gtk_icon_theme_get_default (),
                                     ICON_DATA_DIR);
  gtk_window_set_default_icon_name ("multimedia-volume-control");

  self->mixer_control = gvc_mixer_control_new ("GNOME Volume Control Dialog"); // FIXME: Rename?
  gvc_mixer_control_open (self->mixer_control);
  g_signal_connect_object (self->mixer_control,
                           "stream-added",
                           G_CALLBACK (stream_added_cb),
                           self, G_CONNECT_SWAPPED);
  g_signal_connect_object (self->mixer_control,
                           "stream-removed",
                           G_CALLBACK (stream_removed_cb),
                           self, G_CONNECT_SWAPPED);

  cc_input_device_combo_box_set_mixer_control (self->input_device_combobox, self->mixer_control);
  cc_output_device_combo_box_set_mixer_control (self->output_device_combobox, self->mixer_control);

  gtk_adjustment_set_upper (self->subwoofer_volume_adjustment, gvc_mixer_control_get_vol_max_norm (NULL));

  self->dialog = gvc_mixer_dialog_new (self->mixer_control);
  gtk_container_add (GTK_CONTAINER (self->main_box), GTK_WIDGET (self->dialog));
  gtk_widget_show (GTK_WIDGET (self->dialog));
}
