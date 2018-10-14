/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2009 Bastien Nocera
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

#include <glib.h>
#include <glib/gi18n-lib.h>
#include <gtk/gtk.h>
#include <canberra.h>
#include <canberra-gtk.h>
#include <pulse/pulseaudio.h>

#include "gvc-speaker-test.h"
#include "gvc-mixer-stream.h"

struct _GvcSpeakerTest
{
        GtkGrid          parent_instance;
        GtkWidget       *channel_controls[PA_CHANNEL_POSITION_MAX];
        ca_context      *canberra;
        GvcMixerStream  *stream;
        GvcMixerControl *control;
};

enum {
        COL_NAME,
        COL_HUMAN_NAME,
        NUM_COLS
};

enum {
        PROP_0,
        PROP_STREAM,
        PROP_CONTROL
};

static void     gvc_speaker_test_class_init (GvcSpeakerTestClass *klass);
static void     gvc_speaker_test_init       (GvcSpeakerTest      *speaker_test);
static void     gvc_speaker_test_finalize   (GObject            *object);
static void     update_channel_map          (GvcSpeakerTest *speaker_test);

G_DEFINE_TYPE (GvcSpeakerTest, gvc_speaker_test, GTK_TYPE_GRID)

static const int position_table[] = {
        /* Position, X, Y */
        PA_CHANNEL_POSITION_FRONT_LEFT, 0, 0,
        PA_CHANNEL_POSITION_FRONT_LEFT_OF_CENTER, 1, 0,
        PA_CHANNEL_POSITION_FRONT_CENTER, 2, 0,
        PA_CHANNEL_POSITION_MONO, 2, 0,
        PA_CHANNEL_POSITION_FRONT_RIGHT_OF_CENTER, 3, 0,
        PA_CHANNEL_POSITION_FRONT_RIGHT, 4, 0,
        PA_CHANNEL_POSITION_SIDE_LEFT, 0, 1,
        PA_CHANNEL_POSITION_SIDE_RIGHT, 4, 1,
        PA_CHANNEL_POSITION_REAR_LEFT, 0, 2,
        PA_CHANNEL_POSITION_REAR_CENTER, 2, 2,
        PA_CHANNEL_POSITION_REAR_RIGHT, 4, 2,
        PA_CHANNEL_POSITION_LFE, 3, 2
};

static void
gvc_speaker_test_set_property (GObject       *object,
                               guint          prop_id,
                               const GValue  *value,
                               GParamSpec    *pspec)
{
        GvcSpeakerTest *self = GVC_SPEAKER_TEST (object);

        switch (prop_id) {
        case PROP_STREAM:
                self->stream = g_value_dup_object (value);
                if (self->control != NULL)
                        update_channel_map (self);
                break;
        case PROP_CONTROL:
                self->control = g_value_dup_object (value);
                if (self->stream != NULL)
                        update_channel_map (self);
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
                break;
        }
}

static void
gvc_speaker_test_get_property (GObject     *object,
                               guint        prop_id,
                               GValue      *value,
                               GParamSpec  *pspec)
{
        GvcSpeakerTest *self = GVC_SPEAKER_TEST (object);

        switch (prop_id) {
        case PROP_STREAM:
                g_value_set_object (value, self->stream);
                break;
        case PROP_CONTROL:
                g_value_set_object (value, self->control);
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
                break;
        }
}

static void
gvc_speaker_test_class_init (GvcSpeakerTestClass *klass)
{
        GObjectClass   *object_class = G_OBJECT_CLASS (klass);

        object_class->finalize = gvc_speaker_test_finalize;
        object_class->set_property = gvc_speaker_test_set_property;
        object_class->get_property = gvc_speaker_test_get_property;

        g_object_class_install_property (object_class,
                                         PROP_STREAM,
                                         g_param_spec_object ("stream",
                                                              "stream",
                                                              "The stream",
                                                              GVC_TYPE_MIXER_STREAM,
                                                              G_PARAM_READWRITE|G_PARAM_CONSTRUCT));
        g_object_class_install_property (object_class,
                                         PROP_CONTROL,
                                         g_param_spec_object ("control",
                                                              "control",
                                                              "The mixer controller",
                                                              GVC_TYPE_MIXER_CONTROL,
                                                              G_PARAM_READWRITE|G_PARAM_CONSTRUCT_ONLY));
}

static const char *
sound_name (pa_channel_position_t position)
{
        switch (position) {
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

static const char *
icon_name (pa_channel_position_t position, gboolean playing)
{
        switch (position) {
        case PA_CHANNEL_POSITION_FRONT_LEFT:
                return playing ? "audio-speaker-left-testing" : "audio-speaker-left";
        case PA_CHANNEL_POSITION_FRONT_RIGHT:
                return playing ? "audio-speaker-right-testing" : "audio-speaker-right";
        case PA_CHANNEL_POSITION_FRONT_CENTER:
                return playing ? "audio-speaker-center-testing" : "audio-speaker-center";
        case PA_CHANNEL_POSITION_REAR_LEFT:
                return playing ? "audio-speaker-left-back-testing" : "audio-speaker-left-back";
        case PA_CHANNEL_POSITION_REAR_RIGHT:
                return playing ? "audio-speaker-right-back-testing" : "audio-speaker-right-back";
        case PA_CHANNEL_POSITION_REAR_CENTER:
                return playing ? "audio-speaker-center-back-testing" : "audio-speaker-center-back";
        case PA_CHANNEL_POSITION_LFE:
                return playing ? "audio-subwoofer-testing" : "audio-subwoofer";
        case PA_CHANNEL_POSITION_SIDE_LEFT:
                return playing ? "audio-speaker-left-side-testing" : "audio-speaker-left-side";
        case PA_CHANNEL_POSITION_SIDE_RIGHT:
                return playing ? "audio-speaker-right-side-testing" : "audio-speaker-right-side";
        case PA_CHANNEL_POSITION_FRONT_LEFT_OF_CENTER:
                return playing ? "audio-speaker-front-left-of-center-testing" : "audio-speaker-front-left-of-center";
        case PA_CHANNEL_POSITION_FRONT_RIGHT_OF_CENTER:
                return playing ? "audio-speaker-front-right-of-center-testing" : "audio-speaker-front-right-of-center";
        case PA_CHANNEL_POSITION_MONO:
                return playing ? "audio-speaker-mono-testing" : "audio-speaker-mono";
        default:
                return NULL;
        }
}

static void
update_button (GtkWidget *control)
{
        GtkWidget *button;
        GtkWidget *image;
        pa_channel_position_t position;
        gboolean playing;

        button = g_object_get_data (G_OBJECT (control), "button");
        image = g_object_get_data (G_OBJECT (control), "image");
        position = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (control), "position"));
        playing = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (control), "playing"));
        gtk_button_set_label (GTK_BUTTON (button), playing ? _("Stop") : _("Test"));
        gtk_image_set_from_icon_name (GTK_IMAGE (image), icon_name (position, playing), GTK_ICON_SIZE_DIALOG);
}

static const char *
pretty_position (pa_channel_position_t position)
{
        if (position == PA_CHANNEL_POSITION_LFE)
                return N_("Subwoofer");

        return pa_channel_position_to_pretty_string (position);
}

static gboolean
idle_cb (GtkWidget *control)
{
        if (control == NULL)
                return FALSE;

        /* This is called in the background thread, hence
         * forward to main thread via idle callback */
        g_object_set_data (G_OBJECT (control), "playing", GINT_TO_POINTER(FALSE));
        update_button (control);

        return FALSE;
}

static void
finish_cb (ca_context *c, uint32_t id, int error_code, void *userdata)
{
        GtkWidget *control = (GtkWidget *) userdata;

        if (error_code == CA_ERROR_DESTROYED || control == NULL)
                return;
        g_idle_add ((GSourceFunc) idle_cb, control);
}

static void
on_test_button_clicked (GtkButton *button,
                        GtkWidget *control)
{
        gboolean playing;
        ca_context *canberra;

        canberra = g_object_get_data (G_OBJECT (control), "canberra");

        ca_context_cancel (canberra, 1);

        playing = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (control), "playing"));

        if (playing) {
                g_object_set_data (G_OBJECT (control), "playing", GINT_TO_POINTER(FALSE));
        } else {
                pa_channel_position_t position;
                const char *name;
                ca_proplist *proplist;

                position = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (control), "position"));

                ca_proplist_create (&proplist);
                ca_proplist_sets (proplist, CA_PROP_MEDIA_ROLE, "test");
                ca_proplist_sets (proplist, CA_PROP_MEDIA_NAME, pretty_position (position));
                ca_proplist_sets (proplist, CA_PROP_CANBERRA_FORCE_CHANNEL,
                                  pa_channel_position_to_string (position));
                ca_proplist_sets (proplist, CA_PROP_CANBERRA_ENABLE, "1");

                name = sound_name (position);
                if (name != NULL) {
                        ca_proplist_sets (proplist, CA_PROP_EVENT_ID, name);
                        playing = ca_context_play_full (canberra, 1, proplist, finish_cb, control) >= 0;
                }

                if (!playing) {
                        ca_proplist_sets (proplist, CA_PROP_EVENT_ID, "audio-test-signal");
                        playing = ca_context_play_full (canberra, 1, proplist, finish_cb, control) >= 0;
                }

                if (!playing) {
                        ca_proplist_sets(proplist, CA_PROP_EVENT_ID, "bell-window-system");
                        playing = ca_context_play_full (canberra, 1, proplist, finish_cb, control) >= 0;
                }
                g_object_set_data (G_OBJECT (control), "playing", GINT_TO_POINTER(playing));
        }

        update_button (control);
}

static GtkWidget *
channel_control_new (ca_context *canberra, pa_channel_position_t position)
{
        GtkWidget *control;
        GtkWidget *box;
        GtkWidget *label;
        GtkWidget *image;
        GtkWidget *test_button;
        const char *name;

        control = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
        g_object_set_data (G_OBJECT (control), "playing", GINT_TO_POINTER(FALSE));
        g_object_set_data (G_OBJECT (control), "position", GINT_TO_POINTER(position));
        g_object_set_data (G_OBJECT (control), "canberra", canberra);

        name = icon_name (position, FALSE);
        if (name == NULL)
                name = "audio-volume-medium";
        image = gtk_image_new_from_icon_name (name, GTK_ICON_SIZE_DIALOG);
        gtk_widget_show (image);
        g_object_set_data (G_OBJECT (control), "image", image);
        gtk_box_pack_start (GTK_BOX (control), image, FALSE, FALSE, 0);

        label = gtk_label_new (pretty_position (position));
        gtk_widget_show (label);
        gtk_box_pack_start (GTK_BOX (control), label, FALSE, FALSE, 0);

        test_button = gtk_button_new_with_label (_("Test"));
        gtk_widget_show (test_button);

        g_signal_connect (G_OBJECT (test_button), "clicked",
                          G_CALLBACK (on_test_button_clicked), control);
        g_object_set_data (G_OBJECT (control), "button", test_button);

        atk_object_add_relationship (gtk_widget_get_accessible (test_button),
                                     ATK_RELATION_LABELLED_BY,
                                     gtk_widget_get_accessible (label));

        box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
        gtk_widget_show (box);
        gtk_box_pack_start (GTK_BOX (box), test_button, TRUE, FALSE, 0);
        gtk_box_pack_start (GTK_BOX (control), box, FALSE, FALSE, 0);

        return control;
}

static void
create_channel_controls (GvcSpeakerTest *speaker_test)
{
        guint i;

        for (i = 0; i < G_N_ELEMENTS (position_table); i += 3) {
                speaker_test->channel_controls[position_table[i]] = channel_control_new (speaker_test->canberra, (pa_channel_position_t) position_table[i]);
                gtk_widget_show (speaker_test->channel_controls[position_table[i]]);
                gtk_grid_attach (GTK_GRID (speaker_test),
                                 speaker_test->channel_controls[position_table[i]],
                                 position_table[i+1], position_table[i+2], 1, 1);
        }
}

static void
update_channel_map (GvcSpeakerTest *speaker_test)
{
        guint i;
        const GvcChannelMap *map;

        g_return_if_fail (speaker_test->control != NULL);
        g_return_if_fail (speaker_test->stream != NULL);

        g_debug ("XXX update_channel_map called XXX");

        map = gvc_mixer_stream_get_channel_map (speaker_test->stream);
        g_return_if_fail (map != NULL);

        ca_context_change_device (speaker_test->canberra,
                                  gvc_mixer_stream_get_name (speaker_test->stream));

        for (i = 0; i < G_N_ELEMENTS (position_table); i += 3) {
                gtk_widget_set_visible (speaker_test->channel_controls[position_table[i]],
                                        gvc_channel_map_has_position(map, position_table[i]));
        }
}

static void
gvc_speaker_test_set_theme (ca_context *ca)
{
        GtkSettings *settings;
        g_autofree gchar *theme_name = NULL;

        settings = gtk_settings_get_for_screen (gdk_screen_get_default ());

        g_object_get (G_OBJECT (settings),
                      "gtk-sound-theme-name", &theme_name,
                      NULL);

        if (theme_name)
                ca_context_change_props (ca, CA_PROP_CANBERRA_XDG_THEME_NAME, theme_name, NULL);
}

static void
gvc_speaker_test_init (GvcSpeakerTest *speaker_test)
{
        GtkWidget *face;

        ca_context_create (&speaker_test->canberra);
        ca_context_set_driver (speaker_test->canberra, "pulse");
        ca_context_change_props (speaker_test->canberra,
                                 CA_PROP_APPLICATION_ID, "org.gnome.VolumeControl",
                                 NULL);
        gvc_speaker_test_set_theme (speaker_test->canberra);

        gtk_widget_set_direction (GTK_WIDGET (speaker_test), GTK_TEXT_DIR_LTR);
        gtk_container_set_border_width (GTK_CONTAINER (speaker_test), 12);
        gtk_grid_set_row_homogeneous (GTK_GRID (speaker_test), TRUE);
        gtk_grid_set_column_homogeneous (GTK_GRID (speaker_test), TRUE);
        gtk_grid_set_row_spacing (GTK_GRID (speaker_test), 12);
        gtk_grid_set_column_spacing (GTK_GRID (speaker_test), 12);

        create_channel_controls (speaker_test);

        face = gtk_image_new_from_icon_name ("face-smile", GTK_ICON_SIZE_DIALOG);
        gtk_grid_attach (GTK_GRID (speaker_test), face, 2, 1, 1, 1);
        gtk_widget_show (face);
}

static void
gvc_speaker_test_finalize (GObject *object)
{
        GvcSpeakerTest *speaker_test;

        g_return_if_fail (object != NULL);
        g_return_if_fail (GVC_IS_SPEAKER_TEST (object));

        speaker_test = GVC_SPEAKER_TEST (object);

        g_return_if_fail (speaker_test != NULL);

        g_object_unref (speaker_test->stream);
        speaker_test->stream = NULL;

        g_object_unref (speaker_test->control);
        speaker_test->control = NULL;

        ca_context_destroy (speaker_test->canberra);
        speaker_test->canberra = NULL;

        G_OBJECT_CLASS (gvc_speaker_test_parent_class)->finalize (object);
}

GtkWidget *
gvc_speaker_test_new (GvcMixerControl *control,
                      GvcMixerStream  *stream)
{
        GObject *speaker_test;

        g_return_val_if_fail (stream != NULL, NULL);
        g_return_val_if_fail (control != NULL, NULL);

        speaker_test = g_object_new (GVC_TYPE_SPEAKER_TEST,
                                  "stream", stream,
                                  "control", control,
                                  NULL);

        return GTK_WIDGET (speaker_test);
}

