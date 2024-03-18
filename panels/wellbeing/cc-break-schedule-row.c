/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/*
 * Copyright 2024 GNOME Foundation, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General
 * Public License along with this library; if not, see <http://www.gnu.org/licenses/>.
 *
 * Authors:
 *  - Philip Withnall <pwithnall@gnome.org>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <adwaita.h>
#include <glib/gi18n.h>
#include <glib-object.h>
#include <gtk/gtk.h>

#include "cc-break-schedule-row.h"
#include "cc-util.h"

/**
 * CcBreakSchedule:
 *
 * A simple object designed to represent a break schedule (a duration and
 * interval) as a #GObject, for use in a #GListModel to set as the model for a
 * #CcBreakScheduleRow.
 */
struct _CcBreakSchedule {
  GObject parent_instance;

  guint duration_secs;
  guint interval_secs;
};

G_DEFINE_TYPE (CcBreakSchedule, cc_break_schedule, G_TYPE_OBJECT)

static void
cc_break_schedule_class_init (CcBreakScheduleClass *klass)
{
}

static void
cc_break_schedule_init (CcBreakSchedule *self)
{
}

/**
 * cc_break_schedule_new:
 * @duration_secs: break duration, in seconds
 * @interval_secs: break interval, in seconds
 *
 * Create a new #CcBreakSchedule, to represent a pairing of break duration and
 * interval.
 *
 * Returns: (transfer full): a new #CcBreakSchedule
 */
CcBreakSchedule *
cc_break_schedule_new (guint duration_secs,
                       guint interval_secs)
{
  g_autoptr(CcBreakSchedule) schedule = g_object_new (CC_TYPE_BREAK_SCHEDULE, NULL);
  schedule->duration_secs = duration_secs;
  schedule->interval_secs = interval_secs;
  return g_steal_pointer (&schedule);
}

/**
 * cc_break_schedule_get_formatted_duration:
 * @self: a #CcBreakSchedule
 *
 * Get the duration of the break, in a human readable and translated format.
 *
 * Returns: (transfer full): human readable and translated duration
 */
char *
cc_break_schedule_get_formatted_duration (CcBreakSchedule *self)
{
  return cc_util_time_to_string_text (self->duration_secs * 1000);
}

/**
 * cc_break_schedule_get_formatted_interval:
 * @self: a #CcBreakSchedule
 *
 * Get the interval of the break, in a human readable and translated format.
 *
 * Returns: (transfer full): human readable and translated interval
 */
char *
cc_break_schedule_get_formatted_interval (CcBreakSchedule *self)
{
  return cc_util_time_to_string_text (self->interval_secs * 1000);
}

/**
 * cc_break_schedule_compare:
 * @a: a #CcBreakSchedule
 * @b: another #CcBreakSchedule
 *
 * Compare two break schedules.
 *
 * They are sorted by increasing interval, and then by increasing duration.
 *
 * Returns: <0 if @a comes before @b, 0 if they are equal, >0 if @a comes after @b
 */
gint
cc_break_schedule_compare (CcBreakSchedule *a,
                           CcBreakSchedule *b)
{
  if (a->interval_secs != b->interval_secs)
    return a->interval_secs - b->interval_secs;
  if (a->duration_secs != b->duration_secs)
    return a->duration_secs - b->duration_secs;
  return 0;
}

/**
 * cc_break_schedule_get_duration_secs:
 * @self: a #CcBreakSchedule
 *
 * Get the duration of the break, in seconds.
 *
 * Returns: duration in seconds
 */
guint
cc_break_schedule_get_duration_secs (CcBreakSchedule *self)
{
  return self->duration_secs;
}

/**
 * cc_break_schedule_get_interval_secs:
 * @self: a #CcBreakSchedule
 *
 * Get the interval of the break, in seconds.
 *
 * Returns: interval in seconds
 */
guint
cc_break_schedule_get_interval_secs (CcBreakSchedule *self)
{
  return self->interval_secs;
}

/**
 * CcBreakScheduleRow:
 *
 * A specific instance of #AdwComboRow for presenting break schedules, which
 * have two parts to them: a duration, and an interval. Each needs to be
 * represented as a separate widget, hence the default factory provided by
 * #AdwComboBox is insufficient.
 *
 * This requires that the items in its model are instances of #CcBreakSchedule.
 */
struct _CcBreakScheduleRow {
  AdwComboRow parent_instance;

  GtkSizeGroup *duration_size_group;
  GtkSizeGroup *interval_size_group;
};

G_DEFINE_TYPE (CcBreakScheduleRow, cc_break_schedule_row, ADW_TYPE_COMBO_ROW)

static void cc_break_schedule_row_dispose (GObject *object);
static void factory_setup_cb (GtkSignalListItemFactory *factory,
                              GObject                  *object,
                              gpointer                  user_data);
static void factory_bind_cb (GtkSignalListItemFactory *factory,
                             GObject                  *object,
                             gpointer                  user_data);
static void factory_unbind_cb (GtkSignalListItemFactory *factory,
                               GObject                  *object,
                               gpointer                  user_data);
static void header_factory_setup_cb (GtkSignalListItemFactory *factory,
                                     GObject                  *object,
                                     gpointer                  user_data);

static void
cc_break_schedule_row_class_init (CcBreakScheduleRowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = cc_break_schedule_row_dispose;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/wellbeing/cc-break-schedule-row.ui");

  gtk_widget_class_bind_template_child (widget_class, CcBreakScheduleRow, duration_size_group);
  gtk_widget_class_bind_template_child (widget_class, CcBreakScheduleRow, interval_size_group);

  gtk_widget_class_bind_template_callback (widget_class, factory_setup_cb);
  gtk_widget_class_bind_template_callback (widget_class, factory_bind_cb);
  gtk_widget_class_bind_template_callback (widget_class, factory_unbind_cb);
  gtk_widget_class_bind_template_callback (widget_class, header_factory_setup_cb);
}

static void
cc_break_schedule_row_init (CcBreakScheduleRow *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

static void
cc_break_schedule_row_dispose (GObject *object)
{
  gtk_widget_dispose_template (GTK_WIDGET (object), CC_TYPE_BREAK_SCHEDULE_ROW);

  G_OBJECT_CLASS (cc_break_schedule_row_parent_class)->dispose (object);
}

static void
factory_notify_selected_item_cb (GObject    *object,
                                 GParamSpec *pspec,
                                 gpointer    user_data)
{
  AdwComboRow *combo_row = ADW_COMBO_ROW (object);
  GtkListItem *list_item = GTK_LIST_ITEM (user_data);
  GtkWidget *box, *icon;
  gboolean selected;

  box = gtk_list_item_get_child (list_item);
  icon = gtk_widget_get_last_child (box);

  selected = (adw_combo_row_get_selected_item (combo_row) == gtk_list_item_get_item (list_item));
  gtk_widget_set_opacity (icon, selected ? 1.0 : 0.0);
}

static void
factory_notify_root_cb (GObject    *object,
                        GParamSpec *pspec,
                        gpointer    user_data)
{
  GtkBox *box = GTK_BOX (object);
  AdwComboRow *combo_row = ADW_COMBO_ROW (user_data);
  GtkWidget *icon, *slash, *duration_label, *box_popover;
  gboolean is_in_combo_popover;

  icon = gtk_widget_get_last_child (GTK_WIDGET (box));
  slash = gtk_widget_get_prev_sibling (gtk_widget_get_prev_sibling (icon));
  duration_label = gtk_widget_get_prev_sibling (slash);
  box_popover = gtk_widget_get_ancestor (GTK_WIDGET (box), GTK_TYPE_POPOVER);
  is_in_combo_popover = (box_popover != NULL && gtk_widget_get_ancestor (box_popover, ADW_TYPE_COMBO_ROW) == (GtkWidget *) combo_row);

  /* Selection icon should only be visible when in the popover. */
  gtk_widget_set_visible (icon, is_in_combo_popover);

  /* Slash should only be visible for the selected entry (not in the popover). */
  gtk_widget_set_visible (slash, !is_in_combo_popover);

  /* Adjust the spacing and alignment so that things are column-aligned when in
   * the popover, but tightly packed when showing the selected entry. */
  gtk_box_set_spacing (box, is_in_combo_popover ? 12 : 0);
  gtk_label_set_xalign (GTK_LABEL (duration_label), is_in_combo_popover ? 0.0 : 1.0);
}

static void
factory_setup_cb (GtkSignalListItemFactory *factory,
                  GObject                  *object,
                  gpointer                  user_data)
{
  GtkListItem *list_item = GTK_LIST_ITEM (object);
  CcBreakScheduleRow *self = CC_BREAK_SCHEDULE_ROW (user_data);
  GtkWidget *box, *duration_label, *slash_label, *interval_label, *icon;

  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 12);

  duration_label = gtk_label_new (NULL);
  gtk_label_set_xalign (GTK_LABEL (duration_label), 0.0);
  gtk_label_set_ellipsize (GTK_LABEL (duration_label), PANGO_ELLIPSIZE_END);
  gtk_label_set_max_width_chars (GTK_LABEL (duration_label), 20);
  gtk_widget_set_valign (duration_label, GTK_ALIGN_CENTER);
  gtk_box_append (GTK_BOX (box), duration_label);
  gtk_size_group_add_widget (self->duration_size_group, duration_label);

  /* Translators: This separates the duration and interval times when displaying
   * the selected break schedule on the wellbeing panel. For example,
   * “1 minute / 20 minutes” to indicate a 1 minute break every 20 minutes. */
  slash_label = gtk_label_new (_(" / "));
  gtk_label_set_xalign (GTK_LABEL (slash_label), 0.5);
  gtk_widget_set_valign (slash_label, GTK_ALIGN_CENTER);
  gtk_box_append (GTK_BOX (box), slash_label);

  interval_label = gtk_label_new (NULL);
  gtk_label_set_xalign (GTK_LABEL (interval_label), 0.0);
  gtk_label_set_ellipsize (GTK_LABEL (interval_label), PANGO_ELLIPSIZE_END);
  gtk_label_set_max_width_chars (GTK_LABEL (interval_label), 20);
  gtk_widget_set_valign (interval_label, GTK_ALIGN_CENTER);
  gtk_box_append (GTK_BOX (box), interval_label);
  gtk_size_group_add_widget (self->interval_size_group, interval_label);

  icon = g_object_new (GTK_TYPE_IMAGE,
                       "accessible-role", GTK_ACCESSIBLE_ROLE_PRESENTATION,
                       "icon-name", "object-select-symbolic",
                       NULL);
  gtk_box_append (GTK_BOX (box), icon);

  gtk_list_item_set_child (list_item, box);
}

static void
factory_bind_cb (GtkSignalListItemFactory *factory,
                 GObject                  *object,
                 gpointer                  user_data)
{
  GtkListItem *list_item = GTK_LIST_ITEM (object);
  CcBreakScheduleRow *self = CC_BREAK_SCHEDULE_ROW (user_data);
  CcBreakSchedule *item;
  GtkWidget *duration_label, *interval_label, *box;
  g_autofree char *duration_str = NULL, *interval_str = NULL;

  item = CC_BREAK_SCHEDULE (gtk_list_item_get_item (list_item));
  box = gtk_list_item_get_child (list_item);
  duration_label = gtk_widget_get_first_child (box);
  interval_label = gtk_widget_get_next_sibling (gtk_widget_get_next_sibling (duration_label));

  duration_str = cc_break_schedule_get_formatted_duration (item);
  gtk_label_set_label (GTK_LABEL (duration_label), duration_str);
  interval_str = cc_break_schedule_get_formatted_interval (item);
  gtk_label_set_label (GTK_LABEL (interval_label), interval_str);

  /* Bind to a signal about selection notification, so we can update the
   * visibility of the selection tick */
  g_signal_connect (self, "notify::selected-item",
                    G_CALLBACK (factory_notify_selected_item_cb), list_item);
  factory_notify_selected_item_cb (G_OBJECT (self), NULL, list_item);

  /* And to root notification. This is notified when the list item widgets are
   * unparented from the popover and reparented in the top-level #AdwComboRow,
   * to display the current selection. Idea for this borrowed from libadwaita,
   * adw-combo-row.c. */
  g_signal_connect (box, "notify::root", G_CALLBACK (factory_notify_root_cb), self);
  factory_notify_root_cb (G_OBJECT (box), NULL, self);
}

static void
factory_unbind_cb (GtkSignalListItemFactory *factory,
                   GObject                  *object,
                   gpointer                  user_data)
{
  GtkListItem *list_item = GTK_LIST_ITEM (object);
  CcBreakScheduleRow *self = CC_BREAK_SCHEDULE_ROW (user_data);
  GtkWidget *box;

  box = gtk_list_item_get_child (list_item);

  g_signal_handlers_disconnect_by_func (self, factory_notify_selected_item_cb, list_item);
  g_signal_handlers_disconnect_by_func (box, factory_notify_root_cb, self);
}

static void
header_factory_setup_cb (GtkSignalListItemFactory *factory,
                         GObject                  *object,
                         gpointer                  user_data)
{
  GtkListHeader *list_header = GTK_LIST_HEADER (object);
  CcBreakScheduleRow *self = CC_BREAK_SCHEDULE_ROW (user_data);
  GtkWidget *box;
  const char *labels[] = { N_("Duration"), N_("Interval") };

  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 20);
  gtk_widget_set_margin_start (box, 14);
  gtk_widget_set_margin_end (box, 14);

  for (size_t i = 0; i < G_N_ELEMENTS (labels); i++)
    {
      GtkWidget *label = gtk_label_new (_(labels[i]));
      gtk_label_set_xalign (GTK_LABEL (label), 0.0);
      gtk_label_set_ellipsize (GTK_LABEL (label), PANGO_ELLIPSIZE_END);
      gtk_label_set_max_width_chars (GTK_LABEL (label), 20);
      gtk_widget_set_valign (label, GTK_ALIGN_CENTER);
      gtk_box_append (GTK_BOX (box), label);

      gtk_size_group_add_widget ((i == 0) ? self->duration_size_group : self->interval_size_group,
                                 label);
    }

  gtk_list_header_set_child (list_header, box);
}

/**
 * cc_break_schedule_row_new:
 *
 * Creates a new #CcBreakScheduleRow.
 *
 * Returns: (transfer full): a new break schedule row
 */
CcBreakScheduleRow *
cc_break_schedule_row_new (void)
{
  return g_object_new (CC_TYPE_BREAK_SCHEDULE_ROW, NULL);
}
