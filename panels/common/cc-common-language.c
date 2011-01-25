/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright 2009-2010  Red Hat, Inc,
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * Written by: Matthias Clasen <mclasen@redhat.com>
 */

#include "config.h"

#include <stdlib.h>
#include <locale.h>

#include <glib.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include <fontconfig/fontconfig.h>

#include "cc-common-language.h"

#include "gdm-languages.h"

static gint
cc_common_language_sort_languages (GtkTreeModel *model,
				   GtkTreeIter  *a,
				   GtkTreeIter  *b,
				   gpointer      data)
{
        char *ca, *cb;
        char *la, *lb;
        gboolean sa, ula;
        gboolean sb, ulb;
        gint result;

	gtk_tree_model_get (model, a,
			    LOCALE_COL, &ca,
			    DISPLAY_LOCALE_COL, &la,
			    SEPARATOR_COL, &sa,
			    USER_LANGUAGE, &ula,
			    -1);
	gtk_tree_model_get (model, b,
			    LOCALE_COL, &cb,
			    DISPLAY_LOCALE_COL, &lb,
			    SEPARATOR_COL, &sb,
			    USER_LANGUAGE, &ulb,
			    -1);

	/* Sort before and after separator first */
	if (sa && sb)
		return 0;
	if (sa)
		return ulb ? 1 : -1;
	if (sb)
		return ula ? -1 : 1;

	/* Sort user-languages first */
	if (ula != ulb) {
		if (ula)
			return -1;
		else
			return 1;
	}

        if (!ca)
                result = 1;
        else if (!cb)
                result = -1;
        else
                result = strcmp (la, lb);

        g_free (ca);
        g_free (cb);
        g_free (la);
        g_free (lb);

        return result;
}

gboolean
cc_common_language_get_iter_for_language (GtkTreeModel *model,
					  const gchar  *lang,
					  GtkTreeIter  *iter)
{
        char *l;
        char *name;
        char *language;

        gtk_tree_model_get_iter_first (model, iter);
        do {
                gtk_tree_model_get (model, iter, LOCALE_COL, &l, -1);
                if (g_strcmp0 (l, lang) == 0) {
                        g_free (l);
                        return TRUE;
                }
                g_free (l);
        } while (gtk_tree_model_iter_next (model, iter));

        name = gdm_normalize_language_name (lang);
        if (name != NULL) {
                language = gdm_get_language_from_name (name, NULL);

                gtk_list_store_append (GTK_LIST_STORE (model), iter);
                gtk_list_store_set (GTK_LIST_STORE (model), iter, LOCALE_COL, name, DISPLAY_LOCALE_COL, language, -1);
                g_free (name);
                g_free (language);
                return TRUE;
        }

        return FALSE;
}

gboolean
cc_common_language_has_font (const gchar *locale)
{
        const FcCharSet *charset;
        FcPattern       *pattern;
        FcObjectSet     *object_set;
        FcFontSet       *font_set;
        gchar           *language_code;
        gboolean         is_displayable;

        is_displayable = FALSE;
        pattern = NULL;
        object_set = NULL;
        font_set = NULL;

        if (!gdm_parse_language_name (locale, &language_code, NULL, NULL, NULL))
                return FALSE;

        charset = FcLangGetCharSet ((FcChar8 *) language_code);
        if (!charset) {
                /* fontconfig does not know about this language */
                is_displayable = TRUE;
        }
        else {
                /* see if any fonts support rendering it */
                pattern = FcPatternBuild (NULL, FC_LANG, FcTypeString, language_code, NULL);

                if (pattern == NULL)
                        goto done;

                object_set = FcObjectSetCreate ();

                if (object_set == NULL)
                        goto done;

                font_set = FcFontList (NULL, pattern, object_set);

                if (font_set == NULL)
                        goto done;

                is_displayable = (font_set->nfont > 0);
        }

 done:
        if (font_set != NULL)
                FcFontSetDestroy (font_set);

        if (object_set != NULL)
                FcObjectSetDestroy (object_set);

        if (pattern != NULL)
                FcPatternDestroy (pattern);

        g_free (language_code);

        return is_displayable;
}

void
cc_common_language_add_available_languages (GtkListStore *store,
					    GHashTable   *user_langs)
{
        char **languages;
        int i;
        char *name;
        char *language;
        GtkTreeIter iter;

        languages = gdm_get_all_language_names ();

        for (i = 0; languages[i] != NULL; i++) {
		name = gdm_normalize_language_name (languages[i]);
		if (g_hash_table_lookup (user_langs, name) != NULL) {
			g_free (name);
			continue;
		}

                if (!cc_common_language_has_font (languages[i])) {
			g_free (name);
                        continue;
		}

                language = gdm_get_language_from_name (name, NULL);

                gtk_list_store_append (store, &iter);
                gtk_list_store_set (store, &iter, LOCALE_COL, name, DISPLAY_LOCALE_COL, language, -1);

                g_free (name);
                g_free (language);
        }

        g_strfreev (languages);
}

gchar *
cc_common_language_get_current_language (void)
{
        gchar *language;
        const gchar *locale;

        locale = (const gchar *) setlocale (LC_MESSAGES, NULL);
        if (locale)
                language = gdm_normalize_language_name (locale);
        else
                language = NULL;

        return language;
}

static void
languages_foreach_cb (gpointer key,
		      gpointer value,
		      gpointer user_data)
{
	GtkListStore *store = (GtkListStore *) user_data;
	const char *locale = (const char *) key;
	const char *display_locale = (const char *) value;
	GtkTreeIter iter;

	gtk_list_store_append (store, &iter);
	gtk_list_store_set (store, &iter,
			    LOCALE_COL, locale,
			    DISPLAY_LOCALE_COL, display_locale,
			    SEPARATOR_COL, FALSE,
			    USER_LANGUAGE, TRUE,
			    -1);
}

static gboolean
separator_func (GtkTreeModel *model,
		GtkTreeIter  *iter,
		gpointer      data)
{
	gboolean is_sep;

	gtk_tree_model_get (model, iter,
			    SEPARATOR_COL, &is_sep,
			    -1);

	return is_sep;
}

void
cc_common_language_setup_list (GtkWidget    *treeview,
			       GHashTable   *initial)
{
	GtkCellRenderer *cell;
	GtkTreeViewColumn *column;
	GtkListStore *store;

        cell = gtk_cell_renderer_text_new ();
        column = gtk_tree_view_column_new_with_attributes (NULL, cell, "text", DISPLAY_LOCALE_COL, NULL);
        gtk_tree_view_append_column (GTK_TREE_VIEW (treeview), column);
        store = gtk_list_store_new (NUM_COLS, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_BOOLEAN, G_TYPE_BOOLEAN);
        gtk_tree_sortable_set_default_sort_func (GTK_TREE_SORTABLE (store),
                                                 cc_common_language_sort_languages, NULL, NULL);
        gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (store),
                                              GTK_TREE_SORTABLE_DEFAULT_SORT_COLUMN_ID,
                                              GTK_SORT_ASCENDING);
        gtk_tree_view_set_row_separator_func (GTK_TREE_VIEW (treeview),
					      separator_func,
					      NULL, NULL);

        gtk_tree_view_set_model (GTK_TREE_VIEW (treeview), GTK_TREE_MODEL (store));


        /* Add languages from the initial hashtable */
        g_hash_table_foreach (initial, (GHFunc) languages_foreach_cb, store);

        /* Add separator if we had any languages added */
        if (initial != NULL &&
            g_hash_table_size (initial) > 0) {
		GtkTreeIter iter;

		gtk_list_store_append (GTK_LIST_STORE (store), &iter);
		gtk_list_store_set (GTK_LIST_STORE (store), &iter,
				    LOCALE_COL, NULL,
				    DISPLAY_LOCALE_COL, "Don't show",
				    SEPARATOR_COL, TRUE,
				    USER_LANGUAGE, FALSE,
				    -1);
	}
}

