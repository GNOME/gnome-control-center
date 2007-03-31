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

#include <string.h>

#include <gnome.h>
#include <glade/glade.h>

#include "capplet-util.h"

#include <libgnomekbd/gkbd-util.h>

#include "gnome-keyboard-properties-xkb.h"

#define GROUP_SWITCHERS_GROUP "grp"
#define DEFAULT_GROUP_SWITCH "grp:alts_toggle"
#define DEFAULT_VARIANT_ID "__default__"

#define COMBO_BOX_MODEL_COL_DESCRIPTION 0
#define COMBO_BOX_MODEL_COL_ID 1
#define COMBO_BOX_MODEL_COL_SORTING 2

static void
 xkb_layout_chooser_available_layouts_fill (GladeXML * chooser_dialog);

static void
 xkb_layout_chooser_available_variants_fill (GladeXML * chooser_dialog);

static void
xkb_layout_chooser_add_variant_to_available_variants (XklConfigRegistry *
						      config_registry,
						      XklConfigItem *
						      config_item,
						      GladeXML *
						      chooser_dialog)
{
	GtkWidget *cbe = CWID ("xkb_variants_available");
	GtkTreeIter iter;
	GtkTreeModel *model =
	    gtk_combo_box_get_model (GTK_COMBO_BOX (cbe));
	GtkTreeStore *tree_store =
	    GTK_TREE_STORE (gtk_tree_model_sort_get_model
			    (GTK_TREE_MODEL_SORT (model)));

	gtk_tree_store_append (tree_store, &iter, NULL);
	if (config_item != NULL) {
		char *utf_variant_name = xci_desc_to_utf8 (config_item);

		gtk_tree_store_set (tree_store, &iter,
				    COMBO_BOX_MODEL_COL_DESCRIPTION,
				    utf_variant_name,
				    COMBO_BOX_MODEL_COL_ID,
				    config_item->name,
				    COMBO_BOX_MODEL_COL_SORTING,
				    utf_variant_name, -1);
		g_free (utf_variant_name);
	} else {
		gtk_tree_store_set (tree_store, &iter,
				    COMBO_BOX_MODEL_COL_DESCRIPTION,
				    _("Default"), COMBO_BOX_MODEL_COL_ID,
				    DEFAULT_VARIANT_ID,
				    COMBO_BOX_MODEL_COL_SORTING, "_", -1);
		gtk_tree_store_append (tree_store, &iter, NULL);
		gtk_tree_store_set (tree_store, &iter,
				    COMBO_BOX_MODEL_COL_DESCRIPTION,
				    "***", COMBO_BOX_MODEL_COL_ID,
				    DEFAULT_VARIANT_ID,
				    COMBO_BOX_MODEL_COL_SORTING, "__", -1);
	}
}

static void
xkb_layout_chooser_add_layout_to_available_layouts (XklConfigRegistry *
						    config_registry,
						    XklConfigItem *
						    config_item,
						    GladeXML *
						    chooser_dialog)
{
	GtkWidget *cbe = CWID ("xkb_layouts_available");
	GtkTreeStore *tree_store =
	    GTK_TREE_STORE (gtk_combo_box_get_model (GTK_COMBO_BOX (cbe)));

	char *utf_layout_name = xci_desc_to_utf8 (config_item);
	GtkTreeIter iter;

	gtk_tree_store_append (tree_store, &iter, NULL);
	gtk_tree_store_set (tree_store, &iter,
			    COMBO_BOX_MODEL_COL_DESCRIPTION,
			    utf_layout_name, COMBO_BOX_MODEL_COL_ID,
			    config_item->name, -1);
	g_free (utf_layout_name);
}

static void
xkb_layout_chooser_enable_disable_buttons (GladeXML * chooser_dialog)
{
	GtkWidget *cbel = CWID ("xkb_layouts_available");
	GtkWidget *cbev = CWID ("xkb_variants_available");
	gboolean enable_ok =
	    (gtk_combo_box_get_active (GTK_COMBO_BOX (cbel)) != -1)
	    && (gtk_combo_box_get_active (GTK_COMBO_BOX (cbev)) != -1);

	gtk_dialog_set_response_sensitive (GTK_DIALOG
					   (CWID ("xkb_layout_chooser")),
					   GTK_RESPONSE_OK, enable_ok);
}

static void
xkb_layout_chooser_available_variant_changed (GladeXML * chooser_dialog)
{
	xkb_layout_preview_update (chooser_dialog);
	xkb_layout_chooser_enable_disable_buttons (chooser_dialog);
}

static void
xkb_layout_chooser_available_layout_changed (GladeXML * chooser_dialog)
{
	xkb_layout_chooser_available_variants_fill (chooser_dialog);
	xkb_layout_chooser_available_variant_changed (chooser_dialog);
}

static void
xkb_layout_chooser_sort_combo_box (GtkWidget * combo_box, int sort_column)
{
	GtkTreeModel *model =
	    gtk_combo_box_get_model (GTK_COMBO_BOX (combo_box));
	/* replace the store with the sorted version */
	GtkTreeModel *sorted_model =
	    gtk_tree_model_sort_new_with_model (model);
	gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE
					      (sorted_model),
					      sort_column,
					      GTK_SORT_ASCENDING);
	gtk_combo_box_set_model (GTK_COMBO_BOX (combo_box), sorted_model);
}

static gboolean
xkl_layout_chooser_separate_default_variant (GtkTreeModel * model,
					     GtkTreeIter * iter,
					     GladeXML * chooser_dialog)
{
	GtkTreePath *path = gtk_tree_model_get_path (model, iter);
	gint *idxs = gtk_tree_path_get_indices (path);
	gint idx = idxs[0];
	gtk_tree_path_free (path);
	return idx == 1;
}

static void
xkb_layout_chooser_available_variants_fill (GladeXML * chooser_dialog)
{
	GtkWidget *cbel = CWID ("xkb_layouts_available");
	GtkWidget *cbev = CWID ("xkb_variants_available");
	GtkTreeModel *model =
	    gtk_combo_box_get_model (GTK_COMBO_BOX (cbev));
	gint selected_layout_idx;

	if (model == NULL) {
		model =
		    GTK_TREE_MODEL (gtk_tree_store_new
				    (3, G_TYPE_STRING, G_TYPE_STRING,
				     G_TYPE_STRING));

		gtk_combo_box_set_model (GTK_COMBO_BOX (cbev),
					 GTK_TREE_MODEL (model));

		gtk_combo_box_entry_set_text_column (GTK_COMBO_BOX_ENTRY
						     (cbev),
						     COMBO_BOX_MODEL_COL_DESCRIPTION);

		xkb_layout_chooser_sort_combo_box (cbev,
						   COMBO_BOX_MODEL_COL_SORTING);

		g_signal_connect_swapped (G_OBJECT (cbev), "changed",
					  G_CALLBACK
					  (xkb_layout_chooser_available_variant_changed),
					  chooser_dialog);
		gtk_combo_box_set_row_separator_func (GTK_COMBO_BOX (cbev),
						      (GtkTreeViewRowSeparatorFunc)
						      xkl_layout_chooser_separate_default_variant,
						      chooser_dialog,
						      NULL);
	} else {
		model =
		    gtk_tree_model_sort_get_model (GTK_TREE_MODEL_SORT
						   (model));
		gtk_tree_store_clear (GTK_TREE_STORE (model));
	}

	selected_layout_idx =
	    gtk_combo_box_get_active (GTK_COMBO_BOX (cbel));

	if (selected_layout_idx != -1) {
		GtkTreeModel *lm =
		    gtk_combo_box_get_model (GTK_COMBO_BOX (cbel));
		GtkTreeIter iter;
		GValue val;

		memset (&val, 0, sizeof (val));

		gtk_tree_model_iter_nth_child (lm, &iter, NULL,
					       selected_layout_idx);
		gtk_tree_model_get_value (lm, &iter,
					  COMBO_BOX_MODEL_COL_ID, &val);

		xkl_config_registry_foreach_layout_variant
		    (config_registry, g_value_get_string (&val),
		     (ConfigItemProcessFunc)
		     xkb_layout_chooser_add_variant_to_available_variants,
		     chooser_dialog);

		xkb_layout_chooser_add_variant_to_available_variants
		    (config_registry, NULL, chooser_dialog);

		g_value_unset (&val);

		/* set default variant as selected */
		gtk_combo_box_set_active (GTK_COMBO_BOX (cbev), 0);
	}
}

static void
xkb_layout_chooser_available_layouts_fill (GladeXML * chooser_dialog)
{
	GtkWidget *cbe = CWID ("xkb_layouts_available");

	GtkTreeStore *model =
	    gtk_tree_store_new (2, G_TYPE_STRING, G_TYPE_STRING);

	gtk_combo_box_set_model (GTK_COMBO_BOX (cbe),
				 GTK_TREE_MODEL (model));

	gtk_combo_box_entry_set_text_column (GTK_COMBO_BOX_ENTRY (cbe),
					     COMBO_BOX_MODEL_COL_DESCRIPTION);

	xkl_config_registry_foreach_layout (config_registry,
					    (ConfigItemProcessFunc)
					    xkb_layout_chooser_add_layout_to_available_layouts,
					    chooser_dialog);

	xkb_layout_chooser_sort_combo_box (cbe,
					   COMBO_BOX_MODEL_COL_DESCRIPTION);

	g_signal_connect_swapped (G_OBJECT (cbe), "changed",
				  G_CALLBACK
				  (xkb_layout_chooser_available_layout_changed),
				  chooser_dialog);
}

void
xkl_layout_chooser_add_default_switcher_if_necessary (GSList *
						      layouts_list)
{
	/* process default switcher */
	if (g_slist_length (layouts_list) >= 2) {
		GSList *options_list = xkb_options_get_selected_list ();
		gboolean any_switcher = False;
		GSList *option = options_list;
		while (option != NULL) {
			char *g, *o;
			if (gkbd_keyboard_config_split_items
			    (option->data, &g, &o)) {
				if (!g_ascii_strcasecmp
				    (g, GROUP_SWITCHERS_GROUP)) {
					any_switcher = True;
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
				    (options_list, g_strdup (id));
				xkb_options_set_selected_list
				    (options_list);
			}
		}
		clear_xkb_elements_list (options_list);
	}
}

static void
xkb_layout_chooser_response (GtkDialog * dialog,
			     gint response, GladeXML * chooser_dialog)
{
	GdkRectangle rect;

	if (response == GTK_RESPONSE_OK) {
		gchar *selected_id = (gchar *)
		    xkb_layout_chooser_get_selected_id (chooser_dialog);

		if (selected_id != NULL) {
			GSList *layouts_list =
			    xkb_layouts_get_selected_list ();

			selected_id = g_strdup (selected_id);

			layouts_list =
			    g_slist_append (layouts_list, selected_id);
			xkb_layouts_set_selected_list (layouts_list);

			xkl_layout_chooser_add_default_switcher_if_necessary
			    (layouts_list);

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

	xkb_layout_chooser_available_layouts_fill (chooser_dialog);
	xkb_layout_chooser_available_layout_changed (chooser_dialog);

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

const gchar *
xkb_layout_chooser_get_selected_id (GladeXML * chooser_dialog)
{
	GtkWidget *cbel = CWID ("xkb_layouts_available");
	GtkWidget *cbev = CWID ("xkb_variants_available");
	GtkTreeModel *lm = gtk_combo_box_get_model (GTK_COMBO_BOX (cbel));
	GtkTreeModel *vm = gtk_combo_box_get_model (GTK_COMBO_BOX (cbev));
	gint lidx = gtk_combo_box_get_active (GTK_COMBO_BOX (cbel));
	gint vidx = gtk_combo_box_get_active (GTK_COMBO_BOX (cbev));

	GtkTreeIter iter;
	GValue lval, vval;
	const gchar *lname, *vname;

	static gchar retval[2 * XKL_MAX_CI_NAME_LENGTH];

	if (lidx == -1 || vidx == -1)
		return NULL;

	memset (&lval, 0, sizeof (lval));
	memset (&vval, 0, sizeof (vval));

	gtk_tree_model_iter_nth_child (lm, &iter, NULL, lidx);
	gtk_tree_model_get_value (lm, &iter,
				  COMBO_BOX_MODEL_COL_ID, &lval);

	gtk_tree_model_iter_nth_child (vm, &iter, NULL, vidx);
	gtk_tree_model_get_value (vm, &iter,
				  COMBO_BOX_MODEL_COL_ID, &vval);

	lname = g_value_get_string (&lval);
	vname = g_value_get_string (&vval);

	g_snprintf (retval, sizeof (retval),
		    strcmp (vname,
			    DEFAULT_VARIANT_ID) ?
		    gkbd_keyboard_config_merge_items (lname,
						      vname) : lname);

	g_value_unset (&lval);
	g_value_unset (&vval);

	return retval;
}
