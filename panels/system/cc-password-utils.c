/*
 * Copyright (C) 2024 Red Hat, Inc.
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
 */

#include "cc-password-utils.h"

#include <gio/gio.h>
#include <glib.h>
#include <glob.h>

static GPtrArray *
get_ascii_lines_from_uri (const char *uri)
{
  g_autoptr(GFile) file = NULL;
  g_autoptr(GError) error = NULL;
  g_autoptr(GFileInputStream) input_stream = NULL;
  g_autoptr(GDataInputStream) data = NULL;
  g_autoptr(GPtrArray) ascii_lines = NULL;
  g_autofree char *line = NULL;

  file = g_file_new_for_uri (uri);

  if (!file)
    return NULL;

  input_stream = g_file_read (file, NULL, &error);

  if (error != NULL)
    {
      g_warning ("Failed to open file: %s", error->message);
      return NULL;
    }

  data = g_data_input_stream_new (G_INPUT_STREAM (input_stream));
  ascii_lines = g_ptr_array_new_with_free_func (g_free);

  while ((line = g_data_input_stream_read_line_utf8 (data, NULL, NULL, &error)) != NULL)
    {
      g_ptr_array_add (ascii_lines, g_str_to_ascii (line, NULL));

      g_clear_pointer (&line, g_free);
    }

  if (error != NULL)
    {
      g_warning ("Failed to read file: %s", error->message);
      return NULL;
    }

  return g_steal_pointer (&ascii_lines);
}

char *
cc_generate_password (void)
{
  g_autoptr(GString) password_string = NULL;
  g_autoptr(GPtrArray) ascii_lines = NULL;
  static const int number_of_password_words = 5;
  int number_of_file_words;

  ascii_lines = get_ascii_lines_from_uri ("resource://org/gnome/control-center/system/wordlist.txt");
  
  if (!ascii_lines)
    return NULL;

  number_of_file_words = ascii_lines->len;
  password_string = g_string_new (NULL);

  for (int i = 0; i < number_of_password_words; i++)
    {
      int word_offset = g_random_int_range (0, number_of_file_words);
      char *word;

      word = ascii_lines->pdata[word_offset];

      /* Capitalize every first letter */
      word[0] = g_ascii_toupper (word[0]);

      g_string_append (password_string, word);
    }

  return g_string_free_and_steal (g_steal_pointer (&password_string));
}
