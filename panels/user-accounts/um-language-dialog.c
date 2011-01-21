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

#include "um-language-dialog.h"
#include "um-user-manager.h"

#include "gdm-languages.h"

struct _UmLanguageDialog {
        GtkWidget *dialog;
        GtkWidget *user_icon;
        GtkWidget *user_name;
        GtkWidget *dialog_combo;
        GtkListStore *dialog_store;

        GtkWidget *chooser;
        GtkWidget *chooser_list;
        GtkListStore *chooser_store;

        char *language;
        UmUser *user;

        gboolean force_setting;
};

enum {
        LOCALE_COL,
        DISPLAY_LOCALE_COL,
        NUM_COLS
};

static void
cancel_language_dialog (GtkButton        *button,
                        UmLanguageDialog *um)
{
        if (um->force_setting)
                um_user_set_language (um->user, um->language);
        gtk_widget_hide (um->dialog);
        um_language_dialog_set_user (um, NULL);

}

static void
accept_language_dialog (GtkButton        *button,
                        UmLanguageDialog *um)
{
        um_user_set_language (um->user, um->language);

        gtk_widget_hide (um->dialog);
        um_language_dialog_set_user (um, NULL);
}

gchar *
um_language_chooser_get_language (GtkWidget *chooser)
{
        GtkTreeView *tv;
        GtkTreeSelection *selection;
        GtkTreeModel *model;
        GtkTreeIter iter;
        gchar *lang;

        tv = (GtkTreeView *) g_object_get_data (G_OBJECT (chooser), "list");
        selection = gtk_tree_view_get_selection (tv);
        if (gtk_tree_selection_get_selected (selection, &model, &iter))
                gtk_tree_model_get (model, &iter, LOCALE_COL, &lang, -1);
        else
                lang = NULL;

        return lang;
}

static gint
sort_languages (GtkTreeModel *model,
                GtkTreeIter  *a,
                GtkTreeIter  *b,
                gpointer      data)
{
        char *ca, *cb;
        char *la, *lb;
        gint result;

        gtk_tree_model_get (model, a, LOCALE_COL, &ca, DISPLAY_LOCALE_COL, &la, -1);
        gtk_tree_model_get (model, b, LOCALE_COL, &cb, DISPLAY_LOCALE_COL, &lb, -1);

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
um_get_iter_for_language (GtkTreeModel *model,
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

static void
select_language (UmLanguageDialog *um,
                 const gchar      *lang)
{
        if (um->chooser)
                gtk_widget_hide (um->chooser);
}

static void
row_activated (GtkTreeView       *tree_view,
               GtkTreePath       *path,
               GtkTreeViewColumn *column,
               GtkWidget         *chooser)
{
        gtk_dialog_response (GTK_DIALOG (chooser), GTK_RESPONSE_OK);
}

static gboolean
language_has_font (const gchar *locale)
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

static void
add_available_languages (GtkListStore *store)
{
        char **languages;
        int i;
        char *name;
        char *language;
        GtkTreeIter iter;

        gtk_list_store_clear (store);

        languages = gdm_get_all_language_names ();

        for (i = 0; languages[i] != NULL; i++) {
                if (!language_has_font (languages[i]))
                        continue;

                name = gdm_normalize_language_name (languages[i]);
                language = gdm_get_language_from_name (name, NULL);

                gtk_list_store_append (store, &iter);
                gtk_list_store_set (store, &iter, LOCALE_COL, name, DISPLAY_LOCALE_COL, language, -1);

                g_free (name);
                g_free (language);
        }

        g_strfreev (languages);
}

void
um_add_user_languages (GtkTreeModel *model)
{
        GHashTable *seen;
        GSList *users, *l;
        UmUser *user;
        const char *lang;
        char *name;
        char *language;
        GtkTreeIter iter;
        UmUserManager *manager;
        GtkListStore *store = GTK_LIST_STORE (model);

        gtk_list_store_clear (store);

        seen = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

        manager = um_user_manager_ref_default ();
        users = um_user_manager_list_users (manager);
        g_object_unref (manager);

        for (l = users; l; l = l->next) {
                user = l->data;
                lang = um_user_get_language (user);
                if (!lang || !language_has_font (lang)) {
                        continue;
                }

                name = gdm_normalize_language_name (lang);

                if (g_hash_table_lookup (seen, name)) {
                        g_free (name);
                        continue;
                }

                g_hash_table_insert (seen, name, GINT_TO_POINTER (TRUE));

                language = gdm_get_language_from_name (name, NULL);
                gtk_list_store_append (store, &iter);
                gtk_list_store_set (store, &iter, LOCALE_COL, name, DISPLAY_LOCALE_COL, language, -1);

                g_free (language);
        }

        g_slist_free (users);

        /* Make sure the current locale is present */
        name = um_get_current_language ();

        if (!g_hash_table_lookup (seen, name)) {
                language = gdm_get_language_from_name (name, NULL);
                gtk_list_store_append (store, &iter);
                gtk_list_store_set (store, &iter, LOCALE_COL, name, DISPLAY_LOCALE_COL, language, -1);
                g_free (language);
        }

        g_free (name);

        g_hash_table_destroy (seen);

        gtk_list_store_append (store, &iter);
        gtk_list_store_set (store, &iter, LOCALE_COL, NULL, DISPLAY_LOCALE_COL, _("Other..."), -1);
}

gchar *
um_get_current_language (void)
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

GtkWidget *
um_language_chooser_new (void)
{
        GtkBuilder *builder;
        const char *filename;
        GError *error = NULL;
        GtkWidget *chooser;
        GtkWidget *list;
        GtkWidget *button;
        GtkTreeViewColumn *column;
        GtkCellRenderer *cell;
        GtkListStore *store;

        builder = gtk_builder_new ();
        filename = UIDIR "/language-chooser.ui";
        if (!g_file_test (filename, G_FILE_TEST_EXISTS))
                filename = "../data/language-chooser.ui";
        if (!gtk_builder_add_from_file (builder, filename, &error)) {
                g_warning ("failed to load language chooser: %s", error->message);
                g_error_free (error);
                return NULL;
        }

        chooser = (GtkWidget *) gtk_builder_get_object (builder, "dialog");

        list = (GtkWidget *) gtk_builder_get_object (builder, "language-list");
        g_object_set_data (G_OBJECT (chooser), "list", list);
        g_signal_connect (list, "row-activated",
                          G_CALLBACK (row_activated), chooser);

        button = (GtkWidget *) gtk_builder_get_object (builder, "cancel-button");
        button = (GtkWidget *) gtk_builder_get_object (builder, "ok-button");
        gtk_widget_grab_default (button);

        cell = gtk_cell_renderer_text_new ();
        column = gtk_tree_view_column_new_with_attributes (NULL, cell, "text", DISPLAY_LOCALE_COL, NULL);
        gtk_tree_view_append_column (GTK_TREE_VIEW (list), column);
        store = gtk_list_store_new (NUM_COLS, G_TYPE_STRING, G_TYPE_STRING);
        gtk_tree_sortable_set_default_sort_func (GTK_TREE_SORTABLE (store),
                                                 sort_languages, NULL, NULL);
        gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (store),
                                              GTK_TREE_SORTABLE_DEFAULT_SORT_COLUMN_ID,
                                              GTK_SORT_ASCENDING);

        gtk_tree_view_set_model (GTK_TREE_VIEW (list), GTK_TREE_MODEL (store));

        add_available_languages (store);

        g_object_unref (builder);

        return chooser;
}

static void
language_combo_changed (GtkComboBox      *combo,
                        UmLanguageDialog *um)
{
        GtkTreeIter iter;
        char *lang;

        if (!gtk_combo_box_get_active_iter (combo, &iter))
                return;

        gtk_tree_model_get (GTK_TREE_MODEL (um->dialog_store), &iter, LOCALE_COL, &lang, -1);

        if (lang) {
                g_free (um->language);
                um->language = lang;
                return;
        }

#if 0
        if (!um->chooser)
                setup_language_chooser (um);
#endif

        gtk_window_present (GTK_WINDOW (um->chooser));
        gtk_widget_grab_focus (um->chooser_list);

        gtk_widget_set_sensitive (GTK_WIDGET (combo), FALSE);
}

UmLanguageDialog *
um_language_dialog_new (void)
{
        GtkBuilder *builder;
        GtkWidget *widget;
        UmLanguageDialog *um;
        const gchar *filename;
        GtkListStore *store;
        GError *error = NULL;

        builder = gtk_builder_new ();

        filename = UIDIR "/language-dialog.ui";
        if (!g_file_test (filename, G_FILE_TEST_EXISTS))
                filename = "../data/language-dialog.ui";
        if (!gtk_builder_add_from_file (builder, filename, &error)) {
                g_error ("%s", error->message);
                g_error_free (error);
                return NULL;
        }

        um = g_new0 (UmLanguageDialog, 1);

        widget = (GtkWidget *) gtk_builder_get_object (builder, "dialog");
        g_signal_connect (widget, "delete-event",
                          G_CALLBACK (gtk_widget_hide_on_delete), NULL);
        um->dialog = widget;

        widget = (GtkWidget *) gtk_builder_get_object (builder, "cancel-button");
        g_signal_connect (widget, "clicked",
                          G_CALLBACK (cancel_language_dialog), um);

        widget = (GtkWidget *) gtk_builder_get_object (builder, "ok-button");
        g_signal_connect (widget, "clicked",
                          G_CALLBACK (accept_language_dialog), um);
        gtk_widget_grab_default (widget);

        store = gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_STRING);
        gtk_tree_sortable_set_default_sort_func (GTK_TREE_SORTABLE (store),
                                                 sort_languages, NULL, NULL);
        gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (store),
                                              GTK_TREE_SORTABLE_DEFAULT_SORT_COLUMN_ID,
                                              GTK_SORT_ASCENDING);
        um->dialog_store = store;

        widget = (GtkWidget *) gtk_builder_get_object (builder, "language-combobox");
        gtk_combo_box_set_model (GTK_COMBO_BOX (widget), GTK_TREE_MODEL (store));
        g_signal_connect (widget, "changed",
                          G_CALLBACK (language_combo_changed), um);
        um->dialog_combo = widget;

        um->user_icon = (GtkWidget *) gtk_builder_get_object (builder, "user-icon");
        um->user_name = (GtkWidget *) gtk_builder_get_object (builder, "user-name");

        return um;
}

void
um_language_dialog_free (UmLanguageDialog *um)
{
        gtk_widget_destroy (um->dialog);

        if (um->chooser)
                gtk_widget_destroy (um->chooser);

        g_free (um->language);

        if (um->user)
                g_object_unref (um->user);

        g_free (um);
}

void
um_language_dialog_set_user (UmLanguageDialog *um,
                             UmUser           *user)
{
        GdkPixbuf *pixbuf;
        const gchar *name;

        if (um->user) {
                g_object_unref (um->user);
                um->user = NULL;
        }
        if (um->language) {
                g_free (um->language);
                um->language = NULL;
        }
        um->force_setting = FALSE;

        um->user = user;
        if (um->user) {
                const gchar *language;

                g_object_ref (user);

                pixbuf = um_user_render_icon (user, FALSE, 48);
                gtk_image_set_from_pixbuf (GTK_IMAGE (um->user_icon), pixbuf);
                g_object_unref (pixbuf);

                name = um_user_get_real_name (user);
                gtk_label_set_label (GTK_LABEL (um->user_name), name);

                um_add_user_languages (gtk_combo_box_get_model (GTK_COMBO_BOX (um->dialog_combo)));

                language = um_user_get_language (user);
                if (language) {
                        select_language (um, language);
                } else if (um_user_get_uid (user) == getuid ()) {
                        gchar *lang;

                        lang = um_get_current_language ();
                        select_language (um, lang);
                        g_free (lang);
                        um->force_setting = TRUE;
                }
        }
}

void
um_language_dialog_show (UmLanguageDialog *um,
                         GtkWindow        *parent)
{
        gtk_window_set_transient_for (GTK_WINDOW (um->dialog), parent);
        gtk_window_present (GTK_WINDOW (um->dialog));
        gtk_widget_grab_focus (um->dialog_combo);
}

