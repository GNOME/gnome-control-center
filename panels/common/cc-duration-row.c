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
#include <glib-object.h>
#include <gtk/gtk.h>

#include "cc-duration-editor.h"
#include "cc-duration-row.h"
#include "cc-util.h"

/**
 * CcDurationRow:
 *
 * An #AdwActionRow used to enter a duration, represented as hours and minutes.
 *
 * The currently specified duration is shown in a label. If the row is activated
 * a popover is shown containing a #CcDurationEditor to edit the duration.
 */
struct _CcDurationRow {
  AdwActionRow parent_instance;

  GtkWidget *arrow_box;
  GtkLabel *current;
  GtkPopover *popover;
  CcDurationEditor *editor;
};

G_DEFINE_TYPE (CcDurationRow, cc_duration_row, ADW_TYPE_ACTION_ROW)

typedef enum {
  PROP_DURATION = 1,
  PROP_MINIMUM,
  PROP_MAXIMUM,
} CcDurationRowProperty;

static GParamSpec *props[PROP_MAXIMUM + 1];

static void cc_duration_row_get_property (GObject    *object,
                                          guint       property_id,
                                          GValue     *value,
                                          GParamSpec *pspec);
static void cc_duration_row_set_property (GObject      *object,
                                          guint         property_id,
                                          const GValue *value,
                                          GParamSpec   *pspec);
static void cc_duration_row_dispose (GObject *object);
static void cc_duration_row_size_allocate (GtkWidget *widget,
                                           int        width,
                                           int        height,
                                           int        baseline);
static gboolean cc_duration_row_focus (GtkWidget        *widget,
                                       GtkDirectionType  direction);
static void cc_duration_row_activate (AdwActionRow *row);
static void popover_notify_visible_cb (GObject    *object,
                                       GParamSpec *pspec,
                                       gpointer    user_data);
static void update_current_label (CcDurationRow *self);
static void editor_notify_duration_cb (GObject    *object,
                                       GParamSpec *pspec,
                                       gpointer    user_data);
static void editor_notify_minimum_cb (GObject    *object,
                                      GParamSpec *pspec,
                                      gpointer    user_data);
static void editor_notify_maximum_cb (GObject    *object,
                                      GParamSpec *pspec,
                                      gpointer    user_data);

static void
cc_duration_row_class_init (CcDurationRowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  AdwActionRowClass *row_class = ADW_ACTION_ROW_CLASS (klass);

  object_class->get_property = cc_duration_row_get_property;
  object_class->set_property = cc_duration_row_set_property;
  object_class->dispose = cc_duration_row_dispose;

  widget_class->size_allocate = cc_duration_row_size_allocate;
  widget_class->focus = cc_duration_row_focus;

  row_class->activate = cc_duration_row_activate;

  /**
   * CcDurationRow:duration:
   *
   * Duration displayed in the row or chosen in the editor, in minutes.
   */
  props[PROP_DURATION] =
    g_param_spec_uint ("duration",
                       NULL, NULL,
                       0, G_MAXUINT, 0,
                       G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * CcDurationRow:minimum:
   *
   * Minimum allowed value (inclusive) for #CcDurationRow:duration, in
   * minutes.
   *
   * If this is changed and the current value of #CcDurationRow:duration is
   * lower than it, the value of #CcDurationRow:duration will automatically
   * be clamped to the new minimum.
   */
  props[PROP_MINIMUM] =
    g_param_spec_uint ("minimum",
                       NULL, NULL,
                       0, G_MAXUINT, 0,
                       G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * CcDurationRow:maximum:
   *
   * Maximum allowed value (inclusive) for #CcDurationRow:duration, in
   * minutes.
   *
   * If this is changed and the current value of #CcDurationRow:duration is
   * higher than it, the value of #CcDurationRow:duration will automatically
   * be clamped to the new maximum.
   */
  props[PROP_MAXIMUM] =
    g_param_spec_uint ("maximum",
                       NULL, NULL,
                       0, G_MAXUINT, G_MAXUINT,
                       G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, G_N_ELEMENTS (props), props);

  g_type_ensure (CC_TYPE_DURATION_EDITOR);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/common/cc-duration-row.ui");

  gtk_widget_class_bind_template_child (widget_class, CcDurationRow, current);
  gtk_widget_class_bind_template_child (widget_class, CcDurationRow, arrow_box);
  gtk_widget_class_bind_template_child (widget_class, CcDurationRow, editor);
  gtk_widget_class_bind_template_child (widget_class, CcDurationRow, popover);
  gtk_widget_class_bind_template_callback (widget_class, popover_notify_visible_cb);
  gtk_widget_class_bind_template_callback (widget_class, editor_notify_duration_cb);
  gtk_widget_class_bind_template_callback (widget_class, editor_notify_minimum_cb);
  gtk_widget_class_bind_template_callback (widget_class, editor_notify_maximum_cb);

  gtk_widget_class_set_accessible_role (widget_class, GTK_ACCESSIBLE_ROLE_COMBO_BOX);
}

static void
cc_duration_row_init (CcDurationRow *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  update_current_label (self);
}

static void
cc_duration_row_get_property (GObject    *object,
                              guint       property_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
  CcDurationRow *self = CC_DURATION_ROW (object);

  switch ((CcDurationRowProperty) property_id)
    {
    case PROP_DURATION:
      g_value_set_uint (value, cc_duration_row_get_duration (self));
      break;
    case PROP_MINIMUM:
      g_value_set_uint (value, cc_duration_row_get_minimum (self));
      break;
    case PROP_MAXIMUM:
      g_value_set_uint (value, cc_duration_row_get_maximum (self));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
cc_duration_row_set_property (GObject      *object,
                              guint         property_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  CcDurationRow *self = CC_DURATION_ROW (object);

  switch ((CcDurationRowProperty) property_id)
    {
    case PROP_DURATION:
      cc_duration_row_set_duration (self, g_value_get_uint (value));
      break;
    case PROP_MINIMUM:
      cc_duration_row_set_minimum (self, g_value_get_uint (value));
      break;
    case PROP_MAXIMUM:
      cc_duration_row_set_maximum (self, g_value_get_uint (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
cc_duration_row_dispose (GObject *object)
{
  gtk_widget_dispose_template (GTK_WIDGET (object), CC_TYPE_DURATION_ROW);

  G_OBJECT_CLASS (cc_duration_row_parent_class)->dispose (object);
}

static void
cc_duration_row_size_allocate (GtkWidget *widget,
                               int        width,
                               int        height,
                               int        baseline)
{
  CcDurationRow *self = CC_DURATION_ROW (widget);

  GTK_WIDGET_CLASS (cc_duration_row_parent_class)->size_allocate (widget, width, height, baseline);

  gtk_popover_present (self->popover);
}

static gboolean
cc_duration_row_focus (GtkWidget        *widget,
                       GtkDirectionType  direction)
{
  CcDurationRow *self = CC_DURATION_ROW (widget);

  if (self->popover != NULL && gtk_widget_get_visible (GTK_WIDGET (self->popover)))
    return gtk_widget_child_focus (GTK_WIDGET (self->popover), direction);
  else
    return GTK_WIDGET_CLASS (cc_duration_row_parent_class)->focus (widget, direction);
}

static void
cc_duration_row_activate (AdwActionRow *row)
{
  CcDurationRow *self = CC_DURATION_ROW (row);

  if (gtk_widget_get_visible (self->arrow_box))
    gtk_popover_popup (self->popover);
}

static void
popover_notify_visible_cb (GObject    *object,
                           GParamSpec *pspec,
                           gpointer    user_data)
{
  CcDurationRow *self = CC_DURATION_ROW (user_data);

  if (gtk_widget_get_visible (GTK_WIDGET (self->popover)))
    gtk_widget_add_css_class (GTK_WIDGET (self), "has-open-popup");
  else
    gtk_widget_remove_css_class (GTK_WIDGET (self), "has-open-popup");
}

static void
update_current_label (CcDurationRow *self)
{
  g_autofree char *duration_str = NULL;

  duration_str = cc_util_time_to_string_text (cc_duration_editor_get_duration (self->editor) * 60 * 1000);
  gtk_label_set_label (self->current, duration_str);
}

static void
editor_notify_duration_cb (GObject    *object,
                           GParamSpec *pspec,
                           gpointer    user_data)
{
  CcDurationRow *self = CC_DURATION_ROW (user_data);

  update_current_label (self);

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_DURATION]);
}

static void
editor_notify_minimum_cb (GObject    *object,
                          GParamSpec *pspec,
                          gpointer    user_data)
{
  CcDurationRow *self = CC_DURATION_ROW (user_data);

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_MINIMUM]);
}

static void
editor_notify_maximum_cb (GObject    *object,
                          GParamSpec *pspec,
                          gpointer    user_data)
{
  CcDurationRow *self = CC_DURATION_ROW (user_data);

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_MAXIMUM]);
}

/**
 * cc_duration_row_new:
 *
 * Create a new #CcDurationRow.
 *
 * Returns: (transfer full): the new #CcDurationRow
 */
CcDurationRow *
cc_duration_row_new (void)
{
  return g_object_new (CC_TYPE_DURATION_ROW, NULL);
}

/**
 * cc_duration_row_get_duration:
 * @self: a #CcDurationRow
 *
 * Get the value of #CcDurationRow:duration.
 *
 * Returns: number of minutes currently specified by the row
 */
guint
cc_duration_row_get_duration (CcDurationRow *self)
{
  g_return_val_if_fail (CC_IS_DURATION_ROW (self), 0);

  return cc_duration_editor_get_duration (self->editor);
}

/**
 * cc_duration_row_set_duration:
 * @self: a #CcDurationRow
 * @duration: the duration, in minutes
 *
 * Set the value of #CcDurationRow:duration.
 */
void
cc_duration_row_set_duration (CcDurationRow *self,
                              guint          duration)
{
  g_return_if_fail (CC_IS_DURATION_ROW (self));

  cc_duration_editor_set_duration (self->editor, duration);
  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_DURATION]);
}

/**
 * cc_duration_row_get_minimum:
 * @self: a #CcDurationRow
 *
 * Get the value of #CcDurationRow:minimum.
 *
 * Returns: minimum value allowed for the duration, in minutes
 */
guint
cc_duration_row_get_minimum (CcDurationRow *self)
{
  g_return_val_if_fail (CC_IS_DURATION_ROW (self), 0);

  return cc_duration_editor_get_minimum (self->editor);
}

/**
 * cc_duration_row_set_minimum:
 * @self: a #CcDurationRow
 * @minimum: minimum value allowed for the duration, in minutes
 *
 * Set the value of #CcDurationRow:minimum to @minimum.
 *
 * If the current value of #CcDurationRow:duration is lower than @minimum, it
 * will automatically be clamped to @minimum.
 */
void
cc_duration_row_set_minimum (CcDurationRow *self,
                             guint          minimum)
{
  g_return_if_fail (CC_IS_DURATION_ROW (self));

  cc_duration_editor_set_minimum (self->editor, minimum);
}

/**
 * cc_duration_row_get_maximum:
 * @self: a #CcDurationRow
 *
 * Get the value of #CcDurationRow:maximum.
 *
 * Returns: maximum value allowed for the duration, in minutes
 */
guint
cc_duration_row_get_maximum (CcDurationRow *self)
{
  g_return_val_if_fail (CC_IS_DURATION_ROW (self), 0);

  return cc_duration_editor_get_maximum (self->editor);
}

/**
 * cc_duration_row_set_maximum:
 * @self: a #CcDurationRow
 * @maximum: maximum value allowed for the duration, in minutes
 *
 * Set the value of #CcDurationRow:maximum to @maximum.
 *
 * If the current value of #CcDurationRow:duration is higher than @maximum,
 * it will automatically be clamped to @maximum.
 */
void
cc_duration_row_set_maximum (CcDurationRow *self,
                             guint          maximum)
{
  g_return_if_fail (CC_IS_DURATION_ROW (self));

  cc_duration_editor_set_maximum (self->editor, maximum);
}
