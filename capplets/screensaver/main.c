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
#include <libxml/tree.h>
#include <libxml/parser.h>
#include <libxml/xmlIO.h>
#include <errno.h>
#include <fcntl.h>

#include <glade/glade.h>

#include "preferences.h"
#include "prefs-widget.h"
#include "preview.h"
#include "daemon.h"
#include "resources.h"

static Preferences *prefs;
static Preferences *old_prefs;
static PrefsWidget *prefs_widget;

static GtkWidget *capplet;

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
response_cb (GtkDialog *dialog, gint response_id, gpointer data)
{
	gboolean old_sm;

	switch (response_id)
	{
	case GTK_RESPONSE_NONE:
	case GTK_RESPONSE_CLOSE:
		close_preview ();
		gtk_main_quit ();
		break;
	case GTK_RESPONSE_APPLY:
		try_cb (GTK_WIDGET (dialog));
		break;
	case GTK_RESPONSE_HELP:
	//	help_all ();
		break;
	}
}

static void 
setup_capplet_widget (void)
{
	capplet = gtk_dialog_new_with_buttons (_("Default Applications"), NULL,
		  			 -1,
					 GTK_STOCK_HELP, GTK_RESPONSE_HELP,
					 GTK_STOCK_APPLY, GTK_RESPONSE_APPLY,
					 GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE,
					 NULL);
	g_signal_connect (G_OBJECT (capplet), "response",
			  G_CALLBACK (response_cb), NULL);
			
	prefs->frozen++;
	
	prefs_widget = PREFS_WIDGET (prefs_widget_new (GTK_WINDOW (capplet)));

	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (capplet)->vbox),
			    GTK_WIDGET (prefs_widget),
			    TRUE, TRUE, 0);

	set_preview_window (PREFS_WIDGET (prefs_widget)->preview_window);

	prefs_widget_get_prefs (prefs_widget, prefs);

	gtk_signal_connect (GTK_OBJECT (prefs_widget), "activate-demo",
			    GTK_SIGNAL_FUNC (demo_cb), NULL);

	gtk_widget_show_all (GTK_WIDGET (capplet));

	prefs->frozen--;
}

static void
do_restore_from_defaults (void) 
{
	prefs = preferences_new ();
	preferences_save (prefs);
}

int
main (int argc, char **argv)
{
	GnomeClient *client;
	GnomeClientFlags flags;
	gint token, res;
	gchar *restart_args[3];

	bindtextdomain (PACKAGE, GNOMELOCALEDIR);
	bind_textdomain_codeset (PACKAGE, "UTF-8");
	textdomain (PACKAGE);

	gnome_program_init ("screensaver-properties", VERSION,
			    LIBGNOMEUI_MODULE, argc, argv,
			    GNOME_PARAM_APP_DATADIR, GNOMECC_DATA_DIR,
			    NULL);

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
		(GNOMECC_DATA_DIR"/icons/gnome-ccscreensaver.png");

	init_resource_database (argc, argv);
	prefs = preferences_new ();
	preferences_load (prefs);

	if (token) {
		if (prefs->selection_mode != SM_DISABLE_SCREENSAVER)
			start_xscreensaver ();
		setup_dpms (prefs);
	}

	old_prefs = preferences_clone (prefs);
	setup_capplet_widget ();

	gtk_main ();

	return 0;
}
