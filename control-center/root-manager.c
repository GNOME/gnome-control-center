/* -*- mode: c; style: linux -*- */

/* root-manager.c
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

#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <glib.h>

#include <libgnome/libgnome.h>

int
main (int argc, char **argv) 
{
	char *buffer, *tmp;
	char **args;
	gint buf_size = 1024;
	pid_t pid;

	if (argc > 1) {
		execv (gnome_is_program_in_path (argv[1]), argv + 1);
		g_error ("%s", g_strerror (errno));
	}

	buffer = g_new (char, buf_size);

	while (!feof(stdin)) {
		buffer[0] = buffer[1] = 0;
		tmp = buffer;
		fgets (tmp, 1023, stdin);

		while (strlen (tmp) == 1023) {
			buf_size *= 2;
			buffer = g_renew (char, buffer, buf_size);
			tmp += 1023;
			fgets (tmp, 1023, stdin);
		}

		if (!strlen(buffer)) continue;

		pid = fork ();

		if (pid == (pid_t) -1) {
			g_error ("%s", g_strerror (errno));
		}
		else if (pid == 0) {
			buffer[strlen (buffer) - 1] = '\0';
			args = g_strsplit (buffer, " ", -1);
			printf ("Output: %s", args[0]);
			execv (gnome_is_program_in_path (args[0]), args);
			g_error ("%s", g_strerror (errno));
		}
	}

	return 0;
}
