/* -*- mode: c; style: linux -*- */

/* prefs-widget-app.h
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

#ifndef __PREFS_WIDGET_APP_H
#define __PREFS_WIDGET_APP_H

#include <gtk/gtk.h>
#include <glade/glade.h>

#include "preferences.h"
#include "prefs-widget.h"

#define PREFS_WIDGET_APP(obj)          GTK_CHECK_CAST (obj, prefs_widget_app_get_type (), PrefsWidgetApp)
#define PREFS_WIDGET_APP_CLASS(klass)  GTK_CHECK_CLASS_CAST (klass, prefs_widget_app_get_type (), PrefsWidgetAppClass)
#define IS_PREFS_WIDGET_APP(obj)       GTK_CHECK_TYPE (obj, prefs_widget_app_get_type ())

typedef struct _PrefsWidgetApp PrefsWidgetApp;
typedef struct _PrefsWidgetAppClass PrefsWidgetAppClass;

struct _PrefsWidgetApp 
{
	PrefsWidget prefs_widget;
};

struct _PrefsWidgetAppClass 
{
	PrefsWidgetClass parent_class;
};

GType prefs_widget_app_get_type (void);

GtkWidget *prefs_widget_app_new         (Preferences *prefs);

#endif /* __PREFS_WIDGET_APP_H */
