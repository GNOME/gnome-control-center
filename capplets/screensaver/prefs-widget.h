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
typedef struct _PrefsWidgetClass PrefsWidgetClass;

struct _PrefsWidget 
{
	GtkNotebook notebook;

	GtkWidget *disable_screensaver_widget;
	GtkWidget *blank_screen_widget;
	GtkWidget *one_screensaver_widget;
	GtkWidget *choose_from_list_widget;
	GtkWidget *choose_randomly_widget;

	GtkWidget *screensaver_list;

	GtkWidget *demo_button;
	GtkWidget *remove_button;
	GtkWidget *settings_button;

	GtkWidget *preview_window;
	GtkWidget *description;

	GtkObject *timeout_widget_adj;
	GtkWidget *timeout_widget;
	GtkObject *cycle_length_widget_adj;
	GtkWidget *cycle_length_widget;

	GtkWidget *lock_widget;
	GtkWidget *enable_timeout_widget;
	GtkObject *time_to_lock_widget_adj;
	GtkWidget *time_to_lock_widget;
	GtkWidget *lock_timeout_seconds_label;
	GtkWidget *lock_vts_widget;
	GtkWidget *pwr_manage_enable;

	GtkWidget *standby_monitor_toggle;
	GtkObject *standby_time_widget_adj;
	GtkWidget *standby_time_widget;
	GtkWidget *standby_monitor_label2;

	GtkWidget *suspend_monitor_toggle;
	GtkObject *suspend_time_widget_adj;
	GtkWidget *suspend_time_widget;
	GtkWidget *suspend_monitor_label2;

	GtkWidget *shut_down_monitor_toggle;
	GtkObject *shut_down_time_widget_adj;
	GtkWidget *shut_down_time_widget;
	GtkWidget *shut_down_monitor_label2;

	GtkWidget *nice_widget;
	GtkWidget *verbose_widget;

	GtkWidget *effects_frame;
	GtkWidget *install_cmap_widget;
	GtkWidget *fade_widget;
	GtkWidget *unfade_widget;
	GtkWidget *fade_duration_label;
	GtkWidget *fade_ticks_widget;
	GtkWidget *fade_ticks_label;
	GtkWidget *fade_duration_widget;
	GtkWidget *fade_duration_high_label;
	GtkWidget *fade_ticks_high_label;
	GtkWidget *fade_duration_low_label;
	GtkWidget *fade_ticks_low_label;

	GList *screensavers;
	Screensaver *selected_saver;

	/* Cached preferences */
	SelectionMode selection_mode;
};

struct _PrefsWidgetClass 
{
	GtkNotebookClass notebook_class;

	void (*state_changed) (PrefsWidget *widget);
	void (*activate_demo) (PrefsWidget *widget);
};

guint prefs_widget_get_type (void);

GtkWidget *prefs_widget_new         (void);

void prefs_widget_get_prefs         (PrefsWidget *prefs_widget,
				     Preferences *prefs);
void prefs_widget_store_prefs       (PrefsWidget *prefs_widget,
				     Preferences *prefs);
void prefs_widget_set_screensavers  (PrefsWidget *prefs_widget,
				     GList *screensavers, 
				     SelectionMode mode);

#endif /* __PREFS_WIDGET_H */
