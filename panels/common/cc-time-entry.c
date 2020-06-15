/* -*- mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*- */
/* cc-time-entry.c
 *
 * Copyright 2020 Purism SPC
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Author(s):
 *   Mohammed Sadiq <sadiq@sadiqpk.org>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "cc-time-entry"

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <gtk/gtk.h>
#include <glib/gi18n.h>

#include "cc-time-entry.h"

#define SEPARATOR_INDEX      2
#define END_INDEX            4
#define EMIT_CHANGED_TIMEOUT 100


struct _CcTimeEntry
{
  GtkEntry   parent_instance;

  guint      insert_text_id;
  guint      time_changed_id;
  int        hour; /* Range: 0-23 in 24H and 1-12 in 12H with is_am set/unset */
  int        minute;
  gboolean   is_am_pm;
  gboolean   is_am; /* AM if TRUE. PM if FALSE. valid iff is_am_pm set */
};

G_DEFINE_TYPE (CcTimeEntry, cc_time_entry, GTK_TYPE_ENTRY)

enum {
  CHANGE_VALUE,
  TIME_CHANGED,
  N_SIGNALS
};

static guint signals[N_SIGNALS];

static gboolean
emit_time_changed (CcTimeEntry *self)
{
  self->time_changed_id = 0;

  g_signal_emit (self, signals[TIME_CHANGED], 0);

  return G_SOURCE_REMOVE;
}

static void
time_entry_fill_time (CcTimeEntry *self)
{
  g_autofree gchar *str = NULL;

  g_assert (CC_IS_TIME_ENTRY (self));

  str = g_strdup_printf ("%02d∶%02d", self->hour, self->minute);

  g_signal_handler_block (self, self->insert_text_id);
  gtk_entry_set_text (GTK_ENTRY (self), str);
  g_signal_handler_unblock (self, self->insert_text_id);
}

static void
cursor_position_changed_cb (CcTimeEntry *self)
{
  int current_pos;

  g_assert (CC_IS_TIME_ENTRY (self));

  current_pos = gtk_editable_get_position (GTK_EDITABLE (self));

  g_signal_handlers_block_by_func (self, cursor_position_changed_cb, self);

  /* If cursor is on ‘:’ move to the next field */
  if (current_pos == SEPARATOR_INDEX)
    gtk_editable_set_position (GTK_EDITABLE (self), current_pos + 1);

  /* If cursor is after the last digit and without selection, move to last digit */
  if (current_pos > END_INDEX &&
      !gtk_editable_get_selection_bounds (GTK_EDITABLE (self), NULL, NULL))
    gtk_editable_set_position (GTK_EDITABLE (self), END_INDEX);

  g_signal_handlers_unblock_by_func (self, cursor_position_changed_cb, self);
}

static void
entry_selection_changed_cb (CcTimeEntry *self)
{
  GtkEditable *editable;

  g_assert (CC_IS_TIME_ENTRY (self));

  editable = GTK_EDITABLE (self);

  g_signal_handlers_block_by_func (self, cursor_position_changed_cb, self);

  /* If cursor is after the last digit and without selection, move to last digit */
  if (gtk_editable_get_position (editable) > END_INDEX &&
      !gtk_editable_get_selection_bounds (editable, NULL, NULL))
    gtk_editable_set_position (editable, END_INDEX);

  g_signal_handlers_unblock_by_func (self, cursor_position_changed_cb, self);
}

static void
editable_insert_text_cb (CcTimeEntry *self,
                         char        *new_text,
                         gint         new_text_length,
                         gint        *position)
{
  g_assert (CC_IS_TIME_ENTRY (self));

  if (new_text_length == -1)
    new_text_length = strlen (new_text);

  if (new_text_length == 5)
    {
      guint16 text_length;

      text_length = gtk_entry_get_text_length (GTK_ENTRY (self));

      /* Return if the text matches XX:XX template (where X is a number) */
      if (text_length == 0 &&
          strstr (new_text, "0123456789:") == new_text + new_text_length &&
          strchr (new_text, ':') == strrchr (new_text, ':'))
        return;
    }

  /* Insert text if single digit number */
  if (new_text_length == 1 &&
      strspn (new_text, "0123456789"))
    {
      int pos, number;

      pos = *position;
      number = *new_text - '0';

      if (pos == 0)
        self->hour = self->hour % 10 + number * 10;
      else if (pos == 1)
        self->hour = self->hour / 10 * 10 + number;
      else if (pos == 3)
        self->minute = self->minute % 10 + number * 10;
      else if (pos == 4)
        self->minute = self->minute / 10 * 10 + number;

      if (self->is_am_pm)
        self->hour = CLAMP (self->hour, 1, 12);
      else
        self->hour = CLAMP (self->hour, 0, 23);

      self->minute = CLAMP (self->minute, 0, 59);

      g_signal_stop_emission_by_name (self, "insert-text");
      time_entry_fill_time (self);
      *position = pos + 1;

      g_clear_handle_id (&self->time_changed_id, g_source_remove);
      self->time_changed_id = g_timeout_add (EMIT_CHANGED_TIMEOUT,
                                             (GSourceFunc)emit_time_changed, self);
      return;
    }

  /* Warn otherwise */
  g_signal_stop_emission_by_name (self, "insert-text");
  gtk_widget_error_bell (GTK_WIDGET (self));
}


static void
entry_select_all (CcTimeEntry *self)
{
  gtk_editable_select_region (GTK_EDITABLE (self), 0, -1);
}

static void
entry_populate_popup_cb (CcTimeEntry *self,
                         GtkWidget   *widget)
{
  GList *children;

  if (!GTK_IS_CONTAINER (widget))
    return;

  children = gtk_container_get_children (GTK_CONTAINER (widget));

  if (GTK_IS_MENU (widget))
    {
      GtkWidget *menu_item;

      for (GList *child = children; child; child = child->next)
        gtk_container_remove (GTK_CONTAINER (widget), child->data);

      menu_item = gtk_menu_item_new_with_mnemonic (_("_Copy"));
      gtk_widget_set_sensitive (menu_item, gtk_editable_get_selection_bounds (GTK_EDITABLE (self), NULL, NULL));
      g_signal_connect_swapped (menu_item, "activate", G_CALLBACK (gtk_editable_copy_clipboard), self);
      gtk_widget_show (menu_item);
      gtk_menu_shell_append (GTK_MENU_SHELL (widget), menu_item);

      menu_item = gtk_menu_item_new_with_mnemonic (_("Select _All"));
      gtk_widget_set_sensitive (menu_item, gtk_entry_get_text_length (GTK_ENTRY (self)) > 0);
      g_signal_connect_swapped (menu_item, "activate", G_CALLBACK (entry_select_all), self);
      gtk_widget_show (menu_item);
      gtk_menu_shell_append (GTK_MENU_SHELL (widget), menu_item);
    }
}

static void
time_entry_change_value_cb (CcTimeEntry   *self,
                            GtkScrollType  type)
{
  int position;
  g_assert (CC_IS_TIME_ENTRY (self));

  position = gtk_editable_get_position (GTK_EDITABLE (self));

  if (position > SEPARATOR_INDEX)
    {
      if (type == GTK_SCROLL_STEP_UP)
        self->minute++;
      else
        self->minute--;

      if (self->minute >= 60)
        self->minute = 0;
      else if (self->minute <= -1)
        self->minute = 59;
    }
  else
    {
      if (type == GTK_SCROLL_STEP_UP)
        self->hour++;
      else
        self->hour--;

      if (self->is_am_pm)
        {
          if (self->hour > 12)
            self->hour = 1;
          else if (self->hour < 1)
            self->hour = 12;
        }
      else
        {
          if (self->hour >= 24)
            self->hour = 0;
          else if (self->hour <= -1)
            self->hour = 23;
        }
    }

  time_entry_fill_time (self);
  gtk_editable_set_position (GTK_EDITABLE (self), position);

  g_clear_handle_id (&self->time_changed_id, g_source_remove);
  self->time_changed_id = g_timeout_add (EMIT_CHANGED_TIMEOUT,
                                         (GSourceFunc)emit_time_changed, self);
}

static void
cc_entry_move_cursor (GtkEntry        *entry,
                      GtkMovementStep  step,
                      gint             count,
                      gboolean         extend_selection)
{
  int current_pos;

  current_pos = gtk_editable_get_position (GTK_EDITABLE (entry));

  /* If cursor is on ‘:’ move backward/forward depending on the current movement */
  if ((step == GTK_MOVEMENT_LOGICAL_POSITIONS ||
       step == GTK_MOVEMENT_VISUAL_POSITIONS) &&
      current_pos + count == SEPARATOR_INDEX)
    count > 0 ? count++ : count--;

  GTK_ENTRY_CLASS (cc_time_entry_parent_class)->move_cursor (entry, step, count, extend_selection);
}

static void
cc_time_entry_error_bell (GtkEntry *entry)
{
  gtk_widget_error_bell (GTK_WIDGET (entry));
}

static void
cc_time_entry_delete_from_cursor (GtkEntry      *entry,
                                  GtkDeleteType  type,
                                  gint            count)
{
  gtk_widget_error_bell (GTK_WIDGET (entry));
}

static gboolean
cc_time_entry_drag_motion (GtkWidget      *widget,
                           GdkDragContext *context,
                           gint            x,
                           gint            y,
                           guint           time)
{
  return TRUE;
}

static gboolean
cc_time_entry_key_press (GtkWidget   *widget,
                         GdkEventKey *event)
{
  CcTimeEntry *self = (CcTimeEntry *)widget;

  /* Allow entering numbers */
  if (!(event->state & GDK_SHIFT_MASK) &&
      ((event->keyval >= GDK_KEY_KP_0 &&
        event->keyval <= GDK_KEY_KP_9) ||
       (event->keyval >= GDK_KEY_0 &&
        event->keyval <= GDK_KEY_9)))
    return GTK_WIDGET_CLASS (cc_time_entry_parent_class)->key_press_event (widget, event);

  /* Allow navigation keys */
  if ((event->keyval >= GDK_KEY_Left &&
       event->keyval <= GDK_KEY_Down) ||
      (event->keyval >= GDK_KEY_KP_Left &&
       event->keyval <= GDK_KEY_KP_Down) ||
      event->keyval == GDK_KEY_Home ||
      event->keyval == GDK_KEY_End ||
      event->keyval == GDK_KEY_Menu)
    return GTK_WIDGET_CLASS (cc_time_entry_parent_class)->key_press_event (widget, event);

  if (event->state & (GDK_CONTROL_MASK | GDK_MOD1_MASK))
    return GTK_WIDGET_CLASS (cc_time_entry_parent_class)->key_press_event (widget, event);

  if (event->keyval == GDK_KEY_Tab)
    {
      /* If focus is on Hour field skip to minute field */
      if (gtk_editable_get_position (GTK_EDITABLE (self)) <= 1)
        {
          gtk_editable_set_position (GTK_EDITABLE (self), SEPARATOR_INDEX + 1);

          return GDK_EVENT_STOP;
        }

      return GTK_WIDGET_CLASS (cc_time_entry_parent_class)->key_press_event (widget, event);
    }

  /* Shift-Tab */
  if (event->keyval == GDK_KEY_ISO_Left_Tab)
    {
      /* If focus is on Minute field skip back to Hour field */
      if (gtk_editable_get_position (GTK_EDITABLE (self)) >= 2)
        {
          gtk_editable_set_position (GTK_EDITABLE (self), 0);

          return GDK_EVENT_STOP;
        }

      return GTK_WIDGET_CLASS (cc_time_entry_parent_class)->key_press_event (widget, event);
    }

  return GDK_EVENT_STOP;
}

static void
cc_time_entry_constructed (GObject *object)
{
  PangoAttrList *list;
  PangoAttribute *attribute;

  G_OBJECT_CLASS (cc_time_entry_parent_class)->constructed (object);

  g_object_set (object,
                "input-purpose", GTK_INPUT_PURPOSE_DIGITS,
                "input-hints", GTK_INPUT_HINT_NO_EMOJI,
                "overwrite-mode", TRUE,
                "xalign", 0.5,
                "max-length", 5,
                NULL);

  time_entry_fill_time (CC_TIME_ENTRY (object));

  list = pango_attr_list_new ();

  attribute = pango_attr_size_new (PANGO_SCALE * 32);
  pango_attr_list_insert (list, attribute);

  attribute = pango_attr_weight_new (PANGO_WEIGHT_LIGHT);
  pango_attr_list_insert (list, attribute);

  /* Use tabular(monospace) letters */
  attribute = pango_attr_font_features_new ("tnum");
  pango_attr_list_insert (list, attribute);

  gtk_entry_set_attributes (GTK_ENTRY (object), list);
}

static void
cc_time_entry_class_init (CcTimeEntryClass *klass)
{
  GObjectClass   *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GtkEntryClass  *entry_class  = GTK_ENTRY_CLASS (klass);
  GtkBindingSet  *binding_set;

  object_class->constructed = cc_time_entry_constructed;

  widget_class->drag_motion = cc_time_entry_drag_motion;
  widget_class->key_press_event = cc_time_entry_key_press;

  entry_class->delete_from_cursor = cc_time_entry_delete_from_cursor;
  entry_class->move_cursor = cc_entry_move_cursor;
  entry_class->toggle_overwrite = cc_time_entry_error_bell;
  entry_class->backspace = cc_time_entry_error_bell;
  entry_class->cut_clipboard = cc_time_entry_error_bell;
  entry_class->paste_clipboard = cc_time_entry_error_bell;

  signals[CHANGE_VALUE] =
    g_signal_new ("change-value",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_ACTION,
                  0, NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 1,
                  GTK_TYPE_SCROLL_TYPE);

  signals[TIME_CHANGED] =
    g_signal_new ("time-changed",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_FIRST,
                  0, NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 0);

  binding_set = gtk_binding_set_by_class (klass);

  gtk_binding_entry_add_signal (binding_set, GDK_KEY_Up, 0,
                                "change-value", 1,
                                GTK_TYPE_SCROLL_TYPE, GTK_SCROLL_STEP_UP);
  gtk_binding_entry_add_signal (binding_set, GDK_KEY_KP_Up, 0,
                                "change-value", 1,
                                GTK_TYPE_SCROLL_TYPE, GTK_SCROLL_STEP_UP);

  gtk_binding_entry_add_signal (binding_set, GDK_KEY_Down, 0,
                                "change-value", 1,
                                GTK_TYPE_SCROLL_TYPE, GTK_SCROLL_STEP_DOWN);
  gtk_binding_entry_add_signal (binding_set, GDK_KEY_KP_Down, 0,
                                "change-value", 1,
                                GTK_TYPE_SCROLL_TYPE, GTK_SCROLL_STEP_DOWN);
}

static void
cc_time_entry_init (CcTimeEntry *self)
{
  g_signal_connect_after (self, "notify::cursor-position",
                          G_CALLBACK (cursor_position_changed_cb), NULL);
  g_signal_connect_after (self, "notify::selection-bound",
                          G_CALLBACK (entry_selection_changed_cb), NULL);
  self->insert_text_id = g_signal_connect (self, "insert-text",
                                           G_CALLBACK (editable_insert_text_cb), NULL);
  g_signal_connect_after (self, "populate-popup",
                          G_CALLBACK (entry_populate_popup_cb), NULL);
  g_signal_connect (self, "change-value",
                    G_CALLBACK (time_entry_change_value_cb), NULL);
}

GtkWidget *
cc_time_entry_new (void)
{
  return g_object_new (CC_TYPE_TIME_ENTRY, NULL);
}

void
cc_time_entry_set_time (CcTimeEntry *self,
                        guint        hour,
                        guint        minute)
{
  gboolean is_am_pm;

  g_return_if_fail (CC_IS_TIME_ENTRY (self));

  if (cc_time_entry_get_hour (self) == hour &&
      cc_time_entry_get_minute (self) == minute)
    return;

  is_am_pm = cc_time_entry_get_am_pm (self);
  cc_time_entry_set_am_pm (self, FALSE);

  self->hour = CLAMP (hour, 0, 23);
  self->minute = CLAMP (minute, 0, 59);

  cc_time_entry_set_am_pm (self, is_am_pm);
  time_entry_fill_time (self);
}

guint
cc_time_entry_get_hour (CcTimeEntry *self)
{
  g_return_val_if_fail (CC_IS_TIME_ENTRY (self), 0);

  if (!self->is_am_pm)
    return self->hour;

  if (self->is_am && self->hour == 12)
    return 0;
  else if (self->is_am || self->hour == 12)
    return self->hour;
  else
    return self->hour + 12;
}

guint
cc_time_entry_get_minute (CcTimeEntry *self)
{
  g_return_val_if_fail (CC_IS_TIME_ENTRY (self), 0);

  return self->minute;
}

gboolean
cc_time_entry_get_is_am (CcTimeEntry *self)
{
  g_return_val_if_fail (CC_IS_TIME_ENTRY (self), FALSE);

  if (self->is_am_pm)
    return self->is_am;

  return self->hour < 12;
}

void
cc_time_entry_set_is_am (CcTimeEntry *self,
                         gboolean     is_am)
{
  g_return_if_fail (CC_IS_TIME_ENTRY (self));

  self->is_am = !!is_am;
  g_signal_emit (self, signals[TIME_CHANGED], 0);
}

gboolean
cc_time_entry_get_am_pm (CcTimeEntry *self)
{
  g_return_val_if_fail (CC_IS_TIME_ENTRY (self), FALSE);

  return self->is_am_pm;
}

void
cc_time_entry_set_am_pm (CcTimeEntry *self,
                         gboolean     is_am_pm)
{
  g_return_if_fail (CC_IS_TIME_ENTRY (self));

  if (self->is_am_pm == !!is_am_pm)
    return;

  if (self->hour < 12)
    self->is_am = TRUE;
  else
    self->is_am = FALSE;

  if (is_am_pm)
    {
      if (self->hour == 0)
        self->hour = 12;
      else if (self->hour > 12)
        self->hour = self->hour - 12;
    }
  else
    {
      if (self->hour == 12 && self->is_am)
        self->hour = 0;
      else if (!self->is_am)
        self->hour = self->hour + 12;
    }

  self->is_am_pm = !!is_am_pm;
  time_entry_fill_time (self);
}
