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

int
main (int argc, char **argv) 
{
	GtkWidget *dialog;

        bindtextdomain (GETTEXT_PACKAGE, GNOMELOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
        textdomain (GETTEXT_PACKAGE);

	gnome_init ("config-manager", VERSION, argc, argv);
	glade_gnome_init ();

	dialog = config_manager_dialog_new (CM_DIALOG_USER_ONLY);
	gtk_widget_show (dialog);

	gtk_signal_connect (GTK_OBJECT (dialog), "destroy",
			    gtk_main_quit, NULL);

	gtk_main ();

	return 0;
}
