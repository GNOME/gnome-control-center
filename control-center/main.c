/* -*- mode: c; style: linux -*- */

/* main.c
 * Copyright (C) 2000, 2001 Ximian, Inc.
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
#include <glade/glade.h>
#include <bonobo.h>
#include <gconf/gconf.h>

#include "capplet-dir.h"
#include "capplet-dir-view.h"

static gint 
real_launch_control (gchar *capplet)
{
	GtkWidget *app;
	if ((app = capplet_control_launch (capplet, _("Configuration"))) == NULL)
	{
		gtk_main_quit ();
		return FALSE;
	}

	gtk_signal_connect (GTK_OBJECT (app), "destroy",
			    GTK_SIGNAL_FUNC (gtk_main_quit), NULL);

	return FALSE;
}

int
main (int argc, char **argv) 
{
	CORBA_ORB orb;

	CappletDirEntry *entry;
	CappletDir *dir;

	static gchar *capplet = NULL;
	static struct poptOption gnomecc_options[] = {
		{ "run-capplet", '\0', POPT_ARG_STRING, &capplet, 0,
		  N_("Run the capplet CAPPLET"), N_("CAPPLET") },
		{ NULL, '\0', 0, NULL, 0, NULL, NULL }
	};

        bindtextdomain (PACKAGE, GNOMELOCALEDIR);
        textdomain (PACKAGE);

	gnome_program_init ("control-center", VERSION, LIBGNOMEUI_MODULE,
			    argc, argv,
			    GNOME_PARAM_POPT_TABLE, gnomecc_options);

	gconf_init (argc, argv, NULL);

	if (capplet == NULL) {
		gnomecc_init ();
		dir = get_root_capplet_dir ();
		if (!dir)
			return -1;
		entry  = CAPPLET_DIR_ENTRY (dir);
		if (!entry)
			return -1;
		capplet_dir_entry_activate (entry, NULL);
	} else {
		gtk_idle_add ((GtkFunction) real_launch_control, capplet);
	}

	bonobo_main ();

	return 0;
}
