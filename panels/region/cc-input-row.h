/*
 * Copyright © 2018 Canonical Ltd.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <gtk/gtk.h>
#include <gio/gdesktopappinfo.h>

G_BEGIN_DECLS

#define CC_TYPE_INPUT_ROW (cc_input_row_get_type ())
G_DECLARE_FINAL_TYPE (CcInputRow, cc_input_row, CC, INPUT_ROW, GtkListBoxRow)

CcInputRow      *cc_input_row_new                 (const gchar     *type,
                                                   const gchar     *id,
                                                   GDesktopAppInfo *app_info);

const gchar     *cc_input_row_get_input_type      (CcInputRow      *row);

const gchar     *cc_input_row_get_id              (CcInputRow      *row);

GDesktopAppInfo *cc_input_row_get_app_info        (CcInputRow      *row);

void             cc_input_row_set_label           (CcInputRow      *row,
                                                   const gchar     *text);

void             cc_input_row_set_is_input_method (CcInputRow      *row,
                                                   gboolean         is_input_method);

G_END_DECLS
