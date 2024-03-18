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

#define CC_TYPE_BREAK_SCHEDULE (cc_break_schedule_get_type())
G_DECLARE_FINAL_TYPE (CcBreakSchedule, cc_break_schedule, CC, BREAK_SCHEDULE, GObject)

CcBreakSchedule *cc_break_schedule_new (guint duration_secs,
                                        guint interval_secs);

char *cc_break_schedule_get_formatted_duration (CcBreakSchedule *self);
char *cc_break_schedule_get_formatted_interval (CcBreakSchedule *self);

gint cc_break_schedule_compare (CcBreakSchedule *a,
                                CcBreakSchedule *b);

guint cc_break_schedule_get_duration_secs (CcBreakSchedule *self);
guint cc_break_schedule_get_interval_secs (CcBreakSchedule *self);

#define CC_TYPE_BREAK_SCHEDULE_ROW (cc_break_schedule_row_get_type())
G_DECLARE_FINAL_TYPE (CcBreakScheduleRow, cc_break_schedule_row, CC, BREAK_SCHEDULE_ROW, AdwComboRow)

CcBreakScheduleRow *cc_break_schedule_row_new (void) G_GNUC_WARN_UNUSED_RESULT;

G_END_DECLS
