/* -*- mode: c; style: linux -*- */

/* preferences.h
 * Copyright (C) 2000, 2001 Ximian, Inc.
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

#ifndef __PREFERENCES_H
#define __PREFERENCES_H

typedef enum _CappletDirViewLayout {
	LAYOUT_NONE,
	LAYOUT_ICON_LIST,
	LAYOUT_TREE,
#ifdef USE_HTML
	LAYOUT_HTML
#endif
} CappletDirViewLayout;

#define GNOMECC_PREFERENCES(obj)          GTK_CHECK_CAST (obj, gnomecc_preferences_get_type (), GnomeCCPreferences)
#define GNOMECC_PREFERENCES_CLASS(klass)  GTK_CHECK_CLASS_CAST (klass, gnomecc_preferences_get_type (), GnomeCCPreferencesClass)
#define IS_GNOMECC_PREFERENCES(obj)       GTK_CHECK_TYPE (obj, gnomecc_preferences_get_type ())

typedef struct _GnomeCCPreferences GnomeCCPreferences;
typedef struct _GnomeCCPreferencesClass GnomeCCPreferencesClass;

struct _GnomeCCPreferences 
{
	GtkObject object;

	gboolean embed;
	gboolean single_window;
	CappletDirViewLayout layout;
};

struct _GnomeCCPreferencesClass 
{
	GtkObjectClass parent;

	void (*changed) (GnomeCCPreferences *);
};

guint gnomecc_preferences_get_type (void);

GnomeCCPreferences *gnomecc_preferences_new (void);
GnomeCCPreferences *gnomecc_preferences_clone (GnomeCCPreferences *prefs);
void gnomecc_preferences_copy (GnomeCCPreferences *new, 
			       GnomeCCPreferences *old);
void gnomecc_preferences_load (GnomeCCPreferences *prefs);
void gnomecc_preferences_save (GnomeCCPreferences *prefs);

GtkWidget *gnomecc_preferences_get_config_dialog (GnomeCCPreferences *prefs);

#endif /* __PREFERNCES_H */
