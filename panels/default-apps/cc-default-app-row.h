/*
 * Copyright © 2019 Canonical Ltd.
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

G_BEGIN_DECLS

#define CC_TYPE_DEFAULT_APP_ROW (cc_default_app_row_get_type ())
G_DECLARE_FINAL_TYPE (CcDefaultAppRow, cc_default_app_row, CC, DEFAULT_APP_ROW, GtkListBoxRow)

CcDefaultAppRow *cc_default_app_row_new (const gchar *content_type, const char *extra_type_filter, const gchar *label, GtkSizeGroup *size_group);

G_END_DECLS
