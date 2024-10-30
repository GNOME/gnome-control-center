/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* cc-mask-paintable.h
 *
 * Copyright 2024 Alice Mikhaylenko <alicem@gnome.org>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
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
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define CC_TYPE_MASK_PAINTABLE (cc_mask_paintable_get_type())
G_DECLARE_FINAL_TYPE (CcMaskPaintable, cc_mask_paintable, CC, MASK_PAINTABLE, GObject)

GdkPaintable *cc_mask_paintable_new (void);

GdkPaintable *cc_mask_paintable_get_paintable (CcMaskPaintable *self);
void          cc_mask_paintable_set_paintable (CcMaskPaintable *self,
                                               GdkPaintable    *paintable);

GdkRGBA      *cc_mask_paintable_get_rgba      (CcMaskPaintable *self);
void          cc_mask_paintable_set_rgba      (CcMaskPaintable *self,
                                               GdkRGBA         *rgba);

gboolean      cc_mask_paintable_get_follow_accent (CcMaskPaintable *self);
void          cc_mask_paintable_set_follow_accent (CcMaskPaintable *self,
                                                   gboolean         follow_accent);

void          cc_mask_paintable_set_resource_scaled (CcMaskPaintable *self,
                                                     const char      *resource_path,
                                                     GtkWidget       *parent_widget);

G_END_DECLS
