/* -*- mode: c; style: linux -*- */

/* mime-type-info.h
 *
 * Copyright (C) 2002 Ximian, Inc.
 *
 * Written by Bradford Hovinen <hovinen@ximian.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef __MIME_TYPE_INFO_H
#define __MIME_TYPE_INFO_H

#include <gnome.h>
#include <bonobo.h>
#include <libgnomevfs/gnome-vfs-mime-info.h>
#include <libgnomevfs/gnome-vfs-mime-handlers.h>

G_BEGIN_DECLS

typedef struct _MimeTypeInfo MimeTypeInfo;

struct _MimeTypeInfo
{
	GtkTreeModel            *model;
	GtkTreeIter             *iter;

	gchar                   *mime_type;
	gchar                   *description;
	gchar                   *icon_name;
	GList                   *file_extensions;

	gchar                   *default_component_id;
	GnomeVFSMimeApplication *default_action;
	gchar                   *custom_line;
	gboolean                 needs_terminal;
	gchar                   *edit_line;
	gchar                   *print_line;

	gboolean                 is_category;
};

MimeTypeInfo *mime_type_info_load   (GtkTreeModel       *model,
				     GtkTreeIter        *iter);
void          mime_type_info_save   (const MimeTypeInfo *info);
void          mime_type_info_update (MimeTypeInfo       *info);
void          mime_type_info_free   (MimeTypeInfo       *info);

char         *mime_type_get_pretty_name_for_server (Bonobo_ServerInfo *server);

void          mime_type_append_to_dirty_list       (MimeTypeInfo      *info);
void          mime_type_remove_from_dirty_list     (const gchar       *mime_type);
void          mime_type_commit_dirty_list          (void);

G_END_DECLS

#endif /* __MIME_TYPE_INFO_H */
