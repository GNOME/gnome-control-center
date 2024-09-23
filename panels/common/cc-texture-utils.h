/* GTK - The GIMP Toolkit
 * Copyright (C) 2016 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

/* Adapted from gdktextureutilsprivate.h
 * https://gitlab.gnome.org/GNOME/gtk/-/blob/bef6352401561d71756326f50c3f223655b3d16e/gtk/gdktextureutilsprivate.h
 */

#pragma once

#include <gtk/gtk.h>

G_BEGIN_DECLS

GdkPaintable *cc_texture_new_from_resource_scaled (const char *path,
                                                   double      scale);

G_END_DECLS
