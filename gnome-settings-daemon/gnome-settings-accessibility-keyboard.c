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

#include "config.h"

#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <gconf/gconf-client.h>

#include "gnome-settings-accessibility-keyboard.h"
#include "gnome-settings-daemon.h"

#ifdef HAVE_X11_EXTENSIONS_XKB_H
#  include <X11/XKBlib.h>
#  include <X11/extensions/XKBstr.h>

#define CONFIG_ROOT "/desktop/gnome/accesibility/keyboard"

#ifdef DEBUG_ACCESSIBILITY
#define d(str)		fprintf (stderr, str)
#else
#define d(str)		do { } while (0)
#endif

static gboolean we_are_changing_xkb_state = FALSE;

static int
get_int (GConfClient *client, char const *key)
{
	int res = gconf_client_get_int  (client, key, NULL);
	if (res <= 0)
		res = 1;
	return res;
}

static void
set_int (GConfClient *client, char const *key, int val)
{
	GError *err;
	if (!gconf_client_set_int (client, key, val, &err)) {
		g_warning (err->message);
		g_error_free (err);
	}
}

static void
set_bool (GConfClient *client, char const *key, int val)
{
	GError *err;
	if (!gconf_client_set_bool (client, key, val ? TRUE : FALSE, &err)) {
		g_warning (err->message);
		g_error_free (err);
	}
}

static void
set_server_from_gconf (GConfEntry *ignored)
{
	unsigned long	 which;
	gint32 		 enabled, enable_mask;
	XkbDescRec	*desc;
	GConfClient	*client = gconf_client_get_default ();

	if (we_are_changing_xkb_state) {
		d ("We changed gconf accessibility state\n");
		return;
	} else
		d ("Someone changed gconf accessibility state\n");

	enable_mask =   XkbAccessXKeysMask	|
			XkbSlowKeysMask		|
			XkbBounceKeysMask	|
			XkbStickyKeysMask	|
			XkbMouseKeysMask	|
			XkbMouseKeysAccelMask	|
			XkbAccessXKeysMask	|
			XkbAccessXTimeoutMask	|
			XkbAccessXFeedbackMask;

	enabled = XkbAccessXFeedbackMask;

	desc = XkbGetMap(GDK_DISPLAY (), 0, XkbUseCoreKbd);
	XkbGetControls (GDK_DISPLAY (), XkbAllControlsMask, desc);

	desc->ctrls->ax_options = XkbAX_LatchToLockMask;

	/* general */
	if (gconf_client_get_bool (client, CONFIG_ROOT "/enable", NULL))
		enabled |= XkbAccessXKeysMask;
	if (gconf_client_get_bool (client, CONFIG_ROOT "/feature_state_change_beep", NULL))
		desc->ctrls->ax_options |= XkbAX_FeatureFBMask | XkbAX_SlowWarnFBMask;
	if (gconf_client_get_bool (client, CONFIG_ROOT "/timeout_enable", NULL))
		enabled |= XkbAccessXTimeoutMask;
	desc->ctrls->ax_timeout      = get_int (client,
		CONFIG_ROOT "/timeout");

	/* bounce keys */
	if (gconf_client_get_bool (client, CONFIG_ROOT "/bouncekeys_enable", NULL))
		enabled |= XkbBounceKeysMask;
	if (gconf_client_get_bool (client, CONFIG_ROOT "/bouncekeys_beep_reject", NULL))
		enabled |= XkbAX_BKRejectFBMask;
	desc->ctrls->debounce_delay  = get_int (client,
		CONFIG_ROOT "/bouncekeys_delay");

	/* mouse keys */
	if (gconf_client_get_bool (client, CONFIG_ROOT "/mousekeys_enable", NULL))
		enabled |= XkbMouseKeysMask | XkbMouseKeysAccelMask;
	desc->ctrls->mk_interval     = 10;	/* msec between mousekey events */
	desc->ctrls->mk_curve	     = 50;
	desc->ctrls->mk_max_speed    = get_int (client, /* pixels / event */
		CONFIG_ROOT "/mousekeys_max_speed");
	desc->ctrls->mk_time_to_max  = get_int (client,	/* events before max */
		CONFIG_ROOT "/mousekeys_accel_time");
	desc->ctrls->mk_delay	     = get_int (client,	/* ms before 1st event */
		CONFIG_ROOT "/mousekeys_init_delay");

	/* slow keys */
	if (gconf_client_get_bool (client, CONFIG_ROOT "/slowkeys_enable", NULL))
		enabled |= XkbSlowKeysMask;
	if (gconf_client_get_bool (client, CONFIG_ROOT "/slowkeys_beep_press", NULL))
		desc->ctrls->ax_options |= XkbAX_SKPressFBMask;
	if (gconf_client_get_bool (client, CONFIG_ROOT "/slowkeys_beep_accept", NULL))
		desc->ctrls->ax_options |= XkbAX_SKAcceptFBMask;
	if (gconf_client_get_bool (client, CONFIG_ROOT "/slowkeys_beep_reject", NULL))
		enabled |= XkbAX_SKRejectFBMask;
	desc->ctrls->slow_keys_delay = get_int (client,
		CONFIG_ROOT "/slowkeys_delay");

	/* sticky keys */
	if (gconf_client_get_bool (client, CONFIG_ROOT "/stickykeys_enable", NULL))
		enabled |= XkbStickyKeysMask;
	if (gconf_client_get_bool (client, CONFIG_ROOT "/stickykeys_two_key_off", NULL))
		desc->ctrls->ax_options |= XkbAX_TwoKeysMask;
	if (gconf_client_get_bool (client, CONFIG_ROOT "/stickykeys_modifier_beep", NULL))
		desc->ctrls->ax_options |= XkbAX_StickyKeysFBMask;

	/* toggle keys */
	if (gconf_client_get_bool (client, CONFIG_ROOT "/togglekeys_enable", NULL))
		desc->ctrls->ax_options |= XkbAX_IndicatorFBMask;
	desc->ctrls->enabled_ctrls &= ~enable_mask;
	desc->ctrls->enabled_ctrls |= (enable_mask & enabled);

	/* guard against reloading gconf when the X server notices that the XKB
	 * state has changed and calls us.
	 */
	g_return_if_fail (!we_are_changing_xkb_state);

	which = XkbAccessXKeysMask | XkbAccessXTimeoutMask |
		XkbControlsEnabledMask |
		XkbMouseKeysMask | XkbMouseKeysAccelMask |
		XkbSlowKeysMask | XkbBounceKeysMask;

	we_are_changing_xkb_state = TRUE;
	XkbSetControls (GDK_DISPLAY (), which, desc);
	XFlush (GDK_DISPLAY ());
	we_are_changing_xkb_state = FALSE;
}

static void
set_gconf_from_server (GConfEntry *ignored)
{
	XkbDescRec	*desc;
	GConfClient	*client = gconf_client_get_default ();

	desc = XkbGetMap(GDK_DISPLAY (), 0, XkbUseCoreKbd);
	XkbGetControls (GDK_DISPLAY (), XkbAllControlsMask, desc);

	desc->ctrls->ax_options = XkbAX_LatchToLockMask;

	/* guard against reloading the server when gconf notices that the state
	 * has changed and calls us.
	 */
	g_return_if_fail (!we_are_changing_xkb_state);
	we_are_changing_xkb_state = TRUE;

	set_bool (client, CONFIG_ROOT "/enable",
		desc->ctrls->enabled_ctrls & XkbAccessXKeysMask);
	set_bool (client, CONFIG_ROOT "/feature_state_change_beep",
		desc->ctrls->ax_options & (XkbAX_FeatureFBMask | XkbAX_SlowWarnFBMask));
	set_bool (client, CONFIG_ROOT "/timeout_enable",
		desc->ctrls->enabled_ctrls & XkbAccessXTimeoutMask);
	set_int (client, CONFIG_ROOT "/timeout",
		desc->ctrls->ax_timeout);

	set_bool (client, CONFIG_ROOT "/bouncekeys_enable",
		desc->ctrls->enabled_ctrls & XkbBounceKeysMask);
	set_int (client, CONFIG_ROOT "/bouncekeys_delay",
		desc->ctrls->debounce_delay);
	set_bool (client, CONFIG_ROOT "/bouncekeys_beep_reject",
		desc->ctrls->enabled_ctrls & XkbAX_BKRejectFBMask);

	set_bool (client, CONFIG_ROOT "/mousekeys_enable",
		desc->ctrls->enabled_ctrls & XkbMouseKeysMask);
	set_int (client, CONFIG_ROOT "/mousekeys_max_speed",
		desc->ctrls->mk_max_speed);
	set_int (client, CONFIG_ROOT "/mousekeys_accel_time",
		desc->ctrls->mk_time_to_max);
	set_int (client, CONFIG_ROOT "/mousekeys_init_delay",
		desc->ctrls->mk_delay);

	set_bool (client, CONFIG_ROOT "/slowkeys_enable",
		desc->ctrls->enabled_ctrls & XkbSlowKeysMask);
	set_bool (client, CONFIG_ROOT "/slowkeys_beep_press",
		desc->ctrls->ax_options & XkbAX_SKPressFBMask);
	set_bool (client, CONFIG_ROOT "/slowkeys_beep_accept",
		desc->ctrls->ax_options & XkbAX_SKAcceptFBMask);
	set_bool (client, CONFIG_ROOT "/slowkeys_beep_reject",
		desc->ctrls->enabled_ctrls & XkbAX_SKRejectFBMask);
	set_int (client, CONFIG_ROOT "/slowkeys_delay",
		desc->ctrls->slow_keys_delay);

	set_bool (client, CONFIG_ROOT "/stickykeys_enable",
		desc->ctrls->enabled_ctrls & XkbStickyKeysMask);
	set_bool (client, CONFIG_ROOT "/stickykeys_two_key_off",
		desc->ctrls->ax_options & XkbAX_TwoKeysMask);
	set_bool (client, CONFIG_ROOT "/stickykeys_modifier_beep",
		desc->ctrls->ax_options & XkbAX_StickyKeysFBMask);

	set_bool (client, CONFIG_ROOT "/togglekeys_enable",
		desc->ctrls->ax_options & XkbAX_IndicatorFBMask);

	we_are_changing_xkb_state = FALSE;
}

static int xkbEventBase;
static GdkFilterReturn 
cb_xkb_event_filter (GdkXEvent *xevent, GdkEvent *event, gpointer user)
{
	XEvent *xev = (XEvent *)xevent;
	if (xev->xany.type == (xkbEventBase + XkbEventCode)) {
		XkbEvent *xkbEv = (XkbEvent *) event;
		d ("xkb event\n");
		if(xkbEv->any.xkb_type == XkbControlsNotify) {
			if (!we_are_changing_xkb_state) {
				d ("Someone changed XKB state\n");
				set_gconf_from_server (NULL);
			} else
				d ("We changed XKB state\n");
		}
		return GDK_FILTER_REMOVE;
	}

	return GDK_FILTER_CONTINUE;
}

/**
 * gnome_settings_accessibility_keyboard_init :
 *
 * If the display supports XKB initialize it.
 */
void
gnome_settings_accessibility_keyboard_init (GConfClient *client)
{
	int opcode, errorBase, major, minor;

	if (!XkbQueryExtension (GDK_DISPLAY (),
		&opcode, &xkbEventBase, &errorBase, &major, &minor) ||
	    !XkbUseExtension (GDK_DISPLAY (), &major, &minor))
		return;

	XkbSelectEvents (GDK_DISPLAY (),
		XkbUseCoreKbd, XkbAllEventsMask, XkbAllEventsMask);
	gdk_window_add_filter (NULL, &cb_xkb_event_filter, NULL);

	gnome_settings_daemon_register_callback (CONFIG_ROOT, &set_server_from_gconf);
}

void
gnome_settings_accessibility_keyboard_load (GConfClient *client)
{
	set_server_from_gconf (NULL);
}

#else

void
gnome_settings_accessibility_keyboard_init (GConfClient *client)
{
	g_warning ("Unsupported in this build");
}

void
gnome_settings_accessibility_keyboard_load (GConfClient *client)
{
	g_warning ("Unsupported in this build");
}
#endif
