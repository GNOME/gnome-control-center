/* -*- mode: c; style: linux -*- */

/* gnome-settings-keyboard-xkb.c
 *
 * Copyright © 2001 Udaltsoft
 *
 * Written by Sergey V. Oudaltsov <svu@users.sourceforge.net>
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

#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <gconf/gconf-client.h>

#include <string.h>

#include <libxklavier/xklavier.h>
#include <libgswitchit/gswitchit_xkb_config.h>

#include "gnome-settings-keyboard-xkb.h"
#include "gnome-settings-daemon.h"

static GSwitchItXkbConfig gswic;
static gboolean initedOk;

static void
apply_settings (void)
{
	GConfClient *confClient;

	printf ("xkb.apply_settings!\n");
	memset (&gswic, 0, sizeof (gswic));

	confClient = gconf_client_get_default ();
	GSwitchItXkbConfigInit (&gswic, confClient);
	g_object_unref (confClient);
	GSwitchItXkbConfigLoad (&gswic);

	if (!gswic.overrideSettings)
		GSwitchItXkbConfigLoadInitial (&gswic);

	if (!GSwitchItXkbConfigActivate (&gswic))
		g_warning ("Could not activate the XKB configuration");

	GSwitchItXkbConfigTerm (&gswic);
}

void
gnome_settings_keyboard_xkb_init (GConfClient * client)
{
	if (!XklInit (GDK_DISPLAY ())) {
		initedOk = TRUE;
		gnome_settings_daemon_register_callback
		    ("/desktop/gnome/peripherals/keyboard/xkb",
		     (KeyCallbackFunc) apply_settings);
	}
}

void
gnome_settings_keyboard_xkb_load (GConfClient * client)
{
	if (initedOk)
		apply_settings ();
}
