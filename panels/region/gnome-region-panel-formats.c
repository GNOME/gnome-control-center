/*
 * Copyright (C) 2011 Rodrigo Moya
 *
 * Written by: Rodrigo Moya <rodrigo@gnome.org>
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

#include <glib/gi18n-lib.h>
#include <locale.h>
#include <langinfo.h>
#include <stdlib.h>
#include "cc-common-language.h"
#include "cc-language-chooser.h"
#include "gdm-languages.h"
#include "gnome-region-panel-formats.h"

static void
display_date (GtkLabel *label, GDateTime *dt, const gchar *format)
{
	gchar *s;

	s = g_date_time_format (dt, format);
	s = g_strstrip (s);
	gtk_label_set_text (label, s);
	g_free (s);
}

static void
select_region (GtkTreeView *treeview, const gchar *lang)
{
        GtkTreeModel *model;
        GtkTreeSelection *selection;
        GtkTreeIter iter;
        GtkTreePath *path;
        gboolean cont;

        model = gtk_tree_view_get_model (treeview);
        selection = gtk_tree_view_get_selection (treeview);
        cont = gtk_tree_model_get_iter_first (model, &iter);
        while (cont) {
                gchar *locale;

                gtk_tree_model_get (model, &iter, 0, &locale, -1);
                if (g_strcmp0 (locale, lang) == 0) {
                        gtk_tree_selection_select_iter (selection, &iter);
                        path = gtk_tree_model_get_path (model, &iter);
                        gtk_tree_view_scroll_to_cell (treeview, path, NULL, FALSE, 0.0, 0.0);
                        gtk_tree_path_free (path);
                        g_free (locale);
                        break;
                }
                g_free (locale);

                cont = gtk_tree_model_iter_next (model, &iter);
        }
}

static void
update_examples_cb (GtkTreeSelection *selection, gpointer user_data)
{
	GtkBuilder *builder = GTK_BUILDER (user_data);
        GtkTreeModel *model;
        GtkTreeIter iter;
        gchar *active_id;
	gchar *locale;
	GDateTime *dt;
	gchar *s;
	struct lconv *num_info;
	const char *fmt;

        if (!gtk_tree_selection_get_selected (selection, &model, &iter)) {
                return;
        }
        gtk_tree_model_get (model, &iter, 0, &active_id, -1);

	locale = g_strdup (setlocale (LC_TIME, NULL));
        setlocale (LC_TIME, active_id);

	dt = g_date_time_new_now_local ();

	/* Display dates */
	display_date (GTK_LABEL (gtk_builder_get_object (builder, "full_date_format")), dt, "%A %e %B %Y");
	display_date (GTK_LABEL (gtk_builder_get_object (builder, "full_day_format")), dt, "%e %B %Y");
	display_date (GTK_LABEL (gtk_builder_get_object (builder, "short_day_format")), dt, "%e %b %Y");
	display_date (GTK_LABEL (gtk_builder_get_object (builder, "shortest_day_format")), dt, "%x");

	/* Display times */
	display_date (GTK_LABEL (gtk_builder_get_object (builder, "full_time_format")), dt, "%r %Z");
	display_date (GTK_LABEL (gtk_builder_get_object (builder, "short_time_format")), dt, "%X");

	setlocale (LC_TIME, locale);
	g_free (locale);

	/* Display numbers */
	locale = g_strdup (setlocale (LC_NUMERIC, NULL));
	setlocale (LC_NUMERIC, active_id);

	s = g_strdup_printf ("%'.2f", 123456789.00);
	gtk_label_set_text (GTK_LABEL (gtk_builder_get_object (builder, "numbers_format")), s);
	g_free (s);

	setlocale (LC_NUMERIC, locale);
	g_free (locale);

	/* Display currency */
	locale = g_strdup (setlocale (LC_MONETARY, NULL));
	setlocale (LC_MONETARY, active_id);

	num_info = localeconv ();
	if (num_info != NULL) {
		gtk_label_set_text (GTK_LABEL (gtk_builder_get_object (builder, "currency_format")), num_info->currency_symbol);
	}

	setlocale (LC_MONETARY, locale);
	g_free (locale);

	/* Display measurement */
#ifdef LC_MEASUREMENT
	locale = g_strdup (setlocale (LC_MEASUREMENT, NULL));
	setlocale (LC_MEASUREMENT, active_id);

	fmt = nl_langinfo (_NL_MEASUREMENT_MEASUREMENT);
	if (fmt && *fmt == 2)
		gtk_label_set_text (GTK_LABEL (gtk_builder_get_object (builder, "measurement_format")), _("Imperial"));
	else
		gtk_label_set_text (GTK_LABEL (gtk_builder_get_object (builder, "measurement_format")), _("Metric"));

	setlocale (LC_MEASUREMENT, locale);
	g_free (locale);
#endif
        g_free (active_id);
}


static void
update_settings_cb (GtkTreeSelection *selection, gpointer user_data)
{
        GtkBuilder *builder = GTK_BUILDER (user_data);
        GtkTreeModel *model;
        GtkTreeIter iter;
        gchar *active_id;
        GtkWidget *treeview;
        GSettings *locale_settings;
        gchar *current_setting;

        if (!gtk_tree_selection_get_selected (selection, &model, &iter)) {
                return;
        }
        gtk_tree_model_get (model, &iter, 0, &active_id, -1);

        treeview = GTK_WIDGET (gtk_builder_get_object (builder, "region_selector"));

        locale_settings = g_object_get_data (G_OBJECT (treeview), "settings");
        current_setting = g_settings_get_string (locale_settings, "region");

        if (g_strcmp0 (active_id, current_setting) != 0) {
                g_settings_set_string (locale_settings, "region", active_id);
        }

        g_free (current_setting);
        g_free (active_id);
}

static void
setting_changed_cb (GSettings *locale_settings, gchar *key, GtkTreeView *treeview)
{
        gchar *current_setting;

        current_setting = g_settings_get_string (locale_settings, "region");
        select_region (treeview, current_setting);
        g_free (current_setting);
}

static gint
sort_regions (GtkTreeModel *model,
              GtkTreeIter  *a,
              GtkTreeIter  *b,
              gpointer      data)
{
        gchar *la, *lb;
        gint result;

        gtk_tree_model_get (model, a, 1, &la, -1);
        gtk_tree_model_get (model, b, 1, &lb, -1);

        result = strcmp (la, lb);

        g_free (la);
        g_free (lb);

        return result;
}

static void
populate_regions (GtkBuilder *builder, const gchar *current_lang)
{
        gchar *current_region;
        GSettings *locale_settings;
        GHashTable *ht;
        GHashTableIter htiter;
        GtkTreeModel *model;
        gchar *name, *language;
        GtkWidget *treeview;
        GtkTreeIter iter;
        GtkTreeSelection *selection;

        treeview = GTK_WIDGET (gtk_builder_get_object (builder, "region_selector"));
        /* don't update the setting just because the list is repopulated */
        selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview));
        g_signal_handlers_block_by_func (selection, update_settings_cb, builder);

        model = gtk_tree_view_get_model (GTK_TREE_VIEW (treeview));
        locale_settings = g_object_get_data (G_OBJECT (treeview), "settings");

        ht = cc_common_language_get_initial_regions (current_lang);

        current_region = g_settings_get_string (locale_settings, "region");
        if (!current_region || !current_region[0]) {
                current_region = g_strdup (current_lang);
        }
        else if (!g_hash_table_lookup (ht, current_region)) {
                name = gdm_get_region_from_name (current_region, NULL);
                g_hash_table_insert (ht, g_strdup (current_region), name);
        }

        gtk_list_store_clear (GTK_LIST_STORE (model));

        g_hash_table_iter_init (&htiter, ht);
        while (g_hash_table_iter_next (&htiter, (gpointer *)&name, (gpointer *)&language)) {
                gtk_list_store_append (GTK_LIST_STORE (model), &iter);
                gtk_list_store_set (GTK_LIST_STORE (model), &iter, 0, name, 1, language, -1);
        }
        g_hash_table_unref (ht);

        select_region (GTK_TREE_VIEW (treeview), current_region);

        g_free (current_region);

        g_signal_handlers_unblock_by_func (selection, update_settings_cb, builder);
}

static void
region_response (GtkDialog *dialog,
                 gint       response_id,
                 GtkWidget *treeview)
{
        gchar *lang;
        GtkTreeModel *model;
        GtkTreeSelection *selection;
        GtkTreeIter iter;

        gtk_widget_hide (GTK_WIDGET (dialog));

        if (response_id != GTK_RESPONSE_OK) {
                return;
        }

        lang = cc_language_chooser_get_language (GTK_WIDGET (dialog));

        if (lang == NULL) {
                return;
        }
        model = gtk_tree_view_get_model (GTK_TREE_VIEW (treeview));
        selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview));

        if (cc_common_language_get_iter_for_region (model, lang, &iter)) {
                gtk_tree_selection_select_iter (selection, &iter);
        }

        gtk_widget_grab_focus (treeview);

        g_free (lang);
}

static void
add_region (GtkWidget *button, GtkWidget *treeview)
{
        GtkWidget *toplevel;
        GtkWidget *chooser;

        toplevel = gtk_widget_get_toplevel (button);
        chooser = g_object_get_data (G_OBJECT (button), "chooser");
        if (chooser == NULL) {
                chooser = cc_language_chooser_new (toplevel, TRUE);

                g_signal_connect (chooser, "response",
                                  G_CALLBACK (region_response), treeview);
                g_signal_connect (chooser, "delete-event",
                                  G_CALLBACK (gtk_widget_hide_on_delete), NULL);

                g_object_set_data_full (G_OBJECT (button), "chooser",
                                        chooser, (GDestroyNotify)gtk_widget_destroy);
        }
        else {
                cc_language_chooser_clear_filter (chooser);
        }

        gdk_window_set_cursor (gtk_widget_get_window (toplevel), NULL);
        gtk_window_present (GTK_WINDOW (chooser));
}

void
setup_formats (GtkBuilder *builder)
{
	GtkWidget *treeview;
	gchar *current_lang;
	GtkTreeModel *model;
        GtkCellRenderer *cell;
        GtkTreeViewColumn *column;
        GtkWidget *widget;
        GtkStyleContext *context;
        GSettings *locale_settings;
        GtkTreeSelection *selection;

	locale_settings = g_settings_new ("org.gnome.system.locale");

        /* Setup junction between toolbar and treeview */
        widget = (GtkWidget *)gtk_builder_get_object (builder, "region-swindow");
        context = gtk_widget_get_style_context (widget);
        gtk_style_context_set_junction_sides (context, GTK_JUNCTION_BOTTOM);
        widget = (GtkWidget *)gtk_builder_get_object (builder, "region-toolbar");
        context = gtk_widget_get_style_context (widget);
        gtk_style_context_set_junction_sides (context, GTK_JUNCTION_TOP);

	/* Setup formats selector */
	treeview = GTK_WIDGET (gtk_builder_get_object (builder, "region_selector"));
        cell = gtk_cell_renderer_text_new ();
        g_object_set (cell,
                      "width-chars", 40,
                      "ellipsize", PANGO_ELLIPSIZE_END,
                      NULL);
        column = gtk_tree_view_column_new_with_attributes (NULL, cell, "text", 1, NULL);
        gtk_tree_view_append_column (GTK_TREE_VIEW (treeview), column);

        model = (GtkTreeModel*)gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_STRING);
        gtk_tree_sortable_set_default_sort_func (GTK_TREE_SORTABLE (model),
                                                 sort_regions, NULL, NULL);
        gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (model),
                                              GTK_TREE_SORTABLE_DEFAULT_SORT_COLUMN_ID,
                                              GTK_SORT_ASCENDING);
        gtk_tree_view_set_model (GTK_TREE_VIEW (treeview), model);

        g_object_set_data_full (G_OBJECT (treeview), "settings", locale_settings, g_object_unref);

        selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview));
        g_signal_connect (selection, "changed",
                          G_CALLBACK (update_settings_cb), builder);
        g_signal_connect (selection, "changed",
                          G_CALLBACK (update_examples_cb), builder);

        /* Connect buttons */
        widget = (GtkWidget *)gtk_builder_get_object (builder, "region_add");
        g_signal_connect (widget, "clicked",
                          G_CALLBACK (add_region), treeview);

	current_lang = cc_common_language_get_current_language ();
        populate_regions (builder, current_lang);
	g_free (current_lang);

        g_signal_connect (locale_settings, "changed::region",
                          G_CALLBACK (setting_changed_cb), treeview);
}

void
formats_update_language (GtkBuilder  *builder,
                         const gchar *language)
{
        populate_regions (builder, language);
}

