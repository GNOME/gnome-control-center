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

#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <unistd.h>
#include <tree.h>
#include <parser.h>
#include <errno.h>
#include <signal.h>

#include "location.h"
#include "archive.h"
#include "util.h"

static GtkObjectClass *parent_class;

enum {
	ARG_0,
	ARG_LOCID,
	ARG_ARCHIVE,
	ARG_INHERITS
};

/* Note about backend containment in this location */

typedef struct _BackendNote BackendNote;

struct _BackendNote 
{
	gchar           *backend_id;
	ContainmentType  type;
};

struct _LocationPrivate 
{
	Archive         *archive;
	gchar           *locid;
	gchar           *fullpath;
	gchar           *label;

	Location        *inherits_location;
	GList           *contains_list;      /* List of BackendNotes */
	gboolean         is_new;
	gboolean         contains_list_dirty;

	ConfigLog       *config_log;
};

static void location_init            (Location *location);
static void location_class_init      (LocationClass *klass);

static void location_set_arg         (GtkObject *object,
				      GtkArg *arg,
				      guint arg_id);

static void location_get_arg         (GtkObject *object,
				      GtkArg *arg,
				      guint arg_id);

static void location_destroy         (GtkObject *object);
static void location_finalize        (GtkObject *object);

static gboolean location_do_rollback (Location *location,
				      gchar *backend_id, 
				      xmlDocPtr xml_doc);

static gint get_backends_cb          (BackendList *backend_list,
				      gchar *backend_id,
				      Location *location);

static gboolean do_create            (Location *location);
static gboolean do_load              (Location *location);
static gboolean load_metadata_file   (Location *location, 
				      char *filename, 
				      gboolean is_default);
static void save_metadata            (Location *location);
static void write_metadata_file      (Location *location, 
				      gchar *filename);

static xmlDocPtr load_xml_data       (gchar *fullpath, gint id);
static gint run_backend_proc         (gchar *backend_id,
				      gboolean do_get,
				      pid_t *pid);

static BackendNote *backend_note_new (gchar *backend_id, 
				      ContainmentType type);
static void backend_note_destroy     (BackendNote *note);
static const BackendNote *find_note  (Location *location, gchar *backend_id);

static GList *create_backends_list   (Location *location1,
				      Location *location2);
static GList *merge_backend_lists    (GList *backends1,
				      GList *backends2);

static void merge_xml_docs           (xmlDocPtr child_doc,
				      xmlDocPtr parent_doc);
static void subtract_xml_doc         (xmlDocPtr child_doc,
				      xmlDocPtr parent_doc,
				      gboolean strict);
static void merge_xml_nodes          (xmlNodePtr node1,
				      xmlNodePtr node2);
static xmlNodePtr subtract_xml_node  (xmlNodePtr node1,
				      xmlNodePtr node2,
				      gboolean strict);
static gboolean compare_xml_nodes    (xmlNodePtr node1, xmlNodePtr node2);

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

	klass->do_rollback = location_do_rollback;

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
			gtk_object_ref (GTK_VALUE_POINTER (*arg));
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
	save_metadata (LOCATION (object));

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
	GList *node;

	g_return_if_fail (object != NULL);
	g_return_if_fail (IS_LOCATION (object));

	location = LOCATION (object);

	for (node = location->p->contains_list; node; node = node->next)
		backend_note_destroy (node->data);
	g_list_free (location->p->contains_list);

	if (location->p->fullpath)
		g_free (location->p->fullpath);

	g_free (location->p);
	location->p = (LocationPrivate *) 0xdeadbeef;

	GTK_OBJECT_CLASS (parent_class)->finalize (object);
}

/* Perform rollback for a given backend and id number. Return TRUE on
 * success and FALSE otherwise.
 *
 * FIXME: Better error reporting
 */

static gboolean
location_do_rollback (Location *location, gchar *backend_id, xmlDocPtr doc) 
{
	int fd, status;
	FILE *output;
	pid_t pid;

	g_return_val_if_fail (location != NULL, FALSE);
	g_return_val_if_fail (IS_LOCATION (location), FALSE);
	g_return_val_if_fail (backend_id != NULL, FALSE);
	g_return_val_if_fail (doc != NULL, FALSE);

	/* FIXME: Some mechanism for retrieving the factory defaults settings
	 * would be useful here
	 */
	if (doc == NULL) return FALSE;

	fd = run_backend_proc (backend_id, FALSE, &pid);
	if (fd == -1) return FALSE;

	output = fdopen (fd, "w");
	xmlDocDump (output, doc);

	DEBUG_MSG ("Done dumping data; flushing and closing output stream");

	if (fflush (output) == EOF) {
		g_critical ("%s: Could not dump buffer: %s",
			    __FUNCTION__, g_strerror (errno));
	}

	if (fclose (output) == EOF) {
		g_critical ("%s: Could not close output stream: %s",
			    __FUNCTION__, g_strerror (errno));
		return FALSE;
	}

	waitpid (pid, &status, 0);

	return TRUE;
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
					 "/location.xml", NULL);
	unlink (metadata_filename);
	g_free (metadata_filename);

	config_log_iterate (location->p->config_log,
			    (ConfigLogIteratorCB) data_delete_cb, location);
	config_log_delete (location->p->config_log);

	if (rmdir (location->p->fullpath) == -1)
		g_warning ("%s: Could not remove directory: %s\n",
			   __FUNCTION__, g_strerror (errno));

	gtk_object_destroy (GTK_OBJECT (location));
}

/**
 * location_store:
 * @location: 
 * @backend_id: 
 * @input: 
 * @store_type: STORE_FULL means blindly store the data, without
 * modification. STORE_COMPARE_PARENT means subtract the settings the parent
 * has that are different and store the result. STORE_MASK_PREVIOUS means
 * store only those settings that are reflected in the previous logged data;
 * if there do not exist such data, act as in
 * STORE_COMPARE_PARENT. STORE_DEFAULT means these are default data.
 * 
 * Store configuration data from the given stream in the location under the
 * given backend id
 *
 * Return value: 0 on success, -1 if it cannot parse the XML, and -2 if no
 * data were supplied
 **/

gint
location_store (Location *location, gchar *backend_id, FILE *input,
		StoreType store_type) 
{
	xmlDocPtr doc;
	char buffer[2048];
	int t = 0;
	GString *doc_str;

	g_return_val_if_fail (location != NULL, -2);
	g_return_val_if_fail (IS_LOCATION (location), -2);

	fflush (input);

	fcntl (fileno (input), F_SETFL, 0);

	doc_str = g_string_new ("");

	while ((t = read (fileno (input), buffer, sizeof (buffer) - 1)) != 0) {
		buffer[t] = '\0';
		g_string_append (doc_str, buffer);
	}

	if (doc_str->len > 0) {
		doc = xmlParseDoc (doc_str->str);

		if (doc == NULL) {
			g_string_free (doc_str, TRUE);
			return -1;
		}

		location_store_xml (location, backend_id, doc, store_type);
		xmlFreeDoc (doc);
	} else {
		return -2;
	}

	g_string_free (doc_str, TRUE);
	return 0;
}

/**
 * location_store_xml:
 * @location: 
 * @backend_id: 
 * @input: 
 * @store_type: STORE_FULL means blindly store the data, without
 * modification. STORE_COMPARE_PARENT means subtract the settings the parent
 * has that are different and store the result. STORE_MASK_PREVIOUS means
 * store only those settings that are reflected in the previous logged data;
 * if there do not exist such data, act as in STORE_COMPARE_PARENT
 * 
 * Store configuration data from the given XML document object in the location
 * under the given backend id
 **/

void 
location_store_xml (Location *location, gchar *backend_id, xmlDocPtr xml_doc,
		    StoreType store_type) 
{
	gint id, prev_id = 0;
	xmlDocPtr parent_doc, prev_doc = NULL;
	char *filename;
	ContainmentType contain_type;

	g_return_if_fail (location != NULL);
	g_return_if_fail (IS_LOCATION (location));
	g_return_if_fail (location->p->config_log != NULL);
	g_return_if_fail (IS_CONFIG_LOG (location->p->config_log));
	g_return_if_fail (xml_doc != NULL);

	contain_type = location_contains (location, backend_id);

	if (contain_type == CONTAIN_NONE) {
		if (!location->p->inherits_location)
			fprintf (stderr, "Could not find a location in the " \
				 "tree ancestry that stores this " \
				 "backend: %s.\n", backend_id);
		else
			location_store_xml (location->p->inherits_location,
					    backend_id, xml_doc, store_type);

		return;
	}

	if (contain_type == CONTAIN_PARTIAL && store_type != STORE_FULL &&
	    location->p->inherits_location != NULL)
	{
		g_assert (store_type == STORE_MASK_PREVIOUS ||
			  store_type == STORE_COMPARE_PARENT);

		parent_doc = location_load_rollback_data
			(location->p->inherits_location, NULL, 0,
			 backend_id, TRUE);

		if (store_type == STORE_MASK_PREVIOUS) {
			prev_id = config_log_get_rollback_id_by_steps
				(location->p->config_log, 0, backend_id);

			if (prev_id != -1)
				prev_doc = load_xml_data
					(location->p->fullpath, prev_id);
		}

		if (prev_id == -1 || store_type == STORE_COMPARE_PARENT) {
			subtract_xml_doc (xml_doc, parent_doc, FALSE);
		} else {
			subtract_xml_doc (parent_doc, prev_doc, FALSE);
			subtract_xml_doc (xml_doc, parent_doc, TRUE);
		}

		xmlFreeDoc (parent_doc);

		if (prev_doc != NULL)
			xmlFreeDoc (prev_doc);
	}

	id = config_log_write_entry (location->p->config_log, backend_id,
				     store_type == STORE_DEFAULT);

	filename = g_strdup_printf ("%s/%08x.xml",
				    location->p->fullpath, id);
	xmlSaveFile (filename, xml_doc);
	g_free (filename);
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
	xmlDocPtr doc;

	g_return_if_fail (location != NULL);
	g_return_if_fail (IS_LOCATION (location));

	doc = location_load_rollback_data (location, date, 0,
					   backend_id, parent_chain);
	LOCATION_CLASS (GTK_OBJECT (location)->klass)->do_rollback
		(location, backend_id, doc);
	xmlFreeDoc (doc);
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
 *
 * FIXME: To enforce some kind of ordering on backend application, just create
 * a comparison function between backend ids that calls a BackendList method
 * to see which item should go first, and then call g_list_sort with that on
 * the backend list
 **/

void 
location_rollback_backends_to (Location *location, struct tm *date,
			       GList *backends, gboolean parent_chain) 
{
	GList *node;

	g_return_if_fail (location != NULL);
	g_return_if_fail (IS_LOCATION (location));
	g_return_if_fail (location->p->config_log != NULL);
	g_return_if_fail (IS_CONFIG_LOG (location->p->config_log));

	for (node = backends; node; node = node->next)
		location_rollback_backend_to (location, date, node->data,
					      parent_chain);
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
	GList *node;
	BackendNote *note;

	g_return_if_fail (location != NULL);
	g_return_if_fail (IS_LOCATION (location));
	g_return_if_fail (location->p->config_log != NULL);
	g_return_if_fail (IS_CONFIG_LOG (location->p->config_log));

	for (node = location->p->contains_list; node; node = node->next) {
		note = node->data;
		location_rollback_backend_to (location, date, note->backend_id,
					      parent_chain);
	}
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
	xmlDocPtr doc;

	g_return_if_fail (location != NULL);
	g_return_if_fail (IS_LOCATION (location));

	doc = location_load_rollback_data (location, NULL, steps,
					   backend_id, parent_chain);
	LOCATION_CLASS (GTK_OBJECT (location)->klass)->do_rollback
		(location, backend_id, doc);
	xmlFreeDoc (doc);
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
	struct tm *date;
	xmlDocPtr xml_doc, parent_doc;

	backend_id = config_log_get_backend_id_for_id
		(location->p->config_log, id);

	if (backend_id == NULL) return;

	xml_doc = load_xml_data (location->p->fullpath, id);

	if (location_contains (location, backend_id) == CONTAIN_PARTIAL) {
		date = config_log_get_date_for_id
			(location->p->config_log, id);
		parent_doc = location_load_rollback_data
			(location->p->inherits_location, date, 0,
			 backend_id, TRUE);
		merge_xml_docs (xml_doc, parent_doc);
		xmlFreeDoc (parent_doc);
	}

	if (backend_id)
		LOCATION_CLASS (GTK_OBJECT (location)->klass)->do_rollback
			(location, backend_id, xml_doc);
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
	xmlDocPtr doc;

	g_return_if_fail (location != NULL);
	g_return_if_fail (IS_LOCATION (location));
	g_return_if_fail (location->p->config_log != NULL);
	g_return_if_fail (IS_CONFIG_LOG (location->p->config_log));

	doc = location_load_rollback_data (location, date, steps, backend_id,
					   parent_chain);

	if (doc != NULL) {
		xmlDocDump (output, doc);
		xmlFreeDoc (doc);
	}
}

/**
 * location_load_rollback_data
 * @location:
 * @date:
 * @steps:
 * @backend_id:
 * @parent_chain:
 *
 * Loads the XML data for rolling back as specified and returns the document
 * object
 **/

xmlDocPtr
location_load_rollback_data (Location *location, struct tm *date,
			     guint steps, gchar *backend_id,
			     gboolean parent_chain)
{
	gint id;
	xmlDocPtr doc = NULL, parent_doc = NULL;
	ContainmentType type;

	g_return_val_if_fail (location != NULL, NULL);
	g_return_val_if_fail (IS_LOCATION (location), NULL);
	g_return_val_if_fail (location->p->config_log != NULL, NULL);
	g_return_val_if_fail (IS_CONFIG_LOG (location->p->config_log), NULL);

	if (steps > 0)
		id = config_log_get_rollback_id_by_steps
			(location->p->config_log, steps, backend_id);
	else
		id = config_log_get_rollback_id_for_date
			(location->p->config_log, date, backend_id);

	if (id != -1)
		doc = load_xml_data (location->p->fullpath, id);

	type = location_contains (location, backend_id);

	if ((id == -1 || type == CONTAIN_PARTIAL) &&
	    parent_chain && location->p->inherits_location != NULL)
		parent_doc = location_load_rollback_data
			(location->p->inherits_location,
			 date, steps, backend_id, TRUE);

	if (doc != NULL && parent_doc != NULL)
		merge_xml_docs (doc, parent_doc);
	else if (parent_doc != NULL)
		doc = parent_doc;

	return doc;
}

/**
 * location_contains:
 * @location: 
 * @backend_id: 
 * 
 * Determine if a location specifies configuration for the given backend
 * 
 * Return value: Containment type, as defined in the enum
 **/

ContainmentType
location_contains (Location *location, gchar *backend_id) 
{
	BackendList *list;

	g_return_val_if_fail (location != NULL, FALSE);
	g_return_val_if_fail (IS_LOCATION (location), FALSE);

	if (location->p->inherits_location == NULL) {
		list = archive_get_backend_list (location->p->archive);

		if (backend_list_contains (list, backend_id))
			return CONTAIN_FULL;
		else
			return CONTAIN_NONE;
	} else {
		const BackendNote *note = find_note (location, backend_id);

		if (note != NULL)
			return note->type;
		else
			return CONTAIN_NONE;
	}
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
 * not registered with the master list, -3 if the type specified was "NONE"
 **/

gint
location_add_backend (Location *location, gchar *backend_id,
		      ContainmentType type) 
{
	g_return_val_if_fail (location != NULL, -1);
	g_return_val_if_fail (IS_LOCATION (location), -1);
	g_return_val_if_fail (backend_id != NULL, -2);

	if (location->p->inherits_location == NULL) return -1;

	if (type == CONTAIN_NONE) return -3;

	if (!backend_list_contains 
	    (archive_get_backend_list (location->p->archive), backend_id))
		return -2;

	location->p->contains_list =
		g_list_append (location->p->contains_list,
			       backend_note_new (backend_id, type));
	location->p->contains_list_dirty = TRUE;

	save_metadata (location);

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
	BackendNote *note;

	g_return_if_fail (location != NULL);
	g_return_if_fail (IS_LOCATION (location));
	g_return_if_fail (backend_id != NULL);

	if (location->p->inherits_location == NULL) return;

	for (node = location->p->contains_list; node; node = node->next) {
		note = node->data;

		if (!strcmp (note->backend_id, backend_id)) {
			backend_note_destroy (note);
			location->p->contains_list =
				g_list_remove_link (location->p->contains_list,
						    node);
			location->p->contains_list_dirty = TRUE;
			return;
		}
	}

	save_metadata (location);
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
	BackendNote *note;

	g_return_if_fail (location != NULL);
	g_return_if_fail (IS_LOCATION (location));
	g_return_if_fail (callback != NULL);

	for (node = location->p->contains_list; node; node = node->next) {
		note = node->data;
		if (callback (location, note->backend_id, data)) break;
	}
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

/**
 * location_store_full_snapshot:
 * @location:
 *
 * Gets XML snapshot data from all the backends contained in this location and
 * archives those data
 *
 * Return value: 0 on success and -1 if any of the backends failed
 **/

gint
location_store_full_snapshot (Location *location)
{
	int fd;
	gint ret;
	FILE *pipe;
	GList *c;
	BackendNote *note;

	g_return_val_if_fail (location != NULL, -1);
	g_return_val_if_fail (IS_LOCATION (location), -1);

	for (c = location->p->contains_list; c; c = c->next) {
		note = c->data;
		DEBUG_MSG ("Storing %s", note->backend_id);

		fd = run_backend_proc (note->backend_id, TRUE, NULL);
		pipe = fdopen (fd, "r");
		ret = location_store (location, note->backend_id,
				      pipe, STORE_DEFAULT);
		fclose (pipe);

		if (ret < 0)
			return -1;
	}

	return 0;
}

/**
 * location_get_changed_backends:
 * @location:
 * @location1:
 *
 * Get a list of backends that change from location to location1
 **/

GList *
location_get_changed_backends (Location *location, Location *location1) 
{
	GList *backends1, *backends2;

	g_return_val_if_fail (location != NULL, NULL);
	g_return_val_if_fail (IS_LOCATION (location), NULL);
	g_return_val_if_fail (location1 != NULL, NULL);
	g_return_val_if_fail (IS_LOCATION (location1), NULL);

	backends1 = create_backends_list (location, location1);
	backends2 = create_backends_list (location1, location);

	return merge_backend_lists (backends1, backends2);
}

/**
 * location_does_backend_change:
 * @location:
 * @location1:
 * @backend_id:
 *
 * Return TRUE if a backend changes when changing from location to location1;
 * FALSE otherwise
 **/

gboolean
location_does_backend_change (Location *location, Location *location1,
			      gchar *backend_id) 
{
	GList *backends;
	gboolean ret;

	g_return_val_if_fail (location != NULL, FALSE);
	g_return_val_if_fail (IS_LOCATION (location), FALSE);
	g_return_val_if_fail (location1 != NULL, FALSE);
	g_return_val_if_fail (IS_LOCATION (location1), FALSE);	
	g_return_val_if_fail (backend_id != NULL, FALSE);

	backends = location_get_changed_backends (location, location1);
	ret = !(g_list_find_custom (backends, backend_id,
				    (GCompareFunc) strcmp) == NULL);
	g_list_free (backends);

	return ret;
}

/* location_get_config_log:
 * @location:
 *
 * Returns the config log object for this location
 */

ConfigLog *
location_get_config_log (Location *location) 
{
	g_return_val_if_fail (location != NULL, FALSE);
	g_return_val_if_fail (IS_LOCATION (location), FALSE);

	return location->p->config_log;
}

static gint
get_backends_cb (BackendList *backend_list, gchar *backend_id,
		 Location *location) 
{
	location->p->contains_list =
		g_list_prepend (location->p->contains_list,
				backend_note_new (backend_id, FALSE));
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
	char *inherits_str = NULL, *contains_str, *type_str;
	GList *list_head = NULL, *list_tail = NULL;
	BackendNote *note;
	ContainmentType type;

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
			type_str = xmlGetProp (node, "type");

			if (contains_str != NULL) {
				if (!strcmp (type_str, "full"))
					type = CONTAIN_FULL;
				else if (!strcmp (type_str, "partial"))
					type = CONTAIN_PARTIAL;
				else {
					type = CONTAIN_NONE;
					g_warning ("Bad type attribute");
				}

				note = backend_note_new (contains_str, type);
				list_tail = g_list_append (list_tail, note);
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
	BackendNote *note;

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
			note = node->data;
			child_node = xmlNewChild (root_node, NULL, 
						  "contains", NULL);
			xmlNewProp (child_node, "backend", note->backend_id);

			if (note->type == CONTAIN_PARTIAL)
				xmlNewProp (child_node, "type", "partial");
			else if (note->type == CONTAIN_FULL)
				xmlNewProp (child_node, "type", "full");
		}
	}

	xmlDocSetRootElement (doc, root_node);

	/* FIXME: Report errors here */
	xmlSaveFile (filename, doc);
	xmlFreeDoc (doc);
}

static xmlDocPtr
load_xml_data (gchar *fullpath, gint id) 
{
	char *filename;
	xmlDocPtr xml_doc;

	filename = g_strdup_printf ("%s/%08x.xml", fullpath, id);
	xml_doc = xmlParseFile (filename);
	g_free (filename);

	return xml_doc;
}

/* Run the given backend and return the file descriptor used to read or write
 * XML to it
 */

static gint
run_backend_proc (gchar *backend_id, gboolean do_get, pid_t *pid_r) 
{
	char *args[3];
	int fd[2];
	pid_t pid;
	int p_fd, c_fd;

	if (pipe (fd) == -1)
		return -1;

	pid = fork ();

	p_fd = do_get ? 0 : 1;
	c_fd = do_get ? 1 : 0;

	if (pid == (pid_t) -1) {
		return -1;
	}
	else if (pid == 0) {
		int i;

		dup2 (fd[c_fd], c_fd);
		close (fd[p_fd]);
		for (i = 3; i < FOPEN_MAX; i++) close (i);

		if (strstr (backend_id, "-conf"))
			args[0] = g_concat_dir_and_file (XST_BACKEND_LOCATION,
							 backend_id);
		else
			args[0] = gnome_is_program_in_path (backend_id);

		args[1] = do_get ? "--get" : "--set";
		args[2] = NULL;

		if (args[0] == NULL) {
			g_warning ("Backend not in path: %s", backend_id);
			close (c_fd);
			exit (-1);
		}

		execv (args[0], args);

		g_warning ("Could not launch backend %s: %s",
			   args[0], g_strerror (errno));
		exit (-1);
		return 0;
	} else {
		if (pid_r != NULL)
			*pid_r = pid;

 		close (fd[c_fd]);
		return fd[p_fd];
	}
}

static BackendNote *
backend_note_new (gchar *backend_id, ContainmentType type)
{
	BackendNote *note;

	note = g_new0 (BackendNote, 1);
	note->backend_id = g_strdup (backend_id);
	note->type = type;

	return note;
}

static void
backend_note_destroy (BackendNote *note)
{
	g_free (note->backend_id);
	g_free (note);
}

static const BackendNote *
find_note (Location *location, gchar *backend_id)
{
	GList *node;

	for (node = location->p->contains_list; node; node = node->next)
		if (!strcmp (((BackendNote *)node->data)->backend_id,
			     backend_id))
			return node->data;

	return NULL;
}

static void
merge_xml_docs (xmlDocPtr child_doc, xmlDocPtr parent_doc)
{
	merge_xml_nodes (xmlDocGetRootElement (child_doc),
			 xmlDocGetRootElement (parent_doc));
}

static void
subtract_xml_doc (xmlDocPtr child_doc, xmlDocPtr parent_doc, gboolean strict) 
{
	subtract_xml_node (xmlDocGetRootElement (child_doc),
			   xmlDocGetRootElement (parent_doc), strict);
}

/* Merge contents of node1 and node2, where node1 overrides node2 as
 * appropriate
 *
 * Notes: Two XML nodes are considered to be "the same" iff their names and
 * all names and values of their attributes are the same. If that is not the
 * case, they are considered "different" and will both be present in the
 * merged node. If nodes are "the same", then this algorithm is called
 * recursively. If nodes are CDATA, then node1 overrides node2 and the
 * resulting node is just node1. The node merging is order-independent; child
 * node from one tree are compared with all child nodes of the other tree
 * regardless of the order they appear in. Hence one may have documents with
 * different node orderings and the algorithm should still run correctly. It
 * will not, however, run correctly in cases when the agent using this
 * facility depends on the nodes being in a particular order.
 *
 * This XML node merging/comparison facility requires that the following
 * standard be set for DTDs:
 *
 * Attributes' sole purpose is to identify a node. For example, a network
 * configuration DTD might have an element like <interface name="eth0"> with a
 * bunch of child nodes to configure that interface. The attribute "name" does
 * not specify the configuration for the interface. It differentiates the node
 * from the configuration for, say, interface eth1. Conversely, a node must be
 * completely identified by its attributes. One cannot include identification
 * information in the node's children, since otherwise the merging and
 * subtraction algorithms will not know what to look for.
 *
 * As a corollary to the above, all configuration information must ultimately
 * be in text nodes. For example, a text string might be stored as
 * <configuration-item>my-value</configuration-item> but never as
 * <configuration-item value="my-value"/>. As an example, if the latter is
 * used, a child location might override a parent's setting for
 * configuration-item. This algorithm will interpret those as different nodes
 * and include them both in the merged result, since it will not have any way
 * of knowing that they are really the same node with different
 * configuration.
 */

static void
merge_xml_nodes (xmlNodePtr node1, xmlNodePtr node2) 
{
	xmlNodePtr child, tmp, iref;
	GList *node1_children = NULL, *i;
	gboolean found;

	if (node1->type == XML_TEXT_NODE)
		return;

	for (child = node1->childs; child != NULL; child = child->next)
		node1_children = g_list_prepend (node1_children, child);

	node1_children = g_list_reverse (node1_children);

	child = node2->childs;

	while (child != NULL) {
		tmp = child->next;

		i = node1_children; found = FALSE;

		while (i != NULL) {
			iref = (xmlNodePtr) i->data;

			if (compare_xml_nodes (iref, child)) {
				merge_xml_nodes (iref, child);
				if (i == node1_children)
					node1_children = node1_children->next;
				g_list_remove_link (node1_children, i);
				g_list_free_1 (i);
				found = TRUE;
				break;
			} else {
				i = i->next;
			}
		}

		if (found == FALSE) {
			xmlUnlinkNode (child);
			xmlAddChild (node1, child);
		}

		child = tmp;
	}

	g_list_free (node1_children);
}

/* Create a list of backends that differ between location1 and the common
 * parent of location1 and location2 */

static GList *
create_backends_list (Location *location1, Location *location2) 
{
	Location *loc;
	GList *location_path, *tail = NULL, *c, *tmp;

	location_path = location_find_path_from_common_parent
		(location1, location2);

	/* Skip the first entry -- it is the common parent */
	tmp = location_path;
	location_path = location_path->next;
	g_list_free_1 (tmp);

	while (location_path != NULL) {
		if (location_path->data != NULL) {
			loc = LOCATION (location_path->data);
			for (c = loc->p->contains_list; c; c = c->next)
				tail = g_list_prepend
					(tail, ((BackendNote *)c->data)->backend_id);
		}

		tmp = location_path;
		location_path = location_path->next;
		g_list_free_1 (tmp);
	}

	return g_list_reverse (tail);
}

/* Merge two backend lists, eliminating duplicates */

static GList *
merge_backend_lists (GList *backends1, GList *backends2) 
{
	GList *head = NULL, *tail = NULL, *tmp;
	int res;

	backends1 = g_list_sort (backends1, (GCompareFunc) strcmp);
	backends2 = g_list_sort (backends2, (GCompareFunc) strcmp);

	while (backends1 && backends2) {
		res = strcmp (backends1->data, backends2->data);

		if (res < 0) {
			if (tail != NULL) tail->next = backends1;
			else head = backends1;
			tail = backends1;
			backends1 = backends1->next;
		}
		else if (res > 0) {
			if (tail != NULL) tail->next = backends2;
			else head = backends2;
			tail = backends2;
			backends2 = backends2->next;
		} else {
			if (tail != NULL) tail->next = backends1;
			else head = backends1;
			tail = backends1;
			backends1 = backends1->next;
			tmp = backends2;
			backends2 = backends2->next;
			g_list_free_1 (tmp);
		}
	}

	if (backends1 != NULL) {
		if (tail != NULL) tail->next = backends1;
		else head = backends1;
	}
	else {
		if (tail != NULL) tail->next = backends2;
		else head = backends2;
	}

	return head;
}

/* Modifies node1 so that it only contains the parts different from node2;
 * returns the modified node or NULL if the node should be destroyed
 *
 * strict determines whether the settings themselves are compared; it should
 * be set to FALSE when the trees are being compared for the purpose of
 * seeing what settings should be included in a tree and TRUE when one wants
 * to restrict the settings included in a tree to those that have already been
 * specified
 */

static xmlNodePtr
subtract_xml_node (xmlNodePtr node1, xmlNodePtr node2, gboolean strict) 
{
	xmlNodePtr child, tmp, iref;
	GList *node2_children = NULL, *i;
	gboolean found, same, all_same = TRUE;

	if (node1->type == XML_TEXT_NODE) {
		if (node2->type == XML_TEXT_NODE &&
		    (strict || !strcmp (xmlNodeGetContent (node1),
					xmlNodeGetContent (node2))))
			return NULL;
		else
			return node1;
	}

	if (node1->childs == NULL && node2->childs == NULL)
		return NULL;

	for (child = node2->childs; child != NULL; child = child->next)
		node2_children = g_list_prepend (node2_children, child);

	node2_children = g_list_reverse (node2_children);

	child = node1->childs;

	while (child != NULL) {
		tmp = child->next;
		i = node2_children; found = FALSE; all_same = TRUE;

		while (i != NULL) {
			iref = (xmlNodePtr) i->data;

			if (compare_xml_nodes (child, iref)) {
				same = (subtract_xml_node
					(child, iref, strict) == NULL);
				all_same = all_same && same;

				if (same) {
					xmlUnlinkNode (child);
					xmlFreeNode (child);
				}

				if (i == node2_children)
					node2_children = node2_children->next;
				g_list_remove_link (node2_children, i);
				g_list_free_1 (i);
				found = TRUE;
				break;
			} else {
				i = i->next;
			}
		}

		if (!found)
			all_same = FALSE;

		child = tmp;
	}

	g_list_free (node2_children);

	if (all_same)
		return NULL;
	else
		return node1;
}

/* Return TRUE iff node1 and node2 are "the same" in the sense defined above */

static gboolean
compare_xml_nodes (xmlNodePtr node1, xmlNodePtr node2) 
{
	xmlAttrPtr attr;
	gint count = 0;

	if (strcmp (node1->name, node2->name))
		return FALSE;

	/* FIXME: This is worst case O(n^2), which can add up. Could we
	 * optimize for the case where people have attributes in the same
	 * order, or does not not matter? It probably does not matter, though,
	 * since people don't generally have more than one or two attributes
	 * in a tag anyway.
	 */

	for (attr = node1->properties; attr != NULL; attr = attr->next) {
		g_assert (xmlNodeIsText (attr->val));

		if (strcmp (xmlNodeGetContent (attr->val),
			    xmlGetProp (node2, attr->name)))
			return FALSE;

		count++;
	}

	/* FIXME: Is checking if the two nodes have the same number of
	 * attributes the correct policy here? Should we instead merge the
	 * attribute(s) that node1 is missing?
	 */

	for (attr = node2->properties; attr != NULL; attr = attr->next)
		count--;

	if (count == 0)
		return TRUE;
	else
		return FALSE;
}
