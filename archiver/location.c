/* -*- mode: c; style: linux -*- */

/* location.c
 * Copyright (C) 2000-2001 Ximian, Inc.
 *
 * Written by Bradford Hovinen (hovinen@ximian.com)
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
#  include <config.h>
#endif

#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <tree.h>
#include <parser.h>
#include <errno.h>

#include "location.h"
#include "archive.h"

static GtkObjectClass *parent_class;

enum {
	ARG_0,
	ARG_LOCID,
	ARG_ARCHIVE,
	ARG_INHERITS
};

struct _LocationPrivate 
{
	Archive *archive;
	gchar *locid;
	gchar *fullpath;
	gchar *label;

	Location *inherits_location;
	GList *contains_list;
	gboolean is_new;
	gboolean contains_list_dirty;

	ConfigLog *config_log;
};

static void location_init (Location *location);
static void location_class_init (LocationClass *klass);

static void location_set_arg (GtkObject *object,
			      GtkArg *arg,
			      guint arg_id);

static void location_get_arg (GtkObject *object,
			      GtkArg *arg,
			      guint arg_id);

static void location_destroy       (GtkObject *object);
static void location_finalize      (GtkObject *object);

static gint get_backends_cb        (BackendList *backend_list,
				    gchar *backend_id,
				    Location *location);

static gboolean do_create          (Location *location);
static gboolean do_load            (Location *location);
static gboolean load_metadata_file (Location *location, 
				    char *filename, 
				    gboolean is_default);
static void save_metadata          (Location *location);
static void write_metadata_file    (Location *location, 
				    gchar *filename);

static gboolean do_rollback  (gchar *fullpath, 
			      gchar *backend_id, 
			      gint id);
static gboolean dump_xml_data (gchar *fullpath, 
			       gint id, 
			       gint fd_output);
static gint run_backend_proc (gchar *backend_id);

guint
location_get_type (void) 
{
	static guint location_type;

	if (!location_type) {
		GtkTypeInfo location_info = {
			"Location",
			sizeof (Location),
			sizeof (LocationClass),
			(GtkClassInitFunc) location_class_init,
			(GtkObjectInitFunc) location_init,
			(GtkArgSetFunc) NULL,
			(GtkArgGetFunc) NULL
		};

		location_type = 
			gtk_type_unique (gtk_object_get_type (),
					 &location_info);
	}

	return location_type;
}

static void
location_init (Location *location) 
{
	location->p = g_new0 (LocationPrivate, 1);
	location->p->archive = NULL;
	location->p->locid = NULL;
	location->p->is_new = FALSE;
	location->p->contains_list_dirty = FALSE;
}

static void
location_class_init (LocationClass *klass) 
{
	GtkObjectClass *object_class;

	object_class = GTK_OBJECT_CLASS (klass);

	object_class->destroy = location_destroy;
	object_class->finalize = location_finalize;
	object_class->set_arg = location_set_arg;
	object_class->get_arg = location_get_arg;

	gtk_object_add_arg_type ("Location::archive",
				 GTK_TYPE_POINTER,
				 GTK_ARG_READWRITE,
				 ARG_ARCHIVE);

	gtk_object_add_arg_type ("Location::locid",
				 GTK_TYPE_POINTER,
				 GTK_ARG_READWRITE,
				 ARG_LOCID);

	gtk_object_add_arg_type ("Location::inherits",
				 GTK_TYPE_POINTER,
				 GTK_ARG_READWRITE,
				 ARG_INHERITS);

	parent_class = gtk_type_class (gtk_object_get_type ());
}

static void
location_set_arg (GtkObject *object, GtkArg *arg, guint arg_id) 
{
	Location *location;

	g_return_if_fail (object != NULL);
	g_return_if_fail (IS_LOCATION (object));
	g_return_if_fail (arg != NULL);

	location = LOCATION (object);

	switch (arg_id) {
	case ARG_ARCHIVE:
		g_return_if_fail (GTK_VALUE_POINTER (*arg) != NULL);
		g_return_if_fail (IS_ARCHIVE (GTK_VALUE_POINTER (*arg)));

		location->p->archive = GTK_VALUE_POINTER (*arg);
		break;
	case ARG_LOCID:
		if (GTK_VALUE_POINTER (*arg) != NULL) {
			if (location->p->locid != NULL)
				g_free (location->p->locid);

			location->p->locid = 
				g_strdup (GTK_VALUE_POINTER (*arg));

			if (!strcmp (location->p->locid, "default"))
				location->p->label = _("Default Location");
			else
				location->p->label = location->p->locid;
		}

		break;
	case ARG_INHERITS:
		if (GTK_VALUE_POINTER (*arg) != NULL) {
			g_return_if_fail (IS_LOCATION
					  (GTK_VALUE_POINTER (*arg)));
			location->p->inherits_location =
				GTK_VALUE_POINTER (*arg);
		}
	default:
		break;
	}
}

static void 
location_get_arg (GtkObject *object, GtkArg *arg, guint arg_id) 
{
	Location *location;

	g_return_if_fail (object != NULL);
	g_return_if_fail (IS_LOCATION (object));
	g_return_if_fail (arg != NULL);

	location = LOCATION (object);

	switch (arg_id) {
	case ARG_ARCHIVE:
		GTK_VALUE_POINTER (*arg) = location->p->archive;
		break;
	case ARG_LOCID:
		GTK_VALUE_POINTER (*arg) = location->p->locid;
		break;
	case ARG_INHERITS:
		GTK_VALUE_POINTER (*arg) = location->p->inherits_location;
		break;
	default:
		arg->type = GTK_TYPE_INVALID;
		break;
	}
}

/**
 * location_new:
 * @archive: 
 * @locid: 
 * @inherits: 
 * 
 * Create a new location with the given name and parent identifier in the
 * given archive
 * 
 * Return value: Reference to location
 **/

GtkObject *
location_new (Archive *archive, const gchar *locid, Location *inherits) 
{
	GtkObject *object;

	g_return_val_if_fail (archive != NULL, NULL);
	g_return_val_if_fail (IS_ARCHIVE (archive), NULL);
	g_return_val_if_fail (locid != NULL, NULL);

	object = gtk_object_new (location_get_type (),
				 "archive", archive,
				 "locid", locid,
				 "inherits", inherits,
				 NULL);

	if (!do_create (LOCATION (object))) {
		gtk_object_destroy (object);
		return NULL;
	}

	LOCATION (object)->p->is_new = TRUE;

	archive_register_location (archive, LOCATION (object));

	return object;
}

/**
 * location_open:
 * @archive: 
 * @locid: 
 * 
 * Open the location with the given name from the given archive
 * 
 * Return value: Reference of location
 **/

GtkObject *
location_open (Archive *archive, const gchar *locid) 
{
	GtkObject *object;

	g_return_val_if_fail (archive != NULL, NULL);
	g_return_val_if_fail (IS_ARCHIVE (archive), NULL);
	g_return_val_if_fail (locid != NULL, NULL);

	object = gtk_object_new (location_get_type (),
				 "archive", archive,
				 "locid", locid,
				 NULL);

	if (!do_load (LOCATION (object))) {
		gtk_object_destroy (object);
		return NULL;
	}

	return object;
}

static void 
location_destroy (GtkObject *object) 
{
	Location *location;

	g_return_if_fail (object != NULL);
	g_return_if_fail (IS_LOCATION (object));

	location = LOCATION (object);

	if (location->p->config_log)
		gtk_object_destroy (GTK_OBJECT (location->p->config_log));

	if (location->p->inherits_location)
		gtk_object_unref (GTK_OBJECT (location->p->inherits_location));

	archive_unregister_location (location->p->archive, location);

	GTK_OBJECT_CLASS (parent_class)->destroy (object);
}

static void
location_finalize (GtkObject *object) 
{
	Location *location;

	g_return_if_fail (object != NULL);
	g_return_if_fail (IS_LOCATION (object));

	location = LOCATION (object);

	if (location->p->fullpath)
		g_free (location->p->fullpath);

	g_free (location->p);
	location->p = (LocationPrivate *) 0xdeadbeef;

	GTK_OBJECT_CLASS (parent_class)->finalize (object);
}

/**
 * location_close:
 * @location: 
 * 
 * Close a location handle; this saves the metadata associated with the
 * location and destroys the object
 **/

void
location_close (Location *location) 
{
	g_return_if_fail (location != NULL);
	g_return_if_fail (IS_LOCATION (location));

	save_metadata (location);

	gtk_object_destroy (GTK_OBJECT (location));
}

static gint
data_delete_cb (ConfigLog *config_log, gint id, gchar *backend_id, 
		struct tm *date, Location *location) 
{
	gchar *filename;

	filename = g_strdup_printf ("%s/%08x.xml", location->p->fullpath, id);
	unlink (filename);
	g_free (filename);

	return 0;
}

void
location_delete (Location *location) 
{
	gchar *metadata_filename;

	g_return_if_fail (location != NULL);
	g_return_if_fail (IS_LOCATION (location));

	metadata_filename = g_strconcat (location->p->fullpath,
					 "/location.xml");
	unlink (metadata_filename);
	g_free (metadata_filename);

	config_log_iterate (location->p->config_log,
			    (ConfigLogIteratorCB) data_delete_cb, location);
	config_log_delete (location->p->config_log);

	rmdir (location->p->fullpath);
	gtk_object_destroy (GTK_OBJECT (location));
}

/**
 * location_store:
 * @location: 
 * @backend_id: 
 * @input: 
 * 
 * Store configuration data from the given stream in the location under the
 * given backend id
 **/

void 
location_store (Location *location, gchar *backend_id, FILE *input) 
{
	gint id;
	FILE *output;
	char buffer[16384];
	char *filename;
	size_t size;

	g_return_if_fail (location != NULL);
	g_return_if_fail (IS_LOCATION (location));
	g_return_if_fail (location->p->config_log != NULL);
	g_return_if_fail (IS_CONFIG_LOG (location->p->config_log));

	if (!location_contains (location, backend_id)) {
		if (!location->p->inherits_location)
			g_warning ("Could not find a location in the " \
				   "tree ancestry that stores this " \
				   "backend.");
		else
			location_store (location->p->inherits_location,
					backend_id, input);
	}

	id = config_log_write_entry (location->p->config_log, backend_id);

	filename = g_strdup_printf ("%s/%08x.xml",
				    location->p->fullpath, id);
	output = fopen (filename, "w");

	if (output == NULL) return;

	while (!feof (input)) {
		size = fread (buffer, sizeof (char), 16384, input);
		fwrite (buffer, sizeof (char), size, output);
	}

	fclose (output);
}

/**
 * location_rollback_backend_to:
 * @location: 
 * @date: 
 * @backend_id: 
 * @parent_chain: 
 * 
 * Roll back the backend with the given id to the given date, optionally
 * chaining to the parent location if the current one does not cover this
 * backend
 **/

void
location_rollback_backend_to (Location *location, struct tm *date, 
			      gchar *backend_id, gboolean parent_chain) 
{
	gint id;

	g_return_if_fail (location != NULL);
	g_return_if_fail (IS_LOCATION (location));
	g_return_if_fail (location->p->config_log != NULL);
	g_return_if_fail (IS_CONFIG_LOG (location->p->config_log));
	g_return_if_fail (backend_id != NULL);

	id = config_log_get_rollback_id_for_date
		(location->p->config_log, date, backend_id);

	if (id != -1)
		do_rollback (location->p->fullpath, backend_id, id);
	else if (parent_chain && location->p->inherits_location != NULL)
		location_rollback_backend_to (location->p->inherits_location,
					      date, backend_id, TRUE);
}

/**
 * location_rollback_backends_to:
 * @location: 
 * @date: 
 * @backends: 
 * @parent_chain: 
 * 
 * Roll back the list of backends specified to the given date, optionally
 * chaining to the parent location. This destroys the list of backends given
 **/

void 
location_rollback_backends_to (Location *location, struct tm *date,
			       GList *backends, gboolean parent_chain) 
{
	gint *id_array, i = 0;
	GList *node;

	g_return_if_fail (location != NULL);
	g_return_if_fail (IS_LOCATION (location));
	g_return_if_fail (location->p->config_log != NULL);
	g_return_if_fail (IS_CONFIG_LOG (location->p->config_log));

	id_array = config_log_get_rollback_ids_for_date
		(location->p->config_log, date, backends);

	if (id_array == NULL) return;

	for (node = backends; node; node = node->next) {
		if (id_array[i] != -1) {
			do_rollback (location->p->fullpath, node->data, 
				     id_array[i]);
			backends = g_list_remove_link (backends, node);
		}

		i++;
	}

	if (parent_chain && location->p->inherits_location != NULL)
		location_rollback_backends_to (location->p->inherits_location,
					       date, backends, TRUE);

	g_free (id_array);
}

/**
 * location_rollback_all_to:
 * @location: 
 * @date: 
 * @parent_chain: 
 * 
 * Roll back all configurations for this location to the given date,
 * optionally chaining to the parent location
 **/

void 
location_rollback_all_to (Location *location, struct tm *date, 
			  gboolean parent_chain) 
{
	gint *id_array, i = 0;
	GList *node;

	g_return_if_fail (location != NULL);
	g_return_if_fail (IS_LOCATION (location));
	g_return_if_fail (location->p->config_log != NULL);
	g_return_if_fail (IS_CONFIG_LOG (location->p->config_log));

	id_array = config_log_get_rollback_ids_for_date
		(location->p->config_log, date, location->p->contains_list);

	if (id_array == NULL) return;

	for (node = location->p->contains_list; node; node = node->next) {
		if (id_array[i] != -1)
			do_rollback (location->p->fullpath, node->data, 
				     id_array[i]);
		i++;
	}

	if (parent_chain && location->p->inherits_location != NULL)
		location_rollback_all_to (location->p->inherits_location,
					  date, TRUE);
}

/**
 * location_rollback_backend_by:
 * @location: 
 * @steps: Number of steps to roll back
 * @backend_id: The backend to roll back
 * @parent_chain: TRUE iff the location should defer to its parent
 * 
 * Roll back a backend a given number of steps
 **/

void
location_rollback_backend_by (Location *location, guint steps, 
			      gchar *backend_id, gboolean parent_chain)
{
	gint id;

	g_return_if_fail (location != NULL);
	g_return_if_fail (IS_LOCATION (location));
	g_return_if_fail (location->p->config_log != NULL);
	g_return_if_fail (IS_CONFIG_LOG (location->p->config_log));
	g_return_if_fail (backend_id != NULL);

	id = config_log_get_rollback_id_by_steps
		(location->p->config_log, steps, backend_id);

	if (id != -1)
		do_rollback (location->p->fullpath, backend_id, id);
	else if (parent_chain && location->p->inherits_location != NULL)
		location_rollback_backend_by (location->p->inherits_location,
					      steps, backend_id, TRUE);
}

/**
 * location_rollback_id:
 * @location: 
 * @id: 
 * 
 * Find the configuration snapshot with the given id number and feed it to the
 * backend associated with it
 **/

void
location_rollback_id (Location *location, gint id) 
{
	gchar *backend_id;

	backend_id = config_log_get_backend_id_for_id
		(location->p->config_log, id);

	if (backend_id)
		do_rollback (location->p->fullpath, backend_id, id);
}

/**
 * location_dump_rollback_data:
 * @location: 
 * @date: 
 * @steps: 
 * @backend_id: 
 * @parent_chain: 
 * @output: 
 * 
 * Output to the given stream configuration for a given backend as of the
 * given date, optionally specifying whether to chain up to the parent
 * location
 **/

void 
location_dump_rollback_data (Location *location, struct tm *date,
			     guint steps, gchar *backend_id,
			     gboolean parent_chain, FILE *output) 
{
	gint id;

	g_return_if_fail (location != NULL);
	g_return_if_fail (IS_LOCATION (location));
	g_return_if_fail (location->p->config_log != NULL);
	g_return_if_fail (IS_CONFIG_LOG (location->p->config_log));

	if (steps > 0)
		id = config_log_get_rollback_id_by_steps
			(location->p->config_log, steps, backend_id);
	else
		id = config_log_get_rollback_id_for_date
			(location->p->config_log, date, backend_id);

	if (id != -1)
		dump_xml_data (location->p->fullpath, id, fileno (output));
	else if (parent_chain && location->p->inherits_location != NULL)
		location_dump_rollback_data (location->p->inherits_location,
					     date, steps, backend_id,
					     TRUE, output);
}

/**
 * location_contains:
 * @location: 
 * @backend_id: 
 * 
 * Determine if a location specifies configuration for the given backend
 * 
 * Return value: TRUE iff the location specifies configuration for the given
 * backend, FALSE otherwise
 **/

gboolean 
location_contains (Location *location, gchar *backend_id) 
{
	GList *node;

	g_return_val_if_fail (location != NULL, FALSE);
	g_return_val_if_fail (IS_LOCATION (location), FALSE);

	for (node = location->p->contains_list; node; node = node->next)
		if (!strcmp (node->data, backend_id)) return TRUE;

	return FALSE;
}

/**
 * location_add_backend:
 * @location: 
 * @backend_id: 
 * 
 * Adds a backend id to the location's metadata, so that the location
 * specifies configuration for that backend
 *
 * Returns: 0 on success, -1 if the location is toplevel, -2 if the backend is
 * not registered with the master list
 **/

gint
location_add_backend (Location *location, gchar *backend_id) 
{
	g_return_val_if_fail (location != NULL, -1);
	g_return_val_if_fail (IS_LOCATION (location), -1);
	g_return_val_if_fail (backend_id != NULL, -2);

	if (location->p->inherits_location == NULL) return -1;

	if (!backend_list_contains 
	    (archive_get_backend_list (location->p->archive),
	     backend_id))
		return -2;

	location->p->contains_list =
		g_list_append (location->p->contains_list, backend_id);
	location->p->contains_list_dirty = TRUE;

	return 0;
}

/**
 * location_remove_backend:
 * @location: 
 * @backend_id: 
 * 
 * Removes a backend id from the location's metadata, so that the location no
 * longer specifies configuration for that backend
 **/

void
location_remove_backend (Location *location, gchar *backend_id) 
{
	GList *node;

	g_return_if_fail (location != NULL);
	g_return_if_fail (IS_LOCATION (location));
	g_return_if_fail (backend_id != NULL);

	if (location->p->inherits_location == NULL) return;

	for (node = location->p->contains_list; node; node = node->next) {
		if (!strcmp (node->data, backend_id)) {
			g_free (node->data);
			location->p->contains_list =
				g_list_remove_link (location->p->contains_list,
						    node);
			location->p->contains_list_dirty = TRUE;
			return;
		}
	}
}

/**
 * location_find_path_from_common_parent:
 * @location: object
 * @location1: The location with which to find the common parent with the
 * object
 * 
 * Finds the common parent between the two locations given and returns a GList
 * of locations starting at the common parent (starts with NULL if there is no
 * common parent) and leading to the location object on which this method is
 * invoked
 * 
 * Return value: A GList of locations
 **/

GList *
location_find_path_from_common_parent (Location *location, 
				       Location *location1)
{
	GList *list_node;
	gint depth = 0, depth1 = 0;
	Location *tmp;

	g_return_val_if_fail (location != NULL, NULL);
	g_return_val_if_fail (IS_LOCATION (location), NULL);
	g_return_val_if_fail (location1 != NULL, NULL);
	g_return_val_if_fail (IS_LOCATION (location1), NULL);

	list_node = g_list_append (NULL, location);

	for (tmp = location; tmp; tmp = tmp->p->inherits_location) depth++;
	for (tmp = location1; tmp; tmp = tmp->p->inherits_location) depth1++;

	while (depth > depth1) {
		location = location->p->inherits_location;
		list_node = g_list_prepend (list_node, location);
		depth--;
	}

	while (depth1 > depth) {
		location1 = location1->p->inherits_location;
		depth1--;
	}

	while (location && location1 && location != location1) {
		location = location->p->inherits_location;
		location1 = location1->p->inherits_location;
		list_node = g_list_prepend (list_node, location);
	}

	return list_node;
}

/**
 * location_get_parent
 * @location:
 *
 * Returns a reference to the location that this location inherits
 *
 * Return value: Reference to parent location
 **/

Location *
location_get_parent (Location *location)
{
	g_return_val_if_fail (location != NULL, NULL);
	g_return_val_if_fail (IS_LOCATION (location), NULL);

	return location->p->inherits_location;
}

/**
 * location_get_path:
 * @location: 
 * 
 * Get the filesystem path to location data
 * 
 * Return value: String with path; should not be freed
 **/

const gchar *
location_get_path (Location *location) 
{
	g_return_val_if_fail (location != NULL, NULL);
	g_return_val_if_fail (IS_LOCATION (location), NULL);

	return location->p->fullpath;
}

/**
 * location_get_label:
 * @location: 
 * 
 * Get the human-readable label associated with the location
 * 
 * Return value: String containing label; should not be freed
 **/

const gchar *
location_get_label (Location *location) 
{
	g_return_val_if_fail (location != NULL, NULL);
	g_return_val_if_fail (IS_LOCATION (location), NULL);

	return location->p->label;
}

/**
 * location_get_id:
 * @location: 
 * 
 * Get the location id
 * 
 * Return value: String containing location id; should not be freed
 **/

const gchar *
location_get_id (Location *location) 
{
	g_return_val_if_fail (location != NULL, NULL);
	g_return_val_if_fail (IS_LOCATION (location), NULL);

	return location->p->locid;
}

/**
 * location_foreach_backend:
 * @location: 
 * @callback: 
 * @data: 
 * 
 * Invokes the function callback on every backend handled by this location
 **/

void
location_foreach_backend (Location *location, LocationBackendCB callback,
			  gpointer data)
{
	GList *node;

	g_return_if_fail (location != NULL);
	g_return_if_fail (IS_LOCATION (location));
	g_return_if_fail (callback != NULL);

	for (node = location->p->contains_list; node; node = node->next)
		if (callback (location, (gchar *) node->data, data)) break;
}

/**
 * location_set_id:
 * @location: 
 * @name: 
 * 
 * Sets the name (or id) of the location
 **/

void
location_set_id (Location *location, const gchar *locid) 
{
	gchar *new_fullpath;

	g_return_if_fail (location != NULL);
	g_return_if_fail (IS_LOCATION (location));
	g_return_if_fail (locid != NULL);

	gtk_object_set (GTK_OBJECT (location), "locid", locid, NULL);

	new_fullpath = g_concat_dir_and_file (archive_get_prefix 
					      (location->p->archive),
					      locid);

	if (rename (location->p->fullpath, new_fullpath) == -1) {
		g_message ("Could not rename %s to %s: %s",
			   location->p->fullpath, new_fullpath,
			   g_strerror (errno));
	}

	g_free (location->p->fullpath);
	location->p->fullpath = new_fullpath;

	config_log_reset_filenames (location->p->config_log);
}

static gint
get_backends_cb (BackendList *backend_list, gchar *backend_id,
		 Location *location) 
{
	location->p->contains_list =
		g_list_prepend (location->p->contains_list,
				backend_id);
	return 0;
}

/* Construct the directory structure for a given location 
 *
 * FIXME: Better error reporting
 */

static gboolean
do_create (Location *location) 
{
	gint ret = 0;

	g_return_val_if_fail (location != NULL, FALSE);
	g_return_val_if_fail (IS_LOCATION (location), FALSE);
	g_return_val_if_fail (location->p->archive != NULL, FALSE);
	g_return_val_if_fail (IS_ARCHIVE (location->p->archive), FALSE);
	g_return_val_if_fail (location->p->locid != NULL, FALSE);

	if (location->p->inherits_location == NULL)
		backend_list_foreach 
			(archive_get_backend_list (location->p->archive),
			 (BackendCB) get_backends_cb,
			 location);

	if (g_file_test (archive_get_prefix (location->p->archive), 
			 G_FILE_TEST_ISDIR) == FALSE)
		return FALSE;

	if (location->p->fullpath) g_free (location->p->fullpath);

	location->p->fullpath =
		g_concat_dir_and_file (archive_get_prefix
				       (location->p->archive),
				       location->p->locid);

	if (g_file_test (location->p->fullpath, G_FILE_TEST_ISDIR) == FALSE)
		ret = mkdir (location->p->fullpath, 
			     S_IREAD | S_IWRITE | S_IEXEC);

	if (ret == -1) return FALSE;

	location->p->config_log = CONFIG_LOG (config_log_open (location));

	return TRUE;
}

/* Load data for a location into the structure. Assumes all the
 * structures are already initialized.
 *
 * Returns TRUE on success and FALSE on failure (directory not present 
 * or config metadata not present)
 */

static gboolean
do_load (Location *location) 
{
	gchar *metadata_filename;

	g_return_val_if_fail (location != NULL, FALSE);
	g_return_val_if_fail (IS_LOCATION (location), FALSE);
	g_return_val_if_fail (location->p->archive != NULL, FALSE);
	g_return_val_if_fail (IS_ARCHIVE (location->p->archive), FALSE);
	g_return_val_if_fail (location->p->locid != NULL, FALSE);

	if (location->p->fullpath) g_free (location->p->fullpath);

	location->p->fullpath =
		g_concat_dir_and_file (archive_get_prefix
				       (location->p->archive),
				       location->p->locid);

	if (g_file_test (location->p->fullpath, G_FILE_TEST_ISDIR) == FALSE)
		return FALSE;

	metadata_filename =
		g_concat_dir_and_file (location->p->fullpath,
				       "location.xml");

	if (!load_metadata_file (location, metadata_filename, FALSE))
		return FALSE;

	g_free (metadata_filename);

	location->p->config_log = CONFIG_LOG (config_log_open (location));

	if (location->p->config_log == NULL)
		return FALSE;

	return TRUE;
}

static gboolean
load_metadata_file (Location *location, char *filename, gboolean is_default) 
{
	xmlDocPtr doc;
	xmlNodePtr root_node, node;
	char *inherits_str = NULL, *contains_str;
	GList *list_head = NULL, *list_tail = NULL;

	g_return_val_if_fail (location != NULL, FALSE);
	g_return_val_if_fail (IS_LOCATION (location), FALSE);
	g_return_val_if_fail (location->p->archive != NULL, FALSE);
	g_return_val_if_fail (IS_ARCHIVE (location->p->archive), FALSE);
	g_return_val_if_fail (filename != NULL, FALSE);

	doc = xmlParseFile (filename);

	if (!doc)
		return FALSE;

	root_node = xmlDocGetRootElement (doc);

	if (strcmp (root_node->name, "location")) {
		xmlFreeDoc (doc);
		return FALSE;
	}

	for (node = root_node->childs; node; node = node->next) {
		if (!strcmp (node->name, "inherits")) {
			inherits_str = xmlGetProp (node, "location");

			if (inherits_str == NULL)
				g_warning ("Bad location config: " \
					   "inherits element with no " \
					   "location attribute");
		}
		else if (!strcmp (node->name, "contains")) {
			contains_str = xmlGetProp (node, "backend");

			if (contains_str != NULL) {
				contains_str = g_strdup (contains_str);
				list_tail = g_list_append (list_tail, 
							   contains_str);
				if (list_head == NULL)
					list_head = list_tail;
				else
					list_tail = list_tail->next;
			} else {
				g_warning ("Bad location config: " \
					   "contains element with no " \
					   "backend attribute");
			}
		}
	}

	xmlFreeDoc (doc);

	location->p->contains_list = list_head;

	if (!is_default) {
		if (inherits_str != NULL) {
			location->p->inherits_location =
				archive_get_location (location->p->archive,
						      inherits_str);
		} else {
			if (list_head != NULL) {
				g_warning ("Top-level locations should not " \
					   "have contains lists");
				g_list_foreach (list_head, (GFunc) g_free,
						NULL);
				g_list_free (list_head);
			}

			backend_list_foreach
				(archive_get_backend_list
				 (location->p->archive),
				 (BackendCB) get_backends_cb,
				 location);
		}
	}

	return TRUE;
}

static void
save_metadata (Location *location) 
{
	gchar *metadata_filename;

	if (!location->p->is_new && !location->p->contains_list_dirty)
		return;

	location->p->is_new = FALSE;

	metadata_filename =
		g_concat_dir_and_file (location->p->fullpath,
				       "location.xml");

	write_metadata_file (location, metadata_filename);

	g_free (metadata_filename);

	location->p->contains_list_dirty = FALSE;
}

static void
write_metadata_file (Location *location, gchar *filename) 
{
	GList *node;
	xmlDocPtr doc;
	xmlNodePtr root_node, child_node;

	doc = xmlNewDoc ("1.0");
	root_node = xmlNewDocNode (doc, NULL, "location", NULL);

	if (location->p->inherits_location) {
		g_return_if_fail
			(location->p->inherits_location->p->locid != NULL);

		child_node = xmlNewChild (root_node, NULL, "inherits", NULL);
		xmlNewProp (child_node, "location",
			    location->p->inherits_location->p->locid);

		for (node = location->p->contains_list; node;
		     node = node->next) 
		{
			child_node = xmlNewChild (root_node, NULL, 
						  "contains", NULL);
			xmlNewProp (child_node, "backend", node->data);
		}
	}

	xmlDocSetRootElement (doc, root_node);

	/* FIXME: Report errors here */
	xmlSaveFile (filename, doc);
	xmlFreeDoc (doc);
}

/* Perform rollback for a given backend and id number. Return TRUE on
 * success and FALSE otherwise.
 *
 * FIXME: Better error reporting
 */

static gboolean
do_rollback (gchar *fullpath, gchar *backend_id, gint id) 
{
	int fd;

	fd = run_backend_proc (backend_id);
	if (fd == -1) return FALSE;
	return dump_xml_data (fullpath, id, fd);
}

static gboolean
dump_xml_data (gchar *fullpath, gint id, gint fd_output) 
{
	char *filename;
	FILE *input;
	char buffer[1024];
	size_t size;

	filename = g_strdup_printf ("%s/%08x.xml", fullpath, id);
	input = fopen (filename, "r");
	g_free (filename);

	if (!input)
		return FALSE;

	while (!feof (input)) {
		size = fread (buffer, sizeof (char), 1024, input);
		write (fd_output, buffer, size);
	}

	close (fd_output);

	return TRUE;
}

/* Run the given backend and return the file descriptor used to write
 * XML to it
 */

static gint
run_backend_proc (gchar *backend_id) 
{
	char *args[3];
	int fd[2];
	pid_t pid;

	if (pipe (fd) == -1)
		return -1;

	pid = fork ();

	if (pid == (pid_t) -1) {
		return -1;
	}
	else if (pid == 0) {
		int i;

		dup2 (fd[0], 0);
		for (i = 3; i < FOPEN_MAX; i++) close (i);

		args[0] = gnome_is_program_in_path (backend_id);
		args[1] = "--set";
		args[2] = NULL;

		if (!args[0]) {
			g_warning ("Backend not in path");
			exit (-1);
		}
			
		execv (args[0], args);
		exit (-1);

		return 0;
	} else {
		return fd[1];
	}
}
