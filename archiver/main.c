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

#include "util.h"
#include "archive.h"

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

static void
do_store (Location *location) 
{
	StoreType type;

	if (!backend_id) {
		fprintf (stderr, "No backend specified\n");
		return;
	}

	if (mask_previous)
		type = STORE_MASK_PREVIOUS;
	else if (compare_parent)
		type = STORE_COMPARE_PARENT;
	else
		type = STORE_FULL;

	location_store (location, backend_id, stdin, type);
}

static void
do_rollback (Location *location) 
{
	gint id;
	struct tm *date = NULL;

	if (date_str)
		date = parse_date (date_str);
	else if (last || steps > 0)
		date = NULL;
	else if (!revision_id) {
		fprintf (stderr, "No date specified\n");
		return;
	}

	if (all) {
		location_rollback_all_to (location, date, TRUE);
	}
	else if (backend_id && (date || last)) {
		/* FIXME: Need to support specifying multiple backends */
		if (show)
			location_dump_rollback_data (location, date, 0,
						     backend_id, TRUE, stdout);
		else
			location_rollback_backend_to (location, date, 
						      backend_id, TRUE);
	}
	else if (backend_id && steps) {
		if (show)
			location_dump_rollback_data (location, NULL, steps,
						     backend_id, TRUE, stdout);
		else
			location_rollback_backend_by (location, steps, 
						      backend_id, TRUE);
	}
	else if (revision_id) {
		sscanf (revision_id, "%x", &id);
		if (id >= 0)
			location_rollback_id (location, id);
		else
			fprintf (stderr, "Bad id specified\n");
	} else {
		g_message ("No backend specified\n");
		return;
	}
}

static void
do_change_location (Archive *archive, Location *location) 
{
	archive_set_current_location (archive, location);
}

static void
do_rename_location (Archive *archive, Location *location) 
{
	gboolean is_current;

	if (new_name == NULL) {
		fprintf (stderr,
			 "You did not specify a new name. Try --help\n");
	} else {
		if (!strcmp (location_get_id (location),
			     archive_get_current_location_id (archive)))
			is_current = TRUE;
		else
			is_current = FALSE;

		location_set_id (location, new_name);

		if (is_current) 
			archive_set_current_location_id (archive, new_name);
	}
}

static void
do_add_location (Archive *archive) 
{
	GtkObject *location;
	Location *parent_location = NULL;

	if (location_id == NULL) {
		fprintf (stderr,
			 "Error: You did not specify a location name\n");
		return;
	}

	if (parent_str != NULL) {
		parent_location = archive_get_location (archive, parent_str);

		if (parent_location == NULL && !strcmp (parent_str, "default"))
			parent_location =
				LOCATION
				(location_new (archive, "default", NULL));
	}

	location = location_new (archive, location_id, parent_location);
}

static void
do_remove_location (Location *location)
{
	location_delete (location);
}

static void
do_add_backend (Location *location) 
{
	ContainmentType type;

	if (contain_full && contain_partial) {
		fprintf (stderr, "Error: Cannot have both full and partial "
			 "containment\n");
		return;
	}
	else if (contain_partial) {
		type = CONTAIN_PARTIAL;
	} else {
		type = CONTAIN_FULL;
	}

	location_add_backend (location, backend_id, type);
}

static void
do_remove_backend (Location *location) 
{
	location_remove_backend (location, backend_id);
}

static void
do_garbage_collect (Location *location) 
{
	location_garbage_collect (location);
}

int
main (int argc, char **argv) 
{
	Archive *archive;
	Location *location = NULL;

	/* For Electric Fence */
	free (malloc (1));

        bindtextdomain (PACKAGE, GNOMELOCALEDIR);
	bind_textdomain_codeset (PACKAGE, "UTF-8");
        textdomain (PACKAGE);

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

	archive = ARCHIVE (archive_load (global));

	if (archive == NULL) {
		fprintf (stderr, "Could not open archive\n");
		return -1;
	}

	if (!add_location) {
		if (location_id == NULL)
			location_id =
				archive_get_current_location_id (archive);

		location = archive_get_location (archive, location_id);

		if (location == NULL) {
			fprintf (stderr, "Error: Could not open location %s\n",
				 location_id);
			return -1;
		}
	}

	if (store)
		do_store (location);
	else if (rollback)
		do_rollback (location);
	else if (change_location)
		do_change_location (archive, location);
	else if (rename_location)
		do_rename_location (archive, location);
	else if (add_location)
		do_add_location (archive);
	else if (remove_location)
		do_remove_location (location);
	else if (add_backend)
		do_add_backend (location);
	else if (remove_backend)
		do_remove_backend (location);
	else if (garbage_collect)
		do_garbage_collect (location);

	archive_close (archive);

	return 0;
}
