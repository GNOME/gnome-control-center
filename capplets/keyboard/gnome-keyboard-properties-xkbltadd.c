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

#include <libgnomekbd/gkbd-keyboard-drawing.h>
#include <libgnomekbd/gkbd-util.h>

#include "capplet-util.h"
#include "gnome-keyboard-properties-xkb.h"

#define GROUP_SWITCHERS_GROUP "grp"
#define DEFAULT_GROUP_SWITCH "grp:alts_toggle"
#define DEFAULT_VARIANT_ID "__default__"

enum {
	COMBO_BOX_MODEL_COL_DESCRIPTION,
	COMBO_BOX_MODEL_COL_XKB_ID,
	COMBO_BOX_MODEL_COL_REAL_ID
};

typedef void (*LayoutIterFunc) (XklConfigRegistry * config,
				ConfigItemProcessFunc func, gpointer data);

typedef struct {
	GtkListStore *list_store;
	const gchar *lang_id;
} AddVariantData;

static void
 xkb_layout_chooser_available_layouts_fill (GladeXML * chooser_dialog,
					    const gchar cblid[],
					    const gchar cbvid[],
					    LayoutIterFunc layout_iterator,
					    ConfigItemProcessFunc
					    layout_handler,
					    GCallback
					    combo_changed_notify);

static void
 xkb_layout_chooser_available_language_variants_fill (GladeXML *
						      chooser_dialog);

static void
 xkb_layout_chooser_available_country_variants_fill (GladeXML *
						     chooser_dialog);

static void
 xkb_layout_chooser_add_variant_to_available_country_variants
    (XklConfigRegistry * config_registry,
     XklConfigItem * parent_config_item, XklConfigItem * config_item,
     AddVariantData * data) {
	gchar *utf_variant_name = config_item ?
	    xkb_layout_description_utf8 (gkbd_keyboard_config_merge_items
					 (parent_config_item->name,
					  config_item->name)) :
	    xci_desc_to_utf8 (parent_config_item);
	GtkTreeIter iter;
	const gchar *xkb_id =
	    config_item ?
	    gkbd_keyboard_config_merge_items (parent_config_item->name,
					      config_item->
					      name) : parent_config_item->
	    name;

	gtk_list_store_insert_with_values (data->list_store, &iter, -1,
					   COMBO_BOX_MODEL_COL_DESCRIPTION,
					   utf_variant_name,
					   COMBO_BOX_MODEL_COL_XKB_ID,
					   xkb_id, -1);
	g_free (utf_variant_name);
}

static void
 xkb_layout_chooser_add_variant_to_available_language_variants
    (XklConfigRegistry * config_registry,
     XklConfigItem * parent_config_item, XklConfigItem * config_item,
     AddVariantData * data) {
	xkb_layout_chooser_add_variant_to_available_country_variants
	    (config_registry, parent_config_item, config_item, data);
}

static void
xkb_layout_chooser_add_language_to_available_languages (XklConfigRegistry *
							config_registry,
							XklConfigItem *
							config_item,
							GtkListStore *
							list_store)
{
	gtk_list_store_insert_with_values (list_store, NULL, -1,
					   COMBO_BOX_MODEL_COL_DESCRIPTION,
					   config_item->description,
					   COMBO_BOX_MODEL_COL_REAL_ID,
					   config_item->name, -1);
}

static void
xkb_layout_chooser_add_country_to_available_countries (XklConfigRegistry *
						       config_registry,
						       XklConfigItem *
						       config_item,
						       GtkListStore *
						       list_store)
{
	gtk_list_store_insert_with_values (list_store, NULL, -1,
					   COMBO_BOX_MODEL_COL_DESCRIPTION,
					   config_item->description,
					   COMBO_BOX_MODEL_COL_REAL_ID,
					   config_item->name, -1);
}

static void
xkb_layout_chooser_enable_disable_buttons (GladeXML * chooser_dialog)
{
	GtkWidget *cbv =
	    CWID (gtk_notebook_get_current_page
		  (GTK_NOTEBOOK (CWID ("choosers_nb"))) ?
		  "xkb_language_variants_available" :
		  "xkb_country_variants_available");
	GtkTreeIter viter;
	gboolean enable_ok =
	    gtk_combo_box_get_active_iter (GTK_COMBO_BOX (cbv),
					   &viter);

	gtk_dialog_set_response_sensitive (GTK_DIALOG
					   (CWID
					    ("xkb_layout_chooser")),
					   GTK_RESPONSE_OK, enable_ok);
	gtk_widget_set_sensitive (CWID ("btnPrint"), enable_ok);
}

static void
xkb_layout_chooser_available_variant_changed (GladeXML * chooser_dialog)
{
	xkb_layout_preview_update (chooser_dialog);
	xkb_layout_chooser_enable_disable_buttons (chooser_dialog);
}

static void
xkb_layout_chooser_available_language_changed (GladeXML * chooser_dialog)
{
	xkb_layout_chooser_available_language_variants_fill
	    (chooser_dialog);
	xkb_layout_chooser_available_variant_changed (chooser_dialog);
}

static void
xkb_layout_chooser_available_country_changed (GladeXML * chooser_dialog)
{
	xkb_layout_chooser_available_country_variants_fill
	    (chooser_dialog);
	xkb_layout_chooser_available_variant_changed (chooser_dialog);
}

static void
xkb_layout_chooser_page_changed (GtkWidget * notebook, GtkWidget * page,
				 gint page_num, GladeXML * chooser_dialog)
{
	xkb_layout_chooser_available_variant_changed (chooser_dialog);
}

static void
xkb_layout_chooser_available_language_variants_fill (GladeXML *
						     chooser_dialog)
{
	GtkWidget *cbl = CWID ("xkb_languages_available");
	GtkWidget *cbv = CWID ("xkb_language_variants_available");
	GtkListStore *list_store;
	GtkTreeIter liter;

	list_store = gtk_list_store_new
	    (3, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);

	if (gtk_combo_box_get_active_iter (GTK_COMBO_BOX (cbl), &liter)) {
		GtkTreeModel *lm =
		    gtk_combo_box_get_model (GTK_COMBO_BOX (cbl));
		gchar *lang_id;
		AddVariantData data = { list_store, 0 };

		/* Now the variants of the selected layout */
		gtk_tree_model_get (lm, &liter,
				    COMBO_BOX_MODEL_COL_REAL_ID,
				    &lang_id, -1);
		data.lang_id = lang_id;

		xkl_config_registry_foreach_language_variant
		    (config_registry, lang_id, (TwoConfigItemsProcessFunc)
		     xkb_layout_chooser_add_variant_to_available_language_variants,
		     &data);
		g_free (lang_id);
	}

	/* Turn on sorting after filling the store, since that's faster */
	gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE
					      (list_store),
					      COMBO_BOX_MODEL_COL_DESCRIPTION,
					      GTK_SORT_ASCENDING);

	gtk_combo_box_set_model (GTK_COMBO_BOX (cbv),
				 GTK_TREE_MODEL (list_store));
	gtk_combo_box_set_active (GTK_COMBO_BOX (cbv), 0);
}

static void
xkb_layout_chooser_available_country_variants_fill (GladeXML *
						    chooser_dialog)
{
	GtkWidget *cbl = CWID ("xkb_countries_available");
	GtkWidget *cbv = CWID ("xkb_country_variants_available");
	GtkListStore *list_store;
	GtkTreeIter liter;

	list_store = gtk_list_store_new
	    (3, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);

	if (gtk_combo_box_get_active_iter (GTK_COMBO_BOX (cbl), &liter)) {
		GtkTreeModel *lm =
		    gtk_combo_box_get_model (GTK_COMBO_BOX (cbl));
		gchar *country_id;
		AddVariantData data = { list_store, 0 };

		/* Now the variants of the selected layout */
		gtk_tree_model_get (lm, &liter,
				    COMBO_BOX_MODEL_COL_REAL_ID,
				    &country_id, -1);
		xkl_config_registry_foreach_country_variant
		    (config_registry, country_id,
		     (TwoConfigItemsProcessFunc)
		     xkb_layout_chooser_add_variant_to_available_country_variants,
		     &data);
		g_free (country_id);
	}

	/* Turn on sorting after filling the store, since that's faster */
	gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE
					      (list_store),
					      COMBO_BOX_MODEL_COL_DESCRIPTION,
					      GTK_SORT_ASCENDING);

	gtk_combo_box_set_model (GTK_COMBO_BOX (cbv),
				 GTK_TREE_MODEL (list_store));
	gtk_combo_box_set_active (GTK_COMBO_BOX (cbv), 0);
}

static void
xkb_layout_chooser_available_layouts_fill (GladeXML *
					   chooser_dialog,
					   const gchar cblid[],
					   const gchar cbvid[],
					   LayoutIterFunc layout_iterator,
					   ConfigItemProcessFunc
					   layout_handler,
					   GCallback combo_changed_notify)
{
	GtkWidget *cbl = CWID (cblid);
	GtkWidget *cbev = CWID (cbvid);
	GtkCellRenderer *renderer;
	GtkListStore *list_store =
	    gtk_list_store_new (3, G_TYPE_STRING, G_TYPE_STRING,
				G_TYPE_STRING);

	gtk_combo_box_set_model (GTK_COMBO_BOX (cbl),
				 GTK_TREE_MODEL (list_store));

	renderer = gtk_cell_renderer_text_new ();
	gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (cbl), renderer, TRUE);
	gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (cbl),
					renderer, "markup",
					COMBO_BOX_MODEL_COL_DESCRIPTION,
					NULL);

	layout_iterator (config_registry, layout_handler, list_store);

	/* Turn on sorting after filling the model since that's faster */
	gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE
					      (list_store),
					      COMBO_BOX_MODEL_COL_DESCRIPTION,
					      GTK_SORT_ASCENDING);

	g_signal_connect_swapped (G_OBJECT (cbl), "changed",
				  combo_changed_notify, chooser_dialog);

	/* Setup the variants combo */
	renderer = gtk_cell_renderer_text_new ();
	gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (cbev),
				    renderer, TRUE);
	gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (cbev),
					renderer, "markup",
					COMBO_BOX_MODEL_COL_DESCRIPTION,
					NULL);

	g_signal_connect_swapped (G_OBJECT (cbev), "changed",
				  G_CALLBACK
				  (xkb_layout_chooser_available_variant_changed),
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
xkb_layout_chooser_print (GladeXML * chooser_dialog)
{
	GtkWidget *chooser = CWID ("xkb_layout_chooser");
	GtkWidget *kbdraw =
	    GTK_WIDGET (g_object_get_data (G_OBJECT (chooser), "kbdraw"));
	const char *id =
	    xkb_layout_chooser_get_selected_id (chooser_dialog);
	char *descr = xkb_layout_description_utf8 (id);
	gkbd_keyboard_drawing_print (GKBD_KEYBOARD_DRAWING
				     (kbdraw),
				     GTK_WINDOW (CWID
						 ("xkb_layout_chooser")),
				     descr);
	g_free (descr);
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
	} else if (response == gtk_dialog_get_response_for_widget
		   (dialog, CWID ("btnPrint"))) {
		xkb_layout_chooser_print (chooser_dialog);
		g_signal_stop_emission_by_name (dialog, "response");
		return;
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
	GtkWidget *lang_chooser = CWID ("xkb_languages_available");
	GtkWidget *notebook = CWID ("choosers_nb");
	GtkWidget *kbdraw = NULL;
	GtkWidget *toplevel = NULL;

	gtk_window_set_transient_for (GTK_WINDOW (chooser),
				      GTK_WINDOW (WID
						  ("keyboard_dialog")));

	xkb_layout_chooser_available_layouts_fill (chooser_dialog,
						   "xkb_countries_available",
						   "xkb_country_variants_available",
						   xkl_config_registry_foreach_country,
						   (ConfigItemProcessFunc)
						   xkb_layout_chooser_add_country_to_available_countries,
						   G_CALLBACK
						   (xkb_layout_chooser_available_country_changed));
	xkb_layout_chooser_available_layouts_fill (chooser_dialog,
						   "xkb_languages_available",
						   "xkb_language_variants_available",
						   xkl_config_registry_foreach_language,
						   (ConfigItemProcessFunc)
						   xkb_layout_chooser_add_language_to_available_languages,
						   G_CALLBACK
						   (xkb_layout_chooser_available_language_changed));

	g_signal_connect_after (G_OBJECT (notebook), "switch_page",
				G_CALLBACK
				(xkb_layout_chooser_page_changed),
				chooser_dialog);

	xkb_layout_chooser_available_country_changed (chooser_dialog);

	if (gtk_tree_model_iter_n_children
	    (gtk_combo_box_get_model (GTK_COMBO_BOX (lang_chooser)),
	     NULL)) {
		xkb_layout_chooser_available_language_changed
		    (chooser_dialog);
	} else {
		/* If language info is not available - remove the corresponding tab,
		   pretend there is no notebook at all */
		gtk_notebook_remove_page (GTK_NOTEBOOK (notebook), 1);
		gtk_notebook_set_show_tabs (GTK_NOTEBOOK (notebook),
					    FALSE);
		gtk_notebook_set_show_border (GTK_NOTEBOOK (notebook),
					      FALSE);
	}

#ifdef HAVE_X11_EXTENSIONS_XKB_H
	if (!strcmp (xkl_engine_get_backend_name (engine), "XKB")) {
		kbdraw = xkb_layout_preview_create_widget (chooser_dialog);
		g_object_set_data (G_OBJECT (chooser), "kbdraw", kbdraw);
		gtk_container_add (GTK_CONTAINER
				   (CWID ("previewFrame")), kbdraw);
		gtk_widget_show_all (kbdraw);
		gtk_button_box_set_child_secondary (GTK_BUTTON_BOX
						    (CWID
						     ("hbtnBox")),
						    CWID
						    ("btnPrint"), TRUE);
	} else
#endif
	{
		gtk_widget_hide_all (CWID ("vboxPreview"));
		gtk_widget_hide (CWID ("btnPrint"));
	}

	g_signal_connect (G_OBJECT (chooser),
			  "response",
			  G_CALLBACK (xkb_layout_chooser_response),
			  chooser_dialog);

	toplevel = gtk_widget_get_toplevel (chooser);
	if (GTK_WIDGET_TOPLEVEL (toplevel)) {
		GdkRectangle *rect = gkbd_preview_load_position ();
		if (rect != NULL) {
			gtk_window_move (GTK_WINDOW (toplevel),
					 rect->x, rect->y);
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
	GtkWidget *cbv =
	    CWID (gtk_notebook_get_current_page
		  (GTK_NOTEBOOK (CWID ("choosers_nb"))) ?
		  "xkb_language_variants_available" :
		  "xkb_country_variants_available");
	GtkTreeModel *vm = gtk_combo_box_get_model (GTK_COMBO_BOX (cbv));
	GtkTreeIter viter;
	gchar *v_id;

	if (!gtk_combo_box_get_active_iter (GTK_COMBO_BOX (cbv), &viter))
		return NULL;

	gtk_tree_model_get (vm, &viter,
			    COMBO_BOX_MODEL_COL_XKB_ID, &v_id, -1);

	return v_id;
}
