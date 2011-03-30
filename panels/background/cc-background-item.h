/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2010-2011 Red Hat, Inc.
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

#include <libgnome-desktop/gnome-desktop-thumbnail.h>
#include <gdesktop-enums.h>
#include <libgnome-desktop/gnome-bg.h>

G_BEGIN_DECLS

#define CC_TYPE_BACKGROUND_ITEM         (cc_background_item_get_type ())
#define CC_BACKGROUND_ITEM(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), CC_TYPE_BACKGROUND_ITEM, CcBackgroundItem))
#define CC_BACKGROUND_ITEM_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), CC_TYPE_BACKGROUND_ITEM, CcBackgroundItemClass))
#define CC_IS_BACKGROUND_ITEM(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), CC_TYPE_BACKGROUND_ITEM))
#define CC_IS_BACKGROUND_ITEM_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), CC_TYPE_BACKGROUND_ITEM))
#define CC_BACKGROUND_ITEM_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), CC_TYPE_BACKGROUND_ITEM, CcBackgroundItemClass))

typedef enum {
	CC_BACKGROUND_ITEM_HAS_SHADING   = 1 << 0,
	CC_BACKGROUND_ITEM_HAS_PLACEMENT = 1 << 1,
	CC_BACKGROUND_ITEM_HAS_PCOLOR    = 1 << 2,
	CC_BACKGROUND_ITEM_HAS_SCOLOR    = 1 << 3,
	CC_BACKGROUND_ITEM_HAS_URI       = 1 << 4
} CcBackgroundItemFlags;

#define CC_BACKGROUND_ITEM_HAS_ALL (CC_BACKGROUND_ITEM_HAS_SHADING &	\
				    CC_BACKGROUND_ITEM_HAS_PLACEMENT &	\
				    CC_BACKGROUND_ITEM_HAS_PCOLOR &	\
				    CC_BACKGROUND_ITEM_HAS_SCOLOR &	\
				    CC_BACKGROUND_ITEM_HAS_FNAME)

typedef struct CcBackgroundItemPrivate CcBackgroundItemPrivate;

typedef struct
{
        GObject                  parent;
        CcBackgroundItemPrivate *priv;
} CcBackgroundItem;

typedef struct
{
        GObjectClass   parent_class;
} CcBackgroundItemClass;

GType              cc_background_item_get_type (void);

CcBackgroundItem * cc_background_item_new                 (const char                   *uri);
CcBackgroundItem * cc_background_item_copy                (CcBackgroundItem             *item);
gboolean           cc_background_item_load                (CcBackgroundItem             *item,
							   GFileInfo                    *info);
gboolean           cc_background_item_changes_with_time   (CcBackgroundItem             *item);

GIcon     *        cc_background_item_get_thumbnail       (CcBackgroundItem             *item,
                                                           GnomeDesktopThumbnailFactory *thumbs,
                                                           int                           width,
                                                           int                           height);
GIcon     *        cc_background_item_get_frame_thumbnail (CcBackgroundItem             *item,
                                                           GnomeDesktopThumbnailFactory *thumbs,
                                                           int                           width,
                                                           int                           height,
                                                           int                           frame,
                                                           gboolean                      force_size);

GDesktopBackgroundStyle   cc_background_item_get_placement  (CcBackgroundItem *item);
GDesktopBackgroundShading cc_background_item_get_shading    (CcBackgroundItem *item);
const char *              cc_background_item_get_uri        (CcBackgroundItem *item);
const char *              cc_background_item_get_source_url (CcBackgroundItem *item);
const char *              cc_background_item_get_source_xml (CcBackgroundItem *item);
CcBackgroundItemFlags     cc_background_item_get_flags      (CcBackgroundItem *item);
const char *              cc_background_item_get_pcolor     (CcBackgroundItem *item);
const char *              cc_background_item_get_scolor     (CcBackgroundItem *item);
const char *              cc_background_item_get_name       (CcBackgroundItem *item);
const char *              cc_background_item_get_size       (CcBackgroundItem *item);
gboolean                  cc_background_item_get_needs_download (CcBackgroundItem *item);

gboolean                  cc_background_item_compare        (CcBackgroundItem *saved,
							     CcBackgroundItem *configured);

void                      cc_background_item_dump           (CcBackgroundItem *item);

G_END_DECLS

#endif /* __CC_BACKGROUND_ITEM_H */
