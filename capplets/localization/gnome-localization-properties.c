/* -*- mode: c; style: linux -*- */

/* gnome-localization-properties.c
 * Copyright (C) 2003 Carlos Perelló Marín
 *
 * Written by: Carlos Perelló Marín <carlos@gnome.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <locale.h>
#include <string.h>
#include <gnome.h>
#include <gconf/gconf-client.h>
#include <glade/glade.h>
#include <unicode/uloc.h>
#include <unicode/udat.h>
#include <unicode/ucal.h>

#include "capplet-util.h"
#include "gconf-property-editor.h"
#include "activate-settings-daemon.h"
#include "capplet-stock-icons.h"

gboolean our_update;
gint idx2Select = -1;

#define GCONF_LOCALIZATION_ROOT_KEY "/desktop/gnome/interface"

#define get_selected_languages_list() \
	gconf_client_get_list (gconf_client_get_default (), \
			GCONF_LOCALIZATION_ROOT_KEY "/languages", \
			GCONF_VALUE_STRING, NULL)

#define set_selected_languages_list(list) \
	gconf_client_set_list (gconf_client_get_default (), \
			GCONF_LOCALIZATION_ROOT_KEY "/languages", \
			GCONF_VALUE_STRING, (list), NULL)

#define ICU_STRING_LONG 128

static void fill_region_option_menu (GladeXML *dialog);

static GladeXML *
create_dialog (void)
{
	GladeXML *dialog;

	dialog = glade_xml_new
		(GNOMECC_DATA_DIR "/interfaces/gnome-localization-properties.glade",
		 NULL, NULL);

	return dialog;
}

static void
enable_disable_move_buttons_cb (GladeXML *dialog)
{
	GtkWidget *up_button;
	GtkWidget *down_button;
	GtkWidget *tree_view;
	GtkTreeSelection *selection;
	GtkTreeIter iter;
	GtkTreeModel *model;
	gboolean can_move_up;
	gboolean can_move_down;
	int nlangs;

	up_button = WID ("language_up_button");
	down_button = WID ("language_down_button");
	tree_view = WID ("languages_treeview");

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (tree_view));
	model = gtk_tree_view_get_model (GTK_TREE_VIEW (tree_view));
	nlangs = gtk_tree_model_iter_n_children (model, NULL);

	can_move_up = FALSE;
	can_move_down = FALSE;
	
	if (gtk_tree_selection_get_selected (selection, NULL, &iter)) {
		GtkTreePath *path =
			gtk_tree_model_get_path (model, &iter);
		if (path != NULL) {
			int *indices = gtk_tree_path_get_indices (path);
			int idx = indices[0];
			can_move_up = idx > 0;
			can_move_down = idx < (nlangs - 1);
			gtk_tree_path_free (path);
		}
	}
	gtk_widget_set_sensitive (up_button, can_move_up);
	gtk_widget_set_sensitive (down_button, can_move_down);
}

static void
prepare_selected_languages_tree (GladeXML * dialog)
{
	GtkListStore *list_store;
	GtkWidget *tree_view;
	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;
	GtkTreeSelection *selection;

	list_store = gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_STRING);
	tree_view = WID ("languages_treeview");
	renderer = gtk_cell_renderer_text_new ();
	column = gtk_tree_view_column_new_with_attributes (NULL,
			renderer, "text", 0, NULL);

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (tree_view));
	g_signal_connect_swapped (G_OBJECT (selection), "changed",
				  G_CALLBACK
				  (enable_disable_move_buttons_cb),
				  dialog);

	
	gtk_tree_view_set_model (GTK_TREE_VIEW (tree_view),
			GTK_TREE_MODEL (list_store));
	gtk_tree_view_append_column (GTK_TREE_VIEW (tree_view), column);
}

static void
item_toggled_cb (GtkCellRendererToggle *cell,
		gchar *path_str,
		GtkTreeModel *model)
{
	GtkTreePath *path;
	GtkTreeIter iter;
	gboolean toggle_item;

	path = gtk_tree_path_new_from_string (path_str);

	/* get toggled iter */
	gtk_tree_model_get_iter (model, &iter, path);
	gtk_tree_model_get (model, &iter, 0, &toggle_item, -1);

	/* do something with the value */
	toggle_item ^= 1;

	/* set new value */
	gtk_tree_store_set (GTK_TREE_STORE (model), &iter, 0,
			toggle_item, -1);

	/* clean up */
	gtk_tree_path_free (path);
}

static void
prepare_available_languages_tree (GladeXML *dialog)
{
	GtkTreeStore *tree_store;
	GtkWidget *tree_view;
	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;
	
	tree_store = gtk_tree_store_new (4, G_TYPE_BOOLEAN, G_TYPE_STRING,
			G_TYPE_STRING, G_TYPE_BOOLEAN);
	tree_view = WID ("available_languages_treeview");

	gtk_tree_view_set_model (GTK_TREE_VIEW (tree_view),
			GTK_TREE_MODEL (tree_store));

	/* Toggle Button */
	renderer = gtk_cell_renderer_toggle_new ();
	g_object_set (G_OBJECT (renderer), "xalign", 0.0, NULL);
	g_signal_connect (G_OBJECT (renderer), "toggled",
			G_CALLBACK (item_toggled_cb), tree_store);
	column = gtk_tree_view_column_new_with_attributes (_("Show"),
			renderer, "active", 0, "visible", 3, NULL);
	
	gtk_tree_view_column_set_sizing (GTK_TREE_VIEW_COLUMN (column),
			GTK_TREE_VIEW_COLUMN_FIXED);
	gtk_tree_view_column_set_fixed_width (GTK_TREE_VIEW_COLUMN (column), 50);
	gtk_tree_view_append_column (GTK_TREE_VIEW (tree_view), column);

	/* Language tree */
	renderer = gtk_cell_renderer_text_new ();
	g_object_set (renderer, "xalign", 0.0, NULL);
	column = gtk_tree_view_column_new_with_attributes (_("Language"),
			renderer, "text", 1, NULL);
	
	gtk_tree_view_column_set_sort_column_id (column, 1);
	gtk_tree_view_append_column (GTK_TREE_VIEW (tree_view), column);

	gtk_tree_view_set_expander_column (GTK_TREE_VIEW (tree_view), column);	
}

static void
fill_selected_languages_tree (GladeXML * dialog)
{
	GSList *langs;
	GSList *cur_lang;
	GtkListStore *list_store;
	gchar *locale;
	gchar *plocale;
	gchar *split;
	
	list_store = GTK_LIST_STORE (gtk_tree_view_get_model
			(GTK_TREE_VIEW (WID ("languages_treeview"))));
	gtk_list_store_clear (list_store);

	langs = get_selected_languages_list ();

	if (langs == NULL) { /* We get the environment preferences */
		locale = g_strdup (setlocale (LC_MESSAGES, NULL));
		plocale = locale;
		while (locale != NULL) {
			split = strchr (locale, ':');
			if (split) {
				*split = '\0';
				split++;
			}
			langs = g_slist_append (langs, locale);
			locale = split;
		}
		g_free (plocale);

		if (langs) {
			set_selected_languages_list (langs);
		}
	}

	for (cur_lang = langs; cur_lang != NULL; cur_lang = cur_lang->next) {
		GtkTreeIter iter;
		gunichar2 name[ICU_STRING_LONG];
		gchar *name_utf8;
		UErrorCode status;
		
		locale = (char *) cur_lang->data;
		status = U_ZERO_ERROR;

		uloc_getDisplayName (locale, locale, name,
				ICU_STRING_LONG, &status);
		/* Change the first letter to uppercase */
		name[0] = g_unichar_toupper (name[0]);
		name_utf8 = g_utf16_to_utf8 (name, -1, NULL, NULL, NULL);

		gtk_list_store_append (list_store, &iter);

		gtk_list_store_set (list_store, &iter,
				0, name_utf8 ? name_utf8 : locale,
				1, locale, -1);
		g_free (name_utf8);
	}
	if (idx2Select != -1) {
		GtkTreeSelection *selection;
		GtkTreePath *path;

		selection = gtk_tree_view_get_selection ((GTK_TREE_VIEW
					(WID ("languages_treeview"))));
		path = gtk_tree_path_new_from_indices (idx2Select, -1);
		gtk_tree_selection_select_path (selection, path);
		gtk_tree_path_free (path);
		idx2Select = -1;
	}
	enable_disable_move_buttons_cb (dialog);
}

static void
fill_available_languages_tree (GladeXML *dialog)
{
	GtkTreeStore *tree_store;
	gint32 nlocales;
	gint32 i;
	GSList *langs;
	
	tree_store = GTK_TREE_STORE (gtk_tree_view_get_model
			(GTK_TREE_VIEW (WID ("available_languages_treeview"))));
	gtk_tree_store_clear (tree_store);
	
	nlocales = uloc_countAvailable ();
	langs = get_selected_languages_list ();

	for (i = 0; i < nlocales; i++) {
		UErrorCode status;
		const gchar *locale;
		gunichar2 name[ICU_STRING_LONG];
		gchar *name_utf8;
		GtkTreeIter iter;
		GtkTreeIter child_iter;

		status = U_ZERO_ERROR;
		locale = uloc_getAvailable (i);
		if (uloc_getVariant (locale, NULL, 0, &status)) {
			continue; /* We don't handle the variant locales */
		}
		
		status = U_ZERO_ERROR;

		if (!uloc_getCountry (locale, NULL, 0, &status)) {
			
			uloc_getDisplayLanguage (locale, locale, name,
					ICU_STRING_LONG, &status);
			name[0] = g_unichar_toupper (name[0]);
			name_utf8 = g_utf16_to_utf8 (name, -1,
					NULL, NULL, NULL);
			
			gtk_tree_store_append (tree_store, &iter, NULL);
			if (g_slist_find_custom
					(langs, locale, (GCompareFunc) strcmp)) {
				gtk_tree_store_set (tree_store, &iter,
						0, TRUE, 1, name_utf8,
						2, locale, 3, TRUE, -1);
			} else {
				gtk_tree_store_set (tree_store, &iter,
						0, FALSE, 1, name_utf8,
						2, locale, 3, TRUE, -1);
			}
		} else {
			status = U_ZERO_ERROR;
			
			uloc_getDisplayCountry (locale, locale, name,
					ICU_STRING_LONG, &status);
			name_utf8 = g_utf16_to_utf8 (name, -1,
					NULL, NULL, NULL);

			gtk_tree_store_append (tree_store, &child_iter, &iter);
			if (g_slist_find_custom
					(langs, locale, (GCompareFunc) strcmp)) {
				gtk_tree_store_set (tree_store, &child_iter,
						0, TRUE, 1, name_utf8,
						2, locale, 3, TRUE, -1);
			} else {
				gtk_tree_store_set (tree_store, &child_iter,
						0, FALSE, 1, name_utf8,
						2, locale, 3, TRUE, -1);
			}
		}
	}
}

static void
update_warning_box (GladeXML *dialog)
{
	GtkWidget *warning;

	warning = WID ("warning_hbox");
	gtk_widget_show (warning);
}

static void
update_info_box (GladeXML *dialog, const gchar *lang, const gchar *region)
{
	GtkWidget *info;
	gchar main_lang[5];
	gchar region_lang[5];
	UErrorCode status;

	info = WID ("info_hbox");

	if (lang == NULL || region == NULL) {
		gtk_widget_show (info);
		return;
	}
	status = U_ZERO_ERROR;
	uloc_getLanguage (lang, main_lang, 5, &status);
	uloc_getLanguage (region, region_lang, 5, &status);

	if (strcmp (main_lang, region_lang)) {
		gtk_widget_show (info);
	} else {
		gtk_widget_hide (info);
	}

}

static void
languages_changed_cb (GConfClient *client, guint cnxn_id,
		GConfEntry *entry, GladeXML *dialog)
{
	GSList *langs;
	const gchar *region;
	
	if (our_update) {
		our_update = FALSE;
	} else {
		fill_selected_languages_tree (dialog);
		fill_available_languages_tree (dialog);
	}
	langs = gconf_value_get_list (entry->value);
	region = gconf_client_get_string (gconf_client_get_default (),
			GCONF_LOCALIZATION_ROOT_KEY "/region", NULL);
	
	update_warning_box (dialog);
	update_info_box (dialog, langs->data, region);
	
	fill_region_option_menu (dialog);
}

static void
languages_sorted_cb (GtkWidget *widget,
		  GdkDragContext *context,
		  GladeXML *dialog)
{
	GSList *old_langs;
	GSList *new_langs;
	GtkWidget *tree_view;
	GtkTreeModel *model;
	GtkTreeIter iter;
	gboolean changed, more_rows;
	gchar *code;
	guint len, i;
	const gchar *region;

	tree_view = WID ("languages_treeview");
	model = gtk_tree_view_get_model (GTK_TREE_VIEW (tree_view));

	old_langs = get_selected_languages_list ();
	new_langs = NULL;
	len = g_slist_length (old_langs);
	changed = FALSE;
	if (gtk_tree_model_get_iter_first (model, &iter)) {
		more_rows = TRUE;
		for (i = 0; i < len && more_rows; i++) {
			gtk_tree_model_get (model, &iter, 1, &code, -1);
			if (strcmp (code, (const gchar *)g_slist_nth_data (old_langs, i))) {
				changed = TRUE;
			}
			new_langs = g_slist_append (new_langs, code);
			more_rows = gtk_tree_model_iter_next (model, &iter);
		}
		if (changed) {
			our_update = TRUE;
			set_selected_languages_list (new_langs);
			enable_disable_move_buttons_cb (dialog);
			
			region = gconf_client_get_string (gconf_client_get_default (),
					GCONF_LOCALIZATION_ROOT_KEY "/region", NULL);

			update_warning_box (dialog);
			update_info_box (dialog, new_langs->data, region);
		}
	}
}

static void
change_languages_cb (GtkWidget *button, GladeXML *dialog)
{
	GtkWidget *chooser;
	GtkWidget *main_capplet;
	
	chooser = WID ("available_languages_dialog");
	main_capplet = WID ("localization_dialog");

	gtk_window_set_transient_for (GTK_WINDOW (chooser),
			GTK_WINDOW (main_capplet));
	if (gtk_dialog_run (GTK_DIALOG (chooser)) == GTK_RESPONSE_OK) {
		GSList *langs;
		GSList *cur_lang;
		GtkWidget *tree_view;
		GtkTreeModel *model;
		GtkTreeIter iter;
		GtkTreeIter child_iter;
		gboolean selected, changed;
		gchar *code;

		tree_view = WID ("available_languages_treeview");
		model = gtk_tree_view_get_model (GTK_TREE_VIEW (tree_view));

		changed = FALSE;
		if (gtk_tree_model_get_iter_first (model, &iter)) {
			langs = get_selected_languages_list ();
			do {
				gtk_tree_model_get (model, &iter,
						0, &selected, 2, &code, -1);
				cur_lang = g_slist_find_custom (langs,
						code, (GCompareFunc) strcmp);
				
				if (cur_lang && !selected) {
					langs = g_slist_delete_link
						(langs, cur_lang);
					changed = TRUE;
				} else if (!cur_lang && selected) {
					langs = g_slist_append
						(langs, code);
					changed = TRUE;
				} else {
					g_free (code);
				}
				if (gtk_tree_model_iter_children
						(model, &child_iter, &iter)) {
					do {
						gtk_tree_model_get (model,
								&child_iter,
								0, &selected,
								2, &code, -1);
						cur_lang = g_slist_find_custom
							(langs, code,
							 (GCompareFunc) strcmp);
				
						if (cur_lang && !selected) {
							langs = g_slist_delete_link
								(langs, cur_lang);
							changed = TRUE;
						} else if (!cur_lang && selected) {
							langs = g_slist_append
								(langs, code);
							changed = TRUE;
						} else {
							g_free (code);
						}
					} while (gtk_tree_model_iter_next (model, &child_iter));
				}
			} while (gtk_tree_model_iter_next (model, &iter));
		}
		if (changed) {
			set_selected_languages_list (langs);
		}
		gtk_widget_hide (chooser);
	} else {
		gtk_widget_hide (chooser);
	}
}

static void
language_move (GladeXML * dialog, int offset)
{
	GtkTreeSelection *selection;
	GtkTreeIter siter;
	GtkTreeModel *model;
	GSList *langs;
	GtkTreePath *path;

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW
			(WID ("languages_treeview")));
	
	if (gtk_tree_selection_get_selected (selection, &model, &siter)) {
		path = gtk_tree_model_get_path (model, &siter);
		if (path != NULL) {
			int *indices;
			char *id;
			GSList *node;
			
			id = NULL;

			langs = get_selected_languages_list ();
			indices = gtk_tree_path_get_indices (path);
			node = g_slist_nth (langs, indices[0]);

			langs = g_slist_remove_link (langs, node);

			id = (char *) node->data;
			g_slist_free_1 (node);

			langs = g_slist_insert (langs, id, indices[0] + offset);
			idx2Select = indices[0] + offset;

			set_selected_languages_list (langs);
			gtk_tree_path_free (path);
		}
	}
}

static void
language_up_cb (GtkWidget *button, GladeXML *dialog)
{
	language_move (dialog, -1);
}

static void
language_down_cb (GtkWidget *button, GladeXML *dialog)
{
	language_move (dialog, +1);
}

static void
menu_item_activated_cb (GtkMenuItem *menu_item, GladeXML *dialog)
{
	const gchar *locale;

	locale = g_object_get_data (G_OBJECT (menu_item), "itemId");
	gconf_client_set_string (gconf_client_get_default (),
			GCONF_LOCALIZATION_ROOT_KEY "/region",
			locale, NULL);
}

static void
submenu_selected_cb (GtkMenu *menu, GladeXML *dialog)
{
	GtkWidget *menu_item;
	GtkWidget *optionmenu;
	const gchar *locale;

	optionmenu = WID ("region_optionmenu");
	menu_item = gtk_menu_get_active (menu);
	locale = g_object_get_data (G_OBJECT (menu_item), "itemId");
	gconf_client_set_string (gconf_client_get_default (),
			GCONF_LOCALIZATION_ROOT_KEY "/region",
			locale, NULL);
	fill_region_option_menu (dialog);
//	gtk_option_menu_set_history (GTK_OPTION_MENU (optionmenu), 0);
}

static void
update_region_examples (GladeXML *dialog, const gchar *region)
{
	UErrorCode status;
	gunichar2 str[ICU_STRING_LONG];
	gchar *str_utf8;
	UCalendar *cal;
	UDate date;
	GtkWidget *label;
	UDateFormat* dfmt;
	UNumberFormat* nf;
	double number;

	status = U_ZERO_ERROR;
	
	cal = ucal_open (NULL, -1, region, UCAL_TRADITIONAL, &status);
	ucal_setDateTime (cal, 2004, UCAL_JANUARY, 2, 12, 34, 0, &status);
	date = ucal_getMillis (cal, &status);

	/* Full date label */
	status = U_ZERO_ERROR;
	dfmt = udat_open(UDAT_NONE, UDAT_FULL, region,
			NULL, -1, NULL, -1, &status);
	udat_format(dfmt, date, str, ICU_STRING_LONG, NULL, &status);
	str_utf8 = g_utf16_to_utf8 (str, -1, NULL, NULL, NULL);
	label = WID ("full_date_label");
	gtk_label_set_text (GTK_LABEL(label), str_utf8);
	g_free (str_utf8);

	/* Medium date label */
	status = U_ZERO_ERROR;
	dfmt = udat_open(UDAT_NONE, UDAT_MEDIUM, region,
			NULL, -1, NULL, -1, &status);
	udat_format(dfmt, date, str, ICU_STRING_LONG, NULL, &status);
	str_utf8 = g_utf16_to_utf8 (str, -1, NULL, NULL, NULL);
	label = WID ("medium_date_label");
	gtk_label_set_text (GTK_LABEL(label), str_utf8);
	g_free (str_utf8);

	/* Short date label */
	status = U_ZERO_ERROR;
	dfmt = udat_open(UDAT_NONE, UDAT_SHORT, region,
			NULL, -1, NULL, -1, &status);
	udat_format(dfmt, date, str, ICU_STRING_LONG, NULL, &status);
	str_utf8 = g_utf16_to_utf8 (str, -1, NULL, NULL, NULL);
	label = WID ("short_date_label");
	gtk_label_set_text (GTK_LABEL(label), str_utf8);
	g_free (str_utf8);

	/* Short time AM label */
	status = U_ZERO_ERROR;
	dfmt = udat_open(UDAT_SHORT, UDAT_NONE, region,
			NULL, -1, NULL, -1, &status);
	udat_format(dfmt, date, str, ICU_STRING_LONG, NULL, &status);
	str_utf8 = g_utf16_to_utf8 (str, -1, NULL, NULL, NULL);
	label = WID ("am_time_label");
	gtk_label_set_text (GTK_LABEL(label), str_utf8);
	g_free (str_utf8);

	/* Short time AM label */
	status = U_ZERO_ERROR;
	ucal_setDateTime (cal, 2004, UCAL_JANUARY, 2, 4, 56, 0, &status);
	date = ucal_getMillis (cal, &status);
	dfmt = udat_open(UDAT_SHORT, UDAT_NONE, region,
			NULL, -1, NULL, -1, &status);
	udat_format(dfmt, date, str, ICU_STRING_LONG, NULL, &status);
	str_utf8 = g_utf16_to_utf8 (str, -1, NULL, NULL, NULL);
	label = WID ("pm_time_label");
	gtk_label_set_text (GTK_LABEL(label), str_utf8);
	g_free (str_utf8);

	/* Currency label */
	number = 1234.56;
	status = U_ZERO_ERROR;
	nf = unum_open(UNUM_CURRENCY, NULL, -1, region, NULL, &status);
	unum_formatDouble(nf, number, str, ICU_STRING_LONG, NULL, &status);
	str_utf8 = g_utf16_to_utf8 (str, -1, NULL, NULL, NULL);
	label = WID ("currency_label");
	gtk_label_set_text (GTK_LABEL(label), str_utf8);
	g_free (str_utf8);
}

static void
fill_region_option_menu (GladeXML *dialog)
{
	GSList *langs;
	gchar main_lang[5];
	UErrorCode status;
	const gchar *selected_region;
	GtkWidget *menu;
	GtkWidget *submenu;
	GtkWidget *menu_item;
	gboolean show_all;
	gint32 nlocales;
	gint32 i;
	gboolean has_childs;
	gboolean selected_region_added;
	GtkWidget *selected_region_mitem;
	GList *pitem_node;
	gint position;
		
	langs = get_selected_languages_list ();
	status = U_ZERO_ERROR;

	uloc_getLanguage ((gchar *)langs->data, main_lang, 5, &status);
	selected_region = gconf_client_get_string (gconf_client_get_default (),
			GCONF_LOCALIZATION_ROOT_KEY "/region", NULL);

	if (selected_region == NULL) { /* We get the environment preferences */
		selected_region = setlocale (LC_TIME, NULL);
		if (selected_region) {
			gconf_client_set_string (gconf_client_get_default (),
					GCONF_LOCALIZATION_ROOT_KEY "/region",
					selected_region, NULL);
			return; /* The GConf event will update the menu */
		} else {
			/* FIXME: What default should we use? */
			selected_region = (gchar *) langs->data;
		}
	}
	selected_region_added = FALSE;
	selected_region_mitem = NULL;

	show_all = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON
			(WID ("show_all_checkbutton")));

	/* FIXME: Is there a way to reuse the old menu? */
	menu = gtk_menu_new ();
	submenu = NULL;
	menu_item = NULL;

	nlocales = uloc_countAvailable ();
	has_childs = FALSE;

	for (i = 0; i < nlocales; i++) {
		const gchar *locale;
		gunichar2 name[ICU_STRING_LONG];
		gchar *name_utf8;
		gint position;

		locale = uloc_getAvailable (i);

		if (uloc_getVariant (locale, NULL, 0, &status)) {
			continue; /* We don't handle the variant locales */
		}
		
		status = U_ZERO_ERROR;

		if (!uloc_getCountry (locale, NULL, 0, &status)) {
			/* We remove the last country submenu
			 * if it does not have childs
			 */
			if (!has_childs && menu_item) {
				gtk_menu_item_remove_submenu
					(GTK_MENU_ITEM (menu_item));
				g_signal_connect
					(menu_item, "activate",
					 G_CALLBACK (menu_item_activated_cb),
					 dialog);
			}

			/* If it's the main language selected
			 * we just add their childs to the root menu
			 */
			if (strcmp (locale, main_lang) == 0 &&
					strcmp ((gchar *)langs->data,
						main_lang)) {
				has_childs = TRUE;
				submenu = menu;
				continue;
			} else if (!show_all) {
				has_childs = TRUE;
				submenu = NULL;
				continue;
			}

			status = U_ZERO_ERROR;
			uloc_getDisplayLanguage (locale, (gchar *)langs->data,
					name, ICU_STRING_LONG, &status);
			name[0] = g_unichar_toupper (name[0]);
			name_utf8 = g_utf16_to_utf8 (name, -1,
					NULL, NULL, NULL);
			
			if (selected_region_added) {
				position = 2;
			} else {
				position = 0;
			}
			pitem_node = g_list_nth (GTK_MENU_SHELL (menu)->children,
					position);

			for (position = selected_region_added ? 2 : 0;
					pitem_node != NULL;
					position++,
					pitem_node = pitem_node->next) {
				GtkWidget *label;
				const gchar *plocale;
				const gchar *txt;

				menu_item = GTK_WIDGET (pitem_node->data);

				plocale = g_object_get_data
					(G_OBJECT (menu_item), "itemId");

				status = U_ZERO_ERROR;
				if (!uloc_getCountry (plocale, NULL, 0, &status)) {
					label = GTK_BIN (menu_item)->child;
					txt = gtk_label_get_text (GTK_LABEL (label));
					
					/* FIXME: We must call setlocale */
					if (g_utf8_collate (txt, name_utf8) > 0) {
						break;
					}
				} else {
					/* We jump the main_locale regions */
					continue;
				}
			}

			menu_item = gtk_menu_item_new_with_label (name_utf8);
			g_object_set_data_full (G_OBJECT (menu_item), "itemId",
					g_strdup (locale),
					(GDestroyNotify) g_free);
	
			gtk_menu_shell_insert (GTK_MENU_SHELL (menu),
					GTK_WIDGET (menu_item), position);
			submenu = gtk_menu_new ();
			g_signal_connect (submenu, "selection-done",
					G_CALLBACK (submenu_selected_cb), dialog);
			gtk_menu_item_set_submenu (GTK_MENU_ITEM (menu_item), submenu);
			has_childs = FALSE;
		} else {
			status = U_ZERO_ERROR;
			has_childs = TRUE;

			uloc_getDisplayCountry (locale, (gchar *)langs->data,
					name, ICU_STRING_LONG, &status);
			name_utf8 = g_utf16_to_utf8 (name, -1,
					NULL, NULL, NULL);

			if (!strcmp (locale, selected_region) && menu != submenu) {
				/* This is the selected region, we must
				 * add it also to the root menu
				 */
				gunichar2 language[ICU_STRING_LONG];
				gchar *language_utf8;
				gchar *label;
				
				uloc_getDisplayLanguage (selected_region,
						(gchar *)langs->data,
						language, ICU_STRING_LONG,
						&status);
				language_utf8 = g_utf16_to_utf8 (language,
						-1, NULL, NULL, NULL);

				label = g_strdup_printf ("%s (%s)",
						name_utf8,
						language_utf8);
				
				g_free (language_utf8);
				
				menu_item = gtk_menu_item_new_with_label (label);
				selected_region_mitem = menu_item;
				g_free (label);
				g_object_set_data_full (G_OBJECT (menu_item),
						"itemId",
						g_strdup (selected_region),
						(GDestroyNotify) g_free);
	
				gtk_menu_shell_insert (GTK_MENU_SHELL (menu),
						GTK_WIDGET (menu_item), 0);

				menu_item = gtk_separator_menu_item_new ();
				gtk_menu_shell_insert (GTK_MENU_SHELL (menu),
						GTK_WIDGET (menu_item), 1);

				selected_region_added = TRUE;
			}

			if (!(show_all || menu == submenu)) {
				continue;
			}
			
			if (menu == submenu && selected_region_added) {
				position = 2;
			} else {
				position = 0;
			}

			pitem_node = g_list_nth (GTK_MENU_SHELL (submenu)->children,
					position);

			for ( ; pitem_node != NULL;
					position++,
					pitem_node = pitem_node->next) {
				GtkWidget *label;
				const gchar *plocale;
				const gchar *txt;
				
				menu_item = GTK_WIDGET (pitem_node->data);
				plocale = g_object_get_data
					(G_OBJECT (menu_item), "itemId");

				status = U_ZERO_ERROR;
				if (!uloc_getCountry (plocale, NULL, 0, &status)) {
					/* Here starts the other regions != main_language */
					break;
				}
				label = GTK_BIN (menu_item)->child;
				txt = gtk_label_get_text (GTK_LABEL (label));
				/* FIXME: We must call setlocale */
				if (g_utf8_collate (txt, name_utf8) > 0) {
					break;
				}
			}
	
			menu_item = gtk_menu_item_new_with_label (name_utf8);
			if (menu == submenu) {
				if (!strcmp (selected_region, locale)) {
					selected_region_mitem = menu_item;
				}
				g_signal_connect (menu_item, "activate",
					 G_CALLBACK (menu_item_activated_cb),
					 dialog);
			}
			g_object_set_data_full (G_OBJECT (menu_item), "itemId",
					g_strdup (locale),
					(GDestroyNotify) g_free);
			gtk_menu_shell_insert (GTK_MENU_SHELL (submenu),
					GTK_WIDGET (menu_item), position);
		}
	}

	if (selected_region_mitem) {
		pitem_node = GTK_MENU_SHELL (menu)->children;
		position = g_list_index (pitem_node, selected_region_mitem);
		if (position == -1) {
			/* FIXME: We have a problem.... */
			position = 0;
		}
	} else {
		/* FIXME: The user has a non valid region */
		position = 0;
	}
	
	/* We remove the old menu */
	gtk_option_menu_remove_menu (GTK_OPTION_MENU (WID ("region_optionmenu")));
	/* Add the new menu */
	gtk_option_menu_set_menu (GTK_OPTION_MENU (WID ("region_optionmenu")),
				  GTK_WIDGET (menu));
	gtk_option_menu_set_history (GTK_OPTION_MENU
			(WID ("region_optionmenu")), position);
	gtk_widget_show_all (menu);

	update_region_examples (dialog, selected_region);
}

static void
show_all_regions_cb (GConfClient *client, guint cnxn_id,
		GConfEntry *entry, GladeXML *dialog)
{
	/* We just refresh the region_optionmenu */
	fill_region_option_menu (dialog);
}

static void
region_changed_cb (GConfClient *client, guint cnxn_id,
		GConfEntry *entry, GladeXML *dialog)
{
	GSList *langs;
	gchar *selected_region;

	langs = get_selected_languages_list ();
	selected_region = g_strdup (gconf_value_get_string (entry->value));

	update_info_box (dialog, langs->data, selected_region);

	g_free (selected_region);

	/* Updates the optionmenu */
	fill_region_option_menu (dialog);

}

static void
dialog_response (GtkWidget *widget,
		 gint       response_id,
		 GConfChangeSet *changeset)
{
	if (response_id == GTK_RESPONSE_HELP) {
		/* FIXME: What should we add here? */
/*		capplet_help (GTK_WINDOW (widget),
			"user-guide.xml",
			"goscustperiph-2");*/
	} else {
		gtk_main_quit ();
	}
}

static void
setup_dialog (GladeXML       *dialog,
	      GConfChangeSet *changeset)
{
	GObject *peditor;
	GnomeProgram *program;
	GSList *langs;
	const gchar *region;

	/* load all the images */
	program = gnome_program_get ();

	capplet_init_stock_icons ();

	/* Language Tab */

	/* Flag to ignore our own updates when sorting the language list */
	our_update = FALSE;

	prepare_selected_languages_tree (dialog);
	prepare_available_languages_tree (dialog);
	
	fill_selected_languages_tree (dialog);
	fill_available_languages_tree (dialog);

	gconf_client_notify_add (gconf_client_get_default (), 
			GCONF_LOCALIZATION_ROOT_KEY "/languages",
			(GConfClientNotifyFunc) languages_changed_cb,
			dialog, NULL, NULL);

	g_signal_connect (G_OBJECT (WID ("languages_treeview")), "drag_end",
			G_CALLBACK (languages_sorted_cb), dialog);
	g_signal_connect (G_OBJECT (WID ("change_languages_button")), "clicked",
			G_CALLBACK (change_languages_cb), dialog);
	g_signal_connect (G_OBJECT (WID ("language_up_button")), "clicked",
			G_CALLBACK (language_up_cb), dialog);
	g_signal_connect (G_OBJECT (WID ("language_down_button")), "clicked",
			G_CALLBACK (language_down_cb), dialog);

	/* Formats Tab */
	fill_region_option_menu (dialog);

	peditor = gconf_peditor_new_boolean
		(changeset, GCONF_LOCALIZATION_ROOT_KEY "/show_all_regions",
		 WID ("show_all_checkbutton"), NULL);

	gconf_client_notify_add (gconf_client_get_default (),
			GCONF_LOCALIZATION_ROOT_KEY "/show_all_regions",
			(GConfClientNotifyFunc) show_all_regions_cb,
			dialog, NULL, NULL);

	gconf_client_notify_add (gconf_client_get_default (),
			GCONF_LOCALIZATION_ROOT_KEY "/region",
			(GConfClientNotifyFunc) region_changed_cb,
			dialog, NULL, NULL);

	langs = get_selected_languages_list();
	region = gconf_client_get_string (gconf_client_get_default (),
			GCONF_LOCALIZATION_ROOT_KEY "/region", NULL);

	update_info_box (dialog, langs->data, region);

	/* Dialog action buttons */
	g_signal_connect (G_OBJECT (WID ("localization_dialog")), "response",
			G_CALLBACK (dialog_response), changeset);
		
}

int
main (int argc, char **argv) 
{
	GConfClient    *client;
	GConfChangeSet *changeset;
	GladeXML       *dialog;

	bindtextdomain (GETTEXT_PACKAGE, GNOMELOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	gnome_program_init ("gnome-localization-properties", VERSION, LIBGNOMEUI_MODULE, argc, argv,
			    GNOME_PARAM_APP_DATADIR, GNOMECC_DATA_DIR,
			    NULL);

	activate_settings_daemon ();
	
	client = gconf_client_get_default ();
	gconf_client_add_dir (client, GCONF_LOCALIZATION_ROOT_KEY, GCONF_CLIENT_PRELOAD_ONELEVEL, NULL);

	changeset = NULL;
	dialog = create_dialog ();
	setup_dialog (dialog, changeset);
	capplet_set_icon (WID ("localization_dialog"), "localization-capplet.png");
	gtk_widget_show (WID ("localization_dialog"));
	gtk_main ();

	return 0;
}
