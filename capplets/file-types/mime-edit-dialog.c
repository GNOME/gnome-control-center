/* -*- mode: c; style: linux -*- */

/* mime-edit-dialog.c
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
#include <libgnomevfs/gnome-vfs-mime-handlers.h>

#include "mime-edit-dialog.h"

#define WID(x) (glade_xml_get_widget (dialog->p->dialog_xml, x))

enum {
	PROP_0,
	PROP_MIME_TYPE_INFO
};

struct _MimeEditDialogPrivate 
{
	MimeTypeInfo *info;
	GladeXML     *dialog_xml;
	GtkWidget    *dialog_win;
	GtkTreeStore *ext_store;
};

static GObjectClass *parent_class;

static void mime_edit_dialog_init        (MimeEditDialog *mime_edit_dialog,
					  MimeEditDialogClass *class);
static void mime_edit_dialog_class_init  (MimeEditDialogClass *class);
static void mime_edit_dialog_base_init   (MimeEditDialogClass *class);

static void mime_edit_dialog_set_prop    (GObject        *object, 
					  guint           prop_id,
					  const GValue   *value, 
					  GParamSpec     *pspec);
static void mime_edit_dialog_get_prop    (GObject        *object,
					  guint           prop_id,
					  GValue         *value,
					  GParamSpec     *pspec);

static void mime_edit_dialog_dispose     (GObject        *object);
static void mime_edit_dialog_finalize    (GObject        *object);

static void fill_dialog                  (MimeEditDialog *dialog);

static void populate_component_list      (MimeEditDialog *dialog);
static void populate_application_list    (MimeEditDialog *dialog);
static void populate_extensions_list     (MimeEditDialog *dialog);

static void add_ext_cb                   (MimeEditDialog *dialog);
static void remove_ext_cb                (MimeEditDialog *dialog);
static void default_action_changed_cb    (MimeEditDialog *dialog);
static void response_cb                  (MimeEditDialog *dialog,
					  gint            response_id);

GType
mime_edit_dialog_get_type (void)
{
	static GType mime_edit_dialog_type = 0;

	if (!mime_edit_dialog_type) {
		GTypeInfo mime_edit_dialog_info = {
			sizeof (MimeEditDialogClass),
			(GBaseInitFunc) mime_edit_dialog_base_init,
			NULL, /* GBaseFinalizeFunc */
			(GClassInitFunc) mime_edit_dialog_class_init,
			NULL, /* GClassFinalizeFunc */
			NULL, /* user-supplied data */
			sizeof (MimeEditDialog),
			0, /* n_preallocs */
			(GInstanceInitFunc) mime_edit_dialog_init,
			NULL
		};

		mime_edit_dialog_type = 
			g_type_register_static (G_TYPE_OBJECT, 
						"MimeEditDialog",
						&mime_edit_dialog_info, 0);
	}

	return mime_edit_dialog_type;
}

static void
mime_edit_dialog_init (MimeEditDialog *dialog, MimeEditDialogClass *class)
{
	GtkSizeGroup *size_group;
	GtkTreeView *view;
	GtkCellRenderer *renderer;

	dialog->p = g_new0 (MimeEditDialogPrivate, 1);
	dialog->p->dialog_xml = glade_xml_new
		(GNOMECC_DATA_DIR "/interfaces/file-types-properties.glade", "edit_widget", NULL);

	size_group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);
	gtk_size_group_add_widget (size_group, WID ("description_label"));
	gtk_size_group_add_widget (size_group, WID ("mime_type_label"));

	size_group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);
	gtk_size_group_add_widget (size_group, WID ("component_label"));
	gtk_size_group_add_widget (size_group, WID ("default_action_label"));
	gtk_size_group_add_widget (size_group, WID ("program_label"));

	dialog->p->ext_store = gtk_tree_store_new (1, G_TYPE_STRING);

	view = GTK_TREE_VIEW (WID ("ext_list"));
	gtk_tree_view_set_model (view, GTK_TREE_MODEL (dialog->p->ext_store));

	renderer = gtk_cell_renderer_text_new ();
	gtk_tree_view_insert_column_with_attributes (view, -1, _("Extension"), renderer, "text", 0, NULL);

	dialog->p->dialog_win = gtk_dialog_new_with_buttons
		(_("Edit file type"), NULL, -1,
		 GTK_STOCK_OK,     GTK_RESPONSE_OK,
		 GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
		 NULL);

	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog->p->dialog_win)->vbox), WID ("edit_widget"), TRUE, TRUE, 0);

	g_signal_connect_swapped (G_OBJECT (WID ("add_ext_button")), "clicked", (GCallback) add_ext_cb, dialog);
	g_signal_connect_swapped (G_OBJECT (WID ("remove_ext_button")), "clicked", (GCallback) remove_ext_cb, dialog);
	g_signal_connect_swapped (G_OBJECT (WID ("default_action_select")), "changed", (GCallback) default_action_changed_cb, dialog);

	g_signal_connect_swapped (G_OBJECT (dialog->p->dialog_win), "response", (GCallback) response_cb, dialog);
}

static void
mime_edit_dialog_base_init (MimeEditDialogClass *class) 
{
}

static void
mime_edit_dialog_class_init (MimeEditDialogClass *class) 
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (class);

	object_class->dispose = mime_edit_dialog_dispose;
	object_class->finalize = mime_edit_dialog_finalize;
	object_class->set_property = mime_edit_dialog_set_prop;
	object_class->get_property = mime_edit_dialog_get_prop;

	g_object_class_install_property
		(object_class, PROP_MIME_TYPE_INFO,
		 g_param_spec_pointer ("mime-type-info",
				       _("MIME type info"),
				       _("Information on MIME type to edit"),
				       G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

	parent_class = G_OBJECT_CLASS
		(g_type_class_ref (G_TYPE_OBJECT));
}

static void
mime_edit_dialog_set_prop (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec) 
{
	MimeEditDialog *mime_edit_dialog;

	g_return_if_fail (object != NULL);
	g_return_if_fail (IS_MIME_EDIT_DIALOG (object));

	mime_edit_dialog = MIME_EDIT_DIALOG (object);

	switch (prop_id) {
	case PROP_MIME_TYPE_INFO:
		mime_edit_dialog->p->info = g_value_get_pointer (value);
		fill_dialog (mime_edit_dialog);
		break;

	default:
		g_warning ("Bad property set");
		break;
	}
}

static void
mime_edit_dialog_get_prop (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec) 
{
	MimeEditDialog *mime_edit_dialog;

	g_return_if_fail (object != NULL);
	g_return_if_fail (IS_MIME_EDIT_DIALOG (object));

	mime_edit_dialog = MIME_EDIT_DIALOG (object);

	switch (prop_id) {
	case PROP_MIME_TYPE_INFO:
		g_value_set_pointer (value, mime_edit_dialog->p->info);
		break;

	default:
		g_warning ("Bad property get");
		break;
	}
}

static void
mime_edit_dialog_dispose (GObject *object) 
{
	MimeEditDialog *dialog;

	g_return_if_fail (object != NULL);
	g_return_if_fail (IS_MIME_EDIT_DIALOG (object));

	dialog = MIME_EDIT_DIALOG (object);

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
mime_edit_dialog_finalize (GObject *object) 
{
	MimeEditDialog *mime_edit_dialog;

	g_return_if_fail (object != NULL);
	g_return_if_fail (IS_MIME_EDIT_DIALOG (object));

	mime_edit_dialog = MIME_EDIT_DIALOG (object);

	g_free (mime_edit_dialog->p);

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

GObject *
mime_edit_dialog_new (MimeTypeInfo *info) 
{
	return g_object_new (mime_edit_dialog_get_type (),
			     "mime-type-info", info,
			     NULL);
}

GObject *
mime_add_dialog_new (void) 
{
	return g_object_new (mime_edit_dialog_get_type (),
			     "mime-type-info", g_new0 (MimeTypeInfo, 1),
			     NULL);
}

static void
fill_dialog (MimeEditDialog *dialog)
{
	gtk_entry_set_text (GTK_ENTRY (WID ("description_entry")), dialog->p->info->description);
	gtk_entry_set_text (GTK_ENTRY (WID ("mime_type_entry")), dialog->p->info->mime_type);

	if (dialog->p->info->custom_line != NULL)
		gnome_file_entry_set_filename (GNOME_FILE_ENTRY (WID ("program_entry")), dialog->p->info->custom_line);

	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (WID ("needs_terminal_toggle")), dialog->p->info->needs_terminal);

	if (dialog->p->info->mime_type != NULL && *dialog->p->info->mime_type != '\0')
		gtk_widget_set_sensitive (WID ("mime_type_entry"), FALSE);

	if (dialog->p->info->icon_name == NULL)
		gnome_icon_entry_set_filename (GNOME_ICON_ENTRY (WID ("icon_entry")), "nautilus/i-regular-24.png");
	else
		gnome_icon_entry_set_filename (GNOME_ICON_ENTRY (WID ("icon_entry")), dialog->p->info->icon_name);

	populate_component_list (dialog);
	populate_application_list (dialog);
	populate_extensions_list (dialog);

	gtk_widget_show_all (dialog->p->dialog_win);
}

static void
populate_component_list (MimeEditDialog *dialog) 
{
	GList             *component_list, *tmp;
	GtkMenu           *menu;
	GtkWidget         *menu_item;
	GtkOptionMenu     *component_select;
	gchar             *component_name;
	Bonobo_ServerInfo *info;
	int                found_idx = -1, i;

	menu = GTK_MENU (gtk_menu_new ());

	component_list = gnome_vfs_mime_get_short_list_components (dialog->p->info->mime_type);

	for (tmp = component_list, i = 0; tmp != NULL; tmp = tmp->next, i++) {
		info = tmp->data;

		if (!strcmp (info->iid, dialog->p->info->default_component_id))
			found_idx = i;

		component_name = mime_type_get_pretty_name_for_server (info);
		menu_item = gtk_menu_item_new_with_label (component_name);
		g_free (component_name);

		/* Store copy of component name in item; free when item destroyed. */
		g_object_set_data_full (G_OBJECT (menu_item),
					"iid",
					g_strdup (info->iid),
					(GDestroyNotify) g_free);

		gtk_menu_append (menu, menu_item);
		gtk_widget_show (menu_item);
	}

	menu_item = gtk_menu_item_new_with_label (_("None"));
	gtk_menu_append (menu, menu_item);
	gtk_widget_show (menu_item);

	if (found_idx < 0)
		found_idx = i;

	component_select = GTK_OPTION_MENU (WID ("component_select"));
	gtk_option_menu_set_menu (component_select, GTK_WIDGET (menu));
	gtk_option_menu_set_history (component_select, found_idx);

	gnome_vfs_mime_component_list_free (component_list);
}

static void
populate_application_list (MimeEditDialog *dialog) 
{
	GList                   *app_list, *tmp;
	GtkMenu                 *menu;
	GtkWidget               *menu_item;
	GtkOptionMenu           *app_select;
	GnomeVFSMimeApplication *app;
	int                      found_idx = -1, i;

	menu = GTK_MENU (gtk_menu_new ());

	app_list = gnome_vfs_mime_get_short_list_applications (dialog->p->info->mime_type);

	for (tmp = app_list, i = 0; tmp != NULL; tmp = tmp->next, i++) {
		app = tmp->data;

		if (dialog->p->info->default_action != NULL &&
		    !strcmp (app->id, dialog->p->info->default_action->id))
			found_idx = i;

		menu_item = gtk_menu_item_new_with_label (app->name);

		/* Store copy of application in item; free when item destroyed. */
		g_object_set_data_full (G_OBJECT (menu_item),
					"app", app,
					(GDestroyNotify) gnome_vfs_mime_application_free);

		gtk_menu_append (menu, menu_item);
		gtk_widget_show (menu_item);
	}

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
populate_extensions_list (MimeEditDialog *dialog) 
{
	GList *tmp;
	GtkTreeIter iter;

	for (tmp = dialog->p->info->file_extensions; tmp != NULL; tmp = tmp->next) {
		gtk_tree_store_append (dialog->p->ext_store, &iter, NULL);
		gtk_tree_store_set (dialog->p->ext_store, &iter, 0, tmp->data, -1);
	}
}

static GList *
collect_filename_extensions (MimeEditDialog *dialog) 
{
	GtkTreeIter iter;
	GValue value;
	GList *ret = NULL;

	gtk_tree_model_get_iter_root (GTK_TREE_MODEL (dialog->p->ext_store), &iter);

	value.g_type = G_TYPE_INVALID;

	do {
		gtk_tree_model_get_value (GTK_TREE_MODEL (dialog->p->ext_store), &iter, 0, &value);
		ret = g_list_prepend (ret, g_value_dup_string (&value));
		g_value_unset (&value);
	} while (gtk_tree_model_iter_next (GTK_TREE_MODEL (dialog->p->ext_store), &iter));

	return ret;
}

static GList *
merge_ext_lists (GList *list1, GList *list2) 
{
	GList *tmp, *tmp1;

	for (tmp = list2; tmp != NULL; tmp = tmp->next) {
		for (tmp1 = list1; tmp1 != NULL && strcmp (tmp->data, tmp1->data); tmp1 = tmp1->next);

		if (tmp1 == NULL)
			list1 = g_list_prepend (list1, g_strdup (tmp->data));
	}

	return list1;
}

static void
store_data (MimeEditDialog *dialog) 
{
	GtkOptionMenu *option_menu;
	GtkMenuShell  *menu_shell;
	GObject       *menu_item;
	gint           idx;

	GList         *ext_list;

	GnomeVFSMimeApplication *app;

	g_free (dialog->p->info->description);
	dialog->p->info->description = g_strdup (gtk_entry_get_text (GTK_ENTRY (WID ("description_entry"))));

	g_free (dialog->p->info->mime_type);
	dialog->p->info->mime_type = g_strdup (gtk_entry_get_text (GTK_ENTRY (WID ("mime_type_entry"))));

	g_free (dialog->p->info->icon_name);
	dialog->p->info->icon_name = g_strdup (gnome_icon_entry_get_filename (GNOME_ICON_ENTRY (WID ("icon_entry"))));

	option_menu = GTK_OPTION_MENU (WID ("component_select"));
	menu_shell = GTK_MENU_SHELL (gtk_option_menu_get_menu (option_menu));
	idx = gtk_option_menu_get_history (option_menu);
	menu_item = (g_list_nth (menu_shell->children, idx))->data;

	g_free (dialog->p->info->default_component_id);
	dialog->p->info->default_component_id = g_strdup (g_object_get_data (menu_item, "iid"));

	option_menu = GTK_OPTION_MENU (WID ("default_action_select"));
	menu_shell = GTK_MENU_SHELL (gtk_option_menu_get_menu (option_menu));
	idx = gtk_option_menu_get_history (option_menu);
	menu_item = (g_list_nth (menu_shell->children, idx))->data;

	gnome_vfs_mime_application_free (dialog->p->info->default_action);
	app = g_object_get_data (menu_item, "app");
	if (app != NULL)
		dialog->p->info->default_action = gnome_vfs_mime_application_copy (app);
	else
		dialog->p->info->default_action = NULL;

	g_free (dialog->p->info->custom_line);
	dialog->p->info->custom_line = g_strdup (gnome_file_entry_get_full_path (GNOME_FILE_ENTRY (WID ("program_entry")), FALSE));

	dialog->p->info->needs_terminal = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (WID ("needs_terminal_toggle")));

	ext_list = collect_filename_extensions (dialog);
	dialog->p->info->file_extensions = merge_ext_lists (dialog->p->info->file_extensions, ext_list);
	g_list_foreach (ext_list, (GFunc) g_free, NULL);
	g_list_free (ext_list);

	mime_type_info_update (dialog->p->info);
	mime_type_append_to_dirty_list (dialog->p->info);
}

static void
add_ext_cb (MimeEditDialog *dialog)
{
	GtkTreeIter iter;
	const gchar *ext_name;

	ext_name = gtk_entry_get_text (GTK_ENTRY (WID ("new_ext_entry")));

	if (ext_name != NULL && *ext_name != '\0') {
		gtk_tree_store_append (dialog->p->ext_store, &iter, NULL);
		gtk_tree_store_set (dialog->p->ext_store, &iter, 0, ext_name, -1);
	}
}

static void
remove_ext_foreach_cb (GtkTreeModel *model, GtkTreePath *path,
		       GtkTreeIter *iter) 
{
	gtk_tree_store_remove (GTK_TREE_STORE (model), iter);
}

static void
remove_ext_cb (MimeEditDialog *dialog)
{
	gtk_tree_selection_selected_foreach (gtk_tree_view_get_selection (GTK_TREE_VIEW (WID ("ext_list"))),
					     (GtkTreeSelectionForeachFunc) remove_ext_foreach_cb, NULL);
}

static void
default_action_changed_cb (MimeEditDialog *dialog)
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
response_cb (MimeEditDialog *dialog, gint response_id) 
{
	if (response_id == GTK_RESPONSE_OK)
		store_data (dialog);

	g_object_unref (G_OBJECT (dialog));
}
