/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * Copyright (C) 2019 GNOME
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
 * Authors: Ludovico de Nittis <denittis@gnome.org>
 *
 */

#include "config.h"

#include <gio/gio.h>
#include <glib/gstdio.h>

#include <errno.h>
#include <stdio.h>
#include <string.h>

#include "usb-io.h"

G_DEFINE_AUTOPTR_CLEANUP_FUNC (FILE, fclose);

int
usb_open (const char *path,
          int flags,
          int mode,
          GError **error)
{
  int fd;

  g_return_val_if_fail (error == NULL || *error == NULL, -1);

  fd = g_open (path, flags, mode);

  if (fd < 0)
    {
      gint code = g_io_error_from_errno (errno);
      g_set_error (error, G_IO_ERROR, code,
                   "could not open '%s': %s",
                   path, g_strerror (errno));
      return -1;
    }

  return fd;
}

gboolean
usb_close (int fd,
           GError **error)
{
  int r;

  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  r = close (fd);

  if (r == 0)
    return TRUE;

  g_set_error (error, G_IO_ERROR,
               g_io_error_from_errno (errno),
               "could not close file: %s",
               g_strerror (errno));

  return FALSE;
}

DIR *
usb_opendir (const char *path,
             GError    **error)
{
  DIR *d = NULL;

  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  d = opendir (path);
  if (d == NULL)
    {
      g_set_error (error,
                   G_IO_ERROR,
                   g_io_error_from_errno (errno),
                   "could not open directory ('%s'): %s",
                   path,
                   g_strerror (errno));

      return NULL;
    }

  return d;
}

gboolean
usb_closedir (DIR     *d,
              GError **error)
{
  int r;

  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  r = closedir (d);

  if (r < 0)
    {
      g_set_error (error,
                   G_IO_ERROR,
                   g_io_error_from_errno (errno),
                   "failed close dir: %s",
                   g_strerror (errno));
      return FALSE;
    }

  return TRUE;
}

