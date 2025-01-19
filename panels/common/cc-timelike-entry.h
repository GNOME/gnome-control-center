/* -*- mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*- */
/* cc-timelike-entry.h
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

#pragma once

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define CC_TYPE_TIMELIKE_ENTRY (cc_timelike_entry_get_type ())
G_DECLARE_FINAL_TYPE (CcTimelikeEntry, cc_timelike_entry, CC, TIMELIKE_ENTRY, GtkWidget)

GtkWidget *cc_timelike_entry_new        (void);
void       cc_timelike_entry_set_time   (CcTimelikeEntry *self,
                                         guint            hour,
                                         guint            minute);
guint      cc_timelike_entry_get_minute (CcTimelikeEntry *self);
guint      cc_timelike_entry_get_hour   (CcTimelikeEntry *self);
gboolean   cc_timelike_entry_get_is_am  (CcTimelikeEntry *self);
void       cc_timelike_entry_set_is_am  (CcTimelikeEntry *self,
                                         gboolean         is_am);
gboolean   cc_timelike_entry_get_am_pm  (CcTimelikeEntry *self);
void       cc_timelike_entry_set_am_pm  (CcTimelikeEntry *self,
                                         gboolean         is_am_pm);

guint      cc_timelike_entry_get_minute_increment (CcTimelikeEntry *self);
void       cc_timelike_entry_set_minute_increment (CcTimelikeEntry *self,
                                                   guint            minutes);

void        cc_timelike_entry_get_hours_and_minutes_midpoints (CcTimelikeEntry *self,
                                                               float           *out_hours_midpoint_x,
                                                               float           *out_minutes_midpoint_x);

G_END_DECLS
