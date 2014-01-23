/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 * vim: set et sw=8 ts=8:
 *
 * Copyright (c) 2008, Novell, Inc.
 * Copyright (c) 2012, Red Hat, Inc.
 *
 * Authors: Vincent Untz <vuntz@gnome.org>
 * Bastien Nocera <hadess@hadess.net>
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

#ifndef __GSD_DISK_SPACE_HELPER_H
#define __GSD_DISK_SPACE_HELPER_H

#include <glib.h>
#include <gio/gunixmounts.h>

G_BEGIN_DECLS

gboolean gsd_should_ignore_unix_mount (GUnixMountEntry *mount);
gboolean gsd_is_removable_mount       (GUnixMountEntry *mount);

G_END_DECLS

#endif /* __GSD_DISK_SPACE_HELPER_H */
