/* bg-colors-source.c */
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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Thomas Wood <thomas.wood@intel.com>
 *
 */

#include <config.h>
#include "bg-colors-source.h"

#include "cc-background-item.h"

#include <glib/gi18n-lib.h>
#include <gdesktop-enums.h>

G_DEFINE_TYPE (BgColorsSource, bg_colors_source, BG_TYPE_SOURCE)

#define COLORS_SOURCE_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), BG_TYPE_COLORS_SOURCE, BgColorsSourcePrivate))

struct {
  GDesktopBackgroundShading type;
  int orientation;
  const char *pcolor;
} items[] = {
  { G_DESKTOP_BACKGROUND_SHADING_SOLID, -1, "#db5d33" },
  { G_DESKTOP_BACKGROUND_SHADING_SOLID, -1, "#008094" },
  { G_DESKTOP_BACKGROUND_SHADING_SOLID, -1, "#5d479d" },
  { G_DESKTOP_BACKGROUND_SHADING_SOLID, -1, "#ab2876" },
  { G_DESKTOP_BACKGROUND_SHADING_SOLID, -1, "#fad166" },
  { G_DESKTOP_BACKGROUND_SHADING_SOLID, -1, "#437740" },
  { G_DESKTOP_BACKGROUND_SHADING_SOLID, -1, "#d272c4" },
  { G_DESKTOP_BACKGROUND_SHADING_SOLID, -1, "#ed9116" },
  { G_DESKTOP_BACKGROUND_SHADING_SOLID, -1, "#ff89a9" },
  { G_DESKTOP_BACKGROUND_SHADING_SOLID, -1, "#7a8aa2" },
  { G_DESKTOP_BACKGROUND_SHADING_SOLID, -1, "#888888" },
  { G_DESKTOP_BACKGROUND_SHADING_SOLID, -1, "#475b52" },
  { G_DESKTOP_BACKGROUND_SHADING_SOLID, -1, "#425265" },
  { G_DESKTOP_BACKGROUND_SHADING_SOLID, -1, "#7a634b" },
};

static void
bg_colors_source_constructed (GObject *object)
{
  BgColorsSource *self = BG_COLORS_SOURCE (object);
  GnomeDesktopThumbnailFactory *thumb_factory;
  guint i;
  GtkListStore *store;
  gint thumbnail_height;
  gint thumbnail_width;

  G_OBJECT_CLASS (bg_colors_source_parent_class)->constructed (object);

  store = bg_source_get_liststore (BG_SOURCE (self));

  thumb_factory = gnome_desktop_thumbnail_factory_new (GNOME_DESKTOP_THUMBNAIL_SIZE_LARGE);
  thumbnail_height = bg_source_get_thumbnail_height (BG_SOURCE (self));
  thumbnail_width = bg_source_get_thumbnail_width (BG_SOURCE (self));

  for (i = 0; i < G_N_ELEMENTS (items); i++)
    {
      CcBackgroundItemFlags flags;
      CcBackgroundItem *item;
      GIcon *pixbuf;

      item = cc_background_item_new (NULL);
      flags = CC_BACKGROUND_ITEM_HAS_PCOLOR |
	      CC_BACKGROUND_ITEM_HAS_SCOLOR |
	      CC_BACKGROUND_ITEM_HAS_SHADING |
	      CC_BACKGROUND_ITEM_HAS_PLACEMENT |
	      CC_BACKGROUND_ITEM_HAS_URI;
      /* It does have a URI, it's "none" */

      g_object_set (G_OBJECT (item),
                    "uri", "file:///" DATADIR "/gnome-control-center/pixmaps/noise-texture-light.png",
		    "primary-color", items[i].pcolor,
		    "secondary-color", items[i].pcolor,
		    "shading", items[i].type,
		    "placement", G_DESKTOP_BACKGROUND_STYLE_WALLPAPER,
		    "flags", flags,
		    NULL);
      cc_background_item_load (item, NULL);

      /* insert the item into the liststore */
      pixbuf = cc_background_item_get_thumbnail (item,
						 thumb_factory,
						 thumbnail_width, thumbnail_height);
      gtk_list_store_insert_with_values (store, NULL, 0,
                                         0, pixbuf,
                                         1, item,
                                         -1);

      g_object_unref (pixbuf);
      g_object_unref (item);
    }

  g_object_unref (thumb_factory);
}

bg_colors_source_init (BgColorsSource *self)
{
}

static void
bg_colors_source_class_init (BgColorsSourceClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = bg_colors_source_constructed;
}

BgColorsSource *
bg_colors_source_new (GtkWindow *window)
{
  return g_object_new (BG_TYPE_COLORS_SOURCE, "window", window, NULL);
}

