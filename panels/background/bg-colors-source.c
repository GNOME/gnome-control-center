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

G_DEFINE_TYPE (BgColorsSource, bg_colors_source, G_TYPE_OBJECT)

#define COLORS_SOURCE_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), BG_TYPE_COLORS_SOURCE, BgColorsSourcePrivate))

struct _BgColorsSourcePrivate
{
  GtkListStore *store;
};


static void
bg_colors_source_dispose (GObject *object)
{
  BgColorsSourcePrivate *priv = BG_COLORS_SOURCE (object)->priv;

  if (priv->store)
    {
      g_object_unref (priv->store);
      priv->store = NULL;
    }

  G_OBJECT_CLASS (bg_colors_source_parent_class)->dispose (object);
}

static void
bg_colors_source_finalize (GObject *object)
{
  G_OBJECT_CLASS (bg_colors_source_parent_class)->finalize (object);
}

static void
bg_colors_source_class_init (BgColorsSourceClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  g_type_class_add_private (klass, sizeof (BgColorsSourcePrivate));

  object_class->dispose = bg_colors_source_dispose;
  object_class->finalize = bg_colors_source_finalize;
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
  BgColorsSourcePrivate *priv;
  gchar **c, **n;


  priv = self->priv = COLORS_SOURCE_PRIVATE (self);

  priv->store = gtk_list_store_new (2, GDK_TYPE_PIXBUF, G_TYPE_POINTER);

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
                                            100, 75);
      gtk_list_store_insert_with_values (priv->store, NULL, 0,
                                         0, pixbuf,
                                         1, item,
                                         -1);
    }

  g_object_unref (thumb_factory);
}

BgColorsSource *
bg_colors_source_new (void)
{
  return g_object_new (BG_TYPE_COLORS_SOURCE, NULL);
}

GtkListStore *
bg_colors_source_get_liststore (BgColorsSource *source)
{
  return source->priv->store;
}
