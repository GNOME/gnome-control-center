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

#include <glib.h>
#include <glib-object.h>
#include <gtk/gtk.h>
#include <math.h>

#include "cc-bar-chart-bar.h"

/**
 * CcBarChartBar:
 *
 * #CcBarChartBar is an individual bar in a #CcBarChart.
 *
 * Bars may only be placed into a #CcBarChart as children of #CcBarChartGroup.
 *
 * A bar may be selected, indicated by #CcBarChartBar:is-selected. They may also
 * be activated by gtk_widget_activate(), which will result in
 * #CcBarChartBar::activate being emitted. By default this selects the bar.
 *
 * # CSS nodes
 *
 * |[<!-- language="plain" -->
 * bar[:hover][:selected]
 * ]|
 *
 * #CcBarChartBar uses a single CSS node named `bar`. A bar may have a `:hover`
 * or `:selected` pseudo-selector to indicate whether it’s selected or being
 * hovered over with the mouse.
 *
 * # Accessibility
 *
 * #CcBarChartBar uses the %GTK_ACCESSIBLE_ROLE_LIST_ITEM role.
 *
 * A textual description for its value must be provided.
 */
struct _CcBarChartBar {
  GtkWidget parent_instance;

  /* Configured state: */
  double value;
  char *accessible_description;
};

G_DEFINE_TYPE (CcBarChartBar, cc_bar_chart_bar, GTK_TYPE_WIDGET)

typedef enum {
  PROP_VALUE = 1,
  PROP_ACCESSIBLE_DESCRIPTION,
} CcBarChartBarProperty;

static GParamSpec *props[PROP_ACCESSIBLE_DESCRIPTION + 1];

typedef enum {
  SIGNAL_ACTIVATE,
} CcBarChartBarSignal;

static guint signals[SIGNAL_ACTIVATE + 1];

static void cc_bar_chart_bar_get_property (GObject    *object,
                                           guint       property_id,
                                           GValue     *value,
                                           GParamSpec *pspec);
static void cc_bar_chart_bar_set_property (GObject      *object,
                                           guint         property_id,
                                           const GValue *value,
                                           GParamSpec   *pspec);
static void cc_bar_chart_bar_dispose (GObject *object);
static void cc_bar_chart_bar_finalize (GObject *object);
static void cc_bar_chart_bar_measure (GtkWidget      *widget,
                                      GtkOrientation  orientation,
                                      int             for_size,
                                      int            *minimum,
                                      int            *natural,
                                      int            *minimum_baseline,
                                      int            *natural_baseline);

static void
cc_bar_chart_bar_class_init (CcBarChartBarClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->get_property = cc_bar_chart_bar_get_property;
  object_class->set_property = cc_bar_chart_bar_set_property;
  object_class->dispose = cc_bar_chart_bar_dispose;
  object_class->finalize = cc_bar_chart_bar_finalize;

  widget_class->measure = cc_bar_chart_bar_measure;

  /**
   * CcBarChartBar:value:
   *
   * The data value represented by the bar.
   *
   * Currently, only non-negative real numbers are supported.
   */
  props[PROP_VALUE] =
    g_param_spec_double ("value",
                         NULL, NULL,
                         0.0, G_MAXDOUBLE, 0.0,
                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * CcBarChartBar:accessible-description:
   *
   * An accessible label for the bar.
   *
   * This should succinctly describe the value of the bar, including any
   * necessary units.
   */
  props[PROP_ACCESSIBLE_DESCRIPTION] =
    g_param_spec_string ("accessible-description",
                         NULL, NULL,
                         NULL,
                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, G_N_ELEMENTS (props), props);

  /**
   * CcBarChartBar::activate:
   *
   * This is a keybinding signal, which will cause this bar to be activated.
   *
   * If you want to be notified when the user activates a bar (by key or not),
   * use the #CcBarChart::bar-activated signal on the bar’s parent #CcBarChart.
   */
  signals[SIGNAL_ACTIVATE] =
    g_signal_new ("activate",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
                  0, NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 0);

  gtk_widget_class_set_activate_signal (widget_class, signals[SIGNAL_ACTIVATE]);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/wellbeing/cc-bar-chart-bar.ui");

  gtk_widget_class_set_css_name (widget_class, "bar");
  gtk_widget_class_set_accessible_role (widget_class, GTK_ACCESSIBLE_ROLE_LIST_ITEM);
}

static void
cc_bar_chart_bar_init (CcBarChartBar *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

static void
cc_bar_chart_bar_get_property (GObject    *object,
                               guint       property_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  CcBarChartBar *self = CC_BAR_CHART_BAR (object);

  switch ((CcBarChartBarProperty) property_id)
    {
    case PROP_VALUE:
      g_value_set_double (value, cc_bar_chart_bar_get_value (self));
      break;
    case PROP_ACCESSIBLE_DESCRIPTION:
      g_value_set_string (value, cc_bar_chart_bar_get_accessible_description (self));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
cc_bar_chart_bar_set_property (GObject      *object,
                               guint         property_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  CcBarChartBar *self = CC_BAR_CHART_BAR (object);

  switch ((CcBarChartBarProperty) property_id)
    {
    case PROP_VALUE:
      cc_bar_chart_bar_set_value (self, g_value_get_double (value));
      break;
    case PROP_ACCESSIBLE_DESCRIPTION:
      cc_bar_chart_bar_set_accessible_description (self, g_value_get_string (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
cc_bar_chart_bar_dispose (GObject *object)
{
  gtk_widget_dispose_template (GTK_WIDGET (object), CC_TYPE_BAR_CHART_BAR);

  G_OBJECT_CLASS (cc_bar_chart_bar_parent_class)->dispose (object);
}

static void
cc_bar_chart_bar_finalize (GObject *object)
{
  CcBarChartBar *self = CC_BAR_CHART_BAR (object);

  g_free (self->accessible_description);

  G_OBJECT_CLASS (cc_bar_chart_bar_parent_class)->finalize (object);
}

static const unsigned int MINIMUM_BAR_WIDTH = 10;
static const unsigned int NATURAL_BAR_WIDTH = 40;

static void
cc_bar_chart_bar_measure (GtkWidget      *widget,
                          GtkOrientation  orientation,
                          int             for_size,
                          int            *minimum,
                          int            *natural,
                          int            *minimum_baseline,
                          int            *natural_baseline)
{
  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      *minimum = MINIMUM_BAR_WIDTH;
      *natural = NATURAL_BAR_WIDTH;
    }
  else if (orientation == GTK_ORIENTATION_VERTICAL)
    {
      *minimum = 0;
      *natural = 0;
    }
  else
    {
      g_assert_not_reached ();
    }
}

/**
 * cc_bar_chart_bar_new:
 * @value: the value the bar represents
 * @accessible_description: an accessible textual description for @value
 *
 * Create a new #CcBarChartBar.
 *
 * Returns: (transfer full): the new #CcBarChartBar
 */
CcBarChartBar *
cc_bar_chart_bar_new (double      value,
                      const char *accessible_description)
{
  g_return_val_if_fail (!isnan (value), NULL);
  g_return_val_if_fail (accessible_description != NULL, NULL);

  return g_object_new (CC_TYPE_BAR_CHART_BAR,
                       "value", value,
                       "accessible-description", accessible_description,
                       NULL);
}

/**
 * cc_bar_chart_bar_get_value:
 * @self: a #CcBarChartBar
 *
 * Get the value of #CcBarChartBar:value.
 *
 * Returns: value represented by the bar
 */
double
cc_bar_chart_bar_get_value (CcBarChartBar *self)
{
  g_return_val_if_fail (CC_IS_BAR_CHART_BAR (self), NAN);

  return self->value;
}

/**
 * cc_bar_chart_bar_set_value:
 * @self: a #CcBarChartBar
 * @value: value represented by the bar
 *
 * Set the value of #CcBarChartBar:value.
 */
void
cc_bar_chart_bar_set_value (CcBarChartBar *self,
                            double         value)
{
  g_return_if_fail (CC_IS_BAR_CHART_BAR (self));
  g_return_if_fail (!isnan (value));
  g_return_if_fail (value >= 0.0);  /* negative values aren’t currently supported */

  if (self->value == value)
    return;

  self->value = value;

  /* Re-render */
  gtk_widget_queue_resize (GTK_WIDGET (self));

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_VALUE]);
}

/**
 * cc_bar_chart_bar_get_accessible_description:
 * @self: a #CcBarChartBar
 *
 * Get the value of #CcBarChartBar:accessible-description.
 *
 * Returns: accessible textual description for the bar’s value
 */
const char *
cc_bar_chart_bar_get_accessible_description (CcBarChartBar *self)
{
  g_return_val_if_fail (CC_IS_BAR_CHART_BAR (self), NULL);

  return self->accessible_description;
}

/**
 * cc_bar_chart_bar_set_accessible_description:
 * @self: a #CcBarChartBar
 * @accessible_description: accessible textual description for the bar’s value
 *
 * Set the value of #CcBarChartBar:accessible-description.
 */
void
cc_bar_chart_bar_set_accessible_description (CcBarChartBar *self,
                                             const char    *accessible_description)
{
  g_return_if_fail (CC_IS_BAR_CHART_BAR (self));
  g_return_if_fail (accessible_description != NULL);

  if (!g_set_str (&self->accessible_description, accessible_description))
    return;

  gtk_accessible_update_property (GTK_ACCESSIBLE (self),
                                  GTK_ACCESSIBLE_PROPERTY_DESCRIPTION,
                                  self->accessible_description,
                                  -1);

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_ACCESSIBLE_DESCRIPTION]);
}
