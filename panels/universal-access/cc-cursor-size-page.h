/*
 * Copyright 2020 Canonical Ltd.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2.1 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#pragma once

#include <adwaita.h>

G_BEGIN_DECLS

#define CC_TYPE_CURSOR_SIZE_PAGE (cc_cursor_size_page_get_type ())
G_DECLARE_FINAL_TYPE (CcCursorSizePage, cc_cursor_size_page, CC, CURSOR_SIZE_PAGE, AdwNavigationPage)

CcCursorSizePage *cc_cursor_size_page_new (void);

G_END_DECLS
