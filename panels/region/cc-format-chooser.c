/*
 * Copyright (C) 2013 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Written by:
 *     Matthias Clasen
 */

#define _GNU_SOURCE
#include <config.h>
#include "cc-format-chooser.h"

#include <locale.h>
#include <langinfo.h>
#include <string.h>
#include <glib/gi18n.h>

#include "list-box-helper.h"
#include "cc-common-language.h"
#include "cc-util.h"

#define GNOME_DESKTOP_USE_UNSTABLE_API
#include <libgnome-desktop/gnome-languages.h>

struct _CcFormatChooser {
        GtkDialog parent_instance;

        GtkWidget *done_button;
        GtkWidget *no_results;
        GtkListBoxRow *more_item;
        GtkWidget *region_filter_entry;
        GtkWidget *region_listbox;
        GtkWidget *date_format_label;
        GtkWidget *time_format_label;
        GtkWidget *date_time_format_label;
        GtkWidget *number_format_label;
        GtkWidget *measurement_format_label;
        GtkWidget *paper_format_label;
        gboolean adding;
        gboolean showing_extra;
        gchar *region;
        gchar **filter_words;
};

G_DEFINE_TYPE (CcFormatChooser, cc_format_chooser, GTK_TYPE_DIALOG)

static void
display_date (GtkWidget *label, GDateTime *dt, const gchar *format)
{
        g_autofree gchar *s = g_date_time_format (dt, format);
        gtk_label_set_text (GTK_LABEL (label), g_strstrip (s));
}

static void
update_format_examples (CcFormatChooser *chooser)
{
        g_autofree gchar *time_locale = NULL;
        g_autofree gchar *numeric_locale = NULL;
        g_autofree gchar *monetary_locale = NULL;
        g_autofree gchar *measurement_locale = NULL;
        g_autofree gchar *paper_locale = NULL;
        g_autoptr(GDateTime) dt = NULL;
        g_autofree gchar *s = NULL;
        const gchar *fmt;
        g_autoptr(GtkPaperSize) paper = NULL;

        time_locale = g_strdup (setlocale (LC_TIME, NULL));
        setlocale (LC_TIME, chooser->region);

        dt = g_date_time_new_now_local ();
        display_date (chooser->date_format_label, dt, "%x");
        display_date (chooser->time_format_label, dt, "%X");
        display_date (chooser->date_time_format_label, dt, "%c");

        setlocale (LC_TIME, time_locale);

        numeric_locale = g_strdup (setlocale (LC_NUMERIC, NULL));
        setlocale (LC_NUMERIC, chooser->region);

        s = g_strdup_printf ("%'.2f", 123456789.00);
        gtk_label_set_text (GTK_LABEL (chooser->number_format_label), s);

        setlocale (LC_NUMERIC, numeric_locale);

#if 0
        monetary_locale = g_strdup (setlocale (LC_MONETARY, NULL));
        setlocale (LC_MONETARY, chooser->region);

        num_info = localeconv ();
        if (num_info != NULL)
                gtk_label_set_text (GTK_LABEL (chooser->currency_format_label), num_info->currency_symbol);

        setlocale (LC_MONETARY, monetary_locale);
#endif

#ifdef LC_MEASUREMENT
        measurement_locale = g_strdup (setlocale (LC_MEASUREMENT, NULL));
        setlocale (LC_MEASUREMENT, chooser->region);

        fmt = nl_langinfo (_NL_MEASUREMENT_MEASUREMENT);
        if (fmt && *fmt == 2)
                gtk_label_set_text (GTK_LABEL (chooser->measurement_format_label), C_("measurement format", "Imperial"));
        else
                gtk_label_set_text (GTK_LABEL (chooser->measurement_format_label), C_("measurement format", "Metric"));

        setlocale (LC_MEASUREMENT, measurement_locale);
#endif

#ifdef LC_PAPER
        paper_locale = g_strdup (setlocale (LC_PAPER, NULL));
        setlocale (LC_PAPER, chooser->region);

        paper = gtk_paper_size_new (gtk_paper_size_get_default ());
        gtk_label_set_text (GTK_LABEL (chooser->paper_format_label), gtk_paper_size_get_display_name (paper));

        setlocale (LC_PAPER, paper_locale);
#endif
}

static void
set_locale_id (CcFormatChooser *chooser,
               const gchar     *locale_id)
{
        g_autoptr(GList) children = NULL;
        GList *l;

        children = gtk_container_get_children (GTK_CONTAINER (chooser->region_listbox));
        for (l = children; l; l = l->next) {
                GtkWidget *row = l->data;
                GtkWidget *check = g_object_get_data (G_OBJECT (row), "check");
                const gchar *region = g_object_get_data (G_OBJECT (row), "locale-id");
                if (check == NULL || region == NULL)
                        continue;

                if (g_strcmp0 (locale_id, region) == 0) {
                        gboolean is_extra;

                        /* mark as selected */
                        gtk_widget_set_opacity (check, 1.0);

                        /* make sure this row is shown */
                        is_extra = GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (row), "is-extra"));
                        if (!chooser->showing_extra && is_extra) {
                                g_object_set_data (G_OBJECT (row), "is-extra", GINT_TO_POINTER (FALSE));
                                gtk_list_box_invalidate_filter (GTK_LIST_BOX (chooser->region_listbox));
                        }

                } else {
                        /* mark as unselected */
                        gtk_widget_set_opacity (check, 0.0);
                }
        }

        g_free (chooser->region);
        chooser->region = g_strdup (locale_id);

        update_format_examples (chooser);
}

static gint
sort_regions (gconstpointer a,
              gconstpointer b,
              gpointer      data)
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

static GtkWidget *
padded_label_new (char *text, gboolean narrow)
{
        GtkWidget *widget, *label;

        widget = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 10);
        gtk_widget_set_halign (widget, GTK_ALIGN_CENTER);
        gtk_widget_set_margin_top (widget, 10);
        gtk_widget_set_margin_bottom (widget, 10);
        gtk_widget_set_margin_start (widget, narrow ? 10 : 80);
        gtk_widget_set_margin_end (widget, narrow ? 10 : 80);
        label = gtk_label_new (text);
        gtk_widget_show (label);
        gtk_container_add (GTK_CONTAINER (widget), label);

        return widget;
}

static GtkWidget *
region_widget_new (const gchar *locale_id,
                   gboolean     is_extra)
{
        gchar *locale_name;
        gchar *locale_current_name;
        gchar *locale_untranslated_name;
        GtkWidget *row, *box;
        GtkWidget *check;

        locale_name = gnome_get_country_from_locale (locale_id, locale_id);
        if (!locale_name)
          return NULL;

        locale_current_name = gnome_get_country_from_locale (locale_id, NULL);
        locale_untranslated_name = gnome_get_country_from_locale (locale_id, "C");

        row = gtk_list_box_row_new ();
        gtk_widget_show (row);
        box = padded_label_new (locale_name, is_extra);
        gtk_widget_show (box);
        gtk_container_add (GTK_CONTAINER (row), box);

        /* We add a check on each side of the label to keep it centered. */
        check = gtk_image_new ();
        gtk_widget_show (check);
        gtk_image_set_from_icon_name (GTK_IMAGE (check), "object-select-symbolic", GTK_ICON_SIZE_MENU);
        gtk_widget_set_opacity (check, 0.0);
        g_object_set (check, "icon-size", GTK_ICON_SIZE_MENU, NULL);
        gtk_container_add (GTK_CONTAINER (box), check);
        gtk_box_reorder_child (GTK_BOX (box), check, 0);

        check = gtk_image_new ();
        gtk_widget_show (check);
        gtk_image_set_from_icon_name (GTK_IMAGE (check), "object-select-symbolic", GTK_ICON_SIZE_MENU);
        gtk_widget_set_opacity (check, 0.0);
        g_object_set (check, "icon-size", GTK_ICON_SIZE_MENU, NULL);
        gtk_container_add (GTK_CONTAINER (box), check);

        g_object_set_data (G_OBJECT (row), "check", check);
        g_object_set_data_full (G_OBJECT (row), "locale-id", g_strdup (locale_id), g_free);
        g_object_set_data_full (G_OBJECT (row), "locale-name", locale_name, g_free);
        g_object_set_data_full (G_OBJECT (row), "locale-current-name", locale_current_name, g_free);
        g_object_set_data_full (G_OBJECT (row), "locale-untranslated-name", locale_untranslated_name, g_free);
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
        gtk_widget_show (box);
        gtk_container_add (GTK_CONTAINER (row), box);
        gtk_widget_set_tooltip_text (box, _("More…"));

        arrow = gtk_image_new_from_icon_name ("view-more-symbolic", GTK_ICON_SIZE_MENU);
        gtk_widget_show (arrow);
        gtk_style_context_add_class (gtk_widget_get_style_context (arrow), "dim-label");
        gtk_widget_set_hexpand (arrow, TRUE);
        gtk_widget_set_margin_top (box, 10);
        gtk_widget_set_margin_bottom (box, 10);
        gtk_container_add (GTK_CONTAINER (box), arrow);

        return GTK_LIST_BOX_ROW (row);
}

static GtkWidget *
no_results_widget_new (void)
{
        GtkWidget *widget;

        widget = padded_label_new (_("No regions found"), TRUE);
        gtk_widget_set_sensitive (widget, FALSE);
        return widget;
}

static void
add_regions (CcFormatChooser *chooser,
             gchar          **locale_ids,
             GHashTable      *initial)
{
        chooser->adding = TRUE;

        while (*locale_ids) {
                gchar *locale_id;
                gboolean is_initial;
                GtkWidget *widget;

                locale_id = *locale_ids;
                locale_ids ++;

                if (!cc_common_language_has_font (locale_id))
                        continue;

                is_initial = (g_hash_table_lookup (initial, locale_id) != NULL);
                widget = region_widget_new (locale_id, !is_initial);
                if (!widget)
                  continue;

                gtk_widget_show (widget);
                gtk_container_add (GTK_CONTAINER (chooser->region_listbox), widget);
        }

        gtk_container_add (GTK_CONTAINER (chooser->region_listbox), GTK_WIDGET (chooser->more_item));

        chooser->adding = FALSE;
}

static void
add_all_regions (CcFormatChooser *chooser)
{
        g_auto(GStrv) locale_ids = NULL;
        g_autoptr(GHashTable) initial = NULL;

        locale_ids = gnome_get_all_locales ();
        initial = cc_common_language_get_initial_languages ();
        add_regions (chooser, locale_ids, initial);
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
region_visible (GtkListBoxRow *row,
                gpointer   user_data)
{
        CcFormatChooser *chooser = user_data;
        g_autofree gchar *locale_name = NULL;
        g_autofree gchar *locale_current_name = NULL;
        g_autofree gchar *locale_untranslated_name = NULL;
        gboolean is_extra;

        if (row == chooser->more_item)
                return !chooser->showing_extra;

        is_extra = GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (row), "is-extra"));

        if (!chooser->showing_extra && is_extra)
                return FALSE;

        if (!chooser->filter_words)
                return TRUE;

        locale_name =
                cc_util_normalize_casefold_and_unaccent (g_object_get_data (G_OBJECT (row), "locale-name"));
        if (match_all (chooser->filter_words, locale_name))
                 return TRUE;

        locale_current_name =
                cc_util_normalize_casefold_and_unaccent (g_object_get_data (G_OBJECT (row), "locale-current-name"));
        if (match_all (chooser->filter_words, locale_current_name))
                 return TRUE;

        locale_untranslated_name =
                cc_util_normalize_casefold_and_unaccent (g_object_get_data (G_OBJECT (row), "locale-untranslated-name"));
        return match_all (chooser->filter_words, locale_untranslated_name);
}

static void
filter_changed (CcFormatChooser *chooser)
{
        g_autofree gchar *filter_contents = NULL;

        g_clear_pointer (&chooser->filter_words, g_strfreev);

        filter_contents =
                cc_util_normalize_casefold_and_unaccent (gtk_entry_get_text (GTK_ENTRY (chooser->region_filter_entry)));
        if (!filter_contents) {
                gtk_list_box_invalidate_filter (GTK_LIST_BOX (chooser->region_listbox));
                gtk_list_box_set_placeholder (GTK_LIST_BOX (chooser->region_listbox), NULL);
                return;
        }
        chooser->filter_words = g_strsplit_set (g_strstrip (filter_contents), " ", 0);
        gtk_list_box_set_placeholder (GTK_LIST_BOX (chooser->region_listbox), GTK_WIDGET (chooser->no_results));
        gtk_list_box_invalidate_filter (GTK_LIST_BOX (chooser->region_listbox));
}

static void
show_more (CcFormatChooser *chooser)
{
        gint width, height;

        gtk_window_get_size (GTK_WINDOW (chooser), &width, &height);
        gtk_widget_set_size_request (GTK_WIDGET (chooser), width, height);
        gtk_window_set_resizable (GTK_WINDOW (chooser), TRUE);

        gtk_widget_show (chooser->region_filter_entry);
        gtk_widget_grab_focus (chooser->region_filter_entry);

        chooser->showing_extra = TRUE;

        gtk_list_box_invalidate_filter (GTK_LIST_BOX (chooser->region_listbox));
}

static void
row_activated (CcFormatChooser *chooser,
               GtkListBoxRow   *row)
{
        const gchar *new_locale_id;

        if (chooser->adding)
                return;

        if (row == NULL)
                return;

        if (row == chooser->more_item) {
                show_more (chooser);
                return;
        }
        new_locale_id = g_object_get_data (G_OBJECT (row), "locale-id");
        if (g_strcmp0 (new_locale_id, chooser->region) == 0) {
                gtk_dialog_response (GTK_DIALOG (chooser),
                                     gtk_dialog_get_response_for_widget (GTK_DIALOG (chooser),
                                                                         chooser->done_button));
        } else {
                set_locale_id (chooser, new_locale_id);
        }
}

static void
activate_default (CcFormatChooser *chooser)
{
        GtkWidget *focus;
        const gchar *locale_id;

        focus = gtk_window_get_focus (GTK_WINDOW (chooser));
        if (!focus)
                return;

        locale_id = g_object_get_data (G_OBJECT (focus), "locale-id");
        if (g_strcmp0 (locale_id, chooser->region) == 0)
                return;

        g_signal_stop_emission_by_name (chooser, "activate-default");
        gtk_widget_activate (focus);
}

static void
cc_format_chooser_dispose (GObject *object)
{
        CcFormatChooser *chooser = CC_FORMAT_CHOOSER (object);

        g_clear_object (&chooser->no_results);
        g_clear_pointer (&chooser->filter_words, g_strfreev);
        g_clear_pointer (&chooser->region, g_free);

        G_OBJECT_CLASS (cc_format_chooser_parent_class)->dispose (object);
}

void
cc_format_chooser_class_init (CcFormatChooserClass *klass)
{
        GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
        GObjectClass *object_class = G_OBJECT_CLASS (klass);

        object_class->dispose = cc_format_chooser_dispose;

        gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/region/cc-format-chooser.ui");

        gtk_widget_class_bind_template_child (widget_class, CcFormatChooser, done_button);
        gtk_widget_class_bind_template_child (widget_class, CcFormatChooser, region_filter_entry);
        gtk_widget_class_bind_template_child (widget_class, CcFormatChooser, region_listbox);
        gtk_widget_class_bind_template_child (widget_class, CcFormatChooser, date_format_label);
        gtk_widget_class_bind_template_child (widget_class, CcFormatChooser, time_format_label);
        gtk_widget_class_bind_template_child (widget_class, CcFormatChooser, date_time_format_label);
        gtk_widget_class_bind_template_child (widget_class, CcFormatChooser, number_format_label);
        gtk_widget_class_bind_template_child (widget_class, CcFormatChooser, measurement_format_label);
        gtk_widget_class_bind_template_child (widget_class, CcFormatChooser, paper_format_label);
}

void
cc_format_chooser_init (CcFormatChooser *chooser)
{
        gtk_widget_init_template (GTK_WIDGET (chooser));

        chooser->more_item = more_widget_new ();
        gtk_widget_show (GTK_WIDGET (chooser->more_item));
        /* We ref-sink here so we can reuse this widget multiple times */
        chooser->no_results = g_object_ref_sink (no_results_widget_new ());
        gtk_widget_show (chooser->no_results);

        gtk_list_box_set_sort_func (GTK_LIST_BOX (chooser->region_listbox),
                                    (GtkListBoxSortFunc)sort_regions, chooser, NULL);
        gtk_list_box_set_filter_func (GTK_LIST_BOX (chooser->region_listbox),
                                      region_visible, chooser, NULL);
        gtk_list_box_set_selection_mode (GTK_LIST_BOX (chooser->region_listbox),
                                         GTK_SELECTION_NONE);
        gtk_list_box_set_header_func (GTK_LIST_BOX (chooser->region_listbox),
                                      cc_list_box_update_header_func, NULL, NULL);

        add_all_regions (chooser);

        g_signal_connect_object (chooser->region_filter_entry, "search-changed",
                                 G_CALLBACK (filter_changed), chooser, G_CONNECT_SWAPPED);

        g_signal_connect_object (chooser->region_listbox, "row-activated",
                                 G_CALLBACK (row_activated), chooser, G_CONNECT_SWAPPED);

        gtk_list_box_invalidate_filter (GTK_LIST_BOX (chooser->region_listbox));

        g_signal_connect_object (chooser, "activate-default",
                                 G_CALLBACK (activate_default), chooser, G_CONNECT_SWAPPED);
}

CcFormatChooser *
cc_format_chooser_new (void)
{
        return CC_FORMAT_CHOOSER (g_object_new (CC_TYPE_FORMAT_CHOOSER,
                                                "use-header-bar", 1,
                                                NULL));
}

void
cc_format_chooser_clear_filter (CcFormatChooser *chooser)
{
        g_return_if_fail (CC_IS_FORMAT_CHOOSER (chooser));
        gtk_entry_set_text (GTK_ENTRY (chooser->region_filter_entry), "");
}

const gchar *
cc_format_chooser_get_region (CcFormatChooser *chooser)
{
        g_return_val_if_fail (CC_IS_FORMAT_CHOOSER (chooser), NULL);
        return chooser->region;
}

void
cc_format_chooser_set_region (CcFormatChooser *chooser,
                              const gchar     *region)
{
        g_return_if_fail (CC_IS_FORMAT_CHOOSER (chooser));
        set_locale_id (chooser, region);
}
