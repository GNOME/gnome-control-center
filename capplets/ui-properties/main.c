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
#include <fcntl.h> 

#include <glade/glade.h>

#include "preferences.h"
#include "prefs-widget.h"
#include "prefs-widget-app.h"
#include "prefs-widget-dialogs.h"
#include "prefs-widget-mdi.h"

static Preferences *prefs;
static PrefsWidget *prefs_widget;

static gint cap_session_init = 0;
static struct poptOption cap_options[] = {
	{"init-session-settings", '\0', POPT_ARG_NONE, &cap_session_init, 0,
	 N_("Initialize session settings"), NULL},
	{NULL, '\0', 0, NULL, 0}
};

static void 
setup_capplet_widget (void)
{
	preferences_freeze (prefs);

	prefs_widget = PREFS_WIDGET (prefs_widget_new (prefs));

	gtk_widget_show (GTK_WIDGET (prefs_widget));

	preferences_thaw (prefs);
}

#ifdef HAVE_XIMIAN_ARCHIVER

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
	prefs = PREFERENCES (preferences_new ());
	preferences_save (prefs);
	preferences_apply_now (prefs);
}

int
main (int argc, char **argv)
{
	gchar *restart_args[3];

	bindtextdomain (PACKAGE, GNOMELOCALEDIR);
	bind_textdomain_codeset (PACKAGE, "UTF-8");
	textdomain (PACKAGE);

  	gnome_program_init ("ui-properties-capplet", VERSION,
		      LIBGNOMEUI_MODULE, argc, argv,
		      GNOME_PARAM_POPT_TABLE, &cap_options,
		      NULL);

	gnome_window_icon_set_default_from_file (GNOMECC_ICONS_DIR"/gnome-applications.png");

#ifdef HAVE_XIMIAN_ARCHIVER
	archive = ARCHIVE (archive_load (FALSE));

	if (capplet_get_location () != NULL &&
	    strcmp (capplet_get_location (),
		    archive_get_current_location_id (archive)))
	{
		outside_location = TRUE;
		do_set_xml (FALSE);
		if (prefs == NULL) return -1;
		preferences_freeze (prefs);
	} else {
		outside_location = FALSE;
		prefs = PREFERENCES (preferences_new ());
		preferences_load (prefs);
	}

	if (!outside_location && token) {
		preferences_apply_now (prefs);
	}

#else /* !HAVE_XIMIAN_ARCHIVER */

	prefs = PREFERENCES (preferences_new ());
	preferences_load (prefs);

#endif /* HAVE_XIMIAN_ARCHIVER */

	if (!cap_session_init) {
		setup_capplet_widget ();

		gtk_main ();
	}

	return 0;
}
