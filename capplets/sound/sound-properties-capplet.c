/* -*- mode: c; style: linux -*- */

/* sound-properties-capplet.c
 * Copyright (C) 2001 Ximian, Inc.
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

#include "capplet-util.h"

#include <glade/glade.h>

/* Needed only for the sound capplet */

#include <stdlib.h>
#include <esd.h>
#include <sys/types.h>

/* Capplet-specific prototypes */

static void start_esd (void);

/* apply_settings
 *
 * Apply the settings of the property bag. This function is per-capplet, though
 * there are some cases where it does not do anything.
 */

static void
apply_settings (Bonobo_ConfigDatabase db) 
{
	gboolean enable_esd;

	enable_esd = bonobo_config_get_boolean (db, "/main/enable_esd", NULL);

        if (enable_esd && gnome_sound_connection < 0)
                start_esd ();

	if (!enable_esd)
		system ("killall esd");

	/* I'm not going to deal with reloading samples until later. It's
	 * entirely too painful */
}

/* start_esd
 *
 * Start the Enlightenment Sound Daemon. This function is specific to the sound
 * properties capplet.
 */

static void
start_esd (void) 
{
#ifdef HAVE_ESD
        int esdpid;
        static const char *esd_cmdline[] = {"esd", "-nobeeps", NULL};
        char *tmpargv[3];
        char argbuf[32];
        time_t starttime;
        GnomeClient *client = gnome_master_client ();

        esdpid = gnome_execute_async (NULL, 2, (char **)esd_cmdline);
        g_snprintf (argbuf, sizeof (argbuf), "%d", esdpid);
        tmpargv[0] = "kill"; tmpargv[1] = argbuf; tmpargv[2] = NULL;
        gnome_client_set_shutdown_command (client, 2, tmpargv);
        starttime = time (NULL);
        gnome_sound_init (NULL);

        while (gnome_sound_connection < 0
	       && ((time(NULL) - starttime) < 4)) 
        {
#ifdef HAVE_USLEEP
                usleep(1000);
#endif
                gnome_sound_init(NULL);
        }
#endif
}

/* create_dialog
 *
 * Create the dialog box and return it as a GtkWidget
 */

static GtkWidget *
create_dialog (void) 
{
	GladeXML *data;
	GtkWidget *widget;

	data = glade_xml_new (GNOMECC_GLADE_DIR "/sound-properties.glade", "prefs_widget");
	widget = glade_xml_get_widget (data, "prefs_widget");
	gtk_object_set_data (GTK_OBJECT (widget), "glade-data", data);

	gtk_signal_connect_object (GTK_OBJECT (widget), "destroy",
				   GTK_SIGNAL_FUNC (gtk_object_destroy),
				   GTK_OBJECT (data));

	return widget;
}

/* setup_dialog
 *
 * Set up the property editors for our dialog
 */

static void
setup_dialog (GtkWidget *widget, Bonobo_PropertyBag bag) 
{
	GladeXML *dialog;

	dialog = gtk_object_get_data (GTK_OBJECT (widget), "glade-data");

	CREATE_PEDITOR (boolean, "enable_esd", "enable_toggle");
	CREATE_PEDITOR (boolean, "event_sounds", "events_toggle");

	bonobo_peditor_set_guard (WID ("events_toggle"), bag, "enable_esd");
}

/* get_legacy_settings
 *
 * Retrieve older gnome_config -style settings and store them in the
 * configuration database.
 *
 * In most cases, it's best to use the COPY_FROM_LEGACY macro defined in
 * capplets/common/capplet-util.h.
 */

static void
get_legacy_settings (Bonobo_ConfigDatabase db) 
{
	gboolean val_boolean, def;

	COPY_FROM_LEGACY (boolean, "enable_esd", bool, "/sound/system/settings/start_esd=false");
	COPY_FROM_LEGACY (boolean, "event_sounds", bool, "/sound/system/settings/event_sounds=false");
}

int
main (int argc, char **argv) 
{
	glade_gnome_init ();
	capplet_init (argc, argv, apply_settings, create_dialog, setup_dialog, get_legacy_settings);

	return 0;
}
