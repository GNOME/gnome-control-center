/* vim: set sw=8: -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* gnome-settings-keyboard.c
 *
 * Copyright © 2001 Ximian, Inc.
 *
 * Written by Jody Goldberg <jody@gnome.org>
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

#define CONFIG_ROOT "/desktop/gnome/accessibility/keyboard"

#undef DEBUG_ACCESSIBILITY
#ifdef DEBUG_ACCESSIBILITY
#define d(str)		fprintf (stderr, str)
#else
#define d(str)		do { } while (0)
#endif

static gboolean we_are_changing_xkb_state = FALSE;
static int xkbEventBase;

static gboolean
xkb_enabled (void)
{
	static gboolean initialized = 0;
	static gboolean have_xkb = 0;

	int opcode, errorBase, major, minor;

	if (initialized)
		return have_xkb;

	gdk_error_trap_push ();
	have_xkb = XkbQueryExtension (GDK_DISPLAY (),
				      &opcode, &xkbEventBase, &errorBase, &major, &minor)
		&& XkbUseExtension (GDK_DISPLAY (), &major, &minor);
	XSync (GDK_DISPLAY (), FALSE);
	gdk_error_trap_pop ();

	return have_xkb;
}

static XkbDescRec *
get_xkb_desc_rec (void)
{
	XkbDescRec *desc;
	Status	    status = Success; /* Any bogus value, to suppress warning */

	if (!xkb_enabled ())
		return NULL;

	gdk_error_trap_push ();
	desc = XkbGetMap (GDK_DISPLAY (), XkbAllMapComponentsMask, XkbUseCoreKbd);
	if (desc != NULL) {
		desc->ctrls = NULL;
		status = XkbGetControls (GDK_DISPLAY (), XkbAllControlsMask, desc);
	}
	XSync (GDK_DISPLAY (), FALSE);
	gdk_error_trap_pop ();

	g_return_val_if_fail (desc != NULL, NULL);
	g_return_val_if_fail (desc->ctrls != NULL, NULL);
	g_return_val_if_fail (status == Success, NULL);

	return desc;
}

static int
get_int (GConfClient *client, char const *key)
{
	int res = gconf_client_get_int  (client, key, NULL);
	if (res <= 0)
		res = 1;
	return res;
}

static gboolean
set_int (GConfClient *client, GConfChangeSet *cs,
	 char const *key, int val)
{
	gconf_change_set_set_int (cs, key, val);
		if (val != gconf_client_get_int (client, key, NULL)) {
			g_warning ("%s changed", key);
		}
	return val != gconf_client_get_int (client, key, NULL);
}

static gboolean
set_bool (GConfClient *client, GConfChangeSet *cs,
	  gboolean in_gconf, char const *key, int val)
{
	if (in_gconf || val) {
		gconf_change_set_set_bool (cs, key, val ? TRUE : FALSE);
		if (val != gconf_client_get_bool (client, key, NULL)) {
			g_warning ("%s changed", key);
		}
		return val != gconf_client_get_bool (client, key, NULL);
	}
	return FALSE;
}

static unsigned long
set_clear (gboolean flag, unsigned long value, unsigned long mask)
{
    if (flag)
	    return value | mask;
    return value & ~mask;
}

static gboolean
set_ctrl_from_gconf (XkbDescRec *desc, GConfClient *client,
		     char const *key, unsigned long mask, gboolean flag)
{
    gboolean result = flag && gconf_client_get_bool (client, key, NULL);
    desc->ctrls->enabled_ctrls =
	    set_clear (result, desc->ctrls->enabled_ctrls, mask);
    return result;
}

static void
set_server_from_gconf (GConfEntry *ignored)
{
	GConfClient	*client = gconf_client_get_default ();
	XkbDescRec	*desc;
	gboolean	 enable_accessX;

	desc = get_xkb_desc_rec ();
	if (!desc) {
		d ("No XKB present\n");
		return;
	}

	if (we_are_changing_xkb_state) {
		d ("We changed gconf accessibility state\n");
		return;
	} else
		d ("Someone changed gconf accessibility state\n");

	/* general */
	enable_accessX = gconf_client_get_bool (client, CONFIG_ROOT "/enable", NULL);

	desc->ctrls->enabled_ctrls = set_clear (enable_accessX,
		desc->ctrls->enabled_ctrls,
		XkbAccessXKeysMask | XkbAccessXFeedbackMask);

	if (set_ctrl_from_gconf (desc, client, CONFIG_ROOT "/timeout_enable",
		XkbAccessXTimeoutMask, enable_accessX)) {
		desc->ctrls->ax_timeout = get_int (client,
			CONFIG_ROOT "/timeout");
		/* disable only the master flag via the server we will disable
		 * the rest on the rebound without affecting gconf state
		 * don't change the option flags at all.
		 */
		desc->ctrls->axt_ctrls_mask = \
			XkbAccessXKeysMask	|
			XkbAccessXFeedbackMask;
		desc->ctrls->axt_ctrls_values = 0;
		desc->ctrls->axt_opts_mask = 0;
	}

	desc->ctrls->ax_options = set_clear (enable_accessX &&
			gconf_client_get_bool (client, CONFIG_ROOT "/feature_state_change_beep", NULL),
			desc->ctrls->ax_options, XkbAX_FeatureFBMask | XkbAX_SlowWarnFBMask);

	/* bounce keys */
	if (set_ctrl_from_gconf (desc, client, CONFIG_ROOT "/bouncekeys_enable",
		XkbBounceKeysMask, enable_accessX)) {
		desc->ctrls->debounce_delay  = get_int (client,
			CONFIG_ROOT "/bouncekeys_delay");
		desc->ctrls->ax_options = set_clear (
			gconf_client_get_bool (client, CONFIG_ROOT "/bouncekeys_beep_reject", NULL),
			desc->ctrls->ax_options, XkbAX_BKRejectFBMask);
	}

	/* mouse keys */
	if (set_ctrl_from_gconf (desc, client, CONFIG_ROOT "/mousekeys_enable",
		XkbMouseKeysMask | XkbMouseKeysAccelMask, enable_accessX)) {
		desc->ctrls->mk_interval     = 100;	/* msec between mousekey events */
		desc->ctrls->mk_curve	     = 50;

		/* We store pixels / sec, XKB wants pixels / event */
		desc->ctrls->mk_max_speed    = get_int (client,
			CONFIG_ROOT "/mousekeys_max_speed") / (1000 / desc->ctrls->mk_interval);
		if (desc->ctrls->mk_max_speed <= 0)
			desc->ctrls->mk_max_speed = 1;

		desc->ctrls->mk_time_to_max  = get_int (client,	/* events before max */
			CONFIG_ROOT "/mousekeys_accel_time") / desc->ctrls->mk_interval;
		if (desc->ctrls->mk_time_to_max <= 0)
			desc->ctrls->mk_time_to_max = 1;

		desc->ctrls->mk_delay	     = get_int (client,	/* ms before 1st event */
			CONFIG_ROOT "/mousekeys_init_delay");
	}

	/* slow keys */
	if (set_ctrl_from_gconf (desc, client, CONFIG_ROOT "/slowkeys_enable",
		XkbSlowKeysMask, enable_accessX)) {
		desc->ctrls->ax_options = set_clear (
			gconf_client_get_bool (client, CONFIG_ROOT "/slowkeys_beep_press", NULL),
			desc->ctrls->ax_options, XkbAX_SKPressFBMask);
		desc->ctrls->ax_options = set_clear (
			gconf_client_get_bool (client, CONFIG_ROOT "/slowkeys_beep_accept", NULL),
			desc->ctrls->ax_options, XkbAX_SKAcceptFBMask);
		desc->ctrls->ax_options = set_clear (
			gconf_client_get_bool (client, CONFIG_ROOT "/slowkeys_beep_reject", NULL),
			desc->ctrls->ax_options, XkbAX_SKRejectFBMask);
		desc->ctrls->slow_keys_delay = get_int (client,
			CONFIG_ROOT "/slowkeys_delay");
		/* anything larger than 500 seems to loose all keyboard input */
		if (desc->ctrls->slow_keys_delay > 500)
			desc->ctrls->slow_keys_delay = 500;
	}

	/* sticky keys */
	if (set_ctrl_from_gconf (desc, client, CONFIG_ROOT "/stickykeys_enable",
		XkbStickyKeysMask, enable_accessX)) {
		desc->ctrls->ax_options |= XkbAX_LatchToLockMask;
		desc->ctrls->ax_options = set_clear (
			gconf_client_get_bool (client, CONFIG_ROOT "/stickykeys_two_key_off", NULL),
			desc->ctrls->ax_options, XkbAX_TwoKeysMask);
		desc->ctrls->ax_options = set_clear (
			gconf_client_get_bool (client, CONFIG_ROOT "/stickykeys_modifier_beep", NULL),
			desc->ctrls->ax_options, XkbAX_StickyKeysFBMask);
	}

	/* toggle keys */
	desc->ctrls->ax_options = set_clear (enable_accessX &&
		gconf_client_get_bool (client, CONFIG_ROOT "/togglekeys_enable", NULL),
		desc->ctrls->ax_options, XkbAX_IndicatorFBMask);

	/*
	fprintf (stderr, "CHANGE to : 0x%x\n", desc->ctrls->enabled_ctrls);
	fprintf (stderr, "CHANGE to : 0x%x (2)\n", desc->ctrls->ax_options);
	*/
	/* guard against reloading gconf when the X server notices that the XKB
	 * state has changed and calls us.
	 */
	g_return_if_fail (!we_are_changing_xkb_state);

	we_are_changing_xkb_state = TRUE;
	gdk_error_trap_push ();
	XkbSetControls (GDK_DISPLAY (),
			XkbSlowKeysMask		|
			XkbBounceKeysMask	|
			XkbStickyKeysMask	|
			XkbMouseKeysMask	|
			XkbMouseKeysAccelMask	|
			XkbAccessXKeysMask	|
			XkbAccessXTimeoutMask	|
			XkbAccessXFeedbackMask	|
			XkbControlsEnabledMask,
			desc);
	XSync (GDK_DISPLAY (), FALSE);
	gdk_error_trap_pop ();
	we_are_changing_xkb_state = FALSE;
}

static void
set_gconf_from_server (GConfEntry *ignored)
{
	gboolean	in_gconf;
	GConfClient	*client = gconf_client_get_default ();
	GConfChangeSet *cs = gconf_change_set_new ();
	XkbDescRec	*desc = get_xkb_desc_rec ();
	gboolean changed = FALSE;

	if (!desc) {
		d ("No XKB present\n");
		return;
	}

	/*
	fprintf (stderr, "changed to : 0x%x\n", desc->ctrls->enabled_ctrls);
	fprintf (stderr, "changed to : 0x%x (2)\n", desc->ctrls->ax_options);
	*/

	/* guard against reloading the server when gconf notices that the state
	 * has changed and calls us.
	 */
	g_return_if_fail (!we_are_changing_xkb_state);
	we_are_changing_xkb_state = TRUE;

	/* always toggle this irrespective of the state */
	changed |= set_bool (client, cs, TRUE, CONFIG_ROOT "/enable",
		desc->ctrls->enabled_ctrls & (XkbAccessXKeysMask | XkbAccessXFeedbackMask));

	/* if master is disabled in gconf do not change gconf state of subordinates
	 * to match the server.  However, we should enable the master if one of the subordinates
	 * get enabled.
	 */
	in_gconf = gconf_client_get_bool (client, CONFIG_ROOT "/enable", NULL);

	changed |= set_bool (client, cs, in_gconf, CONFIG_ROOT "/feature_state_change_beep",
		desc->ctrls->ax_options & (XkbAX_FeatureFBMask | XkbAX_SlowWarnFBMask));
	changed |= set_bool (client, cs, in_gconf, CONFIG_ROOT "/timeout_enable",
		desc->ctrls->enabled_ctrls & XkbAccessXTimeoutMask);
	changed |= set_int (client, cs, CONFIG_ROOT "/timeout",
		desc->ctrls->ax_timeout);

	changed |= set_bool (client, cs, in_gconf, CONFIG_ROOT "/bouncekeys_enable",
		desc->ctrls->enabled_ctrls & XkbBounceKeysMask);
	changed |= set_int (client, cs, CONFIG_ROOT "/bouncekeys_delay",
		desc->ctrls->debounce_delay);
	changed |= set_bool (client, cs, TRUE, CONFIG_ROOT "/bouncekeys_beep_reject",
		desc->ctrls->ax_options & XkbAX_BKRejectFBMask);

	changed |= set_bool (client, cs, in_gconf, CONFIG_ROOT "/mousekeys_enable",
		desc->ctrls->enabled_ctrls & XkbMouseKeysMask);
	changed |= set_int (client, cs, CONFIG_ROOT "/mousekeys_max_speed",
		desc->ctrls->mk_max_speed * (1000 / desc->ctrls->mk_interval));
	/* NOTE : mk_time_to_max is measured in events not time */
	changed |= set_int (client, cs, CONFIG_ROOT "/mousekeys_accel_time",
		desc->ctrls->mk_time_to_max * desc->ctrls->mk_interval);
	changed |= set_int (client, cs, CONFIG_ROOT "/mousekeys_init_delay",
		desc->ctrls->mk_delay);

	changed |= set_bool (client, cs, in_gconf, CONFIG_ROOT "/slowkeys_enable",
		desc->ctrls->enabled_ctrls & XkbSlowKeysMask);
	changed |= set_bool (client, cs, TRUE, CONFIG_ROOT "/slowkeys_beep_press",
		desc->ctrls->ax_options & XkbAX_SKPressFBMask);
	changed |= set_bool (client, cs, TRUE, CONFIG_ROOT "/slowkeys_beep_accept",
		desc->ctrls->ax_options & XkbAX_SKAcceptFBMask);
	changed |= set_bool (client, cs, TRUE, CONFIG_ROOT "/slowkeys_beep_reject",
		desc->ctrls->ax_options & XkbAX_SKRejectFBMask);
	changed |= set_int (client, cs, CONFIG_ROOT "/slowkeys_delay",
		desc->ctrls->slow_keys_delay);

	changed |= set_bool (client, cs, in_gconf, CONFIG_ROOT "/stickykeys_enable",
		desc->ctrls->enabled_ctrls & XkbStickyKeysMask);
	changed |= set_bool (client, cs, TRUE, CONFIG_ROOT "/stickykeys_two_key_off",
		desc->ctrls->ax_options & XkbAX_TwoKeysMask);
	changed |= set_bool (client, cs, TRUE, CONFIG_ROOT "/stickykeys_modifier_beep",
		desc->ctrls->ax_options & XkbAX_StickyKeysFBMask);

	changed |= set_bool (client, cs, in_gconf, CONFIG_ROOT "/togglekeys_enable",
		desc->ctrls->ax_options & XkbAX_IndicatorFBMask);

	if (changed) {
		gconf_client_commit_change_set (client, cs, FALSE, NULL);
		gconf_client_suggest_sync (client, NULL);
	}
	gconf_change_set_unref (cs);
	we_are_changing_xkb_state = FALSE;
}

static GdkFilterReturn 
cb_xkb_event_filter (GdkXEvent *xevent, GdkEvent *ignored1, gpointer ignored2)
{
	XEvent   *xev   = (XEvent *) xevent;
	XkbEvent *xkbEv = (XkbEvent *) xevent;
	if (xev->xany.type == (xkbEventBase + XkbEventCode) &&
	    xkbEv->any.xkb_type == XkbControlsNotify) {
		if (!we_are_changing_xkb_state) {
			d ("Someone changed XKB state\n");
			set_gconf_from_server (NULL);
		} else
			d ("We changed XKB state\n");
	}

	return GDK_FILTER_CONTINUE;
}

void
gnome_settings_accessibility_keyboard_load (GConfClient *client)
{
	static gboolean has_filter = FALSE;
	if (!xkb_enabled ())
		return;

	/* be sure to init before starting to monitor the server */
	set_server_from_gconf (NULL);

	/* be careful not to install multipled filters */
	if (has_filter)
		return;

	gdk_error_trap_push ();
	XkbSelectEvents (GDK_DISPLAY (),
		XkbUseCoreKbd, XkbControlsNotifyMask, XkbControlsNotifyMask);

	XSync (GDK_DISPLAY (), FALSE);
	gdk_error_trap_pop ();

	gdk_window_add_filter (NULL, &cb_xkb_event_filter, NULL);
}

#else

void
gnome_settings_accessibility_keyboard_load (GConfClient *client)
{
	g_warning ("Unsupported in this build");
}
#endif

void
gnome_settings_accessibility_keyboard_init (GConfClient *client)
{
	gnome_settings_daemon_register_callback (CONFIG_ROOT, &set_server_from_gconf);
}
