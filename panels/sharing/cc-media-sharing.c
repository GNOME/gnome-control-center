/*
 * Copyright (C) 2013 Intel, Inc
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Author: Thomas Wood <thomas.wood@intel.com>
 *
 */

#include "cc-media-sharing.h"

#include <gio/gio.h>
#include <gio/gdesktopappinfo.h>
#include <glib/gstdio.h>

static GKeyFile*
cc_media_sharing_open_key_file (void)
{
  gchar *path;
  GKeyFile *file;

  file = g_key_file_new ();

  path = g_build_filename (g_get_user_config_dir (), "rygel.conf", NULL);

  g_key_file_load_from_file (file, path,
                             G_KEY_FILE_KEEP_COMMENTS | G_KEY_FILE_KEEP_TRANSLATIONS,
                             NULL);

  g_free (path);

  return file;
}

void
cc_media_sharing_get_preferences (gchar  ***folders)
{
  GKeyFile *file;

  file = cc_media_sharing_open_key_file ();

  if (folders)
    {
      gsize length;
      GPtrArray *array;
      char **str_list, **orig_list;

      str_list = g_key_file_get_string_list (file, "MediaExport", "uris",
                                             &length, NULL);
      orig_list = str_list;
      array = g_ptr_array_new ();

      while (str_list && *str_list)
        {
          const char *dir;

          if (g_str_equal (*str_list, "@MUSIC@"))
            dir = g_get_user_special_dir (G_USER_DIRECTORY_MUSIC);
	  else if (g_str_equal (*str_list, "@VIDEOS@"))
            dir = g_get_user_special_dir (G_USER_DIRECTORY_VIDEOS);
	  else if (g_str_equal (*str_list, "@PICTURES@"))
            dir = g_get_user_special_dir (G_USER_DIRECTORY_PICTURES);
	  else
	    dir = g_strdup (*str_list);

          if (dir != NULL)
            g_ptr_array_add (array, g_strdup (dir));

          str_list++;
        }

      g_ptr_array_add (array, NULL);

      *folders = (char **) g_ptr_array_free (array, FALSE);
      g_strfreev (orig_list);
    }

  g_key_file_free (file);
}

void
cc_media_sharing_set_preferences (gchar    **folders)
{
  GKeyFile *file;
  gchar **str_list;
  gchar *path;
  gsize length;
  gchar *data;

  file = cc_media_sharing_open_key_file ();

  g_key_file_set_boolean (file, "general", "upnp-enabled", TRUE);
  g_key_file_set_boolean (file, "MediaExport", "enabled", TRUE);

  str_list = folders;
  length = 0;

  while (str_list && *str_list)
    {
      if (g_strcmp0 (*str_list, g_get_user_special_dir (G_USER_DIRECTORY_MUSIC)) == 0)
        {
          g_free (*str_list);
          *str_list = g_strdup ("@MUSIC@");
        }

      if (g_strcmp0 (*str_list, g_get_user_special_dir (G_USER_DIRECTORY_VIDEOS)) == 0)
        {
          g_free (*str_list);
          *str_list = g_strdup ("@VIDEOS@");
        }

      if (g_strcmp0 (*str_list, g_get_user_special_dir (G_USER_DIRECTORY_PICTURES)) == 0)
        {
          g_free (*str_list);
          *str_list = g_strdup ("@PICTURES@");
        }

      str_list++;
      length++;
    }

  g_key_file_set_string_list (file, "MediaExport", "uris", (const gchar**) folders, length);

  data = g_key_file_to_data (file, NULL, NULL);

  path = g_build_filename (g_get_user_config_dir (), "rygel.conf", NULL);

  g_file_set_contents (path, data, -1, NULL);

  g_free (path);

  g_key_file_free (file);
}
