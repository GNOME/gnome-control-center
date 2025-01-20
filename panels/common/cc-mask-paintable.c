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

#include <adwaita.h>

#include "cc-mask-paintable.h"
#include "cc-texture-utils.h"

struct _CcMaskPaintable
{
  GObject       parent_instance;

  GdkPaintable *paintable;
  GdkRGBA       rgba;
  GskMaskMode   mask_mode;

  gboolean      follow_accent;
  gboolean      updating_accent;

  gboolean      reloading_resource;
  char         *resource_path;
  GtkWidget    *parent_widget;
};

static void cc_mask_paintable_iface_init (GdkPaintableInterface *iface);

G_DEFINE_FINAL_TYPE_WITH_CODE (CcMaskPaintable, cc_mask_paintable, G_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (GDK_TYPE_PAINTABLE, cc_mask_paintable_iface_init))

enum
{
  PROP_0,
  PROP_PAINTABLE,
  PROP_RGBA,
  PROP_FOLLOW_ACCENT,
  N_PROPS,
};

static GParamSpec *props[N_PROPS];

static void
update_mask_color (CcMaskPaintable *self)
{
  AdwStyleManager *style_manager = adw_style_manager_get_default ();
  AdwAccentColor color;
  GdkRGBA rgba;

  color = adw_style_manager_get_accent_color (style_manager);
  adw_accent_color_to_rgba (color, &rgba);

  self->updating_accent = TRUE;
  cc_mask_paintable_set_rgba (self, &rgba);
  self->updating_accent = FALSE;
}

static void
reload_scalable_resource (CcMaskPaintable *self)
{
  g_autoptr (GdkPaintable) paintable = NULL;
  int scale;

  g_return_if_fail (self->parent_widget != NULL);

  scale = gtk_widget_get_scale_factor (GTK_WIDGET (self->parent_widget));
  paintable = cc_texture_new_from_resource_scaled (self->resource_path, scale);

  self->reloading_resource = TRUE;
  cc_mask_paintable_set_paintable (self, paintable);
  self->reloading_resource = FALSE;
}

static void
on_parent_widget_root_cb (CcMaskPaintable *self)
{
  g_return_if_fail (self->parent_widget != NULL);

  g_signal_handlers_disconnect_by_func (self->parent_widget, on_parent_widget_root_cb, self);

  g_signal_connect_swapped (self->parent_widget, "notify::scale-factor",
                            G_CALLBACK (reload_scalable_resource), self);

  reload_scalable_resource (self);
}

static void
clear_parent_widget (CcMaskPaintable *self)
{
  if (self->parent_widget)
    {
      g_signal_handlers_disconnect_by_func (self->parent_widget, on_parent_widget_root_cb, self);
      g_signal_handlers_disconnect_by_func (self->parent_widget, reload_scalable_resource, self);
    }

  g_clear_weak_pointer (&self->parent_widget);
}

static void
cc_mask_paintable_dispose (GObject *object)
{
  CcMaskPaintable *self = CC_MASK_PAINTABLE (object);

  clear_parent_widget (self);

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
cc_mask_paintable_finalize (GObject *object)
{
  CcMaskPaintable *self = CC_MASK_PAINTABLE (object);

  g_free (self->resource_path);

  G_OBJECT_CLASS (cc_mask_paintable_parent_class)->finalize (object);
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
    case PROP_FOLLOW_ACCENT:
      g_value_set_boolean (value, self->follow_accent);
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
    case PROP_FOLLOW_ACCENT:
      cc_mask_paintable_set_follow_accent (self, g_value_get_boolean (value));
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
  object_class->finalize = cc_mask_paintable_finalize;
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

  props[PROP_FOLLOW_ACCENT] =
    g_param_spec_boolean ("follow-accent", NULL, NULL,
                          FALSE,
                          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, N_PROPS, props);
}

static void
cc_mask_paintable_init (CcMaskPaintable *self)
{
  self->mask_mode = GSK_MASK_MODE_ALPHA;
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

  gtk_snapshot_push_mask (snapshot, self->mask_mode);

  gtk_snapshot_append_node (snapshot, node);
  gtk_snapshot_pop (snapshot);

  gtk_snapshot_append_color (snapshot,
                             &self->rgba,
                             &GRAPHENE_RECT_INIT (0, 0, width, height));
  gtk_snapshot_pop (snapshot);
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

  if (!self->reloading_resource)
    clear_parent_widget (self);

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

  if (self->follow_accent && !self->updating_accent)
    cc_mask_paintable_set_follow_accent (self, FALSE);

  self->rgba = *rgba;

  gdk_paintable_invalidate_contents (GDK_PAINTABLE (self));

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_RGBA]);
}

gboolean
cc_mask_paintable_get_follow_accent (CcMaskPaintable *self)
{
  g_return_val_if_fail (CC_IS_MASK_PAINTABLE (self), FALSE);

  return self->follow_accent;
}

void
cc_mask_paintable_set_follow_accent (CcMaskPaintable *self,
                                     gboolean         follow_accent)
{
  AdwStyleManager *style_manager;

  g_return_if_fail (CC_IS_MASK_PAINTABLE (self));

  follow_accent = !!follow_accent;

  if (self->follow_accent == follow_accent)
    return;
  self->follow_accent = follow_accent;

  style_manager = adw_style_manager_get_default ();

  if (self->follow_accent)
    {
      g_signal_connect_object (style_manager, "notify::accent-color",
                               G_CALLBACK (update_mask_color), self,
                               G_CONNECT_SWAPPED);
      update_mask_color (self);
    }
  else
    {
      g_signal_handlers_disconnect_by_func (style_manager, update_mask_color, self);
    }

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_FOLLOW_ACCENT]);
}

void
cc_mask_paintable_set_resource_scaled (CcMaskPaintable *self,
                                       const char      *resource_path,
                                       GtkWidget       *parent_widget)
{
  gboolean resource_is_webm, resource_is_scalable;

  g_return_if_fail (CC_IS_MASK_PAINTABLE (self));
  g_return_if_fail (resource_path != NULL);
  g_return_if_fail (GTK_IS_WIDGET (parent_widget));

  clear_parent_widget (self);

  g_set_str (&self->resource_path, resource_path);

  /* FIXME: As long as VP9 alpha decoding is in gstreamer-plugins-bad,
   * we should probably use B&W assets and luminance masking
   * https://gitlab.gnome.org/GNOME/gnome-control-center/-/issues/3173 
   * https://gitlab.freedesktop.org/gstreamer/gstreamer/-/issues/3978 */
  resource_is_webm = g_str_has_suffix (self->resource_path, ".webm");
  if (resource_is_webm)
    self->mask_mode = GSK_MASK_MODE_LUMINANCE;
  else
    self->mask_mode = GSK_MASK_MODE_ALPHA;

  resource_is_scalable = g_str_has_suffix (self->resource_path, ".svg");

  if (!resource_is_scalable)
    {
      g_autoptr (GtkMediaStream) media_stream = NULL;

      media_stream = gtk_media_file_new_for_resource (self->resource_path);
      cc_mask_paintable_set_paintable (self, GDK_PAINTABLE (media_stream));

      return;
    }

  self->parent_widget = parent_widget;
  g_object_add_weak_pointer (G_OBJECT (self->parent_widget), (gpointer *) &self->parent_widget);

  /* We will wait until the parent widget is rooted, as otherwise the scale-factor can't
     be retrieved reliably. Doing a resource update with the wrong scale-factor can be
     costly, so it's worth the wait */
  if (gtk_widget_get_root (self->parent_widget))
    on_parent_widget_root_cb (self);
  else
    g_signal_connect_swapped (self->parent_widget, "notify::root",
                              G_CALLBACK (on_parent_widget_root_cb), self);
}
