/* -*- mode: c; style: linux -*- */

/* gnome-settings-display.c
 *
 * Most of this code comes from the old gnome-session/gsm-xrandr.c.
 *
 * Copyright (C) 2003 Red Hat, Inc.
 * Copyright (C) 2007 Novell, Inc.
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

#ifdef HAVE_RANDR
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include <X11/extensions/Xrandr.h>
#endif

#include "gnome-settings-module.h"

typedef struct {
	GnomeSettingsModule parent;
} GnomeSettingsModuleDisplay;

typedef struct {
	GnomeSettingsModuleClass parent_class;
} GnomeSettingsModuleDisplayClass;

static GnomeSettingsModuleRunlevel gnome_settings_module_xrandr_get_runlevel (GnomeSettingsModule *module);
static gboolean gnome_settings_module_xrandr_initialize (GnomeSettingsModule *module, GConfClient *config_client);
static gboolean gnome_settings_module_xrandr_start (GnomeSettingsModule *module);
static gboolean gnome_settings_module_xrandr_stop (GnomeSettingsModule *module);

static void
gnome_settings_module_xrandr_class_init (GnomeSettingsModuleDisplayClass *klass)
{
	GnomeSettingsModuleClass *module_class;

	module_class = (GnomeSettingsModuleClass *) klass;
	module_class->get_runlevel = gnome_settings_module_xrandr_get_runlevel;
	module_class->initialize = gnome_settings_module_xrandr_initialize;
	module_class->start = gnome_settings_module_xrandr_start;
	module_class->stop = gnome_settings_module_xrandr_stop;
}

static void
gnome_settings_module_xrandr_init (GnomeSettingsModuleDisplay *module)
{
}

GType
gnome_settings_module_xrandr_get_type (void)
{
	static GType module_type = 0;

	if (!module_type) {
		const GTypeInfo module_info = {
			sizeof (GnomeSettingsModuleDisplayClass),
			NULL,		/* base_init */
			NULL,		/* base_finalize */
			(GClassInitFunc) gnome_settings_module_xrandr_class_init,
			NULL,		/* class_finalize */
			NULL,		/* class_data */
			sizeof (GnomeSettingsModuleDisplay),
			0,		/* n_preallocs */
			(GInstanceInitFunc) gnome_settings_module_xrandr_init,
		};

		module_type = g_type_register_static (GNOME_SETTINGS_TYPE_MODULE,
						      "GnomeSettingsModuleDisplay",
						      &module_info, 0);
	}

	return module_type;
}

static GnomeSettingsModuleRunlevel
gnome_settings_module_xrandr_get_runlevel (GnomeSettingsModule *module)
{
	return GNOME_SETTINGS_MODULE_RUNLEVEL_XSETTINGS;
}

#ifdef HAVE_RANDR
static int
get_rotation (GConfClient *client, char *display, int screen)
{
	char *key;
	int val;
	GError *error = NULL;
	
	key = g_strdup_printf ("%s/%d/rotation", display, screen);
	val = gconf_client_get_int (client, key, &error);
	g_free (key);
	
	if (error == NULL)
		return val;
	
	g_error_free (error);

	return 0;
}

static int
get_resolution (GConfClient *gconf, int screen, char *keys[], int *width, int *height)
{
	int i;
	char *key;
	char *val;
	int w, h;
	
	val = NULL;
	for (i = 0; keys[i] != NULL; i++) {
		key = g_strdup_printf ("%s/%d/resolution", keys[i], screen);
		val = gconf_client_get_string (gconf, key, NULL);
		g_free (key);
		
		if (val != NULL)
			break;
	}
	
	if (val == NULL)
		return -1;
	
	if (sscanf (val, "%dx%d", &w, &h) != 2) {
		g_free (val);
		return -1;
	}
	
	g_free (val);
	
	*width = w;
	*height = h;
	
	return i;
}

static int
get_rate (GConfClient *gconf, char *display, int screen)
{
	char *key;
	int val;
	GError *error = NULL;
	
	key = g_strdup_printf ("%s/%d/rate", display, screen);
	val = gconf_client_get_int (gconf, key, &error);
	g_free (key);
	
	if (error == NULL)
		return val;
	
	g_error_free (error);

	return 0;
}

static int
find_closest_size (XRRScreenSize *sizes, int nsizes, int width, int height)
{
	int closest;
	int closest_width, closest_height;
	int i;
	
	closest = 0;
	closest_width = sizes[0].width;
	closest_height = sizes[0].height;
	for (i = 1; i < nsizes; i++) {
		if (ABS (sizes[i].width - width) < ABS (closest_width - width) ||
		    (sizes[i].width == closest_width &&
		     ABS (sizes[i].height - height) < ABS (closest_height - height))) {
			closest = i;
			closest_width = sizes[i].width;
			closest_height = sizes[i].height;
		}
	}
	
	return closest;
}

#endif /* HAVE_RANDR */

static void
apply_settings (GnomeSettingsModule *module)
{
#ifdef HAVE_RANDR
	GdkDisplay *display;
	Display *xdisplay;
	int major, minor;
	int event_base, error_base;
	GConfClient *gconf;
	int n_screens;
	GdkScreen *screen;
	GdkWindow *root_window;
	int width, height, rate, rotation;
#ifdef HOST_NAME_MAX
	char hostname[HOST_NAME_MAX + 1];
#else
	char hostname[256];
#endif
	char *specific_path;
	char *keys[3];
	int i, residx;
	
	display = gdk_display_get_default ();
	xdisplay = gdk_x11_display_get_xdisplay (display);
	
	/* Check if XRandR is supported on the display */
	if (!XRRQueryExtension (xdisplay, &event_base, &error_base) ||
	    XRRQueryVersion (xdisplay, &major, &minor) == 0)
		return;
	
	if (major != 1 || minor < 1) {
		g_message ("Display has unsupported version of XRandR (%d.%d), not setting resolution.", major, minor);
		return;
	}
	
	gconf = gnome_settings_module_get_config_client (module);
	
	i = 0;
	specific_path = NULL;
	if (gethostname (hostname, sizeof (hostname)) == 0) {
		specific_path = g_strconcat ("/desktop/gnome/screen/", hostname,  NULL);
		keys[i++] = specific_path;
	}
	keys[i++] = "/desktop/gnome/screen/default";
	keys[i++] = NULL;
	
	n_screens = gdk_display_get_n_screens (display);
	for (i = 0; i < n_screens; i++) {
		screen = gdk_display_get_screen (display, i);
		root_window = gdk_screen_get_root_window (screen);
		residx = get_resolution (gconf, i, keys, &width, &height);
		
		if (residx != -1) {
			XRRScreenSize *sizes;
			int nsizes, j;
			int closest;
			short *rates;
			int nrates;
			int status;
			int current_size;
			short current_rate;
			XRRScreenConfiguration *config;
			Rotation current_rotation;
			
			config = XRRGetScreenInfo (xdisplay, gdk_x11_drawable_get_xid (GDK_DRAWABLE (root_window)));
			
			rate = get_rate (gconf, keys[residx], i);
			
			sizes = XRRConfigSizes (config, &nsizes);
			closest = find_closest_size (sizes, nsizes, width, height);
			
			rates = XRRConfigRates (config, closest, &nrates);
			for (j = 0; j < nrates; j++) {
				if (rates[j] == rate)
					break;
			}

			/* Rate not supported, let X pick */
			if (j == nrates)
				rate = 0;
		
			rotation = get_rotation (gconf, keys[residx], i);
			if (rotation == 0)
				rotation = RR_Rotate_0;
	
			current_size = XRRConfigCurrentConfiguration (config, &current_rotation);
			current_rate = XRRConfigCurrentRate (config);
			
			if (closest != current_size ||
			    rate != current_rate ||
			    rotation != current_rotation) {
				status = XRRSetScreenConfigAndRate (xdisplay, 
				  				    config,
				  				    gdk_x11_drawable_get_xid (GDK_DRAWABLE (root_window)),
				  				    closest,
				  				    (Rotation) rotation,
				  				    rate,
				  				    GDK_CURRENT_TIME);
			}
			
			XRRFreeScreenConfigInfo (config);
		}
	}
	
	g_free (specific_path);
	
	/* We need to make sure we process the screen resize event. */
	gdk_display_sync (display);

	while (gtk_events_pending ())
		gtk_main_iteration();

#endif /* HAVE_RANDR */
}

static gboolean
gnome_settings_module_xrandr_initialize (GnomeSettingsModule *module, GConfClient *config_client)
{
	return TRUE;
}

static gboolean
gnome_settings_module_xrandr_start (GnomeSettingsModule *module)
{
	apply_settings (module);

	return TRUE;
}

static gboolean
gnome_settings_module_xrandr_stop (GnomeSettingsModule *module)
{
	return TRUE;
}
