/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*-
 *
 * Copyright (C) 2017 Mohammed Sadiq <sadiq@sadiqpk.org>
 * Copyright (C) 2010 Red Hat, Inc
 * Copyright (C) 2008 William Jon McCann <jmccann@redhat.com>
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

#include <config.h>

#include <glib.h>

#include <shell/cc-panel.h>

#include "cc-applications-panel.h"
#include "cc-removable-media-row.h"
#include "cc-removable-media-settings.h"

/* Autorun options */
#define PREF_MEDIA_AUTORUN_NEVER "autorun-never"

#define MEDIA_HANDLING_SCHEMA "org.gnome.desktop.media-handling"

struct _CcRemovableMediaSettings {
    AdwPreferencesGroup parent;

    GSettings *settings;
};

G_DEFINE_FINAL_TYPE (CcRemovableMediaSettings, cc_removable_media_settings, ADW_TYPE_PREFERENCES_GROUP)

static void
cc_removable_media_settings_finalize (GObject *object)
{
    CcRemovableMediaSettings *self = CC_REMOVABLE_MEDIA_SETTINGS (object);

    g_clear_object (&self->settings);

    G_OBJECT_CLASS (cc_removable_media_settings_parent_class)->finalize (object);
}

static void
cc_removable_media_settings_class_init (CcRemovableMediaSettingsClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

    object_class->finalize = cc_removable_media_settings_finalize;

    gtk_widget_class_set_template_from_resource (
        widget_class, "/org/gnome/control-center/applications/cc-removable-media-settings.ui");
}

static void
cc_removable_media_settings_init (CcRemovableMediaSettings *self)
{
    g_type_ensure (CC_TYPE_REMOVABLE_MEDIA_ROW);

    gtk_widget_init_template (GTK_WIDGET (self));
    self->settings = g_settings_new (MEDIA_HANDLING_SCHEMA);

    g_settings_bind (self->settings, PREF_MEDIA_AUTORUN_NEVER, self, "sensitive", G_SETTINGS_BIND_INVERT_BOOLEAN);
}

CcRemovableMediaSettings *
cc_removable_media_settings_new (void)
{
    return g_object_new (CC_TYPE_REMOVABLE_MEDIA_SETTINGS, NULL);
}
