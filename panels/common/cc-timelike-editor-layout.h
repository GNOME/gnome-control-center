/* -*- mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*- */
/* cc-timelike-editor-layout.h
 *
 * Copyright 2025 GNOME Foundation, Inc.
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
 *   Philip Withnall <pwithnall@gnome.org>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define CC_TYPE_TIMELIKE_EDITOR_LAYOUT (cc_timelike_editor_layout_get_type ())

G_DECLARE_FINAL_TYPE (CcTimelikeEditorLayout, cc_timelike_editor_layout, CC, TIMELIKE_EDITOR_LAYOUT, GtkLayoutManager)

CcTimelikeEditorLayout *cc_timelike_editor_layout_new (void);

unsigned int cc_timelike_editor_layout_get_row_spacing (CcTimelikeEditorLayout *self);
void cc_timelike_editor_layout_set_row_spacing (CcTimelikeEditorLayout *self,
                                                unsigned int            row_spacing);

unsigned int cc_timelike_editor_layout_get_column_spacing (CcTimelikeEditorLayout *self);
void cc_timelike_editor_layout_set_column_spacing (CcTimelikeEditorLayout *self,
                                                   unsigned int            column_spacing);

G_END_DECLS
