/* -*- mode: c; style: linux -*- */

/* cluster-location.h
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

#ifndef __CLUSTER_LOCATION_H
#define __CLUSTER_LOCATION_H

#include <gnome.h>

#include "location.h"

BEGIN_GNOME_DECLS

#define CLUSTER_LOCATION(obj)          GTK_CHECK_CAST (obj, cluster_location_get_type (), ClusterLocation)
#define CLUSTER_LOCATION_CLASS(klass)  GTK_CHECK_CLASS_CAST (klass, cluster_location_get_type (), ClusterLocationClass)
#define IS_CLUSTER_LOCATION(obj)       GTK_CHECK_TYPE (obj, cluster_location_get_type ())

typedef struct _ClusterLocation ClusterLocation;
typedef struct _ClusterLocationClass ClusterLocationClass;
typedef struct _ClusterLocationPrivate ClusterLocationPrivate;

struct _ClusterLocation 
{
	Location parent;

	ClusterLocationPrivate *p;
};

struct _ClusterLocationClass 
{
	LocationClass location_class;
};

GType cluster_location_get_type         (void);

GtkObject *cluster_location_new         (void);

END_GNOME_DECLS

#endif /* __CLUSTER_LOCATION_H */
