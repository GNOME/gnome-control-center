/* -*- mode: c; style: linux -*- */

/* prefs-widget-dialogs.c
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

#include "prefs-widget-dialogs.h"

#define WID(str) (glade_xml_get_widget (prefs_widget->dialog_data, str))

static PrefsWidgetClass *parent_class;

static widget_desc_t widget_desc[] = {
	WD_OPTION (dialog_buttons_style, "dialog_buttons_style"),
	WD_CHECK (statusbar_not_dialog, "statusbar_not_dialog"),
	WD_OPTION (dialog_position, "dialog_position"),
	WD_OPTION (dialog_type, "dialog_type"),
	WD_CHECK (dialog_centered, "dialog_centered"),
	WD_CHECK (dialog_icons, "dialog_icons"),
	WD_END
};

static void prefs_widget_dialogs_init (PrefsWidgetDialogs *prefs_widget_dlgs);
static void prefs_widget_dialogs_class_init (PrefsWidgetDialogsClass *class);

guint
prefs_widget_dialogs_get_type (void)
{
	static guint prefs_widget_dialogs_type = 0;

	if (!prefs_widget_dialogs_type) {
		GtkTypeInfo prefs_widget_dialogs_info = {
			"PrefsWidgetDialogs",
			sizeof (PrefsWidgetDialogs),
			sizeof (PrefsWidgetDialogsClass),
			(GtkClassInitFunc) prefs_widget_dialogs_class_init,
			(GtkObjectInitFunc) prefs_widget_dialogs_init,
			(GtkArgSetFunc) NULL,
			(GtkArgGetFunc) NULL
		};

		prefs_widget_dialogs_type = 
			gtk_type_unique (prefs_widget_get_type (), 
					 &prefs_widget_dialogs_info);
	}

	return prefs_widget_dialogs_type;
}

static void
prefs_widget_dialogs_init (PrefsWidgetDialogs *prefs_widget_dialogs)
{
}

static void
prefs_widget_dialogs_class_init (PrefsWidgetDialogsClass *class) 
{
	PrefsWidgetClass *prefs_widget_class;

	prefs_widget_class = PREFS_WIDGET_CLASS (class);
	prefs_widget_class->widget_desc = widget_desc;

	parent_class = PREFS_WIDGET_CLASS
		(gtk_type_class (prefs_widget_get_type ()));
}

GtkWidget *
prefs_widget_dialogs_new (Preferences *prefs) 
{
	GtkWidget *widget, *dlg_widget;
	GladeXML *dialog_data;

	g_return_val_if_fail (prefs == NULL || IS_PREFERENCES (prefs), NULL);

	dialog_data = 
		glade_xml_new (GNOMECC_GLADE_DIR "/behavior-properties.glade",
					"prefs_widget_dialogs");

	widget = gtk_widget_new (prefs_widget_dialogs_get_type (),
				 "dialog_data", dialog_data,
				 "preferences", prefs,
				 NULL);

	dlg_widget = glade_xml_get_widget (dialog_data, 
					   "prefs_widget_dialogs");
	gtk_container_add (GTK_CONTAINER (widget), dlg_widget);

	return widget;
}
