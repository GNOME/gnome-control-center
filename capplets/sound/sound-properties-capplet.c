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
#include "libsounds/sound-view.h"

#include <glade/glade.h>

/* Needed only for the sound capplet */

#include <stdlib.h>
#include <esd.h>
#include <sys/types.h>

/* Capplet-specific prototypes */

static SoundProperties *props = NULL;

static void start_esd (void);
static void reload_foreach_cb (SoundEvent *event, gpointer data);

/* apply_settings
 *
 * Apply the settings of the property bag. This function is per-capplet, though
 * there are some cases where it does not do anything.
 */

static void
apply_settings (Bonobo_ConfigDatabase db) 
{
	gboolean enable_esd;
	gboolean event_sounds;

	enable_esd = bonobo_config_get_boolean (db, "/main/enable_esd", NULL);
	event_sounds = bonobo_config_get_boolean (db, "/main/event_sounds", NULL);

        if (enable_esd && gnome_sound_connection < 0)
                start_esd ();

	if (!enable_esd)
		system ("killall esd");

	system ("gmix -i");

	/* gnome-libs checks this */
	gnome_config_set_bool ("/sound/system/settings/event_sounds", event_sounds);
	gnome_config_set_bool ("/sound/system/settings/enable_esd", enable_esd);
	gnome_config_sync ();

	/* Were we created from a dialog? */
	if (props)
	{
		sound_properties_user_save (props);
	}
	else
	{
		props = sound_properties_new ();
		sound_properties_add_defaults (props, NULL);
	}
	
	sound_properties_foreach (props, reload_foreach_cb, NULL);
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
 
/* reload_foreach_cb
 *
 * For a given SoundEvent, reload the sound file associate with the event. 
 */
static void
reload_foreach_cb (SoundEvent *event, gpointer data)
{
	gchar *file, *tmp, *key;
	int sid;
	
	key = sound_event_compose_key (event);
	/* We need to free up the old sample, because
	 * esd allows multiple samples with the same name,
	 * putting memory to waste. */
	sid = esd_sample_getid(gnome_sound_connection, key);
	if (sid >= 0)
		esd_sample_free(gnome_sound_connection, sid);

	if (!event->file || !strcmp (event->file, ""))
		return;
	
	file = g_strdup (event->file);
	if (file[0] != '/')
	{
		tmp = gnome_sound_file (file);
		g_free (file);
		file = tmp;
	}
	
	if (!file)
	{
		g_free (key);
		return;
	}

	sid = gnome_sound_sample_load (key, file);
	
	if (sid < 0)
		g_warning ("Couldn't load sound file %s as sample %s",
			   file, key);

	g_free (key);
}


/* create_dialog
 *
 * Create the dialog box and return it as a GtkWidget
 */

static GtkWidget *
create_dialog (void) 
{
	GladeXML *data;
	GtkWidget *widget, *box;

	data = glade_xml_new (GNOMECC_GLADE_DIR "/sound-properties.glade", "prefs_widget");
	widget = glade_xml_get_widget (data, "prefs_widget");
	gtk_object_set_data (GTK_OBJECT (widget), "glade-data", data);

	gtk_signal_connect_object (GTK_OBJECT (widget), "destroy",
				   GTK_SIGNAL_FUNC (gtk_object_destroy),
				   GTK_OBJECT (data));

	props = sound_properties_new ();
	sound_properties_add_defaults (props, NULL);
	box = glade_xml_get_widget (data, "events_vbox");
	gtk_box_pack_start (GTK_BOX (box), sound_view_new (props),
			    TRUE, TRUE, 0);

	gtk_signal_connect_object (GTK_OBJECT (widget), "destroy",
				   GTK_SIGNAL_FUNC (gtk_object_destroy),
				   GTK_OBJECT (props));

	gtk_widget_set_usize (widget, -1, 250);

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
	bonobo_peditor_set_guard (WID ("events_vbox"), bag, "enable_esd");
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

	COPY_FROM_LEGACY (boolean, "/main/enable_esd", bool, "/sound/system/settings/start_esd=false");
	COPY_FROM_LEGACY (boolean, "/main/event_sounds", bool, "/sound/system/settings/event_sounds=false");
}

int
main (int argc, char **argv) 
{
	const gchar* legacy_files[] = { "sound/system", "sound/events", NULL };
	
	glade_gnome_init ();
	
	capplet_init (argc, argv, legacy_files, apply_settings, create_dialog, setup_dialog, get_legacy_settings);

	return 0;
}
