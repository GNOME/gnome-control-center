/* -*- mode: c; style: linux -*- */

/* resources.c
 * Copyright (C) 2000 Helix Code, Inc.
 *
 * Written by Bradford Hovinen <hovinen@helixcode.com>
 * Parts written by Jamie Zawinski <jwz@jwz.org>
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
# include "config.h"
#endif

#include <ctype.h>

#include <X11/Intrinsic.h>
#include <X11/Shell.h>
#include <gdk/gdkx.h>

#include "resources.h"
#include "rc-parse.h"

static char *defaults[] = {
#include "XScreenSaver_ad.h"
 0
};

static XrmDatabase db;

void
init_resource_database (int argc, char **argv) 
{
	XtAppContext app;
	Widget toplevel_shell;
	Display *dpy;

	/* From xscreensaver 3.24 display/demo-Gtk.c line 1908 ... */

	XtToolkitInitialize ();
	app = XtCreateApplicationContext ();
	dpy = gdk_display;
	XtAppSetFallbackResources (app, defaults);
	XtDisplayInitialize (app, dpy, "xscreensaver", "XScreenSaver",
			     0, 0, &argc, argv);
	toplevel_shell = XtAppCreateShell ("xscreensaver", "XScreenSaver",
					   applicationShellWidgetClass,
					   dpy, 0, 0);

	dpy = XtDisplay (toplevel_shell);
	db = XtDatabase (dpy);
}

/* From xscreensaver 3.24 utils/resource.c line 34 ... */

static char *
get_resource (char *res_name, char *res_class)
{
	XrmValue value;
	char *type, *value_str = NULL, *full_name, *full_class;

	full_name = g_strconcat ("xscreensaver.", res_name, NULL);
	full_class = g_strconcat ("XScreenSaver.", res_class, NULL);

	if (XrmGetResource (db, full_name, full_class, &type, &value))
		value_str = g_strndup (value.addr, value.size);

	g_free (full_name);
	g_free (full_class);

	return value_str;
}

static gchar *rc_names[][2] = {
	{ "verbose",           "Boolean" },
	{ "lock",              "Boolean" },
	{ "lockVTs",           "Boolean" },
	{ "fade",              "Boolean" },
	{ "unfade",            "Boolean" },
	{ "fadeSeconds",       "Time" },
	{ "fadeTicks",         "Integer" },
	{ "installColormap",   "Boolean" },
	{ "nice",              "Nice" },
	{ "timeout",           "Time" },
	{ "lockTimeout",       "Time" },
	{ "cycle",             "Time" },
	{ "passwdTimeout",     "Time" },
	{ "xidleExtension",    "Boolean" },
	{ "mitSaverExtension", "Boolean" },
	{ "sgiSaverExtension", "Boolean" },
	{ "procInterrupts",    "Boolean" },
	{ "bourneShell",       "BourneShell" },
	{ "programs",          "Programs" },
	{ NULL, NULL }
};

void
preferences_load_from_xrdb (Preferences *prefs) 
{
	int i;

	prefs->config_db = g_tree_new ((GCompareFunc) strcmp);

	for (i = 0; rc_names[i][0]; i++)
		g_tree_insert (prefs->config_db, rc_names[i][0],
			       get_resource (rc_names[i][0], rc_names[i][1]));
}

void
screensaver_get_desc_from_xrdb (Screensaver *saver) 
{
	gchar *s, *desc;
	int i, j;
	gboolean flag;

	s = g_strconcat ("hacks.", saver->name, ".documentation", NULL);
	desc = get_resource (s, s);
	g_free (s);

	saver->description = g_new (char, strlen (desc) + 1);
	flag = FALSE;
	for (i = 0, j = 0; desc[i]; i++) {
		if (!isspace(desc[i])) {
			saver->description[j++] = desc[i];
			flag = FALSE;
		} else if (!flag) {
			saver->description[j++] = ' ';
			flag = TRUE;
		}
	}

	saver->description[j] = '\0';

	g_free (desc);
}

gchar *
screensaver_get_label_from_xrdb (gchar *name) 
{
	gchar *s;

	s = g_strdup_printf ("hacks.%s.name", name);
	return get_resource (s, s);
}
