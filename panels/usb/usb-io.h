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

#pragma once

#include <glib.h>

#include <dirent.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

G_BEGIN_DECLS

/* *INDENT-OFF* */
G_DEFINE_AUTOPTR_CLEANUP_FUNC (DIR, closedir);
/* *INDENT-ON* */

int        usb_open (const char *path,
                     int         flags,
                     int         mode,
                     GError    **error);

gboolean   usb_close (int      fd,
                      GError **error);

DIR *      usb_opendir (const char *path,
                        GError    **error);

gboolean   usb_closedir (DIR     *d,
                         GError **error);

G_END_DECLS

