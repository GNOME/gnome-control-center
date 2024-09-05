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

#define CC_TYPE_BAR_CHART_BAR (cc_bar_chart_bar_get_type())
G_DECLARE_FINAL_TYPE (CcBarChartBar, cc_bar_chart_bar, CC, BAR_CHART_BAR, GtkWidget)

CcBarChartBar *cc_bar_chart_bar_new (double      value,
                                     const char *accessible_description);

double cc_bar_chart_bar_get_value (CcBarChartBar *self);
void cc_bar_chart_bar_set_value (CcBarChartBar *self,
                                 double         value);

const char *cc_bar_chart_bar_get_accessible_description (CcBarChartBar *self);
void cc_bar_chart_bar_set_accessible_description (CcBarChartBar *self,
                                                  const char    *accessible_description);

G_END_DECLS
