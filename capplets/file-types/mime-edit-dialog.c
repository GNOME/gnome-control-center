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
#include <libgnomevfs/gnome-vfs-utils.h>

#include "mime-edit-dialog.h"
#include "mime-types-model.h"

#include "libuuid/uuid.h"

#define WID(x) (glade_xml_get_widget (dialog->p->dialog_xml, x))

enum {
	PROP_0,
	PROP_MODEL,
	PROP_INFO,
	PROP_IS_ADD
};

struct _MimeEditDialogPrivate 
{
	MimeTypeInfo *info;
	GladeXML     *dialog_xml;
	GtkWidget    *dialog_win;
	GtkTreeStore *ext_store;

	gboolean      is_add;

	GtkTreeModel *model;

	gboolean      component_active         : 1;
	gboolean      default_action_active    : 1;
	gboolean      custom_action            : 1;
	gboolean      use_cat_dfl              : 1;
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
static void setup_add_dialog             (MimeEditDialog *dialog);

static void populate_component_list      (MimeEditDialog *dialog);
static void populate_application_list    (MimeEditDialog *dialog);
static void populate_extensions_list     (MimeEditDialog *dialog);

static void add_ext_cb                   (MimeEditDialog *dialog);
static void remove_ext_cb                (MimeEditDialog *dialog);
static void choose_cat_cb                (MimeEditDialog *dialog);
static void default_action_changed_cb    (MimeEditDialog *dialog);
static void use_category_defaults_toggled_cb (MimeEditDialog  *dialog,
					      GtkToggleButton *tb);
static void response_cb                  (MimeEditDialog *dialog,
					  gint            response_id);

static void update_sensitivity           (MimeEditDialog *dialog);

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
	gtk_size_group_add_widget (size_group, WID ("category_label"));

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
	g_signal_connect_swapped (G_OBJECT (WID ("choose_button")), "clicked", (GCallback) choose_cat_cb, dialog);
	g_signal_connect_swapped (G_OBJECT (WID ("default_action_select")), "changed", (GCallback) default_action_changed_cb, dialog);
	g_signal_connect_swapped (G_OBJECT (WID ("use_category_defaults_toggle")), "toggled",
				  (GCallback) use_category_defaults_toggled_cb, dialog);

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
		(object_class, PROP_MODEL,
		 g_param_spec_object ("model",
				      _("Model"),
				      _("Underlying model to notify when Ok is clicked"),
				      gtk_tree_model_get_type (),
				      G_PARAM_READWRITE));

	g_object_class_install_property
		(object_class, PROP_INFO,
		 g_param_spec_pointer ("mime-type-info",
				       _("MIME type information"),
				       _("Structure with data on the MIME type"),
				       G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

	g_object_class_install_property
		(object_class, PROP_IS_ADD,
		 g_param_spec_boolean ("is-add",
				       _("Is add dialog"),
				       _("True if this dialog is for adding a MIME type"),
				       FALSE,
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
	case PROP_MODEL:
		mime_edit_dialog->p->model = GTK_TREE_MODEL (g_value_get_object (value));
		break;

	case PROP_INFO:
		if (g_value_get_pointer (value) != NULL) {
			mime_edit_dialog->p->info = g_value_get_pointer (value);
			fill_dialog (mime_edit_dialog);

			gtk_widget_show_all (mime_edit_dialog->p->dialog_win);
		}

		break;

	case PROP_IS_ADD:
		mime_edit_dialog->p->is_add = g_value_get_boolean (value);

		if (mime_edit_dialog->p->is_add) {
			mime_edit_dialog->p->info = mime_type_info_new (NULL, NULL);
			setup_add_dialog (mime_edit_dialog);
			gtk_widget_show_all (mime_edit_dialog->p->dialog_win);
		}

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
	case PROP_MODEL:
		g_value_set_object (value, G_OBJECT (mime_edit_dialog->p->model));
		break;

	case PROP_INFO:
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
mime_edit_dialog_new (GtkTreeModel *model, MimeTypeInfo *info) 
{
	return g_object_new (mime_edit_dialog_get_type (),
			     "model", model,
			     "mime-type-info", info,
			     NULL);
}

GObject *
mime_add_dialog_new (GtkTreeModel *model) 
{
	return g_object_new (mime_edit_dialog_get_type (),
			     "model", model,
			     "is-add", TRUE,
			     NULL);
}

static void
fill_dialog (MimeEditDialog *dialog)
{
	mime_type_info_load_all (dialog->p->info);

	gtk_entry_set_text (GTK_ENTRY (WID ("description_entry")), dialog->p->info->description);
	gtk_entry_set_text (GTK_ENTRY (WID ("mime_type_entry")), dialog->p->info->mime_type);
	gtk_entry_set_text (GTK_ENTRY (WID ("category_entry")), mime_type_info_get_category_name (dialog->p->info));

	dialog->p->use_cat_dfl = dialog->p->info->use_category;
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (WID ("use_category_defaults_toggle")), dialog->p->use_cat_dfl);
	update_sensitivity (dialog);

	if (dialog->p->info->custom_line != NULL)
		gnome_file_entry_set_filename (GNOME_FILE_ENTRY (WID ("program_entry")), dialog->p->info->custom_line);

	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (WID ("needs_terminal_toggle")), dialog->p->info->needs_terminal);

	if (dialog->p->info->mime_type != NULL && *dialog->p->info->mime_type != '\0')
		gtk_widget_set_sensitive (WID ("mime_type_entry"), FALSE);

	gnome_icon_entry_set_filename (GNOME_ICON_ENTRY (WID ("icon_entry")), mime_type_info_get_icon_path (dialog->p->info));

	populate_component_list (dialog);
	populate_application_list (dialog);
	populate_extensions_list (dialog);
}

static void
setup_add_dialog (MimeEditDialog *dialog)
{
	GtkWidget *menu, *item;

	item = gtk_menu_item_new_with_label (_("None"));
	menu = gtk_menu_new ();
	gtk_menu_append (GTK_MENU (menu), item);
	gtk_option_menu_set_menu (GTK_OPTION_MENU (WID ("component_select")), menu);

	item = gtk_menu_item_new_with_label (_("Custom"));
	menu = gtk_menu_new ();
	gtk_menu_append (GTK_MENU (menu), item);
	gtk_option_menu_set_menu (GTK_OPTION_MENU (WID ("default_action_select")), menu);

	gtk_widget_set_sensitive (WID ("component_box"), FALSE);
	gtk_widget_set_sensitive (WID ("default_action_box"), FALSE);

	gnome_icon_entry_set_filename (GNOME_ICON_ENTRY (WID ("icon_entry")),
				       gnome_vfs_icon_path_from_filename ("nautilus/i-regular-24.png"));
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

	/* FIXME: We are leaking the whole list here, but this will be the case until I know of an easy way to duplicate
	 * Bonobo_ServerInfo structures */

	for (tmp = component_list, i = 0; tmp != NULL; tmp = tmp->next, i++) {
		info = tmp->data;

		if (!strcmp (info->iid, dialog->p->info->default_component->iid))
			found_idx = i;

		component_name = mime_type_get_pretty_name_for_server (info);
		menu_item = gtk_menu_item_new_with_label (component_name);
		g_free (component_name);

		/* Store copy of component name in item; free when item destroyed. */
		g_object_set_data (G_OBJECT (menu_item),
				   "component", info);

		gtk_menu_append (menu, menu_item);
		gtk_widget_show (menu_item);
	}

	dialog->p->component_active = !(i == 0);

	menu_item = gtk_menu_item_new_with_label (_("None"));
	gtk_menu_append (menu, menu_item);
	gtk_widget_show (menu_item);

	if (found_idx < 0)
		found_idx = i;

	component_select = GTK_OPTION_MENU (WID ("component_select"));
	gtk_option_menu_set_menu (component_select, GTK_WIDGET (menu));
	gtk_option_menu_set_history (component_select, found_idx);

	update_sensitivity (dialog);
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

	dialog->p->default_action_active = !(i == 0);
	dialog->p->custom_action = (found_idx < 0);

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

	update_sensitivity (dialog);
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

	if (!gtk_tree_model_get_iter_root (GTK_TREE_MODEL (dialog->p->ext_store), &iter))
		return NULL;

	value.g_type = G_TYPE_INVALID;

	do {
		gtk_tree_model_get_value (GTK_TREE_MODEL (dialog->p->ext_store), &iter, 0, &value);
		ret = g_list_prepend (ret, g_value_dup_string (&value));
		g_value_unset (&value);
	} while (gtk_tree_model_iter_next (GTK_TREE_MODEL (dialog->p->ext_store), &iter));

	ret = g_list_reverse (ret);

	return ret;
}

static void
store_data (MimeEditDialog *dialog) 
{
	GtkOptionMenu *option_menu;
	GtkMenuShell  *menu_shell;
	GObject       *menu_item;
	gint           idx;
	gchar         *tmp;
	const gchar   *tmp1;
	gboolean       cat_changed = FALSE;

	GList         *ext_list;

	uuid_t         mime_uuid;
	gchar          mime_uuid_str[100];

	GnomeVFSMimeApplication *app;

	GtkTreeIter    iter;
	GtkTreePath   *path;

	g_free (dialog->p->info->description);
	dialog->p->info->description = g_strdup (gtk_entry_get_text (GTK_ENTRY (WID ("description_entry"))));

	g_free (dialog->p->info->mime_type);
	tmp1 = gtk_entry_get_text (GTK_ENTRY (WID ("mime_type_entry")));

	if (tmp1 != NULL && *tmp1 != '\0') {
		dialog->p->info->mime_type = g_strdup (tmp1);
	} else {
		uuid_generate (mime_uuid);
		uuid_unparse (mime_uuid, mime_uuid_str);

		dialog->p->info->mime_type = g_strconcat ("custom/", mime_uuid_str, NULL);
	}

	g_free (dialog->p->info->icon_path);
	dialog->p->info->icon_path = NULL;

	g_free (dialog->p->info->icon_name);
	dialog->p->info->icon_name = g_strdup (gnome_icon_entry_get_filename (GNOME_ICON_ENTRY (WID ("icon_entry"))));

	if (dialog->p->info->icon_pixbuf != NULL) {
		g_object_unref (G_OBJECT (dialog->p->info->icon_pixbuf));
		dialog->p->info->icon_pixbuf = NULL;
	}

	if (dialog->p->info->small_icon_pixbuf != NULL) {
		g_object_unref (G_OBJECT (dialog->p->info->small_icon_pixbuf));
		dialog->p->info->small_icon_pixbuf = NULL;
	}

	dialog->p->info->use_category =
		gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (WID ("use_category_defaults_toggle")));

	option_menu = GTK_OPTION_MENU (WID ("component_select"));
	menu_shell = GTK_MENU_SHELL (gtk_option_menu_get_menu (option_menu));
	idx = gtk_option_menu_get_history (option_menu);
	menu_item = (g_list_nth (menu_shell->children, idx))->data;

	CORBA_free (dialog->p->info->default_component);
	dialog->p->info->default_component = g_object_get_data (menu_item, "component");

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
	mime_type_info_set_file_extensions (dialog->p->info, ext_list);

	tmp = mime_type_info_get_category_name (dialog->p->info);
	tmp1 = gtk_entry_get_text (GTK_ENTRY (WID ("category_entry")));
	if (strcmp (tmp, tmp1)) {
		cat_changed = TRUE;
		mime_type_info_set_category_name (dialog->p->info, tmp1, dialog->p->model);
	}
	g_free (tmp);

	model_entry_append_to_dirty_list (MODEL_ENTRY (dialog->p->info));

	if (!cat_changed) {
		mime_types_model_construct_iter (MIME_TYPES_MODEL (dialog->p->model),
						 MODEL_ENTRY (dialog->p->info), &iter);
		path = gtk_tree_model_get_path (dialog->p->model, &iter);
		gtk_tree_model_row_changed (dialog->p->model, path, &iter);
		gtk_tree_path_free (path);
	}
}

static gboolean
validate_data (MimeEditDialog *dialog) 
{
	const gchar *tmp;
	GtkWidget *err_dialog;

	tmp = gtk_entry_get_text (GTK_ENTRY (WID ("mime_type_entry")));

	if (tmp != NULL && *tmp != '\0') {
		if (strchr (tmp, ' ') || !strchr (tmp, '/')) {
			err_dialog = gnome_error_dialog_parented
				(_("Invalid MIME type. Please enter a valid MIME type, or"
				   "leave the field blank to have one generated for you."),
				 GTK_WINDOW (dialog->p->dialog_win));

			gtk_window_set_modal (GTK_WINDOW (err_dialog), TRUE);

			return FALSE;
		}
		else if (dialog->p->is_add && (gnome_vfs_mime_type_is_known (tmp) || get_mime_type_info (tmp) != NULL)) {
			err_dialog = gnome_error_dialog_parented
				(_("There already exists a MIME type of that name."),
				 GTK_WINDOW (dialog->p->dialog_win));

			gtk_window_set_modal (GTK_WINDOW (err_dialog), TRUE);

			return FALSE;
		}
	}

	return TRUE;
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

	gtk_entry_set_text (GTK_ENTRY (WID ("new_ext_entry")), "");
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
choose_cat_cb (MimeEditDialog *dialog)
{
	GtkTreeModel     *model;
	GtkTreeSelection *selection;
	GtkTreeIter       iter;
	GtkWidget        *treeview;
	GtkWidget        *dialog_win;
	GtkWidget        *scrolled_win;
	GtkCellRenderer  *renderer;

	model = GTK_TREE_MODEL (mime_types_model_new (TRUE));
	treeview = gtk_tree_view_new_with_model (model);
	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview));
	gtk_tree_selection_set_mode (selection, GTK_SELECTION_SINGLE);
	
	if (dialog->p->info->entry.parent != NULL) {
		mime_types_model_construct_iter (MIME_TYPES_MODEL (model), dialog->p->info->entry.parent, &iter);
		gtk_tree_selection_select_iter (selection, &iter);
	}

	renderer = gtk_cell_renderer_text_new ();
	gtk_tree_view_insert_column_with_attributes
		(GTK_TREE_VIEW (treeview), -1, _("Category"), renderer,
		 "text", MODEL_COLUMN_DESCRIPTION,
		 NULL);

	dialog_win = gtk_dialog_new_with_buttons
		(_("Choose a file category"), NULL, -1,
		 GTK_STOCK_OK,     GTK_RESPONSE_OK,
		 GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
		 NULL);

	gtk_widget_set_usize (dialog_win, 300, 300);

	scrolled_win = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_win), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	gtk_container_add (GTK_CONTAINER (scrolled_win), treeview);

	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog_win)->vbox), scrolled_win, TRUE, TRUE, GNOME_PAD_SMALL);
	gtk_widget_show_all (dialog_win);

	if (gtk_dialog_run (GTK_DIALOG (dialog_win)) == GTK_RESPONSE_OK) {
		gtk_tree_selection_get_selected (selection, &model, &iter);
		gtk_entry_set_text (GTK_ENTRY (WID ("category_entry")),
				    mime_type_info_get_category_name (MIME_TYPE_INFO (MODEL_ENTRY_FROM_ITER (&iter))));
	}

	gtk_widget_destroy (dialog_win);
	g_object_unref (G_OBJECT (model));
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

	dialog->p->custom_action = (id == g_list_length (menu->children) - 1);

	update_sensitivity (dialog);
}

static void
use_category_defaults_toggled_cb (MimeEditDialog *dialog, GtkToggleButton *tb)
{
	dialog->p->use_cat_dfl = gtk_toggle_button_get_active (tb);
	update_sensitivity (dialog);
}

static void
response_cb (MimeEditDialog *dialog, gint response_id) 
{
	if (response_id == GTK_RESPONSE_OK) {
		if (validate_data (dialog)) {
			store_data (dialog);
			g_object_unref (G_OBJECT (dialog));
		}
	} else {
		if (dialog->p->is_add)
			mime_type_info_free (dialog->p->info);

		g_object_unref (G_OBJECT (dialog));
	}
}

static void
update_sensitivity (MimeEditDialog *dialog) 
{
	gtk_widget_set_sensitive (WID ("component_box"), dialog->p->component_active && !dialog->p->use_cat_dfl);
	gtk_widget_set_sensitive (WID ("default_action_box"), dialog->p->default_action_active && !dialog->p->use_cat_dfl);
	gtk_widget_set_sensitive (WID ("program_entry_box"), dialog->p->custom_action && !dialog->p->use_cat_dfl);
	gtk_widget_set_sensitive (WID ("needs_terminal_toggle"), dialog->p->custom_action && !dialog->p->use_cat_dfl);
}
