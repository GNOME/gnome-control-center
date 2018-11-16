/* cc-action-row.h
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

#define CC_TYPE_ACTION_ROW (cc_action_row_get_type())
G_DECLARE_FINAL_TYPE (CcActionRow, cc_action_row, CC, ACTION_ROW, GtkListBoxRow)

CcActionRow* cc_action_row_new          (void);

void         cc_action_row_set_title    (CcActionRow *row,
                                         const gchar *label);

void         cc_action_row_set_subtitle (CcActionRow *row,
                                         const gchar *label);

void         cc_action_row_set_action   (CcActionRow *row,
                                         const gchar *action,
                                         gboolean     sensitive);

G_END_DECLS
