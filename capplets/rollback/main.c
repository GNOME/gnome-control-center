/* -*- mode: c; style: linux -*- */

/* config-manager.c
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
#include <glade/glade.h>

#include "config-manager-dialog.h"

static gboolean is_global;

static struct poptOption rollback_options[] = {
	{"global", 'g', POPT_ARG_NONE, &is_global, 0,
	 N_("Operate on global backends")},
	{NULL, '\0', 0, NULL, 0}
};

int
main (int argc, char **argv) 
{
	GtkWidget *dialog;

        bindtextdomain (PACKAGE, GNOMELOCALEDIR);
        textdomain (PACKAGE);

	gnomelib_register_popt_table (rollback_options,
				      _("Options for the rollback GUI"));

	gnome_init ("config-manager", VERSION, argc, argv);
	glade_gnome_init ();

	dialog = config_manager_dialog_new
		(is_global ? CM_DIALOG_GLOBAL : CM_DIALOG_USER);
	gtk_widget_show (dialog);

	gtk_signal_connect (GTK_OBJECT (dialog), "destroy",
			    gtk_main_quit, NULL);

	gtk_main ();

	return 0;
}
