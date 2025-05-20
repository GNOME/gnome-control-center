/* cc-locale-row.h
 *
 * Copyright 2025 Hari Rana <theevilskeleton@riseup.net>
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
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define CC_TYPE_LOCALE_ROW (cc_locale_row_get_type ())
G_DECLARE_FINAL_TYPE (CcLocaleRow, cc_locale_row, CC, LOCALE_ROW, GtkListBoxRow)

/**
 * CcLocaleLayoutType:
 *
 * @CC_LOCALE_LAYOUT_TYPE_NONE: no layout type
 * @CC_LOCALE_LAYOUT_TYPE_REGION: the title of the row is the region
 * @CC_LOCALE_LAYOUT_TYPE_LANGUAGE: the title of the row is the language
 */
typedef enum {
    CC_LOCALE_LAYOUT_TYPE_NONE = -1,
    CC_LOCALE_LAYOUT_TYPE_REGION,
    CC_LOCALE_LAYOUT_TYPE_LANGUAGE,
} CcLocaleLayoutType;

GtkWidget *cc_locale_row_new (const char *locale_id, CcLocaleLayoutType layout_type);
CcLocaleLayoutType cc_locale_row_get_locale_layout_type (CcLocaleRow *self);
const char *cc_locale_row_get_locale_id (CcLocaleRow *self);
const char *cc_locale_row_get_language (CcLocaleRow *self);
const char *cc_locale_row_get_language_local (CcLocaleRow *self);
const char *cc_locale_row_get_country (CcLocaleRow *self);
const char *cc_locale_row_get_country_local (CcLocaleRow *self);
void cc_locale_row_set_is_extra (CcLocaleRow *self, gboolean is_extra);
gboolean cc_locale_row_get_is_extra (CcLocaleRow *self);
void cc_locale_row_add_suffix (CcLocaleRow *self, GtkWidget *widget);

G_END_DECLS
