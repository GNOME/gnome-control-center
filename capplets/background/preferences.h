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

#include <tree.h>

#define PREFERENCES(obj)          GTK_CHECK_CAST (obj, preferences_get_type (), Preferences)
#define PREFERENCES_CLASS(klass)  GTK_CHECK_CLASS_CAST (klass, preferences_get_type (), PreferencesClass)
#define IS_PREFERENCES(obj)       GTK_CHECK_TYPE (obj, preferences_get_type ())

typedef struct _Preferences Preferences;
typedef struct _PreferencesClass PreferencesClass;

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

guint        preferences_get_type   (void);

GtkObject   *preferences_new        (void);
GtkObject   *preferences_clone      (Preferences *prefs);
void         preferences_destroy    (GtkObject *object);

void         preferences_load       (Preferences *prefs);
void         preferences_save       (Preferences *prefs);
void         preferences_changed    (Preferences *prefs);
void         preferences_apply_now  (Preferences *prefs);
void         preferences_apply_preview (Preferences *prefs);

void         preferences_freeze     (Preferences *prefs);
void         preferences_thaw       (Preferences *prefs);

Preferences *preferences_read_xml   (xmlDocPtr xml_doc);
xmlDocPtr    preferences_write_xml  (Preferences *prefs);

#endif /* __PREFERENCES_H */
