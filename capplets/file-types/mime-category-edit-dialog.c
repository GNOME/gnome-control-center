/* -*- mode: c; style: linux -*- */

/* mime-category-edit-dialog.c
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
#include <libgnomevfs/gnome-vfs-application-registry.h>
#include <libgnomevfs/gnome-vfs-mime-handlers.h>

#include "mime-types-model.h"
#include "mime-type-info.h"
#include "mime-category-edit-dialog.h"

#define WID(x) (glade_xml_get_widget (dialog->p->dialog_xml, x))

enum {
	PROP_0,
	PROP_MODEL,
	PROP_INFO
};

struct _MimeCategoryEditDialogPrivate 
{
	GladeXML         *dialog_xml;
	GtkWidget        *dialog_win;
	MimeCategoryInfo *info;

	GtkTreeModel     *model;
};

static GObjectClass *parent_class;

static void mime_category_edit_dialog_init        (MimeCategoryEditDialog *dialog,
						   MimeCategoryEditDialogClass *class);
static void mime_category_edit_dialog_class_init  (MimeCategoryEditDialogClass *class);
static void mime_category_edit_dialog_base_init   (MimeCategoryEditDialogClass *class);

static void mime_category_edit_dialog_set_prop    (GObject      *object, 
						   guint         prop_id,
						   const GValue *value, 
						   GParamSpec   *pspec);
static void mime_category_edit_dialog_get_prop    (GObject      *object,
						   guint         prop_id,
						   GValue       *value,
						   GParamSpec   *pspec);

static void mime_category_edit_dialog_dispose     (GObject *object);
static void mime_category_edit_dialog_finalize    (GObject *object);

static void populate_application_list             (MimeCategoryEditDialog *dialog);

static void fill_dialog                           (MimeCategoryEditDialog *dialog);
static void store_data                            (MimeCategoryEditDialog *dialog);

static void default_action_changed_cb             (MimeCategoryEditDialog *dialog);
static void response_cb                           (MimeCategoryEditDialog *dialog,
						   gint                    response_id);

GType
mime_category_edit_dialog_get_type (void)
{
	static GType mime_category_edit_dialog_type = 0;

	if (!mime_category_edit_dialog_type) {
		GTypeInfo mime_category_edit_dialog_info = {
			sizeof (MimeCategoryEditDialogClass),
			(GBaseInitFunc) mime_category_edit_dialog_base_init,
			NULL, /* GBaseFinalizeFunc */
			(GClassInitFunc) mime_category_edit_dialog_class_init,
			NULL, /* GClassFinalizeFunc */
			NULL, /* user-supplied data */
			sizeof (MimeCategoryEditDialog),
			0, /* n_preallocs */
			(GInstanceInitFunc) mime_category_edit_dialog_init,
			NULL
		};

		mime_category_edit_dialog_type = 
			g_type_register_static (G_TYPE_OBJECT, 
						"MimeCategoryEditDialog",
						&mime_category_edit_dialog_info, 0);
	}

	return mime_category_edit_dialog_type;
}

static void
mime_category_edit_dialog_init (MimeCategoryEditDialog *dialog, MimeCategoryEditDialogClass *class)
{
	GtkSizeGroup *size_group;

	dialog->p = g_new0 (MimeCategoryEditDialogPrivate, 1);
	dialog->p->dialog_xml = glade_xml_new
		(GNOMECC_DATA_DIR "/interfaces/file-types-properties.glade", "mime_category_edit_widget", NULL);

	dialog->p->model = NULL;
	dialog->p->info = NULL;

	size_group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);
	gtk_size_group_add_widget (size_group, WID ("default_action_label"));
	gtk_size_group_add_widget (size_group, WID ("program_label"));

	gtk_widget_set_sensitive (WID ("name_box"), FALSE);

	dialog->p->dialog_win = gtk_dialog_new_with_buttons
		(_("Edit file category"), NULL, -1,
		 GTK_STOCK_OK,     GTK_RESPONSE_OK,
		 GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
		 NULL);

	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog->p->dialog_win)->vbox), WID ("mime_category_edit_widget"), TRUE, TRUE, 0);

	g_signal_connect_swapped (G_OBJECT (WID ("default_action_select")), "changed", (GCallback) default_action_changed_cb, dialog);

	g_signal_connect_swapped (G_OBJECT (dialog->p->dialog_win), "response", (GCallback) response_cb, dialog);
}

static void
mime_category_edit_dialog_base_init (MimeCategoryEditDialogClass *class) 
{
}

static void
mime_category_edit_dialog_class_init (MimeCategoryEditDialogClass *class) 
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (class);

	object_class->dispose = mime_category_edit_dialog_dispose;
	object_class->finalize = mime_category_edit_dialog_finalize;
	object_class->set_property = mime_category_edit_dialog_set_prop;
	object_class->get_property = mime_category_edit_dialog_get_prop;

	g_object_class_install_property
		(object_class, PROP_MODEL,
		 g_param_spec_pointer ("model",
				      _("Model"),
				      _("GtkTreeModel that contains the category data"),
				      G_PARAM_READWRITE));
	g_object_class_install_property
		(object_class, PROP_INFO,
		 g_param_spec_pointer ("mime-cat-info",
				      _("MIME category info"),
				      _("Structure containing information on the MIME category"),
				      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

	parent_class = G_OBJECT_CLASS
		(g_type_class_ref (G_TYPE_OBJECT));
}

static void
mime_category_edit_dialog_set_prop (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec) 
{
	MimeCategoryEditDialog *dialog;

	g_return_if_fail (object != NULL);
	g_return_if_fail (IS_MIME_CATEGORY_EDIT_DIALOG (object));

	dialog = MIME_CATEGORY_EDIT_DIALOG (object);

	switch (prop_id) {
	case PROP_MODEL:
		dialog->p->model = g_value_get_pointer (value);

		if (dialog->p->info != NULL)
			fill_dialog (dialog);

		break;

	case PROP_INFO:
		dialog->p->info = g_value_get_pointer (value);

		if (dialog->p->model != NULL)
			fill_dialog (dialog);

		break;

	default:
		g_warning ("Bad property set");
		break;
	}
}

static void
mime_category_edit_dialog_get_prop (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec) 
{
	MimeCategoryEditDialog *dialog;

	g_return_if_fail (object != NULL);
	g_return_if_fail (IS_MIME_CATEGORY_EDIT_DIALOG (object));

	dialog = MIME_CATEGORY_EDIT_DIALOG (object);

	switch (prop_id) {
	case PROP_MODEL:
		g_value_set_pointer (value, dialog->p->model);
		break;

	case PROP_INFO:
		g_value_set_pointer (value, dialog->p->info);
		break;

	default:
		g_warning ("Bad property get");
		break;
	}
}

static void
mime_category_edit_dialog_dispose (GObject *object) 
{
	MimeCategoryEditDialog *dialog;

	g_return_if_fail (object != NULL);
	g_return_if_fail (IS_MIME_CATEGORY_EDIT_DIALOG (object));

	dialog = MIME_CATEGORY_EDIT_DIALOG (object);

	if (dialog->p->dialog_xml != NULL) {
		g_object_unref (G_OBJECT (dialog->p->dialog_xml));
		dialog->p->dialog_xml = NULL;
	}

	if (dialog->p->dialog_win != NULL) {
		gtk_widget_destroy (GTK_WIDGET (dialog->p->dialog_win));
		dialog->p->dialog_win = NULL;
	}

	G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
mime_category_edit_dialog_finalize (GObject *object) 
{
	MimeCategoryEditDialog *mime_category_edit_dialog;

	g_return_if_fail (object != NULL);
	g_return_if_fail (IS_MIME_CATEGORY_EDIT_DIALOG (object));

	mime_category_edit_dialog = MIME_CATEGORY_EDIT_DIALOG (object);

	g_free (mime_category_edit_dialog->p);

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

GObject *
mime_category_edit_dialog_new (GtkTreeModel *model, MimeCategoryInfo *info) 
{
	return g_object_new (mime_category_edit_dialog_get_type (),
			     "model", model,
			     "mime-cat-info", info,
			     NULL);
}

static void
fill_dialog (MimeCategoryEditDialog *dialog)
{
	mime_category_info_load_all (dialog->p->info);

	gtk_entry_set_text (GTK_ENTRY (WID ("name_entry")), dialog->p->info->name);

	populate_application_list (dialog);

	gtk_widget_show_all (dialog->p->dialog_win);
}

/* FIXME: This should be factored with corresponding functions in mime-edit-dialog.c and service-edit-dialog.c */

static void
populate_application_list (MimeCategoryEditDialog *dialog) 
{
	GList                   *app_list, *tmp;
	GtkMenu                 *menu;
	GtkWidget               *menu_item;
	GtkOptionMenu           *app_select;
	GnomeVFSMimeApplication *app;
	int                      found_idx = -1, i;

	menu = GTK_MENU (gtk_menu_new ());

	app_list = mime_category_info_find_apps (dialog->p->info);

	for (tmp = app_list, i = 0; tmp != NULL; tmp = tmp->next, i++) {
		app = gnome_vfs_application_registry_get_mime_application (tmp->data);
		if (dialog->p->info->default_action != NULL &&
		    !strcmp (tmp->data, dialog->p->info->default_action->id))
			found_idx = i;

		menu_item = gtk_menu_item_new_with_label (app->name);

		/* Store copy of application in item; free when item destroyed. */
		g_object_set_data_full (G_OBJECT (menu_item),
					"app", app,
					(GDestroyNotify) g_free);

		gtk_menu_append (menu, menu_item);
		gtk_widget_show (menu_item);
	}

	if (i == 0)
		gtk_widget_set_sensitive (WID ("default_action_box"), FALSE);

	gtk_menu_append (menu, gtk_menu_item_new_with_label (_("Custom")));

	if (found_idx < 0) {
		found_idx = i;
		if (dialog->p->info->custom_line != NULL)
			gnome_file_entry_set_filename (GNOME_FILE_ENTRY (WID ("program_entry")),
						       dialog->p->info->custom_line);
	} else {
		gtk_widget_set_sensitive (WID ("program_entry_box"), FALSE);
	}

	app_select = GTK_OPTION_MENU (WID ("default_action_select"));
	gtk_option_menu_set_menu (app_select, GTK_WIDGET (menu));
	gtk_option_menu_set_history (app_select, found_idx);

	g_list_free (app_list);
}

static void
store_data (MimeCategoryEditDialog *dialog)
{
	GtkTreePath *path;
	GtkTreeIter  iter;

	model_entry_append_to_dirty_list (MODEL_ENTRY (dialog->p->info));

	mime_types_model_construct_iter (MIME_TYPES_MODEL (dialog->p->model), MODEL_ENTRY (dialog->p->info), &iter);
	path = gtk_tree_model_get_path (dialog->p->model, &iter);
	gtk_tree_model_row_changed (dialog->p->model, path, &iter);
	gtk_tree_path_free (path);
}

static void
default_action_changed_cb (MimeCategoryEditDialog *dialog)
{
	int id;
	GtkOptionMenu *option_menu;
	GtkMenuShell *menu;

	option_menu = GTK_OPTION_MENU (WID ("default_action_select"));
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
response_cb (MimeCategoryEditDialog *dialog, gint response_id)
{
	if (response_id == GTK_RESPONSE_OK)
		store_data (dialog);

	g_object_unref (G_OBJECT (dialog));
}
