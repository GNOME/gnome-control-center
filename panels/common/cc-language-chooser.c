/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2013 Red Hat, Inc
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * Written by: Matthias Clasen <mclasen@redhat.com>
 */

#define _GNU_SOURCE
#include <config.h>
#include "cc-language-chooser.h"
#include "cc-common-resources.h"

#include <locale.h>
#include <string.h>
#include <glib/gi18n.h>
#include <gio/gio.h>
#include <gtk/gtk.h>

#include "egg-list-box/egg-list-box.h"

#include "cc-common-language.h"
#include "cc-util.h"

#define GNOME_DESKTOP_USE_UNSTABLE_API
#include <libgnome-desktop/gnome-languages.h>

typedef struct {
        GtkWidget *no_results;
        GtkWidget *more_item;
        GtkWidget *filter_entry;
        GtkWidget *language_list;
        GtkWidget *scrolledwindow;
        gboolean adding_languages;
        gboolean showing_extra;
        gchar *language;
        gchar **filter_words;
} CcLanguageChooserPrivate;

#define GET_PRIVATE(chooser) ((CcLanguageChooserPrivate *) g_object_get_data (G_OBJECT (chooser), "private"))

static GtkWidget *
padded_label_new (char *text, gboolean narrow)
{
        GtkWidget *widget;

        widget = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 10);
        gtk_widget_set_halign (widget, GTK_ALIGN_CENTER);
        gtk_widget_set_margin_top (widget, 10);
        gtk_widget_set_margin_bottom (widget, 10);
        gtk_widget_set_margin_left (widget, narrow ? 10 : 80);
        gtk_widget_set_margin_right (widget, narrow ? 10 : 80);
        gtk_box_pack_start (GTK_BOX (widget), gtk_label_new (text), FALSE, FALSE, 0);

        return widget;
}

static GtkWidget *
language_widget_new (const gchar *locale_id,
                     const gchar *current_locale_id,
                     gboolean     is_extra)
{
        gchar *locale_name;
        gchar *locale_current_name;
        gchar *locale_untranslated_name;
        GtkWidget *widget;
        GtkWidget *check;

        locale_name = gnome_get_language_from_locale (locale_id, locale_id);
        locale_current_name = gnome_get_language_from_locale (locale_id, NULL);
        locale_untranslated_name = gnome_get_language_from_locale (locale_id, "C");

        widget = padded_label_new (locale_name, is_extra);

        /* We add a check on each side of the label to keep it centered. */
        check = gtk_image_new ();
        gtk_image_set_from_icon_name (GTK_IMAGE (check), "object-select-symbolic", GTK_ICON_SIZE_MENU);
        gtk_widget_set_opacity (check, 0.0);
        g_object_set (check, "icon-size", GTK_ICON_SIZE_MENU, NULL);
        gtk_box_pack_start (GTK_BOX (widget), check, FALSE, FALSE, 0);
        gtk_box_reorder_child (GTK_BOX (widget), check, 0);

        check = gtk_image_new ();
        gtk_image_set_from_icon_name (GTK_IMAGE (check), "object-select-symbolic", GTK_ICON_SIZE_MENU);
        gtk_widget_set_opacity (check, 0.0);
        g_object_set (check, "icon-size", GTK_ICON_SIZE_MENU, NULL);
        gtk_box_pack_start (GTK_BOX (widget), check, FALSE, FALSE, 0);
        if (g_strcmp0 (locale_id, current_locale_id) == 0)
                gtk_widget_set_opacity (check, 1.0);

        g_object_set_data (G_OBJECT (widget), "check", check);
        g_object_set_data_full (G_OBJECT (widget), "locale-id", g_strdup (locale_id), g_free);
        g_object_set_data_full (G_OBJECT (widget), "locale-name", locale_name, g_free);
        g_object_set_data_full (G_OBJECT (widget), "locale-current-name", locale_current_name, g_free);
        g_object_set_data_full (G_OBJECT (widget), "locale-untranslated-name", locale_untranslated_name, g_free);
        g_object_set_data (G_OBJECT (widget), "is-extra", GUINT_TO_POINTER (is_extra));

        return widget;
}

static GtkWidget *
more_widget_new (void)
{
        GtkWidget *widget;
        GtkWidget *arrow;

        widget = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 10);
        gtk_widget_set_tooltip_text (widget, _("More…"));

        arrow = gtk_image_new_from_icon_name ("view-more-symbolic", GTK_ICON_SIZE_MENU);
        gtk_style_context_add_class (gtk_widget_get_style_context (arrow), "dim-label");
        gtk_widget_set_margin_top (widget, 10);
        gtk_widget_set_margin_bottom (widget, 10);
        gtk_misc_set_alignment (GTK_MISC (arrow), 0.5, 0.5);
        gtk_box_pack_start (GTK_BOX (widget), arrow, TRUE, TRUE, 0);

        return widget;
}

static GtkWidget *
no_results_widget_new (void)
{
        GtkWidget *widget;

        widget = padded_label_new (_("No languages found"), TRUE);
        gtk_widget_set_sensitive (widget, FALSE);
        return widget;
}

static void
add_languages (GtkDialog   *chooser,
               gchar      **locale_ids,
               GHashTable  *initial)
{
        CcLanguageChooserPrivate *priv = GET_PRIVATE (chooser);

        priv->adding_languages = TRUE;

        while (*locale_ids) {
                gchar *locale_id;
                gboolean is_initial;
                GtkWidget *widget;

                locale_id = *locale_ids;
                locale_ids ++;

                if (!cc_common_language_has_font (locale_id))
                        continue;

                is_initial = (g_hash_table_lookup (initial, locale_id) != NULL);
                widget = language_widget_new (locale_id, priv->language, !is_initial);
                gtk_container_add (GTK_CONTAINER (priv->language_list), widget);
        }

        gtk_container_add (GTK_CONTAINER (priv->language_list), priv->more_item);
        gtk_container_add (GTK_CONTAINER (priv->language_list), priv->no_results);

        gtk_widget_show_all (priv->language_list);

        priv->adding_languages = FALSE;
}

static void
add_all_languages (GtkDialog *chooser)
{
        gchar **locale_ids;
        GHashTable *initial;

        locale_ids = gnome_get_all_locales ();
        initial = cc_common_language_get_initial_languages ();
        add_languages (chooser, locale_ids, initial);
        g_hash_table_destroy (initial);
        g_strfreev (locale_ids);
}

static gboolean
match_all (gchar       **words,
           const gchar  *str)
{
        gchar **w;

        for (w = words; *w; ++w)
                if (!strstr (str, *w))
                        return FALSE;

        return TRUE;
}

static gboolean
language_visible (GtkWidget *child,
                  gpointer   user_data)
{
        GtkDialog *chooser = user_data;
        CcLanguageChooserPrivate *priv = GET_PRIVATE (chooser);
        gchar *locale_name = NULL;
        gchar *locale_current_name = NULL;
        gchar *locale_untranslated_name = NULL;
        gboolean is_extra;
        gboolean visible;

        if (child == priv->more_item)
                return !priv->showing_extra;

        /* We hide this in the after-refilter handler below. */
        if (child == priv->no_results)
                return TRUE;

        is_extra = GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (child), "is-extra"));

        if (!priv->showing_extra && is_extra)
                return FALSE;

        if (!priv->filter_words)
                return TRUE;

        visible = FALSE;

        locale_name =
                cc_util_normalize_casefold_and_unaccent (g_object_get_data (G_OBJECT (child), "locale-name"));
        visible = match_all (priv->filter_words, locale_name);
        if (visible)
                goto out;

        locale_current_name =
                cc_util_normalize_casefold_and_unaccent (g_object_get_data (G_OBJECT (child), "locale-current-name"));
        visible = match_all (priv->filter_words, locale_current_name);
        if (visible)
                goto out;

        locale_untranslated_name =
                cc_util_normalize_casefold_and_unaccent (g_object_get_data (G_OBJECT (child), "locale-untranslated-name"));
        visible = match_all (priv->filter_words, locale_untranslated_name);

out:
        g_free (locale_untranslated_name);
        g_free (locale_current_name);
        g_free (locale_name);
        return visible;
}

static gint
sort_languages (GtkWidget *a,
                GtkWidget *b,
                gpointer   data)
{
        const gchar *la;
        const gchar *lb;

        if (g_object_get_data (G_OBJECT (a), "locale-id") == NULL)
                return 1;
        if (g_object_get_data (G_OBJECT (b), "locale-id") == NULL)
                return -1;

        la = g_object_get_data (G_OBJECT (a), "locale-name");
        lb = g_object_get_data (G_OBJECT (b), "locale-name");

        return g_strcmp0 (la, lb);
}

static void
filter_changed (GtkDialog *chooser)
{
        CcLanguageChooserPrivate *priv = GET_PRIVATE (chooser);
        gchar *filter_contents = NULL;

        g_clear_pointer (&priv->filter_words, g_strfreev);

        filter_contents =
                cc_util_normalize_casefold_and_unaccent (gtk_entry_get_text (GTK_ENTRY (priv->filter_entry)));
        if (!filter_contents) {
                egg_list_box_refilter (EGG_LIST_BOX (priv->language_list));
                return;
        }
        priv->filter_words = g_strsplit_set (g_strstrip (filter_contents), " ", 0);
        g_free (filter_contents);
        egg_list_box_refilter (EGG_LIST_BOX (priv->language_list));
}

static void
show_more (GtkDialog *chooser)
{
        CcLanguageChooserPrivate *priv = GET_PRIVATE (chooser);
        GtkWidget *widget;
        gint width, height;

        gtk_window_get_size (GTK_WINDOW (chooser), &width, &height);
        gtk_widget_set_size_request (GTK_WIDGET (chooser), width, height);
        gtk_window_set_resizable (GTK_WINDOW (chooser), TRUE);

        widget = priv->scrolledwindow;
        gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (widget),
                                        GTK_POLICY_NEVER,
                                        GTK_POLICY_AUTOMATIC);

        gtk_widget_show (priv->filter_entry);
        gtk_widget_grab_focus (priv->filter_entry);

        priv->showing_extra = TRUE;

        egg_list_box_refilter (EGG_LIST_BOX (priv->language_list));
}

static void
set_locale_id (GtkDialog *chooser,
               const gchar       *locale_id)
{
        CcLanguageChooserPrivate *priv = GET_PRIVATE (chooser);
        GList *children, *l;

        children = gtk_container_get_children (GTK_CONTAINER (priv->language_list));
        for (l = children; l; l = l->next) {
                GtkWidget *row = l->data;
                GtkWidget *check = g_object_get_data (G_OBJECT (row), "check");
                const gchar *language = g_object_get_data (G_OBJECT (row), "locale-id");
                if (check == NULL || language == NULL)
                        continue;

                if (g_strcmp0 (locale_id, language) == 0) {
                        gboolean is_extra;

                        gtk_widget_set_opacity (check, 1.0);

                        /* make sure the selected language is shown */
                        is_extra = GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (row), "is-extra"));
                        if (!priv->showing_extra && is_extra) {
                                g_object_set_data (G_OBJECT (row), "is-extra", GINT_TO_POINTER (FALSE));
                                egg_list_box_refilter (EGG_LIST_BOX (priv->language_list));
                        }
                } else {
                        gtk_widget_set_opacity (check, 0.0);
                }
        }
        g_list_free (children);

        g_free (priv->language);
        priv->language = g_strdup (locale_id);
}

static void
child_activated (EggListBox        *box,
                 GtkWidget         *child,
                 GtkDialog *chooser)
{
        CcLanguageChooserPrivate *priv = GET_PRIVATE (chooser);
        gchar *new_locale_id;

        if (priv->adding_languages)
                return;

        if (child == NULL)
                return;

        if (child == priv->no_results)
                return;

        if (child == priv->more_item) {
                show_more (chooser);
                return;
        }
        new_locale_id = g_object_get_data (G_OBJECT (child), "locale-id");
        set_locale_id (chooser, new_locale_id);
}

typedef struct {
        gint count;
        GtkWidget *ignore;
} CountChildrenData;

static void
count_visible_children (GtkWidget *widget,
                        gpointer   user_data)
{
        CountChildrenData *data = user_data;
        if (widget != data->ignore &&
            gtk_widget_get_child_visible (widget) &&
            gtk_widget_get_visible (widget))
                data->count++;
}

static void
end_refilter (EggListBox *list_box,
              gpointer    user_data)
{
        GtkDialog *chooser = user_data;
        CcLanguageChooserPrivate *priv = GET_PRIVATE (chooser);
        CountChildrenData data = { 0 };

        data.ignore = priv->no_results;

        gtk_container_foreach (GTK_CONTAINER (list_box),
                               count_visible_children, &data);

        gtk_widget_set_visible (priv->no_results, (data.count == 0));
}

static void
update_separator_func (GtkWidget **separator,
                       GtkWidget  *child,
                       GtkWidget  *before,
                       gpointer    user_data)
{
        if (before == NULL)
                return;

        if (*separator == NULL) {
                *separator = gtk_separator_new (GTK_ORIENTATION_HORIZONTAL);
                g_object_ref_sink (*separator);
                gtk_widget_show (*separator);
        }
}

static void
cc_language_chooser_private_free (gpointer data)
{
        CcLanguageChooserPrivate *priv = data;

        g_strfreev (priv->filter_words);
        g_free (priv->language);
        g_free (priv);
}

#define WID(name) ((GtkWidget *) gtk_builder_get_object (builder, name))

GtkWidget *
cc_language_chooser_new (GtkWidget *parent)
{
        GtkBuilder *builder;
        GtkWidget *chooser;
        CcLanguageChooserPrivate *priv;
        GError *error = NULL;

        g_resources_register (cc_common_get_resource ());

        builder = gtk_builder_new ();
        if (gtk_builder_add_from_resource (builder, "/org/gnome/control-center/common/language-chooser.ui", &error) == 0) {
                g_object_unref (builder);
                g_warning ("failed to load language chooser: %s", error->message);
                g_error_free (error);
                return NULL;
        }

        chooser = WID ("language-dialog");
        priv = g_new0 (CcLanguageChooserPrivate, 1);
        g_object_set_data_full (G_OBJECT (chooser), "private", priv, cc_language_chooser_private_free);
        g_object_set_data_full (G_OBJECT (chooser), "builder", builder, g_object_unref);

        priv->filter_entry = WID ("language-filter-entry");
        priv->language_list = WID ("language-list");
        priv->scrolledwindow = WID ("language-scrolledwindow");
        priv->more_item = more_widget_new ();
        priv->no_results = no_results_widget_new ();

        egg_list_box_set_adjustment (EGG_LIST_BOX (priv->language_list),
                                     gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW (priv->scrolledwindow)));

        egg_list_box_set_sort_func (EGG_LIST_BOX (priv->language_list),
                                    sort_languages, chooser, NULL);
        egg_list_box_set_filter_func (EGG_LIST_BOX (priv->language_list),
                                      language_visible, chooser, NULL);
        egg_list_box_set_selection_mode (EGG_LIST_BOX (priv->language_list),
                                         GTK_SELECTION_NONE);
        egg_list_box_set_separator_funcs (EGG_LIST_BOX (priv->language_list),
                                          update_separator_func, NULL, NULL);
        add_all_languages (GTK_DIALOG (chooser));

        g_signal_connect_swapped (priv->filter_entry, "changed",
                                  G_CALLBACK (filter_changed), chooser);

        g_signal_connect (priv->language_list, "child-activated",
                          G_CALLBACK (child_activated), chooser);

        g_signal_connect_after (priv->language_list, "refilter",
                                G_CALLBACK (end_refilter), chooser);

        egg_list_box_refilter (EGG_LIST_BOX (priv->language_list));

        gtk_window_set_transient_for (GTK_WINDOW (chooser), GTK_WINDOW (parent));

        return chooser;
}

void
cc_language_chooser_clear_filter (GtkWidget *chooser)
{
        CcLanguageChooserPrivate *priv = GET_PRIVATE (chooser);

        gtk_entry_set_text (GTK_ENTRY (priv->filter_entry), "");
}

const gchar *
cc_language_chooser_get_language (GtkWidget *chooser)
{
        CcLanguageChooserPrivate *priv = GET_PRIVATE (chooser);

        return priv->language;
}

void
cc_language_chooser_set_language (GtkWidget   *chooser,
                                  const gchar *language)
{
        set_locale_id (GTK_DIALOG (chooser), language);
}
