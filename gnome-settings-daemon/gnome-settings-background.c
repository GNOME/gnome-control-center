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

static BGApplier *bg_applier;
static BGPreferences *prefs;

static void
background_callback (GConfEntry *entry) 
{
	bg_preferences_merge_entry (prefs, entry);
	bg_applier_apply_prefs (bg_applier, prefs);
}

void
gnome_settings_background_init (GConfClient *client)
{
	bg_applier = BG_APPLIER (bg_applier_new (BG_APPLIER_ROOT));

	prefs = BG_PREFERENCES (bg_preferences_new ());
	bg_preferences_load (prefs);

	gnome_settings_daemon_register_callback ("/desktop/gnome/background", background_callback);
}

void
gnome_settings_background_load (GConfClient *client)
{
	bg_applier_apply_prefs (bg_applier, prefs);
}
