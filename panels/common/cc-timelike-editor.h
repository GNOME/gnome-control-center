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

#define CC_TYPE_TIMELIKE_EDITOR (cc_timelike_editor_get_type ())

G_DECLARE_FINAL_TYPE (CcTimelikeEditor, cc_timelike_editor, CC, TIMELIKE_EDITOR, AdwBin)

CcTimelikeEditor *cc_timelike_editor_new    (void);
void          cc_timelike_editor_set_time   (CcTimelikeEditor *self,
                                             guint             hour,
                                             guint             minute);
guint         cc_timelike_editor_get_hour   (CcTimelikeEditor *self);
guint         cc_timelike_editor_get_minute (CcTimelikeEditor *self);

G_END_DECLS
