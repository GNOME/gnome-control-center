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

#include <glade/glade.h>

#include <capplet-widget.h>

#include <ximian-archiver/archive.h>
#include <ximian-archiver/location.h>

#include "preferences.h"
#include "prefs-widget.h"

static Preferences *prefs;
static Preferences *old_prefs;
static PrefsWidget *prefs_widget;

static guint ok_handler_id;
static guint cancel_handler_id;

static void
store_archive_data (void) 
{
	Archive *archive;
	Location *location;
	xmlDocPtr xml_doc;

	archive = ARCHIVE (archive_load (FALSE));
	location = archive_get_current_location (archive);
	xml_doc = preferences_write_xml (prefs);
	location_store_xml (location, "background-properties-capplet",
			    xml_doc, STORE_MASK_PREVIOUS);
	xmlFreeDoc (xml_doc);
	archive_close (archive);
}

static void
ok_cb (GtkWidget *widget) 
{
	preferences_save (prefs);
	preferences_apply_now (prefs);
	gtk_signal_disconnect (GTK_OBJECT (prefs_widget), ok_handler_id);
	gtk_signal_disconnect (GTK_OBJECT (prefs_widget), cancel_handler_id);
	gtk_object_destroy (GTK_OBJECT (prefs_widget));
	store_archive_data ();
}

static void
cancel_cb (GtkWidget *widget) 
{
	preferences_save (old_prefs);
	preferences_apply_now (old_prefs);
	gtk_signal_disconnect (GTK_OBJECT (prefs_widget), ok_handler_id);
	gtk_signal_disconnect (GTK_OBJECT (prefs_widget), cancel_handler_id);
	gtk_object_destroy (GTK_OBJECT (prefs_widget));
}

static void 
setup_capplet_widget (void)
{
	preferences_freeze (prefs);

	prefs_widget = PREFS_WIDGET (prefs_widget_new (prefs));

	ok_handler_id =
		gtk_signal_connect (GTK_OBJECT (prefs_widget), "ok", 
				    GTK_SIGNAL_FUNC (ok_cb), NULL);
	cancel_handler_id =
		gtk_signal_connect (GTK_OBJECT (prefs_widget), "cancel", 
				    GTK_SIGNAL_FUNC (cancel_cb), NULL);		

	gtk_widget_show_all (GTK_WIDGET (prefs_widget));

	preferences_thaw (prefs);
}

static void
do_get_xml (void) 
{
	Preferences *prefs;
	xmlDocPtr doc;

	prefs = PREFERENCES (preferences_new ());
	preferences_load (prefs);
	doc = preferences_write_xml (prefs);
	xmlDocDump (stdout, doc);
	gtk_object_destroy (GTK_OBJECT (prefs));
}

static void
do_set_xml (void) 
{
	Preferences *prefs;
	xmlDocPtr doc;
	char *buffer = NULL;
	int len = 0;

	while (!feof (stdin)) {
		if (!len) buffer = g_new (char, 16384);
		else buffer = g_renew (char, buffer, len + 16384);
		fread (buffer + len, 1, 16384, stdin);
		len += 16384;
	}

	doc = xmlParseMemory (buffer, strlen (buffer));

	prefs = preferences_read_xml (doc);

	if (prefs) {
		preferences_save (prefs);
		preferences_apply_now (prefs);
	} else {
		g_warning ("Error while reading the screensaver config file");
	}
}

static void
do_restore_from_defaults (void) 
{
	prefs = PREFERENCES (preferences_new ());
	preferences_save (prefs);
	preferences_apply_now (prefs);
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

	glade_gnome_init ();
	res = gnome_capplet_init ("background-properties-capplet",
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
	else if (res == 5) {
		do_restore_from_defaults ();
		return 0;
	}

	client = gnome_master_client ();
	flags = gnome_client_get_flags (client);

	if (flags & GNOME_CLIENT_IS_CONNECTED) {
		token = gnome_startup_acquire_token
			("GNOME_BACKGROUND_PROPERTIES",
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
		(GNOME_ICONDIR"/gnome-ccbackground.png");

	prefs = PREFERENCES (preferences_new ());
	preferences_load (prefs);

	if (token) {
		preferences_apply_now (prefs);
	}

	if (!res) {
		old_prefs = PREFERENCES (preferences_clone (prefs));
		setup_capplet_widget ();

		capplet_gtk_main ();
	}

	return 0;
}
