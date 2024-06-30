/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* cc-mask-paintable.c
 *
 * Copyright (C) 2024 Alice Mikhaylenko <alicem@gnome.org>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "cc-mask-paintable"

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include "cc-mask-paintable.h"

struct _CcMaskPaintable
{
  GObject       parent_instance;

  GdkPaintable *paintable;
  GdkRGBA       rgba;
};

static void cc_mask_paintable_iface_init (GdkPaintableInterface *iface);

G_DEFINE_FINAL_TYPE_WITH_CODE (CcMaskPaintable, cc_mask_paintable, G_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (GDK_TYPE_PAINTABLE, cc_mask_paintable_iface_init))

enum
{
  PROP_0,
  PROP_PAINTABLE,
  PROP_RGBA,
  N_PROPS,
};

static GParamSpec *props[N_PROPS];

static void
cc_mask_paintable_dispose (GObject *object)
{
  CcMaskPaintable *self = CC_MASK_PAINTABLE (object);

  if (self->paintable)
    {
      g_signal_handlers_disconnect_by_func (self->paintable,
                                            gdk_paintable_invalidate_size,
                                            self);
      g_signal_handlers_disconnect_by_func (self->paintable,
                                            gdk_paintable_invalidate_contents,
                                            self);
    }

  g_clear_object (&self->paintable);

  G_OBJECT_CLASS (cc_mask_paintable_parent_class)->dispose (object);
}

static void
cc_mask_paintable_get_property (GObject    *object,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  CcMaskPaintable *self = CC_MASK_PAINTABLE (object);

  switch (prop_id)
    {
    case PROP_PAINTABLE:
      g_value_set_object (value, cc_mask_paintable_get_paintable (self));
      break;
    case PROP_RGBA:
      g_value_take_boxed (value, cc_mask_paintable_get_rgba (self));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
cc_mask_paintable_set_property (GObject      *object,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  CcMaskPaintable *self = CC_MASK_PAINTABLE (object);

  switch (prop_id)
    {
    case PROP_PAINTABLE:
      cc_mask_paintable_set_paintable (self, g_value_get_object (value));
      break;
    case PROP_RGBA:
      cc_mask_paintable_set_rgba (self, g_value_get_boxed (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
cc_mask_paintable_class_init (CcMaskPaintableClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = cc_mask_paintable_dispose;
  object_class->get_property = cc_mask_paintable_get_property;
  object_class->set_property = cc_mask_paintable_set_property;

  props[PROP_PAINTABLE] =
    g_param_spec_object ("paintable", NULL, NULL,
                         GDK_TYPE_PAINTABLE,
                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  props[PROP_RGBA] =
    g_param_spec_boxed ("rgba", NULL, NULL,
                        GDK_TYPE_RGBA,
                        G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, N_PROPS, props);
}

static void
cc_mask_paintable_init (CcMaskPaintable *self)
{
}

static void
cc_mask_paintable_snapshot (GdkPaintable *paintable,
                            GdkSnapshot  *snapshot,
                            double        width,
                            double        height)
{
  CcMaskPaintable *self = CC_MASK_PAINTABLE (paintable);
  GtkSnapshot *inner_snapshot;
  g_autoptr (GskRenderNode) node = NULL;

  if (!self->paintable)
    return;

  inner_snapshot = gtk_snapshot_new ();
  gdk_paintable_snapshot (self->paintable, inner_snapshot, width, height);
  node = gtk_snapshot_free_to_node (inner_snapshot);

  if (!node)
    return;

  gtk_snapshot_push_mask (snapshot, GSK_MASK_MODE_ALPHA);

  gtk_snapshot_append_node (snapshot, node);
  gtk_snapshot_pop (snapshot);

  gtk_snapshot_append_color (snapshot,
                             &self->rgba,
                             &GRAPHENE_RECT_INIT (0, 0, width, height));
  gtk_snapshot_pop (snapshot);
}

static GdkPaintable *
cc_mask_paintable_get_current_image (GdkPaintable *paintable)
{
  CcMaskPaintable *self = CC_MASK_PAINTABLE (paintable);
  g_autoptr (GdkPaintable) current = NULL;
  GdkPaintable *ret;

  if (!self->paintable)
    return gdk_paintable_new_empty (0, 0);

  current = gdk_paintable_get_current_image (self->paintable);

  ret = cc_mask_paintable_new ();
  cc_mask_paintable_set_paintable (CC_MASK_PAINTABLE (ret), current);
  cc_mask_paintable_set_rgba (CC_MASK_PAINTABLE (ret), &self->rgba);

  return ret;
}

static int
cc_mask_paintable_get_intrinsic_width (GdkPaintable *paintable)
{
  CcMaskPaintable *self = CC_MASK_PAINTABLE (paintable);

  if (self->paintable)
    return gdk_paintable_get_intrinsic_width (self->paintable);

  return 0;
}

static int
cc_mask_paintable_get_intrinsic_height (GdkPaintable *paintable)
{
  CcMaskPaintable *self = CC_MASK_PAINTABLE (paintable);

  if (self->paintable)
    return gdk_paintable_get_intrinsic_height (self->paintable);

  return 0;
}

static double
cc_mask_paintable_get_intrinsic_aspect_ratio (GdkPaintable *paintable)
{
  CcMaskPaintable *self = CC_MASK_PAINTABLE (paintable);

  if (self->paintable)
    return gdk_paintable_get_intrinsic_aspect_ratio (self->paintable);

  return 0;
}

static void
cc_mask_paintable_iface_init (GdkPaintableInterface *iface)
{
  iface->snapshot = cc_mask_paintable_snapshot;
  iface->get_current_image = cc_mask_paintable_get_current_image;
  iface->get_intrinsic_width = cc_mask_paintable_get_intrinsic_width;
  iface->get_intrinsic_height = cc_mask_paintable_get_intrinsic_height;
  iface->get_intrinsic_aspect_ratio = cc_mask_paintable_get_intrinsic_aspect_ratio;
}

GdkPaintable *
cc_mask_paintable_new (void)
{
  return g_object_new (CC_TYPE_MASK_PAINTABLE, NULL);
}

GdkPaintable *
cc_mask_paintable_get_paintable (CcMaskPaintable *self)
{
  g_return_val_if_fail (CC_IS_MASK_PAINTABLE (self), NULL);

  return self->paintable;
}

void
cc_mask_paintable_set_paintable (CcMaskPaintable *self,
                                 GdkPaintable    *paintable)
{
  g_return_if_fail (CC_IS_MASK_PAINTABLE (self));
  g_return_if_fail (GDK_IS_PAINTABLE (paintable));

  if (self->paintable)
    {
      g_signal_handlers_disconnect_by_func (self->paintable,
                                            gdk_paintable_invalidate_size,
                                            self);
      g_signal_handlers_disconnect_by_func (self->paintable,
                                            gdk_paintable_invalidate_contents,
                                            self);
    }

  g_set_object (&self->paintable, paintable);

  if (self->paintable)
    {
      g_signal_connect_swapped (self->paintable, "invalidate-size",
                                G_CALLBACK (gdk_paintable_invalidate_size), self);
      g_signal_connect_swapped (self->paintable, "invalidate-contents",
                                G_CALLBACK (gdk_paintable_invalidate_contents), self);
    }

  gdk_paintable_invalidate_size (GDK_PAINTABLE (self));
  gdk_paintable_invalidate_contents (GDK_PAINTABLE (self));

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_PAINTABLE]);
}

GdkRGBA *
cc_mask_paintable_get_rgba (CcMaskPaintable *self)
{
  g_return_val_if_fail (CC_IS_MASK_PAINTABLE (self), NULL);

  return gdk_rgba_copy (&self->rgba);
}

void
cc_mask_paintable_set_rgba (CcMaskPaintable *self,
                            GdkRGBA         *rgba)
{
  g_return_if_fail (CC_IS_MASK_PAINTABLE (self));
  g_return_if_fail (rgba != NULL);

  self->rgba = *rgba;

  gdk_paintable_invalidate_contents (GDK_PAINTABLE (self));

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_RGBA]);
}
