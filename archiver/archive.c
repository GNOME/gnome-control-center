/* -*- mode: c; style: linux -*- */

/* archive.c
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
#include <dirent.h>
#include <errno.h>

#include "archive.h"
#include "util.h"

static GtkObjectClass *parent_class;

enum {
	ARG_0,
	ARG_PREFIX,
	ARG_IS_GLOBAL
};

#define ARCHIVE_FROM_SERVANT(servant) (ARCHIVE (bonobo_object_from_servant (servant)))

static void archive_init          (Archive *archive);
static void archive_class_init    (ArchiveClass *klass);

static void archive_destroy       (GtkObject *object);

static void archive_set_arg       (GtkObject *object,
				   GtkArg *arg,
				   guint arg_id);

static void archive_get_arg       (GtkObject *object,
				   GtkArg *arg,
				   guint arg_id);

static void load_all_locations    (Archive *archive);

/* CORBA interface methods */

static ConfigArchiver_Location
impl_ConfigArchiver_Archive_getLocation (PortableServer_Servant  servant,
					 const CORBA_char       *locid,
					 CORBA_Environment      *ev) 
{
	Location *loc;

	loc = archive_get_location (ARCHIVE_FROM_SERVANT (servant), locid);

	if (loc == NULL) {
		bonobo_exception_set (ev, ex_ConfigArchiver_Archive_LocationNotFound);
		return CORBA_OBJECT_NIL;
	} else {
		return CORBA_Object_duplicate (BONOBO_OBJREF (loc), ev);
	}
}

static ConfigArchiver_Location
impl_ConfigArchiver_Archive_createLocation (PortableServer_Servant         servant,
					    const CORBA_char              *locid,
					    const CORBA_char              *label,
					    const ConfigArchiver_Location  parent_ref,
					    CORBA_Environment             *ev) 
{
	Location *loc;

	loc = archive_create_location (ARCHIVE_FROM_SERVANT (servant), locid, label,
				       LOCATION (bonobo_object_from_servant (parent_ref->servant)));

	return CORBA_Object_duplicate (BONOBO_OBJREF (loc), ev);
}

static ConfigArchiver_LocationSeq *
impl_ConfigArchiver_Archive_getChildLocations (PortableServer_Servant   servant,
					       ConfigArchiver_Location  location_ref,
					       CORBA_Environment       *ev) 
{
	ConfigArchiver_LocationSeq *ret;
	Archive *archive;
	Location *location;
	GList *locs, *tmp;
	guint i = 0;

	archive = ARCHIVE_FROM_SERVANT (servant);

	if (location_ref == CORBA_OBJECT_NIL)
		location = NULL;
	else
		location = LOCATION (bonobo_object_from_servant (location_ref->servant));

	locs = archive_get_child_locations (archive, location);

	ret = ConfigArchiver_LocationSeq__alloc ();
	ret->_length = g_list_length (locs);
	ret->_buffer = CORBA_sequence_ConfigArchiver_Location_allocbuf (ret->_length);

	for (tmp = locs; tmp != NULL; tmp = tmp->next)
		ret->_buffer[i++] = CORBA_Object_duplicate (BONOBO_OBJREF (tmp->data), ev);

	g_list_free (locs);
	return ret;
}

static CORBA_char *
impl_ConfigArchiver_Archive__get_prefix (PortableServer_Servant  servant,
					 CORBA_Environment      *ev) 
{
	return CORBA_string_dup (ARCHIVE_FROM_SERVANT (servant)->prefix);
}

static CORBA_boolean
impl_ConfigArchiver_Archive__get_isGlobal (PortableServer_Servant  servant,
					   CORBA_Environment      *ev) 
{
	return ARCHIVE_FROM_SERVANT (servant)->is_global;
}

static ConfigArchiver_BackendList
impl_ConfigArchiver_Archive__get_backendList (PortableServer_Servant  servant,
					      CORBA_Environment      *ev) 
{
	return bonobo_object_dup_ref (BONOBO_OBJREF (ARCHIVE_FROM_SERVANT (servant)->backend_list), ev);
}

static ConfigArchiver_Location
impl_ConfigArchiver_Archive__get_currentLocation (PortableServer_Servant  servant,
						  CORBA_Environment      *ev)
{
	return CORBA_Object_duplicate (BONOBO_OBJREF (archive_get_current_location (ARCHIVE_FROM_SERVANT (servant))), ev);
}

static void
impl_ConfigArchiver_Archive__set_currentLocation (PortableServer_Servant   servant,
						  ConfigArchiver_Location  location_ref,
						  CORBA_Environment       *ev) 
{
	Archive *archive = ARCHIVE_FROM_SERVANT (servant);
	Location *location;

	location = LOCATION (bonobo_object_from_servant (location_ref->servant));

	if (location == NULL)
		bonobo_exception_set (ev, ex_ConfigArchiver_Archive_LocationNotFound);
	else
		archive_set_current_location (archive, location);
}

static void
impl_ConfigArchiver_Archive__set_currentLocationId (PortableServer_Servant   servant,
						    const CORBA_char        *locid,
						    CORBA_Environment       *ev) 
{
	archive_set_current_location_id (ARCHIVE_FROM_SERVANT (servant), locid);
}

CORBA_char *
impl_ConfigArchiver_Archive__get_currentLocationId (PortableServer_Servant  servant,
						    CORBA_Environment      *ev) 
{
	return CORBA_string_dup (archive_get_current_location_id (ARCHIVE_FROM_SERVANT (servant)));
}

BONOBO_X_TYPE_FUNC_FULL (Archive, ConfigArchiver_Archive, BONOBO_X_OBJECT_TYPE, archive);

static void
archive_init (Archive *archive) 
{
	archive->prefix = NULL;
	archive->locations = g_tree_new ((GCompareFunc) strcmp);
	archive->current_location_id = NULL;
}

static void
archive_class_init (ArchiveClass *klass) 
{
	GtkObjectClass *object_class;

	object_class = GTK_OBJECT_CLASS (klass);

	object_class->destroy = archive_destroy;
	object_class->set_arg = archive_set_arg;
	object_class->get_arg = archive_get_arg;

	gtk_object_add_arg_type ("Archive::prefix",
				 GTK_TYPE_POINTER,
				 GTK_ARG_READWRITE | GTK_ARG_CONSTRUCT_ONLY,
				 ARG_PREFIX);
	gtk_object_add_arg_type ("Archive::is-global",
				 GTK_TYPE_INT,
				 GTK_ARG_READWRITE | GTK_ARG_CONSTRUCT_ONLY,
				 ARG_IS_GLOBAL);

	klass->epv.getLocation            = impl_ConfigArchiver_Archive_getLocation;
	klass->epv.createLocation         = impl_ConfigArchiver_Archive_createLocation;
	klass->epv.getChildLocations      = impl_ConfigArchiver_Archive_getChildLocations;

	klass->epv._get_prefix            = impl_ConfigArchiver_Archive__get_prefix;
	klass->epv._get_isGlobal          = impl_ConfigArchiver_Archive__get_isGlobal;
	klass->epv._get_backendList       = impl_ConfigArchiver_Archive__get_backendList;
	klass->epv._get_currentLocation   = impl_ConfigArchiver_Archive__get_currentLocation;
	klass->epv._get_currentLocationId = impl_ConfigArchiver_Archive__get_currentLocationId;

	klass->epv._set_currentLocation   = impl_ConfigArchiver_Archive__set_currentLocation;
	klass->epv._set_currentLocationId = impl_ConfigArchiver_Archive__set_currentLocationId;

	parent_class = gtk_type_class (BONOBO_X_OBJECT_TYPE);
}

static void
archive_set_arg (GtkObject *object, GtkArg *arg, guint arg_id) 
{
	Archive *archive;

	g_return_if_fail (object != NULL);
	g_return_if_fail (IS_ARCHIVE (object));
	g_return_if_fail (arg != NULL);

	archive = ARCHIVE (object);

	switch (arg_id) {
	case ARG_PREFIX:
		if (GTK_VALUE_POINTER (*arg) != NULL)
			archive->prefix = g_strdup (GTK_VALUE_POINTER (*arg));
		break;

	case ARG_IS_GLOBAL:
		archive->is_global = GTK_VALUE_INT (*arg);
		archive->backend_list = 
			BACKEND_LIST (backend_list_new (archive->is_global));
		break;

	default:
		break;
	}
}

static void 
archive_get_arg (GtkObject *object, GtkArg *arg, guint arg_id) 
{
	Archive *archive;

	g_return_if_fail (object != NULL);
	g_return_if_fail (IS_ARCHIVE (object));
	g_return_if_fail (arg != NULL);

	archive = ARCHIVE (object);

	switch (arg_id) {
	case ARG_PREFIX:
		GTK_VALUE_POINTER (*arg) = archive->prefix;
		break;

	case ARG_IS_GLOBAL:
		GTK_VALUE_INT (*arg) = archive->is_global;
		break;

	default:
		arg->type = GTK_TYPE_INVALID;
		break;
	}
}

/**
 * archive_construct:
 * @archive:
 * @is_new: TRUE iff this is a new archive
 *
 * Load the archive information from disk
 *
 * Returns: TRUE on success and FALSE on failure
 */

gboolean
archive_construct (Archive *archive, gboolean is_new)
{
	gint ret = 0;

	g_return_val_if_fail (archive != NULL, FALSE);
	g_return_val_if_fail (IS_ARCHIVE (archive), FALSE);
	g_return_val_if_fail (archive->prefix != NULL, FALSE);

	if (is_new) {
		if (g_file_exists (archive->prefix))
			return FALSE;

		ret = mkdir (archive->prefix, S_IREAD | S_IWRITE | S_IEXEC);
		if (ret == -1) return FALSE;
	} else {
		if (!g_file_test (archive->prefix, G_FILE_TEST_ISDIR))
			return FALSE;
	}

	return TRUE;
}

/**
 * archive_load:
 * @is_global: TRUE iff we should load the global archive
 * 
 * Load either the global or per-user configuration archive
 * 
 * Return value: Reference to archive
 **/

BonoboObject *
archive_load (gboolean is_global) 
{
	BonoboObject *object;
	gchar *prefix;

	if (is_global)
		prefix = "/var/ximian-setup-tools";
	else
		prefix = g_concat_dir_and_file (g_get_home_dir (),
						".gnome/capplet-archive");

	object = BONOBO_OBJECT (gtk_object_new (archive_get_type (),
						"prefix", prefix,
						"is-global", is_global,
						NULL));

	if (!is_global)
		g_free (prefix);

	if (archive_construct (ARCHIVE (object), FALSE) == FALSE &&
	    archive_construct (ARCHIVE (object), TRUE) == FALSE)
	{
		bonobo_object_unref (object);
		return NULL;
	}

	return object;
}

static gint
free_location_cb (gchar *locid, BonoboObject *location) 
{
	bonobo_object_unref (location);
	g_free (locid);

	return FALSE;
}

static void 
archive_destroy (GtkObject *object) 
{
	Archive *archive;

	g_return_if_fail (object != NULL);
	g_return_if_fail (IS_ARCHIVE (object));

	DEBUG_MSG ("Enter");

	archive = ARCHIVE (object);

	g_tree_traverse (archive->locations, 
			 (GTraverseFunc) free_location_cb,
			 G_IN_ORDER,
			 NULL);

	g_tree_destroy (archive->locations);

	if (archive->current_location_id != NULL)
		g_free (archive->current_location_id);

	if (archive->backend_list != NULL)
		bonobo_object_unref (BONOBO_OBJECT (archive->backend_list));

	GTK_OBJECT_CLASS (parent_class)->destroy (GTK_OBJECT (archive));
}

/**
 * archive_get_location:
 * @locid: 
 * 
 * Get a reference to the location with the given name. 
 * 
 * Return value: Reference to location, NULL if no such location exists
 **/

Location *
archive_get_location (Archive       *archive,
		      const gchar   *locid) 
{
	BonoboObject *loc_obj;
	gchar *tmp;

	g_return_val_if_fail (archive != NULL, NULL);
	g_return_val_if_fail (IS_ARCHIVE (archive), NULL);
	g_return_val_if_fail (locid != NULL, NULL);

	/* Stupid borken glib... */
	tmp = g_strdup (locid);
	loc_obj = g_tree_lookup (archive->locations, tmp);
	g_free (tmp);

	if (loc_obj == NULL) {
		loc_obj = location_open (archive, locid);

		if (loc_obj == NULL)
			return NULL;
		else
			g_tree_insert (archive->locations, 
				       g_strdup (locid), loc_obj);
	} else {
		bonobo_object_ref (loc_obj);
	}

	return LOCATION (loc_obj);
}

/**
 * archive_create_location:
 * @archive:
 * @location:
 *
 * Creates a new location
 */

Location *
archive_create_location (Archive     *archive,
			 const gchar *locid,
			 const gchar *label,
			 Location    *parent)
{
	Location *location;

	g_return_val_if_fail (archive != NULL, NULL);
	g_return_val_if_fail (IS_ARCHIVE (archive), NULL);
	g_return_val_if_fail (locid != NULL, NULL);
	g_return_val_if_fail (label != NULL, NULL);
	g_return_val_if_fail (parent == NULL || IS_LOCATION (parent), NULL);

	location = LOCATION (location_new (archive, locid, label, parent));

	if (location == NULL)
		return NULL;

	g_tree_insert (archive->locations, g_strdup (locid), location);
	return location;
}

/**
 * archive_unregister_location:
 * @archive: 
 * @location: 
 * 
 * Unregisters a location from the archive
 **/

void
archive_unregister_location (Archive *archive, Location *location) 
{
	gchar *tmp;

	g_return_if_fail (archive != NULL);
	g_return_if_fail (IS_ARCHIVE (archive));
	g_return_if_fail (location != NULL);
	g_return_if_fail (IS_LOCATION (location));

	if (GTK_OBJECT_DESTROYED (archive)) return;

	tmp = g_strdup (location_get_id (location));
	g_tree_remove (archive->locations, tmp);
	g_free (tmp);
}

/**
 * archive_get_current_location:
 * 
 * Convenience function to get a pointer to the current location
 * 
 * Return value: Pointer to current location, or NULL if the current location
 * does not exist and a default location could not be created
 **/

Location *
archive_get_current_location (Archive *archive)
{
	const gchar *locid;

	g_return_val_if_fail (archive != NULL, NULL);
	g_return_val_if_fail (IS_ARCHIVE (archive), NULL);

	locid = archive_get_current_location_id (archive);

	if (locid == NULL)
		return NULL;
	else
		return archive_get_location (archive, locid);
}

/**
 * archive_set_current_location:
 * @location: Location to which to set archive
 * 
 * Set the current location in an archive to the location given; apply
 * configuration for the new location to all backends necessary
 **/

void
archive_set_current_location (Archive  *archive,
			      Location *location) 
{
	Location *old_location;
	GList *backends;

	g_return_if_fail (archive != NULL);
	g_return_if_fail (IS_ARCHIVE (archive));
	g_return_if_fail (location != NULL);
	g_return_if_fail (IS_LOCATION (location));

	old_location = archive_get_current_location (archive);

	archive_set_current_location_id (archive, location_get_id (location));

	backends = location_get_changed_backends (location, old_location);
	location_rollback_backends_to (location, NULL, 0, backends, TRUE);
}

/**
 * archive_set_current_location_id: 
 * @name: 
 * 
 * Sets the current location's name, but does not invoke any rollback
 **/

void
archive_set_current_location_id (Archive       *archive,
				 const gchar   *locid) 
{
	g_return_if_fail (archive != NULL);
	g_return_if_fail (IS_ARCHIVE (archive));
	g_return_if_fail (locid != NULL);

	if (archive->current_location_id != NULL)
		g_free (archive->current_location_id);

	archive->current_location_id = g_strdup (locid);

	if (archive->is_global)
		gnome_config_set_string
			("/ximian-setup-tools/config/current/global-location",
			 archive->current_location_id);
	else
		gnome_config_set_string
			("/capplet-archive/config/current/location",
			 archive->current_location_id);

	gnome_config_sync ();
}

/**
 * archive_get_current_location_id:
 * 
 * Get the name of the current location
 * 
 * Return value: String containing current location, should not be freed, or
 * NULL if no current location exists and the default location could not be
 * created
 **/

const gchar *
archive_get_current_location_id (Archive *archive) 
{
	gboolean def;
	Location *loc;
	Location *current_location;

	g_return_val_if_fail (archive != NULL, NULL);
	g_return_val_if_fail (IS_ARCHIVE (archive), NULL);

	if (archive->current_location_id == NULL) {
		if (archive->is_global)
			archive->current_location_id =
				gnome_config_get_string_with_default
				("/ximian-setup-tools/config/current/global-location=default", &def);
		else
			archive->current_location_id =
				gnome_config_get_string_with_default
				("/capplet-archive/config/current/location=default", &def);

		/* Create default location if it does not exist */
		if (def) {
			current_location =
				archive_get_location (archive, archive->current_location_id);

			if (current_location == NULL) {
				loc = archive_create_location (archive, archive->current_location_id,
							       _("Default location"), NULL);
				if (archive->is_global &&
				    location_store_full_snapshot (loc) < 0)
				{
					location_delete (loc);
					return NULL;
				}

				bonobo_object_unref (BONOBO_OBJECT (loc));
			} else {
				bonobo_object_unref (BONOBO_OBJECT (current_location));
			}
		}
	}

	return archive->current_location_id;
}

/**
 * archive_get_prefix:
 * @archive: 
 * 
 * Get the prefix for locations in this archive
 * 
 * Return value: String containing prefix; should not be freed
 **/

const gchar *
archive_get_prefix (Archive *archive) 
{
	g_return_val_if_fail (archive != NULL, FALSE);
	g_return_val_if_fail (IS_ARCHIVE (archive), FALSE);

	return archive->prefix;
}

/**
 * archive_is_global:
 * @archive: 
 * 
 * Tell whether the archive is global or per-user
 * 
 * Return value: TRUE if global, FALSE if per-user
 **/

gboolean
archive_is_global (Archive *archive) 
{
	g_return_val_if_fail (archive != NULL, FALSE);
	g_return_val_if_fail (IS_ARCHIVE (archive), FALSE);

	return archive->is_global;
}

/**
 * archive_get_backend_list:
 * @archive: 
 * 
 * Get the master backend list for this archive
 * 
 * Return value: Reference to the master backend list
 **/

BackendList *
archive_get_backend_list (Archive *archive)
{
	g_return_val_if_fail (archive != NULL, FALSE);
	g_return_val_if_fail (IS_ARCHIVE (archive), FALSE);

	return archive->backend_list;
}

/**
 * archive_foreach_child_location:
 * @archive:
 * @callback: Callback to invoke
 * @parent: Iterate through the children of this location; iterate through
 * toplevel locations if this is NULL
 * @data: Arbitrary data to pass to the callback
 *
 * Invoke the given callback for each location that inherits the given
 * location, or for each toplevel location if the parent given is
 * NULL. Terminate the iteration if any child returns a nonzero value
 **/

static gint
foreach_build_list_cb (gchar *key, Location *value, GList **node) 
{
	if (!location_is_deleted (value))
		*node = g_list_prepend (*node, value);

	return 0;
}

GList *
archive_get_child_locations (Archive *archive,
			     Location *parent)
{
	GList *list = NULL, *node, *tmp;
	Location *loc;

	g_return_val_if_fail (archive != NULL, NULL);
	g_return_val_if_fail (IS_ARCHIVE (archive), NULL);

	load_all_locations (archive);

	g_tree_traverse (archive->locations,
			 (GTraverseFunc) foreach_build_list_cb,
			 G_IN_ORDER,
			 &list);

	node = list;

	while (node != NULL) {
		loc = node->data;
		tmp = node->next;

		if (location_get_parent (loc) != parent) {
			list = g_list_remove_link (list, node);
			g_list_free_1 (node);
			bonobo_object_unref (BONOBO_OBJECT (loc));
		}

		node = tmp;
	}

	return list;
}

/* Load and register all the locations for this archive */

static void
load_all_locations (Archive *archive) 
{
	DIR *archive_dir;
	struct dirent entry, *entryp;
	gchar *filename;

	archive_dir = opendir (archive->prefix);

	if (archive_dir == NULL) {
		g_warning ("load_all_locations: %s", g_strerror (errno));
		return;
	}

	while (1) {
		if (readdir_r (archive_dir, &entry, &entryp)) {
			g_warning ("load_all_locations: %s",
				   g_strerror (errno));
			break;
		}

		if (entryp == NULL) break;

		if (strcmp (entry.d_name, ".") &&
		    strcmp (entry.d_name, ".."))
		{
			filename = g_concat_dir_and_file (archive->prefix,
							  entry.d_name);
			if (g_file_test (filename, G_FILE_TEST_ISDIR))
				archive_get_location (archive, entry.d_name);
		}
	}
}
