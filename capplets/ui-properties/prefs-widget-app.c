/* -*- mode: c; style: linux -*- */

/* prefs-widget-app.c
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "prefs-widget-app.h"

#define WID(str) (glade_xml_get_widget (prefs_widget->dialog_data, str))

static PrefsWidgetClass *parent_class;

static widget_desc_t widget_desc[] = {
	WD_CHECK (menubar_detachable, "menubar_detachable"),
	WD_CHECK (menubar_relief, "menubar_relief"),
	WD_CHECK (menus_have_tearoff, "menus_have_tearoff"),
	WD_CHECK (menus_have_icons, "menus_have_icons"),
	WD_CHECK (statusbar_is_interactive, "statusbar_is_interactive"),
	WD_CHECK (statusbar_meter_on_right, "statusbar_meter_on_right"),
	WD_CHECK (toolbar_detachable, "toolbar_detachable"),
	WD_CHECK (toolbar_relief, "toolbar_relief"),
	WD_CHECK (toolbar_relief_btn, "toolbar_relief_btn"),
	WD_CHECK (toolbar_lines, "toolbar_lines"),
/*  	WD_CHECK (toolbar_labels, "toolbar_labels"), */
	WD_END
};

static void prefs_widget_app_init         (PrefsWidgetApp *prefs_widget_app);
static void prefs_widget_app_class_init   (PrefsWidgetAppClass *class);

guint
prefs_widget_app_get_type (void)
{
	static guint prefs_widget_app_type = 0;

	if (!prefs_widget_app_type) {
		GtkTypeInfo prefs_widget_app_info = {
			"PrefsWidgetApp",
			sizeof (PrefsWidgetApp),
			sizeof (PrefsWidgetAppClass),
			(GtkClassInitFunc) prefs_widget_app_class_init,
			(GtkObjectInitFunc) prefs_widget_app_init,
			(GtkArgSetFunc) NULL,
			(GtkArgGetFunc) NULL
		};

		prefs_widget_app_type = 
			gtk_type_unique (prefs_widget_get_type (), 
					 &prefs_widget_app_info);
	}

	return prefs_widget_app_type;
}

static void
prefs_widget_app_init (PrefsWidgetApp *prefs_widget_app)
{
}

static void
prefs_widget_app_class_init (PrefsWidgetAppClass *class) 
{
	PrefsWidgetClass *prefs_widget_class;

	prefs_widget_class = PREFS_WIDGET_CLASS (class);
	prefs_widget_class->widget_desc = widget_desc;

	parent_class = PREFS_WIDGET_CLASS
		(gtk_type_class (prefs_widget_get_type ()));
}

GtkWidget *
prefs_widget_app_new (Preferences *prefs) 
{
	GtkWidget *widget, *dlg_widget;
	GladeXML *dialog_data;

	g_return_val_if_fail (prefs == NULL || IS_PREFERENCES (prefs), NULL);

	dialog_data = glade_xml_new (GNOMECC_GLADE_DIR "/behavior-properties.glade",
						    "prefs_widget_app");

	widget = gtk_widget_new (prefs_widget_app_get_type (),
				 "dialog_data", dialog_data,
				 "preferences", prefs,
				 NULL);

	dlg_widget = glade_xml_get_widget (dialog_data, "prefs_widget_app");
	gtk_container_add (GTK_CONTAINER (widget), dlg_widget);

	return widget;
}
