/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* file-types-capplet.h
 *
 * Copyright (C) 1998 Redhat Software Inc. 
 * Copyright (C) 2000  Free Software Foundaton
 * Copyright (C) 2000  Eazel, Inc.
 * Copyright (C) 2002 Ximian, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Authors: 	Jonathan Blandford <jrb@redhat.com>,
 * 		Gene Z. Ragan <gzr@eazel.com>,
 *              Bradford Hovinen <hovinen@ximian.com>
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <gnome.h>
#include <glade/glade.h>
#include <gconf/gconf-client.h>

#include "capplet-util.h"
#include "mime-types-model.h"
#include "mime-edit-dialog.h"
#include "mime-category-edit-dialog.h"
#include "mime-type-info.h"
#include "service-edit-dialog.h"
#include "service-info.h"

static void
add_mime_cb (GtkButton *button, GladeXML *dialog) 
{
	GtkTreeView     *treeview;
	GtkTreeModel    *model;
	GObject         *add_dialog;

	treeview = GTK_TREE_VIEW (WID ("mime_types_tree"));
	model = gtk_tree_view_get_model (treeview);

	add_dialog = mime_add_dialog_new (model,
		GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (button))),
		NULL);
}

static void
add_service_cb (GtkButton *button, GladeXML *dialog) 
{
	GtkTreeView     *treeview;
	GtkTreeModel    *model;
	GObject         *add_dialog;

	treeview = GTK_TREE_VIEW (WID ("mime_types_tree"));
	model = gtk_tree_view_get_model (treeview);

	add_dialog = service_add_dialog_new (model);
}

static GObject *
launch_edit_dialog (GtkTreeModel *model, GtkTreeIter *iter) 
{
	ModelEntry *entry;

	entry = MODEL_ENTRY_FROM_ITER (iter);

	switch (entry->type) {
	case MODEL_ENTRY_MIME_TYPE:
		return mime_edit_dialog_new (model, MIME_TYPE_INFO (entry));

	case MODEL_ENTRY_SERVICE:
		return service_edit_dialog_new (model, SERVICE_INFO (entry));

	case MODEL_ENTRY_CATEGORY:
		return mime_category_edit_dialog_new (model, MIME_CATEGORY_INFO (entry));

	default:
		return NULL;
	}
}

static void
edit_cb (GtkButton *button, GladeXML *dialog) 
{
	GtkTreeView       *treeview;
	GtkTreeModel      *model;
	GtkTreeSelection  *selection;
	GtkTreeIter        iter;

	treeview = GTK_TREE_VIEW (WID ("mime_types_tree"));
	selection = gtk_tree_view_get_selection (treeview);
	gtk_tree_selection_get_selected (selection, &model, &iter);

	launch_edit_dialog (model, &iter);
}

static void
row_activated_cb (GtkTreeView *view, GtkTreePath *path, GtkTreeViewColumn *column, GladeXML *dialog) 
{
	GtkTreeModel *model;
	GtkTreeIter   iter;

	model = gtk_tree_view_get_model (view);
	gtk_tree_model_get_iter (model, &iter, path);
	launch_edit_dialog (model, &iter);
}

static void
edit_count_cb (GtkTreeModel *model, GtkTreePath *path, GtkTreeIter *iter, gint *count) 
{
	if (MODEL_ENTRY_FROM_ITER (iter)->type != MODEL_ENTRY_SERVICES_CATEGORY)
		(*count)++;
}

static void
remove_count_cb (GtkTreeModel *model, GtkTreePath *path, GtkTreeIter *iter, gint *count) 
{
	if (MODEL_ENTRY_FROM_ITER (iter)->type != MODEL_ENTRY_SERVICES_CATEGORY &&
	    MODEL_ENTRY_FROM_ITER (iter)->type != MODEL_ENTRY_CATEGORY)
		(*count)++;
}

static void
selection_changed_cb (GtkTreeSelection *selection, GladeXML *dialog) 
{
	gint count = 0;

	gtk_tree_selection_selected_foreach (selection, (GtkTreeSelectionForeachFunc) edit_count_cb, &count);

	if (count == 0)
		gtk_widget_set_sensitive (WID ("edit_button"), FALSE);
	else
		gtk_widget_set_sensitive (WID ("edit_button"), TRUE);

	count = 0;

	gtk_tree_selection_selected_foreach (selection, (GtkTreeSelectionForeachFunc) remove_count_cb, &count);

	if (count == 0)
		gtk_widget_set_sensitive (WID ("remove_button"), FALSE);
	else
		gtk_widget_set_sensitive (WID ("remove_button"), TRUE);
}

static void
remove_cb (GtkButton *button, GladeXML *dialog) 
{
	GtkTreeView       *treeview;
	GtkTreeModel      *model;
	GtkTreeSelection  *selection;
	GtkTreeIter        iter;
	ModelEntry        *entry;

	treeview = GTK_TREE_VIEW (WID ("mime_types_tree"));
	selection = gtk_tree_view_get_selection (treeview);
	gtk_tree_selection_get_selected (selection, &model, &iter);

	entry = MODEL_ENTRY_FROM_ITER (&iter);

	model_entry_remove_child (entry->parent, entry, model);
	model_entry_delete (entry);

	selection_changed_cb (selection, dialog);
}

static void
cb_file_type_dialog_response (GtkDialog *dialog, gint response_id)
{
	if (response_id == GTK_RESPONSE_HELP)
		capplet_help (GTK_WINDOW (dialog),
			"wgoscustlookandfeel.xml",
			"goscustdoc-7");
	else
		gtk_main_quit ();
}

static GladeXML *
create_dialog (void) 
{
	GladeXML          *dialog;

	GtkTreeModel      *model;

	GtkWidget         *treeview;
	GtkCellRenderer   *renderer;
	GtkTreeViewColumn *column;
	GtkTreeSelection  *selection;

	dialog = glade_xml_new (GNOMECC_DATA_DIR "/interfaces/file-types-properties.glade", "main_dialog", NULL);

	model = GTK_TREE_MODEL (mime_types_model_new (FALSE));
	treeview = WID ("mime_types_tree");

	gtk_tree_view_set_model (GTK_TREE_VIEW (treeview), model);

	/* Icon/description column */
	column = gtk_tree_view_column_new ();

	renderer = gtk_cell_renderer_pixbuf_new ();
	gtk_tree_view_column_pack_start (column, renderer, FALSE);
	gtk_tree_view_column_set_attributes
		(column, renderer, "pixbuf", MODEL_COLUMN_ICON, NULL);

	/* Description column */
	renderer = gtk_cell_renderer_text_new ();
	gtk_tree_view_column_pack_start (column, renderer, FALSE);
	gtk_tree_view_column_set_attributes
		(column, renderer, "text", MODEL_COLUMN_DESCRIPTION, NULL);
	g_object_set (G_OBJECT (treeview), 
		"search_column", MODEL_COLUMN_DESCRIPTION,
		NULL);

	gtk_tree_view_column_set_title (column, _("Description"));
	gtk_tree_view_append_column (GTK_TREE_VIEW (treeview), column);
	gtk_tree_view_set_expander_column (GTK_TREE_VIEW (treeview), column);

	/* Extensions column */
	renderer = gtk_cell_renderer_text_new ();
	gtk_tree_view_insert_column_with_attributes
		(GTK_TREE_VIEW (treeview), -1, _("Extensions"), renderer,
		 "text", MODEL_COLUMN_FILE_EXT,
		 NULL);

	gtk_widget_set_sensitive (WID ("edit_button"), FALSE);
	gtk_widget_set_sensitive (WID ("remove_button"), FALSE);

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview));
	gtk_tree_selection_set_mode (selection, GTK_SELECTION_SINGLE);

	g_signal_connect (G_OBJECT (WID ("add_mime_button")), "clicked", (GCallback) add_mime_cb, dialog);
	g_signal_connect (G_OBJECT (WID ("add_service_button")), "clicked", (GCallback) add_service_cb, dialog);
	g_signal_connect (G_OBJECT (WID ("edit_button")), "clicked", (GCallback) edit_cb, dialog);
	g_signal_connect (G_OBJECT (WID ("remove_button")), "clicked", (GCallback) remove_cb, dialog);
	g_signal_connect (G_OBJECT (selection), "changed", (GCallback) selection_changed_cb, dialog);

	g_signal_connect (G_OBJECT (WID ("mime_types_tree")), "row-activated", (GCallback) row_activated_cb, dialog);

	g_signal_connect (G_OBJECT (WID ("main_dialog")),
		"response",
		G_CALLBACK (cb_file_type_dialog_response), NULL);

	return dialog;
}

static void
dialog_done_cb (MimeEditDialog *dialog, gboolean done, MimeTypeInfo *info) 
{
	gtk_main_quit ();
}


int
main (int argc, char **argv) 
{
	char const   *mime_type = NULL;
	char const   *file_name = NULL;
	GtkTreeModel *model;
	MimeTypeInfo *info = NULL;
	GObject      *mime_dialog;
	GnomeProgram *program;
	poptContext   popt_ctxt = 0;

	bindtextdomain (GETTEXT_PACKAGE, GNOMELOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	program = gnome_program_init ("gnome-file-types-properties",
		VERSION, LIBGNOMEUI_MODULE,
		argc, argv,
		NULL);
	g_object_get (G_OBJECT (program),
		GNOME_PARAM_POPT_CONTEXT, &popt_ctxt,
		NULL);

	if (popt_ctxt) {
		const gchar **startup_files = poptGetArgs (popt_ctxt);
		if (startup_files != NULL) {
			mime_type = startup_files [0];
			if (mime_type != NULL)
				file_name = startup_files [1];
		}
	}

	if (mime_type == NULL) {
		GladeXML *dialog = create_dialog ();
		gtk_widget_show_all (WID ("main_dialog"));
	} else {
		model = GTK_TREE_MODEL (mime_types_model_new (FALSE));
		if (strcmp (GNOME_VFS_MIME_TYPE_UNKNOWN, mime_type)) {
			info = mime_type_info_new (mime_type, model);
			mime_dialog = mime_edit_dialog_new (model, info);
		} else {
			mime_dialog = mime_add_dialog_new (model, NULL, file_name);
		}
		g_signal_connect (mime_dialog, "done", (GCallback) dialog_done_cb, info);
	}

	gtk_main ();

	return 0;
}
