/* -*- mode: c; style: linux -*- */

/* gnome-keyboard-properties-xkbltadd.c
 * Copyright (C) 2007 Sergey V. Udaltsov
 *
 * Written by: Sergey V. Udaltsov <svu@gnome.org>
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
#include <glade/glade.h>

#include "capplet-util.h"

#include <libgnomekbd/gkbd-util.h>

#include "gnome-keyboard-properties-xkb.h"

#define GROUP_SWITCHERS_GROUP "grp"
#define DEFAULT_GROUP_SWITCH "grp:alts_toggle"

static GtkTreeIter current1st_level_iter;
static const char *current1st_level_id;

static void
add_variant_to_available_layouts_tree (XklConfigRegistry * config_registry,
				       XklConfigItem * config_item,
				       GladeXML * chooser_dialog)
{
	GtkWidget *layouts_tree = CWID ("xkb_layouts_available");
	GtkTreeIter iter;
	GtkTreeStore *tree_store =
	    GTK_TREE_STORE (gtk_tree_view_get_model
			    (GTK_TREE_VIEW (layouts_tree)));
	const gchar *full_layout_name =
	    gkbd_keyboard_config_merge_items (current1st_level_id,
					      config_item->name);
	char *utf_variant_name = xci_desc_to_utf8 (config_item);

	gtk_tree_store_append (tree_store, &iter, &current1st_level_iter);
	gtk_tree_store_set (tree_store, &iter,
			    AVAIL_LAYOUT_TREE_COL_DESCRIPTION,
			    utf_variant_name, AVAIL_LAYOUT_TREE_COL_ID,
			    full_layout_name, -1);
	g_free (utf_variant_name);
}

static void
add_layout_to_available_layouts_tree (XklConfigRegistry * config_registry,
				      XklConfigItem * config_item,
				      GladeXML * chooser_dialog)
{
	GtkWidget *layouts_tree = CWID ("xkb_layouts_available");
	GtkTreeStore *tree_store =
	    GTK_TREE_STORE (gtk_tree_view_get_model
			    (GTK_TREE_VIEW (layouts_tree)));
	char *utf_layout_name = xci_desc_to_utf8 (config_item);

	gtk_tree_store_append (tree_store, &current1st_level_iter, NULL);
	gtk_tree_store_set (tree_store, &current1st_level_iter,
			    AVAIL_LAYOUT_TREE_COL_DESCRIPTION,
			    utf_layout_name, AVAIL_LAYOUT_TREE_COL_ID,
			    config_item->name, -1);
	g_free (utf_layout_name);

	current1st_level_id = config_item->name;

	xkl_config_registry_foreach_layout_variant (config_registry,
						    config_item->name,
						    (ConfigItemProcessFunc)
						    add_variant_to_available_layouts_tree,
						    chooser_dialog);
}

static void
xkb_layout_chooser_enable_disable_buttons (GladeXML * chooser_dialog)
{
        GtkWidget *available_layouts_tree = CWID ("xkb_layouts_available");
        GtkTreeSelection *selection =
            gtk_tree_view_get_selection (GTK_TREE_VIEW
                                         (available_layouts_tree));
        const int n_selected_available_layouts =
            gtk_tree_selection_count_selected_rows (selection);

        gtk_dialog_set_response_sensitive (GTK_DIALOG
                                           (CWID ("xkb_layout_chooser")),
                                           GTK_RESPONSE_OK,
                                           n_selected_available_layouts >
                                           0);
}

static void
xkb_layout_chooser_selection_changed (GladeXML * chooser_dialog)
{
	xkb_layout_preview_update (chooser_dialog);
	xkb_layout_chooser_enable_disable_buttons (chooser_dialog);
}

void
sort_tree_content (GtkWidget * tree_view)
{
	GtkTreeModel *tree_model =
	    gtk_tree_view_get_model (GTK_TREE_VIEW (tree_view));
	GtkTreeModel *sorted_tree_model;
	/* replace the store with the sorted version */
	sorted_tree_model =
	    gtk_tree_model_sort_new_with_model (tree_model);
	gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE
					      (sorted_tree_model), 0,
					      GTK_SORT_ASCENDING);
	gtk_tree_view_set_model (GTK_TREE_VIEW (tree_view),
				 sorted_tree_model);
}

void
xkb_layouts_fill_available_tree (GladeXML * chooser_dialog)
{
	GtkTreeStore *tree_store =
	    gtk_tree_store_new (2, G_TYPE_STRING, G_TYPE_STRING);
	GtkWidget *tree_view = CWID ("xkb_layouts_available");
	GtkCellRenderer *renderer =
	    GTK_CELL_RENDERER (gtk_cell_renderer_text_new ());
	GtkTreeViewColumn *column =
	    gtk_tree_view_column_new_with_attributes (NULL,
						      renderer,
						      "text",
						      AVAIL_LAYOUT_TREE_COL_DESCRIPTION,
						      NULL);
	GtkTreeSelection *selection =
	    gtk_tree_view_get_selection (GTK_TREE_VIEW (tree_view));

	gtk_tree_view_set_model (GTK_TREE_VIEW (tree_view),
				 GTK_TREE_MODEL (tree_store));
	gtk_tree_view_append_column (GTK_TREE_VIEW (tree_view), column);

	xkl_config_registry_foreach_layout (config_registry,
					    (ConfigItemProcessFunc)
					    add_layout_to_available_layouts_tree,
					    chooser_dialog);

	sort_tree_content (tree_view);
	g_signal_connect_swapped (G_OBJECT (selection), "changed",
				  G_CALLBACK
				  (xkb_layout_chooser_selection_changed),
				  chooser_dialog);
}

static void
xkb_layout_chooser_response (GtkDialog * dialog,
			     gint response, GladeXML * chooser_dialog)
{
	GdkRectangle rect;

	if (response == GTK_RESPONSE_OK) {
		GtkTreeSelection *selection =
		    gtk_tree_view_get_selection (GTK_TREE_VIEW
						 (CWID
						  ("xkb_layouts_available")));
		GtkTreeIter selected_iter;
		GtkTreeModel *model;
		if (gtk_tree_selection_get_selected
		    (selection, &model, &selected_iter)) {
			gchar *id;
			GSList *layouts_list =
			    xkb_layouts_get_selected_list ();
			gtk_tree_model_get (model, &selected_iter,
					    AVAIL_LAYOUT_TREE_COL_ID, &id,
					    -1);
			layouts_list = g_slist_append (layouts_list, id);
			xkb_layouts_set_selected_list (layouts_list);
			/* process default switcher */
			if (g_slist_length (layouts_list) >= 2) {
				GSList *options_list =
				    xkb_options_get_selected_list ();
				gboolean any_switcher = False;
				GSList *option = options_list;
				while (option != NULL) {
					char *g, *o;
					if (gkbd_keyboard_config_split_items (option->data, &g, &o)) {
						if (!g_ascii_strcasecmp
						    (g,
						     GROUP_SWITCHERS_GROUP))
						{
							any_switcher =
							    True;
							break;
						}
					}
					option = option->next;
				}
				if (!any_switcher) {
					XklConfigItem ci;
					g_snprintf (ci.name,
						    XKL_MAX_CI_NAME_LENGTH,
						    DEFAULT_GROUP_SWITCH);
					if (xkl_config_registry_find_option
					    (config_registry,
					     GROUP_SWITCHERS_GROUP, &ci)) {
						const gchar *id =
						    gkbd_keyboard_config_merge_items
						    (GROUP_SWITCHERS_GROUP,
						     DEFAULT_GROUP_SWITCH);
						options_list =
						    g_slist_append
						    (options_list,
						     g_strdup (id));
						xkb_options_set_selected_list
						    (options_list);
					}
				}
				clear_xkb_elements_list (options_list);
			}
			clear_xkb_elements_list (layouts_list);
		}
	}

	gtk_window_get_position (GTK_WINDOW (dialog), &rect.x, &rect.y);
	gtk_window_get_size (GTK_WINDOW (dialog), &rect.width,
			     &rect.height);
	gkbd_preview_save_position (&rect);
}

void
xkb_layout_choose (GladeXML * dialog)
{
	GladeXML *chooser_dialog =
	    glade_xml_new (GNOMECC_GLADE_DIR
			   "/gnome-keyboard-properties.glade",
			   "xkb_layout_chooser", NULL);
	GtkWidget *chooser = CWID ("xkb_layout_chooser");
	GtkWidget *kbdraw = NULL;
	GtkWidget *toplevel = NULL;

	gtk_window_set_transient_for (GTK_WINDOW (chooser),
				      GTK_WINDOW (WID
						  ("keyboard_dialog")));

	xkb_layouts_fill_available_tree (chooser_dialog);
	xkb_layout_chooser_selection_changed (chooser_dialog);

#ifdef HAVE_X11_EXTENSIONS_XKB_H
	if (!strcmp (xkl_engine_get_backend_name (engine), "XKB")) {
		kbdraw = xkb_layout_preview_create_widget (chooser_dialog);
		g_object_set_data (G_OBJECT (chooser), "kbdraw", kbdraw);
		gtk_container_add (GTK_CONTAINER (CWID ("vboxPreview")),
				   kbdraw);
		gtk_widget_show_all (kbdraw);
	} else
#endif
	{
		gtk_widget_hide_all (CWID ("vboxPreview"));
	}

	g_signal_connect (G_OBJECT (chooser),
			  "response",
			  G_CALLBACK (xkb_layout_chooser_response),
			  chooser_dialog);

	toplevel = gtk_widget_get_toplevel (chooser);
	if (GTK_WIDGET_TOPLEVEL (toplevel)) {
		GdkRectangle *rect = gkbd_preview_load_position ();
		if (rect != NULL) {
			gtk_window_move (GTK_WINDOW (toplevel), rect->x,
					 rect->y);
			gtk_window_resize (GTK_WINDOW (toplevel),
					   rect->width, rect->height);
			g_free (rect);
		}
	}

	gtk_dialog_run (GTK_DIALOG (chooser));
	gtk_widget_destroy (chooser);
}
