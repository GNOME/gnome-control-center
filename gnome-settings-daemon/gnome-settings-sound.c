/* -*- mode: c; style: linux -*- */

/* gnome-settings-sound.c
 *
 * Copyright © 2001 Ximian, Inc.
 *
 * Written by Rachel Hestilow <hestilow@ximian.com>
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

#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <esd.h>
#include <sys/types.h>

#include <gconf/gconf-client.h>
#include <libgnome/gnome-i18n.h>
#include <libgnome/gnome-sound.h>
#include <libgnome/gnome-util.h>
#include <libgnome/gnome-exec.h>
#include <libgnomeui/gnome-client.h>
#include "libsounds/sound-properties.h"
#include "gnome-settings-sound.h"
#include "gnome-settings-daemon.h"

/* start_esd
 *
 * Start the Enlightenment Sound Daemon. 
 */
static void
start_esd (void) 
{
        int esdpid;
        static const char *esd_cmdline[] = {"esd", "-nobeeps", NULL};
        char *tmpargv[3];
        char argbuf[32];
        time_t starttime;
        GnomeClient *client = gnome_master_client ();

	g_print (_("Starting esd\n"));
        esdpid = gnome_execute_async (NULL, 2, (char **)esd_cmdline);
        g_snprintf (argbuf, sizeof (argbuf), "%d", esdpid);
        tmpargv[0] = "kill"; tmpargv[1] = argbuf; tmpargv[2] = NULL;
        gnome_client_set_shutdown_command (client, 2, tmpargv);
        starttime = time (NULL);
        gnome_sound_init (NULL);

        while (gnome_sound_connection_get () < 0
	       && ((time(NULL) - starttime) < 4)) 
        {
#ifdef HAVE_USLEEP
                usleep(1000);
#endif
                gnome_sound_init(NULL);
        }
}

/* stop_esd
 *
 * Stop the Enlightenment Sound Daemon. 
 */
static void
stop_esd (void) 
{
	g_print (_("Stopping esd\n"));
	/* Can't think of a way to do this reliably, so we fake it for now */
	esd_standby (gnome_sound_connection_get ());
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
	sid = esd_sample_getid(gnome_sound_connection_get (), key);
	if (sid >= 0)
		esd_sample_free(gnome_sound_connection_get (), sid);

	if (!event->file || !strcmp (event->file, ""))
		return;
	
	file = g_strdup (event->file);
	if (file[0] != '/')
	{
		tmp = gnome_program_locate_file (NULL, GNOME_FILE_DOMAIN_SOUND, file, TRUE, NULL);
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
		g_warning (_("Couldn't load sound file %s as sample %s"),
			   file, key);

	g_free (key);
}


static void
apply_settings (void)
{
	GConfClient *client;
	static gboolean inited = FALSE;
	static int event_changed_old = 0;
	int event_changed_new;

	gboolean enable_esd;
	gboolean event_sounds;

	client = gconf_client_get_default ();

	enable_esd        = gconf_client_get_bool (client, "/desktop/gnome/sound/enable_esd", NULL);
	event_sounds      = gconf_client_get_bool (client, "/desktop/gnome/sound/event_sounds", NULL);
	event_changed_new = gconf_client_get_int  (client, "/desktop/gnome/sound/event_changed", NULL);

	if (enable_esd && gnome_sound_connection_get () < 0)
		start_esd ();
	else if (!enable_esd)
		stop_esd ();

	if (!inited || event_changed_old != event_changed_new)
	{
		SoundProperties *props;
		g_print (_("Reloading events\n"));
		
		inited = TRUE;
		event_changed_old = event_changed_new;
		
		props = sound_properties_new ();
		sound_properties_add_defaults (props, NULL);
		sound_properties_foreach (props, reload_foreach_cb, NULL);
		gtk_object_destroy (GTK_OBJECT (props));
	}

	g_object_unref (G_OBJECT (client));
}

void
gnome_settings_sound_init (GConfClient *client)
{
	gnome_settings_daemon_register_callback ("/desktop/gnome/sound", (KeyCallbackFunc) apply_settings);
}

void
gnome_settings_sound_load (GConfClient *client)
{
	apply_settings ();
}
