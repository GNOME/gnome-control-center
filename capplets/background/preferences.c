/* -*- mode: c; style: linux -*- */

/* preferences.c
 * Copyright (C) 2001 Ximian, Inc.
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

#include <stdlib.h>

#include <gnome.h>
#include <bonobo.h>

#include "preferences.h"

static GObjectClass *parent_class;

static void      preferences_init             (Preferences               *prefs,
					       PreferencesClass          *class);
static void      preferences_class_init       (PreferencesClass          *class);

static void      preferences_finalize         (GObject                   *object);

static GdkColor *read_color_from_string       (const gchar               *string);

GType
preferences_get_type (void)
{
	static GType preferences_type = 0;

	if (!preferences_type) {
		GTypeInfo preferences_info = {
			sizeof (PreferencesClass),
			NULL,
			NULL,
			(GClassInitFunc) preferences_class_init,
			NULL,
			NULL,
			sizeof (Preferences),
			0,
			(GInstanceInitFunc) preferences_init,
		};

		preferences_type = 
			g_type_register_static (G_TYPE_OBJECT, "Preferences", &preferences_info, 0);
	}

	return preferences_type;
}

static void
preferences_init (Preferences *prefs, PreferencesClass *class)
{
	prefs->frozen             = FALSE;

	/* Load default values */
	prefs->color1             = read_color_from_string ("#39374b");
	prefs->color2             = read_color_from_string ("#42528f");
	prefs->enabled            = TRUE;
	prefs->wallpaper_enabled  = FALSE;
	prefs->gradient_enabled   = TRUE;
	prefs->orientation        = ORIENTATION_VERT;
	prefs->wallpaper_type     = WPTYPE_TILED;
	prefs->wallpaper_filename = NULL;
	prefs->wallpaper_sel_path = g_strdup (g_get_home_dir ());
	prefs->auto_apply         = TRUE;
	prefs->wallpapers         = NULL;
	prefs->adjust_opacity     = TRUE;
	prefs->opacity            = 255;
}

static void
preferences_class_init (PreferencesClass *class) 
{
	GObjectClass *object_class;

	object_class = (GObjectClass *) class;
	object_class->finalize = preferences_finalize;

	parent_class = 
		G_OBJECT_CLASS (g_type_class_ref (G_TYPE_OBJECT));
}

GObject *
preferences_new (void) 
{
	GObject *object;

	object = g_object_new (preferences_get_type (), NULL);
	PREFERENCES (object)->enabled = TRUE;

	return object;
}

GObject *
preferences_clone (const Preferences *prefs)
{
	GObject *object;
	Preferences *new_prefs;

	g_return_val_if_fail (prefs != NULL, NULL);
	g_return_val_if_fail (IS_PREFERENCES (prefs), NULL);

	object = preferences_new ();

	new_prefs = PREFERENCES (object);

	new_prefs->enabled            = prefs->enabled;
	new_prefs->gradient_enabled   = prefs->gradient_enabled;
	new_prefs->wallpaper_enabled  = prefs->wallpaper_enabled;
	new_prefs->orientation        = prefs->orientation;
	new_prefs->wallpaper_type     = prefs->wallpaper_type;

	if (prefs->color1)
		new_prefs->color1 = gdk_color_copy (prefs->color1);
	if (prefs->color2)
		new_prefs->color2 = gdk_color_copy (prefs->color2);

	new_prefs->wallpaper_filename = g_strdup (prefs->wallpaper_filename);
	new_prefs->wallpaper_sel_path = g_strdup (prefs->wallpaper_sel_path);;

	new_prefs->auto_apply         = prefs->auto_apply;
	new_prefs->adjust_opacity     = prefs->adjust_opacity;
	new_prefs->opacity            = prefs->opacity;

	return object;
}

static void
preferences_finalize (GObject *object) 
{
	Preferences *prefs;

	g_return_if_fail (object != NULL);
	g_return_if_fail (IS_PREFERENCES (object));

	prefs = PREFERENCES (object);

	g_free (prefs->wallpaper_filename);
	g_free (prefs->wallpaper_sel_path);

	parent_class->finalize (object);
}

void
preferences_load (Preferences *prefs)
{
	GConfEngine *engine;
	GError      *error;

	g_return_if_fail (prefs != NULL);
	g_return_if_fail (IS_PREFERENCES (prefs));

	engine = gconf_engine_get_default ();

	prefs->enabled = gconf_engine_get_bool (engine, "/background-properties/enabled", &error);
	prefs->wallpaper_type = gconf_engine_get_int (engine, "/background-properties/wallpaper-type", &error);
	prefs->wallpaper_filename = gconf_engine_get_string (engine, "/background-properties/wallpaper-filename", &error);
	prefs->wallpaper_enabled = gconf_engine_get_bool (engine, "/background-properties/wallpaper-enabled", &error);
	prefs->color1 = read_color_from_string (gconf_engine_get_string (engine, "/background-properties/color1", &error));
	prefs->color2 = read_color_from_string (gconf_engine_get_string (engine, "/background-properties/color2", &error));
	prefs->opacity = gconf_engine_get_int (engine, "/background-properties/opacity", &error);
	if (prefs->opacity >= 100 || prefs->opacity < 0)
		prefs->adjust_opacity = FALSE;

	prefs->orientation = gconf_engine_get_int (engine, "/background-properties/orientation", &error);

	if (prefs->orientation == ORIENTATION_SOLID)
		prefs->gradient_enabled = FALSE;
	else
		prefs->gradient_enabled = TRUE;
}

/* Parse the event name given (the event being notification of a property having
 * changed and apply that change to the preferences structure. Eliminates the
 * need to reload the structure entirely on every event notification
 */

void
preferences_merge_entry (Preferences      *prefs,
			 const GConfEntry *entry)
{
	const gchar *name;
	const GConfValue *value = gconf_entry_get_value (entry);

	g_return_if_fail (prefs != NULL);
	g_return_if_fail (IS_PREFERENCES (prefs));

	if (!strcmp (entry->key, "/background-properties/wallpaper_type")) {
		prefs->wallpaper_type = gconf_value_get_int (value);
	}
	else if (!strcmp (entry->key, "/background-properties/wallpaper_filename")) {
		prefs->wallpaper_filename = g_strdup (gconf_value_get_string (value));

		if (prefs->wallpaper_filename != NULL &&
		    strcmp (prefs->wallpaper_filename, "") != 0 &&
		    strcmp (prefs->wallpaper_filename, "(none)") != 0)
			prefs->wallpaper_enabled = TRUE;
		else
			prefs->wallpaper_enabled = FALSE;
	}
	else if (!strcmp (entry->key, "/background-properties/color1")) {
		prefs->color1 = read_color_from_string (gconf_value_get_string (value));
	}
	else if (!strcmp (entry->key, "/background-properties/color2")) {
		prefs->color2 = read_color_from_string (gconf_value_get_string (value));
	}
	else if (!strcmp (entry->key, "/background-properties/opacity")) {
		prefs->opacity = gconf_value_get_int (value);

		if (prefs->opacity >= 100)
			prefs->adjust_opacity = FALSE;
	}
	else if (!strcmp (entry->key, "/background-properties/orientation")) {
		prefs->orientation = gconf_value_get_int (value);

		if (prefs->orientation == ORIENTATION_SOLID)
			prefs->gradient_enabled = FALSE;
		else
			prefs->gradient_enabled = TRUE;
	}
	else if (!strcmp (entry->key, "/background-properties/wallpaper_enabled")) {
		prefs->wallpaper_enabled = gconf_value_get_bool (value);
	} else {
		g_warning ("%s: Unknown property: %s", __FUNCTION__, name);
	}
}

static GdkColor *
read_color_from_string (const gchar *string) 
{
	GdkColor *color;
	gint32 rgb;

	color = g_new0 (GdkColor, 1);

	gdk_color_parse (string, color);
	rgb = ((color->red >> 8) << 16) ||
		((color->green >> 8) << 8) ||
		(color->blue >> 8);
	color->pixel = gdk_rgb_xpixel_from_rgb (rgb);

	return color;
}
