/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 * Copyright (C) 2008 Bastien Nocera <hadess@hadess.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include <config.h>
#include <glib/gstdio.h>
#include <glib/gi18n-lib.h>
#include <gio/gio.h>
#include <utime.h>
#include <strings.h>

#include "sound-theme-file-utils.h"

#define CUSTOM_THEME_NAME       "__custom"

/* This function needs to be called after each individual
 * changeset to the theme */
void
custom_theme_update_time (void)
{
        g_autofree gchar *path = NULL;

        path = custom_theme_dir_path (NULL);
        utime (path, NULL);
}

char *
custom_theme_dir_path (const char *child)
{
        static char *dir = NULL;
        const char *data_dir;

        if (dir == NULL) {
                data_dir = g_get_user_data_dir ();
                dir = g_build_filename (data_dir, "sounds", CUSTOM_THEME_NAME, NULL);
        }
        if (child == NULL)
                return g_strdup (dir);

        return g_build_filename (dir, child, NULL);
}

static gboolean
directory_delete_recursive (GFile *directory, GError **error)
{
        GFileEnumerator *enumerator;
        GFileInfo *info;
        gboolean success = TRUE;

        enumerator = g_file_enumerate_children (directory,
                                                G_FILE_ATTRIBUTE_STANDARD_NAME ","
                                                G_FILE_ATTRIBUTE_STANDARD_TYPE,
                                                G_FILE_QUERY_INFO_NONE,
                                                NULL, error);
        if (enumerator == NULL)
                return FALSE;

        while (success &&
               (info = g_file_enumerator_next_file (enumerator, NULL, NULL))) {
                GFile *child;

                child = g_file_get_child (directory, g_file_info_get_name (info));

                if (g_file_info_get_file_type (info) == G_FILE_TYPE_DIRECTORY) {
                        success = directory_delete_recursive (child, error);
                }
                g_object_unref (info);

                if (success)
                        success = g_file_delete (child, NULL, error);
        }
        g_file_enumerator_close (enumerator, NULL, NULL);

        if (success)
                success = g_file_delete (directory, NULL, error);

        return success;
}

/**
 * capplet_file_delete_recursive :
 * @file :
 * @error  :
 *
 * A utility routine to delete files and/or directories,
 * including non-empty directories.
 **/
static gboolean
capplet_file_delete_recursive (GFile *file, GError **error)
{
        g_autoptr(GFileInfo) info = NULL;
        GFileType type;

        g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

        info = g_file_query_info (file,
                                  G_FILE_ATTRIBUTE_STANDARD_TYPE,
                                  G_FILE_QUERY_INFO_NONE,
                                  NULL, error);
        if (info == NULL)
                return FALSE;

        type = g_file_info_get_file_type (info);

        if (type == G_FILE_TYPE_DIRECTORY)
                return directory_delete_recursive (file, error);
        else
                return g_file_delete (file, NULL, error);
}

void
delete_custom_theme_dir (void)
{
        g_autofree gchar *dir = NULL;
        g_autoptr(GFile) file = NULL;

        dir = custom_theme_dir_path (NULL);
        file = g_file_new_for_path (dir);
        capplet_file_delete_recursive (file, NULL);

        g_debug ("deleted the custom theme dir");
}

gboolean
custom_theme_dir_is_empty (void)
{
        g_autofree gchar *dir = NULL;
        g_autoptr(GFile)  file = NULL;
        gboolean          is_empty;
        GFileEnumerator  *enumerator;
        g_autoptr(GError) error = NULL;

        dir = custom_theme_dir_path (NULL);
        file = g_file_new_for_path (dir);

        is_empty = TRUE;

        enumerator = g_file_enumerate_children (file,
                                                G_FILE_ATTRIBUTE_STANDARD_NAME ","
                                                G_FILE_ATTRIBUTE_STANDARD_TYPE,
                                                G_FILE_QUERY_INFO_NONE,
                                                NULL, &error);
        if (enumerator == NULL) {
                g_warning ("Unable to enumerate files: %s", error->message);
                return TRUE;
        }

        while (is_empty) {
                g_autoptr(GFileInfo) info = NULL;

                info = g_file_enumerator_next_file (enumerator, NULL, NULL);
                if (info == NULL)
                        break;

                if (strcmp ("index.theme", g_file_info_get_name (info)) != 0) {
                        is_empty = FALSE;
                }
        }
        g_file_enumerator_close (enumerator, NULL, NULL);

        return is_empty;
}

typedef enum {
	SOUND_TYPE_OGG,
	SOUND_TYPE_DISABLED
} SoundType;

static void
delete_one_file (const char *sound_name, SoundType sound_type)
{
        g_autofree gchar *name = NULL;
        g_autofree gchar *filename = NULL;
        g_autoptr(GFile) file = NULL;

        switch (sound_type) {
        case SOUND_TYPE_OGG:
                name = g_strdup_printf ("%s.ogg", sound_name);
                break;
        case SOUND_TYPE_DISABLED:
                name = g_strdup_printf ("%s.disabled", sound_name);
                break;
        default:
                g_assert_not_reached ();
        }

        filename = custom_theme_dir_path (name);
        file = g_file_new_for_path (filename);
        capplet_file_delete_recursive (file, NULL);
}

void
delete_old_files (const char **sounds)
{
        guint i;

        for (i = 0; sounds[i] != NULL; i++)
                delete_one_file (sounds[i], SOUND_TYPE_OGG);
}

void
delete_disabled_files (const char **sounds)
{
        guint i;

        for (i = 0; sounds[i] != NULL; i++)
                delete_one_file (sounds[i], SOUND_TYPE_DISABLED);
}

static void
create_one_file (GFile *file)
{
        g_autoptr(GFileOutputStream) stream = NULL;

        stream = g_file_create (file, G_FILE_CREATE_NONE, NULL, NULL);
        if (stream != NULL)
                g_output_stream_close (G_OUTPUT_STREAM (stream), NULL, NULL);
}

void
add_disabled_file (const char **sounds)
{
        guint i;

        for (i = 0; sounds[i] != NULL; i++) {
                g_autofree gchar *name = NULL;
                g_autofree gchar *filename = NULL;
                g_autoptr(GFile) file = NULL;

                name = g_strdup_printf ("%s.disabled", sounds[i]);
                filename = custom_theme_dir_path (name);
                file = g_file_new_for_path (filename);

                create_one_file (file);
        }
}

void
add_custom_file (const char **sounds, const char *filename)
{
        guint i;

        for (i = 0; sounds[i] != NULL; i++) {
                g_autofree gchar *name = NULL;
                g_autofree gchar *path = NULL;
                g_autoptr(GFile) file = NULL;

                /* We use *.ogg because it's the first type of file that
                 * libcanberra looks at */
                name = g_strdup_printf ("%s.ogg", sounds[i]);
                path = custom_theme_dir_path (name);
                /* In case there's already a link there, delete it */
                g_unlink (path);
                file = g_file_new_for_path (path);

                /* Create the link */
                g_file_make_symbolic_link (file, filename, NULL, NULL);
        }
}

void
create_custom_theme (const char *parent)
{
        g_autofree gchar    *path = NULL;
        g_autoptr(GKeyFile)  keyfile = NULL;
        g_autofree gchar    *data = NULL;
        g_autofree gchar    *index_path = NULL;

        /* Create the custom directory */
        path = custom_theme_dir_path (NULL);
        g_mkdir_with_parents (path, USER_DIR_MODE);

        /* Set the data for index.theme */
        keyfile = g_key_file_new ();
        g_key_file_set_string (keyfile, "Sound Theme", "Name", _("Custom"));
        g_key_file_set_string (keyfile, "Sound Theme", "Inherits", parent);
        g_key_file_set_string (keyfile, "Sound Theme", "Directories", ".");
        data = g_key_file_to_data (keyfile, NULL, NULL);

        /* Save the index.theme */
        index_path = custom_theme_dir_path ("index.theme");
        g_file_set_contents (index_path, data, -1, NULL);

        custom_theme_update_time ();
}
