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

#include "preferences.h"

#define PREFS_WIDGET(obj)          GTK_CHECK_CAST (obj, prefs_widget_get_type (), PrefsWidget)
#define PREFS_WIDGET_CLASS(klass)  GTK_CHECK_CLASS_CAST (klass, prefs_widget_get_type (), PrefsWidgetClass)
#define IS_PREFS_WIDGET(obj)       GTK_CHECK_TYPE (obj, prefs_widget_get_type ())

typedef struct _PrefsWidget PrefsWidget;
typedef struct _PrefsWidgetPrivate PrefsWidgetPrivate;

typedef struct _PrefsWidgetClass PrefsWidgetClass;

struct _PrefsWidget 
{
	GtkVBox vbox;

	GList *screensavers;
	Screensaver *selected_saver;

	GtkWidget *preview_window;

	/* Cached preferences */
	SelectionMode selection_mode;

	PrefsWidgetPrivate *priv;
};

struct _PrefsWidgetClass 
{
	GtkVBoxClass vbox_class;

	void (*state_changed) (PrefsWidget *widget);
	void (*activate_demo) (PrefsWidget *widget);
};

guint prefs_widget_get_type (void);

GtkWidget *prefs_widget_new         (GtkWindow *parent);

void prefs_widget_get_prefs         (PrefsWidget *prefs_widget,
				     Preferences *prefs);
void prefs_widget_store_prefs       (PrefsWidget *prefs_widget,
				     Preferences *prefs);
void prefs_widget_set_mode          (PrefsWidget *prefs_widget,
				     SelectionMode mode);
void prefs_widget_set_screensavers  (PrefsWidget *prefs_widget,
				     GList *screensavers); 

#endif /* __PREFS_WIDGET_H */
