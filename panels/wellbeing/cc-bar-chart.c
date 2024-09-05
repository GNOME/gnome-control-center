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
#include <gsk/gsk.h>
#include <gtk/gtk.h>
#include <math.h>

#include "cc-bar-chart.h"
#include "cc-bar-chart-bar.h"
#include "cc-bar-chart-group.h"
#include "cc-util.h"

/**
 * CcBarChart:
 *
 * #CcBarChart is a widget for displaying a
 * [bar chart](https://en.wikipedia.org/wiki/Bar_chart).
 *
 * It currently supports vertical bar charts, with a horizontal discrete axis
 * and a vertical continuous axis. It supports single (non-grouped) bars, and a
 * single colour scheme. It does not support negative data values.
 * These limitations may change in future.
 *
 * The labels on the discrete axis can be set using
 * cc_bar_chart_set_discrete_axis_labels(). You must localise these before
 * setting them. The number of discrete axis labels must match the number of
 * data elements set using cc_bar_chart_set_data().
 *
 * The widget’s appearance is undefined until data is provided to it via
 * cc_bar_chart_set_data(). If you need to present a placeholder if no data is
 * available, it’s recommended to place the #CcBarChart in a #GtkStack with the
 * placeholder widgets in another page of the stack. Placeholders could be an
 * image, text or spinner.
 *
 * The labels on the continuous axis are constructed dynamically to match the
 * grid lines. Provide a grid line generator callback using
 * cc_bar_chart_set_continuous_axis_grid_line_callback(), and provide a
 * labelling callback using cc_bar_chart_set_continuous_axis_label_callback().
 *
 * An overlay line may be rendered to indicate a target or average. Set its
 * value using cc_bar_chart_set_overlay_line_value();
 *
 * Bars in the chart may be selected, and the currently selected bar is
 * available as #CcBarChart:selected-index.
 *
 * # CSS nodes
 *
 * |[<!-- language="plain" -->
 * bar-chart
 * ├── label.discrete-axis-label
 * ├── label.continuous-axis-label
 * ╰── bar-group[:hover][:selected]
 *     ╰── bar[:hover][:selected]
 * ]|
 *
 * #CcBarChart uses a single CSS node named `bar-chart`. Each bar group is a
 * sub-node named `bar-group`, with `bar` sub-nodes beneath it. Bars and groups
 * may have `:hover` or `:selected` pseudo-selectors to indicate whether they
 * are selected or being hovered over with the mouse. Axis labels are `label`
 * sub-nodes, with either a `.discrete-axis-label` or `.continuous-axis-label`
 * class.
 */
struct _CcBarChart {
  GtkWidget parent_instance;

  /* Configured state: */
  double *data;  /* (nullable) (owned) */
  size_t n_data;

  char **discrete_axis_labels;  /* (nullable) (array zero-terminated=1) */
  size_t n_discrete_axis_labels;  /* cached result of g_strv_length(discrete_axis_labels) */

  CcBarChartLabelCallback continuous_axis_label_callback;
  void *continuous_axis_label_user_data;
  GDestroyNotify continuous_axis_label_destroy_notify;

  CcBarChartGridLineCallback continuous_axis_grid_line_callback;
  void *continuous_axis_grid_line_user_data;
  GDestroyNotify continuous_axis_grid_line_destroy_notify;

  gboolean selected_index_set;
  unsigned int selected_index;  /* undefined if !selected_index_set */

  double overlay_line_value;

  /* Layout and rendering cache. See cc-bar-chart-diagram.svg for a rough sketch
   * of how these child widgets and lengths fit into the overall widget. */
  GPtrArray *cached_discrete_axis_labels;  /* (owned) (nullable) (element-type GtkLabel), always indexed the same as data */
  GPtrArray *cached_continuous_axis_labels;  /* (owned) (nullable) (element-type GtkLabel), always indexed the same as cached_continuous_axis_grid_line_values */
  GArray *cached_continuous_axis_grid_line_values;  /* (owned) (nullable) (element-type double), always indexed the same as cached_continuous_axis_labels */
  GPtrArray *cached_groups;  /* (owned) (nullable) (element-type CcBarChartGroup), always indexed the same as data */
  int cached_continuous_axis_area_width;
  int cached_continuous_axis_label_height;
  int cached_discrete_axis_area_height;
  int cached_discrete_axis_baseline;  /* may be -1 if baseline is undefined */
  double cached_pixels_per_data;  /* > 0.0 */
  int cached_minimum_group_width;
  unsigned int cached_continuous_axis_label_collision_modulus;
};

G_DEFINE_TYPE (CcBarChart, cc_bar_chart, GTK_TYPE_WIDGET)

typedef enum {
  PROP_SELECTED_INDEX = 1,
  PROP_SELECTED_INDEX_SET,
  PROP_OVERLAY_LINE_VALUE,
  PROP_DISCRETE_AXIS_LABELS,
} CcBarChartProperty;

static GParamSpec *props[PROP_DISCRETE_AXIS_LABELS + 1];

typedef enum {
  SIGNAL_DATA_CHANGED,
} CcBarChartSignal;

static guint signals[SIGNAL_DATA_CHANGED + 1];

static void cc_bar_chart_get_property (GObject    *object,
                                       guint       property_id,
                                       GValue     *value,
                                       GParamSpec *pspec);
static void cc_bar_chart_set_property (GObject      *object,
                                       guint         property_id,
                                       const GValue *value,
                                       GParamSpec   *pspec);
static void cc_bar_chart_dispose (GObject *object);
static void cc_bar_chart_finalize (GObject *object);
static void cc_bar_chart_size_allocate (GtkWidget *widget,
                                        int        width,
                                        int        height,
                                        int        baseline);
static void cc_bar_chart_measure (GtkWidget      *widget,
                                  GtkOrientation  orientation,
                                  int             for_size,
                                  int            *minimum,
                                  int            *natural,
                                  int            *minimum_baseline,
                                  int            *natural_baseline);
static void cc_bar_chart_snapshot (GtkWidget   *widget,
                                   GtkSnapshot *snapshot);
static gboolean cc_bar_chart_focus (GtkWidget        *widget,
                                    GtkDirectionType  direction);

static CcBarChartGroup *get_adjacent_focusable_group (CcBarChart      *self,
                                                      CcBarChartGroup *group,
                                                      int              direction);
static CcBarChartGroup *get_first_focusable_group (CcBarChart *self);
static CcBarChartGroup *get_last_focusable_group (CcBarChart *self);
static void ensure_cached_grid_lines_and_labels (CcBarChart *self);
static inline void calculate_axis_area_widths (CcBarChart *self,
                                               int        *out_left_axis_area_width,
                                               int        *out_right_axis_area_width);
static inline void calculate_group_x_bounds (CcBarChart   *self,
                                             unsigned int  idx,
                                             int          *out_spacing_start_x,
                                             int          *out_bar_start_x,
                                             int          *out_bar_finish_x,
                                             int          *out_spacing_finish_x);
static int value_to_widget_y (CcBarChart *self,
                              double      value);
static GtkLabel *create_discrete_axis_label (const char *text);
static GtkLabel *create_continuous_axis_label (const char *text);
static char *format_continuous_axis_label (CcBarChart *self,
                                           double      value);
static double get_maximum_data_value (CcBarChart *self,
                                      gboolean    include_overlay_line);

static void
cc_bar_chart_class_init (CcBarChartClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->get_property = cc_bar_chart_get_property;
  object_class->set_property = cc_bar_chart_set_property;
  object_class->dispose = cc_bar_chart_dispose;
  object_class->finalize = cc_bar_chart_finalize;

  widget_class->size_allocate = cc_bar_chart_size_allocate;
  widget_class->measure = cc_bar_chart_measure;
  widget_class->snapshot = cc_bar_chart_snapshot;
  widget_class->focus = cc_bar_chart_focus;

  /**
   * CcBarChart:selected-index:
   *
   * Index of the currently selected data.
   *
   * If nothing is currently selected, the value of this property is undefined.
   * See #CcBarChart:selected-index-set to check this.
   */
  props[PROP_SELECTED_INDEX] =
    g_param_spec_uint ("selected-index",
                       NULL, NULL,
                       0, G_MAXUINT, 0,
                       G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * CcBarChart:selected-index-set:
   *
   * Whether a data item is currently selected.
   *
   * If this property is `TRUE`, the value of #CcBarChart:selected-index is
   * defined.
   */
  props[PROP_SELECTED_INDEX_SET] =
    g_param_spec_boolean ("selected-index-set",
                          NULL, NULL,
                          FALSE,
                          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * CcBarChart:overlay-line-value:
   *
   * Value (in the same domain as the chart data) to render an overlay line at,
   * or `NAN` to not render one.
   *
   * An overlay line could represent an average value or target value for the
   * bars, for example.
   */
  props[PROP_OVERLAY_LINE_VALUE] =
    g_param_spec_double ("overlay-line-value",
                         NULL, NULL,
                         -G_MAXDOUBLE, G_MAXDOUBLE, 0.0,
                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * CcBarChart:discrete-axis-labels: (nullable)
   *
   * Labels for the discrete axis of the chart.
   *
   * The number of labels must match the number of bars set in the chart data,
   * one label per bar.
   *
   * This will be `NULL` if no labels have been set yet.
   */
  props[PROP_DISCRETE_AXIS_LABELS] =
    g_param_spec_boxed ("discrete-axis-labels",
                        NULL, NULL,
                        G_TYPE_STRV,
                        G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, G_N_ELEMENTS (props), props);

  /**
   * CcBarChart::data-changed:
   *
   * Emitted when the data in the widget is updated.
   */
  signals[SIGNAL_DATA_CHANGED] =
    g_signal_new ("data-changed",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_FIRST,
                  0, NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 0);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/wellbeing/cc-bar-chart.ui");

  gtk_widget_class_set_css_name (widget_class, "bar-chart");
}

static void
cc_bar_chart_init (CcBarChart *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  /* Some default values */
  self->cached_pixels_per_data = 1.0;
  self->overlay_line_value = NAN;
}

static void
cc_bar_chart_get_property (GObject    *object,
                           guint       property_id,
                           GValue     *value,
                           GParamSpec *pspec)
{
  CcBarChart *self = CC_BAR_CHART (object);

  switch ((CcBarChartProperty) property_id)
    {
    case PROP_SELECTED_INDEX: {
      size_t idx;
      gboolean valid = cc_bar_chart_get_selected_index (self, &idx);
      g_value_set_uint (value, valid ? idx : 0);
      break;
    }
    case PROP_SELECTED_INDEX_SET:
      g_value_set_boolean (value, cc_bar_chart_get_selected_index (self, NULL));
      break;
    case PROP_OVERLAY_LINE_VALUE:
      g_value_set_double (value, cc_bar_chart_get_overlay_line_value (self));
      break;
    case PROP_DISCRETE_AXIS_LABELS:
      g_value_set_boxed (value, cc_bar_chart_get_discrete_axis_labels (self, NULL));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
cc_bar_chart_set_property (GObject      *object,
                           guint         property_id,
                           const GValue *value,
                           GParamSpec   *pspec)
{
  CcBarChart *self = CC_BAR_CHART (object);

  switch ((CcBarChartProperty) property_id)
    {
    case PROP_SELECTED_INDEX:
      cc_bar_chart_set_selected_index (self, TRUE, g_value_get_uint (value));
      break;
    case PROP_SELECTED_INDEX_SET:
      /* Read only */
      g_assert_not_reached ();
      break;
    case PROP_OVERLAY_LINE_VALUE:
      cc_bar_chart_set_overlay_line_value (self, g_value_get_double (value));
      break;
    case PROP_DISCRETE_AXIS_LABELS:
      cc_bar_chart_set_discrete_axis_labels (self, g_value_get_boxed (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
cc_bar_chart_dispose (GObject *object)
{
  CcBarChart *self = CC_BAR_CHART (object);

  gtk_widget_dispose_template (GTK_WIDGET (object), CC_TYPE_BAR_CHART);

  g_clear_pointer (&self->cached_discrete_axis_labels, g_ptr_array_unref);
  g_clear_pointer (&self->cached_continuous_axis_labels, g_ptr_array_unref);
  g_clear_pointer (&self->cached_continuous_axis_grid_line_values, g_array_unref);
  g_clear_pointer (&self->cached_groups, g_ptr_array_unref);

  G_OBJECT_CLASS (cc_bar_chart_parent_class)->dispose (object);
}

static void
cc_bar_chart_finalize (GObject *object)
{
  CcBarChart *self = CC_BAR_CHART (object);

  g_strfreev (self->discrete_axis_labels);

  if (self->continuous_axis_label_destroy_notify != NULL)
    self->continuous_axis_label_destroy_notify (self->continuous_axis_label_user_data);

  if (self->continuous_axis_grid_line_destroy_notify != NULL)
    self->continuous_axis_grid_line_destroy_notify (self->continuous_axis_grid_line_user_data);

  g_free (self->data);

  G_OBJECT_CLASS (cc_bar_chart_parent_class)->finalize (object);
}

/* Various constants defining the widget appearance. These can’t be moved into
 * CSS yet as GTK doesn’t expose enough of the CSS parsing machinery. */
static const unsigned int GRID_LINE_WIDTH = 1;
static const GdkRGBA GRID_LINE_COLOR = { .red = 0.957, .green = 0.961, .blue = 0.969, .alpha = 1.0 };
static const unsigned int MINIMUM_CHART_HEIGHT = 120;
static const unsigned int NATURAL_CHART_HEIGHT = 300;
static const unsigned int OVERLAY_LINE_WIDTH = 2;
static const float OVERLAY_LINE_DASH[] = { 6, 5 };
static const GdkRGBA OVERLAY_LINE_COLOR = { .red = 0.110, .green = 0.443, .blue = 0.847, .alpha = 1.0 };
static const double GROUP_TO_SPACE_WIDTH_FILL_RATIO = 0.8;  /* proportion of additional width which gets allocated to bar groups, rather than the space between them */

static void
cc_bar_chart_size_allocate (GtkWidget *widget,
                            int        width,
                            int        height,
                            int        baseline)
{
  CcBarChart *self = CC_BAR_CHART (widget);
  double latest_grid_line_value;
  gboolean collision_detected = FALSE;

  /* Empty state. */
  if (self->n_data == 0)
    return;

  const double max_value = get_maximum_data_value (self, TRUE);

  /* Position the labels for the discrete axis in the correct places. */
  for (unsigned int i = 0; self->n_data > 0 && self->cached_discrete_axis_labels != NULL && i < self->cached_discrete_axis_labels->len; i++)
    {
      GtkAllocation child_alloc;
      int spacing_start_x, spacing_finish_x;

      /* The label is allocated the full possible space, and its xalign and
       * yalign are used to position the text correctly within that. */
      calculate_group_x_bounds (self, i,
                                &spacing_start_x,
                                NULL, NULL,
                                &spacing_finish_x);

      child_alloc.x = spacing_start_x;
      child_alloc.y = height - self->cached_discrete_axis_area_height;
      child_alloc.width = spacing_finish_x - spacing_start_x;
      child_alloc.height = self->cached_discrete_axis_area_height;

      gtk_widget_size_allocate (self->cached_discrete_axis_labels->pdata[i], &child_alloc, self->cached_discrete_axis_baseline);
    }

  /* Calculate our continuous axis grid lines and labels */
  ensure_cached_grid_lines_and_labels (self);

  if (self->cached_continuous_axis_grid_line_values != NULL)
    latest_grid_line_value = g_array_index (self->cached_continuous_axis_grid_line_values, double, self->cached_continuous_axis_grid_line_values->len - 1);
  else
    latest_grid_line_value = max_value;

  /* Calculate the scale of data on the chart, given the available space to the
   * topmost continuous axis grid line (factoring in half the height of the
   * label extending beyond that). Space beyond our natural request is allocated
   * to the chart area rather than the axis areas. */
  g_assert (height > self->cached_discrete_axis_area_height);
  self->cached_pixels_per_data = (height - self->cached_discrete_axis_area_height - self->cached_continuous_axis_label_height / 2) / (latest_grid_line_value + 1.0);
  g_assert (self->cached_pixels_per_data > 0.0);

  /* Position the continuous axis labels in the correct places. In a subsequent
   * step we work out collisions and hide labels based on index modulus to avoid
   * drawing text on top of other text. See below. */
  if (self->cached_continuous_axis_labels != NULL &&
      self->cached_continuous_axis_grid_line_values != NULL)
    {
      for (unsigned int i = 0; i < self->cached_continuous_axis_labels->len; i++)
        {
          const double grid_line_value = g_array_index (self->cached_continuous_axis_grid_line_values, double, i);
          GtkAllocation child_alloc;
          int label_natural_height, label_natural_baseline;

          gtk_widget_measure (GTK_WIDGET (self->cached_continuous_axis_labels->pdata[i]),
                              GTK_ORIENTATION_VERTICAL, self->cached_continuous_axis_area_width,
                              NULL, &label_natural_height,
                              NULL, &label_natural_baseline);

          child_alloc.x = width - self->cached_continuous_axis_area_width;
          /* centre the label vertically on the grid line position, and let the valign code
           * in GtkLabel align the text baseline correctly with that */
          child_alloc.y = value_to_widget_y (self, grid_line_value) - label_natural_height / 2;
          child_alloc.width = self->cached_continuous_axis_area_width;
          child_alloc.height = label_natural_height;

          gtk_widget_size_allocate (self->cached_continuous_axis_labels->pdata[i], &child_alloc, label_natural_baseline);
        }

      /* Check for collisions. Compare pairs of continuous axis labels which are
       * self->cached_continuous_axis_label_collision_modulus positions apart. If
       * any of those pairs collide, increment the collision modulus and restart the
       * check. */
      self->cached_continuous_axis_label_collision_modulus = 1;

      do
        {
          collision_detected = FALSE;

          for (unsigned int i = self->cached_continuous_axis_label_collision_modulus;
               i < self->cached_continuous_axis_labels->len;
               i++)
            {
              graphene_rect_t child_allocs[2];
              gboolean success;

              success = gtk_widget_compute_bounds (self->cached_continuous_axis_labels->pdata[i - self->cached_continuous_axis_label_collision_modulus],
                                                   widget,
                                                   &child_allocs[0]);
              g_assert (success);
              success = gtk_widget_compute_bounds (self->cached_continuous_axis_labels->pdata[i],
                                                   widget,
                                                   &child_allocs[1]);
              g_assert (success);

              if (graphene_rect_intersection (&child_allocs[0], &child_allocs[1], NULL))
                {
                  collision_detected = TRUE;
                  g_assert (self->cached_continuous_axis_label_collision_modulus < G_MAXUINT);
                  self->cached_continuous_axis_label_collision_modulus++;
                  break;
                }
            }
        }
      while (collision_detected);

      /* Hide continuous axis labels according to the collision modulus. */
      for (unsigned int i = 0; i < self->cached_continuous_axis_labels->len; i++)
        {
          gtk_widget_set_visible (self->cached_continuous_axis_labels->pdata[i],
                                  (i % self->cached_continuous_axis_label_collision_modulus) == 0);
        }
    }

  /* Chart bar groups */
  for (unsigned int i = 0; self->cached_groups != NULL && i < self->cached_groups->len; i++)
    {
      CcBarChartGroup *group = CC_BAR_CHART_GROUP (self->cached_groups->pdata[i]);
      int group_bottom_y, group_left_x, group_right_x;
      GtkAllocation child_alloc;

      calculate_group_x_bounds (self, i,
                                NULL,
                                &group_left_x,
                                &group_right_x,
                                NULL);

      /* Position the bottom of the bar just above the axis grid line, to avoid
       * them overlapping. */
      group_bottom_y = value_to_widget_y (self, 0.0) - (GRID_LINE_WIDTH + (2 - 1)) / 2;

      child_alloc.x = group_left_x;
      child_alloc.y = 0;
      child_alloc.width = group_right_x - group_left_x;
      child_alloc.height = group_bottom_y;

      cc_bar_chart_group_set_scale (group, self->cached_pixels_per_data);
      gtk_widget_size_allocate (GTK_WIDGET (group), &child_alloc, -1);
    }
}

static void
cc_bar_chart_measure (GtkWidget      *widget,
                      GtkOrientation  orientation,
                      int             for_size,
                      int            *minimum,
                      int            *natural,
                      int            *minimum_baseline,
                      int            *natural_baseline)
{
  CcBarChart *self = CC_BAR_CHART (widget);

  /* Empty state. */
  if (self->n_data == 0)
    {
      *minimum = 0;
      *natural = 0;
      *minimum_baseline = -1;
      *natural_baseline = -1;
      return;
    }

  /* Calculate our continuous axis labels for measuring. Even though some of
   * them won’t be visible (according to
   * self->cached_continuous_axis_label_collision_modulus), measure them all
   * so that the width of the chart doesn’t jump about as the modulus changes. */
  if (self->continuous_axis_label_callback != NULL)
    ensure_cached_grid_lines_and_labels (self);

  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      int maximum_continuous_label_natural_width = 0;
      int maximum_discrete_label_minimum_width = 0, maximum_discrete_label_natural_width = 0;
      int maximum_bar_minimum_width = 0, maximum_bar_natural_width = 0;

      /* Measure the first and last continuous axis labels and work out their
       * minimum widths. */
      for (unsigned int i = 0; self->cached_continuous_axis_labels != NULL && i < self->cached_continuous_axis_labels->len; i++)
        {
          int label_natural_width = -1;

          gtk_widget_measure (GTK_WIDGET (self->cached_continuous_axis_labels->pdata[i]),
                              GTK_ORIENTATION_HORIZONTAL, -1,
                              NULL, &label_natural_width,
                              NULL, NULL);

          maximum_continuous_label_natural_width = MAX (maximum_continuous_label_natural_width, label_natural_width);
        }

      /* Measure the bar groups to get their minimum and natural widths too. */
      for (unsigned int i = 0; i < self->cached_groups->len; i++)
        {
          int bar_natural_width = -1, bar_minimum_width = -1;

          gtk_widget_measure (GTK_WIDGET (self->cached_groups->pdata[i]),
                              GTK_ORIENTATION_HORIZONTAL, -1,
                              &bar_minimum_width, &bar_natural_width,
                              NULL, NULL);

          maximum_bar_natural_width = MAX (maximum_bar_natural_width, bar_natural_width);
          maximum_bar_minimum_width = MAX (maximum_bar_minimum_width, bar_minimum_width);
        }

      /* Also measure the discrete axis labels to see if any of them are wider
       * than the groups. */
      for (unsigned int i = 0; self->cached_discrete_axis_labels != NULL && i < self->cached_discrete_axis_labels->len; i++)
        {
          int label_natural_width = -1, label_minimum_width = -1;

          gtk_widget_measure (GTK_WIDGET (self->cached_discrete_axis_labels->pdata[i]),
                              GTK_ORIENTATION_HORIZONTAL, -1,
                              &label_minimum_width, &label_natural_width,
                              NULL, NULL);

          maximum_discrete_label_natural_width = MAX (maximum_discrete_label_natural_width, label_natural_width);
          maximum_discrete_label_minimum_width = MAX (maximum_discrete_label_minimum_width, label_minimum_width);
        }

      self->cached_minimum_group_width = MAX (maximum_bar_minimum_width, maximum_discrete_label_minimum_width);

      /* Don’t complicate things by allowing the continuous axis labels to get
       * less than their natural width as an allocation. */
      self->cached_continuous_axis_area_width = maximum_continuous_label_natural_width;

      *minimum = MAX (maximum_bar_minimum_width, maximum_discrete_label_minimum_width) * self->n_data + maximum_continuous_label_natural_width;
      *natural = MAX (maximum_bar_natural_width, maximum_discrete_label_natural_width) * self->n_data + maximum_continuous_label_natural_width;
      *minimum_baseline = -1;
      *natural_baseline = -1;
    }
  else if (orientation == GTK_ORIENTATION_VERTICAL)
    {
      int maximum_discrete_label_natural_height = 0, maximum_discrete_label_natural_baseline = -1;
      int continuous_label_minimum_height = 0, continuous_label_natural_height = 0;

      /* Measure all the discrete labels. */
      for (unsigned int i = 0; self->cached_discrete_axis_labels != NULL && i < self->cached_discrete_axis_labels->len; i++)
        {
          int label_natural_height = -1, label_natural_baseline = -1;

          gtk_widget_measure (GTK_WIDGET (self->cached_discrete_axis_labels->pdata[i]),
                              GTK_ORIENTATION_VERTICAL, -1,
                              NULL, &label_natural_height,
                              NULL, &label_natural_baseline);

          maximum_discrete_label_natural_height = MAX (maximum_discrete_label_natural_height, label_natural_height);
          maximum_discrete_label_natural_baseline = MAX (maximum_discrete_label_natural_baseline, label_natural_baseline);
        }

      /* Measure the last continuous axis label and work out its minimum height,
       * because it will be vertically centred on the top-most grid line, which
       * might be near enough the top of the chart to push the top of the text
       * off the default allocation.
       *
       * There’s no need to do the same for the first continuous axis label,
       * because it will never drop below the discrete axis labels unless the
       * font sizes are ludicrously different. */
      if (self->cached_continuous_axis_labels != NULL)
        {
          gtk_widget_measure (GTK_WIDGET (self->cached_continuous_axis_labels->pdata[self->cached_continuous_axis_labels->len - 1]),
                              GTK_ORIENTATION_VERTICAL, -1,
                              &continuous_label_minimum_height, &continuous_label_natural_height,
                              NULL, NULL);
        }

      self->cached_continuous_axis_label_height = continuous_label_natural_height;

      /* Don’t complicate things by allowing the discrete axis labels to get
       * less than their natural height as an allocation. */
      self->cached_discrete_axis_area_height = maximum_discrete_label_natural_height;
      self->cached_discrete_axis_baseline = maximum_discrete_label_natural_baseline;

      *minimum = MINIMUM_CHART_HEIGHT + maximum_discrete_label_natural_height + continuous_label_minimum_height / 2;
      *natural = NATURAL_CHART_HEIGHT + maximum_discrete_label_natural_height + continuous_label_natural_height / 2;
      *minimum_baseline = -1;
      *natural_baseline = -1;
    }
  else
    {
      g_assert_not_reached ();
    }
}

static void
cc_bar_chart_snapshot (GtkWidget   *widget,
                       GtkSnapshot *snapshot)
{
  CcBarChart *self = CC_BAR_CHART (widget);
  const int width = gtk_widget_get_width (widget);
  int left_axis_area_width, right_axis_area_width;

  /* Empty state. */
  if (self->n_data == 0)
    return;

  calculate_axis_area_widths (self, &left_axis_area_width, &right_axis_area_width);

  /* Continuous axis grid lines, should have been cached in size_allocate, but
   * may not have been set yet. */
  if (self->cached_continuous_axis_grid_line_values != NULL)
    {
      g_autoptr(GskPathBuilder) grid_line_builder = gsk_path_builder_new ();
      g_autoptr(GskPath) grid_line_path = NULL;
      GskStroke *grid_line_stroke = NULL;

      grid_line_stroke = gsk_stroke_new (GRID_LINE_WIDTH);

      for (unsigned int i = 0; i < self->cached_continuous_axis_grid_line_values->len; i++)
        {
          const double value = g_array_index (self->cached_continuous_axis_grid_line_values, double, i);
          int y = value_to_widget_y (self, value);

          gsk_path_builder_move_to (grid_line_builder, left_axis_area_width, y);
          gsk_path_builder_line_to (grid_line_builder,
                                    width - right_axis_area_width,
                                    y);
        }

      grid_line_path = gsk_path_builder_free_to_path (g_steal_pointer (&grid_line_builder));
      gtk_snapshot_append_stroke (snapshot, grid_line_path, grid_line_stroke, &GRID_LINE_COLOR);

      gsk_stroke_free (g_steal_pointer (&grid_line_stroke));
    }

  /* Continuous axis labels, should have been cached in size_allocate, but may
   * not have been set yet. */
  for (unsigned int i = 0; self->cached_continuous_axis_labels != NULL && i < self->cached_continuous_axis_labels->len; i++)
    gtk_widget_snapshot_child (widget, self->cached_continuous_axis_labels->pdata[i], snapshot);

  /* Discrete axis labels, should have been cached in size_allocate, but may not
   * have been set yet */
  for (unsigned int i = 0; self->cached_discrete_axis_labels != NULL && i < self->cached_discrete_axis_labels->len; i++)
    gtk_widget_snapshot_child (widget, self->cached_discrete_axis_labels->pdata[i], snapshot);

  /* Bar groups, should have been cached in size_allocate, but may not have been set yet */
  for (unsigned int i = 0; self->cached_groups != NULL && i < self->cached_groups->len; i++)
    gtk_widget_snapshot_child (widget, self->cached_groups->pdata[i], snapshot);

  /* Overlay line */
  if (!isnan (self->overlay_line_value))
    {
      g_autoptr(GskPathBuilder) overlay_builder = gsk_path_builder_new ();
      g_autoptr(GskPath) overlay_path = NULL;
      GskStroke *overlay_stroke = NULL;
      int overlay_y;

      overlay_stroke = gsk_stroke_new (OVERLAY_LINE_WIDTH);
      gsk_stroke_set_line_cap (overlay_stroke, GSK_LINE_CAP_SQUARE);
      gsk_stroke_set_dash (overlay_stroke, OVERLAY_LINE_DASH, G_N_ELEMENTS (OVERLAY_LINE_DASH));

      overlay_y = value_to_widget_y (self, self->overlay_line_value);
      gsk_path_builder_move_to (overlay_builder, left_axis_area_width, overlay_y);
      gsk_path_builder_line_to (overlay_builder,
                                width - right_axis_area_width,
                                overlay_y);

      overlay_path = gsk_path_builder_free_to_path (g_steal_pointer (&overlay_builder));
      gtk_snapshot_append_stroke (snapshot, overlay_path, overlay_stroke, &OVERLAY_LINE_COLOR);

      gsk_stroke_free (g_steal_pointer (&overlay_stroke));
    }
}

static gboolean
cc_bar_chart_focus (GtkWidget        *widget,
                    GtkDirectionType  direction)
{
  CcBarChart *self = CC_BAR_CHART (widget);
  GtkWidget *focus_child;
  CcBarChartGroup *next_focus_group = NULL;

  focus_child = gtk_widget_get_focus_child (widget);

  if (focus_child != NULL)
    {
      /* Can the focus move around inside the currently focused child widget? */
      if (gtk_widget_child_focus (focus_child, direction))
        return TRUE;

      if (CC_IS_BAR_CHART_GROUP (focus_child) &&
          (direction == GTK_DIR_LEFT || direction == GTK_DIR_TAB_BACKWARD))
        next_focus_group = get_adjacent_focusable_group (self, CC_BAR_CHART_GROUP (focus_child), -1);
      else if (CC_IS_BAR_CHART_GROUP (focus_child) &&
               (direction == GTK_DIR_RIGHT || direction == GTK_DIR_TAB_FORWARD))
        next_focus_group = get_adjacent_focusable_group (self, CC_BAR_CHART_GROUP (focus_child), 1);
    }
  else
    {
      /* No current focus group. If a group is selected, focus on that. Otherwise,
       * focus on the first/last focusable group, depending on which direction
       * we’re coming in from. */
      if (self->selected_index_set)
        next_focus_group = self->cached_groups->pdata[self->selected_index];

      if (next_focus_group == NULL &&
          (direction == GTK_DIR_UP || direction == GTK_DIR_LEFT || direction == GTK_DIR_TAB_BACKWARD))
        next_focus_group = get_last_focusable_group (self);
      else if (next_focus_group == NULL &&
               (direction == GTK_DIR_DOWN || direction == GTK_DIR_RIGHT || direction == GTK_DIR_TAB_FORWARD))
        next_focus_group = get_first_focusable_group (self);
    }

  if (next_focus_group == NULL)
    {
      if (direction == GTK_DIR_LEFT || direction == GTK_DIR_RIGHT)
        {
          if (gtk_widget_keynav_failed (widget, direction))
            return TRUE;
        }

      return FALSE;
    }

  return gtk_widget_child_focus (GTK_WIDGET (next_focus_group), direction);
}

static gboolean
find_index_for_group (CcBarChart      *self,
                      CcBarChartGroup *group,
                      unsigned int    *out_idx)
{
  g_assert (gtk_widget_is_ancestor (GTK_WIDGET (group), GTK_WIDGET (self)));
  g_assert (self->cached_groups != NULL);

  return g_ptr_array_find (self->cached_groups, group, out_idx);
}

static gboolean
group_is_focusable (CcBarChartGroup *group)
{
  GtkWidget *widget = GTK_WIDGET (group);

  return (gtk_widget_is_visible (widget) &&
          gtk_widget_is_sensitive (widget) &&
          gtk_widget_get_focusable (widget) &&
          gtk_widget_get_can_focus (widget));
}

/* direction == -1 means get previous sensitive and visible group;
 * direction == 1 means get next one. */
static CcBarChartGroup *
get_adjacent_focusable_group (CcBarChart      *self,
                              CcBarChartGroup *group,
                              int              direction)
{
  unsigned int group_idx, i;

  g_assert (gtk_widget_is_ancestor (GTK_WIDGET (group), GTK_WIDGET (self)));
  g_assert (self->cached_groups != NULL);
  g_assert (direction == -1 || direction == 1);

  if (!find_index_for_group (self, group, &group_idx))
    return NULL;

  i = group_idx;

  while (!((direction == -1 && i == 0) ||
           (direction == 1 && i >= self->cached_groups->len - 1)))
    {
      CcBarChartGroup *adjacent_group;

      i += direction;
      adjacent_group = self->cached_groups->pdata[i];

      if (group_is_focusable (adjacent_group))
        return adjacent_group;
    }

  return NULL;
}

static CcBarChartGroup *
get_first_focusable_group (CcBarChart *self)
{
  g_assert (self->cached_groups != NULL);

  for (unsigned int i = 0; i < self->cached_groups->len; i++)
    {
      CcBarChartGroup *group = self->cached_groups->pdata[i];

      if (group_is_focusable (group))
        return group;
    }

  return NULL;
}

static CcBarChartGroup *
get_last_focusable_group (CcBarChart *self)
{
  g_assert (self->cached_groups != NULL);

  for (unsigned int i = 0; i < self->cached_groups->len; i++)
    {
      CcBarChartGroup *group = self->cached_groups->pdata[self->cached_groups->len - 1 - i];

      if (group_is_focusable (group))
        return group;
    }

  return NULL;
}

static void
ensure_cached_grid_lines_and_labels (CcBarChart *self)
{
  const double max_value = get_maximum_data_value (self, TRUE);
  double latest_grid_line_value;

  /* Calculate our continuous axis grid lines. Use the user’s provided callback
   * to lay them out, until we’ve got enough to cover the maximum data value.
   * We always need at least two grid lines to define the top and bottom of the
   * plot. */
  if (self->cached_continuous_axis_grid_line_values == NULL &&
      self->continuous_axis_grid_line_callback != NULL)
    {
      self->cached_continuous_axis_grid_line_values = g_array_new (FALSE, FALSE, sizeof (double));

      do
        {
          latest_grid_line_value = self->continuous_axis_grid_line_callback (self,
                                                                             self->cached_continuous_axis_grid_line_values->len,
                                                                             self->continuous_axis_grid_line_user_data);
          g_assert (latest_grid_line_value >= 0.0);
          g_array_append_val (self->cached_continuous_axis_grid_line_values, latest_grid_line_value);
        }
      while (latest_grid_line_value <= max_value ||
             self->cached_continuous_axis_grid_line_values->len < 2);
    }

  /* Create one continuous axis label for each grid line. In a subsequent step
   * in cc_bar_chart_size_allocate() we position them all and we work out
   * collisions and hide labels based on index modulus to avoid drawing text on
   * top of other text. See cc_bar_chart_size_allocate(). */
  if (self->cached_continuous_axis_labels == NULL &&
      self->continuous_axis_label_callback != NULL &&
      self->cached_continuous_axis_grid_line_values != NULL)
    {
      self->cached_continuous_axis_labels = g_ptr_array_new_with_free_func ((GDestroyNotify) gtk_widget_unparent);

      for (unsigned int i = 0; i < self->cached_continuous_axis_grid_line_values->len; i++)
        {
          const double grid_line_value = g_array_index (self->cached_continuous_axis_grid_line_values, double, i);
          g_autofree char *label_text = format_continuous_axis_label (self, grid_line_value);
          g_autoptr(GtkLabel) label = GTK_LABEL (g_object_ref_sink (create_continuous_axis_label (label_text)));
          gtk_widget_set_parent (GTK_WIDGET (label), GTK_WIDGET (self));
          /* don’t insert the label into the widget child order using
           * gtk_widget_insert_after(), as it shouldn’t be focusable */
          g_ptr_array_add (self->cached_continuous_axis_labels, g_steal_pointer (&label));
        }
    }
}

static inline void
calculate_axis_area_widths (CcBarChart *self,
                            int        *out_left_axis_area_width,
                            int        *out_right_axis_area_width)
{
  int left_axis_area_width, right_axis_area_width;

  left_axis_area_width = 0;
  right_axis_area_width = self->cached_continuous_axis_area_width;

  if (out_left_axis_area_width != NULL)
    *out_left_axis_area_width = left_axis_area_width;
  if (out_right_axis_area_width != NULL)
    *out_right_axis_area_width = right_axis_area_width;
}

/* Calculate the x-coordinate bounds of a bar group and its spacing. This is
 * done individually for each group, rather than caching a single width for
 * groups and multiplying it, so that rounding errors don’t accumulate across
 * the width of the plot area. */
static inline void
calculate_group_x_bounds (CcBarChart   *self,
                          unsigned int  idx,
                          int          *out_spacing_start_x,
                          int          *out_group_start_x,
                          int          *out_group_finish_x,
                          int          *out_spacing_finish_x)
{
  int widget_width, plot_width, extra_plot_width;
  int left_axis_area_width, right_axis_area_width;
  int group_width, group_spacing;
  int spacing_start_x, group_start_x, group_finish_x, spacing_finish_x;

  g_assert (self->n_data > 0);

  calculate_axis_area_widths (self, &left_axis_area_width, &right_axis_area_width);

  widget_width = gtk_widget_get_width (GTK_WIDGET (self));
  plot_width = widget_width - left_axis_area_width - right_axis_area_width;
  extra_plot_width = plot_width - self->cached_minimum_group_width * self->n_data;

  group_width = self->cached_minimum_group_width + (extra_plot_width / self->n_data) * GROUP_TO_SPACE_WIDTH_FILL_RATIO;
  group_spacing = (extra_plot_width / self->n_data) * (1.0 - GROUP_TO_SPACE_WIDTH_FILL_RATIO);

  spacing_start_x = left_axis_area_width + plot_width * idx / self->n_data;
  group_start_x = spacing_start_x + group_spacing / 2;
  group_finish_x = group_start_x + group_width;
  spacing_finish_x = left_axis_area_width + plot_width * (idx + 1) / self->n_data;

  g_assert (spacing_start_x <= group_start_x &&
            group_start_x <= group_finish_x &&
            group_finish_x <= spacing_finish_x);

  if (out_spacing_start_x != NULL)
    *out_spacing_start_x = spacing_start_x;
  if (out_group_start_x != NULL)
    *out_group_start_x = group_start_x;
  if (out_group_finish_x != NULL)
    *out_group_finish_x = group_finish_x;
  if (out_spacing_finish_x != NULL)
    *out_spacing_finish_x = spacing_finish_x;
}

/* Convert a value from the domain of self->data to widget coordinates. */
static int
value_to_widget_y (CcBarChart *self,
                   double      value)
{
  int height = gtk_widget_get_height (GTK_WIDGET (self));

  /* Negative values are not currently supported. */
  g_assert (value >= 0.0);

  /* The widget should be sized to accommodate all values in the data. */
  g_assert (self->cached_pixels_per_data * value <= height - self->cached_discrete_axis_area_height);

  return height - self->cached_discrete_axis_area_height - self->cached_pixels_per_data * value;
}

/* returns floating reference */
static GtkLabel *
create_discrete_axis_label (const char *text)
{
  GtkLabel *label = GTK_LABEL (gtk_label_new (text));
  gtk_label_set_xalign (label, 0.5);
  gtk_widget_add_css_class (GTK_WIDGET (label), "discrete-axis-label");

  return g_steal_pointer (&label);
}

/* returns floating reference */
static GtkLabel *
create_continuous_axis_label (const char *text)
{
  GtkLabel *label = GTK_LABEL (gtk_label_new (text));
  gtk_label_set_xalign (label, 0.0);
  gtk_label_set_yalign (label, 0.0);
  gtk_widget_set_valign (GTK_WIDGET (label), GTK_ALIGN_BASELINE_CENTER);
  gtk_widget_add_css_class (GTK_WIDGET (label), "continuous-axis-label");

  return g_steal_pointer (&label);
}

static char *
format_continuous_axis_label (CcBarChart *self,
                              double      value)
{
  g_autofree char *out = NULL;

  g_assert (self->continuous_axis_label_callback != NULL);

  out = self->continuous_axis_label_callback (self, value, self->continuous_axis_label_user_data);
  g_assert (out != NULL);

  return g_steal_pointer (&out);
}

static double
get_maximum_data_value (CcBarChart *self,
                        gboolean    include_overlay_line)
{
  double value = 0.0;

  g_assert (self->data != NULL);

  for (size_t i = 0; i < self->n_data; i++)
    value = MAX (value, self->data[i]);

  if (include_overlay_line && !isnan (self->overlay_line_value))
    value = MAX (value, self->overlay_line_value);

  return value;
}

/**
 * cc_bar_chart_new:
 *
 * Create a new #CcBarChart.
 *
 * Returns: (transfer full): the new #CcBarChart
 */
CcBarChart *
cc_bar_chart_new (void)
{
  return g_object_new (CC_TYPE_BAR_CHART, NULL);
}

/**
 * cc_bar_chart_get_discrete_axis_labels:
 * @self: a #CcBarChart
 * @out_n_discrete_axis_labels: (out) (optional): return location for the number
 *   of labels, or `NULL` to ignore
 *
 * Get the discrete axis labels for the chart.
 *
 * This will be `NULL` if no labels have been set yet, in which case `0` will be
 * returned in @out_n_discrete_axis_labels.
 *
 * See #CcBarChart:discrete-axis-labels.
 *
 * Returns: (nullable) (array zero-terminated=1 length=out_n_discrete_axis_labels) (transfer none): array
 *   of discrete axis labels
 */
const char * const *
cc_bar_chart_get_discrete_axis_labels (CcBarChart *self,
                                       size_t     *out_n_discrete_axis_labels)
{
  g_return_val_if_fail (CC_IS_BAR_CHART (self), NULL);

  if (out_n_discrete_axis_labels != NULL)
    *out_n_discrete_axis_labels = self->n_discrete_axis_labels;

  return (const char * const *) self->discrete_axis_labels;
}

/**
 * cc_bar_chart_set_discrete_axis_labels:
 * @self: a #CcBarChart
 * @labels: (array zero-terminated=1) (nullable) (transfer none): new set of
 *   discrete axis labels, or `NULL` to unset
 *
 * Set the discrete axis labels for the chart.
 *
 * This can be `NULL` if the labels are currently unknown.
 *
 * See #CcBarChart:discrete-axis-labels.
 */
void
cc_bar_chart_set_discrete_axis_labels (CcBarChart         *self,
                                       const char * const *labels)
{
  g_return_if_fail (CC_IS_BAR_CHART (self));

  if ((self->discrete_axis_labels == NULL && labels == NULL) ||
      (self->discrete_axis_labels != NULL && labels != NULL &&
       g_strv_equal ((const char * const *) self->discrete_axis_labels, labels)))
    return;

  g_strfreev (self->discrete_axis_labels);
  self->discrete_axis_labels = g_strdupv ((char **) labels);
  self->n_discrete_axis_labels = (labels != NULL) ? g_strv_length ((char **) labels) : 0;

  /* Rebuild the cache */
  g_clear_pointer (&self->cached_discrete_axis_labels, g_ptr_array_unref);
  if (self->n_discrete_axis_labels > 0)
    self->cached_discrete_axis_labels = g_ptr_array_new_with_free_func ((GDestroyNotify) gtk_widget_unparent);

  for (size_t i = 0; i < self->n_discrete_axis_labels; i++)
    {
      g_autoptr(GtkLabel) label = GTK_LABEL (g_object_ref_sink (create_discrete_axis_label (self->discrete_axis_labels[i])));
      gtk_widget_set_parent (GTK_WIDGET (label), GTK_WIDGET (self));
      /* don’t insert the label into the widget child order using
       * gtk_widget_insert_after(), as it shouldn’t be focusable */
      g_ptr_array_add (self->cached_discrete_axis_labels, g_steal_pointer (&label));
    }

  /* Re-render */
  gtk_widget_queue_resize (GTK_WIDGET (self));

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_DISCRETE_AXIS_LABELS]);
}

/**
 * cc_bar_chart_set_continuous_axis_label_callback:
 * @self: a #CcBarChart
 * @callback: (nullable): callback to generate continuous axis labels, or
 *   `NULL` to disable them
 * @user_data: (nullable) (closure callback): user data for @callback
 * @destroy_notify: (nullable) (destroy callback): destroy function for @user_data
 *
 * Set the callback to generate labels for the continuous axis.
 *
 * This is called multiple times when sizing and rendering the chart, to
 * generate the labels for the continuous axis. Grid lines and marks are
 * generated, then some of them are converted to textual labels using @callback.
 */
void
cc_bar_chart_set_continuous_axis_label_callback (CcBarChart              *self,
                                                 CcBarChartLabelCallback  callback,
                                                 void                    *user_data,
                                                 GDestroyNotify           destroy_notify)
{
  g_return_if_fail (CC_IS_BAR_CHART (self));

  if (self->continuous_axis_label_callback == callback &&
      self->continuous_axis_label_user_data == user_data &&
      self->continuous_axis_label_destroy_notify == destroy_notify)
    return;

  if (self->continuous_axis_label_destroy_notify != NULL)
    self->continuous_axis_label_destroy_notify (self->continuous_axis_label_user_data);

  self->continuous_axis_label_callback = callback;
  self->continuous_axis_label_user_data = user_data;
  self->continuous_axis_label_destroy_notify = destroy_notify;

  /* Clear the old cache */
  g_clear_pointer (&self->cached_continuous_axis_labels, g_ptr_array_unref);

  /* Re-render */
  gtk_widget_queue_resize (GTK_WIDGET (self));
}

/**
 * cc_bar_chart_set_continuous_axis_grid_line_callback:
 * @self: a #CcBarChart
 * @callback: (nullable): callback to generate continuous axis grid lines, or
 *   `NULL` to disable them
 * @user_data: (nullable) (closure callback): user data for @callback
 * @destroy_notify: (nullable) (destroy callback): destroy function for @user_data
 *
 * Set the callback to generate grid lines for the continuous axis.
 *
 * This is called multiple times when sizing and rendering the chart, to
 * generate the grid lines for the continuous axis. See the documentation for
 * #CcBarChartGridLineCallback for further details.
 */
void
cc_bar_chart_set_continuous_axis_grid_line_callback (CcBarChart                 *self,
                                                     CcBarChartGridLineCallback  callback,
                                                     void                       *user_data,
                                                     GDestroyNotify              destroy_notify)
{
  g_return_if_fail (CC_IS_BAR_CHART (self));

  if (self->continuous_axis_grid_line_callback == callback &&
      self->continuous_axis_grid_line_user_data == user_data &&
      self->continuous_axis_grid_line_destroy_notify == destroy_notify)
    return;

  if (self->continuous_axis_grid_line_destroy_notify != NULL)
    self->continuous_axis_grid_line_destroy_notify (self->continuous_axis_grid_line_user_data);

  self->continuous_axis_grid_line_callback = callback;
  self->continuous_axis_grid_line_user_data = user_data;
  self->continuous_axis_grid_line_destroy_notify = destroy_notify;

  /* Clear the old cache, including the labels */
  g_clear_pointer (&self->cached_continuous_axis_grid_line_values, g_array_unref);
  g_clear_pointer (&self->cached_continuous_axis_labels, g_ptr_array_unref);

  /* Re-render */
  gtk_widget_queue_resize (GTK_WIDGET (self));
}

/**
 * cc_bar_chart_get_data:
 * @self: a #CcBarChart
 * @out_n_data: (out) (not optional): return location for the number of data
 *
 * Get the data for the bar chart.
 *
 * If no data is currently set, `NULL` will be returned and @out_n_data will be
 * set to `0`.
 *
 * Returns: (array length=out_n_data) (nullable) (transfer none): data for the
 *   chart, or `NULL` if it’s not currently set
 */
const double *
cc_bar_chart_get_data (CcBarChart *self,
                       size_t     *out_n_data)
{
  g_return_val_if_fail (CC_IS_BAR_CHART (self), NULL);
  g_return_val_if_fail (out_n_data != NULL, NULL);

  *out_n_data = self->n_data;

  /* Normalise to `NULL` */
  return (self->n_data != 0) ? self->data : NULL;
}

/**
 * cc_bar_chart_set_data:
 * @self: a #CcBarChart
 * @data: (array length=n_data) (nullable) (transfer none): data for the bar
 *   chart, or `NULL` to unset
 * @n_data: number of data
 *
 * Set the data for the bar chart.
 *
 * To clear the data for the chart, pass `NULL` for @data and `0` for @n_data.
 */
void
cc_bar_chart_set_data (CcBarChart   *self,
                       const double *data,
                       size_t        n_data)
{
  g_return_if_fail (CC_IS_BAR_CHART (self));
  g_return_if_fail (n_data == 0 || data != NULL);
  g_return_if_fail (n_data <= G_MAXSIZE / sizeof (*data));

  /* Normalise input. */
  if (n_data == 0)
    data = NULL;

  if ((self->data == NULL && data == NULL) ||
      (self->data != NULL && data != NULL &&
       memcmp (self->data, data, n_data * sizeof (*data)) == 0))
    return;

  g_clear_pointer (&self->data, g_free);
  self->data = g_memdup2 (data, n_data * sizeof (*data));
  self->n_data = n_data;

  /* Clear the cached bars, and also the grid lines and labels which are calculated based on the data. */
  g_clear_pointer (&self->cached_groups, g_ptr_array_unref);
  g_clear_pointer (&self->cached_continuous_axis_grid_line_values, g_array_unref);
  g_clear_pointer (&self->cached_continuous_axis_labels, g_ptr_array_unref);

  /* Also clear the selection index. */
  self->selected_index_set = FALSE;
  self->selected_index = 0;

  /* Rebuild the cache. Currently we support exactly at most one bar per group.
   * There will be zero bars in groups where the data is NAN (i.e. not provided). */
  if (self->n_data > 0)
    self->cached_groups = g_ptr_array_new_with_free_func ((GDestroyNotify) gtk_widget_unparent);

  for (size_t i = 0; i < self->n_data; i++)
    {
      g_autoptr(CcBarChartGroup) group = NULL;
      CcBarChartGroup *previous_group = (i > 0) ? self->cached_groups->pdata[i - 1] : NULL;

      group = CC_BAR_CHART_GROUP (g_object_ref_sink (cc_bar_chart_group_new ()));
      cc_bar_chart_group_set_scale (group, self->cached_pixels_per_data);
      if (!isnan (self->data[i]))
        cc_bar_chart_group_insert_bar (group, -1, cc_bar_chart_bar_new (self->data[i]));
      gtk_widget_set_parent (GTK_WIDGET (group), GTK_WIDGET (self));
      gtk_widget_insert_after (GTK_WIDGET (group), GTK_WIDGET (self), GTK_WIDGET (previous_group));
      g_ptr_array_add (self->cached_groups, g_steal_pointer (&group));
    }

  /* Re-render */
  gtk_widget_queue_resize (GTK_WIDGET (self));

  g_signal_emit (self, signals[SIGNAL_DATA_CHANGED], 0);
}

/**
 * cc_bar_chart_get_overlay_line_value:
 * @self: a #CcBarChart
 *
 * Get the value of #CcBarChart:overlay-line-value.
 *
 * Returns: value to render an overlay line at, or `NAN` if unset
 */
double
cc_bar_chart_get_overlay_line_value (CcBarChart *self)
{
  g_return_val_if_fail (CC_IS_BAR_CHART (self), NAN);

  return self->overlay_line_value;
}

/**
 * cc_bar_chart_set_overlay_line_value:
 * @self: a #CcBarChart
 * @value: value to render an overlay line at, or `NAN` to not render one
 *
 * Set the value of #CcBarChart:overlay-line-value.
 */
void
cc_bar_chart_set_overlay_line_value (CcBarChart *self,
                                     double      value)
{
  g_return_if_fail (CC_IS_BAR_CHART (self));

  if ((isnan (self->overlay_line_value) && isnan (value)) ||
      self->overlay_line_value == value)
    return;

  self->overlay_line_value = value;

  /* Clear the cached grid lines and labels as the overlay line might have been
   * the highest data value. */
  g_clear_pointer (&self->cached_continuous_axis_grid_line_values, g_array_unref);
  g_clear_pointer (&self->cached_continuous_axis_labels, g_ptr_array_unref);

  /* Re-render */
  gtk_widget_queue_resize (GTK_WIDGET (self));

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_OVERLAY_LINE_VALUE]);
}

/**
 * cc_bar_chart_get_selected_index:
 * @self: a #CcBarChart
 * @out_index: (out) (optional): return location for the selected index, or
 *   `NULL` to ignore
 *
 * Get the currently selected data index.
 *
 * If nothing is currently selected, @out_index will be set to `0` and `FALSE`
 * will be returned.
 *
 * Returns: `TRUE` if something is currently selected, `FALSE` otherwise
 */
gboolean
cc_bar_chart_get_selected_index (CcBarChart *self,
                                 size_t     *out_index)
{
  g_return_val_if_fail (CC_IS_BAR_CHART (self), FALSE);

  if (out_index != NULL)
    *out_index = self->selected_index_set ? self->selected_index : 0;

  return self->selected_index_set;
}

/**
 * cc_bar_chart_set_selected_index:
 * @self: a #CcBarChart
 * @is_selected: `TRUE` if something should be selected, `FALSE` if everything
 *   should be unselected
 * @idx: index of the data to select, ignored if @is_selected is `FALSE`
 *
 * Set the currently selected data index, or unselect everything.
 *
 * If @is_selected is `TRUE`, the data at @idx will be selected. If @is_selected
 * is `FALSE`, @idx will be ignored and all data will be unselected.
 */
void
cc_bar_chart_set_selected_index (CcBarChart *self,
                                 gboolean    is_selected,
                                 size_t      idx)
{
  g_return_if_fail (CC_IS_BAR_CHART (self));
  g_return_if_fail (!is_selected || idx < self->n_data);

  if (self->selected_index_set == is_selected &&
      (!self->selected_index_set || self->selected_index == idx))
    return;

  self->selected_index_set = is_selected;
  self->selected_index = is_selected ? idx : 0;

  if (is_selected)
    {
      g_assert (self->cached_groups != NULL);
      cc_bar_chart_group_set_selected_index (self->cached_groups->pdata[idx], TRUE, 0);
    }

  /* Re-render */
  gtk_widget_queue_draw (GTK_WIDGET (self));

  g_object_freeze_notify (G_OBJECT (self));
  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_SELECTED_INDEX]);
  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_SELECTED_INDEX_SET]);
  g_object_thaw_notify (G_OBJECT (self));
}
