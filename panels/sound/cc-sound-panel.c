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

#include "cc-sound-panel.h"
#include "gvc-mixer-dialog.h"

struct _CcSoundPanel {
	CcPanel parent_instance;

	GvcMixerControl *control;
	GvcMixerDialog  *dialog;
	GtkWidget       *connecting_label;
};

CC_PANEL_REGISTER (CcSoundPanel, cc_sound_panel)

enum {
        PROP_0,
        PROP_PARAMETERS
};

static void cc_sound_panel_finalize (GObject *object);

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
cc_sound_panel_class_init (CcSoundPanelClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);
	CcPanelClass *panel_class = CC_PANEL_CLASS (klass);

	panel_class->get_help_uri = cc_sound_panel_get_help_uri;

        object_class->finalize = cc_sound_panel_finalize;
        object_class->set_property = cc_sound_panel_set_property;

        g_object_class_override_property (object_class, PROP_PARAMETERS, "parameters");
}

static void
cc_sound_panel_finalize (GObject *object)
{
        CcSoundPanel *panel = CC_SOUND_PANEL (object);

        panel->dialog = NULL;
        panel->connecting_label = NULL;
        g_clear_object (&panel->control);

        G_OBJECT_CLASS (cc_sound_panel_parent_class)->finalize (object);
}

static void
cc_sound_panel_init (CcSoundPanel *self)
{
        gtk_icon_theme_append_search_path (gtk_icon_theme_get_default (),
                                           ICON_DATA_DIR);
        gtk_window_set_default_icon_name ("multimedia-volume-control");

        self->control = gvc_mixer_control_new ("GNOME Volume Control Dialog");
        gvc_mixer_control_open (self->control);
        self->dialog = gvc_mixer_dialog_new (self->control);
        gtk_container_add (GTK_CONTAINER (self), GTK_WIDGET (self->dialog));
        gtk_widget_show (GTK_WIDGET (self->dialog));
}
