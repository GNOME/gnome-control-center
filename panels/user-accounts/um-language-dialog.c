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
#include "cc-common-language.h"

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

static void
row_activated (GtkTreeView       *tree_view,
               GtkTreePath       *path,
               GtkTreeViewColumn *column,
               GtkWidget         *chooser)
{
        gtk_dialog_response (GTK_DIALOG (chooser), GTK_RESPONSE_OK);
}

static GHashTable *
new_ht_for_user_languages (void)
{
	GHashTable *ht;
        UmUserManager *manager;
        GSList *users, *l;
        UmUser *user;
        char *name;

        ht = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);

        manager = um_user_manager_ref_default ();
        users = um_user_manager_list_users (manager);
        g_object_unref (manager);

        for (l = users; l; l = l->next) {
		const char *lang;
		char *language;

                user = l->data;
                lang = um_user_get_language (user);
                if (!lang || !cc_common_language_has_font (lang)) {
                        continue;
                }

                name = gdm_normalize_language_name (lang);
                if (g_hash_table_lookup (ht, name) != NULL) {
                        g_free (name);
                        continue;
                }

                language = gdm_get_language_from_name (name, NULL);

                g_hash_table_insert (ht, name, language);
        }

        g_slist_free (users);

        /* Make sure the current locale is present */
        name = cc_common_language_get_current_language ();
        if (g_hash_table_lookup (ht, name) == NULL) {
		char *language;
                language = gdm_get_language_from_name (name, NULL);
                g_hash_table_insert (ht, name, language);
        }
        g_free (name);

	return ht;
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
			    -1);
}


void
um_add_user_languages (GtkTreeModel *model)
{
        char *name;
        GtkTreeIter iter;
        GtkListStore *store = GTK_LIST_STORE (model);
        GHashTable *user_langs;
        const char *display;

        gtk_list_store_clear (store);

	user_langs = new_ht_for_user_languages ();

	/* Add the current locale first */
	name = cc_common_language_get_current_language ();
	display = g_hash_table_lookup (user_langs, name);

        gtk_list_store_append (store, &iter);
        gtk_list_store_set (store, &iter, LOCALE_COL, name, DISPLAY_LOCALE_COL, display, -1);
        g_hash_table_remove (user_langs, name);
        g_free (name);

        /* The rest of the languages */
	g_hash_table_foreach (user_langs, (GHFunc) languages_foreach_cb, store);

	/* And now the "Other..." selection */
        gtk_list_store_append (store, &iter);
        gtk_list_store_set (store, &iter, LOCALE_COL, NULL, DISPLAY_LOCALE_COL, _("Other..."), -1);

        g_hash_table_destroy (user_langs);
}

static void
remove_timeout (gpointer data,
		GObject *where_the_object_was)
{
	guint timeout = GPOINTER_TO_UINT (data);
	g_source_remove (timeout);
}

static gboolean
finish_um_language_chooser (gpointer user_data)
{
	GtkWidget *chooser = (GtkWidget *) user_data;
	GtkWidget *list;
	GtkTreeModel *model;
	GtkWindow *parent;
	GHashTable *user_langs;
	guint timeout;

	/* Did we get called after the widget was destroyed? */
	if (chooser == NULL)
		return FALSE;

	list = g_object_get_data (G_OBJECT (chooser), "list");
	model = gtk_tree_view_get_model (GTK_TREE_VIEW (list));
	user_langs = g_object_get_data (G_OBJECT (chooser), "user-langs");

	cc_common_language_add_available_languages (GTK_LIST_STORE (model), user_langs);

	parent = gtk_window_get_transient_for (GTK_WINDOW (chooser));
	gdk_window_set_cursor (gtk_widget_get_window (GTK_WIDGET (parent)), NULL);

	g_object_set_data (G_OBJECT (chooser), "user-langs", NULL);
	timeout = GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (chooser), "timeout"));
	g_object_weak_unref (G_OBJECT (chooser), (GWeakNotify) remove_timeout, GUINT_TO_POINTER (timeout));

	return FALSE;
}

GtkWidget *
um_language_chooser_new (GtkWidget *parent)
{
        GtkBuilder *builder;
        const char *filename;
        GError *error = NULL;
        GtkWidget *chooser;
        GtkWidget *list;
        GtkWidget *button;
        GtkTreeModel *model;
        GHashTable *user_langs;
        GdkCursor *cursor;
        guint timeout;

        builder = gtk_builder_new ();
        filename = UIDIR "/language-chooser.ui";
        if (!g_file_test (filename, G_FILE_TEST_EXISTS))
                filename = "data/language-chooser.ui";
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

	/* Add user languages */
	user_langs = new_ht_for_user_languages ();
        cc_common_language_setup_list (list, user_langs);
        model = gtk_tree_view_get_model (GTK_TREE_VIEW (list));

	/* Setup so that the list is added after the dialogue is shown */
        cursor = gdk_cursor_new (GDK_WATCH);
        gdk_window_set_cursor (gtk_widget_get_window (parent), cursor);
        g_object_unref (cursor);

	g_object_set_data_full (G_OBJECT (chooser), "user-langs",
				user_langs, (GDestroyNotify) g_hash_table_destroy);
        timeout = g_idle_add ((GSourceFunc) finish_um_language_chooser, chooser);
        g_object_set_data (G_OBJECT (chooser), "timeout", GUINT_TO_POINTER (timeout));
        g_object_weak_ref (G_OBJECT (chooser), (GWeakNotify) remove_timeout, GUINT_TO_POINTER (timeout));

        g_object_unref (builder);

        return chooser;
}

