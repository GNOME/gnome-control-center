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

#include <string.h>
#include <stdlib.h>

#include <gnome.h>
#include <bonobo.h>

#include "preferences.h"

static GObjectClass *parent_class;

static void      bg_preferences_init       (BGPreferences      *prefs,
					    BGPreferencesClass *class);
static void      bg_preferences_class_init (BGPreferencesClass *class);

static void      bg_preferences_finalize   (GObject            *object);

static GdkColor *read_color_from_string    (const gchar        *string);

GType
bg_preferences_get_type (void)
{
	static GType bg_preferences_type = 0;

	if (!bg_preferences_type) {
		GTypeInfo bg_preferences_info = {
			sizeof (BGPreferencesClass),
			NULL,
			NULL,
			(GClassInitFunc) bg_preferences_class_init,
			NULL,
			NULL,
			sizeof (BGPreferences),
			0,
			(GInstanceInitFunc) bg_preferences_init,
		};

		bg_preferences_type = 
			g_type_register_static (G_TYPE_OBJECT, "BGPreferences", &bg_preferences_info, 0);
	}

	return bg_preferences_type;
}

static void
bg_preferences_init (BGPreferences      *prefs,
		     BGPreferencesClass *class)
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
bg_preferences_class_init (BGPreferencesClass *class) 
{
	GObjectClass *object_class;

	object_class = (GObjectClass *) class;
	object_class->finalize = bg_preferences_finalize;

	parent_class = 
		G_OBJECT_CLASS (g_type_class_ref (G_TYPE_OBJECT));
}

GObject *
bg_preferences_new (void) 
{
	GObject *object;

	object = g_object_new (bg_preferences_get_type (), NULL);
	BG_PREFERENCES (object)->enabled = TRUE;

	return object;
}

GObject *
bg_preferences_clone (const BGPreferences *prefs)
{
	GObject *object;
	BGPreferences *new_prefs;

	g_return_val_if_fail (prefs != NULL, NULL);
	g_return_val_if_fail (IS_BG_PREFERENCES (prefs), NULL);

	object = bg_preferences_new ();

	new_prefs = BG_PREFERENCES (object);

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
bg_preferences_finalize (GObject *object) 
{
	BGPreferences *prefs;

	g_return_if_fail (object != NULL);
	g_return_if_fail (IS_BG_PREFERENCES (object));

	prefs = BG_PREFERENCES (object);

	g_free (prefs->wallpaper_filename);
	g_free (prefs->wallpaper_sel_path);

	parent_class->finalize (object);
}

void
bg_preferences_load (BGPreferences *prefs)
{
	GConfClient *client;
	GError      *error = NULL;

	g_return_if_fail (prefs != NULL);
	g_return_if_fail (IS_BG_PREFERENCES (prefs));

	client = gconf_client_get_default ();

	prefs->enabled = gconf_client_get_bool (client, "/desktop/gnome/background/enabled", &error);
	prefs->wallpaper_type = gconf_client_get_int (client, "/desktop/gnome/background/wallpaper-type", &error);
	prefs->wallpaper_filename = gconf_client_get_string (client, "/desktop/gnome/background/wallpaper-filename", &error);
	prefs->wallpaper_enabled = gconf_client_get_bool (client, "/desktop/gnome/background/wallpaper-enabled", &error);
	prefs->color1 = read_color_from_string (gconf_client_get_string (client, "/desktop/gnome/background/color1", &error));
	prefs->color2 = read_color_from_string (gconf_client_get_string (client, "/desktop/gnome/background/color2", &error));
	prefs->opacity = gconf_client_get_int (client, "/desktop/gnome/background/opacity", &error);
	if (prefs->opacity >= 100 || prefs->opacity < 0)
		prefs->adjust_opacity = FALSE;

	prefs->orientation = gconf_client_get_int (client, "/desktop/gnome/background/orientation", &error);

	if (prefs->orientation == ORIENTATION_SOLID)
		prefs->gradient_enabled = FALSE;
	else
		prefs->gradient_enabled = TRUE;
}

/* Parse the event name given (the event being notification of a property having
 * changed and apply that change to the bg_preferences structure. Eliminates the
 * need to reload the structure entirely on every event notification
 */

void
bg_preferences_merge_entry (BGPreferences    *prefs,
			    const GConfEntry *entry)
{
	const GConfValue *value = gconf_entry_get_value (entry);

	g_return_if_fail (prefs != NULL);
	g_return_if_fail (IS_BG_PREFERENCES (prefs));

	if (!strcmp (entry->key, "/desktop/gnome/background/wallpaper_type")) {
		prefs->wallpaper_type = gconf_value_get_int (value);
	}
	else if (!strcmp (entry->key, "/desktop/gnome/background/wallpaper-filename")) {
		prefs->wallpaper_filename = g_strdup (gconf_value_get_string (value));

		if (prefs->wallpaper_filename != NULL &&
		    strcmp (prefs->wallpaper_filename, "") != 0 &&
		    strcmp (prefs->wallpaper_filename, "(none)") != 0)
			prefs->wallpaper_enabled = TRUE;
		else
			prefs->wallpaper_enabled = FALSE;
	}
	else if (!strcmp (entry->key, "/desktop/gnome/background/color1")) {
		prefs->color1 = read_color_from_string (gconf_value_get_string (value));
	}
	else if (!strcmp (entry->key, "/desktop/gnome/background/color2")) {
		prefs->color2 = read_color_from_string (gconf_value_get_string (value));
	}
	else if (!strcmp (entry->key, "/desktop/gnome/background/opacity")) {
		prefs->opacity = gconf_value_get_int (value);

		if (prefs->opacity >= 100)
			prefs->adjust_opacity = FALSE;
	}
	else if (!strcmp (entry->key, "/desktop/gnome/background/orientation")) {
		prefs->orientation = gconf_value_get_int (value);

		if (prefs->orientation == ORIENTATION_SOLID)
			prefs->gradient_enabled = FALSE;
		else
			prefs->gradient_enabled = TRUE;
	}
	else if (!strcmp (entry->key, "/desktop/gnome/background/wallpaper-enabled")) {
		if (gconf_value_get_bool (value) &&
				(prefs->wallpaper_filename != NULL) &&
		    strcmp (prefs->wallpaper_filename, "") != 0 &&
		    strcmp (prefs->wallpaper_filename, "(none)") != 0)
			prefs->wallpaper_enabled = TRUE;
		else
			prefs->wallpaper_enabled = FALSE;
	}
	else if (!strcmp (entry->key, "/desktop/gnome/background/wallpaper-type")) {
		prefs->wallpaper_type = gconf_value_get_int (value);
	} else {
		g_warning ("%s: Unknown property: %s", G_GNUC_FUNCTION, entry->key);
	}
}

static GdkColor *
read_color_from_string (const gchar *string) 
{
	GdkColor *color;
	gint32 rgb;

	color = g_new0 (GdkColor, 1);

	if (string != NULL) {
		gdk_color_parse (string, color);
		rgb = ((color->red >> 8) << 16) ||
			((color->green >> 8) << 8) ||
			(color->blue >> 8);
#if 0
		color->pixel = gdk_rgb_xpixel_from_rgb (rgb);
#else
		gdk_rgb_find_color (gdk_rgb_get_colormap (), color);
#endif
	}

	return color;
}

#define DGB "/desktop/gnome/background/"

void
bg_preferences_save (BGPreferences *prefs)
{
	GConfChangeSet *cs;
	gchar *tmp;
	
	g_return_if_fail (prefs != NULL);
	g_return_if_fail (IS_BG_PREFERENCES (prefs));

	cs = gconf_change_set_new ();
	gconf_change_set_set_bool (cs, DGB "enabled", prefs->enabled);
	gconf_change_set_set_int (cs, DGB "wallpaper-type", prefs->wallpaper_type);
	gconf_change_set_set_string (cs, DGB "wallpaper-filename", prefs->wallpaper_filename);
	
	tmp = g_strdup_printf ("#%02x%02x%02x",
		prefs->color1->red >> 8,
		prefs->color1->green >> 8,
		prefs->color1->blue >> 8);
	gconf_change_set_set_string (cs, DGB "color1", tmp);
	g_free (tmp);
	
	tmp = g_strdup_printf ("#%02x%02x%02x",
		prefs->color2->red >> 8,
		prefs->color2->green >> 8,
		prefs->color2->blue >> 8);
	gconf_change_set_set_string (cs, DGB "color2", tmp);
	g_free (tmp);

	gconf_change_set_set_int (cs, DGB "orientation", prefs->orientation);
	gconf_client_commit_change_set (gconf_client_get_default (), cs, TRUE, NULL);
	gconf_change_set_unref (cs);
}

#undef DGB

