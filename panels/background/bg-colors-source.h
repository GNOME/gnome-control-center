/* bg-colors-source.h */
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
#include "bg-source.h"

G_BEGIN_DECLS

#define BG_TYPE_COLORS_SOURCE (bg_colors_source_get_type ())
G_DECLARE_FINAL_TYPE (BgColorsSource, bg_colors_source, BG, COLORS_SOURCE, BgSource)

BgColorsSource *bg_colors_source_new (GtkWidget *widget);

gboolean bg_colors_source_add        (BgColorsSource       *self,
                                      GdkRGBA              *rgba,
                                      GtkTreeRowReference **ret_row_ref);

G_END_DECLS
