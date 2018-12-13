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
#include <flatpak/flatpak.h>
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
  GList *children, *l;

  children = gtk_container_get_children (container);
  for (l = children; l; l = l->next)
    {
      gtk_widget_destroy (GTK_WIDGET (l->data));
    }

  g_list_free (children);
}

FlatpakInstalledRef *
find_flatpak_ref (const gchar *app_id)
{
  g_autoptr(FlatpakInstallation) inst = NULL;
  g_autoptr(GPtrArray) array = NULL;
  FlatpakInstalledRef *ref;
  gint i;

  inst = flatpak_installation_new_user (NULL, NULL);
  ref = flatpak_installation_get_current_installed_app (inst, app_id, NULL, NULL);
  if (ref)
    return ref;

  array = flatpak_get_system_installations (NULL, NULL);
  for (i = 0; i < array->len; i++)
    {
      FlatpakInstallation *si = g_ptr_array_index (array, i);
      ref = flatpak_installation_get_current_installed_app (si, app_id, NULL, NULL);
      if (ref)
        return ref;
    }

  return NULL;
}

guint64
get_flatpak_app_size (const gchar *app_id)
{
  g_autoptr(FlatpakInstalledRef) ref = NULL;

  ref = find_flatpak_ref (app_id);
  if (ref)
    return flatpak_installed_ref_get_installed_size (ref);

  return 0;
}

char *
get_app_id (GAppInfo *info)
{
  gchar *app_id = g_strdup (g_app_info_get_id (info));

  if (g_str_has_suffix (app_id, ".desktop"))
    app_id[strlen (app_id) - strlen (".desktop")] = '\0';

  return app_id;
}
