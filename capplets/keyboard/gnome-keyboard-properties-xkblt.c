/* -*- mode: c; style: linux -*- */

/* gnome-keyboard-properties-xkblt.c
 * Copyright (C) 2003 Sergey V. Oudaltsov
 *
 * Written by: Sergey V. Oudaltsov <svu@users.sourceforge.net>
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
#  include <config.h>
#endif

#include <gnome.h>
#include <gconf/gconf-client.h>
#include <glade/glade.h>

#include "libgswitchit/gswitchit_xkb_config.h"

#include "capplet-util.h"
#include "gconf-property-editor.h"
#include "activate-settings-daemon.h"
#include "capplet-stock-icons.h"
#include <../accessibility/keyboard/accessibility-keyboard.h>

#include "gnome-keyboard-properties-xkb.h"

static GtkTreeIter current1stLevelIter;
static const char *current1stLevelId;
static int idx2Select = -1;
static int maxSelectedLayouts = -1;

static void
clear_list (GSList * list)
{
	while (list != NULL) {
		GSList *p = list;
		list = list->next;
		g_free (p->data);
		g_slist_free_1 (p);
	}
}

static GSList *
get_selected_layouts_list ()
{
	return gconf_client_get_list (gconf_client_get_default (),
				      GSWITCHIT_CONFIG_XKB_KEY_LAYOUTS,
				      GCONF_VALUE_STRING, NULL);

}

static void
set_selected_layouts_list (GSList * list)
{
	gconf_client_set_list (gconf_client_get_default (),
			       GSWITCHIT_CONFIG_XKB_KEY_LAYOUTS,
			       GCONF_VALUE_STRING, list, NULL);
}

static void
add_variant_to_available_layouts_tree (const XklConfigItemPtr
				       configItem, GladeXML * dialog)
{
	GtkWidget *layoutsTree = WID ("xkb_layouts_available");
	GtkTreeIter iter;
	GtkTreeStore *treeStore =
	    GTK_TREE_STORE (gtk_tree_view_get_model
			    (GTK_TREE_VIEW (layoutsTree)));
	const gchar *fullLayoutName =
	    GSwitchItConfigMergeItems (current1stLevelId,
				       configItem->name);
	char *utfVariantName = xci_desc_to_utf8 (configItem);

	gtk_tree_store_append (treeStore, &iter, &current1stLevelIter);
	gtk_tree_store_set (treeStore, &iter, 0, utfVariantName, 1,
			    fullLayoutName, -1);
	g_free (utfVariantName);
}

static void
add_layout_to_available_layouts_tree (const XklConfigItemPtr
				      configItem, GladeXML * dialog)
{
	GtkWidget *layoutsTree = WID ("xkb_layouts_available");
	GtkTreeStore *treeStore =
	    GTK_TREE_STORE (gtk_tree_view_get_model
			    (GTK_TREE_VIEW (layoutsTree)));
	char *utfLayoutName = xci_desc_to_utf8 (configItem);

	gtk_tree_store_append (treeStore, &current1stLevelIter, NULL);
	gtk_tree_store_set (treeStore, &current1stLevelIter, 0,
			    utfLayoutName, 1, configItem->name, -1);
	g_free (utfLayoutName);

	current1stLevelId = configItem->name;

	XklConfigEnumLayoutVariants (configItem->name,
				     (ConfigItemProcessFunc)
				     add_variant_to_available_layouts_tree,
				     dialog);
}

static void
enable_disable_layout_buttons (GladeXML * dialog)
{
	GtkWidget *addLayoutBtn = WID ("xkb_layouts_add");
	GtkWidget *delLayoutBtn = WID ("xkb_layouts_remove");
	GtkWidget *upLayoutBtn = WID ("xkb_layouts_up");
	GtkWidget *dnLayoutBtn = WID ("xkb_layouts_down");
	GtkWidget *availableLayoutsTree = WID ("xkb_layouts_available");
	GtkWidget *selectedLayoutsTree = WID ("xkb_layouts_selected");

	GtkTreeSelection *aSelection =
	    gtk_tree_view_get_selection (GTK_TREE_VIEW
					 (availableLayoutsTree));
	const int nSelectedAvailableLayouts =
	    gtk_tree_selection_count_selected_rows (aSelection);
	GtkTreeSelection *sSelection =
	    gtk_tree_view_get_selection (GTK_TREE_VIEW
					 (selectedLayoutsTree));
	const int nSelectedSelectedLayouts =
	    gtk_tree_selection_count_selected_rows (sSelection);
	gboolean canMoveUp = FALSE;
	gboolean canMoveDn = FALSE;
	GtkTreeIter iter;
	GtkTreeModel *selectedLayoutsModel = gtk_tree_view_get_model
	    (GTK_TREE_VIEW (selectedLayoutsTree));
	const int nSelectedLayouts =
	    gtk_tree_model_iter_n_children (selectedLayoutsModel,
					    NULL);

	gtk_widget_set_sensitive (addLayoutBtn,
				  (nSelectedAvailableLayouts > 0)
				  && (nSelectedLayouts <
				      maxSelectedLayouts));
	gtk_widget_set_sensitive (delLayoutBtn,
				  nSelectedSelectedLayouts > 0);

	if (gtk_tree_selection_get_selected
	    (sSelection, &selectedLayoutsModel, &iter)) {
		GtkTreePath *path =
		    gtk_tree_model_get_path (selectedLayoutsModel,
					     &iter);
		if (path != NULL) {
			int *indices = gtk_tree_path_get_indices (path);
			int idx = indices[0];
			canMoveUp = idx > 0;
			canMoveDn = idx < (nSelectedLayouts - 1);
			gtk_tree_path_free (path);
		}
	}
	gtk_widget_set_sensitive (upLayoutBtn, canMoveUp);
	gtk_widget_set_sensitive (dnLayoutBtn, canMoveDn);
}

void
prepare_selected_layouts_tree (GladeXML * dialog)
{
	GtkListStore *listStore =
	    gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_STRING);
	GtkWidget *treeView = WID ("xkb_layouts_selected");
	GtkCellRenderer *renderer =
	    GTK_CELL_RENDERER (gtk_cell_renderer_text_new ());
	GtkTreeViewColumn *column =
	    gtk_tree_view_column_new_with_attributes (NULL,
						      renderer,
						      "text",
						      0,
						      NULL);
	GtkTreeSelection *selection =
	    gtk_tree_view_get_selection (GTK_TREE_VIEW (treeView));
	gtk_tree_view_set_model (GTK_TREE_VIEW (treeView),
				 GTK_TREE_MODEL (listStore));
	gtk_tree_view_append_column (GTK_TREE_VIEW (treeView), column);
	g_signal_connect_swapped (G_OBJECT (selection), "changed",
				  G_CALLBACK
				  (enable_disable_layout_buttons), dialog);
	maxSelectedLayouts =
	    XklMultipleLayoutsSupported ()? XkbNumKbdGroups : 1;
}

void
fill_selected_layouts_tree (GladeXML * dialog)
{
	GSList *layouts = get_selected_layouts_list ();
	GSList *curLayout;
	GtkListStore *listStore =
	    GTK_LIST_STORE (gtk_tree_view_get_model
			    (GTK_TREE_VIEW
			     (WID ("xkb_layouts_selected"))));
	gtk_list_store_clear (listStore);

	for (curLayout = layouts; curLayout != NULL;
	     curLayout = curLayout->next) {
		GtkTreeIter iter;
		char *l, *sl, *v, *sv;
		const char *visible = (char *) curLayout->data;
		gtk_list_store_append (listStore, &iter);
		if (GSwitchItConfigGetDescriptions
		    (curLayout->data, &sl, &l, &sv, &v))
			visible = GSwitchItConfigFormatFullLayout (l, v);
		gtk_list_store_set (listStore, &iter,
				    0, visible, 1, curLayout->data, -1);
	}

	clear_list (layouts);
	enable_disable_layout_buttons (dialog);
	if (idx2Select != -1) {
		GtkTreeSelection *selection =
		    gtk_tree_view_get_selection ((GTK_TREE_VIEW
						  (WID
						   ("xkb_layouts_selected"))));
		GtkTreePath *path =
		    gtk_tree_path_new_from_indices (idx2Select, -1);
		gtk_tree_selection_select_path (selection, path);
		gtk_tree_path_free (path);
		idx2Select = -1;
	}
}

void
fill_available_layouts_tree (GladeXML * dialog)
{
	GtkTreeStore *treeStore =
	    gtk_tree_store_new (2, G_TYPE_STRING, G_TYPE_STRING);
	GtkWidget *treeView = WID ("xkb_layouts_available");
	GtkCellRenderer *renderer =
	    GTK_CELL_RENDERER (gtk_cell_renderer_text_new ());
	GtkTreeViewColumn *column =
	    gtk_tree_view_column_new_with_attributes (NULL,
						      renderer,
						      "text",
						      0,
						      NULL);
	GtkTreeSelection *selection =
	    gtk_tree_view_get_selection (GTK_TREE_VIEW (treeView));

	gtk_tree_view_set_model (GTK_TREE_VIEW (treeView),
				 GTK_TREE_MODEL (treeStore));
	gtk_tree_view_append_column (GTK_TREE_VIEW (treeView), column);

	XklConfigEnumLayouts ((ConfigItemProcessFunc)
			      add_layout_to_available_layouts_tree,
			      dialog);

	g_signal_connect_swapped (G_OBJECT (selection), "changed",
				  G_CALLBACK
				  (enable_disable_layout_buttons), dialog);
}

static void
add_selected_layout (GtkWidget * button, GladeXML * dialog)
{
	GtkTreeSelection *selection =
	    gtk_tree_view_get_selection (GTK_TREE_VIEW
					 (WID ("xkb_layouts_available")));
	GtkTreeIter selectedIter;
	GtkTreeModel *model;
	if (gtk_tree_selection_get_selected
	    (selection, &model, &selectedIter)) {
		gchar *id;
		GSList *layoutsList = get_selected_layouts_list ();
		gtk_tree_model_get (model, &selectedIter, 1, &id, -1);
		layoutsList = g_slist_append (layoutsList, id);
		set_selected_layouts_list (layoutsList);
		clear_list (layoutsList);
	}
}

static void
move_selected_layout (GladeXML * dialog, int offset)
{
	GtkTreeSelection *selection =
	    gtk_tree_view_get_selection (GTK_TREE_VIEW
					 (WID ("xkb_layouts_selected")));
	GtkTreeIter selectedIter;
	GtkTreeModel *model;
	if (gtk_tree_selection_get_selected
	    (selection, &model, &selectedIter)) {
		GSList *layoutsList = get_selected_layouts_list ();
		GtkTreePath *path = gtk_tree_model_get_path (model,
							     &selectedIter);
		if (path != NULL) {
			int *indices = gtk_tree_path_get_indices (path);
			char *id = NULL;
			GSList *node2Remove =
			    g_slist_nth (layoutsList, indices[0]);

			layoutsList =
			    g_slist_remove_link (layoutsList, node2Remove);

			id = (char *) node2Remove->data;
			g_slist_free_1 (node2Remove);

			if (offset == 0)
				g_free (id);
			else {
				layoutsList =
				    g_slist_insert (layoutsList, id,
						    indices[0] + offset);
				idx2Select = indices[0] + offset;
			}

			set_selected_layouts_list (layoutsList);
			gtk_tree_path_free (path);
		}
		clear_list (layoutsList);
	}
}

static void
remove_selected_layout (GtkWidget * button, GladeXML * dialog)
{
	move_selected_layout (dialog, 0);
}

static void
up_selected_layout (GtkWidget * button, GladeXML * dialog)
{
	move_selected_layout (dialog, -1);
}

static void
down_selected_layout (GtkWidget * button, GladeXML * dialog)
{
	move_selected_layout (dialog, +1);
}

void
register_layouts_buttons_handlers (GladeXML * dialog)
{
	g_signal_connect (G_OBJECT (WID ("xkb_layouts_add")), "clicked",
			  G_CALLBACK (add_selected_layout), dialog);
	g_signal_connect (G_OBJECT (WID ("xkb_layouts_remove")), "clicked",
			  G_CALLBACK (remove_selected_layout), dialog);
	g_signal_connect (G_OBJECT (WID ("xkb_layouts_up")), "clicked",
			  G_CALLBACK (up_selected_layout), dialog);
	g_signal_connect (G_OBJECT (WID ("xkb_layouts_down")), "clicked",
			  G_CALLBACK (down_selected_layout), dialog);
}

static void
update_layouts_list (GConfClient * client,
		     guint cnxn_id, GConfEntry * entry, GladeXML * dialog)
{
	fill_selected_layouts_tree (dialog);
}

void
register_layouts_gconf_listener (GladeXML * dialog)
{
	gconf_client_notify_add (gconf_client_get_default (),
				 GSWITCHIT_CONFIG_XKB_KEY_LAYOUTS,
				 (GConfClientNotifyFunc)
				 update_layouts_list, dialog, NULL, NULL);
}
