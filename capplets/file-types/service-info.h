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

#include "model-entry.h"

G_BEGIN_DECLS

#define SERVICE_INFO(obj) ((ServiceInfo *) obj)

typedef struct _ServiceInfo ServiceInfo;

struct _ServiceInfo {
	ModelEntry               entry;

	gchar                   *protocol;
	gchar                   *description;
	gboolean                 run_program;

	GnomeVFSMimeApplication *app;
};

void         load_all_services            (GtkTreeModel      *model);

ServiceInfo *service_info_new             (const gchar       *protocol,
					   GtkTreeModel      *model);
void         service_info_load_all        (ServiceInfo       *info);
const gchar *service_info_get_description (ServiceInfo       *info);
gboolean     service_info_using_custom_app (const ServiceInfo *info);
void         service_info_save            (const ServiceInfo *info);
void         service_info_delete          (const ServiceInfo *info);
void         service_info_free            (ServiceInfo       *info);

const GList *get_apps_for_service_type    (gchar             *protocol);
ModelEntry  *get_services_category_entry  (GtkTreeModel      *model);

ServiceInfo *get_service_info             (const gchar       *protocol);

G_END_DECLS

#endif /* __SERVICE_INFO_H */
