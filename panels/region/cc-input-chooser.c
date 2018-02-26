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
 */

#include <config.h>
#include <locale.h>
#include <glib/gi18n.h>

#define GNOME_DESKTOP_USE_UNSTABLE_API
#include <libgnome-desktop/gnome-languages.h>

#include "list-box-helper.h"
#include "cc-common-language.h"
#include "cc-util.h"
#include "cc-input-chooser.h"

#ifdef HAVE_IBUS
#include <ibus.h>
#include "cc-ibus-utils.h"
#endif  /* HAVE_IBUS */

#define INPUT_SOURCE_TYPE_XKB "xkb"
#define INPUT_SOURCE_TYPE_IBUS "ibus"

#define MAIN_WINDOW_WIDTH_RATIO 0.60
#define FILTER_TIMEOUT 150 /* ms */

typedef enum {
  ROW_TRAVEL_DIRECTION_NONE,
  ROW_TRAVEL_DIRECTION_FORWARD,
  ROW_TRAVEL_DIRECTION_BACKWARD
} RowTravelDirection;

typedef enum {
  ROW_LABEL_POSITION_START,
  ROW_LABEL_POSITION_CENTER,
  ROW_LABEL_POSITION_END
} RowLabelPosition;

typedef struct {
  /* Not owned */
  GtkWidget *add_button;
  GtkWidget *filter_entry;
  GtkWidget *list;
  GtkWidget *scrolledwindow;
  GtkAdjustment *adjustment;
  GnomeXkbInfo *xkb_info;
  GHashTable *ibus_engines;

  /* Owned */
  GtkListBoxRow *more_row;
  GtkWidget *no_results;
  GHashTable *locales;
  GHashTable *locales_by_language;
  gboolean showing_extra;
  guint filter_timeout_id;
  gchar **filter_words;

  gboolean is_login;
} CcInputChooserPrivate;

#define GET_PRIVATE(chooser) ((CcInputChooserPrivate *) g_object_get_data (G_OBJECT (chooser), "private"))
#define WID(name) ((GtkWidget *) gtk_builder_get_object (builder, name))

typedef struct {
  gchar *id;
  gchar *name;
  gchar *unaccented_name;
  gchar *untranslated_name;
  GtkListBoxRow *default_input_source_row;
  GtkListBoxRow *locale_row;
  GtkListBoxRow *back_row;
  GHashTable *layout_rows_by_id;
  GHashTable *engine_rows_by_id;
} LocaleInfo;

static void
locale_info_free (gpointer data)
{
  LocaleInfo *info = data;

  g_free (info->id);
  g_free (info->name);
  g_free (info->unaccented_name);
  g_free (info->untranslated_name);
  g_clear_object (&info->default_input_source_row);
  g_clear_object (&info->locale_row);
  g_clear_object (&info->back_row);
  g_hash_table_destroy (info->layout_rows_by_id);
  g_hash_table_destroy (info->engine_rows_by_id);
  g_free (info);
}

static void
set_row_widget_margins (GtkWidget *widget)
{
  gtk_widget_set_margin_start (widget, 20);
  gtk_widget_set_margin_end (widget, 20);
  gtk_widget_set_margin_top (widget, 6);
  gtk_widget_set_margin_bottom (widget, 6);
}

static GtkWidget *
padded_label_new (const gchar        *text,
                  RowLabelPosition    position,
                  RowTravelDirection  direction,
                  gboolean            dim_label)
{
  GtkWidget *widget;
  GtkWidget *label;
  GtkWidget *arrow;
  GtkAlign alignment;

  if (position == ROW_LABEL_POSITION_START)
    alignment = GTK_ALIGN_START;
  else if (position == ROW_LABEL_POSITION_CENTER)
    alignment = GTK_ALIGN_CENTER;
  else
    alignment = GTK_ALIGN_END;

  widget = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);

  if (direction == ROW_TRAVEL_DIRECTION_BACKWARD)
    {
      arrow = gtk_image_new_from_icon_name ("go-previous-symbolic", GTK_ICON_SIZE_MENU);
      gtk_box_pack_start (GTK_BOX (widget), arrow, FALSE, TRUE, 0);
    }

  label = gtk_label_new (text);
  gtk_label_set_ellipsize (GTK_LABEL (label), PANGO_ELLIPSIZE_MIDDLE);
  gtk_widget_set_halign (label, alignment);
  set_row_widget_margins (label);
  gtk_box_pack_start (GTK_BOX (widget), label, TRUE, TRUE, 0);
  if (dim_label)
    gtk_style_context_add_class (gtk_widget_get_style_context (label), "dim-label");

  if (direction == ROW_TRAVEL_DIRECTION_FORWARD)
    {
      arrow = gtk_image_new_from_icon_name ("go-next-symbolic", GTK_ICON_SIZE_MENU);
      gtk_box_pack_start (GTK_BOX (widget), arrow, FALSE, TRUE, 0);
    }

  return widget;
}

static GtkListBoxRow *
more_row_new (void)
{
  GtkWidget *row;
  GtkWidget *box;
  GtkWidget *arrow;

  row = gtk_list_box_row_new ();
  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_container_add (GTK_CONTAINER (row), box);
  gtk_widget_set_tooltip_text (row, _("More…"));

  arrow = gtk_image_new_from_icon_name ("view-more-symbolic", GTK_ICON_SIZE_MENU);
  gtk_style_context_add_class (gtk_widget_get_style_context (arrow), "dim-label");
  set_row_widget_margins (arrow);
  gtk_box_pack_start (GTK_BOX (box), arrow, TRUE, TRUE, 0);

  return GTK_LIST_BOX_ROW (row);
}

static GtkWidget *
no_results_widget_new (void)
{
  return padded_label_new (_("No input sources found"), ROW_LABEL_POSITION_CENTER, ROW_TRAVEL_DIRECTION_NONE, TRUE);
}

static GtkListBoxRow *
back_row_new (const gchar *text)
{
  GtkWidget *row;
  GtkWidget *widget;

  row = gtk_list_box_row_new ();
  widget = padded_label_new (text, ROW_LABEL_POSITION_CENTER, ROW_TRAVEL_DIRECTION_BACKWARD, TRUE);
  gtk_container_add (GTK_CONTAINER (row), widget);

  return GTK_LIST_BOX_ROW (row);
}

static GtkListBoxRow *
locale_row_new (const gchar *text)
{
  GtkWidget *row;
  GtkWidget *widget;

  row = gtk_list_box_row_new ();
  widget = padded_label_new (text, ROW_LABEL_POSITION_CENTER, ROW_TRAVEL_DIRECTION_NONE, FALSE);
  gtk_container_add (GTK_CONTAINER (row), widget);

  return GTK_LIST_BOX_ROW (row);
}

static GtkListBoxRow *
input_source_row_new (GtkWidget   *chooser,
                         const gchar *type,
                         const gchar *id)
{
  CcInputChooserPrivate *priv = GET_PRIVATE (chooser);
  GtkWidget *row = NULL;
  GtkWidget *widget;

  if (g_str_equal (type, INPUT_SOURCE_TYPE_XKB))
    {
      const gchar *display_name;

      gnome_xkb_info_get_layout_info (priv->xkb_info, id, &display_name, NULL, NULL, NULL);

      row = gtk_list_box_row_new ();
      widget = padded_label_new (display_name,
                                 ROW_LABEL_POSITION_START,
                                 ROW_TRAVEL_DIRECTION_NONE,
                                 FALSE);
      gtk_container_add (GTK_CONTAINER (row), widget);
      g_object_set_data (G_OBJECT (row), "name", (gpointer) display_name);
      g_object_set_data_full (G_OBJECT (row), "unaccented-name",
                              cc_util_normalize_casefold_and_unaccent (display_name), g_free);
    }
  else if (g_str_equal (type, INPUT_SOURCE_TYPE_IBUS))
    {
#ifdef HAVE_IBUS
      gchar *display_name;
      GtkWidget *image;

      display_name = engine_get_display_name (g_hash_table_lookup (priv->ibus_engines, id));

      row = gtk_list_box_row_new ();
      widget = padded_label_new (display_name,
                                 ROW_LABEL_POSITION_START,
                                 ROW_TRAVEL_DIRECTION_NONE,
                                 FALSE);
      gtk_container_add (GTK_CONTAINER (row), widget);
      image = gtk_image_new_from_icon_name ("system-run-symbolic", GTK_ICON_SIZE_MENU);
      set_row_widget_margins (image);
      gtk_style_context_add_class (gtk_widget_get_style_context (image), "dim-label");
      gtk_box_pack_start (GTK_BOX (widget), image, FALSE, TRUE, 0);

      g_object_set_data_full (G_OBJECT (row), "name", display_name, g_free);
      g_object_set_data_full (G_OBJECT (row), "unaccented-name",
                              cc_util_normalize_casefold_and_unaccent (display_name), g_free);
#else
      widget = NULL;
#endif  /* HAVE_IBUS */
    }

  if (row)
    {
      g_object_set_data (G_OBJECT (row), "type", (gpointer) type);
      g_object_set_data (G_OBJECT (row), "id", (gpointer) id);

      return GTK_LIST_BOX_ROW (row);
    }

  return NULL;
}

static void
remove_all_children (GtkContainer *container)
{
  g_autoptr(GList) list = NULL;
  GList *l;

  list = gtk_container_get_children (container);
  for (l = list; l; l = l->next)
    gtk_container_remove (container, (GtkWidget *) l->data);
}

static void
set_fixed_size (GtkWidget *chooser)
{
  CcInputChooserPrivate *priv = GET_PRIVATE (chooser);
  GtkPolicyType policy;
  gint width, height;

  gtk_scrolled_window_get_policy (GTK_SCROLLED_WINDOW (priv->scrolledwindow), &policy, NULL);
  if (policy == GTK_POLICY_AUTOMATIC)
    return;

  /* Don't let it automatically get wider than the main CC window nor
     get taller than the initial height */
  gtk_window_get_size (gtk_window_get_transient_for (GTK_WINDOW (chooser)),
                       &width, NULL);
  gtk_window_get_size (GTK_WINDOW (chooser), NULL, &height);
  gtk_widget_set_size_request (chooser, width * MAIN_WINDOW_WIDTH_RATIO, height);

  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (priv->scrolledwindow),
                                  GTK_POLICY_AUTOMATIC,
                                  GTK_POLICY_AUTOMATIC);
}

static void
add_input_source_rows_for_locale (GtkWidget  *chooser,
                                  LocaleInfo *info)
{
  CcInputChooserPrivate *priv = GET_PRIVATE (chooser);
  GtkWidget *row;
  GHashTableIter iter;
  const gchar *id;

  if (info->default_input_source_row)
    gtk_container_add (GTK_CONTAINER (priv->list), GTK_WIDGET (info->default_input_source_row));

  g_hash_table_iter_init (&iter, info->layout_rows_by_id);
  while (g_hash_table_iter_next (&iter, (gpointer *) &id, (gpointer *) &row))
    gtk_container_add (GTK_CONTAINER (priv->list), row);

  g_hash_table_iter_init (&iter, info->engine_rows_by_id);
  while (g_hash_table_iter_next (&iter, (gpointer *) &id, (gpointer *) &row))
    gtk_container_add (GTK_CONTAINER (priv->list), row);
}

static void
show_input_sources_for_locale (GtkWidget   *chooser,
                               LocaleInfo  *info)
{
  CcInputChooserPrivate *priv = GET_PRIVATE (chooser);

  set_fixed_size (chooser);

  remove_all_children (GTK_CONTAINER (priv->list));

  if (!info->back_row)
    {
      info->back_row = g_object_ref_sink (back_row_new (info->name));
      g_object_set_data (G_OBJECT (info->back_row), "back", GINT_TO_POINTER (TRUE));
      g_object_set_data (G_OBJECT (info->back_row), "locale-info", info);
    }
  gtk_container_add (GTK_CONTAINER (priv->list), GTK_WIDGET (info->back_row));

  add_input_source_rows_for_locale (chooser, info);

  gtk_widget_show_all (priv->list);

  gtk_adjustment_set_value (priv->adjustment,
                            gtk_adjustment_get_lower (priv->adjustment));
  gtk_list_box_set_header_func (GTK_LIST_BOX (priv->list), cc_list_box_update_header_func, NULL, NULL);
  gtk_list_box_invalidate_filter (GTK_LIST_BOX (priv->list));
  gtk_list_box_set_selection_mode (GTK_LIST_BOX (priv->list), GTK_SELECTION_SINGLE);
  gtk_list_box_set_activate_on_single_click (GTK_LIST_BOX (priv->list), FALSE);
  gtk_list_box_unselect_all (GTK_LIST_BOX (priv->list));

  if (gtk_widget_is_visible (priv->filter_entry) &&
      !gtk_widget_is_focus (priv->filter_entry))
    gtk_widget_grab_focus (priv->filter_entry);
}

static gboolean
is_current_locale (const gchar *locale)
{
  return g_strcmp0 (setlocale (LC_CTYPE, NULL), locale) == 0;
}

static void
show_locale_rows (GtkWidget *chooser)
{
  CcInputChooserPrivate *priv = GET_PRIVATE (chooser);
  g_autoptr(GHashTable) initial = NULL;
  LocaleInfo *info;
  GHashTableIter iter;

  remove_all_children (GTK_CONTAINER (priv->list));

  if (!priv->showing_extra)
    initial = cc_common_language_get_initial_languages ();

  g_hash_table_iter_init (&iter, priv->locales);
  while (g_hash_table_iter_next (&iter, NULL, (gpointer *) &info))
    {
      if (!info->default_input_source_row &&
          !g_hash_table_size (info->layout_rows_by_id) &&
          !g_hash_table_size (info->engine_rows_by_id))
        continue;

      if (!info->locale_row)
        {
          info->locale_row = g_object_ref_sink (locale_row_new (info->name));
          g_object_set_data (G_OBJECT (info->locale_row), "locale-info", info);

          if (!priv->showing_extra &&
              !g_hash_table_contains (initial, info->id) &&
              !is_current_locale (info->id))
            g_object_set_data (G_OBJECT (info->locale_row), "is-extra", GINT_TO_POINTER (TRUE));
        }
      gtk_container_add (GTK_CONTAINER (priv->list), GTK_WIDGET (info->locale_row));
    }

  gtk_container_add (GTK_CONTAINER (priv->list), GTK_WIDGET (priv->more_row));

  gtk_widget_show_all (priv->list);

  gtk_adjustment_set_value (priv->adjustment,
                            gtk_adjustment_get_lower (priv->adjustment));
  gtk_list_box_set_header_func (GTK_LIST_BOX (priv->list), cc_list_box_update_header_func, NULL, NULL);
  gtk_list_box_invalidate_filter (GTK_LIST_BOX (priv->list));
  gtk_list_box_set_selection_mode (GTK_LIST_BOX (priv->list), GTK_SELECTION_NONE);
  gtk_list_box_set_activate_on_single_click (GTK_LIST_BOX (priv->list), TRUE);

  if (gtk_widget_is_visible (priv->filter_entry) &&
      !gtk_widget_is_focus (priv->filter_entry))
    gtk_widget_grab_focus (priv->filter_entry);
}

static gint
list_sort (gconstpointer a,
           gconstpointer b,
           gpointer      data)
{
  GtkWidget *chooser = data;
  CcInputChooserPrivate *priv = GET_PRIVATE (chooser);
  LocaleInfo *ia;
  LocaleInfo *ib;
  const gchar *la;
  const gchar *lb;
  gint retval;

  /* Always goes at the end */
  if (a == priv->more_row)
    return 1;
  if (b == priv->more_row)
    return -1;

  ia = g_object_get_data (G_OBJECT (a), "locale-info");
  ib = g_object_get_data (G_OBJECT (b), "locale-info");

  /* The "Other" locale always goes at the end */
  if (!ia->id[0] && ib->id[0])
    return 1;
  else if (ia->id[0] && !ib->id[0])
    return -1;

  retval = g_strcmp0 (ia->name, ib->name);
  if (retval)
    return retval;

  la = g_object_get_data (G_OBJECT (a), "name");
  lb = g_object_get_data (G_OBJECT (b), "name");

  /* Only input sources have a "name" property and they should always
     go after their respective heading */
  if (la && !lb)
    return 1;
  else if (!la && lb)
    return -1;
  else if (!la && !lb)
    return 0; /* Shouldn't happen */

  /* The default input source always goes first in its group */
  if (g_object_get_data (G_OBJECT (a), "default"))
    return -1;
  if (g_object_get_data (G_OBJECT (b), "default"))
    return 1;

  return g_strcmp0 (la, lb);
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
match_source_in_table (gchar      **words,
                       GHashTable  *table)
{
  GHashTableIter iter;
  gpointer row;
  const gchar *source_name;

  g_hash_table_iter_init (&iter, table);
  while (g_hash_table_iter_next (&iter, NULL, &row))
    {
      source_name = g_object_get_data (G_OBJECT (row), "unaccented-name");
      if (source_name && match_all (words, source_name))
        return TRUE;
    }
  return FALSE;
}

static gboolean
list_filter (GtkListBoxRow *row,
             gpointer   user_data)
{
  GtkDialog *chooser = user_data;
  CcInputChooserPrivate *priv = GET_PRIVATE (chooser);
  LocaleInfo *info;
  gboolean is_extra;
  const gchar *source_name;

  if (row == priv->more_row)
    return !priv->showing_extra;

  is_extra = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (row), "is-extra"));

  if (!priv->showing_extra && is_extra)
    return FALSE;

  if (!priv->filter_words)
    return TRUE;

  info = g_object_get_data (G_OBJECT (row), "locale-info");

  if (row == info->back_row)
    return TRUE;

  if (match_all (priv->filter_words, info->unaccented_name))
    return TRUE;

  if (match_all (priv->filter_words, info->untranslated_name))
    return TRUE;

  source_name = g_object_get_data (G_OBJECT (row), "unaccented-name");
  if (source_name)
    {
      if (match_all (priv->filter_words, source_name))
        return TRUE;
    }
  else
    {
      if (match_source_in_table (priv->filter_words, info->layout_rows_by_id))
        return TRUE;
      if (match_source_in_table (priv->filter_words, info->engine_rows_by_id))
        return TRUE;
    }

  return FALSE;
}

static gboolean
strvs_differ (gchar **av,
              gchar **bv)
{
  gchar **a, **b;

  for (a = av, b = bv; *a && *b; ++a, ++b)
    if (!g_str_equal (*a, *b))
      return TRUE;

  if (*a == NULL && *b == NULL)
    return FALSE;

  return TRUE;
}

static gboolean
do_filter (GtkWidget *chooser)
{
  CcInputChooserPrivate *priv = GET_PRIVATE (chooser);
  g_autofree gchar *filter_contents = NULL;
  gboolean words_changed;

  priv->filter_timeout_id = 0;

  filter_contents =
    cc_util_normalize_casefold_and_unaccent (gtk_entry_get_text (GTK_ENTRY (priv->filter_entry)));

  words_changed = !priv->filter_words;
  if (filter_contents)
    {
      g_auto(GStrv) previous_words = priv->filter_words;
      priv->filter_words = g_strsplit_set (g_strstrip (filter_contents), " ", 0);
      if (strvs_differ (priv->filter_words, previous_words))
	words_changed = TRUE;
    }

  if (!priv->filter_words || !priv->filter_words[0])
    {
      gtk_list_box_invalidate_filter (GTK_LIST_BOX (priv->list));
      gtk_list_box_set_placeholder (GTK_LIST_BOX (priv->list), NULL);
    }
  else if (words_changed)
    {
      gtk_list_box_invalidate_filter (GTK_LIST_BOX (priv->list));
      gtk_list_box_set_placeholder (GTK_LIST_BOX (priv->list), priv->no_results);
    }

  return G_SOURCE_REMOVE;
}

static void
filter_changed (GtkWidget *chooser)
{
  CcInputChooserPrivate *priv = GET_PRIVATE (chooser);

  if (priv->filter_timeout_id == 0)
    priv->filter_timeout_id = g_timeout_add (FILTER_TIMEOUT, (GSourceFunc) do_filter, chooser);
}

static void
show_more (GtkWidget *chooser)
{
  CcInputChooserPrivate *priv = GET_PRIVATE (chooser);

  set_fixed_size (chooser);

  gtk_widget_show (priv->filter_entry);
  gtk_widget_grab_focus (priv->filter_entry);

  priv->showing_extra = TRUE;

  gtk_list_box_invalidate_filter (GTK_LIST_BOX (priv->list));
}

static void
row_activated (GtkListBox *box,
               GtkListBoxRow *row,
               GtkWidget  *chooser)
{
  CcInputChooserPrivate *priv = GET_PRIVATE (chooser);
  gpointer data;

  if (!row)
    return;

  if (row == priv->more_row)
    {
      show_more (chooser);
      return;
    }

  data = g_object_get_data (G_OBJECT (row), "back");
  if (data)
    {
      show_locale_rows (chooser);
      return;
    }

  data = g_object_get_data (G_OBJECT (row), "name");
  if (data)
    {
      if (gtk_widget_is_sensitive (priv->add_button))
        gtk_dialog_response (GTK_DIALOG (chooser),
                             gtk_dialog_get_response_for_widget (GTK_DIALOG (chooser),
                                                                 priv->add_button));
      return;
    }

  data = g_object_get_data (G_OBJECT (row), "locale-info");
  if (data)
    {
      show_input_sources_for_locale (chooser, (LocaleInfo *) data);
      return;
    }
}

static void
selected_rows_changed (GtkListBox *box,
                       GtkWidget  *chooser)
{
  CcInputChooserPrivate *priv = GET_PRIVATE (chooser);
  gboolean sensitive = TRUE;
  GtkListBoxRow *row;

  row = gtk_list_box_get_selected_row (box);
  if (!row || g_object_get_data (G_OBJECT (row), "back"))
    sensitive = FALSE;

  gtk_widget_set_sensitive (priv->add_button, sensitive);
}

static gboolean
list_button_release_event (GtkListBox *box,
                           GdkEvent   *event,
                           GtkWidget  *chooser)
{
  gdouble x, y;
  GtkListBoxRow *row;

  gdk_event_get_coords (event, &x, &y);
  row = gtk_list_box_get_row_at_y (box, y);
  if (row && g_object_get_data (G_OBJECT (row), "back"))
    {
      g_signal_emit_by_name (row, "activate", NULL);
      return TRUE;
    }

  return FALSE;
}

static void
add_default_row (GtkWidget   *chooser,
                 LocaleInfo  *info,
                 const gchar *type,
                 const gchar *id)
{
  info->default_input_source_row = input_source_row_new (chooser, type, id);
  if (info->default_input_source_row)
    {
      g_object_ref_sink (info->default_input_source_row);
      g_object_set_data (G_OBJECT (info->default_input_source_row), "default", GINT_TO_POINTER (TRUE));
      g_object_set_data (G_OBJECT (info->default_input_source_row), "locale-info", info);
    }
}

static void
add_rows_to_table (GtkWidget   *chooser,
                   LocaleInfo  *info,
                   GList       *list,
                   const gchar *type,
                   const gchar *default_id)
{
  GHashTable *table;
  GtkListBoxRow *row;
  const gchar *id;

  if (g_str_equal (type, INPUT_SOURCE_TYPE_XKB))
    table = info->layout_rows_by_id;
  else if (g_str_equal (type, INPUT_SOURCE_TYPE_IBUS))
    table = info->engine_rows_by_id;
  else
    return;

  while (list)
    {
      id = (const gchar *) list->data;

      /* The widget for the default input source lives elsewhere */
      if (g_strcmp0 (id, default_id))
        {
          row = input_source_row_new (chooser, type, id);
          if (row)
            {
              g_object_set_data (G_OBJECT (row), "locale-info", info);
              g_hash_table_replace (table, (gpointer) id, g_object_ref_sink (row));
            }
        }
      list = list->next;
    }
}

static void
add_row (GtkWidget   *chooser,
         LocaleInfo  *info,
         const gchar *type,
         const gchar *id)
{
  GList tmp = { 0 };
  tmp.data = (gpointer) id;
  add_rows_to_table (chooser, info, &tmp, type, NULL);
}

static void
add_row_other (GtkWidget   *chooser,
               const gchar *type,
               const gchar *id)
{
  CcInputChooserPrivate *priv = GET_PRIVATE (chooser);
  LocaleInfo *info = g_hash_table_lookup (priv->locales, "");
  add_row (chooser, info, type, id);
}

#ifdef HAVE_IBUS
static gboolean
maybe_set_as_default (GtkWidget   *chooser,
                      LocaleInfo  *info,
                      const gchar *engine_id)
{
  const gchar *type, *id;

  if (!gnome_get_input_source_from_locale (info->id, &type, &id))
    return FALSE;

  if (g_str_equal (type, INPUT_SOURCE_TYPE_IBUS) &&
      g_str_equal (id, engine_id) &&
      info->default_input_source_row == NULL)
    {
      add_default_row (chooser, info, type, id);
      return TRUE;
    }

  return FALSE;
}

static void
get_ibus_locale_infos (GtkWidget *chooser)
{
  CcInputChooserPrivate *priv = GET_PRIVATE (chooser);
  GHashTableIter iter;
  LocaleInfo *info;
  const gchar *engine_id;
  IBusEngineDesc *engine;

  if (!priv->ibus_engines || priv->is_login)
    return;

  g_hash_table_iter_init (&iter, priv->ibus_engines);
  while (g_hash_table_iter_next (&iter, (gpointer *) &engine_id, (gpointer *) &engine))
    {
      g_autofree gchar *lang_code = NULL;
      g_autofree gchar *country_code = NULL;
      const gchar *ibus_locale = ibus_engine_desc_get_language (engine);

      if (gnome_parse_locale (ibus_locale, &lang_code, &country_code, NULL, NULL) &&
          lang_code != NULL &&
          country_code != NULL)
        {
          g_autofree gchar *locale = g_strdup_printf ("%s_%s.UTF-8", lang_code, country_code);

          info = g_hash_table_lookup (priv->locales, locale);
          if (info)
            {
              const gchar *type, *id;

              if (gnome_get_input_source_from_locale (locale, &type, &id) &&
                  g_str_equal (type, INPUT_SOURCE_TYPE_IBUS) &&
                  g_str_equal (id, engine_id))
                {
                  add_default_row (chooser, info, type, id);
                }
              else
                {
                  add_row (chooser, info, INPUT_SOURCE_TYPE_IBUS, engine_id);
                }
            }
          else
            {
              add_row_other (chooser, INPUT_SOURCE_TYPE_IBUS, engine_id);
            }
        }
      else if (lang_code != NULL)
        {
          GHashTableIter iter;
          GHashTable *locales_for_language;
          g_autofree gchar *language = NULL;

          /* Most IBus engines only specify the language so we try to
             add them to all locales for that language. */

          language = gnome_get_language_from_code (lang_code, NULL);
          if (language)
            locales_for_language = g_hash_table_lookup (priv->locales_by_language, language);
          else
            locales_for_language = NULL;

          if (locales_for_language)
            {
              g_hash_table_iter_init (&iter, locales_for_language);
              while (g_hash_table_iter_next (&iter, (gpointer *) &info, NULL))
                if (!maybe_set_as_default (chooser, info, engine_id))
                  add_row (chooser, info, INPUT_SOURCE_TYPE_IBUS, engine_id);
            }
          else
            {
              add_row_other (chooser, INPUT_SOURCE_TYPE_IBUS, engine_id);
            }
        }
      else
        {
          add_row_other (chooser, INPUT_SOURCE_TYPE_IBUS, engine_id);
        }
    }
}
#endif  /* HAVE_IBUS */

static void
add_locale_to_table (GHashTable  *table,
                     const gchar *lang_code,
                     LocaleInfo  *info)
{
  GHashTable *set;
  g_autofree gchar *language = NULL;

  language = gnome_get_language_from_code (lang_code, NULL);

  set = g_hash_table_lookup (table, language);
  if (!set)
    {
      set = g_hash_table_new (NULL, NULL);
      g_hash_table_replace (table, g_strdup (language), set);
    }
  g_hash_table_add (set, info);
}

static void
add_ids_to_set (GHashTable *set,
                GList      *list)
{
  while (list)
    {
      g_hash_table_add (set, list->data);
      list = list->next;
    }
}

static void
get_locale_infos (GtkWidget *chooser)
{
  CcInputChooserPrivate *priv = GET_PRIVATE (chooser);
  g_autoptr(GHashTable) layouts_with_locale = NULL;
  LocaleInfo *info;
  g_auto(GStrv) locale_ids = NULL;
  gchar **locale;
  g_autoptr(GList) all_layouts = NULL;
  GList *l;

  priv->locales = g_hash_table_new_full (g_str_hash, g_str_equal,
                                         NULL, locale_info_free);
  priv->locales_by_language = g_hash_table_new_full (g_str_hash, g_str_equal,
                                                     g_free, (GDestroyNotify) g_hash_table_destroy);

  layouts_with_locale = g_hash_table_new (g_str_hash, g_str_equal);

  locale_ids = gnome_get_all_locales ();
  for (locale = locale_ids; *locale; ++locale)
    {
      g_autofree gchar *lang_code = NULL;
      g_autofree gchar *country_code = NULL;
      g_autofree gchar *simple_locale = NULL;
      g_autofree gchar *tmp = NULL;
      const gchar *type = NULL;
      const gchar *id = NULL;
      g_autoptr(GList) language_layouts = NULL;

      if (!gnome_parse_locale (*locale, &lang_code, &country_code, NULL, NULL))
        continue;

      if (country_code != NULL)
	simple_locale = g_strdup_printf ("%s_%s.UTF-8", lang_code, country_code);
      else
	simple_locale = g_strdup_printf ("%s.UTF-8", lang_code);

      if (g_hash_table_contains (priv->locales, simple_locale))
          continue;

      info = g_new0 (LocaleInfo, 1);
      info->id = g_steal_pointer (&simple_locale);
      info->name = gnome_get_language_from_locale (simple_locale, NULL);
      info->unaccented_name = cc_util_normalize_casefold_and_unaccent (info->name);
      tmp = gnome_get_language_from_locale (simple_locale, "C");
      info->untranslated_name = cc_util_normalize_casefold_and_unaccent (tmp);

      g_hash_table_replace (priv->locales, simple_locale, info);
      add_locale_to_table (priv->locales_by_language, lang_code, info);

      if (gnome_get_input_source_from_locale (simple_locale, &type, &id) &&
          g_str_equal (type, INPUT_SOURCE_TYPE_XKB))
        {
          add_default_row (chooser, info, type, id);
          g_hash_table_add (layouts_with_locale, (gpointer) id);
        }

      /* We don't own these ids */
      info->layout_rows_by_id = g_hash_table_new_full (g_str_hash, g_str_equal,
                                                       NULL, g_object_unref);
      info->engine_rows_by_id = g_hash_table_new_full (g_str_hash, g_str_equal,
                                                       NULL, g_object_unref);

      language_layouts = gnome_xkb_info_get_layouts_for_language (priv->xkb_info, lang_code);
      add_rows_to_table (chooser, info, language_layouts, INPUT_SOURCE_TYPE_XKB, id);
      add_ids_to_set (layouts_with_locale, language_layouts);

      if (country_code != NULL)
        {
          g_autoptr(GList) country_layouts = gnome_xkb_info_get_layouts_for_country (priv->xkb_info, country_code);
          add_rows_to_table (chooser, info, country_layouts, INPUT_SOURCE_TYPE_XKB, id);
          add_ids_to_set (layouts_with_locale, country_layouts);
        }
    }

  /* Add a "Other" locale to hold the remaining input sources */
  info = g_new0 (LocaleInfo, 1);
  info->id = g_strdup ("");
  info->name = g_strdup (C_("Input Source", "Other"));
  info->unaccented_name = g_strdup ("");
  info->untranslated_name = g_strdup ("");
  g_hash_table_replace (priv->locales, info->id, info);

  info->layout_rows_by_id = g_hash_table_new_full (g_str_hash, g_str_equal,
                                                   NULL, g_object_unref);
  info->engine_rows_by_id = g_hash_table_new_full (g_str_hash, g_str_equal,
                                                   NULL, g_object_unref);

  all_layouts = gnome_xkb_info_get_all_layouts (priv->xkb_info);
  for (l = all_layouts; l; l = l->next)
    if (!g_hash_table_contains (layouts_with_locale, l->data))
      add_row_other (chooser, INPUT_SOURCE_TYPE_XKB, l->data);
}

static void
cc_input_chooser_private_free (gpointer data)
{
  CcInputChooserPrivate *priv = data;

  g_object_unref (priv->more_row);
  g_object_unref (priv->no_results);
  g_hash_table_destroy (priv->locales);
  g_hash_table_destroy (priv->locales_by_language);
  g_strfreev (priv->filter_words);
  if (priv->filter_timeout_id)
    g_source_remove (priv->filter_timeout_id);
  g_free (priv);
}

static gboolean
reset_on_escape (GtkWidget   *widget,
                 GdkEventKey *event,
                 GtkWidget   *chooser)
{
  if (event->keyval == GDK_KEY_Escape)
    cc_input_chooser_reset (chooser);

  return FALSE;
}

GtkWidget *
cc_input_chooser_new (GtkWindow    *main_window,
                      gboolean      is_login,
                      GnomeXkbInfo *xkb_info,
                      GHashTable   *ibus_engines)
{
  g_autoptr(GtkBuilder) builder = NULL;
  GtkWidget *chooser;
  CcInputChooserPrivate *priv;
  gint width, height;
  g_autoptr(GError) error = NULL;

  builder = gtk_builder_new ();
  if (gtk_builder_add_from_resource (builder, "/org/gnome/control-center/region/input-chooser.ui", &error) == 0)
    {
      g_warning ("failed to load input chooser: %s", error->message);
      return NULL;
    }
  chooser = WID ("input-dialog");
  priv = g_new0 (CcInputChooserPrivate, 1);
  g_object_set_data_full (G_OBJECT (chooser), "private", priv, cc_input_chooser_private_free);

  priv->is_login = is_login;
  priv->xkb_info = xkb_info;
  priv->ibus_engines = ibus_engines;

  priv->add_button = WID ("add-button");
  priv->filter_entry = WID ("filter-entry");
  priv->list = WID ("list");
  priv->scrolledwindow = WID ("scrolledwindow");
  priv->adjustment = gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW (priv->scrolledwindow));

  priv->more_row = g_object_ref_sink (more_row_new ());
  priv->no_results = g_object_ref_sink (no_results_widget_new ());
  gtk_widget_show_all (priv->no_results);

  gtk_list_box_set_filter_func (GTK_LIST_BOX (priv->list), list_filter, chooser, NULL);
  gtk_list_box_set_sort_func (GTK_LIST_BOX (priv->list), (GtkListBoxSortFunc)list_sort, chooser, NULL);
  g_signal_connect (priv->list, "row-activated", G_CALLBACK (row_activated), chooser);
  g_signal_connect (priv->list, "selected-rows-changed", G_CALLBACK (selected_rows_changed), chooser);
  g_signal_connect (priv->list, "button-release-event", G_CALLBACK (list_button_release_event), chooser);

  g_signal_connect_swapped (priv->filter_entry, "search-changed", G_CALLBACK (filter_changed), chooser);
  g_signal_connect (priv->filter_entry, "key-release-event", G_CALLBACK (reset_on_escape), chooser);

  if (priv->is_login)
    gtk_widget_show (WID ("login-label"));

  get_locale_infos (chooser);
#ifdef HAVE_IBUS
  get_ibus_locale_infos (chooser);
#endif  /* HAVE_IBUS */
  show_locale_rows (chooser);

  /* Try to come up with a sensible size */
  gtk_window_get_size (main_window, &width, &height);
  gtk_widget_set_size_request (chooser, width * MAIN_WINDOW_WIDTH_RATIO, -1);
  gtk_window_set_resizable (GTK_WINDOW (chooser), TRUE);

  gtk_window_set_transient_for (GTK_WINDOW (chooser), main_window);

  return chooser;
}

void
cc_input_chooser_set_ibus_engines (GtkWidget  *chooser,
                                   GHashTable *ibus_engines)
{
#ifdef HAVE_IBUS
  CcInputChooserPrivate *priv = GET_PRIVATE (chooser);

  /* This should only be called once when IBus shows up in case it
     wasn't up yet when the user opened the input chooser dialog. */
  g_return_if_fail (priv->ibus_engines == NULL);

  priv->ibus_engines = ibus_engines;
  get_ibus_locale_infos (chooser);
  show_locale_rows (chooser);
#endif  /* HAVE_IBUS */
}

gboolean
cc_input_chooser_get_selected (GtkWidget  *chooser,
                               gchar     **type,
                               gchar     **id,
                               gchar     **name)
{
  CcInputChooserPrivate *priv = GET_PRIVATE (chooser);
  GtkListBoxRow *selected;
  const gchar *t, *i, *n;

  selected = gtk_list_box_get_selected_row (GTK_LIST_BOX (priv->list));
  if (!selected)
    return FALSE;

  t = g_object_get_data (G_OBJECT (selected), "type");
  i = g_object_get_data (G_OBJECT (selected), "id");
  n = g_object_get_data (G_OBJECT (selected), "name");

  if (!t || !i || !n)
    return FALSE;

  *type = g_strdup (t);
  *id = g_strdup (i);
  *name = g_strdup (n);

  return TRUE;
}

void
cc_input_chooser_reset (GtkWidget *chooser)
{
  CcInputChooserPrivate *priv = GET_PRIVATE (chooser);
  priv->showing_extra = FALSE;
  gtk_entry_set_text (GTK_ENTRY (priv->filter_entry), "");
  gtk_widget_hide (priv->filter_entry);
  g_clear_pointer (&priv->filter_words, g_strfreev);
  show_locale_rows (chooser);
}
