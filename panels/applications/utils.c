/* utils.c
 *
 * Copyright 2018 Matthias Clasen <matthias.clasen@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <config.h>
#include <glib/gi18n.h>

#include "utils.h"

void
file_remove_recursively (GFile *file)
{
  const char *argv[] = { "rm", "-rf", "path", NULL };

  /* FIXME: async, in process */
  argv[2] = g_file_peek_path (file);

  g_spawn_sync (NULL, (char **)argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, NULL, NULL, NULL);
}

guint64
file_size_recursively (GFile *file)
{
  const char *argv[] = { "du", "-s", "path", NULL };
  g_autofree char *out = NULL;
  guint64 size = 0;
  
  /* FIXME: async, in process */
  argv[2] = g_file_peek_path (file);

  if (!g_spawn_sync (NULL, (char **)argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL, &out, NULL, NULL, NULL))
    return 0;

  size = strtoul (out, NULL, 10);

  return size;
}

void
container_remove_all (GtkContainer *container)
{
  GList *children, *l;

  children = gtk_container_get_children (container);
  for (l = children; l; l = l->next)
    {
      gtk_widget_destroy (GTK_WIDGET (l->data));
    }

  g_list_free (children);
}

