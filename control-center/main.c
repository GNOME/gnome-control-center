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

#include <string.h>
#include <gnome.h>
#include <glade/glade.h>
#include <gconf/gconf.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <gdk/gdkx.h>

#include "capplet-dir.h"
#include "capplet-dir-view.h"

static gint use_shell = 0;

static struct poptOption cap_options[] = {
	{"use-shell", '\0', POPT_ARG_NONE, &use_shell, 0,
	 N_("Use shell even if nautilus is running."), NULL},
	{NULL, '\0', 0, NULL, 0}
};

static gboolean
is_nautilus_running (void)
{
	Atom window_id_atom;
	Window nautilus_xid;
	Atom actual_type;
	int actual_format;
	unsigned long nitems, bytes_after;
	unsigned char *data;
	int retval;
	Atom wmclass_atom;
	gboolean running;
	gint error;

	window_id_atom = XInternAtom (GDK_DISPLAY (), 
				      "NAUTILUS_DESKTOP_WINDOW_ID", True);

	if (window_id_atom == None) return FALSE;

	retval = XGetWindowProperty (GDK_DISPLAY (), GDK_ROOT_WINDOW (),
				     window_id_atom, 0, 1, False, XA_WINDOW,
				     &actual_type, &actual_format, &nitems,
				     &bytes_after, &data);

	if (data != NULL) {
		nautilus_xid = *(Window *) data;
		XFree (data);
	} else {
		return FALSE;
	}

	if (actual_type != XA_WINDOW) return FALSE;
	if (actual_format != 32) return FALSE;

	wmclass_atom = XInternAtom (GDK_DISPLAY (), "WM_CLASS", False);

	gdk_error_trap_push ();

	retval = XGetWindowProperty (GDK_DISPLAY (), nautilus_xid,
				     wmclass_atom, 0, 24, False, XA_STRING,
				     &actual_type, &actual_format, &nitems,
				     &bytes_after, &data);

	error = gdk_error_trap_pop ();

	if (error == BadWindow) return FALSE;

	if (actual_type == XA_STRING &&
	    nitems == 24 &&
	    bytes_after == 0 &&
	    actual_format == 8 &&
	    data != NULL &&
	    !strcmp (data, "desktop_window") &&
	    !strcmp (data + strlen (data) + 1, "Nautilus"))
		running = TRUE;
	else
		running = FALSE;

	if (data != NULL)
		XFree (data);

	return running;
}

static gboolean
gnome_cc_save_yourself (GnomeClient *client, gint phase, GnomeSaveStyle save_style,
			gboolean shutdown, GnomeInteractStyle interact_style,
			gboolean fast, gchar *argv0)
{
	gchar *argv[3];
	gint argc;

	argv[0] = argv0;
	argv[1] = "--use-shell";
	argc = 2;
	gnome_client_set_clone_command (client, argc, argv);
	gnome_client_set_restart_command (client, argc, argv);

	return TRUE;
}

static void
gnome_cc_die (GnomeClient *client, gpointer data)
{
	gtk_main_quit ();
}

int
main (int argc, char **argv) 
{
	CappletDirEntry *entry;
	CappletDir *dir;
	GnomeClient *client;

        bindtextdomain (GETTEXT_PACKAGE, GNOMELOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
        textdomain (GETTEXT_PACKAGE);

	gnome_program_init ("control-center", VERSION, LIBGNOMEUI_MODULE,
			    argc, argv,
			    GNOME_PARAM_APP_DATADIR, GNOMECC_DATA_DIR,
			    GNOME_PARAM_POPT_TABLE, cap_options,
			    NULL);

	if (!use_shell && is_nautilus_running ())
		execlp ("nautilus", "nautilus", "preferences:///", NULL);
	
	gnomecc_init ();
	dir = get_root_capplet_dir ();
	if (dir == NULL)
		return -1;
	entry  = CAPPLET_DIR_ENTRY (dir);
	if (entry == NULL)
		return -1;
	capplet_dir_entry_activate (entry, NULL);

	client = gnome_master_client ();
	g_signal_connect (G_OBJECT (client), "save_yourself",
			  G_CALLBACK (gnome_cc_save_yourself), argv[0]);
	g_signal_connect (G_OBJECT (client), "die",
			  G_CALLBACK (gnome_cc_die), NULL);

	gtk_main ();

	return 0;
}
