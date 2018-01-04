/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2008 William Jon McCann
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <math.h>

#include <glib.h>
#include <glib/gi18n-lib.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>
#include <pulse/pulseaudio.h>

#include "gvc-channel-bar.h"
#include "gvc-balance-bar.h"
#include "gvc-combo-box.h"
#include "gvc-mixer-control.h"
#include "gvc-mixer-card.h"
#include "gvc-mixer-sink.h"
#include "gvc-mixer-source.h"
#include "gvc-mixer-source-output.h"
#include "gvc-mixer-dialog.h"
#include "gvc-sound-theme-chooser.h"
#include "gvc-level-bar.h"
#include "gvc-speaker-test.h"
#include "gvc-mixer-control-private.h"

#define SCALE_SIZE 128

struct _GvcMixerDialog
{
        GtkBox           parent_instance;

        GvcMixerControl *mixer_control;
        GHashTable      *bars; /* Application and event bars only */
        GtkWidget       *notebook;
        GtkWidget       *output_bar;
        GtkWidget       *input_bar;
        GtkWidget       *input_level_bar;
        GtkWidget       *effects_bar;
        GtkWidget       *output_stream_box;
        GtkWidget       *sound_effects_box;
        GtkWidget       *input_box;
        GtkWidget       *output_box;
        GtkWidget       *applications_box;
        GtkWidget       *no_apps_label;
        GtkWidget       *output_treeview;
        GtkWidget       *output_settings_box;
        GtkWidget       *output_balance_bar;
        GtkWidget       *output_fade_bar;
        GtkWidget       *output_lfe_bar;
        GtkWidget       *output_profile_combo;
        GtkWidget       *input_treeview;
        GtkWidget       *input_profile_combo;
        GtkWidget       *input_settings_box;
        GtkWidget       *sound_theme_chooser;
        GtkWidget       *click_feedback_button;
        GtkWidget       *audible_bell_button;
        GtkWidget       *test_dialog;
        GtkSizeGroup    *size_group;

        gdouble          last_input_peak;
        guint            num_apps;
};

enum {
        NAME_COLUMN,
        DEVICE_COLUMN,
        ACTIVE_COLUMN,
        ID_COLUMN,
        ICON_COLUMN,
        NUM_COLUMNS
};

enum
{
        PROP_0,
        PROP_MIXER_CONTROL
};

static void     gvc_mixer_dialog_class_init (GvcMixerDialogClass *klass);
static void     gvc_mixer_dialog_init       (GvcMixerDialog      *mixer_dialog);
static void     gvc_mixer_dialog_finalize   (GObject             *object);

static void     bar_set_stream              (GvcMixerDialog      *dialog,
                                             GtkWidget           *bar,
                                             GvcMixerStream      *stream);

static void     on_adjustment_value_changed (GtkAdjustment  *adjustment,
                                             GvcMixerDialog *dialog);
static void     on_control_active_output_update (GvcMixerControl *control,
                                                 guint            id,
                                                 GvcMixerDialog  *dialog);

static void     on_control_active_input_update (GvcMixerControl *control,
                                                guint            id,
                                                GvcMixerDialog  *dialog);

static void     on_test_speakers_clicked (GvcComboBox *widget,
                                          gpointer     user_data);


G_DEFINE_TYPE (GvcMixerDialog, gvc_mixer_dialog, GTK_TYPE_BOX)

static void
profile_selection_changed (GvcComboBox    *combo_box,
                           const char     *profile,
                           GvcMixerDialog *dialog)
{
        GvcMixerUIDevice *output;

        g_debug ("profile_selection_changed() to %s", profile);

        output = g_object_get_data (G_OBJECT (combo_box), "uidevice");

        if (output == NULL) {
                g_warning ("Could not find Output for profile combo box");
                return;
        }

        g_debug ("on profile selection changed on output '%s' (origin: %s, id: %i)",
                gvc_mixer_ui_device_get_description (output),
                gvc_mixer_ui_device_get_origin (output),
                gvc_mixer_ui_device_get_id (output));

        if (gvc_mixer_control_change_profile_on_selected_device (dialog->mixer_control, output, profile) == FALSE) {
                g_warning ("Could not change profile on device %s",
                           gvc_mixer_ui_device_get_description (output));
         }
}

static void
update_output_settings (GvcMixerDialog      *dialog,
                        GvcMixerUIDevice    *device)
{
        GvcMixerStream      *stream;
        const GvcChannelMap *map;
        const GList         *profiles;
        GtkAdjustment       *adj;

        g_debug ("Updating output settings");
        if (dialog->output_balance_bar != NULL) {
                gtk_container_remove (GTK_CONTAINER (dialog->output_settings_box),
                                      dialog->output_balance_bar);
                dialog->output_balance_bar = NULL;
        }
        if (dialog->output_fade_bar != NULL) {
                gtk_container_remove (GTK_CONTAINER (dialog->output_settings_box),
                                      dialog->output_fade_bar);
                dialog->output_fade_bar = NULL;
        }
        if (dialog->output_lfe_bar != NULL) {
                gtk_container_remove (GTK_CONTAINER (dialog->output_settings_box),
                                      dialog->output_lfe_bar);
                dialog->output_lfe_bar = NULL;
        }
        if (dialog->output_profile_combo != NULL) {
                gtk_container_remove (GTK_CONTAINER (dialog->output_settings_box),
                                      dialog->output_profile_combo);
                dialog->output_profile_combo = NULL;
        }

        stream = gvc_mixer_control_get_stream_from_device (dialog->mixer_control,
                                                           device);
        if (stream == NULL) {
                g_warning ("Default sink stream not found");
                return;
        }

        gvc_channel_bar_set_base_volume (GVC_CHANNEL_BAR (dialog->output_bar),
                                         gvc_mixer_stream_get_base_volume (stream));
        gvc_channel_bar_set_is_amplified (GVC_CHANNEL_BAR (dialog->output_bar),
                                          gvc_mixer_stream_get_can_decibel (stream));

	/* Update the adjustment in case the previous bar wasn't decibel
	 * capable, and we clipped it */
        adj = GTK_ADJUSTMENT (gvc_channel_bar_get_adjustment (GVC_CHANNEL_BAR (dialog->output_bar)));
	gtk_adjustment_set_value (adj,
				  gvc_mixer_stream_get_volume (stream));

        map = gvc_mixer_stream_get_channel_map (stream);
        if (map == NULL) {
                g_warning ("Default sink stream has no channel map");
                return;
        }

        dialog->output_balance_bar = gvc_balance_bar_new (map, BALANCE_TYPE_RL);
        if (dialog->size_group != NULL) {
                gvc_balance_bar_set_size_group (GVC_BALANCE_BAR (dialog->output_balance_bar),
                                                dialog->size_group,
                                                TRUE);
        }
        gtk_box_pack_start (GTK_BOX (dialog->output_settings_box),
                            dialog->output_balance_bar,
                            FALSE, FALSE, 6);
        gtk_widget_show (dialog->output_balance_bar);

        if (gvc_channel_map_can_fade (map)) {
                dialog->output_fade_bar = gvc_balance_bar_new (map, BALANCE_TYPE_FR);
                if (dialog->size_group != NULL) {
                        gvc_balance_bar_set_size_group (GVC_BALANCE_BAR (dialog->output_fade_bar),
                                                        dialog->size_group,
                                                        TRUE);
                }
                gtk_box_pack_start (GTK_BOX (dialog->output_settings_box),
                                    dialog->output_fade_bar,
                                    FALSE, FALSE, 6);
                gtk_widget_show (dialog->output_fade_bar);
        }

        if (gvc_channel_map_has_lfe (map)) {
                dialog->output_lfe_bar = gvc_balance_bar_new (map, BALANCE_TYPE_LFE);
                if (dialog->size_group != NULL) {
                        gvc_balance_bar_set_size_group (GVC_BALANCE_BAR (dialog->output_lfe_bar),
                                                        dialog->size_group,
                                                        TRUE);
                }
                gtk_box_pack_start (GTK_BOX (dialog->output_settings_box),
                                    dialog->output_lfe_bar,
                                    FALSE, FALSE, 6);
                gtk_widget_show (dialog->output_lfe_bar);
        }

        profiles = gvc_mixer_ui_device_get_profiles (device);
        /* FIXME: How do we make sure the "Test speakers" button is shown
         * even when there are no profiles to choose between? */
        if (TRUE /*g_list_length((GList *) profiles) >= 2 */) {
                const gchar *active_profile;

                dialog->output_profile_combo = gvc_combo_box_new (_("_Profile:"));

                g_object_set (G_OBJECT (dialog->output_profile_combo), "button-label", _("_Test Speakers"), NULL);
                g_object_set (G_OBJECT (dialog->output_profile_combo),
                              "show-button", TRUE, NULL);
                g_signal_connect (G_OBJECT (dialog->output_profile_combo), "button-clicked",
                                  G_CALLBACK (on_test_speakers_clicked), dialog);

                if (profiles)
                        gvc_combo_box_set_profiles (GVC_COMBO_BOX (dialog->output_profile_combo),
                                                    profiles);
                gtk_box_pack_start (GTK_BOX (dialog->output_settings_box),
                                    dialog->output_profile_combo,
                                    TRUE, FALSE, 6);

                if (dialog->size_group != NULL) {
                        gvc_combo_box_set_size_group (GVC_COMBO_BOX (dialog->output_profile_combo),
                                                      dialog->size_group, FALSE);
                }

                active_profile = gvc_mixer_ui_device_get_active_profile (device);
                if (active_profile)
                        gvc_combo_box_set_active (GVC_COMBO_BOX (dialog->output_profile_combo), active_profile);

                g_object_set_data (G_OBJECT (dialog->output_profile_combo),
                                   "uidevice",
                                   device);
                if (g_list_length((GList *) profiles))
                        g_signal_connect (G_OBJECT (dialog->output_profile_combo), "changed",
                                          G_CALLBACK (profile_selection_changed), dialog);

                gtk_widget_show (dialog->output_profile_combo);
        }

        /* FIXME: We could make this into a "No settings" label instead */
        gtk_widget_set_sensitive (dialog->output_balance_bar, gvc_channel_map_can_balance (map));
}

#define DECAY_STEP .15

static void
update_input_peak (GvcMixerDialog *dialog,
                   gdouble         v)
{
        GtkAdjustment *adj;

        if (dialog->last_input_peak >= DECAY_STEP) {
                if (v < dialog->last_input_peak - DECAY_STEP) {
                        v = dialog->last_input_peak - DECAY_STEP;
                }
        }

        dialog->last_input_peak = v;

        adj = gvc_level_bar_get_peak_adjustment (GVC_LEVEL_BAR (dialog->input_level_bar));
        if (v >= 0) {
                gtk_adjustment_set_value (adj, v);
        } else {
                gtk_adjustment_set_value (adj, 0.0);
        }
}

static void
update_input_meter (GvcMixerDialog *dialog,
                    uint32_t        source_index,
                    uint32_t        sink_input_idx,
                    double          v)
{
        update_input_peak (dialog, v);
}

static void
on_monitor_suspended_callback (pa_stream *s,
                               void      *userdata)
{
        GvcMixerDialog *dialog;

        dialog = userdata;

        if (pa_stream_is_suspended (s)) {
                g_debug ("Stream suspended");
                update_input_meter (dialog,
                                    pa_stream_get_device_index (s),
                                    PA_INVALID_INDEX,
                                    -1);
        }
}

static void
on_monitor_read_callback (pa_stream *s,
                          size_t     length,
                          void      *userdata)
{
        GvcMixerDialog *dialog;
        const void     *data;
        double          v;

        dialog = userdata;

        if (pa_stream_peek (s, &data, &length) < 0) {
                g_warning ("Failed to read data from stream");
                return;
        }

        if (!data) {
                pa_stream_drop (s);
                return;
        }

        assert (length > 0);
        assert (length % sizeof (float) == 0);

        v = ((const float *) data)[length / sizeof (float) -1];

        pa_stream_drop (s);

        if (v < 0) {
                v = 0;
        }
        if (v > 1) {
                v = 1;
        }

        update_input_meter (dialog,
                            pa_stream_get_device_index (s),
                            pa_stream_get_monitor_stream (s),
                            v);
}

static void
create_monitor_stream_for_source (GvcMixerDialog *dialog,
                                  GvcMixerStream *stream)
{
        pa_stream     *s;
        char           t[16];
        pa_buffer_attr attr;
        pa_sample_spec ss;
        pa_context    *context;
        int            res;
        pa_proplist   *proplist;
        gboolean       has_monitor;

        if (stream == NULL) {
                return;
        }
        has_monitor = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (stream), "has-monitor"));
        if (has_monitor != FALSE) {
                return;
        }

        g_debug ("Create monitor for %u",
                 gvc_mixer_stream_get_index (stream));

        context = gvc_mixer_control_get_pa_context (dialog->mixer_control);

        if (pa_context_get_server_protocol_version (context) < 13) {
                return;
        }

        ss.channels = 1;
        ss.format = PA_SAMPLE_FLOAT32;
        ss.rate = 25;

        memset (&attr, 0, sizeof (attr));
        attr.fragsize = sizeof (float);
        attr.maxlength = (uint32_t) -1;

        snprintf (t, sizeof (t), "%u", gvc_mixer_stream_get_index (stream));

        proplist = pa_proplist_new ();
        pa_proplist_sets (proplist, PA_PROP_APPLICATION_ID, "org.gnome.VolumeControl");
        s = pa_stream_new_with_proplist (context, _("Peak detect"), &ss, NULL, proplist);
        pa_proplist_free (proplist);
        if (s == NULL) {
                g_warning ("Failed to create monitoring stream");
                return;
        }

        pa_stream_set_read_callback (s, on_monitor_read_callback, dialog);
        pa_stream_set_suspended_callback (s, on_monitor_suspended_callback, dialog);

        res = pa_stream_connect_record (s,
                                        t,
                                        &attr,
                                        (pa_stream_flags_t) (PA_STREAM_DONT_MOVE
                                                             |PA_STREAM_PEAK_DETECT
                                                             |PA_STREAM_ADJUST_LATENCY));
        if (res < 0) {
                g_warning ("Failed to connect monitoring stream");
                pa_stream_unref (s);
        } else {
                g_object_set_data (G_OBJECT (stream), "has-monitor", GINT_TO_POINTER (TRUE));
                g_object_set_data (G_OBJECT (dialog->input_level_bar), "pa_stream", s);
                g_object_set_data (G_OBJECT (dialog->input_level_bar), "stream", stream);
        }
}

static void
stop_monitor_stream_for_source (GvcMixerDialog *dialog)
{
        pa_stream      *s;
        pa_context     *context;
        int             res;
        GvcMixerStream *stream;

        s = g_object_get_data (G_OBJECT (dialog->input_level_bar), "pa_stream");
        if (s == NULL)
                return;
        stream = g_object_get_data (G_OBJECT (dialog->input_level_bar), "stream");
        g_assert (stream != NULL);

        g_debug ("Stopping monitor for %u", pa_stream_get_index (s));

        context = gvc_mixer_control_get_pa_context (dialog->mixer_control);

        if (pa_context_get_server_protocol_version (context) < 13) {
                return;
        }

        res = pa_stream_disconnect (s);
        if (res == 0)
                g_object_set_data (G_OBJECT (stream), "has-monitor", GINT_TO_POINTER (FALSE));
        g_object_set_data (G_OBJECT (dialog->input_level_bar), "pa_stream", NULL);
        g_object_set_data (G_OBJECT (dialog->input_level_bar), "stream", NULL);
}

static void
update_input_settings (GvcMixerDialog   *dialog,
                       GvcMixerUIDevice *device)
{
        GvcMixerStream *stream;
        const GList    *profiles;
        GtkAdjustment  *adj;

        g_debug ("Updating input settings");

        stop_monitor_stream_for_source (dialog);

        if (dialog->input_profile_combo != NULL) {
                gtk_container_remove (GTK_CONTAINER (dialog->input_settings_box),
                                      dialog->input_profile_combo);
                dialog->input_profile_combo = NULL;
        }

        stream = gvc_mixer_control_get_stream_from_device (dialog->mixer_control,
                                                           device);
        if (stream == NULL) {
                g_debug ("Default source stream not found");
                return;
        }

        gvc_channel_bar_set_base_volume (GVC_CHANNEL_BAR (dialog->input_bar),
                                         gvc_mixer_stream_get_base_volume (stream));
        gvc_channel_bar_set_is_amplified (GVC_CHANNEL_BAR (dialog->input_bar),
                                          gvc_mixer_stream_get_can_decibel (stream));

	/* Update the adjustment in case the previous bar wasn't decibel
	 * capable, and we clipped it */
        adj = GTK_ADJUSTMENT (gvc_channel_bar_get_adjustment (GVC_CHANNEL_BAR (dialog->input_bar)));
	gtk_adjustment_set_value (adj,
				  gvc_mixer_stream_get_volume (stream));

        profiles = gvc_mixer_ui_device_get_profiles (device);
        if (g_list_length ((GList *) profiles) >= 2) {
                const gchar *active_profile;

                dialog->input_profile_combo = gvc_combo_box_new (_("_Profile:"));
                gvc_combo_box_set_profiles (GVC_COMBO_BOX (dialog->input_profile_combo),
                                            profiles);

                gtk_box_pack_start (GTK_BOX (dialog->input_settings_box),
                                    dialog->input_profile_combo,
                                    TRUE, TRUE, 0);

                if (dialog->size_group != NULL) {
                        gvc_combo_box_set_size_group (GVC_COMBO_BOX (dialog->input_profile_combo),
                                                      dialog->size_group, FALSE);
                }

                active_profile = gvc_mixer_ui_device_get_active_profile (device);
                if (active_profile)
                        gvc_combo_box_set_active (GVC_COMBO_BOX (dialog->input_profile_combo), active_profile);

                g_object_set_data (G_OBJECT (dialog->input_profile_combo),
                                   "uidevice",
                                   device);
                g_signal_connect (G_OBJECT (dialog->input_profile_combo), "changed",
                                  G_CALLBACK (profile_selection_changed), dialog);

                gtk_widget_show (dialog->input_profile_combo);
        }

        create_monitor_stream_for_source (dialog, stream);
}

static void
gvc_mixer_dialog_set_mixer_control (GvcMixerDialog  *dialog,
                                    GvcMixerControl *control)
{
        g_return_if_fail (GVC_MIXER_DIALOG (dialog));
        g_return_if_fail (GVC_IS_MIXER_CONTROL (control));

        g_object_ref (control);

        if (dialog->mixer_control != NULL) {
                g_signal_handlers_disconnect_by_func (dialog->mixer_control,
                                                      G_CALLBACK (on_control_active_input_update),
                                                      dialog);
                g_signal_handlers_disconnect_by_func (dialog->mixer_control,
                                                      G_CALLBACK (on_control_active_output_update),
                                                      dialog);
                g_object_unref (dialog->mixer_control);
        }

        dialog->mixer_control = control;

        /* FIXME: Why are some mixer_control signals connected here,
         * and others in the dialog constructor? (And similar for disconnect) */
        g_signal_connect (dialog->mixer_control,
                          "active-input-update",
                          G_CALLBACK (on_control_active_input_update),
                          dialog);
        g_signal_connect (dialog->mixer_control,
                          "active-output-update",
                          G_CALLBACK (on_control_active_output_update),
                          dialog);

        g_object_notify (G_OBJECT (dialog), "mixer-control");
}

static GvcMixerControl *
gvc_mixer_dialog_get_mixer_control (GvcMixerDialog *dialog)
{
        g_return_val_if_fail (GVC_IS_MIXER_DIALOG (dialog), NULL);

        return dialog->mixer_control;
}

static void
gvc_mixer_dialog_set_property (GObject       *object,
                               guint          prop_id,
                               const GValue  *value,
                               GParamSpec    *pspec)
{
        GvcMixerDialog *self = GVC_MIXER_DIALOG (object);

        switch (prop_id) {
        case PROP_MIXER_CONTROL:
                gvc_mixer_dialog_set_mixer_control (self, g_value_get_object (value));
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
                break;
        }
}

static void
gvc_mixer_dialog_get_property (GObject     *object,
                               guint        prop_id,
                               GValue      *value,
                               GParamSpec  *pspec)
{
        GvcMixerDialog *self = GVC_MIXER_DIALOG (object);

        switch (prop_id) {
        case PROP_MIXER_CONTROL:
                g_value_set_object (value, gvc_mixer_dialog_get_mixer_control (self));
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
                break;
        }
}

static void
on_adjustment_value_changed (GtkAdjustment  *adjustment,
                             GvcMixerDialog *dialog)
{
        GvcMixerStream *stream;

        stream = g_object_get_data (G_OBJECT (adjustment), "gvc-mixer-dialog-stream");
        if (stream != NULL) {
                GObject *bar;
                gdouble volume, rounded;
                char *name;

                volume = gtk_adjustment_get_value (adjustment);
                rounded = round (volume);

                bar = g_object_get_data (G_OBJECT (adjustment), "gvc-mixer-dialog-bar");
                g_object_get (bar, "name", &name, NULL);
                g_debug ("Setting stream volume %lf (rounded: %lf) for bar '%s'", volume, rounded, name);
                g_free (name);

                /* FIXME would need to do that in the balance bar really... */
                /* Make sure we do not unmute muted streams, there's a button for that */
                if (volume == 0.0)
                        gvc_mixer_stream_set_is_muted (stream, TRUE);
                /* Only push the volume if it's actually changed */
                if (gvc_mixer_stream_set_volume (stream, (pa_volume_t) rounded) != FALSE)
                        gvc_mixer_stream_push_volume (stream);
        }
}

static void
on_bar_is_muted_notify (GObject        *object,
                        GParamSpec     *pspec,
                        GvcMixerDialog *dialog)
{
        gboolean        is_muted;
        GvcMixerStream *stream;

        is_muted = gvc_channel_bar_get_is_muted (GVC_CHANNEL_BAR (object));

        stream = g_object_get_data (object, "gvc-mixer-dialog-stream");
        if (stream != NULL) {
                gvc_mixer_stream_change_is_muted (stream, is_muted);
        } else {
                char *name;
                g_object_get (object, "name", &name, NULL);
                g_warning ("Unable to find stream for bar '%s'", name);
                g_free (name);
        }
}

static GtkWidget *
lookup_bar_for_stream (GvcMixerDialog *dialog,
                       GvcMixerStream *stream)
{
        GtkWidget *bar;

        bar = g_hash_table_lookup (dialog->bars, GUINT_TO_POINTER (gvc_mixer_stream_get_id (stream)));
        if (bar)
                return bar;

        if (g_object_get_data (G_OBJECT (dialog->output_bar), "gvc-mixer-dialog-stream") == stream)
                return dialog->output_bar;
        if (g_object_get_data (G_OBJECT (dialog->input_bar), "gvc-mixer-dialog-stream") == stream)
                return dialog->input_bar;

        return NULL;
}

static void
on_stream_volume_notify (GObject        *object,
                         GParamSpec     *pspec,
                         GvcMixerDialog *dialog)
{
        GvcMixerStream *stream;
        GtkWidget      *bar;
        GtkAdjustment  *adj;

        stream = GVC_MIXER_STREAM (object);

        bar = lookup_bar_for_stream (dialog, stream);

        if (bar == NULL) {
                g_warning ("Unable to find bar for stream %s in on_stream_volume_notify()",
                           gvc_mixer_stream_get_name (stream));
                return;
        }

        adj = GTK_ADJUSTMENT (gvc_channel_bar_get_adjustment (GVC_CHANNEL_BAR (bar)));

        g_signal_handlers_block_by_func (adj,
                                         on_adjustment_value_changed,
                                         dialog);

        gtk_adjustment_set_value (adj,
                                  gvc_mixer_stream_get_volume (stream));

        g_signal_handlers_unblock_by_func (adj,
                                           on_adjustment_value_changed,
                                           dialog);
}

static void
on_stream_is_muted_notify (GObject        *object,
                           GParamSpec     *pspec,
                           GvcMixerDialog *dialog)
{
        GvcMixerStream *stream;
        GtkWidget      *bar;
        gboolean        is_muted;

        stream = GVC_MIXER_STREAM (object);
        bar = lookup_bar_for_stream (dialog, stream);

        if (bar == NULL) {
                g_warning ("Unable to find bar for stream %s in on_stream_is_muted_notify()",
                           gvc_mixer_stream_get_name (stream));
                return;
        }

        is_muted = gvc_mixer_stream_get_is_muted (stream);
        gvc_channel_bar_set_is_muted (GVC_CHANNEL_BAR (bar),
                                      is_muted);

        if (stream == gvc_mixer_control_get_default_sink (dialog->mixer_control)) {
                gtk_widget_set_sensitive (dialog->applications_box,
                                          !is_muted);
        }

}

static void
save_bar_for_stream (GvcMixerDialog *dialog,
                     GvcMixerStream *stream,
                     GtkWidget      *bar)
{
        g_hash_table_insert (dialog->bars,
                             GUINT_TO_POINTER (gvc_mixer_stream_get_id (stream)),
                             bar);
}

static GtkWidget *
create_bar (GvcMixerDialog *dialog,
            gboolean        add_to_size_group,
            gboolean        symmetric)
{
        GtkWidget *bar;

        bar = gvc_channel_bar_new ();
        gtk_widget_set_sensitive (bar, FALSE);
        if (add_to_size_group && dialog->size_group != NULL) {
                gvc_channel_bar_set_size_group (GVC_CHANNEL_BAR (bar),
                                                dialog->size_group,
                                                symmetric);
        }
        gvc_channel_bar_set_show_mute (GVC_CHANNEL_BAR (bar),
                                       TRUE);
        g_signal_connect (bar,
                          "notify::is-muted",
                          G_CALLBACK (on_bar_is_muted_notify),
                          dialog);
        return bar;
}

static GtkWidget *
create_app_bar (GvcMixerDialog *dialog,
                const char     *name,
                const char     *icon_name)
{
        GtkWidget *bar;

        bar = create_bar (dialog, FALSE, FALSE);
        gvc_channel_bar_set_ellipsize (GVC_CHANNEL_BAR (bar), TRUE);
        gvc_channel_bar_set_icon_name (GVC_CHANNEL_BAR (bar), icon_name);
        if (name == NULL || strchr (name, '_') == NULL) {
                gvc_channel_bar_set_name (GVC_CHANNEL_BAR (bar), name);
        } else {
                char **tokens, *escaped;

                tokens = g_strsplit (name, "_", -1);
                escaped = g_strjoinv ("__", tokens);
                g_strfreev (tokens);
                gvc_channel_bar_set_name (GVC_CHANNEL_BAR (bar), escaped);
                g_free (escaped);
        }

        return bar;
}

/* active_input_update
 * Handle input update change from the backend (control).
 * Trust the backend whole-heartedly to deliver the correct input. */
static void
active_input_update (GvcMixerDialog *dialog,
                     GvcMixerUIDevice *active_input)
{
        /* First make sure the correct UI device is selected. */
        GtkTreeModel   *model;
        GtkTreeIter     iter;
        GvcMixerStream *stream;

        g_debug ("active_input_update device id = %i",
                 gvc_mixer_ui_device_get_id (active_input));

        model = gtk_tree_view_get_model (GTK_TREE_VIEW (dialog->input_treeview));

        if (gtk_tree_model_get_iter_first (model, &iter) == FALSE) {
                g_warning ("No devices in the tree, so cannot set the active output");
                return;
        }

        do {
                gboolean         is_selected = FALSE;
                gint             id;

                gtk_tree_model_get (model, &iter,
                                    ID_COLUMN, &id,
                                    -1);

                is_selected = id == gvc_mixer_ui_device_get_id (active_input);

                gtk_list_store_set (GTK_LIST_STORE (model),
                                    &iter,
                                    ACTIVE_COLUMN, is_selected,
                                    -1);

                if (is_selected) {
                        GtkTreeSelection *selection;
                        selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (dialog->input_treeview));
                        gtk_tree_selection_select_iter (selection, &iter);
                }
        } while (gtk_tree_model_iter_next (model, &iter));

        stream = gvc_mixer_control_get_stream_from_device (dialog->mixer_control,
                                                           active_input);
        if (stream == NULL) {
                g_warning ("Couldn't find a stream from the active input");
                gtk_widget_set_sensitive (dialog->input_bar, FALSE);
                return;
        }

        bar_set_stream (dialog, dialog->input_bar, stream);
        update_input_settings (dialog, active_input);

}

/* active_output_update
 * Handle output update change from the backend (control).
 * Trust the backend whole heartedly to deliver the correct output. */
static void
active_output_update (GvcMixerDialog   *dialog,
                      GvcMixerUIDevice *active_output)
{
        /* First make sure the correct UI device is selected. */
        GvcMixerStream *stream;
        GtkTreeModel   *model;
        GtkTreeIter     iter;

        g_debug ("active output update device id = %i",
                 gvc_mixer_ui_device_get_id (active_output));

        model = gtk_tree_view_get_model (GTK_TREE_VIEW (dialog->output_treeview));

        if (gtk_tree_model_get_iter_first (model, &iter) == FALSE){
                g_warning ("No devices in the tree, so cannot set the active output");
                return;
        }

        do {
                gboolean         is_selected;
                gint             id;

                gtk_tree_model_get (model, &iter,
                                    ID_COLUMN, &id,
                                    ACTIVE_COLUMN, &is_selected,
                                    -1);

                if (is_selected && id == gvc_mixer_ui_device_get_id (active_output)) {
                        /* XXX: profile change on the same device? */
                        g_debug ("Unneccessary active output update");
                }

                is_selected = id == gvc_mixer_ui_device_get_id (active_output);

                gtk_list_store_set (GTK_LIST_STORE (model),
                                    &iter,
                                    ACTIVE_COLUMN, is_selected,
                                    -1);

                if (is_selected) {
                        GtkTreeSelection *selection;
                        selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (dialog->output_treeview));
                        gtk_tree_selection_select_iter (selection, &iter);
                }
        } while (gtk_tree_model_iter_next (model, &iter));

        stream = gvc_mixer_control_get_stream_from_device (dialog->mixer_control,
                                                           active_output);
        if (stream == NULL) {
                g_warning ("Couldn't find a stream from the active output");
                return;
        }

        bar_set_stream (dialog, dialog->output_bar, stream);
        update_output_settings (dialog, active_output);
}

static void
bar_set_stream (GvcMixerDialog *dialog,
                GtkWidget      *bar,
                GvcMixerStream *stream)
{
        GtkAdjustment  *adj;
        GvcMixerStream *old_stream;

        g_assert (bar != NULL);

        old_stream = g_object_get_data (G_OBJECT (bar), "gvc-mixer-dialog-stream");
        if (old_stream != NULL) {
                char *name;

                g_object_get (bar, "name", &name, NULL);
                g_debug ("Disconnecting old stream '%s' from bar '%s'",
                         gvc_mixer_stream_get_name (old_stream), name);
                g_free (name);

                g_signal_handlers_disconnect_by_func (old_stream, on_stream_is_muted_notify, dialog);
                g_signal_handlers_disconnect_by_func (old_stream, on_stream_volume_notify, dialog);
                g_hash_table_remove (dialog->bars, GUINT_TO_POINTER (gvc_mixer_stream_get_id (old_stream)));
        }

        gtk_widget_set_sensitive (bar, (stream != NULL));

        adj = GTK_ADJUSTMENT (gvc_channel_bar_get_adjustment (GVC_CHANNEL_BAR (bar)));

        g_signal_handlers_disconnect_by_func (adj, on_adjustment_value_changed, dialog);

        g_object_set_data (G_OBJECT (bar), "gvc-mixer-dialog-stream", stream);
        g_object_set_data (G_OBJECT (bar), "gvc-mixer-dialog-stream-id",
                           GUINT_TO_POINTER (gvc_mixer_stream_get_id (stream)));
        g_object_set_data (G_OBJECT (adj), "gvc-mixer-dialog-stream", stream);
        g_object_set_data (G_OBJECT (adj), "gvc-mixer-dialog-bar", bar);

        if (stream != NULL) {
                gboolean is_muted;

                is_muted = gvc_mixer_stream_get_is_muted (stream);
                gvc_channel_bar_set_is_muted (GVC_CHANNEL_BAR (bar), is_muted);

                gtk_adjustment_set_value (adj,
                                          gvc_mixer_stream_get_volume (stream));

                g_signal_connect (stream,
                                  "notify::is-muted",
                                  G_CALLBACK (on_stream_is_muted_notify),
                                  dialog);
                g_signal_connect (stream,
                                  "notify::volume",
                                  G_CALLBACK (on_stream_volume_notify),
                                  dialog);
                g_signal_connect (adj,
                                  "value-changed",
                                  G_CALLBACK (on_adjustment_value_changed),
                                  dialog);
        }
}

static void
add_stream (GvcMixerDialog *dialog,
            GvcMixerStream *stream)
{
        GtkWidget      *bar;
        GvcMixerStream *old_stream;

        bar = NULL;

        if (GVC_IS_MIXER_SOURCE (stream) || GVC_IS_MIXER_SINK (stream))
                return;
        else if (stream == gvc_mixer_control_get_event_sink_input (dialog->mixer_control)) {
                bar = dialog->effects_bar;
                g_debug ("Adding effects stream");
        } else {
                /* Must be an application stream */
                const char *name;
                name = gvc_mixer_stream_get_name (stream);
                g_debug ("Add bar for application stream : %s", name);

                bar = create_app_bar (dialog, name,
                                      gvc_mixer_stream_get_icon_name (stream));
                gtk_box_pack_start (GTK_BOX (dialog->applications_box), bar, FALSE, FALSE, 12);
                dialog->num_apps++;
                gtk_widget_hide (dialog->no_apps_label);
        }

        /* We should have a bar by now. */
        g_assert (bar != NULL);

        if (bar != NULL) {
                old_stream = g_object_get_data (G_OBJECT (bar), "gvc-mixer-dialog-stream");
                if (old_stream != NULL) {
                        char *name;

                        g_object_get (bar, "name", &name, NULL);
                        g_debug ("Disconnecting old stream '%s' from bar '%s'",
                                 gvc_mixer_stream_get_name (old_stream), name);
                        g_free (name);

                        g_signal_handlers_disconnect_by_func (old_stream, on_stream_is_muted_notify, dialog);
                        g_signal_handlers_disconnect_by_func (old_stream, on_stream_volume_notify, dialog);
                        g_hash_table_remove (dialog->bars, GUINT_TO_POINTER (gvc_mixer_stream_get_id (old_stream)));
                }
                save_bar_for_stream (dialog, stream, bar);
                bar_set_stream (dialog, bar, stream);
                gtk_widget_show (bar);
        }
}

static void
on_control_stream_added (GvcMixerControl *control,
                         guint            id,
                         GvcMixerDialog  *dialog)
{
        GvcMixerStream *stream;
        const char     *app_id;

        stream = gvc_mixer_control_lookup_stream_id (control, id);
        if (stream == NULL)
                return;

        app_id = gvc_mixer_stream_get_application_id (stream);

        if (stream == gvc_mixer_control_get_event_sink_input (dialog->mixer_control) ||
            (GVC_IS_MIXER_SOURCE (stream) == FALSE &&
             GVC_IS_MIXER_SINK (stream) == FALSE &&
             gvc_mixer_stream_is_virtual (stream) == FALSE &&
             gvc_mixer_stream_is_event_stream (stream) == FALSE &&
             g_strcmp0 (app_id, "org.gnome.VolumeControl") != 0 &&
             g_strcmp0 (app_id, "org.PulseAudio.pavucontrol") != 0)) {
                GtkWidget      *bar;

                bar = g_hash_table_lookup (dialog->bars, GUINT_TO_POINTER (id));
                if (bar != NULL) {
                        g_debug ("GvcMixerDialog: Stream %u already added", id);
                        return;
                }
                add_stream (dialog, stream);
        }
}

static gboolean
find_item_by_id (GtkTreeModel *model,
                   guint         id,
                   guint         column,
                   GtkTreeIter  *iter)
{
        gboolean found_item;

        found_item = FALSE;

        if (!gtk_tree_model_get_iter_first (model, iter)) {
                return FALSE;
        }

        do {
                guint t_id;

                gtk_tree_model_get (model, iter,
                                    column, &t_id, -1);

                if (id == t_id) {
                        found_item = TRUE;
                }
        } while (!found_item && gtk_tree_model_iter_next (model, iter));

        return found_item;
}

static void
add_input_ui_entry (GvcMixerDialog *dialog,
                    GvcMixerUIDevice *input)
{
        gchar               *final_name;
        gchar               *port_name;
        gchar               *origin;
        gchar               *description;
        gboolean             available;
        gint                 stream_id;
        GtkTreeModel        *model;
        GtkTreeIter          iter;
        GIcon               *icon;

        g_debug ("Add input ui entry with id :%u",
                  gvc_mixer_ui_device_get_id (input));

        g_object_get (G_OBJECT (input),
                     "stream-id", &stream_id,
                     "origin", &origin,
                     "description", &description,
                     "port-name", &port_name,
                     "port-available", &available,
                      NULL);

        if (origin && origin[0] != '\0')
                final_name = g_strdup_printf ("%s - %s", description, origin);
        else
                final_name = g_strdup (description);

        g_free (port_name);
        g_free (origin);
        g_free (description);

        icon = gvc_mixer_ui_device_get_gicon (input);

        if (icon == NULL) {
                GvcMixerStream *stream;
                g_debug ("just detected a network source");
                stream = gvc_mixer_control_get_stream_from_device (dialog->mixer_control, input);
                if (stream == NULL) {
                        g_warning ("tried to add the network source but the stream was null - fail ?!");
                        g_free (final_name);
                        return;
                }
                icon = gvc_mixer_stream_get_gicon (stream);
        }

        model = gtk_tree_view_get_model (GTK_TREE_VIEW (dialog->input_treeview));
        gtk_list_store_append (GTK_LIST_STORE (model), &iter);

        gtk_list_store_set (GTK_LIST_STORE (model),
                            &iter,
                            NAME_COLUMN, final_name,
                            DEVICE_COLUMN, "",
                            ACTIVE_COLUMN, FALSE,
                            ICON_COLUMN, icon,
                            ID_COLUMN, gvc_mixer_ui_device_get_id (input),
                            -1);

        if (icon != NULL)
                g_object_unref (icon);
        g_free (final_name);
}

static void
add_output_ui_entry (GvcMixerDialog   *dialog,
                     GvcMixerUIDevice *output)
{
        gchar         *sink_port_name;
        gchar         *origin;
        gchar         *description;
        gchar         *final_name;
        gboolean       available;
        gint           sink_stream_id;
        GtkTreeModel  *model;
        GtkTreeIter    iter;
        GIcon         *icon;

        g_debug ("Add output ui entry with id :%u",
                  gvc_mixer_ui_device_get_id (output));

        g_object_get (G_OBJECT (output),
                     "stream-id", &sink_stream_id,
                     "origin", &origin,
                     "description", &description,
                     "port-name", &sink_port_name,
                     "port-available", &available,
                      NULL);

        if (origin && origin[0] != '\0')
                final_name = g_strdup_printf ("%s - %s", description, origin);
        else
                final_name = g_strdup (description);

        g_free (sink_port_name);
        g_free (origin);
        g_free (description);

        icon = gvc_mixer_ui_device_get_gicon (output);

        if (icon == NULL) {
                GvcMixerStream *stream;

                g_debug ("just detected a network sink");
                stream = gvc_mixer_control_get_stream_from_device (dialog->mixer_control, output);

                if (stream == NULL) {
                        g_warning ("tried to add the network sink but the stream was null - fail ?!");
                        g_free (final_name);
                        return;
                }
                icon = gvc_mixer_stream_get_gicon (stream);
        }

        model = gtk_tree_view_get_model (GTK_TREE_VIEW (dialog->output_treeview));
        gtk_list_store_append (GTK_LIST_STORE (model), &iter);

        gtk_list_store_set (GTK_LIST_STORE (model),
                            &iter,
                            NAME_COLUMN, final_name,
                            DEVICE_COLUMN, "",
                            ACTIVE_COLUMN, FALSE,
                            ICON_COLUMN, icon,
                            ID_COLUMN, gvc_mixer_ui_device_get_id (output),
                            -1);

        if (icon != NULL)
                g_object_unref (icon);
        g_free (final_name);
}


static void
on_control_active_input_update (GvcMixerControl *control,
                                guint            id,
                                GvcMixerDialog  *dialog)
{
        GvcMixerUIDevice* in = NULL;
        in = gvc_mixer_control_lookup_input_id (control, id);

        if (in == NULL) {
                g_warning ("on_control_active_input_update - tried to fetch an input of id %u but got nothing", id);
                return;
        }
        active_input_update (dialog, in);
}

static void
on_control_active_output_update (GvcMixerControl *control,
                                 guint            id,
                                 GvcMixerDialog  *dialog)
{
        GvcMixerUIDevice* out = NULL;
        out = gvc_mixer_control_lookup_output_id (control, id);

        if (out == NULL) {
                g_warning ("on_control_active_output_update - tried to fetch an output of id %u but got nothing", id);
                return;
        }
        active_output_update (dialog, out);
}

static void
on_control_input_added (GvcMixerControl *control,
                        guint            id,
                        GvcMixerDialog  *dialog)
{
        GvcMixerUIDevice* in = NULL;
        in = gvc_mixer_control_lookup_input_id (control, id);

        if (in == NULL) {
                g_warning ("on_control_input_added - tried to fetch an input of id %u but got nothing", id);
                return;
        }
        add_input_ui_entry (dialog, in);
}

static void
on_control_input_removed (GvcMixerControl *control,
                          guint           id,
                          GvcMixerDialog  *dialog)
{
        gboolean          found;
        GtkTreeIter       iter;
        GtkTreeModel     *model;
        gint              stream_id;
        GvcMixerUIDevice *in;

        in = gvc_mixer_control_lookup_input_id (control, id);

        g_object_get (G_OBJECT (in),
                     "stream-id", &stream_id,
                      NULL);

        g_debug ("Remove input from dialog, id: %u, stream id: %i",
                 id,
                 stream_id);

        /* remove from any models */
        model = gtk_tree_view_get_model (GTK_TREE_VIEW (dialog->input_treeview));
        found = find_item_by_id (GTK_TREE_MODEL (model), id, ID_COLUMN, &iter);
        if (found) {
                gtk_list_store_remove (GTK_LIST_STORE (model), &iter);
        }
}

static void
on_control_output_added (GvcMixerControl *control,
                         guint            id,
                         GvcMixerDialog  *dialog)
{
        GvcMixerUIDevice* out = NULL;
        out = gvc_mixer_control_lookup_output_id (control, id);

        if (out == NULL) {
                g_warning ("on_control_output_added - tried to fetch an output of id %u but got nothing", id);
                return;
        }

        add_output_ui_entry (dialog, out);
}

static void
on_control_output_removed (GvcMixerControl *control,
                           guint           id,
                           GvcMixerDialog  *dialog)
{
        gboolean      found;
        GtkTreeIter   iter;
        GtkTreeModel *model;
        gint          sink_stream_id;

        GvcMixerUIDevice* out = NULL;
        out = gvc_mixer_control_lookup_output_id (control, id);

        g_object_get (G_OBJECT (out),
                     "stream-id", &sink_stream_id,
                      NULL);

        g_debug ("Remove output from dialog \n id : %u \n sink stream id : %i \n",
                  id,
                  sink_stream_id);

         /* remove from any models */
        model = gtk_tree_view_get_model (GTK_TREE_VIEW (dialog->output_treeview));
        found = find_item_by_id (GTK_TREE_MODEL (model), id, ID_COLUMN, &iter);
        if (found) {
                gtk_list_store_remove (GTK_LIST_STORE (model), &iter);
        }
}

static void
remove_stream (GvcMixerDialog  *dialog,
               guint            id)
{
        GtkWidget *bar;
        guint output_id, input_id;

        bar = g_hash_table_lookup (dialog->bars, GUINT_TO_POINTER (id));
        if (bar != NULL) {
                g_hash_table_remove (dialog->bars, GUINT_TO_POINTER (id));
                gtk_container_remove (GTK_CONTAINER (gtk_widget_get_parent (bar)),
                                      bar);
                dialog->num_apps--;
                if (dialog->num_apps == 0) {
                        gtk_widget_show (dialog->no_apps_label);
                }
                return;
        }

	output_id = GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (dialog->output_bar), "gvc-mixer-dialog-stream-id"));
	input_id = GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (dialog->input_bar), "gvc-mixer-dialog-stream-id"));

	if (output_id == id)
		bar = dialog->output_bar;
	else if (input_id == id)
		bar = dialog->input_bar;
	else
		return;

	g_object_set_data (G_OBJECT (bar), "gvc-mixer-dialog-stream-id", NULL);
	g_object_set_data (G_OBJECT (bar), "gvc-mixer-dialog-stream", NULL);
}

static void
on_control_stream_removed (GvcMixerControl *control,
                           guint            id,
                           GvcMixerDialog  *dialog)
{
        remove_stream (dialog, id);
}

static void
_gtk_label_make_bold (GtkLabel *label)
{
        gchar *str;
        str = g_strdup_printf ("<span font-weight='bold'>%s</span>",
                               gtk_label_get_label (label));
        gtk_label_set_markup_with_mnemonic (label, str);
        g_free (str);
}

static void
on_input_selection_changed (GtkTreeSelection *selection,
                            GvcMixerDialog   *dialog)
{
        GtkTreeModel     *model;
        GtkTreeIter       iter;
        gboolean          active;
        guint             id;
        GvcMixerUIDevice *input;

        if (gtk_get_current_event_device () == NULL)
                return;

        if (gtk_tree_selection_get_selected (selection, &model, &iter) == FALSE) {
                g_debug ("Could not get default input from selection");
                return;
        }

        gtk_tree_model_get (model, &iter,
                            ID_COLUMN, &id,
                            ACTIVE_COLUMN, &active,
                            -1);

        input = gvc_mixer_control_lookup_input_id (dialog->mixer_control, id);

        if (input == NULL) {
                g_warning ("on_input_selection_changed - Unable to find input with id: %u", id);
                return;
        }

        gvc_mixer_control_change_input (dialog->mixer_control, input);
}

static void
on_output_selection_changed (GtkTreeSelection *selection,
                             GvcMixerDialog   *dialog)
{
        GtkTreeModel     *model;
        GtkTreeIter       iter;
        gboolean          active;
        guint             id;
        GvcMixerUIDevice *output;

        if (gtk_get_current_event_device () == NULL)
                return;

        if (gtk_tree_selection_get_selected (selection, &model, &iter) == FALSE) {
                g_debug ("Could not get default output from selection");
                return;
        }

        gtk_tree_model_get (model, &iter,
                            ID_COLUMN, &id,
                            ACTIVE_COLUMN, &active,
                            -1);

        g_debug ("on_output_selection_changed() stream id: %u, active %i", id, active);
        if (active)
                return;

        output = gvc_mixer_control_lookup_output_id (dialog->mixer_control, id);

        if (output == NULL) {
                g_warning ("Unable to find output with id: %u", id);
                return;
        }

        gvc_mixer_control_change_output (dialog->mixer_control, output);
}

static GtkWidget *
create_ui_device_treeview (GvcMixerDialog *dialog,
                           GCallback       on_selection_changed)
{
        GtkWidget         *treeview;
        GtkListStore      *store;
        GtkCellRenderer   *renderer;
        GtkTreeViewColumn *column;
        GtkTreeSelection  *selection;

        treeview = gtk_tree_view_new ();
        gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (treeview), FALSE);

        store = gtk_list_store_new (NUM_COLUMNS,
                                    G_TYPE_STRING,
                                    G_TYPE_STRING,
                                    G_TYPE_BOOLEAN,
                                    G_TYPE_UINT,
                                    G_TYPE_ICON);
        gtk_tree_view_set_model (GTK_TREE_VIEW (treeview),
                                 GTK_TREE_MODEL (store));

        selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview));
        gtk_tree_selection_set_mode (selection, GTK_SELECTION_BROWSE);

        column = gtk_tree_view_column_new ();
        gtk_tree_view_column_set_title (column, _("Name"));
        renderer = gtk_cell_renderer_pixbuf_new ();
        gtk_tree_view_column_pack_start (column, renderer, FALSE);
        g_object_set (G_OBJECT (renderer), "stock-size", GTK_ICON_SIZE_LARGE_TOOLBAR, NULL);
        gtk_tree_view_column_set_attributes (column, renderer,
                                             "gicon", ICON_COLUMN,
                                             NULL);

        renderer = gtk_cell_renderer_text_new ();
        gtk_tree_view_column_pack_start (column, renderer, TRUE);
        gtk_tree_view_column_set_attributes (column, renderer,
                                             "text", NAME_COLUMN,
                                             NULL);
        gtk_tree_view_append_column (GTK_TREE_VIEW (treeview), column);

        g_signal_connect (G_OBJECT (selection), "changed",
                          on_selection_changed, dialog);
#if 0
        renderer = gtk_cell_renderer_text_new ();
        column = gtk_tree_view_column_new_with_attributes (_("Device"),
                                                           renderer,
                                                           "text", DEVICE_COLUMN,
                                                           NULL);
        gtk_tree_view_append_column (GTK_TREE_VIEW (treeview), column);
#endif
        return treeview;
}

static void
on_test_speakers_clicked (GvcComboBox *widget,
                          gpointer     user_data)
{
        GvcMixerDialog      *dialog = GVC_MIXER_DIALOG (user_data);
        GtkTreeModel        *model;
        GtkTreeIter          iter;
        gint                 stream_id;
        gint                 active_output = GVC_MIXER_UI_DEVICE_INVALID;
        GvcMixerUIDevice    *output;
        GvcMixerStream      *stream;
        GtkWidget           *d, *speaker_test, *container;
        char                *title;

        model = gtk_tree_view_get_model (GTK_TREE_VIEW (dialog->output_treeview));

        if (gtk_tree_model_get_iter_first (model, &iter) == FALSE) {
                g_warning ("The tree is empty => we have no device to test speakers with return");
                return;
        }

        do {
                gboolean         is_selected = FALSE;
                gint             id;

                gtk_tree_model_get (model, &iter,
                                    ID_COLUMN, &id,
                                    ACTIVE_COLUMN, &is_selected,
                                    -1);

                if (is_selected) {
                        active_output = id;
                        break;
                }
        } while (gtk_tree_model_iter_next (model, &iter));

        if (active_output == GVC_MIXER_UI_DEVICE_INVALID) {
                g_warning ("Can't find the active output from the UI");
                return;
        }

        output = gvc_mixer_control_lookup_output_id (dialog->mixer_control, (guint)active_output);
        stream_id = gvc_mixer_ui_device_get_stream_id (output);

        if (stream_id == GVC_MIXER_UI_DEVICE_INVALID)
                return;

        g_debug ("Test speakers on '%s'", gvc_mixer_ui_device_get_description (output));

        stream = gvc_mixer_control_lookup_stream_id (dialog->mixer_control, stream_id);
        if (stream == NULL) {
                g_debug ("Stream/sink not found");
                return;
        }
        title = g_strdup_printf (_("Speaker Testing for %s"), gvc_mixer_ui_device_get_description (output));
        d = g_object_new (GTK_TYPE_DIALOG, "title", title,
                                           "transient-for", GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (widget))),
                                           "modal", TRUE,
                                           "destroy-with-parent", TRUE,
                                           "use-header-bar", TRUE,
                                           "resizable", FALSE,
                                           NULL);

        g_free (title);
        speaker_test = gvc_speaker_test_new (dialog->mixer_control,
                                             stream);
        gtk_widget_show (speaker_test);
        container = gtk_dialog_get_content_area (GTK_DIALOG (d));
        gtk_container_add (GTK_CONTAINER (container), speaker_test);

        dialog->test_dialog = d;
        g_object_add_weak_pointer (G_OBJECT (d),
                                   (gpointer *) &dialog->test_dialog);
        gtk_dialog_run (GTK_DIALOG (d));
        gtk_widget_destroy (d);
}

static GObject *
gvc_mixer_dialog_constructor (GType                  type,
                              guint                  n_construct_properties,
                              GObjectConstructParam *construct_params)
{
        GObject          *object;
        GvcMixerDialog   *self;
        GtkWidget        *main_vbox;
        GtkWidget        *label;
        GtkWidget        *sw;
        GtkWidget        *box;
        GtkWidget        *sbox;
        GtkWidget        *ebox;
        GSList           *streams;
        GSList           *l;
        GvcMixerStream   *stream;

        object = G_OBJECT_CLASS (gvc_mixer_dialog_parent_class)->constructor (type, n_construct_properties, construct_params);

        self = GVC_MIXER_DIALOG (object);

        main_vbox = GTK_WIDGET (self);
        gtk_box_set_spacing (GTK_BOX (main_vbox), 2);

        gtk_container_set_border_width (GTK_CONTAINER (self), 12);

        self->output_stream_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 12);
        gtk_widget_set_margin_top (self->output_stream_box, 12);
        gtk_box_pack_start (GTK_BOX (main_vbox),
                            self->output_stream_box,
                            FALSE, FALSE, 0);
        self->output_bar = create_bar (self, TRUE, TRUE);
        gvc_channel_bar_set_name (GVC_CHANNEL_BAR (self->output_bar),
                                  _("_Output volume:"));
        gtk_widget_set_sensitive (self->output_bar, FALSE);
        gtk_box_pack_start (GTK_BOX (self->output_stream_box),
                            self->output_bar, TRUE, TRUE, 12);

        self->notebook = gtk_notebook_new ();
        gtk_box_pack_start (GTK_BOX (main_vbox),
                            self->notebook,
                            TRUE, TRUE, 0);
        gtk_container_set_border_width (GTK_CONTAINER (self->notebook), 5);

        /* Output page */
        self->output_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 12);
        gtk_container_set_border_width (GTK_CONTAINER (self->output_box), 12);
        label = gtk_label_new (_("Output"));
        gtk_notebook_append_page (GTK_NOTEBOOK (self->notebook),
                                  self->output_box,
                                  label);

        box = gtk_frame_new (_("C_hoose a device for sound output:"));
        label = gtk_frame_get_label_widget (GTK_FRAME (box));
        _gtk_label_make_bold (GTK_LABEL (label));
        gtk_label_set_use_underline (GTK_LABEL (label), TRUE);
        gtk_frame_set_shadow_type (GTK_FRAME (box), GTK_SHADOW_NONE);
        gtk_box_pack_start (GTK_BOX (self->output_box), box, TRUE, TRUE, 0);

        self->output_treeview = create_ui_device_treeview (self,
                                                                 G_CALLBACK (on_output_selection_changed));
        gtk_label_set_mnemonic_widget (GTK_LABEL (label), self->output_treeview);

        sw = gtk_scrolled_window_new (NULL, NULL);
        gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
                                        GTK_POLICY_NEVER,
                                        GTK_POLICY_AUTOMATIC);
        gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (sw),
                                             GTK_SHADOW_IN);
        gtk_container_add (GTK_CONTAINER (sw), self->output_treeview);
        gtk_scrolled_window_set_min_content_height (GTK_SCROLLED_WINDOW (sw), 150);
        gtk_widget_set_margin_top (sw, 6);
        gtk_container_add (GTK_CONTAINER (box), sw);

        box = gtk_frame_new (_("Settings for the selected device:"));
        label = gtk_frame_get_label_widget (GTK_FRAME (box));
        _gtk_label_make_bold (GTK_LABEL (label));
        gtk_frame_set_shadow_type (GTK_FRAME (box), GTK_SHADOW_NONE);
        gtk_box_pack_start (GTK_BOX (self->output_box), box, FALSE, FALSE, 12);
        self->output_settings_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
        gtk_container_add (GTK_CONTAINER (box), self->output_settings_box);

        /* Input page */
        self->input_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 12);
        gtk_container_set_border_width (GTK_CONTAINER (self->input_box), 12);
        label = gtk_label_new (_("Input"));
        gtk_notebook_append_page (GTK_NOTEBOOK (self->notebook),
                                  self->input_box,
                                  label);

        self->input_bar = create_bar (self, TRUE, TRUE);
        gvc_channel_bar_set_name (GVC_CHANNEL_BAR (self->input_bar),
                                  _("_Input volume:"));
        gvc_channel_bar_set_low_icon_name (GVC_CHANNEL_BAR (self->input_bar),
                                           "audio-input-microphone-low-symbolic");
        gvc_channel_bar_set_high_icon_name (GVC_CHANNEL_BAR (self->input_bar),
                                            "audio-input-microphone-high-symbolic");
        gtk_widget_set_sensitive (self->input_bar, FALSE);
        gtk_widget_set_margin_top (self->input_bar, 6);
        gtk_box_pack_start (GTK_BOX (self->input_box),
                            self->input_bar,
                            FALSE, FALSE, 0);

        box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
        gtk_box_pack_start (GTK_BOX (self->input_box),
                            box,
                            FALSE, FALSE, 6);

        sbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
        gtk_box_pack_start (GTK_BOX (box),
                            sbox,
                            FALSE, FALSE, 0);

        label = gtk_label_new (_("Input level:"));
        gtk_box_pack_start (GTK_BOX (sbox),
                            label,
                            FALSE, FALSE, 0);
        if (self->size_group != NULL)
                gtk_size_group_add_widget (self->size_group, sbox);

        self->input_level_bar = gvc_level_bar_new ();
        gvc_level_bar_set_scale (GVC_LEVEL_BAR (self->input_level_bar),
                                 GVC_LEVEL_SCALE_LINEAR);
        gtk_box_pack_start (GTK_BOX (box),
                            self->input_level_bar,
                            TRUE, TRUE, 6);

        ebox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
        gtk_box_pack_start (GTK_BOX (box),
                            ebox,
                            FALSE, FALSE, 0);
        if (self->size_group != NULL)
                gtk_size_group_add_widget (self->size_group, ebox);

        self->input_settings_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
        gtk_box_pack_start (GTK_BOX (self->input_box),
                            self->input_settings_box,
                            FALSE, FALSE, 0);

        box = gtk_frame_new (_("C_hoose a device for sound input:"));
        label = gtk_frame_get_label_widget (GTK_FRAME (box));
        _gtk_label_make_bold (GTK_LABEL (label));
        gtk_label_set_use_underline (GTK_LABEL (label), TRUE);
        gtk_frame_set_shadow_type (GTK_FRAME (box), GTK_SHADOW_NONE);
        gtk_box_pack_start (GTK_BOX (self->input_box), box, TRUE, TRUE, 0);

        self->input_treeview = create_ui_device_treeview (self,
                                                                G_CALLBACK (on_input_selection_changed));
        gtk_label_set_mnemonic_widget (GTK_LABEL (label), self->input_treeview);

        sw = gtk_scrolled_window_new (NULL, NULL);
        gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
                                        GTK_POLICY_NEVER,
                                        GTK_POLICY_AUTOMATIC);
        gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (sw),
                                             GTK_SHADOW_IN);
        gtk_container_add (GTK_CONTAINER (sw), self->input_treeview);
        gtk_widget_set_margin_top (sw, 6);
        gtk_container_add (GTK_CONTAINER (box), sw);

        /* Effects page */
        self->sound_effects_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
        gtk_container_set_border_width (GTK_CONTAINER (self->sound_effects_box), 12);
        label = gtk_label_new (_("Sound Effects"));
        gtk_notebook_append_page (GTK_NOTEBOOK (self->notebook),
                                  self->sound_effects_box,
                                  label);

        self->effects_bar = create_bar (self, TRUE, TRUE);
        gvc_channel_bar_set_name (GVC_CHANNEL_BAR (self->effects_bar),
                                  _("_Alert volume:"));
        gtk_widget_set_sensitive (self->effects_bar, FALSE);
        gtk_box_pack_start (GTK_BOX (self->sound_effects_box),
                            self->effects_bar, FALSE, FALSE, 0);

        self->sound_theme_chooser = gvc_sound_theme_chooser_new ();
        gtk_box_pack_start (GTK_BOX (self->sound_effects_box),
                            self->sound_theme_chooser,
                            TRUE, TRUE, 6);

        /* Applications */
        self->applications_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 12);
        gtk_container_set_border_width (GTK_CONTAINER (self->applications_box), 12);
        gtk_widget_set_margin_end (self->applications_box, 10);
        label = gtk_label_new (_("Applications"));

        box = gtk_scrolled_window_new (NULL, NULL);
        gtk_container_add (GTK_CONTAINER (box), self->applications_box);
        gtk_notebook_append_page (GTK_NOTEBOOK (self->notebook),
                                  box,
                                  label);
        self->no_apps_label = gtk_label_new (_("No application is currently playing or recording audio."));
        gtk_box_pack_start (GTK_BOX (self->applications_box),
                            self->no_apps_label,
                            TRUE, TRUE, 0);

        g_signal_connect (self->mixer_control,
                          "output-added",
                          G_CALLBACK (on_control_output_added),
                          self);
        g_signal_connect (self->mixer_control,
                          "output-removed",
                          G_CALLBACK (on_control_output_removed),
                          self);
        g_signal_connect (self->mixer_control,
                          "input-added",
                          G_CALLBACK (on_control_input_added),
                          self);
        g_signal_connect (self->mixer_control,
                          "input-removed",
                          G_CALLBACK (on_control_input_removed),
                          self);

        g_signal_connect (self->mixer_control,
                          "stream-added",
                          G_CALLBACK (on_control_stream_added),
                          self);
        g_signal_connect (self->mixer_control,
                          "stream-removed",
                          G_CALLBACK (on_control_stream_removed),
                          self);

        gtk_widget_show_all (main_vbox);

        streams = gvc_mixer_control_get_streams (self->mixer_control);
        for (l = streams; l != NULL; l = l->next) {
                stream = l->data;
                add_stream (self, stream);
        }
        g_slist_free (streams);

        return object;
}

static void
gvc_mixer_dialog_dispose (GObject *object)
{
        GvcMixerDialog *dialog = GVC_MIXER_DIALOG (object);

        if (dialog->mixer_control != NULL) {

                g_signal_handlers_disconnect_by_func (dialog->mixer_control,
                                                      on_control_output_added,
                                                      dialog);
                g_signal_handlers_disconnect_by_func (dialog->mixer_control,
                                                      on_control_output_removed,
                                                      dialog);
                g_signal_handlers_disconnect_by_func (dialog->mixer_control,
                                                      on_control_input_added,
                                                      dialog);
                g_signal_handlers_disconnect_by_func (dialog->mixer_control,
                                                      on_control_input_removed,
                                                      dialog);
                g_signal_handlers_disconnect_by_func (dialog->mixer_control,
                                                      on_control_active_input_update,
                                                      dialog);
                g_signal_handlers_disconnect_by_func (dialog->mixer_control,
                                                      on_control_active_output_update,
                                                      dialog);
                g_signal_handlers_disconnect_by_func (dialog->mixer_control,
                                                      on_control_stream_added,
                                                      dialog);
                g_signal_handlers_disconnect_by_func (dialog->mixer_control,
                                                      on_control_stream_removed,
                                                      dialog);

                g_object_unref (dialog->mixer_control);
                dialog->mixer_control = NULL;
        }

        if (dialog->bars != NULL) {
                g_hash_table_destroy (dialog->bars);
                dialog->bars = NULL;
        }

        if (dialog->test_dialog != NULL) {
                gtk_dialog_response (GTK_DIALOG (dialog->test_dialog),
                                     GTK_RESPONSE_OK);
        }

        G_OBJECT_CLASS (gvc_mixer_dialog_parent_class)->dispose (object);
}

static void
gvc_mixer_dialog_class_init (GvcMixerDialogClass *klass)
{
        GObjectClass   *object_class = G_OBJECT_CLASS (klass);

        object_class->constructor = gvc_mixer_dialog_constructor;
        object_class->dispose = gvc_mixer_dialog_dispose;
        object_class->finalize = gvc_mixer_dialog_finalize;
        object_class->set_property = gvc_mixer_dialog_set_property;
        object_class->get_property = gvc_mixer_dialog_get_property;

        g_object_class_install_property (object_class,
                                         PROP_MIXER_CONTROL,
                                         g_param_spec_object ("mixer-control",
                                                              "mixer control",
                                                              "mixer control",
                                                              GVC_TYPE_MIXER_CONTROL,
                                                              G_PARAM_READWRITE|G_PARAM_CONSTRUCT));
}


static void
gvc_mixer_dialog_init (GvcMixerDialog *dialog)
{
        gtk_orientable_set_orientation (GTK_ORIENTABLE (dialog),
                                        GTK_ORIENTATION_VERTICAL);
        dialog->bars = g_hash_table_new (NULL, NULL);
        dialog->size_group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);
}

static void
gvc_mixer_dialog_finalize (GObject *object)
{
        GvcMixerDialog *mixer_dialog;

        g_return_if_fail (object != NULL);
        g_return_if_fail (GVC_IS_MIXER_DIALOG (object));

        mixer_dialog = GVC_MIXER_DIALOG (object);

        g_return_if_fail (mixer_dialog != NULL);
        G_OBJECT_CLASS (gvc_mixer_dialog_parent_class)->finalize (object);
}

GvcMixerDialog *
gvc_mixer_dialog_new (GvcMixerControl *control)
{
        GObject *dialog;
        dialog = g_object_new (GVC_TYPE_MIXER_DIALOG,
                               "mixer-control", control,
                               NULL);
        return GVC_MIXER_DIALOG (dialog);
}

enum {
        PAGE_OUTPUT,
        PAGE_INPUT,
        PAGE_EVENTS,
        PAGE_APPLICATIONS
};

gboolean
gvc_mixer_dialog_set_page (GvcMixerDialog *self,
                           const char     *page)
{
        guint num;

        g_return_val_if_fail (self != NULL, FALSE);

        num = PAGE_OUTPUT;

        if (g_str_equal (page, "effects"))
                num = PAGE_EVENTS;
        else if (g_str_equal (page, "input"))
                num = PAGE_INPUT;
        else if (g_str_equal (page, "output"))
                num = PAGE_OUTPUT;
        else if (g_str_equal (page, "applications"))
                num = PAGE_APPLICATIONS;

        gtk_notebook_set_current_page (GTK_NOTEBOOK (self->notebook), num);

        return TRUE;
}
