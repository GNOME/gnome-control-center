/* -*- mode: c; style: linux -*- */

/* config-manager-dialog.c
 * Copyright (C) 2000-2001 Ximian, Inc.
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

#include <time.h>

#include <glade/glade.h>

#include "config-manager-dialog.h"
#include "create-location-dialog.h"
#include "archive.h"
#include "location.h"
#include "backend-list.h"
#include "location-list.h"

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
	gchar        *selected_location_id;

	Archive      *global_archive;
	Archive      *user_archive;

	BackendList  *global_list;
	BackendList  *user_list;

	Location     *current_global;
	Location     *current_user;

	LocationList *location_list;
};

static GnomeDialogClass *parent_class;

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

static void create_cb                         (GtkWidget *button,
					       ConfigManagerDialog *dialog);
static void rename_cb                         (GtkWidget *button,
					       ConfigManagerDialog *dialog);
static void destroy_cb                        (GtkWidget *button,
					       ConfigManagerDialog *dialog);
static void change_location_cb                (GtkWidget *button,
					       ConfigManagerDialog *dialog);
static void edit_location_cb                  (GtkWidget *button,
					       ConfigManagerDialog *dialog);
static void real_create_cb                    (CreateLocationDialog
					       *create_dialog,
					       gchar *name,
					       Location *parent, 
					       ConfigManagerDialog *dialog);

static void do_rollback                       (ConfigManagerDialog *dialog);
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
			gtk_type_unique (gnome_dialog_get_type (), 
					 &config_manager_dialog_info);
	}

	return config_manager_dialog_type;
}

static void
config_manager_dialog_init (ConfigManagerDialog *dialog)
{
	static char *buttons[] = {
		GNOME_STOCK_BUTTON_OK,
		GNOME_STOCK_BUTTON_APPLY,
		GNOME_STOCK_BUTTON_CANCEL,
		NULL
	};

	gnome_dialog_constructv (GNOME_DIALOG (dialog),
				 _("Rollback and Location Management"),
				 buttons);

	dialog->p = g_new0 (ConfigManagerDialogPrivate, 1);
	dialog->p->config_dialog_data =
		glade_xml_new (GLADE_DIR "/rollback-location-management.glade",
			       "config_dialog_data");

	gtk_box_pack_start (GTK_BOX
			    (GNOME_DIALOG (dialog)->vbox),
			    WID ("config_dialog_data"), 0, TRUE, TRUE);

	gtk_window_set_policy (GTK_WINDOW (dialog),
			       TRUE, FALSE, TRUE);

	gnome_dialog_button_connect (GNOME_DIALOG (dialog),
				     0, GTK_SIGNAL_FUNC (ok_cb),
				     dialog);
	gnome_dialog_button_connect (GNOME_DIALOG (dialog),
				     1, GTK_SIGNAL_FUNC (apply_cb),
				     dialog);
	gnome_dialog_button_connect (GNOME_DIALOG (dialog),
				     2, GTK_SIGNAL_FUNC (cancel_cb),
				     dialog);

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
	glade_xml_signal_connect_data (dialog->p->config_dialog_data, 
				       "create_cb",
				       create_cb,
				       dialog);
	glade_xml_signal_connect_data (dialog->p->config_dialog_data, 
				       "rename_cb",
				       rename_cb,
				       dialog);
	glade_xml_signal_connect_data (dialog->p->config_dialog_data, 
				       "destroy_cb",
				       destroy_cb,
				       dialog);
	glade_xml_signal_connect_data (dialog->p->config_dialog_data, 
				       "change_location_cb",
				       change_location_cb,
				       dialog);
	glade_xml_signal_connect_data (dialog->p->config_dialog_data, 
				       "edit_location_cb",
				       edit_location_cb,
				       dialog);

	dialog->p->rollback_all = TRUE;
	dialog->p->date = g_new (struct tm, 1);
	dialog->p->location_list = 
		LOCATION_LIST (location_list_new (FALSE, NULL, NULL));

	gtk_widget_show (GTK_WIDGET (dialog->p->location_list));
	gtk_container_add (GTK_CONTAINER (WID ("location_tree_location")),
			   GTK_WIDGET (dialog->p->location_list));

	set_backend_controls_sensitive (dialog, FALSE);
	reset_time (dialog, 0);
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

	parent_class = GNOME_DIALOG_CLASS
		(gtk_type_class (gnome_dialog_get_type ()));
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

		if (dialog->p->user_archive != NULL) {
			dialog->p->user_list =
				archive_get_backend_list
				(dialog->p->user_archive);
			dialog->p->current_user =
				archive_get_current_location
				(dialog->p->user_archive);
			populate_backends_list
				(dialog, dialog->p->user_list);
		}

		if (dialog->p->global_archive != NULL) {
			dialog->p->global_list =
				archive_get_backend_list
				(dialog->p->global_archive);
			dialog->p->current_global =
				archive_get_current_location
				(dialog->p->global_archive);
			populate_backends_list
				(dialog, dialog->p->global_list);
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

	if (dialog->p->type == CM_DIALOG_USER_ONLY || 
	    dialog->p->type == CM_DIALOG_BOTH)
	{
		gtk_object_unref (GTK_OBJECT (dialog->p->current_user));
		gtk_object_unref (GTK_OBJECT (dialog->p->user_list));
		gtk_object_unref (GTK_OBJECT (dialog->p->user_archive));
	}

	if (dialog->p->type == CM_DIALOG_GLOBAL_ONLY || 
	    dialog->p->type == CM_DIALOG_BOTH)
	{
		gtk_object_unref (GTK_OBJECT (dialog->p->current_global));
		gtk_object_unref (GTK_OBJECT (dialog->p->global_list));
		gtk_object_unref (GTK_OBJECT (dialog->p->global_archive));
	}

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

	do_rollback (dialog);

	gnome_dialog_close (GNOME_DIALOG (dialog));
}

static void
apply_cb (GtkWidget *widget, ConfigManagerDialog *dialog)
{
	g_return_if_fail (dialog != NULL);
	g_return_if_fail (IS_CONFIG_MANAGER_DIALOG (dialog));

	do_rollback (dialog);
}

static void
cancel_cb (GtkWidget *widget, ConfigManagerDialog *dialog)
{
	g_return_if_fail (dialog != NULL);
	g_return_if_fail (IS_CONFIG_MANAGER_DIALOG (dialog));

	/* This little hack will trick the location manager into rolling back
	 * to the last known configuration
	 */
	g_free (dialog->p->date);
	dialog->p->date = NULL;
	do_rollback (dialog);

	gnome_dialog_close (GNOME_DIALOG (dialog));
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
create_cb (GtkWidget *button, ConfigManagerDialog *dialog) 
{
	CreateLocationDialog *create_dialog;

	g_return_if_fail (dialog != NULL);
	g_return_if_fail (IS_CONFIG_MANAGER_DIALOG (dialog));

	create_dialog = CREATE_LOCATION_DIALOG
		(create_location_dialog_new (dialog->p->type));

	gtk_signal_connect (GTK_OBJECT (create_dialog),
			    "create-location",
			    GTK_SIGNAL_FUNC (real_create_cb),
			    dialog);

	gtk_widget_show (GTK_WIDGET (create_dialog));
}

static void
rename_cb (GtkWidget *button, ConfigManagerDialog *dialog)
{
	g_return_if_fail (dialog != NULL);
	g_return_if_fail (IS_CONFIG_MANAGER_DIALOG (dialog));
}

static void
destroy_cb (GtkWidget *button, ConfigManagerDialog *dialog)
{
	g_return_if_fail (dialog != NULL);
	g_return_if_fail (IS_CONFIG_MANAGER_DIALOG (dialog));
}

static void
change_location_cb (GtkWidget *button, ConfigManagerDialog *dialog) 
{
	g_return_if_fail (dialog != NULL);
	g_return_if_fail (IS_CONFIG_MANAGER_DIALOG (dialog));

	/* FIXME */
	archive_set_current_location (dialog->p->user_archive,
				      location_list_get_selected_location
				      (dialog->p->location_list));
}

static void
edit_location_cb (GtkWidget *button, ConfigManagerDialog *dialog) 
{
	g_return_if_fail (dialog != NULL);
	g_return_if_fail (IS_CONFIG_MANAGER_DIALOG (dialog));
}

static void
real_create_cb (CreateLocationDialog *create_dialog, gchar *name,
		Location *parent, ConfigManagerDialog *dialog) 
{
	g_return_if_fail (dialog != NULL);
	g_return_if_fail (IS_CONFIG_MANAGER_DIALOG (dialog));

	/* FIXME */
	location_new (dialog->p->user_archive, name, parent);
	location_list_reread (dialog->p->location_list);
}

static void
do_rollback (ConfigManagerDialog *dialog) 
{
	switch (dialog->p->type) {
	case CM_DIALOG_USER_ONLY:
		if (dialog->p->rollback_all)
			location_rollback_all_to
				(dialog->p->current_user,
				 dialog->p->date, TRUE);
		else
			location_rollback_backend_to
				(dialog->p->current_user,
				 dialog->p->date,
				 dialog->p->backend_id, TRUE);
		break;

	case CM_DIALOG_GLOBAL_ONLY:
		if (dialog->p->rollback_all)
			location_rollback_all_to
				(dialog->p->current_global,
				 dialog->p->date, TRUE);
		else
			location_rollback_backend_to
				(dialog->p->current_global,
				 dialog->p->date,
				 dialog->p->backend_id, TRUE);
		break;

	case CM_DIALOG_BOTH:
		if (dialog->p->rollback_all) {
			location_rollback_all_to
				(dialog->p->current_global,
				 dialog->p->date, TRUE);
			location_rollback_all_to
				(dialog->p->current_user,
				 dialog->p->date, TRUE);
		} 
		else if (backend_list_contains
			 (dialog->p->global_list, dialog->p->backend_id))
		{
			location_rollback_backend_to
				(dialog->p->current_global,
				 dialog->p->date,
				 dialog->p->backend_id, TRUE);
		} else {
			location_rollback_backend_to
				(dialog->p->current_user,
				 dialog->p->date,
				 dialog->p->backend_id, TRUE);
		}

		break;
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
