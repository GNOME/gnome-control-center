/* -*- mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*- */
/* cc-timelike-editor.c
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
#define G_LOG_DOMAIN "cc-timelike-editor"

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#define GNOME_DESKTOP_USE_UNSTABLE_API
#include <libgnome-desktop/gnome-wall-clock.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>

#include "cc-timelike-entry.h"
#include "cc-timelike-editor.h"
#include "cc-timelike-editor-enums.h"
#include "cc-timelike-editor-layout.h"


#define TIMEOUT_INITIAL  500
#define TIMEOUT_REPEAT    50

#define FILECHOOSER_SCHEMA "org.gtk.Settings.FileChooser"
#define CLOCK_SCHEMA       "org.gnome.desktop.interface"
#define CLOCK_FORMAT_KEY   "clock-format"
#define SECONDS_PER_MINUTE  (60)
#define SECONDS_PER_HOUR    (60 * 60)
#define SECONDS_PER_DAY     (60 * 60 * 24)


struct _CcTimelikeEditor
{
  GtkWidget  parent_instance;

  GtkButton *am_pm_button;
  GtkStack  *am_pm_stack;
  GtkLabel  *am_label;
  GtkLabel  *pm_label;
  GtkButton *hour_up_button;
  GtkButton *hour_down_button;
  GtkButton *minute_up_button;
  GtkButton *minute_down_button;
  CcTimelikeEntry *timelike_entry;

  GtkButton *clicked_button; /* The button currently being clicked */
  GSettings *clock_settings;
  GSettings *filechooser_settings;

  guint      timer_id;

  CcTimelikeEditorMode mode;
};

G_DEFINE_TYPE (CcTimelikeEditor, cc_timelike_editor, GTK_TYPE_WIDGET)

typedef enum {
  PROP_MODE = 1,
  PROP_MINUTE_INCREMENT,
} CcTimelikeEditorProperty;

static GParamSpec *props[PROP_MINUTE_INCREMENT + 1];

enum {
  TIME_CHANGED,
  N_SIGNALS
};

static guint signals[N_SIGNALS];

static void
update_am_pm_widgets (CcTimelikeEditor *self)
{
  GDesktopClockFormat value;
  gboolean is_am_pm, is_time;

  g_assert (CC_IS_TIMELIKE_EDITOR (self));

  value = g_settings_get_enum (self->clock_settings, CLOCK_FORMAT_KEY);
  is_am_pm = value == G_DESKTOP_CLOCK_FORMAT_12H;
  is_time = (self->mode == CC_TIMELIKE_EDITOR_MODE_TIME);

  cc_timelike_entry_set_am_pm (self->timelike_entry, is_am_pm);

  gtk_widget_set_visible (GTK_WIDGET (self->am_pm_button), is_am_pm && is_time);

  if (is_am_pm && is_time)
    {
      if (cc_timelike_entry_get_is_am (self->timelike_entry))
        gtk_stack_set_visible_child (self->am_pm_stack, GTK_WIDGET (self->am_label));
      else
        gtk_stack_set_visible_child (self->am_pm_stack, GTK_WIDGET (self->pm_label));
    }
}

static void
timelike_editor_clock_changed_cb (CcTimelikeEditor *self)
{
  update_am_pm_widgets (self);
}

static void
timelike_editor_time_changed_cb (CcTimelikeEditor *self)
{
  g_assert (CC_IS_TIMELIKE_EDITOR (self));

  update_am_pm_widgets (self);
  g_signal_emit (self, signals[TIME_CHANGED], 0);
}

static void
editor_change_time_clicked_cb (CcTimelikeEditor *self,
                               GtkButton        *button)
{
  g_assert (CC_IS_TIMELIKE_EDITOR (self));

  if (button == NULL)
    return;

  if (button == self->hour_up_button)
    {
      gtk_editable_set_position (GTK_EDITABLE (self->timelike_entry), 0);
      g_signal_emit_by_name (self->timelike_entry, "change-value", GTK_SCROLL_STEP_UP);
    }
  else if (button == self->hour_down_button)
    {
      gtk_editable_set_position (GTK_EDITABLE (self->timelike_entry), 0);
      g_signal_emit_by_name (self->timelike_entry, "change-value", GTK_SCROLL_STEP_DOWN);
    }
  else if (button == self->minute_up_button)
    {
      gtk_editable_set_position (GTK_EDITABLE (self->timelike_entry), 3);
      g_signal_emit_by_name (self->timelike_entry, "change-value", GTK_SCROLL_STEP_UP);
    }
  else if (button == self->minute_down_button)
    {
      gtk_editable_set_position (GTK_EDITABLE (self->timelike_entry), 3);
      g_signal_emit_by_name (self->timelike_entry, "change-value", GTK_SCROLL_STEP_DOWN);
    }
}

static gboolean
editor_change_time_repeat (CcTimelikeEditor *self)
{
  if (self->clicked_button == NULL)
    {
      self->timer_id = 0;

      return G_SOURCE_REMOVE;
    }

  editor_change_time_clicked_cb (self, self->clicked_button);

  return G_SOURCE_CONTINUE;
}

static gboolean
editor_change_time_cb (CcTimelikeEditor *self)
{
  g_assert (CC_IS_TIMELIKE_EDITOR (self));
  g_clear_handle_id (&self->timer_id, g_source_remove);

  editor_change_time_clicked_cb (self, self->clicked_button);
  self->timer_id = g_timeout_add (TIMEOUT_REPEAT,
                                  (GSourceFunc)editor_change_time_repeat,
                                  self);
  return G_SOURCE_REMOVE;
}

static gboolean
editor_change_time_pressed_cb (CcTimelikeEditor *self,
                               gint              n_press,
                               gdouble           x,
                               gdouble           y,
                               GtkGestureClick  *click_gesture)
{
  GtkWidget *button;

  g_assert (CC_IS_TIMELIKE_EDITOR (self));

  button = gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (click_gesture));

  self->clicked_button = GTK_BUTTON (button);
  /* Keep changing time until the press is released */
  self->timer_id = g_timeout_add (TIMEOUT_INITIAL,
                                  (GSourceFunc)editor_change_time_cb,
                                  self);
  editor_change_time_clicked_cb (self, GTK_BUTTON (button));
  return FALSE;
}

static gboolean
editor_change_time_released_cb (CcTimelikeEditor *self)
{
  self->clicked_button = NULL;
  g_clear_handle_id (&self->timer_id, g_source_remove);

  return FALSE;
}

static void
editor_am_pm_button_clicked_cb (CcTimelikeEditor *self)
{
  gboolean is_am;

  g_assert (CC_IS_TIMELIKE_EDITOR (self));
  g_assert (cc_timelike_entry_get_am_pm (self->timelike_entry));

  is_am = cc_timelike_entry_get_is_am (self->timelike_entry);
  /* Toggle AM PM */
  cc_timelike_entry_set_is_am (self->timelike_entry, !is_am);
  update_am_pm_widgets (self);
}

static void
editor_am_pm_stack_changed_cb (CcTimelikeEditor *self)
{
  GtkWidget *label;
  const gchar *text;

  g_assert (CC_IS_TIMELIKE_EDITOR (self));

  label = gtk_stack_get_visible_child (self->am_pm_stack);
  text = gtk_label_get_text (GTK_LABEL (label));
  gtk_accessible_update_property (GTK_ACCESSIBLE (self->am_pm_button),
                                  GTK_ACCESSIBLE_PROPERTY_LABEL, text,
                                  -1);
}

static void
cc_timelike_editor_constructed (GObject *object)
{
  CcTimelikeEditor *self = (CcTimelikeEditor *)object;
  GDateTime *date;
  char *label;

  G_OBJECT_CLASS (cc_timelike_editor_parent_class)->constructed (object);

  /* Set localized identifier for AM */
  date = g_date_time_new_utc (1, 1, 1, 0, 0, 0);
  label = g_date_time_format (date, "%p");
  gtk_label_set_label (self->am_label, label);
  g_date_time_unref (date);
  g_free (label);

  /* Set localized identifier for PM */
  date = g_date_time_new_utc (1, 1, 1, 12, 0, 0);
  label = g_date_time_format (date, "%p");
  gtk_label_set_label (self->pm_label, label);
  g_date_time_unref (date);
  g_free (label);
}

static void
cc_timelike_editor_get_property (GObject    *object,
                                 guint       property_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  CcTimelikeEditor *self = CC_TIMELIKE_EDITOR (object);

  switch ((CcTimelikeEditorProperty) property_id)
    {
    case PROP_MODE:
      g_value_set_enum (value, cc_timelike_editor_get_mode (self));
      break;
    case PROP_MINUTE_INCREMENT:
      g_value_set_uint (value, cc_timelike_editor_get_minute_increment (self));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
cc_timelike_editor_set_property (GObject      *object,
                                 guint         property_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  CcTimelikeEditor *self = CC_TIMELIKE_EDITOR (object);

  switch ((CcTimelikeEditorProperty) property_id)
    {
    case PROP_MODE:
      cc_timelike_editor_set_mode (self, g_value_get_enum (value));
      break;
    case PROP_MINUTE_INCREMENT:
      cc_timelike_editor_set_minute_increment (self, g_value_get_uint (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
cc_timelike_editor_dispose (GObject *object)
{
  gtk_widget_dispose_template (GTK_WIDGET (object), CC_TYPE_TIMELIKE_EDITOR);

  G_OBJECT_CLASS (cc_timelike_editor_parent_class)->dispose (object);
}

static void
cc_timelike_editor_finalize (GObject *object)
{
  CcTimelikeEditor *self = (CcTimelikeEditor *)object;

  g_clear_handle_id (&self->timer_id, g_source_remove);
  g_clear_object (&self->clock_settings);
  g_clear_object (&self->filechooser_settings);

  G_OBJECT_CLASS (cc_timelike_editor_parent_class)->finalize (object);
}

static void
cc_timelike_editor_class_init (CcTimelikeEditorClass *klass)
{
  GObjectClass   *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->constructed = cc_timelike_editor_constructed;
  object_class->get_property = cc_timelike_editor_get_property;
  object_class->set_property = cc_timelike_editor_set_property;
  object_class->dispose = cc_timelike_editor_dispose;
  object_class->finalize = cc_timelike_editor_finalize;

  /**
   * CcTimelikeEditor:mode:
   *
   * What kind of time the editor is meant to represent — a wall clock time,
   * or a duration.
   *
   * This affects whether the AM/PM buttons are potentially shown.
   */
  props[PROP_MODE] =
    g_param_spec_enum ("mode",
                       NULL, NULL,
                       CC_TYPE_TIMELIKE_EDITOR_MODE,
                       CC_TIMELIKE_EDITOR_MODE_TIME,
                       G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * CcTimelikeEditor:minute-increment:
   *
   * Number of minutes the up/down buttons change the time by, which will
 *   always be in the range [1, 59].
   */
  props[PROP_MINUTE_INCREMENT] =
    g_param_spec_uint ("minute-increment",
                       NULL, NULL,
                       1, 59, 1,
                       G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, G_N_ELEMENTS (props), props);

  signals[TIME_CHANGED] =
    g_signal_new ("time-changed",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_FIRST,
                  0, NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 0);

  gtk_widget_class_set_template_from_resource (widget_class,
                                               "/org/gnome/control-center/"
                                               "common/cc-timelike-editor.ui");
  gtk_widget_class_set_layout_manager_type (widget_class, CC_TYPE_TIMELIKE_EDITOR_LAYOUT);

  gtk_widget_class_bind_template_child (widget_class, CcTimelikeEditor, am_pm_button);
  gtk_widget_class_bind_template_child (widget_class, CcTimelikeEditor, am_pm_stack);
  gtk_widget_class_bind_template_child (widget_class, CcTimelikeEditor, am_label);
  gtk_widget_class_bind_template_child (widget_class, CcTimelikeEditor, pm_label);
  gtk_widget_class_bind_template_child (widget_class, CcTimelikeEditor, hour_up_button);
  gtk_widget_class_bind_template_child (widget_class, CcTimelikeEditor, hour_down_button);
  gtk_widget_class_bind_template_child (widget_class, CcTimelikeEditor, minute_up_button);
  gtk_widget_class_bind_template_child (widget_class, CcTimelikeEditor, minute_down_button);
  gtk_widget_class_bind_template_child (widget_class, CcTimelikeEditor, timelike_entry);

  gtk_widget_class_bind_template_callback (widget_class, editor_change_time_pressed_cb);
  gtk_widget_class_bind_template_callback (widget_class, editor_change_time_released_cb);
  gtk_widget_class_bind_template_callback (widget_class, editor_am_pm_button_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, editor_am_pm_stack_changed_cb);

  g_type_ensure (CC_TYPE_TIMELIKE_ENTRY);
}

static void
cc_timelike_editor_init (CcTimelikeEditor *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  self->mode = CC_TIMELIKE_EDITOR_MODE_TIME;
  self->clock_settings = g_settings_new (CLOCK_SCHEMA);
  self->filechooser_settings = g_settings_new (FILECHOOSER_SCHEMA);

  g_signal_connect_object (self->clock_settings, "changed::" CLOCK_FORMAT_KEY,
                           G_CALLBACK (timelike_editor_clock_changed_cb), self,
                           G_CONNECT_SWAPPED);
  g_signal_connect_swapped (self->timelike_entry, "time-changed",
                            G_CALLBACK (timelike_editor_time_changed_cb), self);
  update_am_pm_widgets (self);
}

CcTimelikeEditor *
cc_timelike_editor_new (void)
{
  return g_object_new (CC_TYPE_TIMELIKE_EDITOR, NULL);
}

void
cc_timelike_editor_set_time (CcTimelikeEditor *self,
                             guint             hour,
                             guint             minute)
{
  g_return_if_fail (CC_IS_TIMELIKE_EDITOR (self));

  cc_timelike_entry_set_time (self->timelike_entry, hour, minute);
}

guint
cc_timelike_editor_get_hour (CcTimelikeEditor *self)
{
  g_return_val_if_fail (CC_IS_TIMELIKE_EDITOR (self), 0);

  return cc_timelike_entry_get_hour (self->timelike_entry);
}

guint
cc_timelike_editor_get_minute (CcTimelikeEditor *self)
{
  g_return_val_if_fail (CC_IS_TIMELIKE_EDITOR (self), 0);

  return cc_timelike_entry_get_minute (self->timelike_entry);
}

/**
 * cc_timelike_editor_get_mode:
 * @self: a #CcTimelikeEditor
 *
 * Get the value of #CcTimelikeEditor:mode.
 *
 * Returns: the current editor mode
 */
CcTimelikeEditorMode
cc_timelike_editor_get_mode (CcTimelikeEditor *self)
{
  g_return_val_if_fail (CC_IS_TIMELIKE_EDITOR (self), CC_TIMELIKE_EDITOR_MODE_TIME);

  return self->mode;
}

/**
 * cc_timelike_editor_set_mode:
 * @self: a #CcTimelikeEditor
 * @mode: new editor mode
 *
 * Set the value of #CcTimelikeEditor:mode.
 */
void
cc_timelike_editor_set_mode (CcTimelikeEditor     *self,
                             CcTimelikeEditorMode  mode)
{
  g_return_if_fail (CC_IS_TIMELIKE_EDITOR (self));

  if (self->mode == mode)
    return;

  g_object_freeze_notify (G_OBJECT (self));

  self->mode = mode;
  update_am_pm_widgets (self);
  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_MODE]);

  g_object_thaw_notify (G_OBJECT (self));
}

/**
 * cc_timelike_editor_get_minute_increment:
 * @self: a #CcTimelikeEditor
 *
 * Get the value of #CcTimelikeEditor:minute-increment.
 *
 * Returns: number of minutes the up/down buttons change the time by, which will
 *   always be in the range [1, 59]
 */
guint
cc_timelike_editor_get_minute_increment (CcTimelikeEditor *self)
{
  guint minutes;

  g_return_val_if_fail (CC_IS_TIMELIKE_EDITOR (self), 1);

  minutes = cc_timelike_entry_get_minute_increment (self->timelike_entry);

  g_assert (minutes > 0 && minutes < 60);

  return minutes;
}

/**
 * cc_timelike_editor_set_minute_increment:
 * @self: a #CcTimelikeEditor
 * @minutes: number of minutes the up/down buttons change the time by; must be
 *   in the range [1, 59]
 *
 * Set the value of #CcTimelikeEditor:minute-increment.
 */
void
cc_timelike_editor_set_minute_increment (CcTimelikeEditor *self,
                                         guint             minutes)
{
  g_return_if_fail (CC_IS_TIMELIKE_EDITOR (self));
  g_return_if_fail (minutes > 0 && minutes < 60);

  cc_timelike_entry_set_minute_increment (self->timelike_entry, minutes);
}
