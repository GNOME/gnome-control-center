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

#include "bg-source.h"
#include "cc-background-item.h"

#include <cairo-gobject.h>

#define THUMBNAIL_WIDTH 154
#define THUMBNAIL_HEIGHT (THUMBNAIL_WIDTH * 3 / 4)

typedef struct
{
  GnomeDesktopThumbnailFactory *thumbnail_factory;
  GListStore *store;
  GtkWidget *widget;
  gint thumbnail_height;
  gint thumbnail_width;
} BgSourcePrivate;

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (BgSource, bg_source, G_TYPE_OBJECT)

enum
{
  PROP_LISTSTORE = 1,
  PROP_WIDGET
};


static void
bg_source_calculate_thumbnail_dimensions (BgSource *source)
{
  BgSourcePrivate *priv = bg_source_get_instance_private (source);
  gint scale_factor;

  priv->thumbnail_height = THUMBNAIL_HEIGHT;
  priv->thumbnail_width = THUMBNAIL_WIDTH;

  if (priv->widget == NULL)
    return;

  scale_factor = gtk_widget_get_scale_factor (priv->widget);
  if (scale_factor > 1)
    {
      priv->thumbnail_height *= scale_factor;
      priv->thumbnail_width *= scale_factor;
    }
}

static void
bg_source_constructed (GObject *object)
{
  G_OBJECT_CLASS (bg_source_parent_class)->constructed (object);

  bg_source_calculate_thumbnail_dimensions (BG_SOURCE (object));
}

static void
bg_source_get_property (GObject    *object,
                        guint       property_id,
                        GValue     *value,
                        GParamSpec *pspec)
{
  BgSource *source = BG_SOURCE (object);

  switch (property_id)
    {
    case PROP_LISTSTORE:
      g_value_set_object (value, bg_source_get_liststore (source));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
bg_source_set_property (GObject      *object,
                        guint         property_id,
                        const GValue *value,
                        GParamSpec   *pspec)
{
  BgSource *source = BG_SOURCE (object);
  BgSourcePrivate *priv = bg_source_get_instance_private (source);

  switch (property_id)
    {
    case PROP_WIDGET:
      priv->widget = GTK_WIDGET (g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
bg_source_dispose (GObject *object)
{
  BgSource *source = BG_SOURCE (object);
  BgSourcePrivate *priv = bg_source_get_instance_private (source);

  g_clear_object (&priv->thumbnail_factory);
  g_clear_object (&priv->store);

  G_OBJECT_CLASS (bg_source_parent_class)->dispose (object);
}

static void
bg_source_class_init (BgSourceClass *klass)
{
  GParamSpec *pspec;
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = bg_source_constructed;
  object_class->get_property = bg_source_get_property;
  object_class->set_property = bg_source_set_property;
  object_class->dispose = bg_source_dispose;

  pspec = g_param_spec_object ("liststore",
                               "Liststore",
                               "Liststore used in the source",
                               G_TYPE_LIST_STORE,
                               G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_LISTSTORE, pspec);

  pspec = g_param_spec_object ("widget",
                               "Widget",
                               "Widget used to view the source",
                               GTK_TYPE_WIDGET,
                               G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_WIDGET, pspec);
}

static void
bg_source_init (BgSource *self)
{
  BgSourcePrivate *priv = bg_source_get_instance_private (self);
  priv->store = g_list_store_new (CC_TYPE_BACKGROUND_ITEM);
  priv->thumbnail_factory = gnome_desktop_thumbnail_factory_new (GNOME_DESKTOP_THUMBNAIL_SIZE_LARGE);
}

GListStore*
bg_source_get_liststore (BgSource *source)
{
  BgSourcePrivate *priv;

  g_return_val_if_fail (BG_IS_SOURCE (source), NULL);

  priv = bg_source_get_instance_private (source);
  return priv->store;
}

gint
bg_source_get_scale_factor (BgSource *source)
{
  BgSourcePrivate *priv;

  g_return_val_if_fail (BG_IS_SOURCE (source), 1);

  priv = bg_source_get_instance_private (source);
  return gtk_widget_get_scale_factor (priv->widget);
}

gint
bg_source_get_thumbnail_height (BgSource *source)
{
  BgSourcePrivate *priv;

  g_return_val_if_fail (BG_IS_SOURCE (source), THUMBNAIL_HEIGHT);

  priv = bg_source_get_instance_private (source);
  return priv->thumbnail_height;
}

gint
bg_source_get_thumbnail_width (BgSource *source)
{
  BgSourcePrivate *priv;

  g_return_val_if_fail (BG_IS_SOURCE (source), THUMBNAIL_WIDTH);

  priv = bg_source_get_instance_private (source);
  return priv->thumbnail_width;
}

GnomeDesktopThumbnailFactory*
bg_source_get_thumbnail_factory (BgSource *source)
{
  BgSourcePrivate *priv;

  g_return_val_if_fail (BG_IS_SOURCE (source), NULL);

  priv = bg_source_get_instance_private (source);
  return priv->thumbnail_factory;
}
