/* -*- mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*- */
/* cc-timelike-editor.h
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

#include <adwaita.h>

G_BEGIN_DECLS

/**
 * CcTimelikeEditorMode:
 * @CC_TIMELIKE_EDITOR_MODE_TIME: a wall clock time [00:00, 23:59]
 * @CC_TIMELIKE_EDITOR_MODE_DURATION: a duration in hours and minutes, may have
 *   an arbitrary range
 *
 * What kind of time the editor is meant to represent — a wall clock time,
 * or a duration.
 */
typedef enum {
  CC_TIMELIKE_EDITOR_MODE_TIME,
  CC_TIMELIKE_EDITOR_MODE_DURATION,
} CcTimelikeEditorMode;

#define CC_TYPE_TIMELIKE_EDITOR (cc_timelike_editor_get_type ())

G_DECLARE_FINAL_TYPE (CcTimelikeEditor, cc_timelike_editor, CC, TIMELIKE_EDITOR, GtkWidget)

CcTimelikeEditor *cc_timelike_editor_new    (void);
void          cc_timelike_editor_set_time   (CcTimelikeEditor *self,
                                             guint             hour,
                                             guint             minute);
guint         cc_timelike_editor_get_hour   (CcTimelikeEditor *self);
guint         cc_timelike_editor_get_minute (CcTimelikeEditor *self);

CcTimelikeEditorMode cc_timelike_editor_get_mode (CcTimelikeEditor     *self);
void                 cc_timelike_editor_set_mode (CcTimelikeEditor     *self,
                                                  CcTimelikeEditorMode  mode);

guint cc_timelike_editor_get_minute_increment (CcTimelikeEditor *self);
void  cc_timelike_editor_set_minute_increment (CcTimelikeEditor *self,
                                               guint             minutes);

G_END_DECLS
