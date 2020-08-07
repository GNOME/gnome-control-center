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
#include <handy.h>

#include "list-box-helper.h"
#include "cc-common-language.h"
#include "cc-util.h"

#define GNOME_DESKTOP_USE_UNSTABLE_API
#include <libgnome-desktop/gnome-languages.h>

struct _CcFormatChooser {
        GtkDialog parent_instance;

        GtkWidget *title_bar;
        GtkWidget *title_buttons;
        GtkWidget *cancel_button;
        GtkWidget *back_button;
        GtkWidget *done_button;
        GtkWidget *empty_results_view;
        GtkWidget *main_leaflet;
        GtkWidget *region_filter_entry;
        GtkWidget *region_list;
        GtkWidget *region_list_stack;
        GtkWidget *common_region_title;
        GtkWidget *common_region_listbox;
        GtkWidget *region_title;
        GtkWidget *region_listbox;
        GtkWidget *date_format_label;
        GtkWidget *time_format_label;
        GtkWidget *date_time_format_label;
        GtkWidget *number_format_label;
        GtkWidget *measurement_format_label;
        GtkWidget *paper_format_label;
        gboolean adding;
        gboolean showing_extra;
        gboolean no_results;
        gchar *region;
        gchar *preview_region;
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
update_format_examples (CcFormatChooser *chooser,
                        const gchar     *region)
{
        locale_t locale;
        locale_t old_locale;
        g_autoptr(GDateTime) dt = NULL;
        g_autofree gchar *s = NULL;
        const gchar *fmt;
        g_autoptr(GtkPaperSize) paper = NULL;

        if (!region || !*region)
                return;

        locale = newlocale (LC_TIME_MASK, region, (locale_t) 0);
        if (locale == (locale_t) 0)
                g_warning ("Failed to create locale %s: %s", region, g_strerror (errno));
        else
                old_locale = uselocale (locale);

        dt = g_date_time_new_now_local ();
        display_date (chooser->date_format_label, dt, "%x");
        display_date (chooser->time_format_label, dt, "%X");
        display_date (chooser->date_time_format_label, dt, "%c");

        if (locale != (locale_t) 0) {
                uselocale (old_locale);
                freelocale (locale);
        }

        locale = newlocale (LC_NUMERIC_MASK, region, (locale_t) 0);
        if (locale == (locale_t) 0)
                g_warning ("Failed to create locale %s: %s", region, g_strerror (errno));
        else
                old_locale = uselocale (locale);

        s = g_strdup_printf ("%'.2f", 123456789.00);
        gtk_label_set_text (GTK_LABEL (chooser->number_format_label), s);

        if (locale != (locale_t) 0) {
                uselocale (old_locale);
                freelocale (locale);
        }

#if 0
        locale = newlocale (LC_MONETARY_MASK, region, (locale_t) 0);
        if (locale == (locale_t) 0)
                g_warning ("Failed to create locale %s: %s", region, g_strerror (errno));
        else
                old_locale = uselocale (locale);

        num_info = localeconv ();
        if (num_info != NULL)
                gtk_label_set_text (GTK_LABEL (chooser->currency_format_label), num_info->currency_symbol);

        if (locale != (locale_t) 0) {
                uselocale (old_locale);
                freelocale (locale);
        }
#endif

#ifdef LC_MEASUREMENT
        locale = newlocale (LC_MEASUREMENT_MASK, region, (locale_t) 0);
        if (locale == (locale_t) 0)
                g_warning ("Failed to create locale %s: %s", region, g_strerror (errno));
        else
                old_locale = uselocale (locale);

        fmt = nl_langinfo (_NL_MEASUREMENT_MEASUREMENT);
        if (fmt && *fmt == 2)
                gtk_label_set_text (GTK_LABEL (chooser->measurement_format_label), C_("measurement format", "Imperial"));
        else
                gtk_label_set_text (GTK_LABEL (chooser->measurement_format_label), C_("measurement format", "Metric"));

        if (locale != (locale_t) 0) {
                uselocale (old_locale);
                freelocale (locale);
        }
#endif

#ifdef LC_PAPER
        locale = newlocale (LC_PAPER_MASK, region, (locale_t) 0);
        if (locale == (locale_t) 0)
                g_warning ("Failed to create locale %s: %s", region, g_strerror (errno));
        else
                old_locale = uselocale (locale);

        paper = gtk_paper_size_new (gtk_paper_size_get_default ());
        gtk_label_set_text (GTK_LABEL (chooser->paper_format_label), gtk_paper_size_get_display_name (paper));

        if (locale != (locale_t) 0) {
                uselocale (old_locale);
                freelocale (locale);
        }
#endif
}

static void
update_check_button_for_list (GtkWidget   *list_box,
                              const gchar *locale_id)
{
  g_autoptr(GList) children = NULL;
  GList *l;

  g_assert (GTK_IS_CONTAINER (list_box));

  children = gtk_container_get_children (GTK_CONTAINER (list_box));
  for (l = children; l; l = l->next)
    {
      GtkWidget *row = l->data;
      GtkWidget *check = g_object_get_data (G_OBJECT (row), "check");
      const gchar *region = g_object_get_data (G_OBJECT (row), "locale-id");
      if (check == NULL || region == NULL)
        continue;

      if (g_strcmp0 (locale_id, region) == 0)
        gtk_widget_set_opacity (check, 1.0);
      else
        gtk_widget_set_opacity (check, 0.0);
    }
}

static void
set_locale_id (CcFormatChooser *chooser,
               const gchar     *locale_id)
{
        g_free (chooser->region);
        chooser->region = g_strdup (locale_id);

        update_check_button_for_list (chooser->region_listbox, locale_id);
        update_check_button_for_list (chooser->common_region_listbox, locale_id);
        update_format_examples (chooser, locale_id);
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
padded_label_new (const char *text)
{
        GtkWidget *widget, *label;

        widget = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 10);
        g_object_set (widget, "margin-top", 4, NULL);
        g_object_set (widget, "margin-bottom", 4, NULL);
        g_object_set (widget, "margin-start", 10, NULL);
        g_object_set (widget, "margin-end", 10, NULL);

        label = gtk_label_new (text);
        gtk_label_set_ellipsize (GTK_LABEL (label), PANGO_ELLIPSIZE_END);
        gtk_widget_show (label);
        gtk_container_add (GTK_CONTAINER (widget), label);

        return widget;
}

static void
format_chooser_back_button_clicked_cb (CcFormatChooser *self)
{
  g_assert (CC_IS_FORMAT_CHOOSER (self));

  gtk_header_bar_set_title (GTK_HEADER_BAR (self->title_bar), _("Formats"));
  hdy_leaflet_set_visible_child_name (HDY_LEAFLET (self->main_leaflet), "region-list");
  gtk_stack_set_visible_child (GTK_STACK (self->title_buttons), self->cancel_button);
  gtk_widget_show (self->done_button);
}

static void
cc_format_chooser_preview_button_set_visible (GtkListBoxRow *row,
                                              gpointer       user_data)
{
  GtkWidget *button;
  gboolean visible;

  g_assert (GTK_IS_LIST_BOX_ROW (row));

  visible = GPOINTER_TO_INT (user_data);
  button = g_object_get_data (G_OBJECT (row), "preview-button");
  g_assert (button);

  gtk_widget_set_opacity (button, visible);
  gtk_widget_set_sensitive (button, visible);
}

static void
format_chooser_leaflet_fold_changed_cb (CcFormatChooser *self)
{
  gboolean folded;

  g_assert (CC_IS_FORMAT_CHOOSER (self));

  folded = hdy_leaflet_get_folded (HDY_LEAFLET (self->main_leaflet));
  gtk_container_foreach (GTK_CONTAINER (self->common_region_listbox),
                         (GtkCallback)cc_format_chooser_preview_button_set_visible,
                         GINT_TO_POINTER (folded));
  gtk_container_foreach (GTK_CONTAINER (self->region_listbox),
                         (GtkCallback)cc_format_chooser_preview_button_set_visible,
                         GINT_TO_POINTER (folded));

  if (!folded)
    {
      update_format_examples (self, self->region);
      gtk_header_bar_set_title (GTK_HEADER_BAR (self->title_bar), _("Formats"));
      hdy_leaflet_set_visible_child_name (HDY_LEAFLET (self->main_leaflet), "region-list");
      gtk_stack_set_visible_child (GTK_STACK (self->title_buttons), self->cancel_button);
      gtk_widget_show (self->done_button);
    }
}

static void
preview_button_clicked_cb (CcFormatChooser *self,
                           GtkWidget       *button)
{
  GtkWidget *row;
  const gchar *region, *locale_name;

  g_assert (CC_IS_FORMAT_CHOOSER (self));
  g_assert (GTK_IS_WIDGET (button));

  row = gtk_widget_get_ancestor (button, GTK_TYPE_LIST_BOX_ROW);
  g_assert (row);

  region = g_object_get_data (G_OBJECT (row), "locale-id");
  locale_name = g_object_get_data (G_OBJECT (row), "locale-name");
  update_format_examples (self, region);

  hdy_leaflet_set_visible_child_name (HDY_LEAFLET (self->main_leaflet), "preview");
  gtk_stack_set_visible_child (GTK_STACK (self->title_buttons), self->back_button);
  gtk_widget_hide (self->done_button);

  if (locale_name)
    gtk_header_bar_set_title (GTK_HEADER_BAR (self->title_bar), locale_name);
}

static GtkWidget *
region_widget_new (CcFormatChooser *self,
                   const gchar     *locale_id)
{
        gchar *locale_name;
        gchar *locale_current_name;
        gchar *locale_untranslated_name;
        GtkWidget *row, *box, *button;
        GtkWidget *check;

        locale_name = gnome_get_country_from_locale (locale_id, locale_id);
        if (!locale_name)
          return NULL;

        locale_current_name = gnome_get_country_from_locale (locale_id, NULL);
        locale_untranslated_name = gnome_get_country_from_locale (locale_id, "C");

        row = gtk_list_box_row_new ();
        gtk_widget_show (row);
        box = padded_label_new (locale_name);
        gtk_widget_show (box);
        gtk_container_add (GTK_CONTAINER (row), box);

        button = gtk_button_new_from_icon_name ("view-layout-symbolic", GTK_ICON_SIZE_BUTTON);
        g_signal_connect_object (button, "clicked", G_CALLBACK (preview_button_clicked_cb),
                                 self, G_CONNECT_SWAPPED);
        gtk_widget_show (button);
        gtk_box_pack_end (GTK_BOX (box), button, FALSE, TRUE, 0);

        check = gtk_image_new ();
        gtk_widget_show (check);
        gtk_image_set_from_icon_name (GTK_IMAGE (check), "object-select-symbolic", GTK_ICON_SIZE_MENU);
        gtk_widget_set_opacity (check, 0.0);
        g_object_set (check, "icon-size", GTK_ICON_SIZE_MENU, NULL);
        gtk_container_add (GTK_CONTAINER (box), check);

        g_object_set_data (G_OBJECT (row), "check", check);
        g_object_set_data (G_OBJECT (row), "preview-button", button);
        g_object_set_data_full (G_OBJECT (row), "locale-id", g_strdup (locale_id), g_free);
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
        GtkWidget *widget;
        GList *l;

        chooser->adding = TRUE;
        initial_locales = g_hash_table_get_keys (initial);

        /* Populate Common Locales */
        for (l = initial_locales; l != NULL; l = l->next) {
                if (!cc_common_language_has_font (l->data))
                        continue;

                widget = region_widget_new (chooser, l->data);
                if (!widget)
                        continue;

                gtk_widget_show (widget);
                gtk_container_add (GTK_CONTAINER (chooser->common_region_listbox), widget);
          }

        /* Populate All locales */
        while (*locale_ids) {
                gchar *locale_id;

                locale_id = *locale_ids;
                locale_ids ++;

                if (!cc_common_language_has_font (locale_id))
                        continue;

                widget = region_widget_new (chooser, locale_id);
                if (!widget)
                  continue;

                gtk_widget_show (widget);
                gtk_container_add (GTK_CONTAINER (chooser->region_listbox), widget);
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
                cc_util_normalize_casefold_and_unaccent (gtk_entry_get_text (GTK_ENTRY (chooser->region_filter_entry)));

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
          gtk_stack_set_visible_child (GTK_STACK (chooser->region_list_stack),
                                       GTK_WIDGET (chooser->empty_results_view));
        else
          gtk_stack_set_visible_child (GTK_STACK (chooser->region_list_stack),
                                       GTK_WIDGET (chooser->region_list));
}

static void
row_activated (CcFormatChooser *chooser,
               GtkListBoxRow   *row)
{
        const gchar *new_locale_id;

        if (chooser->adding)
                return;

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

        gtk_widget_class_bind_template_child (widget_class, CcFormatChooser, title_bar);
        gtk_widget_class_bind_template_child (widget_class, CcFormatChooser, title_buttons);
        gtk_widget_class_bind_template_child (widget_class, CcFormatChooser, cancel_button);
        gtk_widget_class_bind_template_child (widget_class, CcFormatChooser, back_button);
        gtk_widget_class_bind_template_child (widget_class, CcFormatChooser, done_button);
        gtk_widget_class_bind_template_child (widget_class, CcFormatChooser, main_leaflet);
        gtk_widget_class_bind_template_child (widget_class, CcFormatChooser, region_filter_entry);
        gtk_widget_class_bind_template_child (widget_class, CcFormatChooser, common_region_title);
        gtk_widget_class_bind_template_child (widget_class, CcFormatChooser, common_region_listbox);
        gtk_widget_class_bind_template_child (widget_class, CcFormatChooser, region_title);
        gtk_widget_class_bind_template_child (widget_class, CcFormatChooser, region_listbox);
        gtk_widget_class_bind_template_child (widget_class, CcFormatChooser, date_format_label);
        gtk_widget_class_bind_template_child (widget_class, CcFormatChooser, time_format_label);
        gtk_widget_class_bind_template_child (widget_class, CcFormatChooser, date_time_format_label);
        gtk_widget_class_bind_template_child (widget_class, CcFormatChooser, number_format_label);
        gtk_widget_class_bind_template_child (widget_class, CcFormatChooser, measurement_format_label);
        gtk_widget_class_bind_template_child (widget_class, CcFormatChooser, paper_format_label);
        gtk_widget_class_bind_template_child (widget_class, CcFormatChooser, region_list);
        gtk_widget_class_bind_template_child (widget_class, CcFormatChooser, region_list_stack);
        gtk_widget_class_bind_template_child (widget_class, CcFormatChooser, empty_results_view);

        gtk_widget_class_bind_template_callback (widget_class, format_chooser_back_button_clicked_cb);
        gtk_widget_class_bind_template_callback (widget_class, format_chooser_leaflet_fold_changed_cb);
        gtk_widget_class_bind_template_callback (widget_class, filter_changed);
        gtk_widget_class_bind_template_callback (widget_class, row_activated);
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
        gtk_list_box_set_header_func (GTK_LIST_BOX (chooser->region_listbox),
                                      cc_list_box_update_header_func, NULL, NULL);
        gtk_list_box_set_header_func (GTK_LIST_BOX (chooser->common_region_listbox),
                                      cc_list_box_update_header_func, NULL, NULL);

        add_all_regions (chooser);
        gtk_list_box_invalidate_filter (GTK_LIST_BOX (chooser->region_listbox));
        format_chooser_leaflet_fold_changed_cb (chooser);

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
