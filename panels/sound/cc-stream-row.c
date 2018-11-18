/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2018 Canonical Ltd.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include "cc-sound-resources.h"
#include "cc-stream-row.h"
#include "cc-volume-slider.h"

struct _CcStreamRow
{
  GtkListBoxRow   parent_instance;

  GtkBox         *label_box;
  GtkLabel       *name_label;
  GtkImage       *icon_image;
  CcVolumeSlider *volume_slider;

  GvcMixerStream *stream;
  guint           id;
};

G_DEFINE_TYPE (CcStreamRow, cc_stream_row, GTK_TYPE_LIST_BOX_ROW)

static void
cc_stream_row_dispose (GObject *object)
{
  CcStreamRow *self = CC_STREAM_ROW (object);

  g_clear_object (&self->stream);

  G_OBJECT_CLASS (cc_stream_row_parent_class)->dispose (object);
}

void
cc_stream_row_class_init (CcStreamRowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = cc_stream_row_dispose;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/sound/cc-stream-row.ui");

  gtk_widget_class_bind_template_child (widget_class, CcStreamRow, label_box);
  gtk_widget_class_bind_template_child (widget_class, CcStreamRow, icon_image);
  gtk_widget_class_bind_template_child (widget_class, CcStreamRow, name_label);
  gtk_widget_class_bind_template_child (widget_class, CcStreamRow, volume_slider);
}

void
cc_stream_row_init (CcStreamRow *self)
{
  g_resources_register (cc_sound_get_resource ());

  gtk_widget_init_template (GTK_WIDGET (self));
}

CcStreamRow *
cc_stream_row_new (GtkSizeGroup   *size_group,
                   GvcMixerStream *stream,
                   guint           id)
{
  CcStreamRow *self;

  self = g_object_new (CC_TYPE_STREAM_ROW, NULL);
  self->stream = g_object_ref (stream);
  self->id = id;

  gtk_image_set_from_gicon (self->icon_image, gvc_mixer_stream_get_gicon (stream), GTK_ICON_SIZE_LARGE_TOOLBAR);

  gtk_label_set_label (self->name_label, gvc_mixer_stream_get_name (stream));
  cc_volume_slider_set_stream (self->volume_slider, stream);

  gtk_size_group_add_widget (size_group, GTK_WIDGET (self->label_box));

  return self;
}

GvcMixerStream *
cc_stream_row_get_stream (CcStreamRow *self)
{
  g_return_val_if_fail (CC_IS_STREAM_ROW (self), NULL);
  return self->stream;
}

guint
cc_stream_row_get_id (CcStreamRow *self)
{
  g_return_val_if_fail (CC_IS_STREAM_ROW (self), 0);
  return self->id;
}
