/* -*- mode: c; style: linux -*- */

/* cluster-location.c
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "cluster.h"
#include "cluster-location.h"

typedef struct _pair_t pair_t;

enum {
	ARG_0,
	ARG_SAMPLE
};

struct _ClusterLocationPrivate 
{
	/* Private data members */
};

struct _pair_t 
{
	gpointer a, b;
};

static LocationClass *parent_class;

static void     cluster_location_init        (ClusterLocation *cluster_location);
static void     cluster_location_class_init  (ClusterLocationClass *class);

static void     cluster_location_set_arg     (GtkObject *object, 
					      GtkArg *arg, 
					      guint arg_id);
static void     cluster_location_get_arg     (GtkObject *object, 
					      GtkArg *arg, 
					      guint arg_id);

static void     cluster_location_finalize    (GtkObject *object);

static gboolean cluster_location_do_rollback (Location *location,
					      gchar *backend_id,
					      xmlDocPtr doc);

static gboolean host_cb                      (Cluster *cluster,
					      gchar *hostname,
					      pair_t *pair);

GType
cluster_location_get_type (void)
{
	static GType cluster_location_type = 0;

	if (!cluster_location_type) {
		GtkTypeInfo cluster_location_info = {
			"ClusterLocation",
			sizeof (ClusterLocation),
			sizeof (ClusterLocationClass),
			(GtkClassInitFunc) cluster_location_class_init,
			(GtkObjectInitFunc) cluster_location_init,
			(GtkArgSetFunc) NULL,
			(GtkArgGetFunc) NULL
		};

		cluster_location_type = 
			gtk_type_unique (location_get_type (), 
					 &cluster_location_info);
	}

	return cluster_location_type;
}

static void
cluster_location_init (ClusterLocation *cluster_location)
{
	cluster_location->p = g_new0 (ClusterLocationPrivate, 1);
}

static void
cluster_location_class_init (ClusterLocationClass *class) 
{
	GtkObjectClass *object_class;
	LocationClass *location_class;

	gtk_object_add_arg_type ("ClusterLocation::sample",
				 GTK_TYPE_POINTER,
				 GTK_ARG_READWRITE,
				 ARG_SAMPLE);

	object_class = GTK_OBJECT_CLASS (class);
	object_class->finalize = cluster_location_finalize;
	object_class->set_arg = cluster_location_set_arg;
	object_class->get_arg = cluster_location_get_arg;

	location_class = LOCATION_CLASS (class);
	location_class->do_rollback = cluster_location_do_rollback;

	parent_class = LOCATION_CLASS
		(gtk_type_class (location_get_type ()));
}

static void
cluster_location_set_arg (GtkObject *object, GtkArg *arg, guint arg_id) 
{
	ClusterLocation *cluster_location;

	g_return_if_fail (object != NULL);
	g_return_if_fail (IS_CLUSTER_LOCATION (object));

	cluster_location = CLUSTER_LOCATION (object);

	switch (arg_id) {
	case ARG_SAMPLE:
		break;

	default:
		g_warning ("Bad argument set");
		break;
	}
}

static void
cluster_location_get_arg (GtkObject *object, GtkArg *arg, guint arg_id) 
{
	ClusterLocation *cluster_location;

	g_return_if_fail (object != NULL);
	g_return_if_fail (IS_CLUSTER_LOCATION (object));

	cluster_location = CLUSTER_LOCATION (object);

	switch (arg_id) {
	case ARG_SAMPLE:
		break;

	default:
		g_warning ("Bad argument get");
		break;
	}
}

static void
cluster_location_finalize (GtkObject *object) 
{
	ClusterLocation *cluster_location;

	g_return_if_fail (object != NULL);
	g_return_if_fail (IS_CLUSTER_LOCATION (object));

	cluster_location = CLUSTER_LOCATION (object);

	g_free (cluster_location->p);

	GTK_OBJECT_CLASS (parent_class)->finalize (object);
}

GtkObject *
cluster_location_new (void) 
{
	return gtk_object_new (cluster_location_get_type (),
			       NULL);
}

static gboolean
cluster_location_do_rollback (Location *location, gchar *backend_id,
			      xmlDocPtr doc) 
{
	Cluster *cluster;
	pair_t pair;

	g_return_val_if_fail (location != NULL, FALSE);
	g_return_val_if_fail (IS_CLUSTER_LOCATION (location), FALSE);
	g_return_val_if_fail (backend_id != NULL, FALSE);
	g_return_val_if_fail (doc != NULL, FALSE);

	gtk_object_get (GTK_OBJECT (location), "archive", &cluster, NULL);
	pair.a = doc;
	pair.b = backend_id;
	cluster_foreach_host (cluster, (ClusterHostCB) host_cb, &pair);

	return TRUE;
}

static gboolean
host_cb (Cluster *cluster, gchar *hostname, pair_t *pair) 
{
	xmlDocPtr doc;
	gchar *backend_id, *command;
	FILE *output;

	doc = pair->a;
	backend_id = pair->b;
	command = g_strconcat ("ssh ", hostname, " ", backend_id, " --set",
			       NULL);
	output = popen (command, "w");
	xmlDocDump (output, doc);
	pclose (output);
	g_free (command);

	return FALSE;
}
