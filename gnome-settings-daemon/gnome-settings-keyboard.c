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

#include "gnome-settings-keyboard-xkb.h"
#include "gnome-settings-module.h"
#include "gnome-settings-xmodmap.h"
#include "utils.h"

#ifdef HAVE_X11_EXTENSIONS_XF86MISC_H
#  include <X11/extensions/xf86misc.h>
#endif
#ifdef HAVE_X11_EXTENSIONS_XKB_H
#include <X11/XKBlib.h>
#include <X11/keysym.h>
#endif

#include <string.h>
#include <unistd.h>

typedef struct {
	GnomeSettingsModule parent;
} GnomeSettingsModuleKeyboard;

typedef struct {
	GnomeSettingsModuleClass parent_class;
} GnomeSettingsModuleKeyboardClass;

static GnomeSettingsModuleRunlevel gnome_settings_module_keyboard_get_runlevel (GnomeSettingsModule *module);
static gboolean gnome_settings_module_keyboard_initialize (GnomeSettingsModule *module, GConfClient *config_client);
static gboolean gnome_settings_module_keyboard_start (GnomeSettingsModule *module);
static gboolean gnome_settings_module_keyboard_stop (GnomeSettingsModule *module);

static void
gnome_settings_module_keyboard_class_init (GnomeSettingsModuleKeyboardClass *klass)
{
	GnomeSettingsModuleClass *module_class;

	module_class = (GnomeSettingsModuleClass *) klass;
	module_class->get_runlevel = gnome_settings_module_keyboard_get_runlevel;
	module_class->initialize = gnome_settings_module_keyboard_initialize;
	module_class->start = gnome_settings_module_keyboard_start;
	module_class->stop = gnome_settings_module_keyboard_stop;
}

static void
gnome_settings_module_keyboard_init (GnomeSettingsModuleKeyboard *module)
{
}

GType
gnome_settings_module_keyboard_get_type (void)
{
	static GType module_type = 0;

	if (!module_type) {
		static const GTypeInfo module_info = {
			sizeof (GnomeSettingsModuleKeyboardClass),
			NULL,		/* base_init */
			NULL,		/* base_finalize */
			(GClassInitFunc) gnome_settings_module_keyboard_class_init,
			NULL,		/* class_finalize */
			NULL,		/* class_data */
			sizeof (GnomeSettingsModuleKeyboard),
			0,		/* n_preallocs */
			(GInstanceInitFunc) gnome_settings_module_keyboard_init,
		};

		module_type = g_type_register_static (GNOME_SETTINGS_TYPE_MODULE,
						      "GnomeSettingsModuleKeyboard",
						      &module_info, 0);
	}

	return module_type;
}

static GnomeSettingsModuleRunlevel
gnome_settings_module_keyboard_get_runlevel (GnomeSettingsModule *module)
{
	return GNOME_SETTINGS_MODULE_RUNLEVEL_XSETTINGS;
}

#ifdef HAVE_X11_EXTENSIONS_XF86MISC_H
static gboolean
xfree86_set_keyboard_autorepeat_rate (int delay, int rate)
{
	gboolean res = FALSE;
	int event_base_return, error_base_return;

	if (XF86MiscQueryExtension (GDK_DISPLAY (),
				    &event_base_return,
				    &error_base_return) == True) {
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
	int interval = (rate <= 0) ? 1000000 : 1000/rate;
	if (delay <= 0)
		delay = 1;
	return XkbSetAutoRepeatRate (GDK_DISPLAY (), XkbUseCoreKbd,
				     delay, interval);
}
#endif

#define GSD_KEYBOARD_KEY "/desktop/gnome/peripherals/keyboard"

static char *
gsd_keyboard_get_hostname_key (const char *subkey)
{
#ifdef HOST_NAME_MAX
	char hostname[HOST_NAME_MAX + 1];
#else
	char hostname[256];
#endif
  
	if (gethostname (hostname, sizeof (hostname)) == 0 &&
	    strcmp (hostname, "localhost") != 0 &&
	    strcmp (hostname, "localhost.localdomain") != 0) {
		char *key = g_strconcat (GSD_KEYBOARD_KEY
		                         "/host-",
		                         hostname,
		                         "/0/",
		                         subkey,
		                         (char *)NULL);
		return key;
	} else
		return NULL;
}

#ifdef HAVE_X11_EXTENSIONS_XKB_H

enum {
	NUMLOCK_STATE_OFF = 0,
	NUMLOCK_STATE_ON = 1,
	NUMLOCK_STATE_UNKNOWN = 2
};

/* something fatal has happened so that it makes no
 * sense to try to remember anything.
 * that means: no calls to the set_state functions!
 */
static gboolean
numlock_setup_error = FALSE;

/* we didn't apply GConf settings yet
 * don't overwrite them with the initial state from
 * the newly started session!
 */
static gboolean
numlock_starting_up = TRUE;


static unsigned
numlock_NumLock_modifier_mask (void)
{
	Display *dpy = GDK_DISPLAY_XDISPLAY (gdk_display_get_default ());
	return XkbKeysymToModifiers (dpy, XK_Num_Lock);
}

static void
numlock_set_xkb_state (gboolean new_state)
{
	unsigned int num_mask;
	Display *dpy = GDK_DISPLAY_XDISPLAY (gdk_display_get_default ());
	if (new_state != NUMLOCK_STATE_ON && new_state != NUMLOCK_STATE_OFF)
		return;
	num_mask = numlock_NumLock_modifier_mask ();
	XkbLockModifiers (dpy, XkbUseCoreKbd, num_mask, new_state ? num_mask : 0);
}

static char *
numlock_gconf_state_key (void)
{
	char *key = gsd_keyboard_get_hostname_key ("numlock_on");
	if (!key) {
		numlock_setup_error = TRUE;
		g_warning ("numlock: Numlock remembering disabled because your hostname is set to \"localhost\".");
	}
	return key;
}

static int
numlock_get_gconf_state (void)
{
	int curr_state;
	GConfClient *gcc;
	GError *err = NULL;
	char *key = numlock_gconf_state_key ();
	if (!key) return NUMLOCK_STATE_UNKNOWN;
	gcc = gnome_settings_get_config_client ();
	curr_state = gconf_client_get_bool (gcc, key, &err);
	if (err) curr_state = NUMLOCK_STATE_UNKNOWN;
	g_clear_error (&err);
	g_free (key);
	return curr_state;
}

static void
numlock_set_gconf_state (gboolean new_state)
{
	char *key;
	GConfClient *gcc;
	if (new_state != NUMLOCK_STATE_ON && new_state != NUMLOCK_STATE_OFF)
		return;
	key = numlock_gconf_state_key ();
	if (!key) return;
	gcc = gnome_settings_get_config_client ();
	gconf_client_set_bool (gcc, key, new_state, NULL);
	g_free (key);
}

static GdkFilterReturn
numlock_xkb_callback (GdkXEvent *xev_, GdkEvent *gdkev_, gpointer xkb_event_code)
{
	XEvent *xev = (XEvent *)xev_;
	if (xev->type == GPOINTER_TO_INT (xkb_event_code)) {
		XkbEvent *xkbev = (XkbEvent *)xev;
		if (xkbev->any.xkb_type == XkbStateNotify)
		if (xkbev->state.changed & XkbModifierLockMask) {
			unsigned num_mask = numlock_NumLock_modifier_mask ();
			unsigned locked_mods = xkbev->state.locked_mods;
			int numlock_state = !! (num_mask & locked_mods);

			if (!numlock_starting_up && !numlock_setup_error)
				numlock_set_gconf_state (numlock_state);
		}
	}
	return GDK_FILTER_CONTINUE;
}

static void
numlock_install_xkb_callback (void)
{
	Display *dpy = GDK_DISPLAY_XDISPLAY (gdk_display_get_default ());
	int op_code = 0, xkb_event_code = 0;
	int error_code = 0, major = XkbMajorVersion, minor = XkbMinorVersion;
	int have_xkb = XkbQueryExtension (dpy,
	                                  &op_code, &xkb_event_code,
	                                  &error_code, &major, &minor);
	if (have_xkb != True) {
		numlock_setup_error = TRUE;
		g_warning ("numlock: XkbQueryExtension returned an error");
		return;
	}

	XkbSelectEventDetails (dpy,
	                       XkbUseCoreKbd,
	                       XkbStateNotifyMask,
	                       XkbModifierLockMask,
	                       XkbModifierLockMask);

	gdk_window_add_filter (NULL,
	                       numlock_xkb_callback,
	                       GINT_TO_POINTER (xkb_event_code));
}

#endif /* HAVE_X11_EXTENSIONS_XKB_H */

static void
apply_settings (void)
{
	GConfClient *client;

	gboolean repeat, click;
	int rate, delay;
	int click_volume, bell_volume, bell_pitch, bell_duration;
	char *volume_string;
#ifdef HAVE_X11_EXTENSIONS_XKB_H
	gboolean rnumlock;
#endif /* HAVE_X11_EXTENSIONS_XKB_H */

	XKeyboardControl kbdcontrol;

	client = gnome_settings_get_config_client ();

	repeat        = gconf_client_get_bool  (client, "/desktop/gnome/peripherals/keyboard/repeat",        NULL);
	click         = gconf_client_get_bool  (client, "/desktop/gnome/peripherals/keyboard/click",         NULL);
	rate          = gconf_client_get_int   (client, "/desktop/gnome/peripherals/keyboard/rate",          NULL);
	delay         = gconf_client_get_int   (client, "/desktop/gnome/peripherals/keyboard/delay",         NULL);
	click_volume  = gconf_client_get_int   (client, "/desktop/gnome/peripherals/keyboard/click_volume",  NULL);
#if 0
	bell_volume   = gconf_client_get_int   (client, "/desktop/gnome/peripherals/keyboard/bell_volume",   NULL);
#endif
	bell_pitch    = gconf_client_get_int   (client, "/desktop/gnome/peripherals/keyboard/bell_pitch",    NULL);
	bell_duration = gconf_client_get_int   (client, "/desktop/gnome/peripherals/keyboard/bell_duration", NULL);

	volume_string = gconf_client_get_string (client, "/desktop/gnome/peripherals/keyboard/bell_mode", NULL);
	bell_volume   = (volume_string && !strcmp (volume_string, "on")) ? 50 : 0;
	g_free (volume_string);
#ifdef HAVE_X11_EXTENSIONS_XKB_H
	rnumlock      = gconf_client_get_bool  (client, GSD_KEYBOARD_KEY "/remember_numlock_state", NULL);
#endif /* HAVE_X11_EXTENSIONS_XKB_H */

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
		if (!rate_set)
			g_warning ("Neither XKeyboard not Xfree86's keyboard extensions are available,\n"
				   "no way to support keyboard autorepeat rate settings");
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
	XChangeKeyboardControl (GDK_DISPLAY (), 
				KBKeyClickPercent | KBBellPercent | KBBellPitch | KBBellDuration,				
				&kbdcontrol);

	
#ifdef HAVE_X11_EXTENSIONS_XKB_H
	if (!numlock_setup_error && rnumlock)
		numlock_set_xkb_state (numlock_get_gconf_state ());
	numlock_starting_up = FALSE;
#endif /* HAVE_X11_EXTENSIONS_XKB_H */

	XSync (GDK_DISPLAY (), FALSE);
	gdk_error_trap_pop ();
}

static gboolean
gnome_settings_module_keyboard_initialize (GnomeSettingsModule *module, GConfClient *client)
{
	/* Essential - xkb initialization should happen before */
	gnome_settings_keyboard_xkb_set_post_activation_callback ((PostActivationCallback) gnome_settings_load_modmap_files, NULL);
	gnome_settings_keyboard_xkb_init (client);

	gnome_settings_register_config_callback (GSD_KEYBOARD_KEY, (GnomeSettingsConfigCallback) apply_settings);
#ifdef HAVE_X11_EXTENSIONS_XKB_H
	numlock_install_xkb_callback ();
#endif /* HAVE_X11_EXTENSIONS_XKB_H */

	return TRUE;
}

static gboolean
gnome_settings_module_keyboard_start (GnomeSettingsModule *module)
{
	/* Essential - xkb initialization should happen before */
	gnome_settings_keyboard_xkb_load (gnome_settings_module_get_config_client (module));

	apply_settings ();

	return TRUE;
}

static gboolean
gnome_settings_module_keyboard_stop (GnomeSettingsModule *module)
{
	return TRUE;
}
