/* Copyright (C) 1999 Red Hat Software, Inc.
 * Copyright 2001 Ximian, Inc.
 *
 * This is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <locale.h>
#include <libintl.h>
#include <unistd.h>
#include <sys/types.h>
#include <gnome.h>
#include "root-manager-wrap.h"

void
userhelper_fatal_error(int signal)
{
	gtk_main_quit();
}

int
main(int argc, char* argv[])
{
	char *argv2[] = { UH_PATH, "0", NULL };
	char **argv_fake;
	int argc_fake = 2;

	int new_fd;

	bindtextdomain (GETTEXT_PACKAGE, GNOMELOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	argv_fake = g_new0 (char *, 3);
	argv_fake[0] = UH_PATH;
	argv_fake[1] = "0";

	if (!gtk_init_check(&argc_fake, &argv_fake)) {
		fprintf (stderr, _("Could not connect to X Display"));
		return -1;
	}

	new_fd = dup (STDIN_FILENO);
	if (new_fd < 0) {
		fprintf (stderr, _("Could not duplicate file descriptor"));
		return -1;
	}

	gnome_init ("root-manager-helper", VERSION, argc_fake, argv_fake);
	gdk_rgb_init ();

	signal(SIGCHLD, userhelper_fatal_error);

	userhelper_runv (UH_PATH, new_fd);

	gtk_main();

	return 0;
}
