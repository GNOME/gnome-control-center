/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2010 Red Hat, Inc.
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 */

#ifndef __CC_BACKGROUND_ITEM_H
#define __CC_BACKGROUND_ITEM_H

#include <glib-object.h>

#define GNOME_DESKTOP_USE_UNSTABLE_API
#include <libgnomeui/gnome-desktop-thumbnail.h>
#include "cc-background-item.h"

G_BEGIN_DECLS

#define CC_TYPE_BACKGROUND_ITEM         (cc_background_item_get_type ())
#define CC_BACKGROUND_ITEM(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), CC_TYPE_BACKGROUND_ITEM, CcBackgroundItem))
#define CC_BACKGROUND_ITEM_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), CC_TYPE_BACKGROUND_ITEM, CcBackgroundItemClass))
#define CC_IS_BACKGROUND_ITEM(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), CC_TYPE_BACKGROUND_ITEM))
#define CC_IS_BACKGROUND_ITEM_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), CC_TYPE_BACKGROUND_ITEM))
#define CC_BACKGROUND_ITEM_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), CC_TYPE_BACKGROUND_ITEM, CcBackgroundItemClass))

typedef struct CcBackgroundItemPrivate CcBackgroundItemPrivate;

typedef struct
{
        GObject                  parent;
        CcBackgroundItemPrivate *priv;
} CcBackgroundItem;

typedef struct
{
        GObjectClass   parent_class;
        void (* changed)           (CcBackgroundItem *item);
} CcBackgroundItemClass;

GType              cc_background_item_get_type (void);

CcBackgroundItem * cc_background_item_new                 (const char                   *filename);
gboolean           cc_background_item_load                (CcBackgroundItem             *item);
gboolean           cc_background_item_changes_with_time   (CcBackgroundItem             *item);

GdkPixbuf *        cc_background_item_get_thumbnail       (CcBackgroundItem             *item,
                                                           GnomeDesktopThumbnailFactory *thumbs,
                                                           int                           width,
                                                           int                           height);
GdkPixbuf *        cc_background_item_get_frame_thumbnail (CcBackgroundItem             *item,
                                                           GnomeDesktopThumbnailFactory *thumbs,
                                                           int                           width,
                                                           int                           height,
                                                           int                           frame);

G_END_DECLS

#endif /* __CC_BACKGROUND_ITEM_H */
