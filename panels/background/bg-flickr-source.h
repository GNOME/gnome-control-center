/* bg-flickr-source.h */
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

#ifndef _BG_FLICKR_SOURCE_H
#define _BG_FLICKR_SOURCE_H

#include <gtk/gtk.h>
#include "bg-source.h"

G_BEGIN_DECLS

#define BG_TYPE_FLICKR_SOURCE bg_flickr_source_get_type()

#define BG_FLICKR_SOURCE(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
  BG_TYPE_FLICKR_SOURCE, BgFlickrSource))

#define BG_FLICKR_SOURCE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), \
  BG_TYPE_FLICKR_SOURCE, BgFlickrSourceClass))

#define BG_IS_FLICKR_SOURCE(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
  BG_TYPE_FLICKR_SOURCE))

#define BG_IS_FLICKR_SOURCE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), \
  BG_TYPE_FLICKR_SOURCE))

#define BG_FLICKR_SOURCE_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), \
  BG_TYPE_FLICKR_SOURCE, BgFlickrSourceClass))

typedef struct _BgFlickrSource BgFlickrSource;
typedef struct _BgFlickrSourceClass BgFlickrSourceClass;
typedef struct _BgFlickrSourcePrivate BgFlickrSourcePrivate;

struct _BgFlickrSource
{
  BgSource parent;

  BgFlickrSourcePrivate *priv;
};

struct _BgFlickrSourceClass
{
  BgSourceClass parent_class;
};

GType bg_flickr_source_get_type (void) G_GNUC_CONST;

BgFlickrSource *bg_flickr_source_new (void);

G_END_DECLS

#endif /* _BG_FLICKR_SOURCE_H */
