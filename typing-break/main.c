/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2002 CodeFactory AB
 * Copyright (C) 2002-2003 Richard Hult <richard@imendio.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <config.h>
#include <string.h>
#include <gdk/gdkx.h>
#include <gtk/gtk.h>
#include <libgnomeui/libgnomeui.h>
#include <libgnome/gnome-i18n.h>
#include "drw-selection.h"
#include "drwright.h"

gboolean debug = FALSE;

static gboolean
session_die (GnomeClient *client, gpointer client_data)
{
	gtk_main_quit ();

	return TRUE;
}

static gboolean
session_save (GnomeClient        *client,
	      gint                phase,
	      GnomeRestartStyle   rstyle,
	      gint                shutdown,
	      GnomeInteractStyle  istyle,
	      gint                fast,
	      gpointer            user_data)
{
	gchar *argv[2];
	
	/* Only with glib 2.2:
	 * argv[0] = g_get_application_name ();
	 */
	
	argv[0] = user_data;
	argv[1] = "-n";
	   
	gnome_client_set_clone_command (client, 2, argv);
        gnome_client_set_restart_command (client, 2, argv);

	return TRUE;
}

static gboolean
have_tray (void)
{
	Screen *xscreen = DefaultScreenOfDisplay (gdk_display);
	Atom    selection_atom;
	char   *selection_atom_name;
	
	selection_atom_name = g_strdup_printf ("_NET_SYSTEM_TRAY_S%d",
					       XScreenNumberOfScreen (xscreen));
	selection_atom = XInternAtom (DisplayOfScreen (xscreen), selection_atom_name, False);
	g_free (selection_atom_name);
	
	if (XGetSelectionOwner (DisplayOfScreen (xscreen), selection_atom)) {
		return TRUE;
	} else {
		return FALSE;
	}
}

int
main (int argc, char *argv[])
{
	gint          i;
	DrWright     *drwright;
	DrwSelection *selection;
	GnomeClient  *client;
	gboolean      no_check = FALSE;

	bindtextdomain (GETTEXT_PACKAGE, GNOMELOCALEDIR);  
        bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);
	
	i = 1;
	while (i < argc) {
		const gchar *arg = argv[i];
      
		if (strcmp (arg, "--debug") == 0 ||
		    strcmp (arg, "-d") == 0) {
			debug = TRUE;
		}
		else if (strcmp (arg, "-n") == 0) {
			no_check = TRUE;
		}
		else if (strcmp (arg, "-?") == 0) {
			g_printerr ("Usage: %s [--debug]\n", argv[0]);
			return 0;
		}
      
		++i;
	}

	gnome_program_init (PACKAGE, VERSION, LIBGNOMEUI_MODULE, 
			    argc, argv, NULL);

	selection = drw_selection_start ();
	if (!drw_selection_is_master (selection)) {
		GtkWidget *dialog;

		dialog = gtk_message_dialog_new (NULL, 0,
						 GTK_MESSAGE_INFO,
						 GTK_BUTTONS_CLOSE,
						 _("The typing monitor is already running."));

		gtk_dialog_run (GTK_DIALOG (dialog));

		return 0;
	}

	if (!no_check && !have_tray ()) {
		GtkWidget *dialog;

		dialog = gtk_message_dialog_new (NULL, 0,
						 GTK_MESSAGE_INFO,
						 GTK_BUTTONS_CLOSE,
						 _("The typing monitor uses the notification area to display "
						   "information. You don't seem to have a notification area "
						   "on your panel. You can add it by right-clicking on your "
						   "panel and choose 'Add to panel -> Utilities -> Notification area'."));

		gtk_dialog_run (GTK_DIALOG (dialog));

		gtk_widget_destroy (dialog);
	}
	
	client = gnome_master_client ();

	gnome_client_set_priority (client, 70);
	if (!debug) {
		gnome_client_set_restart_style (client, GNOME_RESTART_IMMEDIATELY);
	} else {
		/* Don't respawn in debug mode. */
		gnome_client_set_restart_style (client, GNOME_RESTART_IF_RUNNING);
	}
	
	g_signal_connect (client,
			  "save_yourself",
			  G_CALLBACK (session_save),
			  argv[0]);

	g_signal_connect (client,
			  "die",
			  G_CALLBACK (session_die),
			  NULL);

	drwright = drwright_new ();

	gtk_main ();

	return 0;
}
