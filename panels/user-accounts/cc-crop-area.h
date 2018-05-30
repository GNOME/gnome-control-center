/*
 * Copyright © 2009 Bastien Nocera <hadess@hadess.net>
 *
 * Licensed under the GNU General Public License Version 2
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
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <glib-object.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define CC_TYPE_CROP_AREA (cc_crop_area_get_type ())
G_DECLARE_FINAL_TYPE (CcCropArea, cc_crop_area, CC, CROP_AREA, GtkDrawingArea)

GtkWidget *cc_crop_area_new                  (void);
GdkPixbuf *cc_crop_area_get_picture          (CcCropArea *area);
void       cc_crop_area_set_picture          (CcCropArea *area,
                                              GdkPixbuf  *pixbuf);
void       cc_crop_area_set_min_size         (CcCropArea *area,
                                              gint        width,
                                              gint        height);
void       cc_crop_area_set_constrain_aspect (CcCropArea *area,
                                              gboolean    constrain);

G_END_DECLS
