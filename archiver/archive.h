/* -*- mode: c; style: linux -*- */

/* archive.h
 * Copyright (C) 2000 Helix Code, Inc.
 *
 * Written by Bradford Hovinen (hovinen@helixcode.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

#ifndef __ARCHIVE_H
#define __ARCHIVE_H

#include <gnome.h>

#include "location.h"
#include "backend-list.h"

#define ARCHIVE(obj)          GTK_CHECK_CAST (obj, archive_get_type (), Archive)
#define ARCHIVE_CLASS(klass)  GTK_CHECK_CLASS_CAST (klass, archive_get_type (), ArchiveClass)
#define IS_ARCHIVE(obj)       GTK_CHECK_TYPE (obj, archive_get_type ())

typedef struct _ArchiveClass ArchiveClass;
typedef gint (*LocationCB) (Archive *, Location *, gpointer);

struct _Archive 
{
	GtkObject    object;

	gchar       *prefix;
	GTree       *locations;
	gboolean     is_global;

	gchar       *current_location_id;

	BackendList *backend_list;
};

struct _ArchiveClass 
{
	GtkObjectClass parent;
};

guint        archive_get_type                (void);

GtkObject   *archive_load                    (gboolean is_global);

void         archive_close                   (Archive *archive);

Location    *archive_get_location            (Archive *archive,
					      const gchar *location);
void         archive_register_location       (Archive *archive,
					      Location *location);
void         archive_unregister_location     (Archive *archive,
					      Location *location);

Location    *archive_get_current_location    (Archive *archive);
void         archive_set_current_location    (Archive *archive,
					      Location *location);

const gchar *archive_get_current_location_id (Archive *archive);
void         archive_set_current_location_id (Archive *archive,
					      const gchar *locid);

const gchar *archive_get_prefix              (Archive *archive);
gboolean     archive_is_global               (Archive *archive);

BackendList *archive_get_backend_list        (Archive *archive);

void         archive_foreach_child_location  (Archive *archive,
					      LocationCB callback,
					      Location *parent,
					      gpointer data);

#endif /* __ARCHIVE */
