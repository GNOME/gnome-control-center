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
#include <gnome.h>

#define PREFERENCES(obj)          GTK_CHECK_CAST (obj, preferences_get_type (), Preferences)
#define PREFERENCES_CLASS(klass)  GTK_CHECK_CLASS_CAST (klass, preferences_get_type (), PreferencesClass)
#define IS_PREFERENCES(obj)       GTK_CHECK_TYPE (obj, preferences_get_type ())

typedef struct _Preferences Preferences;
typedef struct _PreferencesClass PreferencesClass;

struct _Preferences 
{
	GtkObject         object;

	gint              frozen;
	guint             timeout_id;

	struct {
		enum { DEFAULT, SPREAD, EDGE, START, END } dialog_buttons_style;
		gboolean dialog_icons;
		gboolean dialog_centered;
		GtkWindowPosition dialog_position;
		GtkWindowType dialog_type;
		gboolean menus_show_icons;
		gboolean menus_have_tearoff;
		gboolean toolbar_labels;
		gboolean toolbar_detachable;
		gboolean toolbar_relief;
		gboolean toolbar_separator;
		gboolean toolbar_popup;
		gboolean menubar_detachable;
		gboolean menubar_relief;
		gboolean statusbar_meter_on_right;
		gboolean statusbar_is_interactive;
		GnomeMDIMode mdi_mode;
		GtkPositionType mdi_tab_pos;
	} gnome_prefs;
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

void         preferences_freeze     (Preferences *prefs);
void         preferences_thaw       (Preferences *prefs);

/* get/set functions. It's really stupid that we need these */

int preferences_get_menubar_detachable                 (Preferences *prefs);
int preferences_get_menubar_relief                     (Preferences *prefs);
int preferences_get_menus_have_tearoff                 (Preferences *prefs);
int preferences_get_menus_have_icons                   (Preferences *prefs);

int preferences_get_statusbar_is_interactive           (Preferences *prefs);
int preferences_get_statusbar_meter_on_left            (Preferences *prefs);
int preferences_get_statusbar_meter_on_right           (Preferences *prefs);

int preferences_get_toolbar_detachable                 (Preferences *prefs);
int preferences_get_toolbar_relief                     (Preferences *prefs);
int preferences_get_toolbar_icons_only                 (Preferences *prefs);
int preferences_get_toolbar_text_below                 (Preferences *prefs);

int preferences_get_dialog_icons                       (Preferences *prefs);
int preferences_get_dialog_centered                    (Preferences *prefs);

GtkWindowPosition preferences_get_dialog_position      (Preferences *prefs);
GtkWindowType preferences_get_dialog_type              (Preferences *prefs);
int preferences_get_dialog_buttons_style	       (Preferences *prefs);

GnomeMDIMode preferences_get_mdi_mode                  (Preferences *prefs);
GtkPositionType preferences_get_mdi_tab_pos            (Preferences *prefs);

#if 0
int preferences_get_property_box_buttons_ok            (Preferences *prefs);
int preferences_get_property_box_buttons_apply         (Preferences *prefs);
int preferences_get_property_box_buttons_close         (Preferences *prefs);
int preferences_get_property_box_buttons_help          (Preferences *prefs);
int preferences_get_disable_imlib_cache                (Preferences *prefs);
#endif






void preferences_set_menubar_detachable                 (Preferences *prefs, int i);
void preferences_set_menubar_relief                     (Preferences *prefs, int i);
void preferences_set_menus_have_tearoff                 (Preferences *prefs, int i);
void preferences_set_menus_have_icons                   (Preferences *prefs, int i);

void preferences_set_statusbar_is_interactive           (Preferences *prefs, int i);
void preferences_set_statusbar_meter_on_left            (Preferences *prefs, int i);
void preferences_set_statusbar_meter_on_right           (Preferences *prefs, int i);

void preferences_set_toolbar_detachable                 (Preferences *prefs, int i);
void preferences_set_toolbar_relief                     (Preferences *prefs, int i);
void preferences_set_toolbar_icons_only                 (Preferences *prefs, int i);
void preferences_set_toolbar_text_below                 (Preferences *prefs, int i);

void preferences_set_dialog_icons                       (Preferences *prefs, int i);
void preferences_set_dialog_centered                    (Preferences *prefs, int i);

void preferences_set_dialog_position                    (Preferences *prefs, int i);
void preferences_set_dialog_type                        (Preferences *prefs, int i);
void preferences_set_dialog_buttons_style               (Preferences *prefs, int i);

void preferences_set_mdi_mode                           (Preferences *prefs, int i);
void preferences_set_mdi_tab_pos                        (Preferences *prefs, int i);

#if 0
void preferences_set_property_box_buttons_ok            (Preferences *prefs, int i);
void preferences_set_property_box_buttons_apply         (Preferences *prefs, int i);
void preferences_set_property_box_buttons_close         (Preferences *prefs, int i);
void preferences_set_property_box_buttons_help          (Preferences *prefs, int i);
void preferences_set_disable_imlib_cache                (Preferences *prefs, int i);
#endif

#endif /* __PREFERENCES_H */
