/* -*- mode: c; style: linux -*- */

/* main.c
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

#include <gtk/gtk.h>
#include <gnome.h>
#include <libgnomeui/gnome-window-icon.h>
#include <fcntl.h> 

#include <glade/glade.h>

#include "preferences.h"
#include "prefs-widget.h"
#include "prefs-widget-app.h"
#include "prefs-widget-dialogs.h"
#include "prefs-widget-mdi.h"

static Preferences *prefs;
static PrefsWidget *prefs_widget;

static gint cap_session_init = 0;
static struct poptOption cap_options[] = {
	{"init-session-settings", '\0', POPT_ARG_NONE, &cap_session_init, 0,
	 N_("Initialize session settings"), NULL},
	{NULL, '\0', 0, NULL, 0}
};

static void 
setup_capplet_widget (void)
{
	preferences_freeze (prefs);

	prefs_widget = PREFS_WIDGET (prefs_widget_new (prefs));

	gtk_widget_show (GTK_WIDGET (prefs_widget));

	preferences_thaw (prefs);
}

static void
do_restore_from_defaults (void) 
{
	prefs = PREFERENCES (preferences_new ());
	preferences_save (prefs);
	preferences_apply_now (prefs);
}

int
main (int argc, char **argv)
{
	gchar *restart_args[3];

	bindtextdomain (PACKAGE, GNOMELOCALEDIR);
	bind_textdomain_codeset (PACKAGE, "UTF-8");
	textdomain (PACKAGE);

  	gnome_program_init ("ui-properties-capplet", VERSION,
		      LIBGNOMEUI_MODULE, argc, argv,
		      GNOME_PARAM_POPT_TABLE, &cap_options,
		      NULL);

	gnome_window_icon_set_default_from_file (GNOMECC_DATA_DIR"/icons/gnome-applications.png");

	prefs = PREFERENCES (preferences_new ());
	preferences_load (prefs);

	if (!cap_session_init) {
		setup_capplet_widget ();

		gtk_main ();
	}

	return 0;
}
