/* -*- mode: c; style: linux -*- */

/* cluster.c
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

#include <parser.h>

#include "cluster.h"

enum {
	ARG_0,
	ARG_SAMPLE
};

typedef struct _SlaveHost SlaveHost;

struct _SlaveHost
{
	gchar *hostname;
};

struct _ClusterPrivate 
{
	GList *slave_hosts;
};

static ArchiveClass *parent_class;

static void        cluster_init              (Cluster *cluster);
static void        cluster_class_init        (ClusterClass *class);

static void        cluster_set_arg           (GtkObject *object, 
					      GtkArg *arg, 
					      guint arg_id);
static void        cluster_get_arg           (GtkObject *object, 
					      GtkArg *arg, 
					      guint arg_id);

static void        cluster_destroy           (GtkObject *object);
static void        cluster_finalize          (GtkObject *object);

static gboolean    cluster_construct         (Cluster *cluster,
					      gboolean is_new);

static gboolean    load_metadata             (Cluster *cluster);
static void        save_metadata             (Cluster *cluster);

static void        cluster_add_slave_host    (Cluster *cluster,
					      SlaveHost *slave_host);
static void        cluster_remove_slave_host (Cluster *cluster,
					      SlaveHost *slave_host);

static SlaveHost  *find_slave_host           (Cluster *cluster,
					      gchar *hostname);
static gchar      *get_metadata_filename     (Cluster *cluster);

static SlaveHost  *slave_host_new            (gchar *hostname);
static SlaveHost  *slave_host_read_xml       (xmlNodePtr node);
static xmlNodePtr  slave_host_write_xml      (SlaveHost *host);

GType
cluster_get_type (void)
{
	static GType cluster_type = 0;

	if (!cluster_type) {
		GtkTypeInfo cluster_info = {
			"Cluster",
			sizeof (Cluster),
			sizeof (ClusterClass),
			(GtkClassInitFunc) cluster_class_init,
			(GtkObjectInitFunc) cluster_init,
			(GtkArgSetFunc) NULL,
			(GtkArgGetFunc) NULL
		};

		cluster_type = 
			gtk_type_unique (archive_get_type (), 
					 &cluster_info);
	}

	return cluster_type;
}

static void
cluster_init (Cluster *cluster)
{
	cluster->p = g_new0 (ClusterPrivate, 1);
}

static void
cluster_class_init (ClusterClass *class) 
{
	GtkObjectClass *object_class;

	gtk_object_add_arg_type ("Cluster::sample",
				 GTK_TYPE_POINTER,
				 GTK_ARG_READWRITE,
				 ARG_SAMPLE);

	object_class = GTK_OBJECT_CLASS (class);
	object_class->destroy = cluster_destroy;
	object_class->finalize = cluster_finalize;
	object_class->set_arg = cluster_set_arg;
	object_class->get_arg = cluster_get_arg;

	parent_class = ARCHIVE_CLASS
		(gtk_type_class (archive_get_type ()));
}

static void
cluster_set_arg (GtkObject *object, GtkArg *arg, guint arg_id) 
{
	Cluster *cluster;

	g_return_if_fail (object != NULL);
	g_return_if_fail (IS_CLUSTER (object));

	cluster = CLUSTER (object);

	switch (arg_id) {
	case ARG_SAMPLE:
		break;

	default:
		g_warning ("Bad argument set");
		break;
	}
}

static void
cluster_get_arg (GtkObject *object, GtkArg *arg, guint arg_id) 
{
	Cluster *cluster;

	g_return_if_fail (object != NULL);
	g_return_if_fail (IS_CLUSTER (object));

	cluster = CLUSTER (object);

	switch (arg_id) {
	case ARG_SAMPLE:
		break;

	default:
		g_warning ("Bad argument get");
		break;
	}
}

static void
cluster_destroy (GtkObject *object) 
{
	Cluster *cluster;

	g_return_if_fail (object != NULL);
	g_return_if_fail (IS_CLUSTER (object));

	cluster = CLUSTER (object);

	save_metadata (cluster);

	GTK_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
cluster_finalize (GtkObject *object) 
{
	Cluster *cluster;

	g_return_if_fail (object != NULL);
	g_return_if_fail (IS_CLUSTER (object));

	cluster = CLUSTER (object);

	g_free (cluster->p);

	GTK_OBJECT_CLASS (parent_class)->finalize (object);
}

GtkObject *
cluster_new (gchar *prefix) 
{
	GtkObject *object;

	object = gtk_object_new (cluster_get_type (),
				 "prefix", prefix,
				 "is-global", TRUE,
				 NULL);

	if (cluster_construct (CLUSTER (object), TRUE) == FALSE) {
		gtk_object_destroy (object);
		return NULL;
	}

	return object;
}

GtkObject *
cluster_load (gchar *prefix) 
{
	GtkObject *object;

	object = gtk_object_new (cluster_get_type (),
				 "prefix", prefix,
				 "is-global", TRUE,
				 NULL);

	if (cluster_construct (CLUSTER (object), FALSE) == FALSE) {
		gtk_object_destroy (object);
		return NULL;
	}

	return object;
}

void
cluster_foreach_host (Cluster *cluster, ClusterHostCB callback, gpointer data) 
{
	GList *node;
	SlaveHost *host;

	g_return_if_fail (cluster != NULL);
	g_return_if_fail (IS_CLUSTER (cluster));
	g_return_if_fail (callback != NULL);

	for (node = cluster->p->slave_hosts; node; node = node->next) {
		host = node->data;
		if (callback (cluster, host->hostname, data)) break;
	}	
}

void
cluster_add_host (Cluster *cluster, gchar *hostname) 
{
	g_return_if_fail (cluster != NULL);
	g_return_if_fail (IS_CLUSTER (cluster));
	g_return_if_fail (hostname != NULL);

	cluster_add_slave_host (cluster, slave_host_new (hostname));
}

void
cluster_remove_host (Cluster *cluster, gchar *hostname) 
{
	g_return_if_fail (cluster != NULL);
	g_return_if_fail (IS_CLUSTER (cluster));
	g_return_if_fail (hostname != NULL);

	cluster_remove_slave_host (cluster,
				   find_slave_host (cluster, hostname));
}

static gboolean
cluster_construct (Cluster *cluster, gboolean is_new) 
{
	if (archive_construct (ARCHIVE (cluster), is_new) == FALSE)
		return FALSE;

	if (!is_new) {
		if (load_metadata (cluster) == FALSE)
			return FALSE;
	}

	return TRUE;
}

/* Loads the metadata associated with the cluster; returns TRUE on success and
 * FALSE on failure
 */

static gboolean
load_metadata (Cluster *cluster)
{
	xmlDocPtr doc;
	xmlNodePtr root_node, node;
	gchar *filename;
	SlaveHost *new_host;

	filename = get_metadata_filename (cluster);
	doc = xmlParseFile (filename);
	g_free (filename);

	if (doc == NULL) return FALSE;

	root_node = xmlDocGetRootElement (doc);
	if (strcmp (root_node->name, "cluster")) {
		xmlFreeDoc (doc);
		return FALSE;
	}

	for (node = root_node->childs; node != NULL; node = node->next) {
		if (!strcmp (node->name, "host")) {
			new_host = slave_host_read_xml (node);
			if (new_host != NULL)
				cluster_add_slave_host (cluster, new_host);
		}
	}

	xmlFreeDoc (doc);

	return TRUE;
}

static void
save_metadata (Cluster *cluster) 
{
	xmlDocPtr doc;
	xmlNodePtr root_node;
	GList *list_node;
	gchar *filename;

	doc = xmlNewDoc ("1.0");
	root_node = xmlNewDocNode (doc, NULL, "cluster", NULL);

	for (list_node = cluster->p->slave_hosts; list_node != NULL;
	     list_node = list_node->next)
	{
		xmlAddChild (root_node,
			     slave_host_write_xml (list_node->data));
	}

	xmlDocSetRootElement (doc, root_node);

	filename = get_metadata_filename (cluster);
	xmlSaveFile (filename, doc);
	g_free (filename);
}

/* Adds a slave host to the list of slave hosts for this cluster */

static void
cluster_add_slave_host (Cluster *cluster, SlaveHost *slave_host)
{
	if (slave_host == NULL) return;

	cluster->p->slave_hosts =
		g_list_append (cluster->p->slave_hosts, slave_host);
}

static void
cluster_remove_slave_host (Cluster *cluster, SlaveHost *slave_host) 
{
	if (slave_host == NULL) return;

	cluster->p->slave_hosts =
		g_list_remove (cluster->p->slave_hosts, slave_host);
}

static SlaveHost *
find_slave_host (Cluster *cluster, gchar *hostname) 
{
	SlaveHost *host;
	GList *node;

	g_return_val_if_fail (hostname != NULL, NULL);

	for (node = cluster->p->slave_hosts; node != NULL; node = node->next) {
		host = node->data;

		if (!strcmp (host->hostname, hostname))
			return host;
	}

	return NULL;
}

/* Returns the filename of the metadata file (should be freed after use) */

static gchar *
get_metadata_filename (Cluster *cluster) 
{
	return g_concat_dir_and_file
		(archive_get_prefix (ARCHIVE (cluster)), "cluster.xml");
}

/* Constructs a new slave host structure */

static SlaveHost *
slave_host_new (gchar *hostname)
{
	SlaveHost *new_host;

	new_host = g_new0 (SlaveHost, 1);
	new_host->hostname = g_strdup (hostname);

	return new_host;
}

/* Constructs a new slave host structure from an XML node */

static SlaveHost *
slave_host_read_xml (xmlNodePtr node)
{
	gchar *hostname;

	hostname = xmlGetProp (node, "name");

	if (hostname != NULL)
		return slave_host_new (hostname);
	else
		return NULL;
}

/* Constructs an XML node for the slave host */

static xmlNodePtr
slave_host_write_xml (SlaveHost *host) 
{
	xmlNodePtr node;

	node = xmlNewNode (NULL, "host");
	xmlNewProp (node, "name", host->hostname);
	return node;
}
