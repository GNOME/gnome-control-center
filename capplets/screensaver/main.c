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

#include <glade/glade.h>

#include <capplet-widget.h>

#include <ximian-archiver/archive.h>
#include <ximian-archiver/location.h>

#include "preferences.h"
#include "prefs-widget.h"
#include "preview.h"
#include "daemon.h"
#include "resources.h"

static Preferences *prefs;
static Preferences *old_prefs;
static PrefsWidget *prefs_widget;

static CappletWidget *capplet;

static void
store_archive_data (void) 
{
	Archive *archive;
	Location *location;
	xmlDocPtr xml_doc;

	archive = ARCHIVE (archive_load (FALSE));
	location = archive_get_current_location (archive);
	xml_doc = preferences_write_xml (prefs);
	location_store_xml (location, "screensaver-properties-capplet",
			    xml_doc, STORE_MASK_PREVIOUS);
	xmlFreeDoc (xml_doc);
	archive_close (archive);
}

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

	prefs_widget_store_prefs (prefs_widget, prefs);
	preferences_save (prefs);
	setup_dpms (prefs);

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

	preferences_save (old_prefs);
	preferences_destroy (prefs);
	prefs = preferences_new ();
	preferences_load (prefs);

	prefs->frozen = TRUE;
	prefs_widget_get_prefs (prefs_widget, prefs);
	prefs->frozen = FALSE;

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

	prefs_widget_store_prefs (prefs_widget, prefs);
	preferences_save (prefs);
	setup_dpms (prefs);

	if (old_sm == SM_DISABLE_SCREENSAVER && 
	    prefs->selection_mode != SM_DISABLE_SCREENSAVER)
		start_xscreensaver ();
	else if (old_sm != SM_DISABLE_SCREENSAVER && 
	    prefs->selection_mode == SM_DISABLE_SCREENSAVER)
		stop_xscreensaver ();

	store_archive_data ();
}

static void
cancel_cb (GtkWidget *widget) 
{
	gboolean old_sm;

	old_sm = old_prefs->selection_mode;

	close_preview ();

	preferences_save (old_prefs);
	setup_dpms (old_prefs);

	if (old_sm == SM_DISABLE_SCREENSAVER && 
	    prefs->selection_mode != SM_DISABLE_SCREENSAVER)
		start_xscreensaver ();
	else if (old_sm != SM_DISABLE_SCREENSAVER && 
	    prefs->selection_mode == SM_DISABLE_SCREENSAVER)
		stop_xscreensaver ();
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

	prefs->frozen = TRUE;

	prefs_widget = PREFS_WIDGET (prefs_widget_new ());

	gtk_container_add (GTK_CONTAINER (capplet), GTK_WIDGET (prefs_widget));

	set_preview_window (PREFS_WIDGET (prefs_widget)->preview_window);

	prefs_widget_get_prefs (prefs_widget, prefs);

	gtk_signal_connect (GTK_OBJECT (prefs_widget), "pref-changed",
			    GTK_SIGNAL_FUNC (state_changed_cb), NULL);
	gtk_signal_connect (GTK_OBJECT (prefs_widget), "activate-demo",
			    GTK_SIGNAL_FUNC (demo_cb), NULL);

	gtk_widget_show_all (GTK_WIDGET (capplet));

	prefs->frozen = FALSE;
}

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
do_set_xml (void) 
{
	xmlDocPtr doc;
	Preferences *old_prefs, *new_prefs;
	char *buffer = NULL;
	int len = 0;

	while (!feof (stdin)) {
		if (!len) buffer = g_new (char, 16384);
		else buffer = g_renew (char, buffer, len + 16384);
		fread (buffer + len, 1, 16384, stdin);
		len += 16384;
	}

	doc = xmlParseMemory (buffer, strlen (buffer));

	old_prefs = preferences_new ();
	preferences_load (old_prefs);

	new_prefs = preferences_read_xml (doc);

	if (new_prefs) {
		new_prefs->config_db = old_prefs->config_db;
		preferences_save (new_prefs);
	} else {
		g_warning ("Error while reading the screensaver config file");
	}
}

int
main (int argc, char **argv)
{
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
		do_get_xml ();
		return 0;
	}
	else if (res == 4) {
		do_set_xml ();
		return 0;
	}

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
		(GNOME_ICONDIR"/gnome-ccscreensaver.png");

	init_resource_database (argc, argv);
	prefs = preferences_new (); preferences_load (prefs);

	if (token) {
		if (prefs->selection_mode != SM_DISABLE_SCREENSAVER)
			start_xscreensaver ();
		setup_dpms (prefs);
	}

	if (!res) {
		old_prefs = preferences_new (); preferences_load (old_prefs);
		setup_capplet_widget ();

		capplet_gtk_main ();
	}

	return 0;
}
