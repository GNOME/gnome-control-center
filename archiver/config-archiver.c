/* -*- mode: c; style: linux -*- */

/* main.c
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
#include <time.h>
#include <sys/time.h>
#include <sys/file.h>
#include <unistd.h>
#include <errno.h>

#include <gnome.h>
#include <bonobo.h>

#include <gnome-xml/parser.h>

#include "archiver-client.h"
#include "util.h"

/* Variables resulting from command line parsing */

static gboolean store;
static gboolean rollback;
static gboolean change_location;
static gboolean rename_location;
static gboolean push_config;
static gboolean garbage_collect;

static gboolean add_location;
static gboolean remove_location;
static gboolean add_backend;
static gboolean remove_backend;

static gboolean global;
static const gchar *location_id;

static gchar *backend_id;

static gboolean compare_parent;
static gboolean mask_previous;

static gchar *date_str;
static gboolean all;
static gchar *revision_id;
static gboolean last;
static guint steps;
static gboolean show;

static gchar *parent_str;
static gchar *new_name;

static gboolean contain_full;
static gboolean contain_partial;
static gboolean master;

static struct poptOption archiver_operations[] = {
	{"store", 's', POPT_ARG_NONE, &store, 0,
	 N_("Store XML data in the archive")},
	{"rollback", 'r', POPT_ARG_NONE, &rollback, 0,
	 N_("Roll back the configuration to a given point")},
	{"change-location", 'c', POPT_ARG_NONE, &change_location, 0,
	 N_("Change the location profile to the given one")},
	{"push-config", 'p', POPT_ARG_NONE, &push_config, 0,
	 N_("Push configuration data out to client machines (UNIMPLEMENTED)")},
	{"rename-location", '\0', POPT_ARG_NONE, &rename_location, 0,
	 N_("Rename a location to a new name")},
	{"add-location", '\0', POPT_ARG_NONE, &add_location, 0,
	 N_("Add a new location to the archive")},
	{"remove-location", '\0', POPT_ARG_NONE, &remove_location, 0,
	 N_("Remove a location from the archive")},
	{"add-backend", '\0', POPT_ARG_NONE, &add_backend, 0,
	 N_("Add a given backend to the given location")},
	{"remove-backend", '\0', POPT_ARG_NONE, &remove_backend, 0,
	 N_("Remove the given backend from the given location")},
	{"garbage-collect", '\0', POPT_ARG_NONE, &garbage_collect, 0,
	 N_("Perform garbage collection on the given location")},
	{NULL, '\0', 0, NULL, 0}
};

static struct poptOption global_options[] = {
        {"global", 'g', POPT_ARG_NONE, &global, 0, 
	 N_("Use the global repository")},
	{"location", 'l', POPT_ARG_STRING, &location_id, 0,
	 N_("Identifier of location profile on which to operate"), 
	 N_("LOCATION")},
        {"backend", 'b', POPT_ARG_STRING, &backend_id, 0, 
	 N_("Backend being used for this operation"), N_("BACKEND_ID")},
        {NULL, '\0', 0, NULL, 0}
};

static struct poptOption store_options[] = {
	{"compare-parent", '\0', POPT_ARG_NONE, &compare_parent, 0,
	 N_("Store only the differences with the parent location's config")},
	{"mask-previous", '\0', POPT_ARG_NONE, &mask_previous, 0,
	 N_("Store only those settings set in the previous config")},
	{NULL, '\0', 0, NULL, 0}
};

static struct poptOption rollback_options[] = {
	{"date", 'd', POPT_ARG_STRING, &date_str, 0,
	 N_("Date to which to roll back"), N_("DATE")},
	{"all", 'a', POPT_ARG_NONE, &all, 0,
	 N_("Roll back all configuration items")},
	{"revision-id", 'i', POPT_ARG_INT, &revision_id, 0,
	 N_("Roll back to the revision REVISION_ID"), N_("REVISION_ID")},
	{"last", 't', POPT_ARG_NONE, &last, 0,
	 N_("Roll back to the last known revision")},
	{"steps", '\0', POPT_ARG_INT, &steps, 0,
	 N_("Roll back by STEPS revisions"), N_("STEPS")},
	{"show", 'h', POPT_ARG_NONE, &show, 0,
	 N_("Don't run the backend, just dump the output")},
	{NULL, '\0', 0, NULL, 0}
};

static struct poptOption add_rename_location_options[] = {
	{"parent", '\0', POPT_ARG_STRING, &parent_str, 0,
	 N_("Parent location for the new location"), N_("PARENT")},
	{"new-name", '\0', POPT_ARG_STRING, &new_name, 0,
	 N_("New name to assign to the location"), N_("NEW_NAME")},
	{NULL, '\0', 0, NULL, 0}
};

static struct poptOption add_remove_backend_options[] = {
	{"master", '\0', POPT_ARG_NONE, &master, 0,
	 N_("Add/remove this backend to/from the master backend list")},
	{"full", '\0', POPT_ARG_NONE, &contain_full, 0,
	 N_("Full containment")},
	{"partial", '\0', POPT_ARG_NONE, &contain_partial, 0,
	 N_("Partial containment")},
	{NULL, '\0', 0, NULL, 0}
};

static xmlDocPtr
xml_load_from_stream (FILE *stream) 
{
	GString *str;
	gchar buffer[4097];
	xmlDocPtr doc;
	size_t t;

	str = g_string_new ("");

	while (!feof (stream)) {
		t = fread (buffer, sizeof (char), 4096, stream);
		buffer[t] = '\0';
		g_string_append (str, buffer);
	}

	doc = xmlParseDoc (str->str);
	g_string_free (str, TRUE);
	return doc;
}

static ConfigArchiver_StringSeq *
make_backend_id_seq (gchar *backend_id, ...) 
{
	ConfigArchiver_StringSeq *seq;

	seq = ConfigArchiver_StringSeq__alloc ();
	seq->_length = 1;
	seq->_buffer = CORBA_sequence_CORBA_string_allocbuf (1);
	seq->_buffer[0] = CORBA_string_dup (backend_id);

	return seq;
}

static void
do_store (ConfigArchiver_Location location, CORBA_Environment *ev) 
{
	ConfigArchiver_StoreType type;
	xmlDocPtr doc;

	if (!backend_id) {
		fprintf (stderr, "No backend specified\n");
		return;
	}

	if (mask_previous)
		type = ConfigArchiver_STORE_MASK_PREVIOUS;
	else if (compare_parent)
		type = ConfigArchiver_STORE_COMPARE_PARENT;
	else
		type = ConfigArchiver_STORE_FULL;

	doc = xml_load_from_stream (stdin);

	location_client_store_xml (location, backend_id, doc, type, ev);
}

static void
do_rollback (ConfigArchiver_Location location, CORBA_Environment *ev) 
{
	struct tm *date = NULL;
	ConfigArchiver_StringSeq *seq;
	ConfigArchiver_Time tm;
	xmlDocPtr doc;

	if (date_str)
		date = parse_date (date_str);
	else if (last || steps > 0)
		date = NULL;
	else if (!revision_id) {
		fprintf (stderr, "No date specified\n");
		return;
	}

	if (backend_id != NULL && (date != NULL || last)) {
		/* FIXME: Need to support specifying multiple backends */
		if (show) {
			doc = location_client_load_rollback_data (location, date, 0, backend_id, TRUE, ev);
			xmlDocDump (stdout, doc);
			xmlFreeDoc (doc);
		} else {
			tm = mktime (date);
			seq = make_backend_id_seq (backend_id, NULL);
			ConfigArchiver_Location_rollbackBackends (location, tm, 0, seq, TRUE, ev);
			CORBA_free (seq);
		}
	}
	else if (backend_id != NULL && steps != 0) {
		if (show) {
			doc = location_client_load_rollback_data (location, date, steps, backend_id, TRUE, ev);
			xmlDocDump (stdout, doc);
			xmlFreeDoc (doc);
		} else {
			seq = make_backend_id_seq (backend_id, NULL);
			ConfigArchiver_Location_rollbackBackends (location, 0, steps, seq, TRUE, ev);
			CORBA_free (seq);
		}
	} else {
		g_message ("No backend specified\n");
		return;
	}
}

static void
do_change_location (ConfigArchiver_Archive archive, ConfigArchiver_Location location, CORBA_Environment *ev) 
{
	ConfigArchiver_Archive__set_currentLocation (archive, location, ev);
}

static void
do_rename_location (ConfigArchiver_Archive archive, ConfigArchiver_Location location, CORBA_Environment *ev) 
{
	gboolean is_current;
	CORBA_char *locid, *cid;

	if (new_name == NULL) {
		fprintf (stderr, "You did not specify a new name. Try --help\n");
	} else {
		locid = ConfigArchiver_Location__get_id (location, ev);
		cid = ConfigArchiver_Archive__get_currentLocationId (archive, ev);

		if (!strcmp (locid, cid))
			is_current = TRUE;
		else
			is_current = FALSE;

		CORBA_free (locid);
		CORBA_free (cid);

		ConfigArchiver_Location__set_id (location, new_name, ev);

		if (is_current) 
			ConfigArchiver_Archive__set_currentLocationId (archive, new_name, ev);
	}
}

static void
do_add_location (ConfigArchiver_Archive archive, CORBA_Environment *ev) 
{
	ConfigArchiver_Location parent_location = CORBA_OBJECT_NIL;

	if (location_id == NULL) {
		fprintf (stderr, "Error: You did not specify a location name\n");
		return;
	}

	if (parent_str != NULL) {
		parent_location = ConfigArchiver_Archive_getLocation (archive, parent_str, ev);

		if (parent_location == NULL && !strcmp (parent_str, "default"))
			parent_location =
				ConfigArchiver_Archive_createLocation (archive, "default", _("Default Location"),
								       CORBA_OBJECT_NIL, ev);
	}

	ConfigArchiver_Archive_createLocation (archive, location_id, location_id, parent_location, ev);
}

static void
do_remove_location (ConfigArchiver_Location location, CORBA_Environment *ev)
{
	ConfigArchiver_Location_delete (location, ev);
}

static void
do_add_backend (ConfigArchiver_Location location, CORBA_Environment *ev) 
{
	ConfigArchiver_ContainmentType type;

	if (contain_full && contain_partial) {
		fprintf (stderr, "Error: Cannot have both full and partial "
			 "containment\n");
		return;
	}
	else if (contain_partial) {
		type = ConfigArchiver_CONTAIN_PARTIAL;
	} else {
		type = ConfigArchiver_CONTAIN_FULL;
	}

	ConfigArchiver_Location_addBackend (location, backend_id, type, ev);
}

static void
do_remove_backend (ConfigArchiver_Location location, CORBA_Environment *ev) 
{
	ConfigArchiver_Location_removeBackend (location, backend_id, ev);
}

static void
do_garbage_collect (ConfigArchiver_Location location, CORBA_Environment *ev) 
{
	ConfigArchiver_Location_garbageCollect (location, ev);
}

int
main (int argc, char **argv) 
{
	CORBA_ORB orb;
	ConfigArchiver_Archive archive;
	ConfigArchiver_Location location = CORBA_OBJECT_NIL;
	CORBA_Environment ev;

	/* For Electric Fence */
	free (malloc (1));

        bindtextdomain (PACKAGE, GNOMELOCALEDIR);
        textdomain (PACKAGE);

	CORBA_exception_init (&ev);

	gnomelib_register_popt_table (global_options, 
				      _("Global archiver options"));
	gnomelib_register_popt_table (archiver_operations, 
				      _("Archiver commands"));
	gnomelib_register_popt_table (store_options, 
				      _("Options for storing data"));
	gnomelib_register_popt_table (rollback_options, 
				      _("Options for rolling back"));
	gnomelib_register_popt_table (add_rename_location_options, 
				      _("Options for adding or renaming " \
					"locations"));
	gnomelib_register_popt_table (add_remove_backend_options, 
				      _("Options for adding and removing " \
					"backends"));

	gtk_type_init ();
	gnomelib_init ("archiver", VERSION);
	gnomelib_parse_args (argc, argv, 0);

	orb = oaf_init (argc, argv);

	if (bonobo_init (orb, CORBA_OBJECT_NIL, CORBA_OBJECT_NIL) == FALSE)
		g_error ("Cannot initialize Bonobo");

	if (global)
		archive = bonobo_get_object ("archive:global-archive", "IDL:ConfigArchiver/Archive:1.0", &ev);
	else
		archive = bonobo_get_object ("archive:user-archive", "IDL:ConfigArchiver/Archive:1.0", &ev);

	if (archive == CORBA_OBJECT_NIL) {
		g_critical ("Could not open archive\n");
		return -1;
	}

	if (!add_location) {
		if (location_id == NULL)
			location_id = ConfigArchiver_Archive__get_currentLocationId (archive, &ev);

		location = ConfigArchiver_Archive_getLocation (archive, location_id, &ev);

		if (location == CORBA_OBJECT_NIL) {
			g_critical ("Error: Could not open location %s\n", location_id);
			return -1;
		}
	}

	if (store)
		do_store (location, &ev);
	else if (rollback)
		do_rollback (location, &ev);
	else if (change_location)
		do_change_location (archive, location, &ev);
	else if (rename_location)
		do_rename_location (archive, location, &ev);
	else if (add_location)
		do_add_location (archive, &ev);
	else if (remove_location)
		do_remove_location (location, &ev);
	else if (add_backend)
		do_add_backend (location, &ev);
	else if (remove_backend)
		do_remove_backend (location, &ev);
	else if (garbage_collect)
		do_garbage_collect (location, &ev);

	bonobo_object_release_unref (archive, NULL);

	if (location != CORBA_OBJECT_NIL)
		bonobo_object_release_unref (location, NULL);

	return 0;
}
