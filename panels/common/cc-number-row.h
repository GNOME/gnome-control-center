/* cc-number-row.h
 *
 * Copyright 2024 Matthijs Velsink <mvelsink@gnome.org>
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

#include <adwaita.h>

G_BEGIN_DECLS

/**
 * CcNumberOrder:
 * @CC_NUMBER_ORDER_FIRST: place number first in the list of the row
 * @CC_NUMBER_ORDER_DEFAULT: use the `sort-type` of the row
 * @CC_NUMBER_ORDER_LAST: place number last in the list of the row
 *
 * Defines a special, fixed ordering of a `CcNumberObject` inside a
 * `CcNumberRow`.
 */
typedef enum
{
  CC_NUMBER_ORDER_FIRST,
  CC_NUMBER_ORDER_DEFAULT,
  CC_NUMBER_ORDER_LAST
} CcNumberOrder;

#define CC_TYPE_NUMBER_OBJECT (cc_number_object_get_type())
G_DECLARE_FINAL_TYPE (CcNumberObject, cc_number_object, CC, NUMBER_OBJECT, GObject)

CcNumberObject *cc_number_object_new             (int value);
CcNumberObject *cc_number_object_new_full        (int value, const char *string, CcNumberOrder order);

int           cc_number_object_get_value  (CcNumberObject *self);
const char*   cc_number_object_get_string (CcNumberObject *self);
CcNumberOrder cc_number_object_get_order  (CcNumberObject *self);

char *cc_number_object_to_string_for_seconds (CcNumberObject *self);
char *cc_number_object_to_string_for_minutes (CcNumberObject *self);

#define CC_TYPE_NUMBER_ROW (cc_number_row_get_type())
G_DECLARE_FINAL_TYPE (CcNumberRow, cc_number_row, CC, NUMBER_ROW, AdwComboRow)

CcNumberRow *cc_number_row_new (GtkSortType sort_type);

guint    cc_number_row_add_value      (CcNumberRow *self, int value);
guint    cc_number_row_add_value_full (CcNumberRow *self, int value, const char *string, CcNumberOrder order);
int      cc_number_row_get_value      (CcNumberRow *self, guint position);
gboolean cc_number_row_has_value      (CcNumberRow *self, int value, guint *position);

G_END_DECLS
