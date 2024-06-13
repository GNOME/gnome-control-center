/*
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

#include "cc-level-bar.h"
#include "cc-sound-resources.h"
#include "cc-stream-row.h"
#include "cc-volume-slider.h"
#include "cc-sound-enums.h"

#define SPEECH_DISPATCHER_PREFIX "speech-dispatcher-"

struct _CcStreamRow
{
  GtkListBoxRow   parent_instance;

  GtkBox         *label_box;
  GtkLabel       *name_label;
  GtkImage       *icon_image;
  CcVolumeSlider *volume_slider;
  CcLevelBar     *level_bar;

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
  gtk_widget_class_bind_template_child (widget_class, CcStreamRow, level_bar);
}

void
cc_stream_row_init (CcStreamRow *self)
{
  g_resources_register (cc_sound_get_resource ());

  gtk_widget_init_template (GTK_WIDGET (self));
}

static GIcon *
get_app_info_icon_from_stream_name (const gchar *stream_name)
{
  g_autolist(GObject) infos = NULL;
  GList *l;

  infos = g_app_info_get_all ();
  for (l = infos; l; l = l->next)
    {
      GAppInfo *info = l->data;

      if (g_str_equal (g_app_info_get_display_name (info), stream_name) ||
          g_str_equal (g_app_info_get_name (info), stream_name))
        {
          GIcon *icon = g_app_info_get_icon (info);
          if (icon)
            g_object_ref (icon);

          return icon;
        }
    }

  return NULL;
}

static GIcon *
get_icon_for_stream (GvcMixerStream *stream)
{
  GIcon* icon;
  const gchar *icon_name;
  const gchar *stream_name;

  icon_name = gvc_mixer_stream_get_icon_name (stream);
  stream_name = gvc_mixer_stream_get_name (stream);

  if (g_str_has_prefix (stream_name, SPEECH_DISPATCHER_PREFIX))
    return g_themed_icon_new_with_default_fallbacks ("org.gnome.Settings-accessibility");

  /* First prefer app info icon, then the icon name, as the icon name is usually very generic */
  if ((icon = get_app_info_icon_from_stream_name (stream_name)))
    return icon;

  if (gtk_icon_theme_has_icon (gtk_icon_theme_get_for_display (gdk_display_get_default ()), icon_name))
    return g_themed_icon_new_with_default_fallbacks (icon_name);

  return g_themed_icon_new_with_default_fallbacks ("application-x-executable");
}

CcStreamRow *
cc_stream_row_new (GtkSizeGroup    *size_group,
                   GvcMixerStream  *stream,
                   guint            id,
                   CcStreamType     stream_type,
                   GvcMixerControl *mixer_control)
{
  CcStreamRow *self;
  g_autoptr(GIcon) gicon = NULL;

  self = g_object_new (CC_TYPE_STREAM_ROW, NULL);
  self->stream = g_object_ref (stream);
  self->id = id;

  gicon = get_icon_for_stream (stream);

  gtk_image_set_from_gicon (self->icon_image, gicon);

  gtk_label_set_label (self->name_label, gvc_mixer_stream_get_name (stream));
  cc_volume_slider_set_stream (self->volume_slider, stream, stream_type);
  cc_volume_slider_set_mixer_control (self->volume_slider, mixer_control);
  cc_level_bar_set_stream (self->level_bar, stream);

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
