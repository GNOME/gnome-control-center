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

#include <string.h>
#include <glade/glade.h>
#include <libgnomevfs/gnome-vfs-mime-handlers.h>
#include <libgnomevfs/gnome-vfs-application-registry.h>
#include <libgnomevfs/gnome-vfs-utils.h>

#include "mime-edit-dialog.h"
#include "mime-types-model.h"

#include "libuuid/uuid.h"

#define WID(x) (glade_xml_get_widget (dialog->p->dialog_xml, x))

enum {
	DONE,
	LAST_SIGNAL
};

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

static guint dialog_signals[LAST_SIGNAL];

static GObjectClass *parent_class;

static void mime_edit_dialog_init        (MimeEditDialog *mime_edit_dialog,
					  MimeEditDialogClass *class);
static void mime_edit_dialog_class_init  (MimeEditDialogClass *class);

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
			(GBaseInitFunc) NULL,
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

/**
 * mime_edit_editable_enters: Make the "activate" signal of an editable click
 * the default dialog button.
 * @dialog: dialog to affect.
 * @editable: Editable to affect.
 *
 * This is a literal copy of gnome_dialog_editable_enters, but not restricted
 * to GnomeDialogs.
 *
 * Normally if there's an editable widget (such as #GtkEntry) in your
 * dialog, pressing Enter will activate the editable rather than the
 * default dialog button. However, in most cases, the user expects to
 * type something in and then press enter to close the dialog. This
 * function enables that behavior.
 *
 **/
static void
mime_edit_editable_enters (MimeEditDialog *dialog, GtkEditable *editable)
{
	g_signal_connect_swapped (G_OBJECT (editable),
		"activate",
		G_CALLBACK (gtk_window_activate_default),
		GTK_WINDOW (dialog->p->dialog_win));
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
		 GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
		 GTK_STOCK_OK,     GTK_RESPONSE_OK,
		 NULL);
	gtk_dialog_set_default_response (GTK_DIALOG (dialog->p->dialog_win),
		 GTK_RESPONSE_OK);

	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog->p->dialog_win)->vbox), WID ("edit_widget"), TRUE, TRUE, 0);

	g_signal_connect_swapped (G_OBJECT (WID ("add_ext_button")), "clicked", (GCallback) add_ext_cb, dialog);
	g_signal_connect_swapped (G_OBJECT (WID ("remove_ext_button")), "clicked", (GCallback) remove_ext_cb, dialog);
	g_signal_connect_swapped (G_OBJECT (WID ("choose_button")), "clicked", (GCallback) choose_cat_cb, dialog);
	g_signal_connect_swapped (G_OBJECT (WID ("default_action_select")), "changed", (GCallback) default_action_changed_cb, dialog);

	g_signal_connect_swapped (G_OBJECT (dialog->p->dialog_win), "response", (GCallback) response_cb, dialog);

	mime_edit_editable_enters (dialog, GTK_EDITABLE (WID ("description_entry")));
	mime_edit_editable_enters (dialog, GTK_EDITABLE (WID ("mime_type_entry")));
	mime_edit_editable_enters (dialog, GTK_EDITABLE (WID ("category_entry")));
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

	dialog_signals[DONE] =
		g_signal_new ("done",
			      G_TYPE_FROM_CLASS (object_class), 0,
			      G_STRUCT_OFFSET (MimeEditDialogClass, done),
			      NULL, NULL,
			      (GSignalCMarshaller) g_cclosure_marshal_VOID__BOOLEAN,
			      G_TYPE_NONE, 1, G_TYPE_BOOLEAN);

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
			mime_edit_dialog->p->info = mime_type_info_new (NULL,
				mime_edit_dialog->p->model);
			setup_add_dialog (mime_edit_dialog);
			gtk_window_set_title (GTK_WINDOW (mime_edit_dialog->p->dialog_win),
				(_("Add file type")));
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
mime_add_dialog_new (GtkTreeModel *model, GtkWindow *parent,
		     char const *file_name) 
{
	GObject *dialog = g_object_new (mime_edit_dialog_get_type (),
		"model", model,	/* must be before is-add */
		NULL);
	g_object_set (dialog,
		      "is-add", TRUE,
		      NULL);
	if (parent != NULL)
		gtk_window_set_transient_for (
			GTK_WINDOW (MIME_EDIT_DIALOG (dialog)->p->dialog_win),
			parent);

	if (file_name != NULL) {
		/* quick and dirty, no tests for backslashed dots */
		char const *last = g_utf8_strrchr (file_name, -1,
			g_utf8_get_char ("."));

		if (last != NULL && last[1] != '\0') {
			MimeTypeInfo *info = MIME_EDIT_DIALOG (dialog)->p->info;
			char *lower = g_utf8_strdown (last +1, -1);

			info->mime_type = g_strdup_printf ("application/x-%s", lower);
			g_free (lower);

			mime_type_info_set_file_extensions (info,
				g_list_prepend (NULL, g_strdup (last+1)));
			mime_type_info_set_category_name (info,
				"Misc", _("Misc"), model);

			fill_dialog (MIME_EDIT_DIALOG (dialog));
		}
	}

	return dialog;
}

static void
safe_set_entry (MimeEditDialog *dialog, char const *widget, char const *txt)
{
	GtkEntry *entry =  GTK_ENTRY (WID (widget));

	g_return_if_fail (entry != NULL);

	if (txt == NULL)
		txt = "";
	gtk_entry_set_text (entry, txt);
}

static void
fill_dialog (MimeEditDialog *dialog)
{
	g_return_if_fail (dialog->p->info != NULL);

	mime_type_info_load_all (dialog->p->info);

	safe_set_entry (dialog, "description_entry",
		dialog->p->info->description);
	safe_set_entry (dialog, "mime_type_entry",
		dialog->p->info->mime_type);
	safe_set_entry (dialog, "category_entry",
		mime_type_info_get_category_name (dialog->p->info));

	dialog->p->use_cat_dfl = dialog->p->info->use_category;
	update_sensitivity (dialog);

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
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
	gtk_option_menu_set_menu (GTK_OPTION_MENU (WID ("component_select")), menu);

	item = gtk_menu_item_new_with_label (_("Custom"));
	menu = gtk_menu_new ();
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
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
	Bonobo_ServerInfo *info, *default_component;
	int                found_idx = -1, i;

	menu = GTK_MENU (gtk_menu_new ());

	component_list = gnome_vfs_mime_get_all_components (dialog->p->info->mime_type);

	/* FIXME: We are leaking the whole list here, but this will be the case until I know of an easy way to duplicate
	 * Bonobo_ServerInfo structures */

	default_component = dialog->p->info->default_component;
	for (tmp = component_list, i = 0; tmp != NULL; tmp = tmp->next, i++) {
		info = tmp->data;

		g_return_if_fail (info != NULL);

		if (default_component != NULL &&
		    !strcmp (info->iid, default_component->iid))
			found_idx = i;

		component_name = mime_type_get_pretty_name_for_server (info);
		menu_item = gtk_menu_item_new_with_label (component_name);
		g_free (component_name);

		/* Store copy of component name in item; free when item destroyed. */
		g_object_set_data (G_OBJECT (menu_item),
				   "component", info);

		gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_item);
		gtk_widget_show (menu_item);
	}

	dialog->p->component_active = !(i == 0);

	menu_item = gtk_menu_item_new_with_label (_("None"));
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_item);
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

	app_list = gnome_vfs_application_registry_get_applications (dialog->p->info->mime_type);

	for (tmp = app_list, i = 0; tmp != NULL; tmp = tmp->next, i++) {
		app = gnome_vfs_application_registry_get_mime_application (tmp->data);

		if (dialog->p->info->default_action != NULL &&
		    dialog->p->info->default_action->id != NULL &&
		    !strcmp (app->id, dialog->p->info->default_action->id))
			found_idx = i;

		menu_item = gtk_menu_item_new_with_label (app->name);

		/* Store copy of application in item; free when item destroyed. */
		g_object_set_data_full (G_OBJECT (menu_item),
					"app", app,
					(GDestroyNotify) gnome_vfs_mime_application_free);

		gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_item);
		gtk_widget_show (menu_item);
	}

	dialog->p->default_action_active = !(i == 0);
	dialog->p->custom_action = (found_idx < 0);

	gtk_menu_shell_append (GTK_MENU_SHELL (menu), gtk_menu_item_new_with_label (_("Custom")));

	if (found_idx < 0) {
		found_idx = i;
		if (dialog->p->info->default_action->command != NULL)
			gnome_file_entry_set_filename (GNOME_FILE_ENTRY (WID ("program_entry")),
						       dialog->p->info->default_action->command);

		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (WID ("needs_terminal_toggle")),
					      dialog->p->info->default_action->requires_terminal);

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

/**
 * mime_edit_dialog_get_app : 
 * @glade :
 * @mime_type : a fall back in case we can't generate a meaningful application name.
 * @current :
 *
 * A utility routine for looking up applications.  it should handle life cycle
 * and hopefully merge in existing copies of custom applications.
 **/
void
mime_edit_dialog_get_app (GladeXML *glade, char const *mime_type,
			  GnomeVFSMimeApplication **current)
{
	GtkWidget *menu = glade_xml_get_widget (glade, "default_action_select");
	gint       idx = gtk_option_menu_get_history (GTK_OPTION_MENU (menu));
	GtkWidget *shell = gtk_option_menu_get_menu (GTK_OPTION_MENU (menu));
	GObject   *item = (g_list_nth (GTK_MENU_SHELL (shell)->children, idx))->data;

	GnomeVFSMimeApplication *res, *app = g_object_get_data (item, "app");

	if (app == NULL) {
		char *cmd = gnome_file_entry_get_full_path (
			GNOME_FILE_ENTRY (glade_xml_get_widget (glade, "program_entry")), FALSE);
		gboolean requires_terminal = gtk_toggle_button_get_active (
			GTK_TOGGLE_BUTTON (glade_xml_get_widget (glade, "needs_terminal_toggle")));
		char *base_cmd;

		GList *ptr, *app_list = NULL;
		
		/* I have no idea what semantics people want, but I'll be anal
		 * and avoid NULL
		 */
		if (cmd == NULL)
			cmd = g_strdup ("");
		base_cmd = g_path_get_basename  (cmd);
		if (base_cmd == NULL);
			base_cmd = g_strdup ("");

		app_list = gnome_vfs_application_registry_get_applications (NULL);
		for (ptr = app_list; ptr != NULL ; ptr = ptr->next) {
			char const *app_cmd = gnome_vfs_application_registry_peek_value (ptr->data,
				GNOME_VFS_APPLICATION_REGISTRY_COMMAND);

			/* Look for a matching application (with or without path) */
			if (app_cmd != NULL &&
			    (!strcmp (cmd, app_cmd) || !strcmp (base_cmd, app_cmd))) {
				gboolean ok, app_req = gnome_vfs_application_registry_get_bool_value (ptr->data,
					GNOME_VFS_APPLICATION_REGISTRY_REQUIRES_TERMINAL, &ok);
				if (ok && app_req == requires_terminal)
					break;
			}
		}

		/* No existing application, lets create one */
		if (ptr == NULL) {
			res = g_new0 (GnomeVFSMimeApplication, 1);
			res->command = cmd;
			res->requires_terminal = requires_terminal;

			res->name = base_cmd;
			if (res->name != NULL && *res->name) {
				/* Can we use the app name as the id ?
				 * We know that there are no apps with the same
				 * command, so if the id is taken we are screwed
				 */
				if (gnome_vfs_application_registry_get_mime_application (res->name) == NULL)
					res->id = g_strdup (res->name);
			} else { /* fail safe to ensure a name */
				g_free (res->name);
				res->name = g_strdup_printf ("Custom %s", mime_type);
			}

			/* If there is no id yet, make up a unique string */
			if (res->id == NULL) {
				uuid_t  app_uuid;
				gchar   app_uuid_str[100];
				uuid_generate (app_uuid);
				uuid_unparse (app_uuid, app_uuid_str);
				res->id = g_strdup (app_uuid_str);
			}

			gnome_vfs_application_registry_save_mime_application (res);
		} else {
			g_free (cmd);
			g_free (base_cmd);
			res = gnome_vfs_application_registry_get_mime_application (ptr->data);
		}

		g_list_free (app_list);
	} else
		res = gnome_vfs_mime_application_copy (app);

	gnome_vfs_mime_application_free (*current);
	*current = res;
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

	option_menu = GTK_OPTION_MENU (WID ("component_select"));
	menu_shell = GTK_MENU_SHELL (gtk_option_menu_get_menu (option_menu));
	idx = gtk_option_menu_get_history (option_menu);
	menu_item = (g_list_nth (menu_shell->children, idx))->data;

	CORBA_free (dialog->p->info->default_component);
	dialog->p->info->default_component = g_object_get_data (menu_item, "component");

	mime_edit_dialog_get_app (dialog->p->dialog_xml,
		dialog->p->info->mime_type,
		&(dialog->p->info->default_action));

	ext_list = collect_filename_extensions (dialog);
	mime_type_info_set_file_extensions (dialog->p->info, ext_list);

	tmp = mime_type_info_get_category_name (dialog->p->info);
	tmp1 = gtk_entry_get_text (GTK_ENTRY (WID ("category_entry")));
	if (strcmp (tmp, tmp1)) {
		cat_changed = TRUE;
		mime_type_info_set_category_name (dialog->p->info, tmp1, tmp1, dialog->p->model);
	}
	g_free (tmp);

	model_entry_save (MODEL_ENTRY (dialog->p->info));

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
	const gchar *mime_type;
	GtkWidget *err_dialog = NULL;

	mime_type = gtk_entry_get_text (GTK_ENTRY (WID ("mime_type_entry")));

	if (mime_type != NULL && *mime_type != '\0') {
		if (strchr (mime_type, ' ') || !strchr (mime_type, '/')) {
			err_dialog = gtk_message_dialog_new (
				GTK_WINDOW (dialog->p->dialog_win),
				GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR,
				GTK_BUTTONS_CANCEL,
				_("Please enter a valid MIME type.  It should be of the form "
				  "class/type and may not contain any spaces."));
		} else if (dialog->p->is_add && gnome_vfs_mime_type_is_known (mime_type)) {
			err_dialog = gtk_message_dialog_new (
				GTK_WINDOW (dialog->p->dialog_win),
				GTK_DIALOG_MODAL, GTK_MESSAGE_QUESTION,
				GTK_BUTTONS_OK_CANCEL,
				_("A MIME type with that name already exists, overwrite ?."));
		}
	}

	if (err_dialog) {
		int res = gtk_dialog_run (GTK_DIALOG (err_dialog));
		gtk_object_destroy (GTK_OBJECT (err_dialog));
		return res != GTK_RESPONSE_CANCEL;
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
		 GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
		 GTK_STOCK_OK,     GTK_RESPONSE_OK,
		 NULL);

	gtk_widget_set_size_request (dialog_win, 300, 300);

	scrolled_win = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_win), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	gtk_container_add (GTK_CONTAINER (scrolled_win), treeview);

	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog_win)->vbox), scrolled_win, TRUE, TRUE, GNOME_PAD_SMALL);
	gtk_widget_show_all (dialog_win);

	if (gtk_dialog_run (GTK_DIALOG (dialog_win)) == GTK_RESPONSE_OK) {
		gtk_tree_selection_get_selected (selection, &model, &iter);
		gtk_entry_set_text (GTK_ENTRY (WID ("category_entry")),
				    mime_category_info_get_full_name (MIME_CATEGORY_INFO (MODEL_ENTRY_FROM_ITER (&iter))));
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
response_cb (MimeEditDialog *dialog, gint response_id) 
{
	if (response_id == GTK_RESPONSE_OK) {
		if (validate_data (dialog)) {
			store_data (dialog);
			g_signal_emit (G_OBJECT (dialog), dialog_signals[DONE], 0, TRUE);
			g_object_unref (G_OBJECT (dialog));
		}
	} else {
		if (dialog->p->is_add)
			mime_type_info_free (dialog->p->info);

		g_signal_emit (G_OBJECT (dialog), dialog_signals[DONE], 0, FALSE);

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
