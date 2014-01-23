/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright 2009-2010  Red Hat, Inc,
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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
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

#define GNOME_DESKTOP_USE_UNSTABLE_API
#include <libgnome-desktop/gnome-languages.h>

#include "cc-common-language.h"

static char *get_lang_for_user_object_path (const char *path);

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
		result = 0;
	else if (sa)
		result = ulb ? 1 : -1;
	else if (sb)
		result = ula ? -1 : 1;

	/* Sort user-languages first */
	else if (ula != ulb) {
		if (ula)
			result = -1;
		else
			result = 1;
	}

        else if (!ca)
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

static gboolean
iter_for_language (GtkTreeModel *model,
                   const gchar  *lang,
                   GtkTreeIter  *iter,
                   gboolean      region)
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

        name = gnome_normalize_locale (lang);
        if (name != NULL) {
                if (region) {
                        language = gnome_get_country_from_locale (name, NULL);
                }
                else {
                        language = gnome_get_language_from_locale (name, NULL);
                }

                gtk_list_store_insert_with_values (GTK_LIST_STORE (model),
                                                   iter,
                                                   -1,
                                                   LOCALE_COL, name,
                                                   DISPLAY_LOCALE_COL, language,
                                                   -1);
                g_free (name);
                g_free (language);
                return TRUE;
        }

        return FALSE;
}

gboolean
cc_common_language_get_iter_for_language (GtkTreeModel *model,
                                          const gchar  *lang,
                                          GtkTreeIter  *iter)
{
  return iter_for_language (model, lang, iter, FALSE);
}

gboolean
cc_common_language_get_iter_for_region (GtkTreeModel *model,
                                        const gchar  *lang,
                                        GtkTreeIter  *iter)
{
  return iter_for_language (model, lang, iter, TRUE);
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

        if (!gnome_parse_locale (locale, &language_code, NULL, NULL, NULL))
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

typedef struct
{
  GtkListStore  *store;
  GHashTable    *user_langs;
  gchar        **languages;
  gboolean       regions;
  gint           position;
} AsyncLangData;

static void
async_lang_data_free (AsyncLangData *data)
{
  g_object_unref (data->store);
  g_hash_table_unref (data->user_langs);
  g_strfreev (data->languages);
  g_free (data);
}

static gboolean
add_one_language (gpointer d)
{
  AsyncLangData *data = d;
  char *name;
  char *language;
  GtkTreeIter iter;

  if (data->languages[data->position] == NULL) {
    /* we are done */
    async_lang_data_free (data);
    return FALSE;
  }

  name = gnome_normalize_locale (data->languages[data->position]);
  if (g_hash_table_lookup (data->user_langs, name) != NULL) {
    g_free (name);
    goto next;
  }

  if (!cc_common_language_has_font (data->languages[data->position])) {
    g_free (name);
    goto next;
  }

  if (data->regions) {
    language = gnome_get_country_from_locale (name, NULL);
  }
  else {
    language = gnome_get_language_from_locale (name, NULL);
  }
  if (!language) {
    g_debug ("Ignoring '%s' as a locale, because we couldn't figure the language name", name);
    g_free (name);
    goto next;
  }

  gtk_list_store_insert_with_values (data->store,
                                     &iter,
                                     -1,
                                     LOCALE_COL, name,
                                     DISPLAY_LOCALE_COL, language,
                                     -1);

  g_free (name);
  g_free (language);

 next:
  data->position++;

  return TRUE;
}

guint
cc_common_language_add_available_languages (GtkListStore *store,
                                            gboolean      regions,
                                            GHashTable   *user_langs)
{
  AsyncLangData *data;

  data = g_new0 (AsyncLangData, 1);

  data->store = g_object_ref (store);
  data->user_langs = g_hash_table_ref (user_langs);
  data->languages = gnome_get_all_locales ();
  data->regions = regions;
  data->position = 0;

  return g_idle_add (add_one_language, data);
}

gchar *
cc_common_language_get_current_language (void)
{
        gchar *language;
        char *path;
        const gchar *locale;

	path = g_strdup_printf ("/org/freedesktop/Accounts/User%d", getuid ());
        language = get_lang_for_user_object_path (path);
        g_free (path);
        if (language != NULL && *language != '\0')
                return language;

        locale = (const gchar *) setlocale (LC_MESSAGES, NULL);
        if (locale)
                language = gnome_normalize_locale (locale);
        else
                language = NULL;

        return language;
}

typedef struct {
	GtkListStore *store;
	gboolean      user_lang;
} LangForeachData;

static void
languages_foreach_cb (gpointer key,
		      gpointer value,
		      gpointer user_data)
{
	LangForeachData *data = (LangForeachData *) user_data;
	const char *locale = (const char *) key;
	const char *display_locale = (const char *) value;

        gtk_list_store_insert_with_values (data->store,
                                           NULL,
                                           -1,
                                           LOCALE_COL, locale,
                                           DISPLAY_LOCALE_COL, display_locale,
                                           SEPARATOR_COL, FALSE,
                                           USER_LANGUAGE, data->user_lang,
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
			       GHashTable   *users,
			       GHashTable   *initial)
{
	GtkCellRenderer *cell;
	GtkTreeViewColumn *column;
	GtkListStore *store;
	LangForeachData data;
	GList *langs, *l;

        cell = gtk_cell_renderer_text_new ();
	g_object_set (cell,
		      "width-chars", 40,
		      "ellipsize", PANGO_ELLIPSIZE_END,
		      NULL);
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

        data.store = store;

        /* Add the original users languages */
        if (users != NULL && g_hash_table_size (users) > 0) {
                data.user_lang = TRUE;
                g_hash_table_foreach (users, (GHFunc) languages_foreach_cb, &data);

                gtk_list_store_insert_with_values (store,
                                                   NULL,
                                                   -1,
                                                   LOCALE_COL, NULL,
                                                   DISPLAY_LOCALE_COL, "Don't show",
                                                   SEPARATOR_COL, TRUE,
                                                   USER_LANGUAGE, FALSE,
                                                   -1);
        }

        /* Add languages from the initial hashtable */
        data.user_lang = FALSE;
        langs = initial ? g_hash_table_get_keys (initial) : NULL;
        for (l = langs; l != NULL; l = l->next) {
                char *lang = l->data;

                if (g_hash_table_lookup (users, lang) != NULL)
                        continue;

                languages_foreach_cb (lang, g_hash_table_lookup (initial, lang), &data);
	}
}

void
cc_common_language_select_current_language (GtkTreeView *treeview)
{
	GtkTreeModel *model;
	GtkTreeIter iter;
	gboolean cont;
	char *lang;
	gboolean found;

	lang = cc_common_language_get_current_language ();
	g_debug ("Trying to select lang '%s' in treeview", lang);
	model = gtk_tree_view_get_model (treeview);
	found = FALSE;
	cont = gtk_tree_model_get_iter_first (model, &iter);
	while (cont) {
		char *locale;

		gtk_tree_model_get (model, &iter,
				    LOCALE_COL, &locale,
				    -1);
		if (locale != NULL &&
		    g_str_equal (locale, lang)) {
			GtkTreeSelection *selection;

			g_debug ("Found '%s' in treeview", locale);

			found = TRUE;
			selection = gtk_tree_view_get_selection (treeview);
			gtk_tree_selection_select_iter (selection, &iter);
			g_free (locale);
			break;
		}
		g_free (locale);

		cont = gtk_tree_model_iter_next (model, &iter);
	}
	g_free (lang);

	if (found == FALSE)
		g_warning ("Could not find current language '%s' in the treeview", lang);
}

static gboolean
user_language_has_translations (const char *locale)
{
        char *name, *language_code, *territory_code;
        gboolean ret;

        gnome_parse_locale (locale,
                            &language_code,
                            &territory_code,
                            NULL, NULL);
        name = g_strdup_printf ("%s%s%s",
                                language_code,
                                territory_code != NULL? "_" : "",
                                territory_code != NULL? territory_code : "");
        g_free (language_code);
        g_free (territory_code);
        ret = gnome_language_has_translations (name);
        g_free (name);

        return ret;
}

static char *
get_lang_for_user_object_path (const char *path)
{
	GError *error = NULL;
	GDBusProxy *user;
	GVariant *props;
	char *lang;

	user = g_dbus_proxy_new_for_bus_sync (G_BUS_TYPE_SYSTEM,
					      G_DBUS_PROXY_FLAGS_NONE,
					      NULL,
					      "org.freedesktop.Accounts",
					      path,
					      "org.freedesktop.Accounts.User",
					      NULL,
					      &error);
	if (user == NULL) {
		g_warning ("Failed to get proxy for user '%s': %s",
			   path, error->message);
		g_error_free (error);
		return NULL;
	}

	props = g_dbus_proxy_get_cached_property (user, "Language");
	if (props == NULL) {
		g_object_unref (user);
		return NULL;
	}
	lang = g_variant_dup_string (props, NULL);

	g_variant_unref (props);
	g_object_unref (user);
	return lang;
}

static void
add_other_users_language (GHashTable *ht)
{
        GVariant *variant;
        GVariantIter *vi;
        GError *error = NULL;
        const char *str;
        GDBusProxy *proxy;

        proxy = g_dbus_proxy_new_for_bus_sync (G_BUS_TYPE_SYSTEM,
                                               G_DBUS_PROXY_FLAGS_NONE,
                                               NULL,
                                               "org.freedesktop.Accounts",
                                               "/org/freedesktop/Accounts",
                                               "org.freedesktop.Accounts",
                                               NULL,
                                               NULL);

        if (proxy == NULL)
                return;

        variant = g_dbus_proxy_call_sync (proxy,
                                          "ListCachedUsers",
                                          NULL,
                                          G_DBUS_CALL_FLAGS_NONE,
                                          -1,
                                          NULL,
                                          &error);
        if (variant == NULL) {
                g_warning ("Failed to list existing users: %s", error->message);
                g_error_free (error);
                g_object_unref (proxy);
                return;
        }
        g_variant_get (variant, "(ao)", &vi);
        while (g_variant_iter_loop (vi, "o", &str)) {
                char *lang;
                char *name;
                char *language;

                lang = get_lang_for_user_object_path (str);
                if (lang != NULL && *lang != '\0' &&
                    cc_common_language_has_font (lang) &&
                    user_language_has_translations (lang)) {
                        name = gnome_normalize_locale (lang);
                        if (!g_hash_table_lookup (ht, name)) {
                                language = gnome_get_language_from_locale (name, NULL);
                                g_hash_table_insert (ht, name, language);
                        }
                        else {
                                g_free (name);
                        }
                }
                g_free (lang);
        }
        g_variant_iter_free (vi);
        g_variant_unref (variant);

        g_object_unref (proxy);
}

static void
insert_language (GHashTable *ht,
                 const char *lang)
{
        gboolean has_translations;
        char *label_own_lang;
        char *label_current_lang;
        char *label_untranslated;
        char *key;

        has_translations = gnome_language_has_translations (lang);
        if (!has_translations) {
                char *lang_code = g_strndup (lang, 2);
                has_translations = gnome_language_has_translations (lang_code);
                g_free (lang_code);

                if (!has_translations)
                        return;
        }

        g_debug ("We have translations for %s", lang);

        if (g_str_has_suffix (lang, ".utf8"))
                key = g_strdup (lang);
        else
                key = g_strdup_printf ("%s.utf8", lang);

        label_own_lang = gnome_get_language_from_locale (key, key);
        label_current_lang = gnome_get_language_from_locale (key, NULL);
        label_untranslated = gnome_get_language_from_locale (key, "C");

        /* We don't have a translation for the label in
         * its own language? */
        if (g_strcmp0 (label_own_lang, label_untranslated) == 0) {
                if (g_strcmp0 (label_current_lang, label_untranslated) == 0)
                        g_hash_table_insert (ht, key, g_strdup (label_untranslated));
                else
                        g_hash_table_insert (ht, key, g_strdup (label_current_lang));
        } else {
                g_hash_table_insert (ht, key, g_strdup (label_own_lang));
        }

        g_free (label_own_lang);
        g_free (label_current_lang);
        g_free (label_untranslated);
}

GHashTable *
cc_common_language_get_initial_languages (void)
{
        GHashTable *ht;

        ht = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);

        insert_language (ht, "en_US");
        insert_language (ht, "en_GB");
        insert_language (ht, "de_DE");
        insert_language (ht, "fr_FR");
        insert_language (ht, "es_ES");
        insert_language (ht, "zh_CN");
        insert_language (ht, "ja_JP");
        insert_language (ht, "ru_RU");
        insert_language (ht, "ar_EG");

        return ht;
}

GHashTable *
cc_common_language_get_user_languages (void)
{
        GHashTable *ht;
        char *name;
        char *language;

        ht = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);

        /* Add the languages used by other users on the system */
        add_other_users_language (ht);

        /* Add current locale */
        name = cc_common_language_get_current_language ();
        if (g_hash_table_lookup (ht, name) == NULL) {
                language = gnome_get_language_from_locale (name, NULL);
                g_hash_table_insert (ht, name, language);
        } else {
                g_free (name);
        }

        return ht;
}

GHashTable *
cc_common_language_get_initial_regions (const gchar *lang)
{
        GHashTable *ht;
        char *language;
        gchar **langs;
        gint i;

        ht = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);

#if 0
        /* Add some common regions */
        g_hash_table_insert (ht, g_strdup ("en_US.utf8"), g_strdup (_("United States")));
        g_hash_table_insert (ht, g_strdup ("de_DE.utf8"), g_strdup (_("Germany")));
        g_hash_table_insert (ht, g_strdup ("fr_FR.utf8"), g_strdup (_("France")));
        g_hash_table_insert (ht, g_strdup ("es_ES.utf8"), g_strdup (_("Spain")));
        g_hash_table_insert (ht, g_strdup ("zh_CN.utf8"), g_strdup (_("China")));
#endif

        gnome_parse_locale (lang, &language, NULL, NULL, NULL);
        langs = gnome_get_all_locales ();
        for (i = 0; langs[i]; i++) {
                gchar *l, *s;
                gnome_parse_locale (langs[i], &l, NULL, NULL, NULL);
                if (g_strcmp0 (language, l) == 0) {
                        if (!g_hash_table_lookup (ht, langs[i])) {
                                s = gnome_get_country_from_locale (langs[i], NULL);
                                g_hash_table_insert (ht, g_strdup (langs[i]), s);
                        }
                }
                g_free (l);
        }
        g_strfreev (langs);
        g_free (language);

        return ht;
}

static void
foreach_user_lang_cb (gpointer key,
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
cc_common_language_add_user_languages (GtkTreeModel *model)
{
        char *name;
        GtkTreeIter iter;
        GtkListStore *store = GTK_LIST_STORE (model);
        GHashTable *user_langs;
        const char *display;

        gtk_list_store_clear (store);

        user_langs = cc_common_language_get_initial_languages ();

        /* Add the current locale first */
        name = cc_common_language_get_current_language ();
        display = g_hash_table_lookup (user_langs, name);
        if (!display) {
                insert_language (user_langs, name);
                display = g_hash_table_lookup (user_langs, name);
        }

        gtk_list_store_append (store, &iter);
        gtk_list_store_set (store, &iter, LOCALE_COL, name, DISPLAY_LOCALE_COL, display, -1);
        g_hash_table_remove (user_langs, name);
        g_free (name);

        /* The rest of the languages */
        g_hash_table_foreach (user_langs, (GHFunc) foreach_user_lang_cb, store);

        /* And now the "Other…" selection */
        gtk_list_store_append (store, &iter);
        gtk_list_store_set (store, &iter, LOCALE_COL, NULL, DISPLAY_LOCALE_COL, _("Other…"), -1);

        g_hash_table_destroy (user_langs);
}

