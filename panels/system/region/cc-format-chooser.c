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
#include "cc-format-preview.h"
#include "cc-locale-row.h"
#include "cc-util.h"

#define GNOME_DESKTOP_USE_UNSTABLE_API
#include <libgnome-desktop/gnome-languages.h>

struct _CcFormatChooser {
  AdwDialog parent_instance;

  AdwOverlaySplitView *split_view;
  GtkSearchEntry *region_filter_entry;
  GtkStack *region_list_stack;
  AdwPreferencesGroup *common_region_group;
  GtkListBox *common_region_listbox;
  AdwPreferencesGroup *region_group;
  GtkListBox *region_listbox;
  GtkWidget *close_sidebar_button;
  GtkLabel *preview_title_label;
  CcFormatPreview *format_preview;
  gboolean adding;
  gboolean showing_extra;
  gboolean no_results;
  gchar *region;
  gchar *region_group_title;
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
update_check_button_for_list (GtkListBox  *list_box,
                              const gchar *locale_id)
{
  GtkWidget *child;
  const gchar *row_locale_id;

  for (child = gtk_widget_get_first_child (GTK_WIDGET (list_box));
       child;
       child = gtk_widget_get_next_sibling (child))
    {
      if (!CC_IS_LOCALE_ROW (child))
        continue;

      row_locale_id = cc_locale_row_get_locale_id (CC_LOCALE_ROW (child));
      if (!row_locale_id)
        continue;

      if (g_strcmp0 (locale_id, row_locale_id) != 0)
        continue;

      gtk_list_box_select_row (list_box, GTK_LIST_BOX_ROW (child));
    }
}

static void
set_locale_id (CcFormatChooser *self,
               const gchar     *locale_id)
{
        g_autofree gchar *locale_name = NULL;

        g_free (self->region);
        self->region = g_strdup (locale_id);

        update_check_button_for_list (self->region_listbox, locale_id);
        update_check_button_for_list (self->common_region_listbox, locale_id);
        cc_format_preview_set_region (self->format_preview, locale_id);

        locale_name = gnome_get_country_from_locale (locale_id, locale_id);
        gtk_label_set_label (self->preview_title_label, locale_name);
}

static gint
sort_regions (GtkListBoxRow* a,
              GtkListBoxRow* b,
              gpointer       user_data)
{
        CcLocaleRow *row1 = CC_LOCALE_ROW (a);
        CcLocaleRow *row2 = CC_LOCALE_ROW (b);
        const gchar *la;
        const gchar *lb;

        if (!cc_locale_row_get_locale_id (row1))
                return 1;
        if (!cc_locale_row_get_locale_id (row2))
                return -1;

        la = cc_locale_row_get_country (row1);
        lb = cc_locale_row_get_country (row2);

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

    if (!adw_overlay_split_view_get_collapsed (self->split_view)) {
        g_autofree gchar *locale_name = NULL;

        cc_format_preview_set_region (self->format_preview, self->region);
        locale_name = gnome_get_country_from_locale (self->region, self->region);
        gtk_label_set_label (self->preview_title_label, locale_name);
    }
}

static void
format_chooser_close_sidebar_button_pressed_cb (CcFormatChooser *self)
{
  adw_overlay_split_view_set_show_sidebar (self->split_view, FALSE);
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

  row = gtk_widget_get_ancestor (button, CC_TYPE_LOCALE_ROW);
  g_assert (row);

  region = cc_locale_row_get_locale_id (CC_LOCALE_ROW (row));
  cc_format_preview_set_region (self->format_preview, region);
  locale_name = cc_locale_row_get_country (CC_LOCALE_ROW (row));
  gtk_label_set_label (self->preview_title_label, locale_name);

  adw_overlay_split_view_set_show_sidebar (self->split_view, TRUE);
}

static CcLocaleRow *
region_widget_new (CcFormatChooser *self,
                   const gchar     *locale_id)
{
        gchar *locale_name;
        gchar *locale_untranslated_name;
        GtkWidget *button;
        CcLocaleRow *row;

        locale_name = gnome_get_country_from_locale (locale_id, locale_id);
        if (!locale_name)
          return NULL;

        locale_untranslated_name = gnome_get_country_from_locale (locale_id, "C");

        row = cc_locale_row_new (locale_id, CC_LOCALE_LAYOUT_TYPE_REGION);

        button = gtk_button_new_from_icon_name ("view-reveal-symbolic");
        gtk_widget_set_tooltip_text (button, _("Preview"));
        gtk_widget_add_css_class (button, "flat");
        gtk_widget_set_valign (button, GTK_ALIGN_CENTER);
        g_signal_connect_object (button, "clicked", G_CALLBACK (preview_button_clicked_cb),
                                 self, G_CONNECT_SWAPPED);
        g_object_bind_property (self->split_view, "collapsed",
                                button, "visible",
                                G_BINDING_SYNC_CREATE);

        cc_locale_row_add_suffix (row, button);

        g_object_set_data_full (G_OBJECT (row), "locale-untranslated-name", locale_untranslated_name, g_free);

        return row;
}

static void
add_regions (CcFormatChooser *self,
             gchar          **locale_ids,
             GHashTable      *initial)
{
        g_autoptr(GList) initial_locales = NULL;
        CcLocaleRow *row;
        GList *l;

        self->adding = TRUE;
        initial_locales = g_hash_table_get_keys (initial);

        /* Populate Common Locales */
        for (l = initial_locales; l != NULL; l = l->next) {
                if (!cc_common_language_has_font (l->data))
                        continue;

                row = region_widget_new (self, l->data);
                if (!row)
                        continue;

                gtk_list_box_append (self->common_region_listbox, GTK_WIDGET (row));
          }

        /* Populate All locales */
        while (*locale_ids) {
                gchar *locale_id;

                locale_id = *locale_ids;
                locale_ids ++;

                if (!cc_common_language_has_font (locale_id))
                        continue;

                row = region_widget_new (self, locale_id);
                if (!row)
                  continue;

                gtk_list_box_append (self->region_listbox, GTK_WIDGET (row));
        }

        self->adding = FALSE;
}

static void
add_all_regions (CcFormatChooser *self)
{
        g_auto(GStrv) locale_ids = NULL;
        g_autoptr(GHashTable) initial = NULL;

        locale_ids = gnome_get_all_locales ();
        initial = cc_common_language_get_initial_languages ();
        add_regions (self, locale_ids, initial);
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
        CcFormatChooser *self = user_data;
        g_autofree gchar *locale_name = NULL;
        g_autofree gchar *locale_current_name = NULL;
        g_autofree gchar *locale_untranslated_name = NULL;
        gboolean match = TRUE;

        if (!self->filter_words)
          goto end;

        locale_name =
                cc_util_normalize_casefold_and_unaccent (cc_locale_row_get_country (CC_LOCALE_ROW (row)));
        if (match_all (self->filter_words, locale_name))
          goto end;

        locale_current_name =
                cc_util_normalize_casefold_and_unaccent (cc_locale_row_get_country_local (CC_LOCALE_ROW (row)));
        if (match_all (self->filter_words, locale_current_name))
          goto end;

        locale_untranslated_name =
                cc_util_normalize_casefold_and_unaccent (g_object_get_data (G_OBJECT (row), "locale-untranslated-name"));

        match = match_all (self->filter_words, locale_untranslated_name);

 end:
        if (match)
          self->no_results = FALSE;
        return match;
}

static void
filter_changed (CcFormatChooser *self)
{
        g_autofree gchar *filter_contents = NULL;
        gboolean search_is_empty;

        g_clear_pointer (&self->filter_words, g_strfreev);

        filter_contents =
                cc_util_normalize_casefold_and_unaccent (gtk_editable_get_text (GTK_EDITABLE (self->region_filter_entry)));

        /* The popular listbox and the region group title are shown only if search is empty */
        search_is_empty = filter_contents == NULL || *filter_contents == '\0';
        gtk_widget_set_visible (GTK_WIDGET (self->common_region_group), search_is_empty);
        if (search_is_empty)
                adw_preferences_group_set_title (self->region_group, self->region_group_title);
        else
                adw_preferences_group_set_title (self->region_group, "");

        /* Reset cached search state */
        self->no_results = TRUE;

        if (!filter_contents) {
                gtk_list_box_invalidate_filter (self->region_listbox);
                gtk_list_box_set_placeholder (self->region_listbox, NULL);
                return;
        }
        self->filter_words = g_strsplit_set (g_strstrip (filter_contents), " ", 0);
        gtk_list_box_invalidate_filter (self->region_listbox);

        if (self->no_results)
          gtk_stack_set_visible_child_name (self->region_list_stack, "empty_results_page");
        else
          gtk_stack_set_visible_child_name (self->region_list_stack, "region_list_page");
}

static void
row_activated (CcFormatChooser *self,
               CcLocaleRow *row)
{
        const gchar *new_locale_id;

        if (self->adding)
                return;

        new_locale_id = cc_locale_row_get_locale_id (row);
        if (g_strcmp0 (new_locale_id, self->region) == 0)
                g_signal_emit (self, signals[LANGUAGE_SELECTED], 0);
        else
                set_locale_id (self, new_locale_id);
}

static void
select_button_clicked_cb (CcFormatChooser *self)
{
        g_signal_emit (self, signals[LANGUAGE_SELECTED], 0);
}

static void
cc_format_chooser_dispose (GObject *object)
{
        CcFormatChooser *self = CC_FORMAT_CHOOSER (object);

        g_clear_pointer (&self->filter_words, g_strfreev);
        g_clear_pointer (&self->region, g_free);
        g_clear_pointer (&self->region_group_title, g_free);

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
        gtk_widget_class_bind_template_child (widget_class, CcFormatChooser, common_region_group);
        gtk_widget_class_bind_template_child (widget_class, CcFormatChooser, common_region_listbox);
        gtk_widget_class_bind_template_child (widget_class, CcFormatChooser, region_group);
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
cc_format_chooser_init (CcFormatChooser *self)
{
        gtk_widget_init_template (GTK_WIDGET (self));

        gtk_list_box_set_sort_func (self->common_region_listbox, sort_regions, self, NULL);
        gtk_list_box_set_sort_func (self->region_listbox, sort_regions, self, NULL);
        gtk_list_box_set_filter_func (self->region_listbox, region_visible, self, NULL);

        add_all_regions (self);
        gtk_list_box_invalidate_filter (self->region_listbox);

        /* Store group title so we can hide it during search */
        g_set_str (&self->region_group_title, adw_preferences_group_get_title (self->region_group));
}

CcFormatChooser *
cc_format_chooser_new (void)
{
        return g_object_new (CC_TYPE_FORMAT_CHOOSER, NULL);
}

void
cc_format_chooser_clear_filter (CcFormatChooser *self)
{
        g_return_if_fail (CC_IS_FORMAT_CHOOSER (self));
        gtk_editable_set_text (GTK_EDITABLE (self->region_filter_entry), "");
}

const gchar *
cc_format_chooser_get_region (CcFormatChooser *self)
{
        g_return_val_if_fail (CC_IS_FORMAT_CHOOSER (self), NULL);
        return self->region;
}

void
cc_format_chooser_set_region (CcFormatChooser *self,
                              const gchar     *region)
{
        g_return_if_fail (CC_IS_FORMAT_CHOOSER (self));
        set_locale_id (self, region);
}
