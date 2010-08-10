/* bg-colors-source.h */
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

#ifndef _BG_COLORS_SOURCE_H
#define _BG_COLORS_SOURCE_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define BG_TYPE_COLORS_SOURCE bg_colors_source_get_type()

#define BG_COLORS_SOURCE(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
  BG_TYPE_COLORS_SOURCE, BgColorsSource))

#define BG_COLORS_SOURCE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), \
  BG_TYPE_COLORS_SOURCE, BgColorsSourceClass))

#define BG_IS_COLORS_SOURCE(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
  BG_TYPE_COLORS_SOURCE))

#define BG_IS_COLORS_SOURCE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), \
  BG_TYPE_COLORS_SOURCE))

#define BG_COLORS_SOURCE_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), \
  BG_TYPE_COLORS_SOURCE, BgColorsSourceClass))

typedef struct _BgColorsSource BgColorsSource;
typedef struct _BgColorsSourceClass BgColorsSourceClass;
typedef struct _BgColorsSourcePrivate BgColorsSourcePrivate;

struct _BgColorsSource
{
  GObject parent;

  BgColorsSourcePrivate *priv;
};

struct _BgColorsSourceClass
{
  GObjectClass parent_class;
};

GType bg_colors_source_get_type (void) G_GNUC_CONST;

BgColorsSource *bg_colors_source_new (void);
GtkListStore * bg_colors_source_get_liststore (BgColorsSource *source);

G_END_DECLS

#endif /* _BG_COLORS_SOURCE_H */
