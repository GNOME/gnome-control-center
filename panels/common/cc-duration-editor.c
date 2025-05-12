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

#include <glib-object.h>
#include <gtk/gtk.h>

#include "cc-duration-editor.h"
#include "cc-timelike-editor.h"

/**
 * CcDurationEditor:
 *
 * An editor for time durations. It shows the hours and minutes of the duration,
 * plus buttons to increment or decrement them, and allows the user to also
 * type a duration in using the keyboard.
 *
 * Contrast with #CcTimeEditor, which is an editor for wall clock times. It
 * looks very similar, but constrains its values to [00:00, 23:59]. Durations
 * may not necessarily be constrained to that (although currently they are, as
 * no users of the widget need anything else).
 */
struct _CcDurationEditor {
  GtkWidget parent_instance;

  CcTimelikeEditor *editor;
  guint minimum;
  guint maximum;
};

G_DEFINE_TYPE (CcDurationEditor, cc_duration_editor, GTK_TYPE_WIDGET)

typedef enum {
  PROP_DURATION = 1,
  PROP_MINIMUM,
  PROP_MAXIMUM,
} CcDurationEditorProperty;

static GParamSpec *props[PROP_MAXIMUM + 1];

static void cc_duration_editor_get_property (GObject    *object,
                                             guint       property_id,
                                             GValue     *value,
                                             GParamSpec *pspec);
static void cc_duration_editor_set_property (GObject      *object,
                                             guint         property_id,
                                             const GValue *value,
                                             GParamSpec   *pspec);
static void cc_duration_editor_dispose (GObject *object);
static void editor_time_changed_cb (CcTimelikeEditor *editor,
                                    gpointer          user_data);

static void
cc_duration_editor_class_init (CcDurationEditorClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->get_property = cc_duration_editor_get_property;
  object_class->set_property = cc_duration_editor_set_property;
  object_class->dispose = cc_duration_editor_dispose;

  /**
   * CcDurationEditor:duration:
   *
   * Duration displayed or chosen in the editor, in minutes.
   */
  props[PROP_DURATION] =
    g_param_spec_uint ("duration",
                       NULL, NULL,
                       0, G_MAXUINT, 0,
                       G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * CcDurationEditor:minimum:
   *
   * Minimum allowed value (inclusive) for #CcDurationEditor:duration, in
   * minutes.
   *
   * If this is changed and the current value of #CcDurationEditor:duration is
   * lower than it, the value of #CcDurationEditor:duration will automatically
   * be clamped to the new minimum.
   */
  props[PROP_MINIMUM] =
    g_param_spec_uint ("minimum",
                       NULL, NULL,
                       0, G_MAXUINT, 0,
                       G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * CcDurationEditor:maximum:
   *
   * Maximum allowed value (inclusive) for #CcDurationEditor:duration, in
   * minutes.
   *
   * If this is changed and the current value of #CcDurationEditor:duration is
   * higher than it, the value of #CcDurationEditor:duration will automatically
   * be clamped to the new maximum.
   */
  props[PROP_MAXIMUM] =
    g_param_spec_uint ("maximum",
                       NULL, NULL,
                       0, G_MAXUINT, G_MAXUINT,
                       G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, G_N_ELEMENTS (props), props);

  g_type_ensure (CC_TYPE_TIMELIKE_EDITOR);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/common/cc-duration-editor.ui");

  gtk_widget_class_bind_template_child (widget_class, CcDurationEditor, editor);

  gtk_widget_class_bind_template_callback (widget_class, editor_time_changed_cb);

  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BOX_LAYOUT);
}

static void
cc_duration_editor_init (CcDurationEditor *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  self->maximum = G_MAXUINT;
}

static void
cc_duration_editor_get_property (GObject    *object,
                                 guint       property_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  CcDurationEditor *self = CC_DURATION_EDITOR (object);

  switch ((CcDurationEditorProperty) property_id)
    {
    case PROP_DURATION:
      g_value_set_uint (value, cc_duration_editor_get_duration (self));
      break;
    case PROP_MINIMUM:
      g_value_set_uint (value, cc_duration_editor_get_minimum (self));
      break;
    case PROP_MAXIMUM:
      g_value_set_uint (value, cc_duration_editor_get_maximum (self));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
cc_duration_editor_set_property (GObject      *object,
                                 guint         property_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  CcDurationEditor *self = CC_DURATION_EDITOR (object);

  switch ((CcDurationEditorProperty) property_id)
    {
    case PROP_DURATION:
      cc_duration_editor_set_duration (self, g_value_get_uint (value));
      break;
    case PROP_MINIMUM:
      cc_duration_editor_set_minimum (self, g_value_get_uint (value));
      break;
    case PROP_MAXIMUM:
      cc_duration_editor_set_maximum (self, g_value_get_uint (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
cc_duration_editor_dispose (GObject *object)
{
  gtk_widget_dispose_template (GTK_WIDGET (object), CC_TYPE_DURATION_EDITOR);

  G_OBJECT_CLASS (cc_duration_editor_parent_class)->dispose (object);
}

static void
editor_time_changed_cb (CcTimelikeEditor *editor,
                        gpointer          user_data)
{
  CcDurationEditor *self = CC_DURATION_EDITOR (user_data);
  guint duration;

  /* Clamp to the minimum/maximum. */
  duration = cc_duration_editor_get_duration (self);
  if (duration < self->minimum || duration > self->maximum)
    {
      cc_duration_editor_set_duration (self, CLAMP (duration, self->minimum, self->maximum));
      return;
    }

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_DURATION]);
}

/**
 * cc_duration_editor_new:
 *
 * Create a new #CcDurationEditor, to represent a duration and allow editing it.
 *
 * Returns: (transfer full): a new #CcDurationEditor
 */
CcDurationEditor *
cc_duration_editor_new (void)
{
  return g_object_new (CC_TYPE_DURATION_EDITOR, NULL);
}

/**
 * cc_duration_editor_get_duration:
 * @self: a #CcDurationEditor
 *
 * Get the value of #CcDurationEditor:duration.
 *
 * Returns: duration specified in the editor, in minutes
 */
guint
cc_duration_editor_get_duration (CcDurationEditor *self)
{
  g_return_val_if_fail (CC_IS_DURATION_EDITOR (self), 0);

  return cc_timelike_editor_get_hour (self->editor) * 60 + cc_timelike_editor_get_minute (self->editor);
}

/**
 * cc_duration_editor_set_duration:
 * @self: a #CcDurationEditor
 * @duration: duration to show in the editor, in minutes
 *
 * Set the value of #CcDurationEditor:duration to @duration.
 */
void
cc_duration_editor_set_duration (CcDurationEditor *self,
                                 guint             duration)
{
  guint hours, minutes;

  g_return_if_fail (CC_IS_DURATION_EDITOR (self));

  /* Clamp to the minimum/maximum. */
  duration = CLAMP (duration, self->minimum, self->maximum);

  hours = duration / 60;
  minutes = duration % 60;

  if (hours == cc_timelike_editor_get_hour (self->editor) &&
      minutes == cc_timelike_editor_get_minute (self->editor))
    return;

  cc_timelike_editor_set_time (self->editor, hours, minutes);

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_DURATION]);
}

/**
 * cc_duration_editor_get_minimum:
 * @self: a #CcDurationEditor
 *
 * Get the value of #CcDurationEditor:minimum.
 *
 * Returns: minimum value allowed for the duration, in minutes
 */
guint
cc_duration_editor_get_minimum (CcDurationEditor *self)
{
  g_return_val_if_fail (CC_IS_DURATION_EDITOR (self), 0);

  return self->minimum;
}

/**
 * cc_duration_editor_set_minimum:
 * @self: a #CcDurationEditor
 * @minimum: minimum value allowed for the duration, in minutes
 *
 * Set the value of #CcDurationEditor:minimum to @minimum.
 *
 * If the current value of #CcDurationEditor:duration is lower than @minimum, it
 * will automatically be clamped to @minimum.
 */
void
cc_duration_editor_set_minimum (CcDurationEditor *self,
                                guint             minimum)
{
  g_return_if_fail (CC_IS_DURATION_EDITOR (self));

  if (self->minimum == minimum)
    return;

  g_object_freeze_notify (G_OBJECT (self));

  self->minimum = minimum;
  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_MINIMUM]);

  /* Ensure the duration is clamped to the new range. */
  cc_duration_editor_set_duration (self, cc_duration_editor_get_duration (self));

  g_object_thaw_notify (G_OBJECT (self));
}

/**
 * cc_duration_editor_get_maximum:
 * @self: a #CcDurationEditor
 *
 * Get the value of #CcDurationEditor:maximum.
 *
 * Returns: maximum value allowed for the duration, in minutes
 */
guint
cc_duration_editor_get_maximum (CcDurationEditor *self)
{
  g_return_val_if_fail (CC_IS_DURATION_EDITOR (self), 0);

  return self->maximum;
}

/**
 * cc_duration_editor_set_maximum:
 * @self: a #CcDurationEditor
 * @maximum: maximum value allowed for the duration, in minutes
 *
 * Set the value of #CcDurationEditor:maximum to @maximum.
 *
 * If the current value of #CcDurationEditor:duration is higher than @maximum,
 * it will automatically be clamped to @maximum.
 */
void
cc_duration_editor_set_maximum (CcDurationEditor *self,
                                guint             maximum)
{
  g_return_if_fail (CC_IS_DURATION_EDITOR (self));

  if (self->maximum == maximum)
    return;

  g_object_freeze_notify (G_OBJECT (self));

  self->maximum = maximum;
  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_MAXIMUM]);

  /* Ensure the duration is clamped to the new range. */
  cc_duration_editor_set_duration (self, cc_duration_editor_get_duration (self));

  g_object_thaw_notify (G_OBJECT (self));
}
