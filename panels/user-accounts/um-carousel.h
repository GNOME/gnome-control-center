/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright 2016 (c) Red Hat, Inc,
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
 * Author: Felipe Borges <felipeborges@gnome.org>
 */

#ifndef UM_CAROUSEL_H
#define UM_CAROUSEL_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define UM_TYPE_CAROUSEL_ITEM (um_carousel_item_get_type ())

G_DECLARE_FINAL_TYPE (UmCarouselItem, um_carousel_item, UM, CAROUSEL_ITEM, GtkRadioButton)

#define UM_TYPE_CAROUSEL (um_carousel_get_type())

G_DECLARE_FINAL_TYPE (UmCarousel, um_carousel, UM, CAROUSEL, GtkRevealer)

GtkWidget       *um_carousel_item_new    (void);

UmCarousel      *um_carousel_new         (void);

void             um_carousel_purge_items (UmCarousel     *self);

UmCarouselItem  *um_carousel_find_item   (UmCarousel     *self,
                                          gconstpointer   data,
                                          GCompareFunc    func);

void             um_carousel_select_item (UmCarousel     *self,
                                          UmCarouselItem *item);

guint            um_carousel_get_item_count (UmCarousel  *self);

G_END_DECLS

#endif /* UM_CAROUSEL_H */
