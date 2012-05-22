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

#ifndef _BG_SOURCE_H
#define _BG_SOURCE_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define THUMBNAIL_WIDTH 256
#define THUMBNAIL_HEIGHT (THUMBNAIL_WIDTH * 3 / 4)

#define BG_TYPE_SOURCE bg_source_get_type()

#define BG_SOURCE(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
  BG_TYPE_SOURCE, BgSource))

#define BG_SOURCE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), \
  BG_TYPE_SOURCE, BgSourceClass))

#define BG_IS_SOURCE(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
  BG_TYPE_SOURCE))

#define BG_IS_SOURCE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), \
  BG_TYPE_SOURCE))

#define BG_SOURCE_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), \
  BG_TYPE_SOURCE, BgSourceClass))

typedef struct _BgSource BgSource;
typedef struct _BgSourceClass BgSourceClass;
typedef struct _BgSourcePrivate BgSourcePrivate;

struct _BgSource
{
  GObject parent;

  BgSourcePrivate *priv;
};

struct _BgSourceClass
{
  GObjectClass parent_class;
};

GType bg_source_get_type (void) G_GNUC_CONST;

GtkListStore* bg_source_get_liststore (BgSource *source);

G_END_DECLS

#endif /* _BG_SOURCE_H */
