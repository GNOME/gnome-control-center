/*
 * Copyright 2011 Inclusive Design Research Centre, OCAD University.
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
 *
 * Author: Joseph Scheuhammer <clown@alum.mit.edu>
 */

#pragma once

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define ZOOM_TYPE_OPTIONS (zoom_options_get_type())

G_DECLARE_FINAL_TYPE (ZoomOptions, zoom_options, ZOOM, OPTIONS, GtkDialog)

ZoomOptions *zoom_options_new (GtkWindow *parent);

G_END_DECLS
