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

#include <errno.h>
#include <locale.h>
#include <langinfo.h>
#include <string.h>
#include <glib/gi18n.h>
#include <adwaita.h>

#include "cc-common-language.h"
#include "cc-regional-language-row.h"
#include "cc-format-preview.h"
#include "cc-util.h"

#define GNOME_DESKTOP_USE_UNSTABLE_API
#include <libgnome-desktop/gnome-languages.h>

struct _CcFormatChooser {
  AdwDialog parent_instance;

  GtkWidget *split_view;
  GtkWidget *region_filter_entry;
  GtkWidget *region_list_stack;
  GtkWidget *common_region_title;
  GtkWidget *common_region_listbox;
  GtkWidget *region_title;
  GtkWidget *region_listbox;
  GtkWidget *close_sidebar_button;
  GtkLabel *preview_title_label;
  CcFormatPreview *format_preview;
  gboolean adding;
  gboolean showing_extra;
  gboolean no_results;
  gchar *region;
  gchar *preview_region;
  gchar **filter_words;
};

G_DEFINE_TYPE (CcFormatChooser, cc_format_chooser, ADW_TYPE_DIALOG)

enum
{
  LANGUAGE_SELECTED,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

static void
update_check_button_for_list (GtkWidget   *list_box,
                              const gchar *locale_id)
{
  GtkWidget *row;
  const gchar *row_locale_id;

  for (row = gtk_widget_get_first_child (list_box);
       row;
       row = gtk_widget_get_next_sibling (row))
    {
      if (!CC_IS_REGIONAL_LANGUAGE_ROW (row))
        continue;

      row_locale_id = cc_regional_language_row_get_locale_id (CC_REGIONAL_LANGUAGE_ROW (row));

      if (!row_locale_id)
        continue;

      cc_regional_language_row_set_checked (CC_REGIONAL_LANGUAGE_ROW (row),
                                            g_strcmp0 (locale_id, row_locale_id) == 0);
    }
}

static void
set_locale_id (CcFormatChooser *chooser,
               const gchar     *locale_id)
{
        g_autofree gchar *locale_name = NULL;

        g_free (chooser->region);
        chooser->region = g_strdup (locale_id);

        update_check_button_for_list (chooser->region_listbox, locale_id);
        update_check_button_for_list (chooser->common_region_listbox, locale_id);
        cc_format_preview_set_region (chooser->format_preview, locale_id);

        locale_name = gnome_get_country_from_locale (locale_id, locale_id);
        gtk_label_set_label (chooser->preview_title_label, locale_name);
}

static gint
sort_regions (CcRegionalLanguageRow *a,
              CcRegionalLanguageRow *b,
              gpointer               data)
{
        const gchar *la;
        const gchar *lb;

        if (!cc_regional_language_row_get_locale_id (a))
                return 1;
        if (!cc_regional_language_row_get_locale_id (b))
                return -1;

        la = g_object_get_data (G_OBJECT (a), "locale-name");
        lb = g_object_get_data (G_OBJECT (b), "locale-name");

        lb = cc_regional_language_row_get_country (b);

        return g_strcmp0 (la, lb);
}

static void
on_stop_search (CcFormatChooser *self)
{
  const char *search_text;
  search_text = gtk_editable_get_text (GTK_EDITABLE (self->region_filter_entry));

  if (search_text && g_strcmp0 (search_text, "") != 0)
    gtk_editable_set_text (GTK_EDITABLE (self->region_filter_entry), "");
  else
    adw_dialog_close (ADW_DIALOG (self));
}

static void
collapsed_cb (CcFormatChooser *self)
{
    if (!self->region)
        return;

    if (!adw_overlay_split_view_get_collapsed (ADW_OVERLAY_SPLIT_VIEW (self->split_view))) {
        g_autofree gchar *locale_name = NULL;

        cc_format_preview_set_region (self->format_preview, self->region);
        locale_name = gnome_get_country_from_locale (self->region, self->region);
        gtk_label_set_label (self->preview_title_label, locale_name);
    }
}

static void
format_chooser_close_sidebar_button_pressed_cb (CcFormatChooser *self)
{
  adw_overlay_split_view_set_show_sidebar (ADW_OVERLAY_SPLIT_VIEW (self->split_view), FALSE);
}

static void
preview_button_clicked_cb (CcFormatChooser *self,
                           GtkWidget       *button)
{
  GtkWidget *row;
  const gchar *region;
  const gchar *locale_name;

  g_assert (CC_IS_FORMAT_CHOOSER (self));
  g_assert (GTK_IS_WIDGET (button));

  row = gtk_widget_get_ancestor (button, GTK_TYPE_LIST_BOX_ROW);
  g_assert (row);

  region = cc_regional_language_row_get_locale_id (CC_REGIONAL_LANGUAGE_ROW (row));
  cc_format_preview_set_region (self->format_preview, region);
  locale_name = g_object_get_data (G_OBJECT (row), "locale-name");
  gtk_label_set_label (self->preview_title_label, locale_name);

  adw_overlay_split_view_set_show_sidebar (ADW_OVERLAY_SPLIT_VIEW (self->split_view), TRUE);
}

static CcRegionalLanguageRow *
region_widget_new (CcFormatChooser *self,
                   const gchar     *locale_id)
{
        gchar *locale_name;
        gchar *locale_current_name;
        gchar *locale_untranslated_name;
        GtkWidget *button;
        CcRegionalLanguageRow *row;

        locale_name = gnome_get_country_from_locale (locale_id, locale_id);
        if (!locale_name)
          return NULL;

        locale_current_name = gnome_get_country_from_locale (locale_id, NULL);
        locale_untranslated_name = gnome_get_country_from_locale (locale_id, "C");

        row = cc_regional_language_row_new (locale_id, CC_REGIONAL_LANGUAGE_ROW_TYPE_REGION);

        button = gtk_button_new_from_icon_name ("view-reveal-symbolic");
        gtk_widget_set_tooltip_text (button, _("Preview"));
        gtk_widget_add_css_class (button, "flat");
        gtk_widget_set_valign (button, GTK_ALIGN_CENTER);
        g_signal_connect_object (button, "clicked", G_CALLBACK (preview_button_clicked_cb),
                                 self, G_CONNECT_SWAPPED);
        g_object_bind_property (self->split_view, "collapsed",
                                button, "visible",
                                G_BINDING_SYNC_CREATE);

        cc_regional_language_row_add_suffix_widget (row, button);

        g_object_set_data_full (G_OBJECT (row), "locale-name", locale_name, g_free);
        g_object_set_data_full (G_OBJECT (row), "locale-current-name", locale_current_name, g_free);
        g_object_set_data_full (G_OBJECT (row), "locale-untranslated-name", locale_untranslated_name, g_free);

        return row;
}

static void
add_regions (CcFormatChooser *chooser,
             gchar          **locale_ids,
             GHashTable      *initial)
{
        g_autoptr(GList) initial_locales = NULL;
        CcRegionalLanguageRow *row;
        GList *l;

        chooser->adding = TRUE;
        initial_locales = g_hash_table_get_keys (initial);

        /* Populate Common Locales */
        for (l = initial_locales; l != NULL; l = l->next) {
                if (!cc_common_language_has_font (l->data))
                        continue;

                row = region_widget_new (chooser, l->data);
                if (!row)
                        continue;

                gtk_list_box_append (GTK_LIST_BOX (chooser->common_region_listbox), GTK_WIDGET (row));
          }

        /* Populate All locales */
        while (*locale_ids) {
                gchar *locale_id;

                locale_id = *locale_ids;
                locale_ids ++;

                if (!cc_common_language_has_font (locale_id))
                        continue;

                row = region_widget_new (chooser, locale_id);
                if (!row)
                  continue;

                gtk_list_box_append (GTK_LIST_BOX (chooser->region_listbox), GTK_WIDGET (row));
        }

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
        gboolean match = TRUE;

        if (!chooser->filter_words)
          goto end;

        locale_name =
                cc_util_normalize_casefold_and_unaccent (g_object_get_data (G_OBJECT (row), "locale-name"));
        if (match_all (chooser->filter_words, locale_name))
          goto end;

        locale_current_name =
                cc_util_normalize_casefold_and_unaccent (g_object_get_data (G_OBJECT (row), "locale-current-name"));
        if (match_all (chooser->filter_words, locale_current_name))
          goto end;

        locale_untranslated_name =
                cc_util_normalize_casefold_and_unaccent (g_object_get_data (G_OBJECT (row), "locale-untranslated-name"));

        match = match_all (chooser->filter_words, locale_untranslated_name);

 end:
        if (match)
          chooser->no_results = FALSE;
        return match;
}

static void
filter_changed (CcFormatChooser *chooser)
{
        g_autofree gchar *filter_contents = NULL;
        gboolean visible;

        g_clear_pointer (&chooser->filter_words, g_strfreev);

        filter_contents =
                cc_util_normalize_casefold_and_unaccent (gtk_editable_get_text (GTK_EDITABLE (chooser->region_filter_entry)));

        /* The popular listbox is shown only if search is empty */
        visible = filter_contents == NULL || *filter_contents == '\0';
        gtk_widget_set_visible (chooser->common_region_listbox, visible);
        gtk_widget_set_visible (chooser->common_region_title, visible);
        gtk_widget_set_visible (chooser->region_title, visible);

        /* Reset cached search state */
        chooser->no_results = TRUE;

        if (!filter_contents) {
                gtk_list_box_invalidate_filter (GTK_LIST_BOX (chooser->region_listbox));
                gtk_list_box_set_placeholder (GTK_LIST_BOX (chooser->region_listbox), NULL);
                return;
        }
        chooser->filter_words = g_strsplit_set (g_strstrip (filter_contents), " ", 0);
        gtk_list_box_invalidate_filter (GTK_LIST_BOX (chooser->region_listbox));

        if (chooser->no_results)
          gtk_stack_set_visible_child_name (GTK_STACK (chooser->region_list_stack),
                                            "empty_results_page");
        else
          gtk_stack_set_visible_child_name (GTK_STACK (chooser->region_list_stack),
                                            "region_list_page");
}

static void
row_activated (CcFormatChooser       *chooser,
               CcRegionalLanguageRow *row)
{
        const gchar *new_locale_id;

        if (chooser->adding)
                return;

        new_locale_id = cc_regional_language_row_get_locale_id (row);
        if (g_strcmp0 (new_locale_id, chooser->region) == 0)
                g_signal_emit (chooser, signals[LANGUAGE_SELECTED], 0);
        else
                set_locale_id (chooser, new_locale_id);
}

static void
select_button_clicked_cb (CcFormatChooser *self)
{
        g_signal_emit (self, signals[LANGUAGE_SELECTED], 0);
}

static void
cc_format_chooser_dispose (GObject *object)
{
        CcFormatChooser *chooser = CC_FORMAT_CHOOSER (object);

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

        g_type_ensure (CC_TYPE_FORMAT_PREVIEW);

        signals[LANGUAGE_SELECTED] =
                g_signal_new ("language-selected",
                              G_TYPE_FROM_CLASS (klass),
                              G_SIGNAL_RUN_LAST,
                              0,
                              NULL, NULL, NULL,
                              G_TYPE_NONE,
                              0);

        gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/system/region/cc-format-chooser.ui");

        gtk_widget_class_bind_template_child (widget_class, CcFormatChooser, split_view);
        gtk_widget_class_bind_template_child (widget_class, CcFormatChooser, region_filter_entry);
        gtk_widget_class_bind_template_child (widget_class, CcFormatChooser, common_region_title);
        gtk_widget_class_bind_template_child (widget_class, CcFormatChooser, common_region_listbox);
        gtk_widget_class_bind_template_child (widget_class, CcFormatChooser, region_title);
        gtk_widget_class_bind_template_child (widget_class, CcFormatChooser, region_listbox);
        gtk_widget_class_bind_template_child (widget_class, CcFormatChooser, region_list_stack);
        gtk_widget_class_bind_template_child (widget_class, CcFormatChooser, format_preview);
        gtk_widget_class_bind_template_child (widget_class, CcFormatChooser, preview_title_label);

        gtk_widget_class_bind_template_callback (widget_class, format_chooser_close_sidebar_button_pressed_cb);
        gtk_widget_class_bind_template_callback (widget_class, select_button_clicked_cb);
        gtk_widget_class_bind_template_callback (widget_class, filter_changed);
        gtk_widget_class_bind_template_callback (widget_class, row_activated);
        gtk_widget_class_bind_template_callback (widget_class, on_stop_search);
        gtk_widget_class_bind_template_callback (widget_class, collapsed_cb);
}

void
cc_format_chooser_init (CcFormatChooser *chooser)
{
        gtk_widget_init_template (GTK_WIDGET (chooser));

        gtk_list_box_set_sort_func (GTK_LIST_BOX (chooser->common_region_listbox),
                                    (GtkListBoxSortFunc)sort_regions, chooser, NULL);
        gtk_list_box_set_sort_func (GTK_LIST_BOX (chooser->region_listbox),
                                    (GtkListBoxSortFunc)sort_regions, chooser, NULL);
        gtk_list_box_set_filter_func (GTK_LIST_BOX (chooser->region_listbox),
                                      region_visible, chooser, NULL);

        add_all_regions (chooser);
        gtk_list_box_invalidate_filter (GTK_LIST_BOX (chooser->region_listbox));
}

CcFormatChooser *
cc_format_chooser_new (void)
{
        return g_object_new (CC_TYPE_FORMAT_CHOOSER, NULL);
}

void
cc_format_chooser_clear_filter (CcFormatChooser *chooser)
{
        g_return_if_fail (CC_IS_FORMAT_CHOOSER (chooser));
        gtk_editable_set_text (GTK_EDITABLE (chooser->region_filter_entry), "");
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
