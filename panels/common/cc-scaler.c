/*
 * Copyright © 2018 Benjamin Otte
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Benjamin Otte <otte@gnome.org>
 */

/* Copied from gtkscaler.c, renamed to CcScaler
 * https://gitlab.gnome.org/GNOME/gtk/-/blob/90c9e88ee91fb8a61563f14df0b59588f9068ee9/gtk/gtkscaler.c */

#include "config.h"

#include "cc-scaler.h"

struct _CcScaler
{
  GObject parent_instance;

  GdkPaintable *paintable;
  double scale;
};

struct _CcScalerClass
{
  GObjectClass parent_class;
};

static void
cc_scaler_paintable_snapshot (GdkPaintable *paintable,
                              GdkSnapshot  *snapshot,
                              double        width,
                              double        height)
{
  CcScaler *self = CC_SCALER (paintable);

  gtk_snapshot_save (snapshot);

  gtk_snapshot_scale (snapshot, 1.0 / self->scale, 1.0 / self->scale);

  gdk_paintable_snapshot (self->paintable,
                          snapshot,
                          width * self->scale,
                          height * self->scale);

  gtk_snapshot_restore (snapshot);
}

static GdkPaintable *
cc_scaler_paintable_get_current_image (GdkPaintable *paintable)
{
  CcScaler *self = CC_SCALER (paintable);
  GdkPaintable *current_paintable, *current_self;

  current_paintable = gdk_paintable_get_current_image (self->paintable);
  current_self = cc_scaler_new (current_paintable, self->scale);
  g_object_unref (current_paintable);

  return current_self;
}

static GdkPaintableFlags
cc_scaler_paintable_get_flags (GdkPaintable *paintable)
{
  CcScaler *self = CC_SCALER (paintable);

  return gdk_paintable_get_flags (self->paintable);
}

static int
cc_scaler_paintable_get_intrinsic_width (GdkPaintable *paintable)
{
  CcScaler *self = CC_SCALER (paintable);

  return gdk_paintable_get_intrinsic_width (self->paintable) / self->scale;
}

static int
cc_scaler_paintable_get_intrinsic_height (GdkPaintable *paintable)
{
  CcScaler *self = CC_SCALER (paintable);

  return gdk_paintable_get_intrinsic_height (self->paintable) / self->scale;
}

static double cc_scaler_paintable_get_intrinsic_aspect_ratio (GdkPaintable *paintable)
{
  CcScaler *self = CC_SCALER (paintable);

  return gdk_paintable_get_intrinsic_aspect_ratio (self->paintable);
};

static void
cc_scaler_paintable_init (GdkPaintableInterface *iface)
{
  iface->snapshot = cc_scaler_paintable_snapshot;
  iface->get_current_image = cc_scaler_paintable_get_current_image;
  iface->get_flags = cc_scaler_paintable_get_flags;
  iface->get_intrinsic_width = cc_scaler_paintable_get_intrinsic_width;
  iface->get_intrinsic_height = cc_scaler_paintable_get_intrinsic_height;
  iface->get_intrinsic_aspect_ratio = cc_scaler_paintable_get_intrinsic_aspect_ratio;
}

G_DEFINE_TYPE_EXTENDED (CcScaler, cc_scaler, G_TYPE_OBJECT, 0,
                        G_IMPLEMENT_INTERFACE (GDK_TYPE_PAINTABLE,
                                               cc_scaler_paintable_init))

static void
cc_scaler_dispose (GObject *object)
{
  CcScaler *self = CC_SCALER (object);

  if (self->paintable)
    {
      const guint flags = gdk_paintable_get_flags (self->paintable);

      if ((flags & GDK_PAINTABLE_STATIC_CONTENTS) == 0)
        g_signal_handlers_disconnect_by_func (self->paintable, gdk_paintable_invalidate_contents, self);

      if ((flags & GDK_PAINTABLE_STATIC_SIZE) == 0)
        g_signal_handlers_disconnect_by_func (self->paintable, gdk_paintable_invalidate_size, self);

      g_clear_object (&self->paintable);
    }

  G_OBJECT_CLASS (cc_scaler_parent_class)->dispose (object);
}

static void
cc_scaler_class_init (CcScalerClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->dispose = cc_scaler_dispose;
}

static void
cc_scaler_init (CcScaler *self)
{
  self->scale = 1.0;
}

GdkPaintable *
cc_scaler_new (GdkPaintable *paintable,
               double        scale)
{
  CcScaler *self;
  guint flags;

  g_return_val_if_fail (GDK_IS_PAINTABLE (paintable), NULL);
  g_return_val_if_fail (scale > 0.0, NULL);

  self = g_object_new (CC_TYPE_SCALER, NULL);

  self->paintable = g_object_ref (paintable);
  flags = gdk_paintable_get_flags (paintable);

  if ((flags & GDK_PAINTABLE_STATIC_CONTENTS) == 0)
    g_signal_connect_swapped (paintable, "invalidate-contents", G_CALLBACK (gdk_paintable_invalidate_contents), self);

  if ((flags & GDK_PAINTABLE_STATIC_SIZE) == 0)
    g_signal_connect_swapped (paintable, "invalidate-size", G_CALLBACK (gdk_paintable_invalidate_size), self);

  self->scale = scale;

  return GDK_PAINTABLE (self);
}
