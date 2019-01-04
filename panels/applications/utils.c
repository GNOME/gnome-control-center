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

#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE 600
#endif

#include <config.h>
#include <glib/gi18n.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <ftw.h>

#include "utils.h"

static gint
ftw_remove_cb (const gchar       *path,
               const struct stat *sb,
               gint               typeflags,
               struct FTW        *ftwbuf)
{
  remove (path);
  return 0;
}

static void
file_remove_thread_func (GTask       *task,
                         gpointer      source_object,
                         gpointer      task_data,
                         GCancellable *cancellable)
{
  GFile *file = source_object;
  g_autofree gchar *path = g_file_get_path (file);

  nftw (path, ftw_remove_cb, 20, FTW_DEPTH);
}

void
file_remove_async (GFile               *file,
                   GAsyncReadyCallback  callback,
                   gpointer             data)
{
  g_autoptr(GTask) task = g_task_new (file, NULL, callback, data);
  g_task_run_in_thread (task, file_remove_thread_func);
}

static GPrivate size_key = G_PRIVATE_INIT (g_free);

static gint
ftw_size_cb (const gchar       *path,
             const struct stat *sb,
             gint               typeflags,
             struct FTW        *ftwbuf)
{
  guint64 *size = (guint64*)g_private_get (&size_key);
  if (typeflags == FTW_F)
    *size += sb->st_size;
  return 0;
}

static void
file_size_thread_func (GTask        *task,
                       gpointer      source_object,
                       gpointer      task_data,
                       GCancellable *cancellable)
{
  GFile *file = source_object;
  g_autofree gchar *path = g_file_get_path (file);
  guint64 *total;

  g_private_replace (&size_key, g_new0 (guint64, 1));

  nftw (path, ftw_size_cb, 20, FTW_DEPTH);

  total = g_new0 (guint64, 1);
  *total = *(guint64*)g_private_get (&size_key);

  g_object_set_data_full (G_OBJECT (task), "size", total, g_free);
}

void
file_size_async (GFile               *file,
                 GAsyncReadyCallback  callback,
                 gpointer             data)
{
  g_autoptr(GTask) task = g_task_new (file, NULL, callback, data);
  g_task_run_in_thread (task, file_size_thread_func);
}

void
container_remove_all (GtkContainer *container)
{
  g_autoptr(GList) children = NULL;
  GList *l;

  children = gtk_container_get_children (container);
  for (l = children; l; l = l->next)
    gtk_widget_destroy (GTK_WIDGET (l->data));
}

static gchar *
get_output_of (const gchar **argv)
{
  g_autofree gchar *output = NULL;
  int status;

  if (!g_spawn_sync (NULL,
                     (gchar**) argv,
                     NULL,
                     G_SPAWN_SEARCH_PATH,
                     NULL, NULL,
                     &output, NULL,
                     &status, NULL))
    return NULL;

  if (!g_spawn_check_exit_status (status, NULL))
    return NULL;

  return g_steal_pointer (&output);
}

GKeyFile *
get_flatpak_metadata (const gchar *app_id)
{
  const gchar *argv[5] = { "flatpak", "info", "-m", "app", NULL };
  g_autofree gchar *data = NULL;
  g_autoptr(GError) error = NULL;
  g_autoptr(GKeyFile) keyfile = NULL;

  argv[3] = app_id;

  data = get_output_of (argv);
  if (data == NULL)
    return NULL;

  keyfile = g_key_file_new ();
  if (!g_key_file_load_from_data (keyfile, data, -1, 0, &error))
    {
      g_warning ("%s", error->message);
      return NULL;
    }

  return g_steal_pointer (&keyfile);
}

guint64
get_flatpak_app_size (const gchar *app_id)
{
  const gchar *argv[5] = { "flatpak", "info", "-s", "app", NULL };
  g_autofree gchar *data = NULL;
  guint64 factor;
  double val;

  argv[3] = app_id;

  data = get_output_of (argv);
  if (data == NULL)
    return 0;

  data = g_strstrip (data);
  
  if (g_str_has_suffix (data, "kB") || g_str_has_suffix (data, "kb"))
    factor = 1000;
  else if (g_str_has_suffix (data, "MB") || g_str_has_suffix (data, "Mb"))
    factor = 1000 * 1000;
  else if (g_str_has_suffix (data, "GB") || g_str_has_suffix (data, "Gb"))
    factor = 1000 * 1000 * 1000;
  else if (g_str_has_suffix (data, "KiB") || g_str_has_suffix (data, "Kib"))
    factor = 1024;
  else if (g_str_has_suffix (data, "MiB") || g_str_has_suffix (data, "Mib"))
    factor = 1024 * 1024;
  else if (g_str_has_suffix (data, "GiB") || g_str_has_suffix (data, "Gib"))
    factor = 1024 * 1024 * 1024;
  else 
    factor = 1;

  val = g_ascii_strtod (data, NULL);

  return (guint64)(val * factor);
}

char *
get_app_id (GAppInfo *info)
{
  gchar *app_id = g_strdup (g_app_info_get_id (info));

  if (g_str_has_suffix (app_id, ".desktop"))
    app_id[strlen (app_id) - strlen (".desktop")] = '\0';

  return app_id;
}
