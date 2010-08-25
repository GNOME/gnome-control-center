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

#include "gnome-wp-item.h"

#include <glib/gi18n-lib.h>

G_DEFINE_TYPE (BgColorsSource, bg_colors_source, BG_TYPE_SOURCE)

#define COLORS_SOURCE_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), BG_TYPE_COLORS_SOURCE, BgColorsSourcePrivate))

static void
bg_colors_source_class_init (BgColorsSourceClass *klass)
{
}

static gchar *colors[] =
{
  "#c4a000",
  "#ce5c00",
  "#8f5902",
  "#4e9a06",
  "#204a87",
  "#5c3566",
  "#a40000",
  "#babdb6",
  "#2e3436",
  "#000000",
  NULL
};

static gchar *color_names[] =
{
  N_("Butter"),
  N_("Orange"),
  N_("Chocolate"),
  N_("Chameleon"),
  N_("Blue"),
  N_("Plum"),
  N_("Red"),
  N_("Aluminium"),
  N_("Gray"),
  N_("Black"),
  NULL
};

static void
bg_colors_source_init (BgColorsSource *self)
{
  GnomeDesktopThumbnailFactory *thumb_factory;
  gchar **c, **n;
  GtkListStore *store;

  store = bg_source_get_liststore (BG_SOURCE (self));

  thumb_factory = gnome_desktop_thumbnail_factory_new (GNOME_DESKTOP_THUMBNAIL_SIZE_NORMAL);

  for (c = colors, n = color_names; *c; c++, n++)
    {
      GnomeWPItem *item;
      GdkPixbuf *pixbuf;
      GdkColor color;

      item = g_new0 (GnomeWPItem, 1);

      item->filename = g_strdup ("(none)");
      item->name = g_strdup (_(*n));

      gdk_color_parse (*c, &color);
      item->pcolor = gdk_color_copy (&color);
      item->scolor = gdk_color_copy (&color);

      item->shade_type = GNOME_BG_COLOR_SOLID;

      gnome_wp_item_ensure_gnome_bg (item);

      /* insert the item into the liststore */
      pixbuf = gnome_wp_item_get_thumbnail (item,
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

