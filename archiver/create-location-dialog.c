/* -*- mode: c; style: linux -*- */

/* create-location-dialog.c
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

#include <glade/glade.h>

#include "create-location-dialog.h"
#include "location-list.h"

#define WID(str) (glade_xml_get_widget (dialog->p->create_dialog_data, str))

enum {
	ARG_0,
	ARG_TYPE
};

enum {
	CREATE_LOCATION_SIGNAL,
	LAST_SIGNAL
};

struct _CreateLocationDialogPrivate 
{
	GladeXML     *create_dialog_data;

	CMDialogType  type;

	gchar        *selected_location_id;

	Archive      *global_archive;
	Archive      *user_archive;

	LocationList *location_list;
};

static GnomeDialogClass *parent_class;

static guint create_location_dialog_signals[LAST_SIGNAL] = { 0 };

static void create_location_dialog_init        (CreateLocationDialog *dialog);
static void create_location_dialog_class_init  (CreateLocationDialogClass *class);

static void create_location_dialog_set_arg     (GtkObject *object, 
						GtkArg *arg, 
						guint arg_id);
static void create_location_dialog_get_arg     (GtkObject *object, 
						GtkArg *arg, 
						guint arg_id);

static void ok_cb                              (GtkWidget *widget,
						CreateLocationDialog *dialog);
static void cancel_cb                          (GtkWidget *widget,
						CreateLocationDialog *dialog);

static void create_location_dialog_finalize    (GtkObject *object);

guint
create_location_dialog_get_type (void)
{
	static guint create_location_dialog_type = 0;

	if (!create_location_dialog_type) {
		GtkTypeInfo create_location_dialog_info = {
			"CreateLocationDialog",
			sizeof (CreateLocationDialog),
			sizeof (CreateLocationDialogClass),
			(GtkClassInitFunc) create_location_dialog_class_init,
			(GtkObjectInitFunc) create_location_dialog_init,
			(GtkArgSetFunc) NULL,
			(GtkArgGetFunc) NULL
		};

		create_location_dialog_type = 
			gtk_type_unique (gnome_dialog_get_type (), 
					 &create_location_dialog_info);
	}

	return create_location_dialog_type;
}

static void
create_location_dialog_init (CreateLocationDialog *dialog)
{
	static char *buttons[] = {
		GNOME_STOCK_BUTTON_OK,
		GNOME_STOCK_BUTTON_CANCEL,
		NULL
	};

	gnome_dialog_constructv (GNOME_DIALOG (dialog),
				 _("Rollback and Location Management"),
				 buttons);

	dialog->p = g_new0 (CreateLocationDialogPrivate, 1);
	dialog->p->create_dialog_data =
		glade_xml_new (GLADE_DIR "/rollback-location-management.glade",
			       "create_dialog_data");

	gtk_box_pack_start (GTK_BOX (GNOME_DIALOG (dialog)->vbox),
			    WID ("create_dialog_data"), 0, TRUE, TRUE);

	gtk_window_set_policy (GTK_WINDOW (dialog), TRUE, FALSE, TRUE);

	gnome_dialog_button_connect (GNOME_DIALOG (dialog),
				     0, GTK_SIGNAL_FUNC (ok_cb),
				     dialog);
	gnome_dialog_button_connect (GNOME_DIALOG (dialog),
				     1, GTK_SIGNAL_FUNC (cancel_cb),
				     dialog);

	dialog->p->location_list = 
		LOCATION_LIST (location_list_new (FALSE, NULL, NULL));

	gtk_widget_show (GTK_WIDGET (dialog->p->location_list));
	gtk_container_add (GTK_CONTAINER (WID ("location_list_location")),
			   GTK_WIDGET (dialog->p->location_list));
}

static void
create_location_dialog_class_init (CreateLocationDialogClass *class) 
{
	GtkObjectClass *object_class;

	gtk_object_add_arg_type ("CreateLocationDialog::type",
				 GTK_TYPE_INT,
				 GTK_ARG_CONSTRUCT_ONLY | GTK_ARG_READWRITE,
				 ARG_TYPE);

	object_class = GTK_OBJECT_CLASS (class);
	object_class->finalize = create_location_dialog_finalize;
	object_class->set_arg = create_location_dialog_set_arg;
	object_class->get_arg = create_location_dialog_get_arg;

	create_location_dialog_signals[CREATE_LOCATION_SIGNAL] =
		gtk_signal_new ("create-location", GTK_RUN_FIRST,
				object_class->type,
				GTK_SIGNAL_OFFSET (CreateLocationDialogClass,
						   create_location),
				gtk_marshal_NONE__POINTER_POINTER,
				GTK_TYPE_NONE, 2, GTK_TYPE_POINTER, 
				GTK_TYPE_POINTER);

	gtk_object_class_add_signals (object_class,
				      create_location_dialog_signals,
				      LAST_SIGNAL);

	parent_class = GNOME_DIALOG_CLASS
		(gtk_type_class (gnome_dialog_get_type ()));
}

static void
create_location_dialog_set_arg (GtkObject *object, GtkArg *arg, guint arg_id) 
{
	CreateLocationDialog *dialog;

	g_return_if_fail (object != NULL);
	g_return_if_fail (IS_CREATE_LOCATION_DIALOG (object));

	dialog = CREATE_LOCATION_DIALOG (object);

	switch (arg_id) {
	case ARG_TYPE:
		dialog->p->type = GTK_VALUE_INT (*arg);

		switch (dialog->p->type) {
		case CM_DIALOG_USER_ONLY:
			dialog->p->user_archive =
				ARCHIVE (archive_load (FALSE));
			dialog->p->global_archive = NULL;
			break;

		case CM_DIALOG_GLOBAL_ONLY:
			dialog->p->global_archive = 
				ARCHIVE (archive_load (TRUE));
			dialog->p->user_archive = NULL;
			break;

		case CM_DIALOG_BOTH:
			dialog->p->user_archive = 
				ARCHIVE (archive_load (FALSE));
			dialog->p->global_archive = 
				ARCHIVE (archive_load (TRUE));
			break;
		}

		gtk_object_set (GTK_OBJECT (dialog->p->location_list),
				"user-archive", dialog->p->user_archive,
				"global-archive", dialog->p->global_archive,
				NULL);

		break;

	default:
		g_warning ("Bad argument set");
		break;
	}
}

static void
create_location_dialog_get_arg (GtkObject *object, GtkArg *arg, guint arg_id) 
{
	CreateLocationDialog *dialog;

	g_return_if_fail (object != NULL);
	g_return_if_fail (IS_CREATE_LOCATION_DIALOG (object));

	dialog = CREATE_LOCATION_DIALOG (object);

	switch (arg_id) {
	case ARG_TYPE:
		GTK_VALUE_INT (*arg) = dialog->p->type;
		break;

	default:
		g_warning ("Bad argument get");
		break;
	}
}

static void
create_location_dialog_finalize (GtkObject *object) 
{
	CreateLocationDialog *dialog;

	g_return_if_fail (object != NULL);
	g_return_if_fail (IS_CREATE_LOCATION_DIALOG (object));

	dialog = CREATE_LOCATION_DIALOG (object);

	g_free (dialog->p);

	GTK_OBJECT_CLASS (parent_class)->finalize (object);
}

GtkObject *
create_location_dialog_new (CMDialogType type) 
{
	return gtk_object_new (create_location_dialog_get_type (),
			       "type", type,
			       NULL);
}

static void
ok_cb (GtkWidget *widget, CreateLocationDialog *dialog)
{
	g_return_if_fail (dialog != NULL);
	g_return_if_fail (IS_CREATE_LOCATION_DIALOG (dialog));

	gtk_signal_emit (GTK_OBJECT (dialog),
			 create_location_dialog_signals
			 [CREATE_LOCATION_SIGNAL],
			 gtk_entry_get_text
			 (GTK_ENTRY (WID ("location_name_entry"))),
			 location_list_get_selected_location
			 (dialog->p->location_list));

	gnome_dialog_close (GNOME_DIALOG (dialog));
}

static void
cancel_cb (GtkWidget *widget, CreateLocationDialog *dialog)
{
	g_return_if_fail (dialog != NULL);
	g_return_if_fail (IS_CREATE_LOCATION_DIALOG (dialog));

	gnome_dialog_close (GNOME_DIALOG (dialog));
}

