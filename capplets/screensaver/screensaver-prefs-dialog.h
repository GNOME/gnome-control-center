/* -*- mode: c; style: linux -*- */

/* screensaver-prefs-dialog.h
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

#ifndef __SCREENSAVER_PREFS_DIALOG_H
#define __SCREENSAVER_PREFS_DIALOG_H

#include <gnome.h>
#include <libxml/tree.h>
#include <glade/glade.h>

#include "preferences.h"
#include "prefs-widget.h"

#define SCREENSAVER_PREFS_DIALOG(obj)          GTK_CHECK_CAST (obj, screensaver_prefs_dialog_get_type (), ScreensaverPrefsDialog)
#define SCREENSAVER_PREFS_DIALOG_CLASS(klass)  GTK_CHECK_CLASS_CAST (klass, screensaver_prefs_dialog_get_type (), ScreensaverPrefsDialogClass)
#define IS_SCREENSAVER_PREFS_DIALOG(obj)       GTK_CHECK_TYPE (obj, screensaver_prefs_dialog_get_type ())

typedef struct _PrefsDialogWidgetSet PrefsDialogWidgetSet;

typedef struct _ScreensaverPrefsDialog ScreensaverPrefsDialog;
typedef struct _ScreensaverPrefsDialogClass ScreensaverPrefsDialogClass;

struct _PrefsDialogWidgetSet
{
	GList       *widgets;
	GtkWidget   *value_widget;
	gboolean     alias;
	gboolean     enabled;
};

struct _ScreensaverPrefsDialog 
{
	GnomeDialog gnome_dialog;

	Screensaver *saver;
	PrefsWidget *global_prefs_widget;
	xmlDocPtr    argument_doc;
	xmlNodePtr   argument_data;
	GTree       *widget_db;
	GScanner    *cli_args_db;

	GtkWidget   *settings_dialog_frame;
	GtkWidget   *description;

	GtkWidget   *cli_entry;
	GtkWidget   *visual_combo;
	GtkWidget   *name_entry;
	GtkWidget   *basic_widget;
};

struct _ScreensaverPrefsDialogClass 
{
	GnomeDialogClass gnome_dialog_class;

	void (*ok_clicked) (ScreensaverPrefsDialog *);
	void (*demo)       (ScreensaverPrefsDialog *);
};

guint screensaver_prefs_dialog_get_type (void);

GtkWidget *screensaver_prefs_dialog_new (Screensaver *saver);
void screensaver_prefs_dialog_destroy   (ScreensaverPrefsDialog *dialog);

#endif /* __SCREENSAVER_PREFS_DIALOG_H */
