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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
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

#include "list-box-helper.h"
#include "cc-common-language.h"
#include "cc-util.h"

#define GNOME_DESKTOP_USE_UNSTABLE_API
#include <libgnome-desktop/gnome-languages.h>

struct _CcLanguageChooser {
        GtkDialog parent_instance;

        GtkWidget *select_button;
        GtkWidget *no_results;
        GtkListBoxRow *more_item;
        GtkWidget *search_bar;
        GtkWidget *language_filter_entry;
        GtkWidget *language_listbox;
        gboolean showing_extra;
        gchar *language;
        gchar **filter_words;
};

G_DEFINE_TYPE (CcLanguageChooser, cc_language_chooser, GTK_TYPE_DIALOG)

static GtkWidget *
language_widget_new (const gchar *locale_id,
                     gboolean     is_extra)
{
        g_autofree gchar *language_code = NULL;
        g_autofree gchar *country_code = NULL;
        g_autofree gchar *language = NULL;
        g_autofree gchar *country = NULL;
        g_autofree gchar *language_local = NULL;
        g_autofree gchar *country_local = NULL;
        GtkWidget *row;
        GtkWidget *box;
        GtkWidget *language_label;
        GtkWidget *check;
        GtkWidget *country_label;

        gnome_parse_locale (locale_id, &language_code, &country_code, NULL, NULL);
        language = gnome_get_language_from_code (language_code, locale_id);
        country = gnome_get_country_from_code (country_code, locale_id);
        language_local = gnome_get_language_from_code (language_code, NULL);
        country_local = gnome_get_country_from_code (country_code, NULL);

        row = gtk_list_box_row_new ();

        box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 12);
        gtk_widget_show (box);
        gtk_widget_set_margin_top (box, 12);
        gtk_widget_set_margin_bottom (box, 12);
        gtk_widget_set_margin_start (box, 18);
        gtk_widget_set_margin_end (box, 18);
        gtk_container_add (GTK_CONTAINER (row), box);

        language_label = gtk_label_new (language);
        gtk_widget_show (language_label);
        gtk_label_set_xalign (GTK_LABEL (language_label), 0.0);
        gtk_box_pack_start (GTK_BOX (box), language_label, FALSE, TRUE, 0);

        check = gtk_image_new ();
        gtk_widget_hide (check);
        gtk_image_set_from_icon_name (GTK_IMAGE (check), "object-select-symbolic", GTK_ICON_SIZE_MENU);
        g_object_set (check, "icon-size", GTK_ICON_SIZE_MENU, NULL);
        gtk_box_pack_start (GTK_BOX (box), check, FALSE, FALSE, 0);

        country_label = gtk_label_new (country);
        gtk_widget_show (country_label);
        gtk_label_set_xalign (GTK_LABEL (country_label), 1.0);
        gtk_style_context_add_class (gtk_widget_get_style_context (country_label), "dim-label");
        gtk_box_pack_start (GTK_BOX (box), country_label, TRUE, TRUE, 0);

        g_object_set_data (G_OBJECT (row), "check", check);
        g_object_set_data_full (G_OBJECT (row), "locale-id", g_strdup (locale_id), g_free);
        g_object_set_data_full (G_OBJECT (row), "language", g_steal_pointer (&language), g_free);
        g_object_set_data_full (G_OBJECT (row), "country", g_steal_pointer (&country), g_free);
        g_object_set_data_full (G_OBJECT (row), "language-local", g_steal_pointer (&language_local), g_free);
        g_object_set_data_full (G_OBJECT (row), "country-local", g_steal_pointer (&country_local), g_free);
        g_object_set_data (G_OBJECT (row), "is-extra", GUINT_TO_POINTER (is_extra));

        return row;
}

static GtkListBoxRow *
more_widget_new (void)
{
        GtkWidget *box, *row;
        GtkWidget *arrow;

        row = gtk_list_box_row_new ();
        box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 10);
        gtk_container_add (GTK_CONTAINER (row), box);
        gtk_widget_set_tooltip_text (box, _("More…"));

        arrow = gtk_image_new_from_icon_name ("view-more-symbolic", GTK_ICON_SIZE_MENU);
        gtk_style_context_add_class (gtk_widget_get_style_context (arrow), "dim-label");
        gtk_widget_set_margin_top (box, 10);
        gtk_widget_set_margin_bottom (box, 10);
        gtk_box_pack_start (GTK_BOX (box), arrow, TRUE, TRUE, 0);

        return GTK_LIST_BOX_ROW (row);
}

static GtkWidget *
no_results_widget_new (void)
{
        GtkWidget *widget;

        widget = gtk_label_new (_("No languages found"));
        gtk_widget_set_sensitive (widget, FALSE);
        return widget;
}

static void
add_languages (CcLanguageChooser *chooser,
               gchar            **locale_ids,
               GHashTable        *initial)
{
        while (*locale_ids) {
                gchar *locale_id;
                gboolean is_initial;
                GtkWidget *widget;

                locale_id = *locale_ids;
                locale_ids ++;

                if (!cc_common_language_has_font (locale_id))
                        continue;

                is_initial = (g_hash_table_lookup (initial, locale_id) != NULL);
                widget = language_widget_new (locale_id, !is_initial);
                gtk_widget_show (widget);
                gtk_container_add (GTK_CONTAINER (chooser->language_listbox), widget);
        }

        gtk_container_add (GTK_CONTAINER (chooser->language_listbox), GTK_WIDGET (chooser->more_item));
}

static void
add_all_languages (CcLanguageChooser *chooser)
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
language_visible (GtkListBoxRow *row,
                  gpointer   user_data)
{
        CcLanguageChooser *chooser = user_data;
        g_autofree gchar *language = NULL;
        g_autofree gchar *country = NULL;
        g_autofree gchar *language_local = NULL;
        g_autofree gchar *country_local = NULL;
        gboolean is_extra;
        gboolean visible;

        if (row == chooser->more_item)
                return !chooser->showing_extra;

        is_extra = GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (row), "is-extra"));

        if (!chooser->showing_extra && is_extra)
                return FALSE;

        if (!chooser->filter_words)
                return TRUE;

        language =
                cc_util_normalize_casefold_and_unaccent (g_object_get_data (G_OBJECT (row), "language"));
        visible = match_all (chooser->filter_words, language);
        if (visible)
                return TRUE;

        country =
                cc_util_normalize_casefold_and_unaccent (g_object_get_data (G_OBJECT (row), "country"));
        visible = match_all (chooser->filter_words, country);
        if (visible)
                return TRUE;

        language_local =
                cc_util_normalize_casefold_and_unaccent (g_object_get_data (G_OBJECT (row), "language-local"));
        visible = match_all (chooser->filter_words, language_local);
        if (visible)
                return TRUE;

        country_local =
                cc_util_normalize_casefold_and_unaccent (g_object_get_data (G_OBJECT (row), "country-local"));
        return match_all (chooser->filter_words, country_local);
}

static gint
sort_languages (GtkListBoxRow *a,
                GtkListBoxRow *b,
                gpointer   data)
{
        const gchar *la, *lb, *ca, *cb;
        int d;

        if (g_object_get_data (G_OBJECT (a), "locale-id") == NULL)
                return 1;
        if (g_object_get_data (G_OBJECT (b), "locale-id") == NULL)
                return -1;

        la = g_object_get_data (G_OBJECT (a), "language");
        lb = g_object_get_data (G_OBJECT (b), "language");
        d = g_strcmp0 (la, lb);
        if (d != 0)
                return d;

        ca = g_object_get_data (G_OBJECT (a), "country");
        cb = g_object_get_data (G_OBJECT (b), "country");
        return g_strcmp0 (ca, cb);
}

static void
filter_changed (CcLanguageChooser *chooser)
{
        g_autofree gchar *filter_contents = NULL;

        g_clear_pointer (&chooser->filter_words, g_strfreev);

        filter_contents =
                cc_util_normalize_casefold_and_unaccent (gtk_entry_get_text (GTK_ENTRY (chooser->language_filter_entry)));
        if (!filter_contents) {
                gtk_list_box_invalidate_filter (GTK_LIST_BOX (chooser->language_listbox));
                gtk_list_box_set_placeholder (GTK_LIST_BOX (chooser->language_listbox), NULL);
                return;
        }
        chooser->filter_words = g_strsplit_set (g_strstrip (filter_contents), " ", 0);
        gtk_list_box_set_placeholder (GTK_LIST_BOX (chooser->language_listbox), chooser->no_results);
        gtk_list_box_invalidate_filter (GTK_LIST_BOX (chooser->language_listbox));
}

static void
show_more (CcLanguageChooser *chooser, gboolean visible)
{
        gint width, height;

        gtk_window_get_size (GTK_WINDOW (chooser), &width, &height);
        gtk_widget_set_size_request (GTK_WIDGET (chooser), width, height);

        gtk_search_bar_set_search_mode (GTK_SEARCH_BAR (chooser->search_bar), visible);
        gtk_widget_grab_focus (visible ? chooser->language_filter_entry : chooser->language_listbox);

        chooser->showing_extra = visible;

        gtk_list_box_invalidate_filter (GTK_LIST_BOX (chooser->language_listbox));
}

static void
set_locale_id (CcLanguageChooser *chooser,
               const gchar       *locale_id)
{
        g_autoptr(GList) children = NULL;
        GList *l;

        gtk_widget_set_sensitive (chooser->select_button, FALSE);

        children = gtk_container_get_children (GTK_CONTAINER (chooser->language_listbox));
        for (l = children; l; l = l->next) {
                GtkWidget *row = l->data;
                GtkWidget *check = g_object_get_data (G_OBJECT (row), "check");
                const gchar *language = g_object_get_data (G_OBJECT (row), "locale-id");
                if (check == NULL || language == NULL)
                        continue;

                if (g_strcmp0 (locale_id, language) == 0) {
                        gboolean is_extra;

                        gtk_widget_show (check);
                        gtk_widget_set_sensitive (chooser->select_button, TRUE);

                        /* make sure the selected language is shown */
                        is_extra = GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (row), "is-extra"));
                        if (!chooser->showing_extra && is_extra) {
                                g_object_set_data (G_OBJECT (row), "is-extra", GINT_TO_POINTER (FALSE));
                                gtk_list_box_invalidate_filter (GTK_LIST_BOX (chooser->language_listbox));
                        }
                } else {
                        gtk_widget_hide (check);
                }
        }

        g_free (chooser->language);
        chooser->language = g_strdup (locale_id);
}

static void
row_activated (GtkListBox        *box,
               GtkListBoxRow     *row,
               CcLanguageChooser *chooser)
{
        gchar *new_locale_id;

        if (row == NULL)
                return;

        if (row == chooser->more_item) {
                show_more (chooser, TRUE);
                return;
        }
        new_locale_id = g_object_get_data (G_OBJECT (row), "locale-id");
        if (g_strcmp0 (new_locale_id, chooser->language) == 0) {
                gtk_dialog_response (GTK_DIALOG (chooser),
                                     gtk_dialog_get_response_for_widget (GTK_DIALOG (chooser),
                                                                         chooser->select_button));
        } else {
                set_locale_id (chooser, new_locale_id);
        }
}

static void
activate_default (GtkWindow         *window,
                  CcLanguageChooser *chooser)
{
        GtkWidget *focus;
        gchar *locale_id;

        focus = gtk_window_get_focus (window);
        if (!focus)
                return;

        locale_id = g_object_get_data (G_OBJECT (focus), "locale-id");
        if (g_strcmp0 (locale_id, chooser->language) == 0)
                return;

        g_signal_stop_emission_by_name (window, "activate-default");
        gtk_widget_activate (focus);
}

void
cc_language_chooser_init (CcLanguageChooser *chooser)
{
        g_resources_register (cc_common_get_resource ());

        gtk_widget_init_template (GTK_WIDGET (chooser));

        chooser->more_item = more_widget_new ();
        /* We ref-sink here so we can reuse this widget multiple times */
        chooser->no_results = g_object_ref_sink (no_results_widget_new ());
        gtk_widget_show (chooser->no_results);

        gtk_list_box_set_sort_func (GTK_LIST_BOX (chooser->language_listbox),
                                    sort_languages, chooser, NULL);
        gtk_list_box_set_filter_func (GTK_LIST_BOX (chooser->language_listbox),
                                      language_visible, chooser, NULL);
        gtk_list_box_set_selection_mode (GTK_LIST_BOX (chooser->language_listbox),
                                         GTK_SELECTION_NONE);
        gtk_list_box_set_header_func (GTK_LIST_BOX (chooser->language_listbox),
                                      cc_list_box_update_header_func, NULL, NULL);
        add_all_languages (chooser);

        g_signal_connect_swapped (chooser->language_filter_entry, "search-changed",
                                  G_CALLBACK (filter_changed), chooser);

        g_signal_connect (chooser->language_listbox, "row-activated",
                          G_CALLBACK (row_activated), chooser);

        gtk_list_box_invalidate_filter (GTK_LIST_BOX (chooser->language_listbox));

        g_signal_connect (chooser, "activate-default",
                          G_CALLBACK (activate_default), chooser);
}

static void
cc_language_chooser_dispose (GObject *object)
{
        CcLanguageChooser *chooser = CC_LANGUAGE_CHOOSER (object);

        g_clear_object (&chooser->no_results);
        g_clear_pointer (&chooser->filter_words, g_strfreev);
        g_clear_pointer (&chooser->language, g_free);

        G_OBJECT_CLASS (cc_language_chooser_parent_class)->dispose (object);
}

void
cc_language_chooser_class_init (CcLanguageChooserClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);
        GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

        object_class->dispose = cc_language_chooser_dispose;

        gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/common/cc-language-chooser.ui");

        gtk_widget_class_bind_template_child (widget_class, CcLanguageChooser, select_button);
        gtk_widget_class_bind_template_child (widget_class, CcLanguageChooser, search_bar);
        gtk_widget_class_bind_template_child (widget_class, CcLanguageChooser, language_filter_entry);
        gtk_widget_class_bind_template_child (widget_class, CcLanguageChooser, language_listbox);
}

CcLanguageChooser *
cc_language_chooser_new (void)
{
        return CC_LANGUAGE_CHOOSER (g_object_new (CC_TYPE_LANGUAGE_CHOOSER,
                                                  "use-header-bar", 1,
                                                  NULL));
}

void
cc_language_chooser_clear_filter (CcLanguageChooser *chooser)
{
        g_return_if_fail (CC_IS_LANGUAGE_CHOOSER (chooser));
        gtk_entry_set_text (GTK_ENTRY (chooser->language_filter_entry), "");
        show_more (chooser, FALSE);
}

const gchar *
cc_language_chooser_get_language (CcLanguageChooser *chooser)
{
        g_return_val_if_fail (CC_IS_LANGUAGE_CHOOSER (chooser), NULL);
        return chooser->language;
}

void
cc_language_chooser_set_language (CcLanguageChooser *chooser,
                                  const gchar       *language)
{
        g_return_if_fail (CC_IS_LANGUAGE_CHOOSER (chooser));
        set_locale_id (chooser, language);
}
