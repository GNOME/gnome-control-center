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

#include "cc-bar-chart-bar.h"

G_BEGIN_DECLS

#define CC_TYPE_BAR_CHART_GROUP (cc_bar_chart_group_get_type())
G_DECLARE_FINAL_TYPE (CcBarChartGroup, cc_bar_chart_group, CC, BAR_CHART_GROUP, GtkWidget)

CcBarChartGroup *cc_bar_chart_group_new (void);

CcBarChartBar * const *cc_bar_chart_group_get_bars (CcBarChartGroup *self,
                                                    size_t          *out_n_bars);
void cc_bar_chart_group_insert_bar (CcBarChartGroup *self,
                                    int              idx,
                                    CcBarChartBar   *bar);
void cc_bar_chart_group_remove_bar (CcBarChartGroup *self,
                                    CcBarChartBar   *bar);

gboolean cc_bar_chart_group_get_selectable (CcBarChartGroup *self);
void cc_bar_chart_group_set_selectable (CcBarChartGroup *self,
                                        gboolean         selectable);

gboolean cc_bar_chart_group_get_is_selected (CcBarChartGroup *self);
void cc_bar_chart_group_set_is_selected (CcBarChartGroup *self,
                                         gboolean         is_selected);

gboolean cc_bar_chart_group_get_selected_index (CcBarChartGroup *self,
                                                size_t          *out_index);
void cc_bar_chart_group_set_selected_index (CcBarChartGroup *self,
                                            gboolean         is_selected,
                                            size_t           idx);

double cc_bar_chart_group_get_scale (CcBarChartGroup *self);
void cc_bar_chart_group_set_scale (CcBarChartGroup *self,
                                   double           scale);

G_END_DECLS
