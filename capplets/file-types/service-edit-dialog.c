/* -*- mode: c; style: linux -*- */

/* service-edit-dialog.c
 * Copyright (C) 2001 Ximian, Inc.
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

#include <glade/glade.h>
#include <gconf/gconf-client.h>
#include <ctype.h>

#include "service-edit-dialog.h"
#include "mime-types-model.h"

#define WID(x) (glade_xml_get_widget (dialog->p->dialog_xml, x))

enum {
	PROP_0,
	PROP_MODEL,
	PROP_INFO,
	PROP_IS_ADD
};

struct _ServiceEditDialogPrivate 
{
	ServiceInfo  *info;
	GladeXML     *dialog_xml;
	GtkWidget    *dialog_win;
	gboolean      is_add;

	GtkTreeModel *model;
};

static GObjectClass *parent_class;

static void service_edit_dialog_init        (ServiceEditDialog *dialog,
					     ServiceEditDialogClass *class);
static void service_edit_dialog_class_init  (ServiceEditDialogClass *class);
static void service_edit_dialog_base_init   (ServiceEditDialogClass *class);

static void service_edit_dialog_set_prop    (GObject      *object, 
					     guint         prop_id,
					     const GValue *value, 
					     GParamSpec   *pspec);
static void service_edit_dialog_get_prop    (GObject      *object,
					     guint         prop_id,
					     GValue       *value,
					     GParamSpec   *pspec);

static void service_edit_dialog_dispose     (GObject *object);
static void service_edit_dialog_finalize    (GObject *object);

static void fill_dialog                     (ServiceEditDialog *dialog);
static void setup_add_dialog                (ServiceEditDialog *dialog);

static void populate_app_list               (ServiceEditDialog *dialog);

static void store_data                      (ServiceEditDialog *dialog);
static gboolean validate_data               (ServiceEditDialog *dialog);

static void program_sensitive_cb            (ServiceEditDialog *dialog,
					     GtkToggleButton   *tb);
static void program_changed_cb              (ServiceEditDialog *dialog,
					     GtkOptionMenu     *option_menu);

static void response_cb                     (ServiceEditDialog *dialog,
					     gint               response_id);

GType
service_edit_dialog_get_type (void)
{
	static GType service_edit_dialog_type = 0;

	if (!service_edit_dialog_type) {
		GTypeInfo service_edit_dialog_info = {
			sizeof (ServiceEditDialogClass),
			(GBaseInitFunc) service_edit_dialog_base_init,
			NULL, /* GBaseFinalizeFunc */
			(GClassInitFunc) service_edit_dialog_class_init,
			NULL, /* GClassFinalizeFunc */
			NULL, /* user-supplied data */
			sizeof (ServiceEditDialog),
			0, /* n_preallocs */
			(GInstanceInitFunc) service_edit_dialog_init,
			NULL
		};

		service_edit_dialog_type = 
			g_type_register_static (G_TYPE_OBJECT, 
						"ServiceEditDialog",
						&service_edit_dialog_info, 0);
	}

	return service_edit_dialog_type;
}

static void
service_edit_dialog_init (ServiceEditDialog *dialog, ServiceEditDialogClass *class)
{
	GtkSizeGroup *size_group;

	dialog->p = g_new0 (ServiceEditDialogPrivate, 1);
	dialog->p->dialog_xml = glade_xml_new
		(GNOMECC_DATA_DIR "/interfaces/file-types-properties.glade", "service_edit_widget", NULL);

	size_group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);
	gtk_size_group_add_widget (size_group, WID ("description_label"));
	gtk_size_group_add_widget (size_group, WID ("protocol_label"));

	dialog->p->dialog_win = gtk_dialog_new_with_buttons
		(_("Edit file type"), NULL, -1,
		 GTK_STOCK_OK,     GTK_RESPONSE_OK,
		 GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
		 NULL);

	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog->p->dialog_win)->vbox), WID ("service_edit_widget"), TRUE, TRUE, 0);

	g_signal_connect_swapped (G_OBJECT (WID ("run_program_toggle")), "toggled", (GCallback) program_sensitive_cb, dialog);
	g_signal_connect_swapped (G_OBJECT (WID ("program_select")), "changed", (GCallback) program_changed_cb, dialog);

	g_signal_connect_swapped (G_OBJECT (dialog->p->dialog_win), "response", (GCallback) response_cb, dialog);
}

static void
service_edit_dialog_base_init (ServiceEditDialogClass *class) 
{
}

static void
service_edit_dialog_class_init (ServiceEditDialogClass *class) 
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (class);

	object_class->dispose = service_edit_dialog_dispose;
	object_class->finalize = service_edit_dialog_finalize;
	object_class->set_property = service_edit_dialog_set_prop;
	object_class->get_property = service_edit_dialog_get_prop;

	g_object_class_install_property
		(object_class, PROP_MODEL,
		 g_param_spec_object ("model",
				      _("Model"),
				      _("Model"),
				      gtk_tree_model_get_type (),
				      G_PARAM_READWRITE));

	g_object_class_install_property
		(object_class, PROP_INFO,
		 g_param_spec_pointer ("service-info",
				       _("Service info"),
				       _("Structure containing service information"),
				       G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

	g_object_class_install_property
		(object_class, PROP_IS_ADD,
		 g_param_spec_boolean ("is-add",
				       _("Is add"),
				       _("TRUE if this is an add service dialog"),
				       FALSE,
				       G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

	parent_class = G_OBJECT_CLASS
		(g_type_class_ref (G_TYPE_OBJECT));
}

static void
service_edit_dialog_set_prop (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec) 
{
	ServiceEditDialog *dialog;

	g_return_if_fail (object != NULL);
	g_return_if_fail (IS_SERVICE_EDIT_DIALOG (object));

	dialog = SERVICE_EDIT_DIALOG (object);

	switch (prop_id) {
	case PROP_MODEL:
		dialog->p->model = GTK_TREE_MODEL (g_value_get_object (value));
		break;

	case PROP_INFO:
		if (!dialog->p->is_add && g_value_get_pointer (value) != NULL) {
			dialog->p->info = g_value_get_pointer (value);
			fill_dialog (dialog);
			gtk_widget_show_all (dialog->p->dialog_win);
		}

		break;

	case PROP_IS_ADD:
		dialog->p->is_add = g_value_get_boolean (value);

		if (dialog->p->is_add) {
			dialog->p->info = service_info_new (NULL, NULL);
			setup_add_dialog (dialog);
			gtk_widget_show_all (dialog->p->dialog_win);
		}

		break;

	default:
		g_warning ("Bad property set");
		break;
	}
}

static void
service_edit_dialog_get_prop (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec) 
{
	ServiceEditDialog *dialog;

	g_return_if_fail (object != NULL);
	g_return_if_fail (IS_SERVICE_EDIT_DIALOG (object));

	dialog = SERVICE_EDIT_DIALOG (object);

	switch (prop_id) {
	case PROP_MODEL:
		g_value_set_object (value, G_OBJECT (dialog->p->model));
		break;

	case PROP_INFO:
		g_value_set_pointer (value, dialog->p->info);
		break;

	case PROP_IS_ADD:
		g_value_set_boolean (value, dialog->p->is_add);
		break;

	default:
		g_warning ("Bad property get");
		break;
	}
}

static void
service_edit_dialog_dispose (GObject *object) 
{
	ServiceEditDialog *dialog;

	g_return_if_fail (object != NULL);
	g_return_if_fail (IS_SERVICE_EDIT_DIALOG (object));

	dialog = SERVICE_EDIT_DIALOG (object);

	if (dialog->p->dialog_win != NULL) {
		gtk_widget_destroy (dialog->p->dialog_win);
		dialog->p->dialog_win = NULL;
	}

	if (dialog->p->dialog_xml != NULL) {
		g_object_unref (G_OBJECT (dialog->p->dialog_xml));
		dialog->p->dialog_xml = NULL;
	}

	G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
service_edit_dialog_finalize (GObject *object) 
{
	ServiceEditDialog *service_edit_dialog;

	g_return_if_fail (object != NULL);
	g_return_if_fail (IS_SERVICE_EDIT_DIALOG (object));

	service_edit_dialog = SERVICE_EDIT_DIALOG (object);

	g_free (service_edit_dialog->p);

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

GObject *
service_edit_dialog_new (GtkTreeModel *model, ServiceInfo *info) 
{
	return g_object_new (service_edit_dialog_get_type (),
			     "model", model,
			     "service-info", info,
			     NULL);
}

GObject *
service_add_dialog_new (GtkTreeModel *model) 
{
	return g_object_new (service_edit_dialog_get_type (),
			     "model", model,
			     "is-add", TRUE,
			     NULL);
}

static void
fill_dialog (ServiceEditDialog *dialog) 
{
	service_info_load_all (dialog->p->info);

	gtk_entry_set_text (GTK_ENTRY (WID ("description_entry")), service_info_get_description (dialog->p->info));

	if (dialog->p->info->protocol != NULL) {
		if (strcmp (dialog->p->info->protocol, "unknown"))
			gtk_entry_set_text (GTK_ENTRY (WID ("protocol_entry")), dialog->p->info->protocol);

		gtk_widget_set_sensitive (WID ("protocol_entry"), FALSE);
	}

	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (WID ("look_at_content_toggle")), !dialog->p->info->run_program);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (WID ("run_program_toggle")), dialog->p->info->run_program);

	if (!dialog->p->info->run_program && strcmp (dialog->p->info->protocol, "ftp"))
		gtk_widget_set_sensitive (WID ("program_frame"), FALSE);

	if (dialog->p->info->custom_line != NULL)
		gnome_file_entry_set_filename (GNOME_FILE_ENTRY (WID ("custom_program_entry")), dialog->p->info->custom_line);

	populate_app_list (dialog);
}

static void
setup_add_dialog (ServiceEditDialog *dialog)
{
	GtkWidget *menu, *item;

	item = gtk_menu_item_new_with_label (_("Custom"));
	menu = gtk_menu_new ();
	gtk_menu_append (GTK_MENU (menu), item);
	gtk_option_menu_set_menu (GTK_OPTION_MENU (WID ("program_select")), menu);

	gtk_widget_set_sensitive (WID ("program_select"), FALSE);

	gtk_widget_set_sensitive (WID ("look_at_content_toggle"), FALSE);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (WID ("run_program_toggle")), TRUE);
}

static void
populate_app_list (ServiceEditDialog *dialog) 
{
	GtkOptionMenu *option_menu;
	GtkMenu       *menu;
	GtkWidget     *item;

	const GList   *service_apps;
	GnomeVFSMimeApplication *app;

	option_menu = GTK_OPTION_MENU (WID ("program_select"));
	menu = GTK_MENU (gtk_menu_new ());
	service_apps = get_apps_for_service_type (dialog->p->info->protocol);

	if (service_apps == NULL)
		gtk_widget_set_sensitive (WID ("program_select"), FALSE);

	while (service_apps != NULL) {
		app = service_apps->data;
		item = gtk_menu_item_new_with_label (app->name);
		g_object_set_data (G_OBJECT (item), "app", app);
		gtk_widget_show (item);
		gtk_menu_append (menu, item);
		service_apps = service_apps->next;
	}

	item = gtk_menu_item_new_with_label (_("Custom"));
	gtk_widget_show (item);
	gtk_menu_append (menu, item);

	gtk_option_menu_set_menu (option_menu, GTK_WIDGET (menu));
}

static void
store_data (ServiceEditDialog *dialog) 
{
	GtkOptionMenu *option_menu;
	GtkMenuShell  *menu_shell;
	GObject       *menu_item;
	gint           idx;

	GtkTreePath   *path;
	GtkTreeIter    iter;

	if (dialog->p->is_add)
		dialog->p->info->protocol = g_strdup (gtk_entry_get_text (GTK_ENTRY (WID ("protocol_entry"))));

	g_free (dialog->p->info->description);
	dialog->p->info->description = g_strdup (gtk_entry_get_text (GTK_ENTRY (WID ("description_entry"))));

	dialog->p->info->run_program =
		gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (WID ("run_program_toggle")));

	g_free (dialog->p->info->custom_line);
	dialog->p->info->custom_line =
		g_strdup (gnome_file_entry_get_full_path (GNOME_FILE_ENTRY (WID ("custom_program_entry")), FALSE));

	option_menu = GTK_OPTION_MENU (WID ("program_select"));
	menu_shell = GTK_MENU_SHELL (gtk_option_menu_get_menu (option_menu));
	idx = gtk_option_menu_get_history (option_menu);
	menu_item = (g_list_nth (menu_shell->children, idx))->data;

	gnome_vfs_mime_application_free (dialog->p->info->app);
	dialog->p->info->app =
		gnome_vfs_mime_application_copy (g_object_get_data (menu_item, "app"));

	model_entry_append_to_dirty_list (MODEL_ENTRY (dialog->p->info));

	if (dialog->p->is_add) {
		model_entry_insert_child (get_services_category_entry (dialog->p->model),
					  MODEL_ENTRY (dialog->p->info),
					  dialog->p->model);
	} else {
		mime_types_model_construct_iter (MIME_TYPES_MODEL (dialog->p->model),
						 MODEL_ENTRY (dialog->p->info), &iter);
		path = gtk_tree_model_get_path (dialog->p->model, &iter);
		gtk_tree_model_row_changed (dialog->p->model, path, &iter);
		gtk_tree_path_free (path);
	}
}

static gboolean
validate_data (ServiceEditDialog *dialog) 
{
	const gchar *tmp, *tmp1;
	gchar *dir;
	GtkWidget *err_dialog;

	tmp = gtk_entry_get_text (GTK_ENTRY (WID ("protocol_entry")));

	if (tmp == NULL || *tmp == '\0') {
		err_dialog = gnome_error_dialog_parented
			(_("Please enter a protocol name."),
			 GTK_WINDOW (dialog->p->dialog_win));

		gtk_window_set_modal (GTK_WINDOW (err_dialog), TRUE);

		return FALSE;
	} else {
		for (tmp1 = tmp; *tmp1 != '\0' && isalnum (*tmp1); tmp1++);

		if (*tmp1 != '\0') {
			err_dialog = gnome_error_dialog_parented
				(_("Invalid protocol name. Please enter a protocol name without any spaces or punctuation."),
				 GTK_WINDOW (dialog->p->dialog_win));

			gtk_window_set_modal (GTK_WINDOW (err_dialog), TRUE);

			return FALSE;
		}

		if (dialog->p->is_add) {
			dir = g_strconcat ("/desktop/gnome/url-handlers/", tmp, NULL);
			if (gconf_client_dir_exists (gconf_client_get_default (), dir, NULL)) {
				err_dialog = gnome_error_dialog_parented
					(_("There is already a protocol by that name."),
					 GTK_WINDOW (dialog->p->dialog_win));

				gtk_window_set_modal (GTK_WINDOW (err_dialog), TRUE);

				return FALSE;
			}
			g_free (dir);
		}
	}

	return TRUE;
}

static void
program_sensitive_cb (ServiceEditDialog *dialog, GtkToggleButton *tb) 
{
	if (gtk_toggle_button_get_active (tb))
		gtk_widget_set_sensitive (WID ("program_frame"), TRUE);
	else if (dialog->p->info == NULL || dialog->p->info->protocol == NULL ||
		 strcmp (dialog->p->info->protocol, "ftp"))
		gtk_widget_set_sensitive (WID ("program_frame"), FALSE);
}

static void
program_changed_cb (ServiceEditDialog *dialog, GtkOptionMenu *option_menu)
{
	int id;
	GtkMenuShell *menu;

	menu = GTK_MENU_SHELL (gtk_option_menu_get_menu (option_menu));
	id = gtk_option_menu_get_history (option_menu);

	if (id == g_list_length (menu->children) - 1) {
		gtk_widget_set_sensitive (WID ("program_entry_box"), TRUE);
		gtk_widget_set_sensitive (WID ("needs_terminal_toggle"), TRUE);
	} else {
		gtk_widget_set_sensitive (WID ("program_entry_box"), FALSE);
		gtk_widget_set_sensitive (WID ("needs_terminal_toggle"), FALSE);
	}
}

static void
response_cb (ServiceEditDialog *dialog, gint response_id) 
{
	if (response_id == GTK_RESPONSE_OK) {
		if (validate_data (dialog)) {
			store_data (dialog);
			g_object_unref (G_OBJECT (dialog));
		}
	} else {
		if (dialog->p->is_add)
			service_info_free (dialog->p->info);

		g_object_unref (G_OBJECT (dialog));
	}
}
