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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
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

G_DEFINE_DYNAMIC_TYPE (CcSoundPanel, cc_sound_panel, CC_TYPE_PANEL)

enum {
        PROP_0,
        PROP_ARGV
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
        case PROP_ARGV: {
                gchar **args;

                args = g_value_get_boxed (value);

                if (args && args[0]) {
                        gvc_mixer_dialog_set_page (self->dialog, args[0]);
                }
                break;
        }
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        }
}

static void
cc_sound_panel_class_init (CcSoundPanelClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);

        object_class->finalize = cc_sound_panel_finalize;
        object_class->set_property = cc_sound_panel_set_property;

        g_object_class_override_property (object_class, PROP_ARGV, "argv");
}

static void
cc_sound_panel_class_finalize (CcSoundPanelClass *klass)
{
}

static void
cc_sound_panel_finalize (GObject *object)
{
        CcSoundPanel *panel = CC_SOUND_PANEL (object);

        if (panel->dialog != NULL)
                panel->dialog = NULL;
        if (panel->connecting_label != NULL)
                panel->connecting_label = NULL;
        if (panel->control != NULL) {
                g_object_unref (panel->control);
                panel->control = NULL;
        }

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

void
cc_sound_panel_register (GIOModule *module)
{
        cc_sound_panel_register_type (G_TYPE_MODULE (module));
        g_io_extension_point_implement (CC_SHELL_PANEL_EXTENSION_POINT,
                                        CC_TYPE_SOUND_PANEL,
                                        "sound", 0);
}

/* GIO extension stuff */
void
g_io_module_load (GIOModule *module)
{
        bindtextdomain (GETTEXT_PACKAGE, LOCALE_DIR);
        bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");

        /* register the panel */
        cc_sound_panel_register (module);
}

void
g_io_module_unload (GIOModule *module)
{
}

