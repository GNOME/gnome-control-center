/* cc-multitasking-row.h
 *
 * Copyright 2018 Purism SPC
 *           2021 Georges Basile Stavracas Neto <georges.stavracas@gmail.com>
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

#include <handy.h>

G_BEGIN_DECLS

#define CC_TYPE_MULTITASKING_ROW (cc_multitasking_row_get_type())
G_DECLARE_FINAL_TYPE (CcMultitaskingRow, cc_multitasking_row, CC, MULTITASKING_ROW, HdyPreferencesRow)

const gchar *cc_multitasking_row_get_subtitle (CcMultitaskingRow *self);
void         cc_multitasking_row_set_subtitle (CcMultitaskingRow *self,
                                               const gchar       *subtitle);

const gchar *cc_multitasking_row_get_icon_name (CcMultitaskingRow *self);
void         cc_multitasking_row_set_icon_name (CcMultitaskingRow *self,
                                                const gchar       *icon_name);

GtkWidget *cc_multitasking_row_get_activatable_widget (CcMultitaskingRow *self);
void       cc_multitasking_row_set_activatable_widget (CcMultitaskingRow *self,
                                                       GtkWidget         *widget);

gboolean cc_multitasking_row_get_use_underline (CcMultitaskingRow *self);
void     cc_multitasking_row_set_use_underline (CcMultitaskingRow *self,
                                                gboolean           use_underline);

gint cc_multitasking_row_get_title_lines (CcMultitaskingRow *self);
void cc_multitasking_row_set_title_lines (CcMultitaskingRow *self,
                                          gint               title_lines);

gint cc_multitasking_row_get_subtitle_lines (CcMultitaskingRow *self);
void cc_multitasking_row_set_subtitle_lines (CcMultitaskingRow *self,
                                             gint               subtitle_lines);

void cc_multitasking_row_add_prefix (CcMultitaskingRow *self,
                                     GtkWidget         *widget);

void cc_multitasking_row_add_artwork (CcMultitaskingRow *self,
                                      GtkWidget         *widget);

void cc_multitasking_row_activate (CcMultitaskingRow *self);

G_END_DECLS
