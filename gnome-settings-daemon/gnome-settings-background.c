/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
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
#include "gnome-settings-module.h"

#include "preferences.h"
#include "applier.h"

typedef struct _GnomeSettingsModuleBackground      GnomeSettingsModuleBackground;
typedef struct _GnomeSettingsModuleBackgroundClass GnomeSettingsModuleBackgroundClass;

struct _GnomeSettingsModuleBackground {
	GnomeSettingsModule parent;

	BGApplier **bg_appliers;
	BGPreferences *prefs;
	guint applier_idle_id;
};

struct _GnomeSettingsModuleBackgroundClass {
	GnomeSettingsModuleClass parent_class;
};

static void gnome_settings_module_background_class_init (GnomeSettingsModuleBackgroundClass *klass);
static void gnome_settings_module_background_init (GnomeSettingsModuleBackground *module);
static gboolean gnome_settings_module_background_initialize (GnomeSettingsModule *module, GConfClient *config_client);
static gboolean gnome_settings_module_background_start (GnomeSettingsModule *module);
static GnomeSettingsModuleRunlevel gnome_settings_module_background_get_runlevel (GnomeSettingsModule *module);

static gboolean
applier_idle (gpointer data)
{
	GnomeSettingsModuleBackground *module;
	int i;
	
	module = (GnomeSettingsModuleBackground *) data;
	
	for (i = 0; module->bg_appliers [i]; i++)
		bg_applier_apply_prefs (module->bg_appliers [i], module->prefs);
	module->applier_idle_id = 0;
	return FALSE;
}

static void
background_callback (GConfClient *client,
                     guint cnxn_id,
                     GConfEntry *entry,
                     gpointer user_data) 
{
	GnomeSettingsModuleBackground *module_bg;
	
	module_bg = (GnomeSettingsModuleBackground *) user_data;
	
	bg_preferences_merge_entry (module_bg->prefs, entry);

	if (module_bg->applier_idle_id != 0) {
		g_source_remove (module_bg->applier_idle_id);
	}

	module_bg->applier_idle_id = g_timeout_add (100, applier_idle, NULL);
}

static void
gnome_settings_module_background_class_init (GnomeSettingsModuleBackgroundClass *klass)
{
	GnomeSettingsModuleClass *module_class;
	
	module_class = (GnomeSettingsModuleClass *) klass;
	module_class->initialize = gnome_settings_module_background_initialize;
	module_class->start = gnome_settings_module_background_start;
	module_class->get_runlevel = gnome_settings_module_background_get_runlevel;
}

static void
gnome_settings_module_background_init (GnomeSettingsModuleBackground *module)
{
	module->applier_idle_id = 0;
}

GType
gnome_settings_module_background_get_type (void)
{
	static GType module_type = 0;
  
	if (!module_type) {
		static const GTypeInfo module_info = {
			sizeof (GnomeSettingsModuleBackgroundClass),
			NULL,		/* base_init */
			NULL,		/* base_finalize */
			(GClassInitFunc) gnome_settings_module_background_class_init,
			NULL,		/* class_finalize */
			NULL,		/* class_data */
			sizeof (GnomeSettingsModuleBackground),
			0,		/* n_preallocs */
			(GInstanceInitFunc) gnome_settings_module_background_init,
		};
      
		module_type = g_type_register_static (GNOME_SETTINGS_TYPE_MODULE,
						      "GnomeSettingsModuleBackground",
						      &module_info, 0);
	}
  
	return module_type;
}

static GnomeSettingsModuleRunlevel
gnome_settings_module_background_get_runlevel (GnomeSettingsModule *module)
{
	return GNOME_SETTINGS_MODULE_RUNLEVEL_GNOME_SETTINGS;
}

static gboolean
gnome_settings_module_background_initialize (GnomeSettingsModule *module,
					     GConfClient *config_client)
{
	GnomeSettingsModuleBackground *module_bg;
	GdkDisplay *display;
	int         n_screens;
	int         i;

	module_bg = (GnomeSettingsModuleBackground *) module;
	display = gdk_display_get_default ();
	n_screens = gdk_display_get_n_screens (display);

	module_bg->bg_appliers = g_new (BGApplier *, n_screens + 1);

	for (i = 0; i < n_screens; i++) {
		GdkScreen *screen;

		screen = gdk_display_get_screen (display, i);

		module_bg->bg_appliers [i] = BG_APPLIER (bg_applier_new_for_screen (BG_APPLIER_ROOT, screen));
	}
	module_bg->bg_appliers [i] = NULL;

	module_bg->prefs = BG_PREFERENCES (bg_preferences_new ());
	bg_preferences_load (module_bg->prefs);

	gconf_client_notify_add (config_client,
	                         "/desktop/gnome/background", 
	                         background_callback,
	                         module,
	                         NULL,
	                         NULL);
	
	return TRUE;
}

static gboolean
gnome_settings_module_background_start (GnomeSettingsModule *module)
{
	GnomeSettingsModuleBackground *module_bg;
	int i;

	module_bg = (GnomeSettingsModuleBackground *) module;
	
	/* If this is set, nautilus will draw the background and is
	 * almost definitely in our session.  however, it may not be
	 * running yet (so is_nautilus_running() will fail).  so, on
	 * startup, just don't do anything if this key is set so we
	 * don't waste time setting the background only to have
	 * nautilus overwrite it.
	 */

	if (gconf_client_get_bool (gnome_settings_module_get_config_client (module),
				   "/apps/nautilus/preferences/show_desktop", NULL))
		return FALSE;

	for (i = 0; module_bg->bg_appliers [i]; i++)
		bg_applier_apply_prefs (module_bg->bg_appliers [i], module_bg->prefs);
	
	return TRUE;
}
