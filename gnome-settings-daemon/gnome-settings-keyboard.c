/* -*- mode: c; style: linux -*- */

/* gnome-settings-keyboard.c
 *
 * Copyright © 2001 Ximian, Inc.
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
# include "config.h"
#endif

#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <gconf/gconf-client.h>

#include "gnome-settings-keyboard.h"
#include "gnome-settings-daemon.h"

#ifdef HAVE_X11_EXTENSIONS_XF86MISC_H
#  include <X11/extensions/xf86misc.h>
#endif

static void
apply_settings (void)
{
	GConfClient *client;

	gboolean repeat, click;
	int rate, delay, volume;
	int bell_volume, bell_pitch, bell_duration;

#ifdef HAVE_X11_EXTENSIONS_XF86MISC_H
	XF86MiscKbdSettings kbdsettings;
#endif
	XKeyboardControl kbdcontrol;
        int event_base_return, error_base_return;

	client = gconf_client_get_default ();

	repeat        = gconf_client_get_bool (client, "/gnome/desktop/peripherals/keyboard/repeat",        NULL);
	click         = gconf_client_get_bool (client, "/gnome/desktop/peripherals/keyboard/click",         NULL);
	rate          = gconf_client_get_int  (client, "/gnome/desktop/peripherals/keyboard/rate",          NULL);
	delay         = gconf_client_get_int  (client, "/gnome/desktop/peripherals/keyboard/delay",         NULL);
	volume        = gconf_client_get_int  (client, "/gnome/desktop/peripherals/keyboard/click_volume",  NULL);
	bell_volume   = gconf_client_get_int  (client, "/gnome/desktop/peripherals/keyboard/bell_volume",   NULL);
	bell_pitch    = gconf_client_get_int  (client, "/gnome/desktop/peripherals/keyboard/bell_pitch",    NULL);
	bell_duration = gconf_client_get_int  (client, "/gnome/desktop/peripherals/keyboard/bell_duration", NULL);

        if (repeat) {
		XAutoRepeatOn (GDK_DISPLAY ());
#ifdef HAVE_X11_EXTENSIONS_XF86MISC_H
		if (XF86MiscQueryExtension (GDK_DISPLAY (),
					    &event_base_return,
					    &error_base_return) == True)
		{
			kbdsettings.type = 0;
                        kbdsettings.rate = rate;
                        kbdsettings.delay = delay;
			kbdsettings.servnumlock = False;
                        XF86MiscSetKbdSettings (GDK_DISPLAY (), &kbdsettings);
                } else {
                        XAutoRepeatOff (GDK_DISPLAY ());
                }
#endif
	} else {
		XAutoRepeatOff (GDK_DISPLAY ());
	}

	kbdcontrol.key_click_percent = 
		click ? volume : 0;
	kbdcontrol.bell_percent = bell_volume;
	kbdcontrol.bell_pitch = bell_pitch;
	kbdcontrol.bell_duration = bell_duration;
	XChangeKeyboardControl (GDK_DISPLAY (), KBKeyClickPercent, 
				&kbdcontrol);
}


void
gnome_settings_keyboard_init (GConfClient *client)
{
	gnome_settings_daemon_register_callback ("/desktop/gnome/peripherals/keyboard", (KeyCallbackFunc) apply_settings);
}

void
gnome_settings_keyboard_load (GConfClient *client)
{
	apply_settings ();
}

