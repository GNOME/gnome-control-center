/* cc-regional-language-row.h
 *
 * Copyright (C) 2024 The GNOME Project
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
 */

#pragma once

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define CC_TYPE_REGIONAL_LANGUAGE_ROW (cc_regional_language_row_get_type ())
G_DECLARE_FINAL_TYPE (CcRegionalLanguageRow, cc_regional_language_row, CC, REGIONAL_LANGUAGE_ROW, GtkListBoxRow)

CcRegionalLanguageRow *cc_regional_language_row_new                (const gchar *locale_id);

const gchar           *cc_regional_language_row_get_locale_id      (CcRegionalLanguageRow *row);

const gchar           *cc_regional_language_row_get_language       (CcRegionalLanguageRow *row);

const gchar           *cc_regional_language_row_get_language_local (CcRegionalLanguageRow *row);

const gchar           *cc_regional_language_row_get_country        (CcRegionalLanguageRow *row);

const gchar           *cc_regional_language_row_get_country_local  (CcRegionalLanguageRow *row);

void                   cc_regional_language_row_set_checked        (CcRegionalLanguageRow *row,
                                                                    gboolean checked);

void                   cc_regional_language_row_set_is_extra       (CcRegionalLanguageRow *row,
                                                                    gboolean is_extra);

gboolean               cc_regional_language_row_get_is_extra       (CcRegionalLanguageRow *row);

G_END_DECLS
