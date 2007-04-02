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
#include <sys/types.h>

#include <glib/gi18n.h>

#ifdef HAVE_ESD
# include <esd.h>
#endif

#include <gconf/gconf-client.h>
#include <libgnome/gnome-sound.h>
#include <libgnome/gnome-exec.h>
#include <libgnomeui/gnome-client.h>
#include "libsounds/sound-properties.h"
#include "gnome-settings-sound.h"
#include "gnome-settings-daemon.h"

/* start_gnome_sound
 *
 * Start GNOME sound. 
 */
static void
start_gnome_sound (void) 
{
	gnome_sound_init (NULL);
	if (gnome_sound_connection_get () < 0)
		g_warning ("Could not start GNOME sound.\n");
}

#ifdef HAVE_ESD
static gboolean set_esd_standby = TRUE;
#endif

/* stop_gnome_sound
 *
 * Stop GNOME sound. 
 */
static void
stop_gnome_sound (void) 
{
#ifdef HAVE_ESD
	/* Can't think of a way to do this reliably, so we fake it for now */
	esd_standby (gnome_sound_connection_get ());
	set_esd_standby = TRUE;
#else
	gnome_sound_shutdown ();
#endif
}

struct reload_foreach_closure {
	gboolean enable_system_sounds;
};

/* reload_foreach_cb
 *
 * For a given SoundEvent, reload the sound file associate with the event. 
 */
static void
reload_foreach_cb (SoundEvent *event, gpointer data)
{
	struct reload_foreach_closure *closure;
	gchar *key, *file;
	int sid;
	gboolean do_load;

	closure = data;

	key = sound_event_compose_key (event);

#ifdef HAVE_ESD
	/* We need to free up the old sample, because
	 * esd allows multiple samples with the same name,
	 * putting memory to waste. */
	sid = esd_sample_getid(gnome_sound_connection_get (), key);
	if (sid >= 0)
		esd_sample_free(gnome_sound_connection_get (), sid);
#endif
	/* We only disable sounds for system events.  Other events, like sounds
	 * in games, should be preserved.  The games should have their own
	 * configuration for sound anyway.
	 */
	if ((strcmp (event->category, "gnome-2") == 0
	     || strcmp (event->category, "gtk-events-2") == 0))
		do_load = closure->enable_system_sounds;
	else
		do_load = TRUE;

	if (!do_load)
		goto out;

	if (!event->file || !strcmp (event->file, ""))
		goto out;

	if (event->file[0] == '/')
		file = g_strdup (event->file);
	else
		file = gnome_program_locate_file (NULL,
						  GNOME_FILE_DOMAIN_SOUND,
						  event->file, TRUE, NULL);

	if (!file)
		goto out;

	sid = gnome_sound_sample_load (key, file);

	if (sid < 0)
		g_warning (_("Couldn't load sound file %s as sample %s"),
			   file, key);

	g_free (file);

 out:
	g_free (key);
}

static void
apply_settings (void)
{
	GConfClient *client;
	static gboolean inited = FALSE;
	static int event_changed_old = 0;
	int event_changed_new;

	gboolean enable_sound;
	gboolean event_sounds;

	struct reload_foreach_closure closure;

	client = gnome_settings_get_config_client ();

#ifdef HAVE_ESD
	enable_sound        = gconf_client_get_bool (client, "/desktop/gnome/sound/enable_esd", NULL);
#else
	enable_sound        = TRUE;
#endif
	event_sounds      = gconf_client_get_bool (client, "/desktop/gnome/sound/event_sounds", NULL);
	event_changed_new = gconf_client_get_int  (client, "/desktop/gnome/sound/event_changed", NULL);

	closure.enable_system_sounds = event_sounds;

	if (enable_sound) {
		if (gnome_sound_connection_get () < 0) 
			start_gnome_sound ();
#ifdef HAVE_ESD
		else if (set_esd_standby) {
			esd_resume (gnome_sound_connection_get ());
			set_esd_standby = FALSE;
		}
#endif
	} else {
#ifdef HAVE_ESD
	  if (!set_esd_standby)
#endif
		stop_gnome_sound ();
	}

	if (enable_sound &&
	    (!inited || event_changed_old != event_changed_new))
	{
		SoundProperties *props;
		
		inited = TRUE;
		event_changed_old = event_changed_new;

		
		props = sound_properties_new ();
		sound_properties_add_defaults (props, NULL);
		sound_properties_foreach (props, reload_foreach_cb, &closure);
		gtk_object_destroy (GTK_OBJECT (props));
	}
}

void
gnome_settings_sound_init (GConfClient *client)
{
	gnome_settings_register_config_callback ("/desktop/gnome/sound",
						 (GnomeSettingsConfigCallback) apply_settings);
}

void
gnome_settings_sound_load (GConfClient *client)
{
	apply_settings ();
}
