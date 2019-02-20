/* -*- mode: c; style: linux -*-
 *
 * Copyright (C) 2019 Purism SPC
 *
 * Written by: Adrien Plazas <adrien.plazas@puri.sm>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
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

#define CC_TYPE_MOUSE_SCROLL_TEST (cc_mouse_scroll_test_get_type ())
G_DECLARE_FINAL_TYPE (CcMouseScrollTest, cc_mouse_scroll_test, CC, MOUSE_SCROLL_TEST, GtkDrawingArea)

void cc_mouse_scroll_test_set_show_gegl (CcMouseScrollTest *self,
                                         gboolean           show_gegl);

G_END_DECLS
