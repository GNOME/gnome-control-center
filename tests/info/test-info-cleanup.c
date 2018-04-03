/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*-
 *
 * Copyright (C) 2010 Red Hat, Inc
 * Copyright (C) 2008 William Jon McCann <jmccann@redhat.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 */

#include <config.h>

#include <glib.h>
#include <locale.h>
#include "info-cleanup.h"

static void
test_info (void)
{
	g_autofree gchar *contents = NULL;
	guint i;
	g_auto(GStrv) lines = NULL;

	if (g_file_get_contents (TEST_SRCDIR "/info-cleanup-test.txt", &contents, NULL, NULL) == FALSE) {
		g_warning ("Failed to load '%s'", TEST_SRCDIR "/info-cleanup-test.txt");
		g_test_fail ();
		return;
	}

	lines = g_strsplit (contents, "\n", -1);
	if (lines == NULL) {
		g_warning ("Test file is empty");
		g_test_fail ();
		return;
	}

	for (i = 0; lines[i] != NULL; i++) {
		g_auto(GStrv) items = NULL;
		g_autofree gchar *utf8 = NULL;
		g_autofree gchar *result = NULL;

		if (*lines[i] == '#')
			continue;
		if (*lines[i] == '\0')
			break;

		items = g_strsplit (lines[i], "\t", -1);
		utf8 = g_locale_from_utf8 (items[0], -1, NULL, NULL, NULL);
		result = info_cleanup (items[0]);
		if (g_strcmp0 (result, items[1]) != 0) {
			g_error ("Result for '%s' doesn't match '%s' (got: '%s')",
				 utf8, items[1], result);
			g_test_fail ();
		} else {
			g_debug ("Result for '%s' matches '%s'",
				 utf8, result);
		}
	}
}

int main (int argc, char **argv)
{
	setlocale (LC_ALL, "");
	g_test_init (&argc, &argv, NULL);

	g_setenv ("G_DEBUG", "fatal_warnings", FALSE);

	g_test_add_func ("/info/info", test_info);

	return g_test_run ();
}
