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

#include <gtk/gtk.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

#ifdef BONOBO_CONF_ENABLE
#include <bonobo-conf/bonobo-config-database.h>
#include "capplet-util.h"
#define CP CAPPLET_CONFIG_PREFIX "background-properties/"
#else
#include <tree.h>
#endif

#define PREFERENCES(obj)          GTK_CHECK_CAST (obj, preferences_get_type (), Preferences)
#define PREFERENCES_CLASS(klass)  GTK_CHECK_CLASS_CAST (klass, preferences_get_type (), PreferencesClass)
#define IS_PREFERENCES(obj)       GTK_CHECK_TYPE (obj, preferences_get_type ())

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
	GtkObject         object;

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
	GtkObjectClass klass;
};

guint        preferences_get_type              (void);

GtkObject   *preferences_new                   (void);
GtkObject   *preferences_clone                 (const Preferences     *prefs);

GtkObject   *preferences_new_from_bonobo_pbag  (Bonobo_PropertyBag     pb,
						CORBA_Environment     *ev);
GtkObject   *preferences_new_from_bonobo_db    (Bonobo_ConfigDatabase  db,
						CORBA_Environment     *ev);
void         preferences_load_from_bonobo_pbag (Preferences           *prefs,
						Bonobo_ConfigDatabase  db,
						CORBA_Environment     *ev);
void         preferences_load_from_bonobo_db   (Preferences           *prefs,
						Bonobo_ConfigDatabase  db,
						CORBA_Environment     *ev);

void         preferences_apply_event           (Preferences           *prefs,
						const gchar           *event_name,
						const CORBA_any       *value);

void         preferences_save                  (const Preferences     *prefs);

#endif /* __PREFERENCES_H */
