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
  CcVerticalRow      parent;

  GtkBox            *picture_box;
  CcMaskPaintable   *picture_mask;
  gchar             *resource_path;

  GtkMediaStream    *media_stream;
};

G_DEFINE_FINAL_TYPE (CcIllustratedRow, cc_illustrated_row, CC_TYPE_VERTICAL_ROW);

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
  if (self->media_stream != NULL)
    {
      gtk_media_stream_set_loop (self->media_stream, FALSE);
      gtk_media_stream_pause (self->media_stream);
    }
}

static void
on_picture_hover_cb (CcIllustratedRow *self)
{
  if (self->media_stream != NULL)
    {
      gtk_media_stream_set_loop (self->media_stream, TRUE);
      gtk_media_stream_play (self->media_stream);
    }
}

static void
update_mask_color (CcIllustratedRow *self)
{
  AdwStyleManager *style_manager = adw_style_manager_get_default ();
  AdwAccentColor color;
  GdkRGBA rgba;

  color = adw_style_manager_get_accent_color (style_manager);
  adw_accent_color_to_rgba (color, &rgba);

  cc_mask_paintable_set_rgba (self->picture_mask, &rgba);
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

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/common/cc-illustrated-row.ui");

  gtk_widget_class_bind_template_child (widget_class, CcIllustratedRow, picture_box);
  gtk_widget_class_bind_template_child (widget_class, CcIllustratedRow, picture_mask);

  gtk_widget_class_bind_template_callback (widget_class, on_picture_hover_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_picture_leave_cb);
}

static void
reload_illustration (CcIllustratedRow *self)
{
  g_autoptr (GdkPaintable) paintable = NULL;
  int scale;

  if (self->media_stream != NULL)
    return;

  scale = gtk_widget_get_scale_factor (GTK_WIDGET (self));
  paintable = cc_texture_new_from_resource_scaled (self->resource_path, scale);

  cc_mask_paintable_set_paintable (self->picture_mask, paintable);
}

static void
cc_illustrated_row_init (CcIllustratedRow *self)
{
  AdwStyleManager *style_manager = adw_style_manager_get_default ();

  gtk_widget_init_template (GTK_WIDGET (self));
  gtk_widget_set_name (GTK_WIDGET (self), "illustrated-row");
  gtk_widget_add_css_class (GTK_WIDGET (self), "illustrated-row");

  update_mask_color (self);

  g_signal_connect_object (style_manager, "notify::dark",
                           G_CALLBACK (update_mask_color), self,
                           G_CONNECT_SWAPPED);
  g_signal_connect_object (style_manager, "notify::accent-color",
                           G_CALLBACK (update_mask_color), self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_swapped (self, "map",
                            G_CALLBACK (reload_illustration), self);
  g_signal_connect_swapped (self, "notify::scale-factor",
                            G_CALLBACK (reload_illustration), self);
}

void
cc_illustrated_row_set_resource (CcIllustratedRow *self,
                                 const gchar      *resource_path)
{
  g_return_if_fail (CC_IS_ILLUSTRATED_ROW (self));

  if (self->media_stream != NULL)
    g_clear_object (&self->media_stream);

  g_set_str (&self->resource_path, resource_path);

  if (g_str_has_suffix (resource_path, ".svg"))
    reload_illustration (self);
  else
    {
      self->media_stream = gtk_media_file_new_for_resource (self->resource_path);
      cc_mask_paintable_set_paintable (self->picture_mask, GDK_PAINTABLE (self->media_stream));
    }

  gtk_widget_set_visible (GTK_WIDGET (self->picture_box),
                          self->resource_path != NULL &&
                          g_strcmp0 (self->resource_path, "") != 0);

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_RESOURCE]);
}
