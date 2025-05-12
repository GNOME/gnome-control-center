/* cc-illustrated-row.c
 *
 * Copyright 2018 Purism SPC
 *           2021 Georges Basile Stavracas Neto <georges.stavracas@gmail.com>
 *           2023 Red Hat, Inc
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

#include "cc-illustrated-row.h"

#include "cc-mask-paintable.h"
#include "cc-texture-utils.h"

struct _CcIllustratedRow
{
  CcContentRow       parent;

  GtkBox            *picture_box;
  CcMaskPaintable   *picture_mask;
  gchar             *resource_path;
};

G_DEFINE_FINAL_TYPE (CcIllustratedRow, cc_illustrated_row, CC_TYPE_CONTENT_ROW);

enum
{
  PROP_0,
  PROP_RESOURCE,
  N_PROPS,
};

static GParamSpec *props[N_PROPS] = { NULL, };

static void
on_picture_leave_cb (CcIllustratedRow *self)
{
  GtkMediaStream *stream;
  GdkPaintable *paintable;

  paintable = cc_mask_paintable_get_paintable (self->picture_mask);

  if (!GTK_IS_MEDIA_STREAM (paintable))
    return;

  stream = GTK_MEDIA_STREAM (paintable);
  gtk_media_stream_set_loop (stream, FALSE);
  gtk_media_stream_pause (stream);
}

static void
on_picture_hover_cb (CcIllustratedRow *self)
{
  GtkMediaStream *stream;
  GdkPaintable *paintable;

  paintable = cc_mask_paintable_get_paintable (self->picture_mask);

  if (!GTK_IS_MEDIA_STREAM (paintable))
    return;

  stream = GTK_MEDIA_STREAM (paintable);
  gtk_media_stream_set_loop (stream, TRUE);
  gtk_media_stream_play (stream);
}

static void
cc_illustrated_row_get_property (GObject      *object,
                                 guint         prop_id,
                                 GValue       *value,
                                 GParamSpec   *pspec)
{
  CcIllustratedRow *self = CC_ILLUSTRATED_ROW (object);

  switch (prop_id)
    {
    case PROP_RESOURCE:
      g_value_set_string (value, self->resource_path);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
cc_illustrated_row_set_property (GObject      *object,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  CcIllustratedRow *self = CC_ILLUSTRATED_ROW (object);

  switch (prop_id)
    {
    case PROP_RESOURCE:
      cc_illustrated_row_set_resource (self, g_value_get_string (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
cc_illustrated_row_finalize (GObject *object)
{
  CcIllustratedRow *self = CC_ILLUSTRATED_ROW (object);

  g_clear_pointer (&self->resource_path, g_free);
  G_OBJECT_CLASS (cc_illustrated_row_parent_class)->finalize (object);
}

static void
cc_illustrated_row_class_init (CcIllustratedRowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->get_property = cc_illustrated_row_get_property;
  object_class->set_property = cc_illustrated_row_set_property;
  object_class->finalize = cc_illustrated_row_finalize;

  props[PROP_RESOURCE] =
    g_param_spec_string ("resource",
                         "Resource",
                         "Resource",
                         "",
                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, N_PROPS, props);

  g_type_ensure (CC_TYPE_MASK_PAINTABLE);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/common/cc-illustrated-row.ui");

  gtk_widget_class_bind_template_child (widget_class, CcIllustratedRow, picture_box);
  gtk_widget_class_bind_template_child (widget_class, CcIllustratedRow, picture_mask);

  gtk_widget_class_bind_template_callback (widget_class, on_picture_hover_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_picture_leave_cb);
}

static void
cc_illustrated_row_init (CcIllustratedRow *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
  gtk_widget_add_css_class (GTK_WIDGET (self), "illustrated");
}

void
cc_illustrated_row_set_resource (CcIllustratedRow *self,
                                 const gchar      *resource_path)
{
  g_return_if_fail (CC_IS_ILLUSTRATED_ROW (self));

  g_set_str (&self->resource_path, resource_path);

  cc_mask_paintable_set_resource_scaled (self->picture_mask, resource_path, GTK_WIDGET (self));

  gtk_widget_set_visible (GTK_WIDGET (self->picture_box),
                          self->resource_path != NULL &&
                          g_strcmp0 (self->resource_path, "") != 0);

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_RESOURCE]);
}
