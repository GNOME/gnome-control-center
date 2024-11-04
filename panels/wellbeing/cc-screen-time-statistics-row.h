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

#include <adwaita.h>
#include <glib-object.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define CC_TYPE_SCREEN_TIME_STATISTICS_ROW (cc_screen_time_statistics_row_get_type())
G_DECLARE_FINAL_TYPE (CcScreenTimeStatisticsRow, cc_screen_time_statistics_row, CC, SCREEN_TIME_STATISTICS_ROW, AdwActionRow)

CcScreenTimeStatisticsRow *cc_screen_time_statistics_row_new (void);

GFile *cc_screen_time_statistics_row_get_history_file (CcScreenTimeStatisticsRow *self);
void cc_screen_time_statistics_row_set_history_file (CcScreenTimeStatisticsRow *self,
                                                     GFile                     *history_file);

const GDate *cc_screen_time_statistics_row_get_selected_date (CcScreenTimeStatisticsRow *self);
void cc_screen_time_statistics_row_set_selected_date (CcScreenTimeStatisticsRow *self,
                                                      const GDate               *selected_date);

unsigned int cc_screen_time_statistics_row_get_daily_limit (CcScreenTimeStatisticsRow *self);
void cc_screen_time_statistics_row_set_daily_limit (CcScreenTimeStatisticsRow *self,
                                                    unsigned int               daily_limit_minutes);

G_END_DECLS
