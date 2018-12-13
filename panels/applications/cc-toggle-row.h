/* cc-toggle-row.h
 *
 * Copyright 2018 Matthias Clasen <matthias.clasen@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
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
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define CC_TYPE_TOGGLE_ROW (cc_toggle_row_get_type())
G_DECLARE_FINAL_TYPE (CcToggleRow, cc_toggle_row, CC, TOGGLE_ROW, GtkListBoxRow)

CcToggleRow* cc_toggle_row_new         (void);

void         cc_toggle_row_set_allowed (CcToggleRow *row,
                                        gboolean     allowed);

gboolean     cc_toggle_row_get_allowed (CcToggleRow *row);

G_END_DECLS
