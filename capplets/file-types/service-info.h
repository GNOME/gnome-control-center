/* -*- mode: c; style: linux -*- */

/* service-info.h
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

#ifndef __SERVICE_INFO_H
#define __SERVICE_INFO_H

#include <gnome.h>
#include <bonobo.h>
#include <libgnomevfs/gnome-vfs-mime-handlers.h>
#include <gconf/gconf-changeset.h>

G_BEGIN_DECLS

typedef struct _ServiceInfo ServiceInfo;

struct _ServiceInfo {
	gchar                   *protocol;
	gchar                   *description;
	gboolean                 run_program;

	GnomeVFSMimeApplication *app;
	gchar                   *custom_line;
	gboolean                 need_terminal;
};

ServiceInfo *service_info_load (const gchar       *protocol,
				GConfChangeSet    *changeset);
void         service_info_save (const ServiceInfo *info,
				GConfChangeSet    *changeset);
void         service_info_free (ServiceInfo       *info);

G_END_DECLS

#endif /* __SERVICE_INFO_H */
