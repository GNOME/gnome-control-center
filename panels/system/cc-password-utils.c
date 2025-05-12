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

static char *
get_word_at_line (GFile *file,
                  guint  line_number)
{
  g_autoptr (GError) error = NULL;
  g_autoptr (GFileInputStream) input_stream = NULL;
  g_autoptr (GDataInputStream) data = NULL;
  g_autofree char *line = NULL;
  gsize length;
  guint line_count = 0;

  input_stream = g_file_read (file, NULL, &error);

  if (error != NULL)
    {
      g_warning ("Failed to open file: %s", error->message);
      return NULL;
    }

  data = g_data_input_stream_new (G_INPUT_STREAM (input_stream));

  while ((line = g_data_input_stream_read_line_utf8 (data, &length, NULL, &error)) != NULL)
    {
      if (line_count == line_number)
        return g_str_to_ascii (line, NULL);

      g_clear_pointer (&line, g_free);
      line_count++;
    }

  if (error != NULL)
    {
      g_warning ("Failed to read file: %s", error->message);
      return NULL;
    }

  if (line_count == 0)
    {
      g_warning ("File is empty");
      return NULL;
    }

  if (line_number >= line_count)
    return get_word_at_line (file, line_number % line_count);

  return NULL;
}

char *
cc_generate_password (void)
{
  g_autoptr(GString) password_string = NULL;
  g_autoptr(GFile) file = NULL;
  static const int number_of_words = 5;

  file = g_file_new_for_uri ("resource://org/gnome/control-center/system/wordlist.txt");

  if (!file)
    return NULL;

  password_string = g_string_new (NULL);

  for (int i = 0; i < number_of_words; i++)
    {
      int word_offset = g_random_int ();
      g_autofree char *word = NULL;

      word = get_word_at_line (file, word_offset);

      if (word == NULL)
        return NULL;

      /* Capitalize every first letter */
      word[0] = g_ascii_toupper (word[0]);

      g_string_append (password_string, word);
    }
    
  return g_string_free_and_steal (g_steal_pointer (&password_string));
}
