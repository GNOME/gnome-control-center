/* -*- mode: c; style: linux -*- */

/* main.c
 * Copyright (C) 2000-2001 Ximian, Inc.
 *
 * Written by Bradford Hovinen <hovinen@ximian.com>
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
#include <bonobo.h>
#include <glade/glade.h>

#include "rollback-capplet-dialog.h"

static gboolean is_global;
static gchar *capplet_name;

static struct poptOption rollback_options[] = {
	{"capplet", 'c', POPT_ARG_STRING, &capplet_name, 0,
	 N_("Rollback the capplet given")},
	{"global", 'g', POPT_ARG_NONE, &is_global, 0,
	 N_("Operate on global backends")},
	{NULL, '\0', 0, NULL, 0}
};

int
main (int argc, char **argv) 
{
	CORBA_ORB orb;
	GtkObject *dialog;

        bindtextdomain (PACKAGE, GNOMELOCALEDIR);
        textdomain (PACKAGE);

	gnomelib_register_popt_table (rollback_options,
				      _("Options for the rollback GUI"));

	gnome_init ("config-manager", VERSION, argc, argv);
	glade_gnome_init ();

	orb = oaf_init (argc, argv);
	if (bonobo_init (orb, CORBA_OBJECT_NIL, CORBA_OBJECT_NIL) == FALSE)
		g_error ("Cannot initialize bonobo");

	if (capplet_name != NULL) {
		dialog = rollback_capplet_dialog_new (capplet_name);

		if (dialog == NULL) {
			g_critical ("Could not create rollback dialog");
			return -1;
		} else {
			gtk_widget_show (GTK_WIDGET (dialog));
			gtk_signal_connect (dialog, "destroy", gtk_main_quit, NULL);
		}
	}

	bonobo_main ();

	return 0;
}
