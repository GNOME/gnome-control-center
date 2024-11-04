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

#pragma once

#include <glib-object.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define CC_TYPE_BAR_CHART (cc_bar_chart_get_type())
G_DECLARE_FINAL_TYPE (CcBarChart, cc_bar_chart, CC, BAR_CHART, GtkWidget)

CcBarChart *cc_bar_chart_new (void);

const char * const *cc_bar_chart_get_discrete_axis_labels (CcBarChart *self,
                                                           size_t     *out_n_discrete_axis_labels);
void cc_bar_chart_set_discrete_axis_labels (CcBarChart         *self,
                                            const char * const *labels);

/**
 * CcBarChartLabelCallback:
 * @chart: a #CcBarChart
 * @value: value to represent as a label
 * @user_data: user data passed to cc_bar_chart_set_continuous_axis_label_callback()
 *
 * Build a human readable textual label representing @value.
 *
 * @value comes from the same domain as the chart data, but might not be one of
 * the chart data values — it might come from the #CcBarChartGridLineCallback,
 * for example.
 *
 * The returned label may be used to label a point on the continuous axis of the
 * chart.
 *
 * Typically a #CcBarChartLabelCallback will format @value to an appropriate
 * precision and with an appropriate unit, as per the
 * [SI Brochure](https://www.bipm.org/en/publications/si-brochure).
 *
 * Returns: (not nullable) (transfer full): human readable text form of @value
 */
typedef char *(*CcBarChartLabelCallback) (CcBarChart *chart,
                                          double      value,
                                          void       *user_data);

void cc_bar_chart_set_continuous_axis_label_callback (CcBarChart              *self,
                                                      CcBarChartLabelCallback  callback,
                                                      void                    *user_data,
                                                      GDestroyNotify           destroy_notify);

/**
 * CcBarChartGridLineCallback:
 * @chart: a #CcBarChart
 * @idx: grid line index
 * @user_data: user data passed to cc_bar_chart_set_continuous_axis_grid_line_callback()
 *
 * Generate the value of the @idx-th grid line.
 *
 * The return value is in the same domain as the chart data, but does not have
 * to be one of the chart data values. The returned values must be monotonically
 * increasing with @idx.
 *
 * This function is called repeatedly when laying out the chart, to generate the
 * grid lines for the chart area. It is called with increasing values of @idx
 * until the chart area layout is complete. @idx has no special meaning beyond
 * being a generator index.
 *
 * Typically a #CcBarChartGridLineCallback will return @idx multiplied by the
 * desired interval (in the data domain) between grid lines. Logarithmic graphs
 * would return an exponential of @idx.
 *
 * Returns: @idx-th grid line value
 */
typedef double (*CcBarChartGridLineCallback) (CcBarChart   *chart,
                                              unsigned int  idx,
                                              void         *user_data);

void cc_bar_chart_set_continuous_axis_grid_line_callback (CcBarChart                 *self,
                                                          CcBarChartGridLineCallback  callback,
                                                          void                       *user_data,
                                                          GDestroyNotify              destroy_notify);

const double *cc_bar_chart_get_data (CcBarChart *self,
                                     size_t     *out_n_data);
void cc_bar_chart_set_data (CcBarChart   *self,
                            const double *data,
                            size_t        n_data);

double cc_bar_chart_get_overlay_line_value (CcBarChart *self);
void cc_bar_chart_set_overlay_line_value (CcBarChart *self,
                                          double      value);

gboolean cc_bar_chart_get_selected_index (CcBarChart *self,
                                          size_t     *out_index);
void cc_bar_chart_set_selected_index (CcBarChart *self,
                                      gboolean    is_selected,
                                      size_t      idx);

G_END_DECLS
