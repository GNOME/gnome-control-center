/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*-
 *
 * Copyright (C) 2019 Canonical Ltd.
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

#include "cc-os-release.h"

gchar *
cc_os_release_get_value (const gchar *key)
{
  g_autoptr(GHashTable) values = NULL;

  values = cc_os_release_get_values ();
  if (values == NULL)
    return NULL;

  return g_strdup (g_hash_table_lookup (values, key));
}

GHashTable *
cc_os_release_get_values (void)
{
  g_autoptr(GHashTable) values = NULL;
  g_autofree gchar *buffer = NULL;
  g_auto(GStrv) lines = NULL;
  int i;
  g_autoptr(GError) error = NULL;

  values = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);

  if (!g_file_get_contents ("/etc/os-release", &buffer, NULL, &error))
    {
       if (!g_error_matches (error, G_FILE_ERROR, G_FILE_ERROR_NOENT))
         return NULL;

       if (!g_file_get_contents ("/usr/lib/os-release", &buffer, NULL, NULL))
         return NULL;
    }

  /* Default values in spec */
  g_hash_table_insert (values, g_strdup ("NAME"), g_strdup ("Linux"));
  g_hash_table_insert (values, g_strdup ("ID"), g_strdup ("Linux"));
  g_hash_table_insert (values, g_strdup ("PRETTY_NAME"), g_strdup ("Linux"));

  lines = g_strsplit (buffer, "\n", -1);
  for (i = 0; lines[i] != NULL; i++)
    {
      gchar *line = lines[i];
      g_auto(GStrv) tokens = NULL;
      const gchar *key, *value;
      g_autofree gchar *unquoted_value = NULL;

      /* Skip comments */
      if (g_str_has_prefix (line, "#"))
        continue;

      tokens = g_strsplit (line, "=", 2);
      if (g_strv_length (tokens) < 2)
        continue;
      key = tokens[0];
      value = tokens[1];
      unquoted_value = g_shell_unquote (value, NULL);
      if (unquoted_value != NULL)
        value = unquoted_value;

      g_hash_table_insert (values, g_strdup (key), g_strdup (value));
    }

  return g_steal_pointer (&values);
}
