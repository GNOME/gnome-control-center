/* -*- mode: c; style: linux -*- */

/* preferences.h
 * Copyright (C) 2000 Helix Code, Inc.
 *
 * Written by Bradford Hovinen <hovinen@helixcode.com>
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

#ifndef __PREFERENCES_H
#define __PREFERENCES_H

#include <glib-object.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gconf/gconf-client.h>

#define PREFERENCES(obj)          G_TYPE_CHECK_INSTANCE_CAST (obj, preferences_get_type (), Preferences)
#define PREFERENCES_CLASS(klass)  G_TYPE_CHECK_CLASS_CAST (klass, preferences_get_type (), PreferencesClass)
#define IS_PREFERENCES(obj)       G_TYPE_CHECK_INSTANCE_TYPE (obj, preferences_get_type ())

typedef struct _Preferences Preferences;
typedef struct _PreferencesClass PreferencesClass;

typedef enum _orientation_t {
	ORIENTATION_SOLID, ORIENTATION_HORIZ, ORIENTATION_VERT
} orientation_t;

typedef enum _wallpaper_type_t {
	WPTYPE_TILED, WPTYPE_CENTERED, WPTYPE_SCALED,
	WPTYPE_STRETCHED, WPTYPE_EMBOSSED
} wallpaper_type_t;

struct _Preferences
{
	GObject           object;

	gint              frozen;
	gboolean          auto_apply;
	guint             timeout_id;

	gboolean          enabled;
	gboolean          gradient_enabled;
	gboolean          wallpaper_enabled;
	orientation_t     orientation;
	wallpaper_type_t  wallpaper_type;

	GdkColor         *color1;
	GdkColor         *color2;

	gchar            *wallpaper_filename;
	gchar            *wallpaper_sel_path;

	GSList           *wallpapers;

	gboolean          adjust_opacity;
	gint              opacity;
};

struct _PreferencesClass
{
	GObjectClass klass;
};

GType        preferences_get_type              (void);

GObject     *preferences_new                   (void);
GObject     *preferences_clone                 (const Preferences     *prefs);

void         preferences_load                  (Preferences           *prefs);

void         preferences_merge_entry           (Preferences           *prefs,
						const GConfEntry      *entry);

#endif /* __PREFERENCES_H */
