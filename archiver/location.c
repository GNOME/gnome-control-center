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
#include "archiver-client.h"

static BonoboXObjectClass *parent_class;

enum {
	ARG_0,
	ARG_LOCID,
	ARG_LABEL,
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
	Archive           *archive;
	gchar             *locid;
	gchar             *fullpath;
	gchar             *label;

	Location          *parent;
	GList             *contains_list;      /* List of BackendNotes */
	gboolean           is_new;
	gboolean           contains_list_dirty;
	gboolean           deleted;

	ConfigLog         *config_log;

	BonoboEventSource *es;
};

#define LOCATION_FROM_SERVANT(servant) (LOCATION (bonobo_object_from_servant (servant)))

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

static gint run_backend_proc         (gchar *backend_id,
				      gboolean do_get,
				      pid_t *pid);

static BackendNote *backend_note_new (const gchar *backend_id, 
				      ContainmentType type);
static void backend_note_destroy     (BackendNote *note);
static const BackendNote *find_note  (Location *location,
				      const gchar *backend_id);

static GList *create_backends_list   (Location *location1,
				      Location *location2);
static GList *merge_backend_lists    (GList *backends1,
				      GList *backends2);

/* CORBA interface methods */

static CORBA_char *
impl_ConfigArchiver_Location_getStorageFilename (PortableServer_Servant  servant,
						 const CORBA_char       *backendId,
						 CORBA_boolean           isDefaultData,
						 CORBA_Environment      *ev) 
{
	gchar *filename;
	CORBA_char *ret;

	filename = location_get_storage_filename (LOCATION_FROM_SERVANT (servant), backendId, isDefaultData);
	ret = CORBA_string_dup (filename);
	g_free (filename);

	return ret;
}

static CORBA_char *
impl_ConfigArchiver_Location_getRollbackFilename (PortableServer_Servant  servant,
						  ConfigArchiver_Time     timep,
						  CORBA_long              steps,
						  const CORBA_char       *backendId,
						  CORBA_boolean           parentChain,
						  CORBA_Environment      *ev) 
{
	gchar *filename;
	CORBA_char *ret;
	struct tm timeb, *timeb_p = &timeb;

	if (timep != 0)
		gmtime_r ((time_t *) &timep, timeb_p);
	else
		timeb_p = NULL;

	filename = location_get_rollback_filename (LOCATION_FROM_SERVANT (servant), timeb_p, steps, backendId, parentChain);

	if (filename != NULL) {
		ret = CORBA_string_dup (filename);
		g_free (filename);
	} else {
		ret = NULL;
		bonobo_exception_set (ev, ex_ConfigArchiver_Location_RollbackDataNotFound);
	}

	return ret;
}

static void
impl_ConfigArchiver_Location_storageComplete (PortableServer_Servant  servant,
					       const CORBA_char       *filename,
					       CORBA_Environment      *ev) 
{
	location_storage_complete (LOCATION_FROM_SERVANT (servant), filename);
}

static void
impl_ConfigArchiver_Location_rollbackBackends (PortableServer_Servant          servant,
					       ConfigArchiver_Time             timep,
					       CORBA_long                      steps,
					       const ConfigArchiver_StringSeq *backends,
					       CORBA_boolean                   parentChain,
					       CORBA_Environment              *ev) 
{
	GList *node = NULL;
	unsigned int i;
	struct tm timeb, *timeb_p = &timeb;

	for (i = 0; i < backends->_length; i++)
		node = g_list_prepend (node, backends->_buffer[i]);

	if (timep != 0)
		gmtime_r ((time_t *) &timep, &timeb);
	else
		timeb_p = NULL;

	location_rollback_backends_to (LOCATION_FROM_SERVANT (servant), timeb_p, steps, node, parentChain);

	g_list_free (node);
}

static ConfigArchiver_Time
impl_ConfigArchiver_Location_getModificationTime (PortableServer_Servant  servant,
						  const CORBA_char       *backendId,
						  CORBA_Environment      *ev) 
{
	const struct tm *time_s;
	struct tm *tmp_date;
	ConfigArchiver_Time ret;

	time_s = location_get_modification_time (LOCATION_FROM_SERVANT (servant), backendId);

	if (time_s != NULL) {
		tmp_date = dup_date (time_s);
		ret = mktime (tmp_date);
		g_free (tmp_date);
	} else {
		ret = 0;
	}

	return ret;
}

static ConfigArchiver_ContainmentType
impl_ConfigArchiver_Location_contains (PortableServer_Servant  servant,
				       const CORBA_char       *backendId,
				       CORBA_Environment      *ev) 
{
	return location_contains (LOCATION_FROM_SERVANT (servant), backendId);
}

static CORBA_long
impl_ConfigArchiver_Location_addBackend (PortableServer_Servant          servant,
					 const CORBA_char               *backendId,
					 ConfigArchiver_ContainmentType  containmentType,
					 CORBA_Environment              *ev) 
{
	return location_add_backend (LOCATION_FROM_SERVANT (servant), backendId, containmentType);
}

static void
impl_ConfigArchiver_Location_removeBackend (PortableServer_Servant  servant,
					    const CORBA_char       *backendId,
					    CORBA_Environment      *ev)
{
	location_remove_backend (LOCATION_FROM_SERVANT (servant), backendId);
}

static CORBA_boolean
impl_ConfigArchiver_Location_doesBackendChange (PortableServer_Servant   servant,
						ConfigArchiver_Location  location,  
						const CORBA_char        *backendId,
						CORBA_Environment       *ev) 
{
	Location *loc1, *loc2;

	loc1 = LOCATION_FROM_SERVANT (servant);
	loc2 = LOCATION_FROM_SERVANT (location->servant);

	return location_does_backend_change (loc1, loc2, backendId);
}

static void
impl_ConfigArchiver_Location_garbageCollect (PortableServer_Servant  servant,
					     CORBA_Environment      *ev) 
{
	location_garbage_collect (LOCATION_FROM_SERVANT (servant));
}

static void
impl_ConfigArchiver_Location_delete (PortableServer_Servant  servant,
				     CORBA_Environment      *ev) 
{
	location_delete (LOCATION_FROM_SERVANT (servant));
}

static ConfigArchiver_Location
impl_ConfigArchiver_Location__get_parent (PortableServer_Servant  servant,
					  CORBA_Environment      *ev) 
{
	Location *location = LOCATION_FROM_SERVANT (servant);

	if (location->p->parent != NULL)
		return bonobo_object_dup_ref (BONOBO_OBJREF (location->p->parent), ev);
	else
		return CORBA_OBJECT_NIL;
}

static CORBA_char *
impl_ConfigArchiver_Location__get_path (PortableServer_Servant  servant,
					CORBA_Environment      *ev) 
{
	return CORBA_string_dup (LOCATION_FROM_SERVANT (servant)->p->fullpath);
}

static ConfigArchiver_StringSeq *
impl_ConfigArchiver_Location__get_backendList (PortableServer_Servant  servant,
					       CORBA_Environment      *ev) 
{
	ConfigArchiver_StringSeq *ret;
	Location *location;
	GList *node;
	guint i = 0;

	location = LOCATION_FROM_SERVANT (servant);

	ret = ConfigArchiver_StringSeq__alloc ();
	ret->_length = g_list_length (location->p->contains_list);
	ret->_buffer = CORBA_sequence_CORBA_string_allocbuf (ret->_length);

	for (node = location->p->contains_list; node != NULL; node = node->next)
		ret->_buffer[i++] = CORBA_string_dup (((BackendNote *) node->data)->backend_id);

	return ret;
}

static CORBA_char *
impl_ConfigArchiver_Location__get_label (PortableServer_Servant  servant,
					 CORBA_Environment      *ev) 
{
	return CORBA_string_dup (LOCATION_FROM_SERVANT (servant)->p->label);
}

static CORBA_char *
impl_ConfigArchiver_Location__get_id (PortableServer_Servant  servant,
				      CORBA_Environment      *ev) 
{
	return CORBA_string_dup (LOCATION_FROM_SERVANT (servant)->p->locid);
}

static void
impl_ConfigArchiver_Location__set_label (PortableServer_Servant  servant,
					 const CORBA_char       *label,
					 CORBA_Environment      *ev) 
{
	gtk_object_set (GTK_OBJECT (LOCATION_FROM_SERVANT (servant)), "label", label, NULL);
}

static void
impl_ConfigArchiver_Location__set_id (PortableServer_Servant  servant,
				      const CORBA_char       *id,
				      CORBA_Environment      *ev) 
{
	gtk_object_set (GTK_OBJECT (LOCATION_FROM_SERVANT (servant)), "locid", id, NULL);
}

BONOBO_X_TYPE_FUNC_FULL (Location, ConfigArchiver_Location, BONOBO_X_OBJECT_TYPE, location);

static void
location_init (Location *location) 
{
	location->p                      = g_new0 (LocationPrivate, 1);
	location->p->archive             = NULL;
	location->p->locid               = NULL;
	location->p->is_new              = FALSE;
	location->p->contains_list_dirty = FALSE;

	location->p->es                  = bonobo_event_source_new ();

	bonobo_object_add_interface (BONOBO_OBJECT (location), BONOBO_OBJECT (location->p->es));
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

	gtk_object_add_arg_type ("Location::label",
				 GTK_TYPE_POINTER,
				 GTK_ARG_READWRITE,
				 ARG_LABEL);

	gtk_object_add_arg_type ("Location::inherits",
				 GTK_TYPE_POINTER,
				 GTK_ARG_READWRITE,
				 ARG_INHERITS);

	klass->do_rollback = location_do_rollback;

	klass->epv.getStorageFilename  = impl_ConfigArchiver_Location_getStorageFilename;
	klass->epv.getRollbackFilename = impl_ConfigArchiver_Location_getRollbackFilename;
	klass->epv.storageComplete     = impl_ConfigArchiver_Location_storageComplete;
	klass->epv.rollbackBackends    = impl_ConfigArchiver_Location_rollbackBackends;
	klass->epv.getModificationTime = impl_ConfigArchiver_Location_getModificationTime;
	klass->epv.contains            = impl_ConfigArchiver_Location_contains;
	klass->epv.addBackend          = impl_ConfigArchiver_Location_addBackend;
	klass->epv.removeBackend       = impl_ConfigArchiver_Location_removeBackend;
	klass->epv.doesBackendChange   = impl_ConfigArchiver_Location_doesBackendChange;
	klass->epv.garbageCollect      = impl_ConfigArchiver_Location_garbageCollect;
	klass->epv.delete              = impl_ConfigArchiver_Location_delete;

	klass->epv._get_parent         = impl_ConfigArchiver_Location__get_parent;
	klass->epv._get_path           = impl_ConfigArchiver_Location__get_path;
	klass->epv._get_backendList    = impl_ConfigArchiver_Location__get_backendList;
	klass->epv._get_label          = impl_ConfigArchiver_Location__get_label;
	klass->epv._get_id             = impl_ConfigArchiver_Location__get_id;

	klass->epv._set_label          = impl_ConfigArchiver_Location__set_label;
	klass->epv._set_id             = impl_ConfigArchiver_Location__set_id;

	parent_class = gtk_type_class (BONOBO_X_OBJECT_TYPE);
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
		bonobo_object_ref (BONOBO_OBJECT (location->p->archive));
		break;

	case ARG_LOCID:
		if (GTK_VALUE_POINTER (*arg) != NULL) {
			if (location->p->locid != NULL)
				g_free (location->p->locid);

			location->p->locid = 
				g_strdup (GTK_VALUE_POINTER (*arg));

			if (!strcmp (location->p->locid, "default") &&
			    location->p->label == NULL)
				location->p->label = g_strdup (_("Default Location"));
			else if (location->p->label == NULL)
				location->p->label = g_strdup (location->p->locid);
		}

		break;

	case ARG_LABEL:
		if (GTK_VALUE_POINTER (*arg) != NULL) {
			if (location->p->label != NULL)
				g_free (location->p->label);

			location->p->label =
				g_strdup (GTK_VALUE_POINTER (*arg));
		}

		break;

	case ARG_INHERITS:
		if (GTK_VALUE_POINTER (*arg) != NULL) {
			g_return_if_fail (IS_LOCATION
					  (GTK_VALUE_POINTER (*arg)));
			location->p->parent =
				GTK_VALUE_POINTER (*arg);
			bonobo_object_ref (BONOBO_OBJECT (location->p->parent));
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
	case ARG_LABEL:
		GTK_VALUE_POINTER (*arg) = location->p->label;
		break;
	case ARG_INHERITS:
		GTK_VALUE_POINTER (*arg) = location->p->parent;
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

BonoboObject *
location_new (Archive *archive, const gchar *locid, const gchar *label, Location *inherits) 
{
	GtkObject *object;

	g_return_val_if_fail (archive != NULL, NULL);
	g_return_val_if_fail (IS_ARCHIVE (archive), NULL);
	g_return_val_if_fail (locid != NULL, NULL);

	object = gtk_object_new (location_get_type (),
				 "archive", archive,
				 "locid", locid,
				 "label", label,
				 "inherits", inherits,
				 NULL);

	if (!do_create (LOCATION (object))) {
		bonobo_object_unref (BONOBO_OBJECT (object));
		return NULL;
	}

	LOCATION (object)->p->is_new = TRUE;

	save_metadata (LOCATION (object));

	return BONOBO_OBJECT (object);
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

BonoboObject *
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
		bonobo_object_unref (BONOBO_OBJECT (object));
		return NULL;
	}

	return BONOBO_OBJECT (object);
}

static void 
location_destroy (GtkObject *object) 
{
	Location *location;

	g_return_if_fail (object != NULL);
	g_return_if_fail (IS_LOCATION (object));

	location = LOCATION (object);

	DEBUG_MSG ("Enter: %s", location->p->locid);

	save_metadata (location);

	if (location->p->config_log)
		gtk_object_destroy (GTK_OBJECT (location->p->config_log));

	if (location->p->parent)
		bonobo_object_unref (BONOBO_OBJECT (location->p->parent));

	archive_unregister_location (location->p->archive, location);

	bonobo_object_unref (BONOBO_OBJECT (location->p->archive));

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

	if (location->p->fullpath != NULL)
		g_free (location->p->fullpath);

	if (location->p->label != NULL)
		g_free (location->p->label);

	if (location->p->locid != NULL)
		g_free (location->p->locid);

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

	if (fflush (output) == EOF)
		g_critical ("%s: Could not dump buffer: %s",
			    __FUNCTION__, g_strerror (errno));

	if (fclose (output) == EOF) {
		g_critical ("%s: Could not close output stream: %s",
			    __FUNCTION__, g_strerror (errno));
		return FALSE;
	}

	waitpid (pid, &status, 0);

	return TRUE;
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

	location->p->deleted = TRUE;
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
location_rollback_backends_to (Location  *location,
			       struct tm *date,
			       gint       steps,
			       GList     *backends,
			       gboolean   parent_chain) 
{
	GList *node;
	gchar *backend_id, *filename;
	xmlDocPtr doc;

	g_return_if_fail (location != NULL);
	g_return_if_fail (IS_LOCATION (location));
	g_return_if_fail (location->p->config_log != NULL);
	g_return_if_fail (IS_CONFIG_LOG (location->p->config_log));

	for (node = backends; node; node = node->next) {
		backend_id = node->data;
		filename = location_get_rollback_filename
			(location, date, steps, backend_id, parent_chain);
		if (filename == NULL) continue;

		doc = xmlParseFile (filename);
		g_free (filename);
		if (doc == NULL) continue;

		LOCATION_CLASS (GTK_OBJECT (location)->klass)->do_rollback
			(location, backend_id, doc);
		xmlFreeDoc (doc);
	}
}

/**
 * location_get_storage_filename:
 * @location:
 * @backend_id:
 *
 * Finds a new storage filename for storing rollback data from the backend id
 **/

gchar *
location_get_storage_filename (Location      *location,
			       const gchar   *backend_id,
			       gboolean       is_default)
{
	guint      id;

	g_return_val_if_fail (location != NULL, NULL);
	g_return_val_if_fail (IS_LOCATION (location), NULL);
	g_return_val_if_fail (backend_id != NULL, NULL);

	id = config_log_write_entry (location->p->config_log, backend_id, is_default);

        return g_strdup_printf ("%s/%08x.xml", location->p->fullpath, id);
}

/**
 * location_get_rollback_filename:
 * @location:
 * @date:
 * @steps:
 * @backend_id:
 * @parent_chain:
 *
 * Get the filename for the rollback data for the date or number of steps, and
 * backend id given. String should be freed after use. Returns NULL if no data
 * were found.
 **/

gchar *
location_get_rollback_filename (Location        *location,
				struct tm       *date,
				gint             steps,
				const gchar     *backend_id,
				gboolean         parent_chain)
{
	gint id;

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
		return g_strdup_printf ("%s/%08x.xml", location->p->fullpath, id);
	else if (parent_chain && location->p->parent != NULL)
		return location_get_rollback_filename
			(location->p->parent, date, steps, backend_id, parent_chain);
	else
		return NULL;
}

/**
 * location_storage_complete:
 * @location:
 * @filename:
 *
 * Notify the location object that storage of the rollback data at the given
 * filename is complete
 **/

void
location_storage_complete (Location *location, const gchar *filename) 
{
	const gchar *tmp;
	const gchar *backend_id;
	guint        id;
	BonoboArg   *value;

	tmp = strrchr (filename, '/');

	if (tmp == NULL)
		return;

	sscanf (tmp + 1, "%x", &id);

	backend_id = config_log_get_backend_id_for_id
		(location->p->config_log, id);

	value = bonobo_arg_new (BONOBO_ARG_STRING);
	BONOBO_ARG_SET_STRING (value, backend_id);
	bonobo_event_source_notify_listeners
		(location->p->es, "ConfigArchiver/Location:newRollbackData", value, NULL);
	bonobo_arg_release (value);
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
	xmlDocPtr          doc;
	char               buffer[2048];
	int                t = 0;
	GString           *doc_str;

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
location_store_xml (Location                 *location,
		    gchar                    *backend_id,
		    xmlDocPtr                 xml_doc,
		    ConfigArchiver_StoreType  store_type) 
{
	CORBA_Environment ev;

	g_return_if_fail (location != NULL);
	g_return_if_fail (IS_LOCATION (location));
	g_return_if_fail (xml_doc != NULL);

	CORBA_exception_init (&ev);
	location_client_store_xml (BONOBO_OBJREF (location), backend_id, xml_doc, store_type, &ev);
	CORBA_exception_free (&ev);
}

/**
 * location_get_modification_time:
 * @location:
 * @backend_id:
 *
 * Get the time the particular backend was last modified (archived)
 **/

const struct tm *
location_get_modification_time (Location    *location,
				const gchar *backend_id) 
{
	gint id;

	id = config_log_get_rollback_id_by_steps (location->p->config_log, 0, backend_id);

	if (id < 0)
		return NULL;
	else
		return config_log_get_date_for_id (location->p->config_log, id);
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
location_contains (Location *location, const gchar *backend_id) 
{
	BackendList *list;

	g_return_val_if_fail (location != NULL, FALSE);
	g_return_val_if_fail (IS_LOCATION (location), FALSE);

	if (location->p->parent == NULL) {
		list = archive_get_backend_list (location->p->archive);

		if (backend_list_contains (list, backend_id))
			return ConfigArchiver_CONTAIN_FULL;
		else
			return ConfigArchiver_CONTAIN_NONE;
	} else {
		const BackendNote *note = find_note (location, backend_id);

		if (note != NULL)
			return note->type;
		else
			return ConfigArchiver_CONTAIN_NONE;
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
location_add_backend (Location *location,
		      const gchar *backend_id,
		      ContainmentType type) 
{
	g_return_val_if_fail (location != NULL, -1);
	g_return_val_if_fail (IS_LOCATION (location), -1);
	g_return_val_if_fail (backend_id != NULL, -2);

	if (location->p->parent == NULL) return -1;

	if (type == ConfigArchiver_CONTAIN_NONE) return -3;

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
location_remove_backend (Location *location, const gchar *backend_id) 
{
	GList *node;
	BackendNote *note;

	g_return_if_fail (location != NULL);
	g_return_if_fail (IS_LOCATION (location));
	g_return_if_fail (backend_id != NULL);

	if (location->p->parent == NULL) return;

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

	for (tmp = location; tmp; tmp = tmp->p->parent) depth++;
	for (tmp = location1; tmp; tmp = tmp->p->parent) depth1++;

	while (depth > depth1) {
		location = location->p->parent;
		list_node = g_list_prepend (list_node, location);
		depth--;
	}

	while (depth1 > depth) {
		location1 = location1->p->parent;
		depth1--;
	}

	while (location && location1 && location != location1) {
		location = location->p->parent;
		location1 = location1->p->parent;
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

	return location->p->parent;
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
location_does_backend_change (Location *location,
			      Location *location1,
			      const gchar *backend_id) 
{
	GList *backends;
	gboolean ret;
	gchar *str;

	g_return_val_if_fail (location != NULL, FALSE);
	g_return_val_if_fail (IS_LOCATION (location), FALSE);
	g_return_val_if_fail (location1 != NULL, FALSE);
	g_return_val_if_fail (IS_LOCATION (location1), FALSE);	
	g_return_val_if_fail (backend_id != NULL, FALSE);

	backends = location_get_changed_backends (location, location1);
	str = g_strdup (backend_id);
	ret = !(g_list_find_custom (backends, str,
				    (GCompareFunc) strcmp) == NULL);
	g_free (str);
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
				      pipe, ConfigArchiver_STORE_DEFAULT);
		fclose (pipe);

		if (ret < 0)
			return -1;
	}

	return 0;
}

/**
 * location_garbage_collect:
 * @location:
 *
 * Iterates through backends and eliminates excess archived data from the
 * configuration log and the XML archive
 **/

static void
garbage_collect_cb (ConfigLog *config_log, gchar *backend_id, gint id, Location *location) 
{
	gchar *filename;

	filename = g_strdup_printf ("%s/%08x.xml", location->p->fullpath, id);
	DEBUG_MSG ("Removing %s", filename);
	unlink (filename);
	g_free (filename);
}

void
location_garbage_collect (Location *location)
{
	GList *node;
	BackendNote *note;

	g_return_if_fail (location != NULL);
	g_return_if_fail (IS_LOCATION (location));

	for (node = location->p->contains_list; node != NULL; node = node->next) {
		note = node->data;
		config_log_garbage_collect (location->p->config_log,
					    note->backend_id,
					    (GarbageCollectCB) garbage_collect_cb,
					    location);
	}
}

/**
 * location_is_deleted:
 * @location:
 *
 * Returns TRUE iff the location is marked deleted
 **/

gboolean
location_is_deleted (const Location *location) 
{
	return location->p->deleted;
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
 * FIXME: Better error reporting */

static gboolean
do_create (Location *location) 
{
	gint ret = 0;

	g_return_val_if_fail (location != NULL, FALSE);
	g_return_val_if_fail (IS_LOCATION (location), FALSE);
	g_return_val_if_fail (location->p->archive != NULL, FALSE);
	g_return_val_if_fail (IS_ARCHIVE (location->p->archive), FALSE);
	g_return_val_if_fail (location->p->locid != NULL, FALSE);

	if (location->p->parent == NULL)
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

	location->p->fullpath = g_concat_dir_and_file
		(archive_get_prefix (location->p->archive), location->p->locid);

	if (g_file_test (location->p->fullpath, G_FILE_TEST_ISDIR) == FALSE)
		return FALSE;

	metadata_filename =
		g_concat_dir_and_file (location->p->fullpath, "location.xml");

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
	char *inherits_str = NULL, *label_str = NULL, *contains_str, *type_str;
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
		else if (!strcmp (node->name, "label")) {
			label_str = xmlNodeGetContent (node);
		}
		else if (!strcmp (node->name, "contains")) {
			contains_str = xmlGetProp (node, "backend");
			type_str = xmlGetProp (node, "type");

			if (contains_str != NULL) {
				if (!strcmp (type_str, "full"))
					type = ConfigArchiver_CONTAIN_FULL;
				else if (!strcmp (type_str, "partial"))
					type = ConfigArchiver_CONTAIN_PARTIAL;
				else {
					type = ConfigArchiver_CONTAIN_NONE;
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
			location->p->parent =
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

	if (label_str != NULL) {
		if (location->p->label != NULL)
			g_free (location->p->label);

		location->p->label = label_str;
	}

	return TRUE;
}

static void
save_metadata (Location *location) 
{
	gchar *metadata_filename;

	if (location->p->deleted || (!location->p->is_new && !location->p->contains_list_dirty))
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

	if (location->p->parent) {
		g_return_if_fail
			(location->p->parent->p->locid != NULL);

		child_node = xmlNewChild (root_node, NULL, "inherits", NULL);
		xmlNewProp (child_node, "location",
			    location->p->parent->p->locid);

		for (node = location->p->contains_list; node;
		     node = node->next) 
		{
			note = node->data;
			child_node = xmlNewChild (root_node, NULL, 
						  "contains", NULL);
			xmlNewProp (child_node, "backend", note->backend_id);

			if (note->type == ConfigArchiver_CONTAIN_PARTIAL)
				xmlNewProp (child_node, "type", "partial");
			else if (note->type == ConfigArchiver_CONTAIN_FULL)
				xmlNewProp (child_node, "type", "full");
		}
	}

	xmlNewChild (root_node, NULL, "label", location->p->label);

	xmlDocSetRootElement (doc, root_node);

	/* FIXME: Report errors here */
	xmlSaveFile (filename, doc);
	xmlFreeDoc (doc);
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
backend_note_new (const gchar *backend_id, ContainmentType type)
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
find_note (Location *location, const gchar *backend_id)
{
	GList *node;

	for (node = location->p->contains_list; node; node = node->next)
		if (!strcmp (((BackendNote *)node->data)->backend_id,
			     backend_id))
			return node->data;

	return NULL;
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
