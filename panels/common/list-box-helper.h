/*
 * Copyright (C) 2014 Red Hat, Inc
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 */

#pragma once

#include <gtk/gtk.h>

#define CC_TYPE_LIST_BOX_ROW (cc_list_box_row_get_type ())
G_DECLARE_FINAL_TYPE (CcListBoxRow, cc_list_box_row, CC, LIST_BOX_ROW, GtkListBoxRow)

#define CC_TYPE_LIST_BOX (cc_list_box_get_type ())
G_DECLARE_FINAL_TYPE (CcListBox, cc_list_box, CC, LIST_BOX, GtkListBox)

void
cc_list_box_update_header_func (GtkListBoxRow *row,
                                GtkListBoxRow *before,
                                gpointer user_data);

void
cc_list_box_adjust_scrolling (GtkListBox *listbox);

void
cc_list_box_setup_scrolling (GtkListBox *listbox,
                             guint       num_rows);
