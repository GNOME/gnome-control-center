/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2021 Alexander Mikhaylenko <alexm@gnome.org>
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
 */

#include <config.h>

#include "cc-background-paintable.h"

struct _CcBackgroundPaintable
{
  GObject           parent_instance;

  BgSource         *source;
  CcBackgroundItem *item;
  int               scale_factor;
  GtkTextDirection  text_direction;

  GdkPaintable     *texture;
  GdkPaintable     *dark_texture;
};

enum
{
  PROP_0,
  PROP_SOURCE,
  PROP_ITEM,
  PROP_SCALE_FACTOR,
  PROP_TEXT_DIRECTION,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

static void cc_background_paintable_paintable_init (GdkPaintableInterface *iface);

G_DEFINE_TYPE_WITH_CODE (CcBackgroundPaintable, cc_background_paintable, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (GDK_TYPE_PAINTABLE,
                                                cc_background_paintable_paintable_init))

static void
update_cache (CcBackgroundPaintable *self)
{
  g_autoptr (GdkPixbuf) pixbuf = NULL;
  GnomeDesktopThumbnailFactory *factory;
  int width, height;

  g_clear_object (&self->texture);
  g_clear_object (&self->dark_texture);

  factory = bg_source_get_thumbnail_factory (self->source);
  width = bg_source_get_thumbnail_width (self->source);
  height = bg_source_get_thumbnail_height (self->source);

  pixbuf = cc_background_item_get_thumbnail (self->item,
                                             factory,
                                             width,
                                             height,
                                             self->scale_factor,
                                             FALSE);

  self->texture = GDK_PAINTABLE (gdk_texture_new_for_pixbuf (pixbuf));

  if (cc_background_item_has_dark_version (self->item))
    {
      g_autoptr (GdkPixbuf) dark_pixbuf = NULL;

      dark_pixbuf = cc_background_item_get_thumbnail (self->item,
                                                      factory,
                                                      width,
                                                      height,
                                                      self->scale_factor,
                                                      TRUE);
      self->dark_texture = GDK_PAINTABLE (gdk_texture_new_for_pixbuf (dark_pixbuf));
    }

  gdk_paintable_invalidate_size (GDK_PAINTABLE (self));
}

static void
cc_background_paintable_dispose (GObject *object)
{
  CcBackgroundPaintable *self = CC_BACKGROUND_PAINTABLE (object);

  g_clear_object (&self->item);
  g_clear_object (&self->source);
  g_clear_object (&self->texture);
  g_clear_object (&self->dark_texture);

  G_OBJECT_CLASS (cc_background_paintable_parent_class)->dispose (object);
}

static void
cc_background_paintable_constructed (GObject *object)
{
  CcBackgroundPaintable *self = CC_BACKGROUND_PAINTABLE (object);

  G_OBJECT_CLASS (cc_background_paintable_parent_class)->constructed (object);

  update_cache (self);
}

static void
cc_background_paintable_get_property (GObject    *object,
                                      guint       prop_id,
                                      GValue     *value,
                                      GParamSpec *pspec)
{
  CcBackgroundPaintable *self = CC_BACKGROUND_PAINTABLE (object);

  switch (prop_id)
    {
    case PROP_SOURCE:
      g_value_set_object (value, self->source);
      break;

    case PROP_ITEM:
      g_value_set_object (value, self->item);
      break;

    case PROP_SCALE_FACTOR:
      g_value_set_int (value, self->scale_factor);
      break;

    case PROP_TEXT_DIRECTION:
      g_value_set_enum (value, self->text_direction);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
cc_background_paintable_set_property (GObject      *object,
                                      guint         prop_id,
                                      const GValue *value,
                                      GParamSpec   *pspec)
{
  CcBackgroundPaintable *self = CC_BACKGROUND_PAINTABLE (object);

  switch (prop_id)
    {
    case PROP_SOURCE:
      g_set_object (&self->source, g_value_get_object (value));
      break;

    case PROP_ITEM:
      g_set_object (&self->item, g_value_get_object (value));
      break;

    case PROP_SCALE_FACTOR:
      self->scale_factor = g_value_get_int (value);
      update_cache (self);
      break;

    case PROP_TEXT_DIRECTION:
      self->text_direction = g_value_get_enum (value);
      gdk_paintable_invalidate_contents (GDK_PAINTABLE (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
cc_background_paintable_class_init (CcBackgroundPaintableClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = cc_background_paintable_dispose;
  object_class->constructed = cc_background_paintable_constructed;
  object_class->get_property = cc_background_paintable_get_property;
  object_class->set_property = cc_background_paintable_set_property;

  properties[PROP_SOURCE] =
    g_param_spec_object ("source",
                         "Source",
                         "Source",
                         BG_TYPE_SOURCE,
                         G_PARAM_READWRITE |
                         G_PARAM_CONSTRUCT_ONLY |
                         G_PARAM_STATIC_STRINGS);

  properties[PROP_ITEM] =
    g_param_spec_object ("item",
                         "Item",
                         "Item",
                         CC_TYPE_BACKGROUND_ITEM,
                         G_PARAM_READWRITE |
                         G_PARAM_CONSTRUCT_ONLY |
                         G_PARAM_STATIC_STRINGS);

  properties[PROP_SCALE_FACTOR] =
    g_param_spec_int ("scale-factor",
                      "Scale Factor",
                      "Scale Factor",
                      1, G_MAXINT, 1,
                      G_PARAM_READWRITE |
                      G_PARAM_STATIC_STRINGS);

  properties[PROP_TEXT_DIRECTION] =
    g_param_spec_enum ("text-direction",
                       "Text Direction",
                       "Text Direction",
                       GTK_TYPE_TEXT_DIRECTION,
                       GTK_TEXT_DIR_LTR,
                       G_PARAM_READWRITE |
                       G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
cc_background_paintable_init (CcBackgroundPaintable *self)
{
  self->scale_factor = 1;
  self->text_direction = GTK_TEXT_DIR_LTR;
}

static void
cc_background_paintable_snapshot (GdkPaintable *paintable,
                                  GdkSnapshot  *snapshot,
                                  double        width,
                                  double        height)
{
  CcBackgroundPaintable *self = CC_BACKGROUND_PAINTABLE (paintable);
  gboolean is_rtl;

  if (!self->dark_texture)
    {
      gdk_paintable_snapshot (self->texture, snapshot, width, height);
      return;
    }

  is_rtl = self->text_direction == GTK_TEXT_DIR_RTL;

  gtk_snapshot_push_clip (GTK_SNAPSHOT (snapshot),
                          &GRAPHENE_RECT_INIT (is_rtl ? width / 2.0f : 0.0f,
                                               0.0f,
                                               width / 2.0f,
                                               height));
  gdk_paintable_snapshot (self->texture, snapshot, width, height);
  gtk_snapshot_pop (GTK_SNAPSHOT (snapshot));

  gtk_snapshot_push_clip (GTK_SNAPSHOT (snapshot),
                          &GRAPHENE_RECT_INIT (is_rtl ? 0.0f : width / 2.0f,
                                               0.0f,
                                               width / 2.0f,
                                               height));
  gdk_paintable_snapshot (self->dark_texture, snapshot, width, height);
  gtk_snapshot_pop (GTK_SNAPSHOT (snapshot));
}

static int
cc_background_paintable_get_intrinsic_width (GdkPaintable *paintable)
{
  CcBackgroundPaintable *self = CC_BACKGROUND_PAINTABLE (paintable);

  return gdk_paintable_get_intrinsic_width (self->texture) / self->scale_factor;
}

static int
cc_background_paintable_get_intrinsic_height (GdkPaintable *paintable)
{
  CcBackgroundPaintable *self = CC_BACKGROUND_PAINTABLE (paintable);

  return gdk_paintable_get_intrinsic_height (self->texture) / self->scale_factor;
}

static double
cc_background_paintable_get_intrinsic_aspect_ratio (GdkPaintable *paintable)
{
  CcBackgroundPaintable *self = CC_BACKGROUND_PAINTABLE (paintable);

  return gdk_paintable_get_intrinsic_aspect_ratio (self->texture);
}

static void
cc_background_paintable_paintable_init (GdkPaintableInterface *iface)
{
  iface->snapshot = cc_background_paintable_snapshot;
  iface->get_intrinsic_width = cc_background_paintable_get_intrinsic_width;
  iface->get_intrinsic_height = cc_background_paintable_get_intrinsic_height;
  iface->get_intrinsic_aspect_ratio = cc_background_paintable_get_intrinsic_aspect_ratio;
}

CcBackgroundPaintable *
cc_background_paintable_new (BgSource         *source,
                             CcBackgroundItem *item)
{
  g_return_val_if_fail (BG_IS_SOURCE (source), NULL);
  g_return_val_if_fail (CC_IS_BACKGROUND_ITEM (item), NULL);

  return g_object_new (CC_TYPE_BACKGROUND_PAINTABLE,
                       "source", source,
                       "item", item,
                       NULL);
}
