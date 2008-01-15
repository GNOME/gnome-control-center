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
#include <libgnomeui/gnome-bg.h>
#include <X11/Xatom.h>
#include <string.h>

#include "gnome-settings-module.h"

#include "preferences.h"

typedef struct _GnomeSettingsModuleBackground      GnomeSettingsModuleBackground;
typedef struct _GnomeSettingsModuleBackgroundClass GnomeSettingsModuleBackgroundClass;

struct _GnomeSettingsModuleBackground {
	GnomeSettingsModule parent;

	BGPreferences *prefs;
        GnomeBG *bg;
        guint timeout_id;
};

static gboolean nautilus_is_running (void);

struct _GnomeSettingsModuleBackgroundClass {
	GnomeSettingsModuleClass parent_class;
};

static void gnome_settings_module_background_class_init (GnomeSettingsModuleBackgroundClass *klass);
static void gnome_settings_module_background_init (GnomeSettingsModuleBackground *module);

static GnomeSettingsModuleRunlevel gnome_settings_module_background_get_runlevel (GnomeSettingsModule *module);
static gboolean gnome_settings_module_background_initialize (GnomeSettingsModule *module, GConfClient *config_client);
static gboolean gnome_settings_module_background_start (GnomeSettingsModule *module);

static gboolean
apply_prefs (gpointer data)
{
	GnomeSettingsModuleBackground *module;

	module = (GnomeSettingsModuleBackground *) data;

        if (!nautilus_is_running()) {
                GdkDisplay *display;
                int n_screens, i;
                GnomeBGPlacement placement;
                GnomeBGColorType color;
                const char *uri;
 
                display = gdk_display_get_default ();
                n_screens = gdk_display_get_n_screens (display);
 
                uri = module->prefs->wallpaper_filename;
 
                placement = GNOME_BG_PLACEMENT_TILED;
 
                switch (module->prefs->wallpaper_type) {
                case WPTYPE_TILED:
                        placement = GNOME_BG_PLACEMENT_TILED;
                        break;
                case WPTYPE_CENTERED:
                        placement = GNOME_BG_PLACEMENT_CENTERED;
                        break;
                case WPTYPE_SCALED:
                        placement = GNOME_BG_PLACEMENT_SCALED;
                        break;
                case WPTYPE_STRETCHED:
                        placement = GNOME_BG_PLACEMENT_FILL_SCREEN;
                        break;
                case WPTYPE_ZOOM:
                        placement = GNOME_BG_PLACEMENT_ZOOMED;
                        break;
                case WPTYPE_NONE:
                case WPTYPE_UNSET:
                        uri = NULL;
                        break;
                }
 
                switch (module->prefs->orientation) {
                case ORIENTATION_SOLID:
                        color = GNOME_BG_COLOR_SOLID;
                        break;
                case ORIENTATION_HORIZ:
                        color = GNOME_BG_COLOR_H_GRADIENT;
                        break;
                case ORIENTATION_VERT:
                        color = GNOME_BG_COLOR_V_GRADIENT;
                        break;
                default:
                        color = GNOME_BG_COLOR_SOLID;
                        break;
                }
 
                gnome_bg_set_uri (module->bg, uri);
                gnome_bg_set_placement (module->bg, placement);
                gnome_bg_set_color (module->bg, color, module->prefs->color1, module->prefs->color2);
 
                for (i = 0; i < n_screens; ++i) {
                        GdkScreen *screen;
                        GdkWindow *root_window;
                        GdkPixmap *pixmap;
 
                        screen = gdk_display_get_screen (display, i);
 
                        root_window = gdk_screen_get_root_window (screen);
 
                        pixmap = gnome_bg_create_pixmap (module->bg, root_window,
                                                         gdk_screen_get_width (screen),
                                                         gdk_screen_get_height (screen),
                                                         TRUE);

                        gnome_bg_set_pixmap_as_root (screen, pixmap);

                        g_object_unref (pixmap);
                }
        }

	return FALSE;
}

static void
queue_apply (gpointer data)
{
	GnomeSettingsModuleBackground *module;

	module = (GnomeSettingsModuleBackground *) data;
	if (module->timeout_id) {
		g_source_remove (module->timeout_id);
	}

	module->timeout_id = g_timeout_add (100, apply_prefs, data);
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

	queue_apply (user_data);
}

static void
on_bg_changed (GnomeBG *bg,
               gpointer user_data)
{
	queue_apply (user_data);
}


static void
gnome_settings_module_background_class_init (GnomeSettingsModuleBackgroundClass *klass)
{
	GnomeSettingsModuleClass *module_class;

	module_class = (GnomeSettingsModuleClass *) klass;
	module_class->get_runlevel = gnome_settings_module_background_get_runlevel;
	module_class->initialize = gnome_settings_module_background_initialize;
	module_class->start = gnome_settings_module_background_start;
}

static void
gnome_settings_module_background_init (GnomeSettingsModuleBackground *module)
{
	module->timeout_id = 0;
}

GType
gnome_settings_module_background_get_type (void)
{
	static GType module_type = 0;

	if (!module_type) {
		const GTypeInfo module_info = {
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

	module_bg = (GnomeSettingsModuleBackground *) module;

	module_bg->prefs = BG_PREFERENCES (bg_preferences_new ());
	module_bg->bg = gnome_bg_new ();

        g_signal_connect (module_bg->bg, "changed", G_CALLBACK (on_bg_changed), module_bg);
	bg_preferences_load (module_bg->prefs);

        apply_prefs (module_bg);

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
		return TRUE;

	apply_prefs (module_bg);

	return TRUE;
}

static gboolean
nautilus_is_running (void)
{
	Atom window_id_atom;
	Window nautilus_xid;
	Atom actual_type;
	int actual_format;
	unsigned long nitems, bytes_after;
	unsigned char *data;
	int retval;
	Atom wmclass_atom;
	gboolean running;
	gint error;

	window_id_atom = XInternAtom (GDK_DISPLAY (),
				      "NAUTILUS_DESKTOP_WINDOW_ID", True);

	if (window_id_atom == None) return FALSE;

	retval = XGetWindowProperty (GDK_DISPLAY (), GDK_ROOT_WINDOW (),
				     window_id_atom, 0, 1, False, XA_WINDOW,
				     &actual_type, &actual_format, &nitems,
				     &bytes_after, &data);

	if (data != NULL) {
		nautilus_xid = *(Window *) data;
		XFree (data);
	} else {
		return FALSE;
	}

	if (actual_type != XA_WINDOW) return FALSE;
	if (actual_format != 32) return FALSE;

	wmclass_atom = XInternAtom (GDK_DISPLAY (), "WM_CLASS", False);

	gdk_error_trap_push ();

	retval = XGetWindowProperty (GDK_DISPLAY (), nautilus_xid,
				     wmclass_atom, 0, 24, False, XA_STRING,
				     &actual_type, &actual_format, &nitems,
				     &bytes_after, &data);

	error = gdk_error_trap_pop ();

	if (error == BadWindow) return FALSE;

	if (actual_type == XA_STRING &&
	    nitems == 24 &&
	    bytes_after == 0 &&
	    actual_format == 8 &&
	    data != NULL &&
	    !strcmp ((char *)data, "desktop_window") &&
	    !strcmp ((char *)data + strlen ((char *)data) + 1, "Nautilus"))
		running = TRUE;
	else
		running = FALSE;

	if (data != NULL)
		XFree (data);

	return running;
}

