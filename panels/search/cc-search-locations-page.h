/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*-
 *
 * Copyright (C) 2012 Red Hat, Inc
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
 * Author: Cosimo Cecchi <cosimoc@gnome.org>
 */

#pragma once

#include <adwaita.h>

#define CC_TYPE_SEARCH_LOCATIONS_PAGE (cc_search_locations_page_get_type ())
G_DECLARE_FINAL_TYPE (CcSearchLocationsPage, cc_search_locations_page, CC, SEARCH_LOCATIONS_PAGE, AdwNavigationPage)

gboolean cc_search_locations_page_is_available        (void);
