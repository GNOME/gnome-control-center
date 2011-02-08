/* gnome-region-panel-xkbltadd.c
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

#include <libgnomekbd/gkbd-keyboard-drawing.h>
#include <libgnomekbd/gkbd-util.h>

#include "gnome-region-panel-xkb.h"

enum {
	COMBO_BOX_MODEL_COL_SORT,
	COMBO_BOX_MODEL_COL_VISIBLE,
	COMBO_BOX_MODEL_COL_XKB_ID,
	COMBO_BOX_MODEL_COL_COUNTRY_DESC,
	COMBO_BOX_MODEL_COL_LANGUAGE_DESC
};

static gchar **search_pattern_list = NULL;

#define RESPONSE_PREVIEW 1

static void
xkl_layout_chooser_add_default_switcher_if_necessary (gchar **
						      layouts_list)
{
	gchar **options_list = xkb_options_get_selected_list ();
	gboolean was_appended;

	options_list =
	    gkbd_keyboard_config_add_default_switch_option_if_necessary
	    (layouts_list, options_list, &was_appended);
	if (was_appended)
		xkb_options_set_selected_list (options_list);
	g_strfreev (options_list);
}

static void
xkb_layout_chooser_response (GtkDialog * dialog,
			     gint response, GtkBuilder * chooser_dialog)
{
	switch (response)
	case GTK_RESPONSE_OK:{
			gchar *selected_id = (gchar *)
			    xkb_layout_chooser_get_selected_id
			    (chooser_dialog);

			if (selected_id != NULL) {
				gchar **layouts_list =
				    xkb_layouts_get_selected_list ();

				layouts_list =
				    gkbd_strv_append (layouts_list,
						      g_strdup
						      (selected_id));

				xkb_layouts_set_selected_list
				    (layouts_list);

				xkl_layout_chooser_add_default_switcher_if_necessary
				    (layouts_list);

				g_strfreev (layouts_list);
			}
			break;
	case RESPONSE_PREVIEW:{
				gchar *selected_id = (gchar *)
				    xkb_layout_chooser_get_selected_id
				    (chooser_dialog);

				if (selected_id != NULL) {
					GtkWidget *dlg =
					    gkbd_keyboard_drawing_dialog_new
					    ();
					gkbd_keyboard_drawing_dialog_set_layout
					    (dlg, config_registry,
					     selected_id);
					gtk_window_set_transient_for
					    (GTK_WINDOW (dlg),
					     GTK_WINDOW (CWID
							 ("xkb_layout_chooser")));
					gtk_widget_show_all (dlg);
				}
			}

			return;
		}
	gtk_widget_destroy (GTK_WIDGET (dialog));
}

static gchar *
xkl_create_description_from_list (const XklConfigItem * item,
				  const XklConfigItem * subitem,
				  const gchar * prop_name,
				  const gchar *
				  (*desc_getter) (const gchar * code))
{
	gchar *rv = NULL, *code = NULL;
	gchar **list = NULL;
	const gchar *desc;

	if (subitem != NULL)
		list =
		    (gchar
		     **) (g_object_get_data (G_OBJECT (subitem),
					     prop_name));
	if (list == NULL || *list == 0)
		list =
		    (gchar
		     **) (g_object_get_data (G_OBJECT (item), prop_name));

	/* First try the parent id as such */
	desc = desc_getter (item->name);
	if (desc != NULL) {
		rv = g_utf8_strup (desc, -1);
	} else {
		code = g_utf8_strup (item->name, -1);
		desc = desc_getter (code);
		if (desc != NULL) {
			rv = g_utf8_strup (desc, -1);
		}
		g_free (code);
	}

	if (list == NULL || *list == 0)
		return rv;

	while (*list != 0) {
		code = *list++;
		desc = desc_getter (code);
		if (desc != NULL) {
			gchar *udesc = g_utf8_strup (desc, -1);
			if (rv == NULL) {
				rv = udesc;
			} else {
				gchar *orv = rv;
				rv = g_strdup_printf ("%s %s", rv, udesc);
				g_free (orv);
				g_free (udesc);
			}
		}
	}
	return rv;
}

static void
xkl_layout_add_to_list (XklConfigRegistry * config,
			const XklConfigItem * item,
			const XklConfigItem * subitem,
			GtkBuilder * chooser_dialog)
{
	GtkListStore *list_store =
	    GTK_LIST_STORE (gtk_builder_get_object (chooser_dialog,
						    "layout_list_model"));
	GtkTreeIter iter;
	gchar *utf_variant_name =
	    subitem ?
	    xkb_layout_description_utf8 (gkbd_keyboard_config_merge_items
					 (item->name,
					  subitem->name)) :
	    xci_desc_to_utf8 (item);
	const gchar *xkb_id =
	    subitem ? gkbd_keyboard_config_merge_items (item->name,
							subitem->name) :
	    item->name;

	gchar *country_desc =
	    xkl_create_description_from_list (item, subitem,
					      XCI_PROP_COUNTRY_LIST,
					      xkl_get_country_name);
	gchar *language_desc =
	    xkl_create_description_from_list (item, subitem,
					      XCI_PROP_LANGUAGE_LIST,
					      xkl_get_language_name);

	if (subitem
	    && g_object_get_data (G_OBJECT (subitem),
				  XCI_PROP_EXTRA_ITEM)) {
		gchar *buf =
		    g_strdup_printf ("<i>%s</i>", utf_variant_name);
		gtk_list_store_insert_with_values (list_store, &iter, -1,
						   COMBO_BOX_MODEL_COL_SORT,
						   utf_variant_name,
						   COMBO_BOX_MODEL_COL_VISIBLE,
						   buf,
						   COMBO_BOX_MODEL_COL_XKB_ID,
						   xkb_id,
						   COMBO_BOX_MODEL_COL_COUNTRY_DESC,
						   country_desc,
						   COMBO_BOX_MODEL_COL_LANGUAGE_DESC,
						   language_desc, -1);
		g_free (buf);
	} else
		gtk_list_store_insert_with_values (list_store, &iter,
						   -1,
						   COMBO_BOX_MODEL_COL_SORT,
						   utf_variant_name,
						   COMBO_BOX_MODEL_COL_VISIBLE,
						   utf_variant_name,
						   COMBO_BOX_MODEL_COL_XKB_ID,
						   xkb_id,
						   COMBO_BOX_MODEL_COL_COUNTRY_DESC,
						   country_desc,
						   COMBO_BOX_MODEL_COL_LANGUAGE_DESC,
						   language_desc, -1);
	g_free (utf_variant_name);
	g_free (country_desc);
	g_free (language_desc);
}

static void
xkb_layout_filter_changed (GtkBuilder * chooser_dialog)
{
	GtkTreeModelFilter *filtered_model =
	    GTK_TREE_MODEL_FILTER (gtk_builder_get_object (chooser_dialog,
							   "filtered_layout_list_model"));
	GtkWidget *xkb_layout_filter = CWID ("xkb_layout_filter");
	const gchar *pattern =
	    gtk_entry_get_text (GTK_ENTRY (xkb_layout_filter));
	gchar *upattern = g_utf8_strup (pattern, -1);

	if (search_pattern_list != NULL)
		g_strfreev (search_pattern_list);

	search_pattern_list = g_strsplit (upattern, " ", -1);
	g_free (upattern);

	gtk_tree_model_filter_refilter (filtered_model);
}

static void
xkb_layout_chooser_selection_changed (GtkTreeSelection * selection,
				      GtkBuilder * chooser_dialog)
{
	GList *selected_layouts =
	    gtk_tree_selection_get_selected_rows (selection, NULL);
	GtkWidget *add_button = CWID ("btnOk");
	GtkWidget *preview_button = CWID ("btnPreview");
	gboolean anything_selected = g_list_length (selected_layouts) == 1;
	gtk_widget_set_sensitive (add_button, anything_selected);
	gtk_widget_set_sensitive (preview_button, anything_selected);
}

static gboolean
xkb_filter_layouts (GtkTreeModel * model,
		    GtkTreeIter * iter, gpointer data)
{
	gchar *desc = NULL, *country_desc = NULL, *language_desc =
	    NULL, **pattern;
	gboolean rv = TRUE;

	if (search_pattern_list == NULL || search_pattern_list[0] == NULL)
		return TRUE;

	gtk_tree_model_get (model, iter,
			    COMBO_BOX_MODEL_COL_SORT, &desc,
			    COMBO_BOX_MODEL_COL_COUNTRY_DESC,
			    &country_desc,
			    COMBO_BOX_MODEL_COL_LANGUAGE_DESC,
			    &language_desc, -1);

	pattern = search_pattern_list;
	do {
		gboolean is_pattern_found = FALSE;
		gchar *udesc = g_utf8_strup (desc, -1);
		if (udesc != NULL && g_strstr_len (udesc, -1, *pattern)) {
			is_pattern_found = TRUE;
		} else if (country_desc != NULL
			   && g_strstr_len (country_desc, -1, *pattern)) {
			is_pattern_found = TRUE;
		} else if (language_desc != NULL
			   && g_strstr_len (language_desc, -1, *pattern)) {
			is_pattern_found = TRUE;
		}
		g_free (udesc);

		if (!is_pattern_found) {
			rv = FALSE;
			break;
		}

	} while (*++pattern != NULL);

	g_free (desc);
	g_free (country_desc);
	g_free (language_desc);
	return rv;
}

void
xkb_layout_choose (GtkBuilder * dialog)
{
	GtkBuilder *chooser_dialog = gtk_builder_new ();
	GtkWidget *chooser, *xkb_filtered_layouts_list, *xkb_layout_filter;
	GtkTreeViewColumn *visible_column;
	GtkTreeSelection *selection;
	GtkListStore *model;
	GtkTreeModelFilter *filtered_model;

	gtk_builder_add_from_file (chooser_dialog, GNOMECC_UI_DIR
				   "/gnome-region-panel-layout-chooser.ui",
				   NULL);
	chooser = CWID ("xkb_layout_chooser");
	xkb_filtered_layouts_list = CWID ("xkb_filtered_layouts_list");
	xkb_layout_filter = CWID ("xkb_layout_filter");

	visible_column =
	    gtk_tree_view_column_new_with_attributes ("Layout",
						      gtk_cell_renderer_text_new
						      (), "text",
						      COMBO_BOX_MODEL_COL_VISIBLE,
						      NULL);

	gtk_window_set_transient_for (GTK_WINDOW (chooser),
				      GTK_WINDOW
				      (gtk_widget_get_toplevel
				       (WID ("region_notebook"))));

	gtk_tree_view_append_column (GTK_TREE_VIEW
				     (xkb_filtered_layouts_list),
				     visible_column);
	g_signal_connect_swapped (G_OBJECT (xkb_layout_filter),
				  "notify::text",
				  G_CALLBACK
				  (xkb_layout_filter_changed),
				  chooser_dialog);

	g_signal_connect (G_OBJECT (chooser),
			  "response",
			  G_CALLBACK (xkb_layout_chooser_response),
			  chooser_dialog);

	selection =
	    gtk_tree_view_get_selection (GTK_TREE_VIEW
					 (xkb_filtered_layouts_list));

	g_signal_connect (G_OBJECT (selection),
			  "changed",
			  G_CALLBACK
			  (xkb_layout_chooser_selection_changed),
			  chooser_dialog);

	xkb_layout_chooser_selection_changed (selection, chooser_dialog);

	filtered_model =
	    GTK_TREE_MODEL_FILTER (gtk_builder_get_object
				   (chooser_dialog,
				    "filtered_layout_list_model"));
	model =
	    GTK_LIST_STORE (gtk_builder_get_object
			    (chooser_dialog, "layout_list_model"));

	xkl_config_registry_search_by_pattern (config_registry,
					       NULL,
					       (TwoConfigItemsProcessFunc)
					       (xkl_layout_add_to_list),
					       chooser_dialog);
	gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (model),
					      COMBO_BOX_MODEL_COL_SORT,
					      GTK_SORT_ASCENDING);

	gtk_tree_model_filter_set_visible_func (filtered_model,
						xkb_filter_layouts,
						NULL, NULL);

	gtk_widget_grab_focus (xkb_layout_filter);

	gtk_widget_show (chooser);
}

gchar *
xkb_layout_chooser_get_selected_id (GtkBuilder * chooser_dialog)
{
	GtkTreeModel *filtered_list_model;
	GtkWidget *xkb_filtered_layouts_list =
	    CWID ("xkb_filtered_layouts_list");
	GtkTreeIter viter;
	gchar *v_id;
	GtkTreeSelection *selection =
	    gtk_tree_view_get_selection (GTK_TREE_VIEW
					 (xkb_filtered_layouts_list));
	GList *selected_layouts =
	    gtk_tree_selection_get_selected_rows (selection,
						  &filtered_list_model);

	if (g_list_length (selected_layouts) != 1)
		return NULL;

	gtk_tree_model_get_iter (filtered_list_model,
				 &viter,
				 (GtkTreePath *) (selected_layouts->data));
	g_list_foreach (selected_layouts,
			(GFunc) gtk_tree_path_free, NULL);
	g_list_free (selected_layouts);

	gtk_tree_model_get (filtered_list_model, &viter,
			    COMBO_BOX_MODEL_COL_XKB_ID, &v_id, -1);

	return v_id;
}
