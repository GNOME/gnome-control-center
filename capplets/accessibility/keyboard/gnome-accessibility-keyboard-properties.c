/* -*- mode: c; style: linux -*- */

/* gnome-accessibility-keyboard-properties.c
 * Copyright (C) 2002 Ximian, Inc.
 *
 * Written by: Jody Goldberg <jody@gnome.org>
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

#include <gnome.h>
#include <gconf/gconf-client.h>

#include <capplet-util.h>
#include <activate-settings-daemon.h>
#include "accessibility-keyboard.h"

static void
dialog_response (GtkWidget *widget,
		 gint       response_id,
		 GConfChangeSet *changeset)
{
	gtk_main_quit ();
}

int
main (int argc, char **argv) 
{
	GtkWidget *dialog;
	GConfChangeSet *changeset;

	bindtextdomain (GETTEXT_PACKAGE, GNOMELOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	gnome_program_init (argv[0], VERSION, LIBGNOMEUI_MODULE, argc, argv,
			    GNOME_PARAM_APP_DATADIR, GNOMECC_DATA_DIR,
			    NULL);
	activate_settings_daemon ();

	changeset = NULL;
	dialog = setup_accessX_dialog (changeset, TRUE);
	g_signal_connect (G_OBJECT (dialog),
		"response",
		G_CALLBACK (dialog_response), changeset);
	gtk_widget_show_all (dialog);
	gtk_main ();

	return 0;
}
