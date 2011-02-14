/* bg-pictures-source.h */
/*
 * Copyright (C) 2010 Intel, Inc
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
 * Author: Thomas Wood <thomas.wood@intel.com>
 *
 */


#ifndef _BG_PICTURES_SOURCE_H
#define _BG_PICTURES_SOURCE_H

#include <gtk/gtk.h>
#include "bg-source.h"
#include "cc-background-item.h"

G_BEGIN_DECLS

#define BG_TYPE_PICTURES_SOURCE bg_pictures_source_get_type()

#define BG_PICTURES_SOURCE(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
  BG_TYPE_PICTURES_SOURCE, BgPicturesSource))

#define BG_PICTURES_SOURCE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), \
  BG_TYPE_PICTURES_SOURCE, BgPicturesSourceClass))

#define BG_IS_PICTURES_SOURCE(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
  BG_TYPE_PICTURES_SOURCE))

#define BG_IS_PICTURES_SOURCE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), \
  BG_TYPE_PICTURES_SOURCE))

#define BG_PICTURES_SOURCE_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), \
  BG_TYPE_PICTURES_SOURCE, BgPicturesSourceClass))

typedef struct _BgPicturesSource BgPicturesSource;
typedef struct _BgPicturesSourceClass BgPicturesSourceClass;
typedef struct _BgPicturesSourcePrivate BgPicturesSourcePrivate;

struct _BgPicturesSource
{
  BgSource parent;

  BgPicturesSourcePrivate *priv;
};

struct _BgPicturesSourceClass
{
  BgSourceClass parent_class;
};

GType bg_pictures_source_get_type (void) G_GNUC_CONST;

BgPicturesSource *bg_pictures_source_new            (void);
char             *bg_pictures_source_get_cache_path (void);
char             *bg_pictures_source_get_unique_path(const char *uri);
gboolean          bg_pictures_source_add            (BgPicturesSource *bg_source,
						     const char       *uri);
gboolean          bg_pictures_source_remove         (BgPicturesSource *bg_source,
						     CcBackgroundItem *item);
gboolean          bg_pictures_source_is_known       (BgPicturesSource *bg_source,
						     const char       *uri);

G_END_DECLS

#endif /* _BG_PICTURES_SOURCE_H */
