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

int
main (int argc, char **argv) 
{
	CappletDirEntry *entry;
	CappletDir *dir;

        bindtextdomain (PACKAGE, GNOMELOCALEDIR);
	bind_textdomain_codeset (PACKAGE, "UTF-8");
        textdomain (PACKAGE);

	gnome_program_init ("control-center", VERSION, LIBGNOMEUI_MODULE,
			    argc, argv,
			    GNOME_PARAM_APP_DATADIR, GNOMECC_DATA_DIR,
			    NULL);

	gconf_init (argc, argv, NULL);

	gnomecc_init ();
	dir = get_root_capplet_dir ();
	if (dir == NULL)
		return -1;
	entry  = CAPPLET_DIR_ENTRY (dir);
	if (entry == NULL)
		return -1;
	capplet_dir_entry_activate (entry, NULL);

	gtk_main ();

	return 0;
}
