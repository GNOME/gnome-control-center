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

#pragma once

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define CC_TYPE_CAROUSEL_ITEM (cc_carousel_item_get_type ())

G_DECLARE_FINAL_TYPE (CcCarouselItem, cc_carousel_item, CC, CAROUSEL_ITEM, GtkRadioButton)

#define CC_TYPE_CAROUSEL (cc_carousel_get_type ())

G_DECLARE_FINAL_TYPE (CcCarousel, cc_carousel, CC, CAROUSEL, GtkRevealer)

GtkWidget       *cc_carousel_item_new    (void);

CcCarousel      *cc_carousel_new         (void);

void             cc_carousel_purge_items (CcCarousel     *self);

CcCarouselItem  *cc_carousel_find_item   (CcCarousel     *self,
                                          gconstpointer   data,
                                          GCompareFunc    func);

void             cc_carousel_select_item (CcCarousel     *self,
                                          CcCarouselItem *item);

guint            cc_carousel_get_item_count (CcCarousel  *self);

G_END_DECLS
