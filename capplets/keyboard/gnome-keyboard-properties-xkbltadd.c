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

enum {
  COMBO_BOX_MODEL_COL_DESCRIPTION,
  COMBO_BOX_MODEL_COL_ID,
  COMBO_BOX_MODEL_COL_IS_DEFAULT
};

typedef struct {
	GtkListStore *list_store;
	guint n_items;
} AddVariantData;

static void
 xkb_layout_chooser_available_layouts_fill (GladeXML * chooser_dialog);

static void
 xkb_layout_chooser_available_variants_fill (GladeXML * chooser_dialog);

static void
xkb_layout_chooser_add_variant_to_available_variants (XklConfigRegistry *
						      config_registry,
						      XklConfigItem *
						      config_item,
	    					      AddVariantData *
						      data)
{
	char *utf_variant_name = xci_desc_to_utf8 (config_item);
	GtkTreeIter iter;

	gtk_list_store_insert_with_values (data->list_store, &iter, -1,
					   COMBO_BOX_MODEL_COL_DESCRIPTION,
					   utf_variant_name,
					   COMBO_BOX_MODEL_COL_ID,
					   config_item->name,
					   COMBO_BOX_MODEL_COL_IS_DEFAULT,
					   FALSE,
					   -1);
	g_free (utf_variant_name);

	data->n_items++;
}

static void
xkb_layout_chooser_add_layout_to_available_layouts (XklConfigRegistry *
						    config_registry,
						    XklConfigItem *
						    config_item,
						    GtkListStore *list_store)
{
	char *utf_layout_name = xci_desc_to_utf8 (config_item);
	GtkTreeIter iter;

	gtk_list_store_insert_with_values (list_store, &iter, -1,
					   COMBO_BOX_MODEL_COL_DESCRIPTION, utf_layout_name,
       					   COMBO_BOX_MODEL_COL_ID, config_item->name,
					   -1);
	g_free (utf_layout_name);
}

static void
xkb_layout_chooser_enable_disable_buttons (GladeXML * chooser_dialog)
{
	GtkWidget *cbel = CWID ("xkb_layouts_available");
	GtkWidget *cbev = CWID ("xkb_variants_available");
	GtkTreeIter liter, viter;
	gboolean enable_ok =
	    gtk_combo_box_get_active_iter (GTK_COMBO_BOX (cbel), &liter)
	    && gtk_combo_box_get_active_iter (GTK_COMBO_BOX (cbev), &viter);

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

static gint
xkb_layout_chooser_variant_compare_func (GtkTreeModel *model,
					 GtkTreeIter *a,
					 GtkTreeIter *b,
					 gpointer user_data)
{
	gchar *desc_a = NULL, *desc_b = NULL;
	gboolean a_is_default, b_is_default;
	gint retval;
  
	gtk_tree_model_get (model, a,
			    COMBO_BOX_MODEL_COL_DESCRIPTION, &desc_a,
      			    COMBO_BOX_MODEL_COL_IS_DEFAULT, &a_is_default,
	    		    -1);
	gtk_tree_model_get (model, b,
			    COMBO_BOX_MODEL_COL_DESCRIPTION, &desc_b,
      			    COMBO_BOX_MODEL_COL_IS_DEFAULT, &b_is_default,
	    		    -1);

	if (a_is_default || b_is_default)
		retval = b_is_default - a_is_default;
	else if (desc_a != NULL && desc_b != NULL)
		retval = g_utf8_collate (desc_a, desc_b);
	else if (desc_a != NULL)
		/* desc_b == NULL hence b is the separator, and a is not the default => b < a */
		retval = 1;
	else if (desc_b != NULL)
		/* desc_a == NULL hence a is the separator, and b is not the default => a < b */
		retval = -1;
	else
		retval = 0;

	g_free (desc_a);
	g_free (desc_b);

	return retval;
}

static gboolean
xkl_layout_chooser_separate_default_variant (GtkTreeModel * model,
					     GtkTreeIter * iter,
					     GladeXML * chooser_dialog)
{
	gchar *id;

	/* Rows with COMBO_BOX_MODEL_COL_DESCRIPTION value NULL are separators */
	gtk_tree_model_get (model, iter, COMBO_BOX_MODEL_COL_DESCRIPTION, &id, -1);
	g_free (id);

	return id == NULL;
}

static void
xkb_layout_chooser_available_variants_fill (GladeXML * chooser_dialog)
{
	GtkWidget *cbel = CWID ("xkb_layouts_available");
	GtkWidget *cbev = CWID ("xkb_variants_available");
	GtkTreeModel *model =
	    gtk_combo_box_get_model (GTK_COMBO_BOX (cbev));
	GtkTreeIter liter, vdefault_iter;
	gboolean set_default = FALSE;

	model =	GTK_TREE_MODEL (gtk_list_store_new
				(3, G_TYPE_STRING, G_TYPE_STRING,
				G_TYPE_BOOLEAN));

	if (gtk_combo_box_get_active_iter (GTK_COMBO_BOX (cbel), &liter)) {
		GtkTreeModel *lm =
		    gtk_combo_box_get_model (GTK_COMBO_BOX (cbel));
		GtkTreeIter viter;
		gchar *value;
		AddVariantData data = { GTK_LIST_STORE (model), 0 };

		/* The 'Default' row */
		gtk_list_store_insert_with_values (GTK_LIST_STORE (model), &vdefault_iter, -1,
						   COMBO_BOX_MODEL_COL_DESCRIPTION,
						   _("Default"),
						   COMBO_BOX_MODEL_COL_ID,
						   DEFAULT_VARIANT_ID,
						   COMBO_BOX_MODEL_COL_IS_DEFAULT, TRUE,
	 					   -1);
		set_default = TRUE;

		/* Now the variants of the selected lang */
		gtk_tree_model_get (lm, &liter,
				    COMBO_BOX_MODEL_COL_ID, &value,
				    -1);
		g_assert (value != NULL);

		xkl_config_registry_foreach_layout_variant
		    (config_registry, value,
		     (ConfigItemProcessFunc)
		     xkb_layout_chooser_add_variant_to_available_variants,
		     &data);
		g_free (value);
	
		/* Add a separator row, but only if we have any non-default items */
		if (data.n_items > 0)
			gtk_list_store_insert_with_values (GTK_LIST_STORE (model), &viter, -1,
							   COMBO_BOX_MODEL_COL_DESCRIPTION,
							   NULL,
	 						   COMBO_BOX_MODEL_COL_ID,
							   NULL,
							   COMBO_BOX_MODEL_COL_IS_DEFAULT, FALSE,
							   -1);
	}

	/* Turn on sorting after filling the store, since that's faster */
	gtk_tree_sortable_set_sort_func (GTK_TREE_SORTABLE (model),
					 COMBO_BOX_MODEL_COL_DESCRIPTION,
      					 (GtkTreeIterCompareFunc)
					 xkb_layout_chooser_variant_compare_func,
      					 NULL, NULL);
	gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (model),
					      COMBO_BOX_MODEL_COL_DESCRIPTION,
	   				      GTK_SORT_ASCENDING);

	gtk_combo_box_set_model (GTK_COMBO_BOX (cbev), model);

	/* Select the default variant */
	if (set_default) {
		gtk_combo_box_set_active_iter (GTK_COMBO_BOX (cbev), &vdefault_iter);
	}
}

static void
xkb_layout_chooser_available_layouts_fill (GladeXML * chooser_dialog)
{
	GtkWidget *cbe = CWID ("xkb_layouts_available");
	GtkWidget *cbev = CWID ("xkb_variants_available");
	GtkCellRenderer *renderer;
	GtkTreeModel *model =
	    GTK_TREE_MODEL (gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_STRING));

	gtk_combo_box_set_model (GTK_COMBO_BOX (cbe), model);

        renderer = gtk_cell_renderer_text_new ();
        gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (cbe), renderer, TRUE);
        gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (cbe), renderer,
                                        "text", COMBO_BOX_MODEL_COL_DESCRIPTION,
                                        NULL);

	xkl_config_registry_foreach_layout (config_registry,
					    (ConfigItemProcessFunc)
					    xkb_layout_chooser_add_layout_to_available_layouts,
					    model);

	/* Turn on sorting after filling the model since that's faster */
	gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (model),
					      COMBO_BOX_MODEL_COL_DESCRIPTION,
	   				      GTK_SORT_ASCENDING);

	g_signal_connect_swapped (G_OBJECT (cbe), "changed",
				  G_CALLBACK
				  (xkb_layout_chooser_available_layout_changed),
				  chooser_dialog);

	/* Setup the variants combo */
	renderer = gtk_cell_renderer_text_new ();
	gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (cbev), renderer, TRUE);
	gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (cbev), renderer,
					"text", COMBO_BOX_MODEL_COL_DESCRIPTION,
					NULL);

	g_signal_connect_swapped (G_OBJECT (cbev), "changed",
				  G_CALLBACK
				  (xkb_layout_chooser_available_variant_changed),
				  chooser_dialog);
	gtk_combo_box_set_row_separator_func (GTK_COMBO_BOX (cbev),
					      (GtkTreeViewRowSeparatorFunc)
					      xkl_layout_chooser_separate_default_variant,
					      chooser_dialog,
					      NULL);
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
			XklConfigItem *ci = xkl_config_item_new ();
			g_snprintf (ci->name,
				    XKL_MAX_CI_NAME_LENGTH,
				    DEFAULT_GROUP_SWITCH);
			if (xkl_config_registry_find_option
			    (config_registry, GROUP_SWITCHERS_GROUP, ci)) {
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
			g_object_unref (G_OBJECT (ci));
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
		gtk_container_add (GTK_CONTAINER (CWID ("previewFrame")),
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

gchar *
xkb_layout_chooser_get_selected_id (GladeXML * chooser_dialog)
{
	GtkWidget *cbel = CWID ("xkb_layouts_available");
	GtkWidget *cbev = CWID ("xkb_variants_available");
	GtkTreeModel *lm = gtk_combo_box_get_model (GTK_COMBO_BOX (cbel));
	GtkTreeModel *vm = gtk_combo_box_get_model (GTK_COMBO_BOX (cbev));
	GtkTreeIter liter, viter;
	gchar *lname, *vname;
	gchar *retval;

	if (!gtk_combo_box_get_active_iter (GTK_COMBO_BOX (cbel), &liter) ||
	    !gtk_combo_box_get_active_iter (GTK_COMBO_BOX (cbev), &viter))
		return NULL;

	gtk_tree_model_get (lm, &liter,
			    COMBO_BOX_MODEL_COL_ID, &lname,
			    -1);

	gtk_tree_model_get (vm, &viter,
			    COMBO_BOX_MODEL_COL_ID, &vname,
			    -1);

	if (strcmp (vname, DEFAULT_VARIANT_ID))
		retval = g_strdup (gkbd_keyboard_config_merge_items (lname, vname));
	else
		retval = g_strdup (lname);

	g_free (lname);
	g_free (vname);

	return retval;
}
