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
#include <gconf/gconf-changeset.h>

#include "mime-types-model.h"
#include "mime-edit-dialog.h"
#include "mime-type-info.h"
#include "service-edit-dialog.h"
#include "service-info.h"

#define WID(x) (glade_xml_get_widget (dialog, x))

static GList          *remove_list = NULL;
static GConfChangeSet *changeset = NULL;

static void
add_cb (GtkButton *button, GladeXML *dialog) 
{
	GObject         *add_dialog;

	add_dialog = mime_add_dialog_new ();
}

static void
edit_cb (GtkButton *button, GladeXML *dialog) 
{
	GtkTreeView       *treeview;
	GtkTreeModel      *model;
	GtkTreeSelection  *selection;
	GtkTreeIter        iter;

	GObject           *edit_dialog;

	treeview = GTK_TREE_VIEW (WID ("mime_types_tree"));
	selection = gtk_tree_view_get_selection (treeview);
	gtk_tree_selection_get_selected (selection, &model, &iter);

	if (model_entry_is_protocol (model, &iter))
		edit_dialog = service_edit_dialog_new (service_info_load (model, &iter, changeset));
	else
		edit_dialog = mime_edit_dialog_new (mime_type_info_load (model, &iter));
}

static void
row_activated_cb (GtkTreeView *view, GtkTreePath *path, GtkTreeViewColumn *column, GladeXML *dialog) 
{
	GtkTreeModel *model;
	GtkTreeIter iter;

	GObject           *edit_dialog;

	model = gtk_tree_view_get_model (view);
	gtk_tree_model_get_iter (model, &iter, path);

	if (model_entry_is_protocol (model, &iter))
		edit_dialog = service_edit_dialog_new (service_info_load (model, &iter, changeset));
	else
		edit_dialog = mime_edit_dialog_new (mime_type_info_load (model, &iter));
}

static void
count_cb (GtkTreeModel *model, GtkTreePath *path, GtkTreeIter *iter, gint *count) 
{
	(*count)++;
}

static void
selection_changed_cb (GtkTreeSelection *selection, GladeXML *dialog) 
{
	gint count;

	gtk_tree_selection_selected_foreach (selection, (GtkTreeSelectionForeachFunc) count_cb, &count);

	if (count == 0)
		gtk_widget_set_sensitive (WID ("edit_button"), FALSE);
	else
		gtk_widget_set_sensitive (WID ("edit_button"), TRUE);
}

static void
remove_cb (GtkButton *button, GladeXML *dialog) 
{
	GtkTreeView       *treeview;
	GtkTreeModel      *model;
	GtkTreeSelection  *selection;
	GtkTreeIter        iter;
	GValue             mime_type;

	treeview = GTK_TREE_VIEW (WID ("mime_types_tree"));
	selection = gtk_tree_view_get_selection (treeview);
	gtk_tree_selection_get_selected (selection, &model, &iter);

	mime_type.g_type = G_TYPE_INVALID;
	gtk_tree_model_get_value (model, &iter, MIME_TYPE_COLUMN, &mime_type);
	remove_list = g_list_prepend (remove_list, g_value_dup_string (&mime_type));
	mime_type_remove_from_dirty_list (g_value_get_string (&mime_type));
	gtk_tree_store_remove (GTK_TREE_STORE (model), &iter);
	g_value_unset (&mime_type);

	selection_changed_cb (selection, dialog);
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

	gint               col_offset;

	dialog = glade_xml_new (GNOMECC_DATA_DIR "/interfaces/file-types-properties.glade", "main_dialog", NULL);

	model = mime_types_model_new ();
	treeview = WID ("mime_types_tree");

	gtk_tree_view_set_model (GTK_TREE_VIEW (treeview), model);

	/* Icon column */
	renderer = gtk_cell_renderer_pixbuf_new ();
	gtk_tree_view_insert_column_with_attributes
		(GTK_TREE_VIEW (treeview), -1, NULL, renderer,
		 "pixbuf", ICON_COLUMN,
		 NULL);

	/* Description column */
	renderer = gtk_cell_renderer_text_new ();
	col_offset = gtk_tree_view_insert_column_with_attributes
		(GTK_TREE_VIEW (treeview), -1, _("Description"), renderer,
		 "text", DESCRIPTION_COLUMN,
		 NULL);

	/* Extensions column */
	renderer = gtk_cell_renderer_text_new ();
	gtk_tree_view_insert_column_with_attributes
		(GTK_TREE_VIEW (treeview), -1, _("Extensions"), renderer,
		 "text", EXTENSIONS_COLUMN,
		 NULL);

	column = gtk_tree_view_get_column (GTK_TREE_VIEW (treeview), col_offset - 1);
	gtk_tree_view_set_expander_column (GTK_TREE_VIEW (treeview), column);

	gtk_widget_set_sensitive (WID ("edit_button"), FALSE);

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview));
	gtk_tree_selection_set_mode (selection, GTK_SELECTION_SINGLE);

	g_signal_connect (G_OBJECT (WID ("add_button")), "clicked", (GCallback) add_cb, dialog);
	g_signal_connect (G_OBJECT (WID ("edit_button")), "clicked", (GCallback) edit_cb, dialog);
	g_signal_connect (G_OBJECT (WID ("remove_button")), "clicked", (GCallback) remove_cb, dialog);
	g_signal_connect (G_OBJECT (selection), "changed", (GCallback) selection_changed_cb, dialog);

	g_signal_connect (G_OBJECT (WID ("mime_types_tree")), "row-activated", (GCallback) row_activated_cb, dialog);

	return dialog;
}

static void
apply_cb (void) 
{
	mime_type_commit_dirty_list ();

	g_list_foreach (remove_list, (GFunc) gnome_vfs_mime_registered_mime_type_delete, NULL);
	g_list_foreach (remove_list, (GFunc) g_free, NULL);
	g_list_free (remove_list);

	gconf_client_commit_change_set (gconf_client_get_default (), changeset, TRUE, NULL);
}

int
main (int argc, char **argv) 
{
	GladeXML       *dialog;

	bindtextdomain (PACKAGE, GNOMELOCALEDIR);
	bind_textdomain_codeset (PACKAGE, "UTF-8");
	textdomain (PACKAGE);

	gnome_program_init ("gnome-file-types-properties", VERSION, LIBGNOMEUI_MODULE, argc, argv, NULL);

	changeset = gconf_change_set_new ();
	dialog = create_dialog ();

	g_signal_connect (G_OBJECT (WID ("main_apply_button")), "clicked", (GCallback) apply_cb, NULL);
	g_signal_connect (G_OBJECT (WID ("main_close_button")), "clicked", (GCallback) gtk_main_quit, NULL);
	gtk_widget_show_all (WID ("main_dialog"));

	gtk_main ();

	return 0;
}
