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
#include <capplet-widget.h>

#include "preferences.h"

#define PREFS_WIDGET(obj)          GTK_CHECK_CAST (obj, prefs_widget_get_type (), PrefsWidget)
#define PREFS_WIDGET_CLASS(klass)  GTK_CHECK_CLASS_CAST (klass, prefs_widget_get_type (), PrefsWidgetClass)
#define IS_PREFS_WIDGET(obj)       GTK_CHECK_TYPE (obj, prefs_widget_get_type ())

typedef struct _PrefsWidget PrefsWidget;
typedef struct _PrefsWidgetClass PrefsWidgetClass;

struct _PrefsWidget 
{
	CappletWidget capplet_widget;

	Preferences *prefs;
	GladeXML *dialog_data;
	GtkWidget *preview;

	GtkWidget *menu;
	GtkWidget *filesel;
};

struct _PrefsWidgetClass 
{
	CappletWidgetClass parent_class;
};

GType prefs_widget_get_type (void);

GtkWidget *prefs_widget_new         (Preferences *prefs);

void prefs_widget_set_preferences   (PrefsWidget *prefs_widget,
				     Preferences *prefs);

#endif /* __PREFS_WIDGET_H */
