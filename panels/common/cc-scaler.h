/*
 * Copyright © 2018 Benjamin Otte
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Benjamin Otte <otte@gnome.org>
 */

/* Copied from gtkscalerprivate.h, renamed to CcScaler
 * https://gitlab.gnome.org/GNOME/gtk/-/blob/90c9e88ee91fb8a61563f14df0b59588f9068ee9/gtk/gtkscalerprivate.h */

#pragma once

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define CC_TYPE_SCALER (cc_scaler_get_type ())
G_DECLARE_FINAL_TYPE (CcScaler, cc_scaler, CC, SCALER, GObject)

GdkPaintable *cc_scaler_new (GdkPaintable   *paintable,
                             double          scale);

G_END_DECLS

