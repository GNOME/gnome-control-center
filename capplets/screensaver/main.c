/* -*- mode: c; style: linux -*- */

/* main.c
 * Copyright (C) 2000 Helix Code, Inc.
 *
 * Written by Bradford Hovinen (hovinen@helixcode.com)
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

#include <gtk/gtk.h>
#include <gnome.h>
#include <libgnomeui/gnome-window-icon.h>
#include <tree.h>
#include <parser.h>
#include <xmlIO.h>
#include <errno.h>
#include <fcntl.h>

#include <glade/glade.h>

#include <capplet-widget.h>

#ifdef HAVE_XIMIAN_ARCHIVER
#  include <bonobo.h>
#  include <ximian-archiver/archiver-client.h>
#endif /* HAVE_XIMIAN_ARCHIVER */

#include "preferences.h"
#include "prefs-widget.h"
#include "preview.h"
#include "daemon.h"
#include "resources.h"

static Preferences *prefs;
static Preferences *old_prefs;
static PrefsWidget *prefs_widget;

static CappletWidget *capplet;

#ifdef HAVE_XIMIAN_ARCHIVER

static ConfigArchiver_Archive archive;
static gboolean outside_location;

static void
store_archive_data (void) 
{
	ConfigArchiver_Location location;
	xmlDocPtr xml_doc;
	CORBA_Environment ev;

	CORBA_exception_init (&ev);

	if (capplet_get_location () == NULL)
		location = ConfigArchiver_Archive__get_currentLocation (archive, &ev);
	else
		location = ConfigArchiver_Archive_getLocation
			(archive, capplet_get_location (), &ev);

	if (BONOBO_EX (&ev) || location == CORBA_OBJECT_NIL) {
		g_critical ("Could not open location %s", capplet_get_location ());
		return;
	}

	xml_doc = preferences_write_xml (prefs);
	location_client_store_xml (location, "screensaver-properties-capplet",
				   xml_doc, STORE_MASK_PREVIOUS, &ev);
	xmlFreeDoc (xml_doc);
	bonobo_object_release_unref (archive, NULL);
	bonobo_object_release_unref (location, NULL);

	CORBA_exception_free (&ev);
}

#endif /* HAVE_XIMIAN_ARCHIVER */

static void 
state_changed_cb (GtkWidget *widget) 
{
	if (!prefs->frozen)
		capplet_widget_state_changed (capplet, TRUE);
}

static void
try_cb (GtkWidget *widget)
{
	gboolean old_sm;

	old_sm = prefs->selection_mode;

#ifdef HAVE_XIMIAN_ARCHIVER
	if (!outside_location) {
		prefs_widget_store_prefs (prefs_widget, prefs);
		preferences_save (prefs);
		setup_dpms (prefs);
	}
#else /* !HAVE_XIMIAN_ARCHIVER */
	prefs_widget_store_prefs (prefs_widget, prefs);
	preferences_save (prefs);
	setup_dpms (prefs);
#endif /* HAVE_XIMIAN_ARCHIVER */

	if (old_sm == SM_DISABLE_SCREENSAVER && 
	    prefs->selection_mode != SM_DISABLE_SCREENSAVER)
		start_xscreensaver ();
	else if (old_sm != SM_DISABLE_SCREENSAVER && 
	    prefs->selection_mode == SM_DISABLE_SCREENSAVER)
		stop_xscreensaver ();
}

static void
revert_cb (GtkWidget *widget) 
{
	gboolean old_sm;

	old_sm = old_prefs->selection_mode;

#ifdef HAVE_XIMIAN_ARCHIVER
	if (!outside_location) {
		preferences_save (old_prefs);
		preferences_destroy (prefs);
	}
#else /* !HAVE_XIMIAN_ARCHIVER */
	preferences_save (old_prefs);
	preferences_destroy (prefs);
#endif /* HAVE_XIMIAN_ARCHIVER */

	prefs = preferences_new ();
	preferences_load (prefs);

	prefs->frozen++;
	prefs_widget_get_prefs (prefs_widget, prefs);
	prefs->frozen--;

	setup_dpms (old_prefs);

	if (old_sm == SM_DISABLE_SCREENSAVER && 
	    prefs->selection_mode != SM_DISABLE_SCREENSAVER)
		start_xscreensaver ();
	else if (old_sm != SM_DISABLE_SCREENSAVER && 
	    prefs->selection_mode == SM_DISABLE_SCREENSAVER)
		stop_xscreensaver ();
}

static void
ok_cb (GtkWidget *widget) 
{
	gboolean old_sm;

	old_sm = prefs->selection_mode;

	close_preview ();

#ifdef HAVE_XIMIAN_ARCHIVER
	if (!outside_location) {
#endif /* HAVE_XIMIAN_ARCHIVER */
		prefs_widget_store_prefs (prefs_widget, prefs);
		preferences_save (prefs);
		setup_dpms (prefs);
		if (old_sm == SM_DISABLE_SCREENSAVER && 
		    prefs->selection_mode != SM_DISABLE_SCREENSAVER)
			start_xscreensaver ();
		else if (old_sm != SM_DISABLE_SCREENSAVER && 
			 prefs->selection_mode == SM_DISABLE_SCREENSAVER)
			stop_xscreensaver ();
#ifdef HAVE_XIMIAN_ARCHIVER
	}

	store_archive_data ();
#endif /* HAVE_XIMIAN_ARCHIVER */
}

static void
cancel_cb (GtkWidget *widget) 
{
	gboolean old_sm;

	old_sm = old_prefs->selection_mode;

	close_preview ();

#ifdef HAVE_XIMIAN_ARCHIVER
	if (!outside_location) {
#endif /* HAVE_XIMIAN_ARCHIVER */
		preferences_save (old_prefs);
		setup_dpms (old_prefs);

		if (old_sm == SM_DISABLE_SCREENSAVER && 
		    prefs->selection_mode != SM_DISABLE_SCREENSAVER)
			start_xscreensaver ();
		else if (old_sm != SM_DISABLE_SCREENSAVER && 
			 prefs->selection_mode == SM_DISABLE_SCREENSAVER)
			stop_xscreensaver ();
#ifdef HAVE_XIMIAN_ARCHIVER
	}
#endif /* HAVE_XIMIAN_ARCHIVER */
}

static void
demo_cb (GtkWidget *widget)
{
	prefs->screensavers = prefs_widget->screensavers;
	preferences_save (prefs);
}

static void 
setup_capplet_widget (void)
{
	capplet = CAPPLET_WIDGET (capplet_widget_new ());

	gtk_signal_connect (GTK_OBJECT (capplet), "try", 
			    GTK_SIGNAL_FUNC (try_cb), NULL);
	gtk_signal_connect (GTK_OBJECT (capplet), "revert", 
			    GTK_SIGNAL_FUNC (revert_cb), NULL);
	gtk_signal_connect (GTK_OBJECT (capplet), "ok", 
			    GTK_SIGNAL_FUNC (ok_cb), NULL);
	gtk_signal_connect (GTK_OBJECT (capplet), "cancel", 
			    GTK_SIGNAL_FUNC (cancel_cb), NULL);		

	prefs->frozen++;

	prefs_widget = PREFS_WIDGET (prefs_widget_new (GTK_WINDOW (capplet->dialog)));

	gtk_container_add (GTK_CONTAINER (capplet), GTK_WIDGET (prefs_widget));

	set_preview_window (PREFS_WIDGET (prefs_widget)->preview_window);

	prefs_widget_get_prefs (prefs_widget, prefs);

	gtk_signal_connect (GTK_OBJECT (prefs_widget), "pref-changed",
			    GTK_SIGNAL_FUNC (state_changed_cb), NULL);
	gtk_signal_connect (GTK_OBJECT (prefs_widget), "activate-demo",
			    GTK_SIGNAL_FUNC (demo_cb), NULL);

	gtk_widget_show_all (GTK_WIDGET (capplet));

	prefs->frozen--;
}

#ifdef HAVE_XIMIAN_ARCHIVER

static void
do_get_xml (void) 
{
	Preferences *prefs;
	xmlDocPtr doc;

	prefs = preferences_new ();
	preferences_load (prefs);
	doc = preferences_write_xml (prefs);
	xmlDocDump (stdout, doc);
	preferences_destroy (prefs);
}

static void
do_set_xml (gboolean apply_settings) 
{
	xmlDocPtr doc;
	char buffer[16384];
	GString *doc_str;
	int t = 0;

	fflush (stdin);

	fcntl (fileno (stdin), F_SETFL, 0);

	doc_str = g_string_new ("");

	while ((t = read (fileno (stdin), buffer, sizeof (buffer) - 1)) != 0) {
		buffer[t] = '\0';
		g_string_append (doc_str, buffer);
	}

	if (doc_str->len > 0) {
		doc = xmlParseDoc (doc_str->str);
		g_string_free (doc_str, TRUE);

		if (doc != NULL) {
			prefs = preferences_read_xml (doc);

			if (prefs && apply_settings) {
				preferences_save (prefs);
				return;
			}
			else if (prefs != NULL) {
				return;
			}

			xmlFreeDoc (doc);
		}
	} else {
		g_critical ("No data to apply");
	}

	return;
}

#endif /* HAVE_XIMIAN_ARCHIVER */

static void
do_restore_from_defaults (void) 
{
	prefs = preferences_new ();
	preferences_save (prefs);
}

int
main (int argc, char **argv)
{
#ifdef HAVE_XIMIAN_ARCHIVER
	CORBA_ORB orb;
	CORBA_Environment ev;
	CORBA_char *current_location_id = NULL;
#endif /* HAVE_XIMIAN_ARCHIVER */

	GnomeClient *client;
	GnomeClientFlags flags;
	gint token, res;
	gchar *restart_args[3];

	bindtextdomain (PACKAGE, GNOMELOCALEDIR);
	textdomain (PACKAGE);

	res = gnome_capplet_init ("screensaver-properties",
				  VERSION, argc, argv, NULL,
				  0, NULL);

	if (res < 0) {
		g_error ("Could not initialize the capplet.");
	}
	else if (res == 3) {
#ifdef HAVE_XIMIAN_ARCHIVER
		do_get_xml ();
#endif /* HAVE_XIMIAN_ARCHIVER */
		return 0;
	}
	else if (res == 4) {
#ifdef HAVE_XIMIAN_ARCHIVER
		do_set_xml (TRUE);
#endif /* HAVE_XIMIAN_ARCHIVER */
		return 0;
	}
	else if (res == 5) {
		do_restore_from_defaults ();
		return 0;
	}

	glade_gnome_init ();

#ifdef HAVE_XIMIAN_ARCHIVER
	CORBA_exception_init (&ev);
	orb = oaf_init (argc, argv);
	if (!bonobo_init (orb, CORBA_OBJECT_NIL, CORBA_OBJECT_NIL))
		g_critical ("Could not initialize Bonobo");
#endif /* HAVE_XIMIAN_ARCHIVER */

	client = gnome_master_client ();
	flags = gnome_client_get_flags (client);

	if (flags & GNOME_CLIENT_IS_CONNECTED) {
		token = gnome_startup_acquire_token
			("GNOME_SCREENSAVER_PROPERTIES",
			 gnome_client_get_id (client));

		if (token) {
			gnome_client_set_priority (client, 20);
			gnome_client_set_restart_style (client,
							GNOME_RESTART_ANYWAY);
			restart_args[0] = argv[0];
			restart_args[1] = "--init-session-settings";
			restart_args[2] = NULL;
			gnome_client_set_restart_command (client, 2,
							  restart_args);
		} else {
			gnome_client_set_restart_style (client,
							GNOME_RESTART_NEVER);
		}
	} else {
		token = 1;
	}

	gnome_window_icon_set_default_from_file
		(GNOMECC_ICONS_DIR"/gnome-ccscreensaver.png");

	init_resource_database (argc, argv);
#ifdef HAVE_XIMIAN_ARCHIVER
	archive = bonobo_get_object ("archive:user-archive", "IDL:ConfigArchiver/Archive:1.0", &ev);

	if (BONOBO_EX (&ev) || archive == CORBA_OBJECT_NIL)
		g_critical ("Could not resolve archive moniker");
	else
		current_location_id = ConfigArchiver_Archive__get_currentLocationId (archive, &ev);

	if (capplet_get_location () != NULL &&
	    current_location_id != NULL &&
	    strcmp (capplet_get_location (), current_location_id))
	{
		outside_location = TRUE;
		do_set_xml (FALSE);
		if (prefs == NULL) return -1;
		prefs->frozen++;
	} else {
		outside_location = FALSE;
		prefs = preferences_new ();
		preferences_load (prefs);
	}

	if (current_location_id != NULL)
		CORBA_free (current_location_id);

	if (!outside_location && token) {
		if (prefs->selection_mode != SM_DISABLE_SCREENSAVER)
			start_xscreensaver ();
		setup_dpms (prefs);
	}

#else /* !HAVE_XIMIAN_ARCHIVER */

	prefs = preferences_new ();
	preferences_load (prefs);

	if (token) {
		if (prefs->selection_mode != SM_DISABLE_SCREENSAVER)
			start_xscreensaver ();
		setup_dpms (prefs);
	}

#endif /* HAVE_XIMIAN_ARCHIVER */

	if (!res) {
		old_prefs = preferences_clone (prefs);
		setup_capplet_widget ();

		capplet_gtk_main ();
	}

	return 0;
}
