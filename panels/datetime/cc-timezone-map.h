/*
 * Copyright (C) 2010 Intel, Inc
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
 * Author: Thomas Wood <thomas.wood@intel.com>
 *
 */

#pragma once

#include <gtk/gtk.h>
#include "tz.h"

G_BEGIN_DECLS

#define CC_TYPE_TIMEZONE_MAP (cc_timezone_map_get_type ())
G_DECLARE_FINAL_TYPE (CcTimezoneMap, cc_timezone_map, CC, TIMEZONE_MAP, GtkWidget)

CcTimezoneMap *cc_timezone_map_new (void);

gboolean cc_timezone_map_set_timezone (CcTimezoneMap *map,
                                       const gchar   *timezone);
void cc_timezone_map_set_bubble_text (CcTimezoneMap *map,
                                      const gchar   *text);
TzLocation * cc_timezone_map_get_location (CcTimezoneMap *map);

G_END_DECLS
