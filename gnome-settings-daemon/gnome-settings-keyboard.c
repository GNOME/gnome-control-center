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
#ifdef HAVE_X11_EXTENSIONS_XKB_H
#include <X11/XKBlib.h>
#endif

#ifdef HAVE_X11_EXTENSIONS_XF86MISC_H
static gboolean
xfree86_set_keyboard_autorepeat_rate (int delay, int rate)
{
	gboolean res = FALSE;
	int event_base_return, error_base_return;

	if (XF86MiscQueryExtension (GDK_DISPLAY (),
				    &event_base_return,
				    &error_base_return) == True)
	{
		/* load the current settings */
		XF86MiscKbdSettings kbdsettings;
		XF86MiscGetKbdSettings (GDK_DISPLAY (), &kbdsettings);

		/* assign the new values */
		kbdsettings.delay = delay;
		kbdsettings.rate = rate;
		XF86MiscSetKbdSettings (GDK_DISPLAY (), &kbdsettings);
		res = TRUE;
	}

	return res;
}
#endif /* HAVE_X11_EXTENSIONS_XF86MISC_H */
#ifdef HAVE_X11_EXTENSIONS_XKB_H
static gboolean
xkb_set_keyboard_autorepeat_rate (int delay, int rate)
{
	return XkbSetAutoRepeatRate (GDK_DISPLAY (), XkbUseCoreKbd,
				     delay, 1000/rate);
}
#endif

static void
apply_settings (void)
{
	GConfClient *client;

	gboolean repeat, click;
	int rate, delay;
	int click_volume, bell_volume, bell_pitch, bell_duration;

	XKeyboardControl kbdcontrol;

	client = gconf_client_get_default ();

	repeat        = gconf_client_get_bool  (client, "/desktop/gnome/peripherals/keyboard/repeat",        NULL);
	click         = gconf_client_get_bool  (client, "/desktop/gnome/peripherals/keyboard/click",         NULL);
	rate          = gconf_client_get_int   (client, "/desktop/gnome/peripherals/keyboard/rate",          NULL);
	delay         = gconf_client_get_int   (client, "/desktop/gnome/peripherals/keyboard/delay",         NULL);
	click_volume  = gconf_client_get_int   (client, "/desktop/gnome/peripherals/keyboard/click_volume",  NULL);
	bell_volume   = gconf_client_get_int   (client, "/desktop/gnome/peripherals/keyboard/bell_volume",   NULL);
	bell_pitch    = gconf_client_get_int   (client, "/desktop/gnome/peripherals/keyboard/bell_pitch",    NULL);
	bell_duration = gconf_client_get_int   (client, "/desktop/gnome/peripherals/keyboard/bell_duration", NULL);

	gdk_error_trap_push ();
        if (repeat) {
		gboolean rate_set = FALSE;

		XAutoRepeatOn (GDK_DISPLAY ());
		/* Use XKB in preference */
#if defined (HAVE_X11_EXTENSIONS_XKB_H)
		rate_set = xkb_set_keyboard_autorepeat_rate (delay, rate);
#endif
#ifdef HAVE_X11_EXTENSIONS_XF86MISC_H
		if (!rate_set)
			rate_set = xfree86_set_keyboard_autorepeat_rate (delay, rate);
#endif
		if (!rate_set) {
			g_warning ("Neither XKeyboard not Xfree86's keyboard extensions are available,\n"
				   "no way to support keyboard autorepeat settings");
			XAutoRepeatOff (GDK_DISPLAY ());
		}
	} else
		XAutoRepeatOff (GDK_DISPLAY ());

	/* as percentage from 0..100 inclusive */
	if (click_volume < 0)
		click_volume = 0;
	else if (click_volume > 100)
		click_volume = 100;
	kbdcontrol.key_click_percent = click ? click_volume : 0;
	kbdcontrol.bell_percent = bell_volume;
	kbdcontrol.bell_pitch = bell_pitch;
	kbdcontrol.bell_duration = bell_duration;
	XChangeKeyboardControl (GDK_DISPLAY (), KBKeyClickPercent, 
				&kbdcontrol);

	XSync (GDK_DISPLAY (), FALSE);
	gdk_error_trap_pop ();
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

