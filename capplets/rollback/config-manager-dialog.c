/* -*- mode: c; style: linux -*- */

/* config-manager-dialog.c
 * Copyright (C) 2000-2001 Ximian, Inc.
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <time.h>

#include <glade/glade.h>

#include <ximian-archiver/archive.h>
#include <ximian-archiver/location.h>
#include <ximian-archiver/backend-list.h>

#include "config-manager-dialog.h"
#include "rollback-widget.h"
#include "rollback-control.h"

#define WID(str) (glade_xml_get_widget (dialog->p->config_dialog_data, str))

enum {
	ARG_0,
	ARG_TYPE
};

struct _ConfigManagerDialogPrivate 
{
	GladeXML     *config_dialog_data;

	CMDialogType  type;

	struct tm    *date;
	gboolean      rollback_all;
	gchar        *backend_id;

	Archive      *archive;
	BackendList  *backend_list;
	Location     *current_location;
};

static CappletWidgetClass *parent_class;

static void config_manager_dialog_init        (ConfigManagerDialog *dialog);
static void config_manager_dialog_class_init  (ConfigManagerDialogClass *class);

static void config_manager_dialog_set_arg     (GtkObject *object, 
					       GtkArg *arg, 
					       guint arg_id);
static void config_manager_dialog_get_arg     (GtkObject *object, 
					       GtkArg *arg, 
					       guint arg_id);

static void config_manager_dialog_finalize    (GtkObject *object);

static void ok_cb                             (GtkWidget *widget,
					       ConfigManagerDialog *dialog);
static void apply_cb                          (GtkWidget *widget,
					       ConfigManagerDialog *dialog);
static void cancel_cb                         (GtkWidget *widget,
					       ConfigManagerDialog *dialog);
static void time_count_changed_cb             (GtkSpinButton *button,
					       ConfigManagerDialog *dialog);
static void rollback_all_toggled_cb           (GtkToggleButton *button,
					       ConfigManagerDialog *dialog);
static void rollback_one_toggled_cb           (GtkToggleButton *button,
					       ConfigManagerDialog *dialog);
static void backend_select_cb                 (GtkMenuItem *menu_item,
					       ConfigManagerDialog *dialog);

static void do_rollback                       (ConfigManagerDialog *dialog,
					       gboolean rollback_to_last);
static void reset_time                        (ConfigManagerDialog *dialog,
					       guint sub_days);
static gint populate_backends_cb              (BackendList *list,
					       gchar *backend_id,
					       ConfigManagerDialog *dialog);
static void populate_backends_list            (ConfigManagerDialog *dialog,
					       BackendList *list);

static void set_backend_controls_sensitive    (ConfigManagerDialog *dialog,
					       gboolean s);

guint
config_manager_dialog_get_type (void)
{
	static guint config_manager_dialog_type = 0;

	if (!config_manager_dialog_type) {
		GtkTypeInfo config_manager_dialog_info = {
			"ConfigManagerDialog",
			sizeof (ConfigManagerDialog),
			sizeof (ConfigManagerDialogClass),
			(GtkClassInitFunc) config_manager_dialog_class_init,
			(GtkObjectInitFunc) config_manager_dialog_init,
			(GtkArgSetFunc) NULL,
			(GtkArgGetFunc) NULL
		};

		config_manager_dialog_type = 
			gtk_type_unique (capplet_widget_get_type (), 
					 &config_manager_dialog_info);
	}

	return config_manager_dialog_type;
}

static void
config_manager_dialog_init (ConfigManagerDialog *dialog)
{
	RollbackWidget *canvas;
	GnomeCanvasGroup *group;

	dialog->p = g_new0 (ConfigManagerDialogPrivate, 1);
	dialog->p->config_dialog_data =
		glade_xml_new (GLADE_DATADIR "/rollback.glade",
			       "config_dialog_data");

	gtk_container_add (GTK_CONTAINER (dialog),
			   WID ("config_dialog_data"));

	gtk_signal_connect (GTK_OBJECT (dialog), "ok",
			    GTK_SIGNAL_FUNC (ok_cb), dialog);
	gtk_signal_connect (GTK_OBJECT (dialog), "try",
			    GTK_SIGNAL_FUNC (apply_cb), dialog);
	gtk_signal_connect (GTK_OBJECT (dialog), "cancel",
			    GTK_SIGNAL_FUNC (cancel_cb), dialog);

	glade_xml_signal_connect_data (dialog->p->config_dialog_data, 
				       "time_count_changed_cb",
				       time_count_changed_cb,
				       dialog);
	glade_xml_signal_connect_data (dialog->p->config_dialog_data, 
				       "rollback_all_toggled_cb",
				       rollback_all_toggled_cb,
				       dialog);
	glade_xml_signal_connect_data (dialog->p->config_dialog_data, 
				       "rollback_one_toggled_cb",
				       rollback_one_toggled_cb,
				       dialog);

	dialog->p->rollback_all = TRUE;
	dialog->p->date = g_new (struct tm, 1);

	set_backend_controls_sensitive (dialog, FALSE);
	reset_time (dialog, 0);

	canvas = ROLLBACK_WIDGET (rollback_widget_new ());
	group = gnome_canvas_root (GNOME_CANVAS (canvas));
	gnome_canvas_item_new (group, rollback_control_get_type (),
			       "backend-id", "background-properties-capplet",
			       "is-global", FALSE,
			       "control-number", 0,
			       NULL);
	gtk_widget_show (GTK_WIDGET (canvas));

	gtk_box_pack_start (GTK_BOX (WID ("config_dialog_data")),
			    GTK_WIDGET (canvas), TRUE, TRUE, 0);

	capplet_widget_state_changed (CAPPLET_WIDGET (dialog), FALSE);
}

static void
config_manager_dialog_class_init (ConfigManagerDialogClass *class) 
{
	GtkObjectClass *object_class;

	gtk_object_add_arg_type ("ConfigManagerDialog::type",
				 GTK_TYPE_INT,
				 GTK_ARG_CONSTRUCT_ONLY | GTK_ARG_READWRITE,
				 ARG_TYPE);

	object_class = GTK_OBJECT_CLASS (class);
	object_class->finalize = config_manager_dialog_finalize;
	object_class->set_arg = config_manager_dialog_set_arg;
	object_class->get_arg = config_manager_dialog_get_arg;

	parent_class = CAPPLET_WIDGET_CLASS
		(gtk_type_class (capplet_widget_get_type ()));
}

static void
config_manager_dialog_set_arg (GtkObject *object, GtkArg *arg, guint arg_id) 
{
	ConfigManagerDialog *dialog;

	g_return_if_fail (object != NULL);
	g_return_if_fail (IS_CONFIG_MANAGER_DIALOG (object));

	dialog = CONFIG_MANAGER_DIALOG (object);

	switch (arg_id) {
	case ARG_TYPE:
		dialog->p->type = GTK_VALUE_INT (*arg);

		switch (dialog->p->type) {
		case CM_DIALOG_USER:
			dialog->p->archive = ARCHIVE (archive_load (FALSE));
			break;

		case CM_DIALOG_GLOBAL:
			dialog->p->archive = ARCHIVE (archive_load (TRUE));
			break;
		}

		dialog->p->backend_list =
			archive_get_backend_list (dialog->p->archive);
		dialog->p->current_location =
			archive_get_current_location (dialog->p->archive);
		populate_backends_list (dialog, dialog->p->backend_list);

		break;

	default:
		g_warning ("Bad argument set");
		break;
	}
}

static void
config_manager_dialog_get_arg (GtkObject *object, GtkArg *arg, guint arg_id) 
{
	ConfigManagerDialog *dialog;

	g_return_if_fail (object != NULL);
	g_return_if_fail (IS_CONFIG_MANAGER_DIALOG (object));

	dialog = CONFIG_MANAGER_DIALOG (object);

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
config_manager_dialog_finalize (GtkObject *object) 
{
	ConfigManagerDialog *dialog;

	g_return_if_fail (object != NULL);
	g_return_if_fail (IS_CONFIG_MANAGER_DIALOG (object));

	dialog = CONFIG_MANAGER_DIALOG (object);

	if (dialog->p->date != NULL)
		g_free (dialog->p->date);

	gtk_object_unref (GTK_OBJECT (dialog->p->current_location));
	gtk_object_unref (GTK_OBJECT (dialog->p->backend_list));
	gtk_object_unref (GTK_OBJECT (dialog->p->archive));

	g_free (dialog->p);

	GTK_OBJECT_CLASS (parent_class)->finalize (object);
}

GtkWidget *
config_manager_dialog_new (CMDialogType type) 
{
	return gtk_widget_new (config_manager_dialog_get_type (),
			       "type", type,
			       NULL);
}

static void
ok_cb (GtkWidget *widget, ConfigManagerDialog *dialog)
{
	g_return_if_fail (dialog != NULL);
	g_return_if_fail (IS_CONFIG_MANAGER_DIALOG (dialog));

	do_rollback (dialog, FALSE);
}

static void
apply_cb (GtkWidget *widget, ConfigManagerDialog *dialog)
{
	g_return_if_fail (dialog != NULL);
	g_return_if_fail (IS_CONFIG_MANAGER_DIALOG (dialog));

	do_rollback (dialog, FALSE);
}

static void
cancel_cb (GtkWidget *widget, ConfigManagerDialog *dialog)
{
	g_return_if_fail (dialog != NULL);
	g_return_if_fail (IS_CONFIG_MANAGER_DIALOG (dialog));

/*  	do_rollback (dialog, TRUE); */
}

static void
time_count_changed_cb (GtkSpinButton *button, ConfigManagerDialog *dialog) 
{
	g_return_if_fail (dialog != NULL);
	g_return_if_fail (IS_CONFIG_MANAGER_DIALOG (dialog));

	reset_time (dialog, gtk_spin_button_get_value_as_int (button));
}

static void
rollback_all_toggled_cb (GtkToggleButton *button, ConfigManagerDialog *dialog) 
{
	g_return_if_fail (dialog != NULL);
	g_return_if_fail (IS_CONFIG_MANAGER_DIALOG (dialog));

	if (gtk_toggle_button_get_active (button)) {
		dialog->p->rollback_all = TRUE;
		set_backend_controls_sensitive (dialog, FALSE);
	}
}

static void
rollback_one_toggled_cb (GtkToggleButton *button, ConfigManagerDialog *dialog) 
{
	g_return_if_fail (dialog != NULL);
	g_return_if_fail (IS_CONFIG_MANAGER_DIALOG (dialog));

	if (gtk_toggle_button_get_active (button)) {
		dialog->p->rollback_all = FALSE;
		set_backend_controls_sensitive (dialog, TRUE);
	}
}

static void
backend_select_cb (GtkMenuItem *menu_item, ConfigManagerDialog *dialog) 
{
	g_return_if_fail (dialog != NULL);
	g_return_if_fail (IS_CONFIG_MANAGER_DIALOG (dialog));

	dialog->p->backend_id = gtk_object_get_data (GTK_OBJECT (menu_item),
						     "backend-id");
}

static void
do_rollback (ConfigManagerDialog *dialog, gboolean rollback_to_last) 
{
	if (rollback_to_last) {
		if (dialog->p->rollback_all)
			location_rollback_all_to
				(dialog->p->current_location, NULL, TRUE);
		else
			location_rollback_backend_by
				(dialog->p->current_location, 0,
				 dialog->p->backend_id, TRUE);
	} else {
		if (dialog->p->rollback_all)
			location_rollback_all_to
				(dialog->p->current_location,
				 dialog->p->date, TRUE);
		else
			location_rollback_backend_to
				(dialog->p->current_location,
				 dialog->p->date,
				 dialog->p->backend_id, TRUE);
	}
}

static void
reset_time (ConfigManagerDialog *dialog, guint sub_days) 
{
	time_t current_time;

	time (&current_time);
	current_time -= sub_days * 24 * 60 * 60;
	localtime_r (&current_time, dialog->p->date);
}

static gint
populate_backends_cb (BackendList *list, gchar *backend_id,
		      ConfigManagerDialog *dialog) 
{
	GtkWidget *menu_item;
	GtkWidget *menu;

	if (dialog->p->backend_id == NULL)
		dialog->p->backend_id = backend_id;

	menu_item = gtk_menu_item_new_with_label (backend_id);
	gtk_widget_show (menu_item);
	gtk_object_set_data (GTK_OBJECT (menu_item),
			     "backend-id", backend_id);
	gtk_signal_connect (GTK_OBJECT (menu_item), "activate",
			    GTK_SIGNAL_FUNC (backend_select_cb), dialog);

	menu = gtk_option_menu_get_menu
		(GTK_OPTION_MENU (WID ("backend_select")));
	gtk_menu_append (GTK_MENU (menu), menu_item);
	return 0;
}

static void
populate_backends_list (ConfigManagerDialog *dialog, BackendList *list)
{
	backend_list_foreach (list, (BackendCB) populate_backends_cb, dialog);

	gtk_option_menu_set_history
		(GTK_OPTION_MENU (WID ("backend_select")), 0);
}

static void
set_backend_controls_sensitive (ConfigManagerDialog *dialog, gboolean s) 
{
	gtk_widget_set_sensitive (WID ("backend_select"), s);
}
