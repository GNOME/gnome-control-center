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

static Applier *applier;
static Preferences *prefs;

static void
background_callback (GConfEntry *entry) 
{
	preferences_merge_entry (prefs, entry);
	applier_apply_prefs (applier, PREFERENCES (prefs));
}

void
gnome_settings_background_init (GConfEngine *engine)
{
	applier = APPLIER (applier_new (APPLIER_ROOT));

	prefs = preferences_new ();
	preferences_load (PREFERENCES (prefs));

	gnome_settings_daemon_register_callback ("/desktop/gnome/peripherals/keyboard", background_callback);
}

void
gnome_settings_background_load (GConfEngine *engine)
{
	applier_apply_prefs (applier, PREFERENCES (prefs));
}
