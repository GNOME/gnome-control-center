/* -*- mode: c; style: linux -*- */

/* prefs-widget.h
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

#ifndef __PREFS_WIDGET_H
#define __PREFS_WIDGET_H

#include <gtk/gtk.h>
#include <glade/glade.h>

#include "preferences.h"

/* Generic widget descriptors to make maintenance easier */

typedef enum _widget_desc_type_t {
	WDTYPE_NONE,
	WDTYPE_CHECK,
	WDTYPE_OPTION
} widget_desc_type_t;

struct _widget_desc_t 
{
	widget_desc_type_t   type;
	char                *name;
	gint               (*get_func) (Preferences *);
	void               (*set_func) (Preferences *, gint);
};

typedef struct _widget_desc_t widget_desc_t;

#define WD_CHECK(name, namestr) \
     { WDTYPE_CHECK, namestr "_toggle", \
	(gint (*) (Preferences *)) preferences_get_##name, \
	(void (*) (Preferences *, gint)) preferences_set_##name }
#define WD_OPTION(name, namestr) \
     { WDTYPE_OPTION, namestr "_select", \
	(gint (*) (Preferences *)) preferences_get_##name, \
	(void (*) (Preferences *, gint)) preferences_set_##name }
#define WD_END \
     { WDTYPE_NONE, NULL, NULL, NULL }

/* Preferences widget class proper */

#define PREFS_WIDGET(obj)          GTK_CHECK_CAST (obj, prefs_widget_get_type (), PrefsWidget)
#define PREFS_WIDGET_CLASS(klass)  GTK_CHECK_CLASS_CAST (klass, prefs_widget_get_type (), PrefsWidgetClass)
#define IS_PREFS_WIDGET(obj)       GTK_CHECK_TYPE (obj, prefs_widget_get_type ())

typedef struct _PrefsWidget PrefsWidget;
typedef struct _PrefsWidgetClass PrefsWidgetClass;

struct _PrefsWidget 
{
	GtkDialog capplet_widget;

	Preferences *prefs;
	GladeXML *dialog_data;
};

struct _PrefsWidgetClass 
{
	GtkDialogClass parent_class;

	void (*read_preferences) (PrefsWidget *prefs_widget,
				  Preferences *prefs);

	widget_desc_t *widget_desc;
};

GType prefs_widget_get_type (void);

GtkWidget *prefs_widget_new         (Preferences *prefs);

void prefs_widget_set_preferences   (PrefsWidget *prefs_widget,
				     Preferences *prefs);

#endif /* __PREFS_WIDGET_H */
