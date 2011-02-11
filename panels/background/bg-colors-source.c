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
#include <gsettings-desktop-schemas/gdesktop-enums.h>

G_DEFINE_TYPE (BgColorsSource, bg_colors_source, BG_TYPE_SOURCE)

#define COLORS_SOURCE_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), BG_TYPE_COLORS_SOURCE, BgColorsSourcePrivate))

static void
bg_colors_source_class_init (BgColorsSourceClass *klass)
{
}

struct {
	const char *name;
	GDesktopBackgroundShading type;
} items[] = {
	{ N_("Horizontal Gradient"), G_DESKTOP_BACKGROUND_SHADING_HORIZONTAL },
	{ N_("Vertical Gradient"), G_DESKTOP_BACKGROUND_SHADING_VERTICAL },
	{ N_("Solid Color"), G_DESKTOP_BACKGROUND_SHADING_SOLID },
};

#define PCOLOR "#023c88"
#define SCOLOR "#5789ca"

static void
bg_colors_source_init (BgColorsSource *self)
{
  GnomeDesktopThumbnailFactory *thumb_factory;
  guint i;
  GtkListStore *store;

  store = bg_source_get_liststore (BG_SOURCE (self));

  thumb_factory = gnome_desktop_thumbnail_factory_new (GNOME_DESKTOP_THUMBNAIL_SIZE_NORMAL);

  for (i = 0; i < G_N_ELEMENTS (items); i++)
    {
      CcBackgroundItemFlags flags;
      CcBackgroundItem *item;
      GIcon *pixbuf;

      item = cc_background_item_new (NULL);
      flags = CC_BACKGROUND_ITEM_HAS_PCOLOR |
	      CC_BACKGROUND_ITEM_HAS_SCOLOR |
	      CC_BACKGROUND_ITEM_HAS_SHADING |
	      CC_BACKGROUND_ITEM_HAS_FNAME;
      /* It does have a filename, it's "none" */

      g_object_set (G_OBJECT (item),
		    "name", _(items[i].name),
		    "primary-color", PCOLOR,
		    "secondary-color", SCOLOR,
		    "shading", items[i].type,
		    "flags", flags,
		    NULL);

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

