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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
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

static void
bg_colors_source_class_init (BgColorsSourceClass *klass)
{
}

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
  { G_DESKTOP_BACKGROUND_SHADING_SOLID, -1, "#000000" },
};

static void
bg_colors_source_init (BgColorsSource *self)
{
  GnomeDesktopThumbnailFactory *thumb_factory;
  guint i;
  GtkListStore *store;

  store = bg_source_get_liststore (BG_SOURCE (self));

  thumb_factory = gnome_desktop_thumbnail_factory_new (GNOME_DESKTOP_THUMBNAIL_SIZE_LARGE);

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
						 THUMBNAIL_WIDTH, THUMBNAIL_HEIGHT);
      gtk_list_store_insert_with_values (store, NULL, 0,
                                         0, pixbuf,
                                         1, item,
                                         -1);

      g_object_unref (pixbuf);
    }

  g_object_unref (thumb_factory);
}

BgColorsSource *
bg_colors_source_new (void)
{
  return g_object_new (BG_TYPE_COLORS_SOURCE, NULL);
}

