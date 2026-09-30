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
#include <glib/gi18n.h>

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

typedef struct {
    gchar *description;
    gchar *content_type;
} OtherMediaType;

static void
other_media_type_free (OtherMediaType *other)
{
    g_free (other->description);
    g_free (other->content_type);
    g_free (other);
}

static gint
other_media_type_sort (gconstpointer a, gconstpointer b)
{
    const OtherMediaType *other_a = *((OtherMediaType **) a);
    const OtherMediaType *other_b = *((OtherMediaType **) b);

    return g_utf8_collate (other_a->description, other_b->description);
}

static void
add_other_type_row (CcRemovableMediaSettings *self, const OtherMediaType *other)
{
    CcRemovableMediaRow *row;

    row = cc_removable_media_row_new (other->content_type);
    /* The descriptions come from shared-mime-info, so they may contain
     * characters that would otherwise be interpreted as Pango markup */
    adw_preferences_row_set_use_markup (ADW_PREFERENCES_ROW (row), FALSE);
    adw_preferences_row_set_title (ADW_PREFERENCES_ROW (row), other->description);

    adw_preferences_group_add (ADW_PREFERENCES_GROUP (self), GTK_WIDGET (row));
}

static void
info_panel_setup_media (CcRemovableMediaSettings *self)
{
    guint n;
    GList *l, *content_types;
    g_autoptr(GPtrArray) other_types = NULL;

    const gchar *const defs[] = {
        "x-content/audio-cdda", "x-content/video-dvd",     "x-content/audio-player",
        "x-content/image-dcf",  "x-content/unix-software",
    };

    struct {
        const gchar *content_type;
        const gchar *description;
    } const other_defs[] = {
        /* translators: these strings are duplicates of shared-mime-info
         * strings, just here to fix capitalization of the English originals.
         * If the shared-mime-info translation works for your language,
         * simply leave these untranslated.
         */
        { "x-content/audio-dvd", N_("audio DVD") },
          { "x-content/blank-bd", N_("blank Blu-ray disc") },
            { "x-content/blank-cd", N_("blank CD disc") },
              { "x-content/blank-dvd", N_("blank DVD disc") },
                { "x-content/blank-hddvd", N_("blank HD DVD disc") },
                  { "x-content/video-bluray", N_("Blu-ray video disc") },
                    { "x-content/ebook-reader", N_("e-book reader") },
                      { "x-content/video-hddvd", N_("HD DVD video disc") },
                        { "x-content/image-picturecd", N_("Picture CD") },
                          { "x-content/video-svcd", N_("Super Video CD") },
                            { "x-content/video-vcd", N_("Video CD") },
                              { "x-content/win32-software", N_("Windows software") },
                            };

    other_types = g_ptr_array_new_with_free_func ((GDestroyNotify) other_media_type_free);

    content_types = g_content_types_get_registered ();

    for (l = content_types; l != NULL; l = l->next) {
        char *content_type = l->data;
        g_autofree char *description = NULL;
        OtherMediaType *other;

        if (!g_str_has_prefix (content_type, "x-content/"))
            continue;

        for (n = 0; n < G_N_ELEMENTS (defs); n++) {
            if (g_content_type_is_a (content_type, defs[n])) {
                goto skip;
            }
        }

        for (n = 0; n < G_N_ELEMENTS (other_defs); n++) {
            if (strcmp (content_type, other_defs[n].content_type) == 0) {
                const gchar *s = other_defs[n].description;
                if (s == _(s))
                    description = g_content_type_get_description (content_type);
                else
                    description = g_strdup (_(s));

                break;
            }
        }

        if (description == NULL) {
            g_debug ("Content type '%s' is missing from the info panel", content_type);
            description = g_content_type_get_description (content_type);
        }

        other = g_new0 (OtherMediaType, 1);
        other->description = g_steal_pointer (&description);
        other->content_type = g_strdup (content_type);

        g_ptr_array_add (other_types, other);
    skip:;
    }

    g_list_free_full (content_types, g_free);

    g_ptr_array_sort (other_types, other_media_type_sort);

    for (n = 0; n < other_types->len; n++) {
        add_other_type_row (self, g_ptr_array_index (other_types, n));
    }

    g_settings_bind (self->settings, PREF_MEDIA_AUTORUN_NEVER, self, "sensitive", G_SETTINGS_BIND_INVERT_BOOLEAN);
}

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

    info_panel_setup_media (self);
}

CcRemovableMediaSettings *
cc_removable_media_settings_new (void)
{
    return g_object_new (CC_TYPE_REMOVABLE_MEDIA_SETTINGS, NULL);
}
