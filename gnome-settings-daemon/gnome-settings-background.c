/* -*- mode: c; style: linux -*- */

/* gnome-settings-background.c
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
#include <gconf/gconf.h>

#include "gnome-settings-keyboard.h"
#include "gnome-settings-daemon.h"

#include "preferences.h"
#include "applier.h"

#ifdef HAVE_GTK_MULTIHEAD
static BGApplier **bg_appliers;
#else
static BGApplier *bg_applier;
#endif
static BGPreferences *prefs;

static void
background_callback (GConfEntry *entry) 
{
#ifdef HAVE_GTK_MULTIHEAD
	int i;
#endif

	bg_preferences_merge_entry (prefs, entry);

#ifdef HAVE_GTK_MULTIHEAD
	for (i = 0; bg_appliers [i]; i++)
		bg_applier_apply_prefs (bg_appliers [i], prefs);
#else
	bg_applier_apply_prefs (bg_applier, prefs);
#endif
}

void
gnome_settings_background_init (GConfClient *client)
{
#ifdef HAVE_GTK_MULTIHEAD
	GdkDisplay *display;
	int         n_screens;
	int         i;

	display = gdk_display_get_default ();
	n_screens = gdk_display_get_n_screens (display);

	bg_appliers = g_new (BGApplier *, n_screens + 1);

	for (i = 0; i < n_screens; i++) {
		GdkScreen *screen;

		screen = gdk_display_get_screen (display, i);

		bg_appliers [i] = BG_APPLIER (bg_applier_new_for_screen (BG_APPLIER_ROOT, screen));
	}
	bg_appliers [i] = NULL;
#else
	bg_applier = BG_APPLIER (bg_applier_new (BG_APPLIER_ROOT));
#endif

	prefs = BG_PREFERENCES (bg_preferences_new ());
	bg_preferences_load (prefs);

	gnome_settings_daemon_register_callback ("/desktop/gnome/background", background_callback);
}

void
gnome_settings_background_load (GConfClient *client)
{
#ifdef HAVE_GTK_MULTIHEAD
	int i;
#endif

	/* If this is set, nautilus will draw the background and is
	 * almost definitely in our session.  however, it may not be
	 * running yet (so is_nautilus_running() will fail).  so, on
	 * startup, just don't do anything if this key is set so we
	 * don't waste time setting the background only to have
	 * nautilus overwrite it.
	 */

	if (gconf_client_get_bool (client, "/apps/nautilus/preferences/show_desktop", NULL))
		return;

#ifdef HAVE_GTK_MULTIHEAD
	for (i = 0; bg_appliers [i]; i++)
		bg_applier_apply_prefs (bg_appliers [i], prefs);
#else
	bg_applier_apply_prefs (bg_applier, prefs);
#endif
}
