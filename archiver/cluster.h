/* -*- mode: c; style: linux -*- */

/* cluster.h
 * Copyright (C) 2000 Ximian, Inc.
 *
 * Written by Bradford Hovinen <hovinen@helixcode.com>
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

#ifndef __CLUSTER_H
#define __CLUSTER_H

#include <gnome.h>

#include "archive.h"

BEGIN_GNOME_DECLS

#define CLUSTER(obj)          GTK_CHECK_CAST (obj, cluster_get_type (), Cluster)
#define CLUSTER_CLASS(klass)  GTK_CHECK_CLASS_CAST (klass, cluster_get_type (), ClusterClass)
#define IS_CLUSTER(obj)       GTK_CHECK_TYPE (obj, cluster_get_type ())

typedef struct _Cluster Cluster;
typedef struct _ClusterClass ClusterClass;
typedef struct _ClusterPrivate ClusterPrivate;

typedef gboolean (*ClusterHostCB) (Cluster *, gchar *, gpointer);

struct _Cluster 
{
	Archive parent;

	ClusterPrivate *p;
};

struct _ClusterClass 
{
	ArchiveClass archive_class;
};

GType      cluster_get_type          (void);

GtkObject *cluster_new               (gchar         *prefix);
GtkObject *cluster_load              (gchar         *prefix);

void       cluster_foreach_host      (Cluster       *cluster,
				      ClusterHostCB  callback,
				      gpointer       data);

void       cluster_add_host          (Cluster       *cluster,
				      gchar         *hostname);
void       cluster_remove_host       (Cluster       *cluster,
				      gchar         *hostname);

END_GNOME_DECLS

#endif /* __CLUSTER_H */
