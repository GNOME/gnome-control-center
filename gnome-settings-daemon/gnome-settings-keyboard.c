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
#include "gnome-settings-locate-pointer.h"

#ifdef HAVE_X11_EXTENSIONS_XF86MISC_H
#  include <X11/extensions/xf86misc.h>
#endif

#define HAVE_XKB
#ifdef HAVE_XKB
#  include <X11/XKBlib.h>
#endif

static gboolean use_xkb = FALSE;
static gint xkb_event_type = 0;

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
			g_message ("Setting rate to %d", kbdsettings.rate);
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

#ifdef HAVE_XKB
/* XKB support
 */
static GdkFilterReturn
gnome_settings_keyboard_xkb_filter (GdkXEvent *xevent,
				    GdkEvent  *event,
				    gpointer   data)
{
	if (((XEvent *) xevent)->type == xkb_event_type) {
	      XkbEvent *xkb_event = (XkbEvent *)xevent;
	      if (xkb_event->any.xkb_type == XkbStateNotify) {
		      /* gnome_settings_locate_pointer (); */
	      }
	}
	return GDK_FILTER_CONTINUE;
}
#endif

static void
gnome_settings_keyboard_init_xkb (void)
{
#ifdef HAVE_XKB
	gint xkb_major = XkbMajorVersion;
	gint xkb_minor = XkbMinorVersion;
	g_print ("foo1\n");
	if (XkbLibraryVersion (&xkb_major, &xkb_minor)) {
		xkb_major = XkbMajorVersion;
		xkb_minor = XkbMinorVersion;
		g_print ("foo2\n");

		if (XkbQueryExtension (gdk_display, NULL, &xkb_event_type, NULL,
				       &xkb_major, &xkb_minor)) {
			g_print ("foo3\n");
			XkbSelectEvents (gdk_display,
					 XkbUseCoreKbd,
					 XkbMapNotifyMask | XkbStateNotifyMask,
					 XkbMapNotifyMask | XkbStateNotifyMask);
			gdk_window_add_filter (NULL, gnome_settings_keyboard_xkb_filter, NULL);
		}
	}
#endif
}



void
gnome_settings_keyboard_init (GConfClient *client)
{
	gnome_settings_daemon_register_callback ("/desktop/gnome/peripherals/keyboard", (KeyCallbackFunc) apply_settings);
	gnome_settings_keyboard_init_xkb ();
}

void
gnome_settings_keyboard_load (GConfClient *client)
{
	apply_settings ();
}
