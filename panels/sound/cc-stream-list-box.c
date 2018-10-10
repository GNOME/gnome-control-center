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

#include <pulse/pulseaudio.h>
#include <gvc-mixer-sink.h>
#include <gvc-mixer-source.h>

#include "cc-stream-list-box.h"
#include "cc-stream-row.h"

struct _CcStreamListBox {
  GtkListBox       parent_instance;

  GtkSizeGroup    *label_size_group;
  GvcMixerControl *mixer_control;
  guint            stream_added_handler;
  guint            stream_removed_handler;
};

G_DEFINE_TYPE (CcStreamListBox, cc_stream_list_box, GTK_TYPE_LIST_BOX)

enum {
  PROP_0,
  PROP_LABEL_SIZE_GROUP
};

static void
stream_added_cb (CcStreamListBox *self,
                 guint            id)
{
  GvcMixerStream *stream;
  const gchar *app_id;
  CcStreamRow *row;

  stream = gvc_mixer_control_lookup_stream_id (self->mixer_control, id);
  if (stream == NULL)
    return;

  app_id = gvc_mixer_stream_get_application_id (stream);

  /* Skip mater volume controls */
  if (g_strcmp0 (app_id, "org.gnome.VolumeControl") == 0 ||
      g_strcmp0 (app_id, "org.PulseAudio.pavucontrol") == 0)
    return;

  /* Skip streams that aren't volume controls */
  if (GVC_IS_MIXER_SOURCE (stream) ||
      GVC_IS_MIXER_SINK (stream) ||
      gvc_mixer_stream_is_virtual (stream) ||
      gvc_mixer_stream_is_event_stream (stream))
    return;

  row = cc_stream_row_new (self->label_size_group, stream, id);
  gtk_widget_show (GTK_WIDGET (row));
  gtk_container_add (GTK_CONTAINER (self), GTK_WIDGET (row));
}

static CcStreamRow *
find_row (CcStreamListBox *self,
          guint            id)
{
  g_autoptr(GList) children = NULL;
  GList *link;

  children = gtk_container_get_children (GTK_CONTAINER (self));
  for (link = children; link; link = link->next) {
    CcStreamRow *row = link->data;

    if (!CC_IS_STREAM_ROW (row))
      continue;

    if (id == cc_stream_row_get_id (row))
      return row;
  }

  return NULL;
}

static void
stream_removed_cb (CcStreamListBox *self,
                   guint            id)
{
  CcStreamRow *row;

  row = find_row (self, id);
  if (row != NULL)
    gtk_container_remove (GTK_CONTAINER (self), GTK_WIDGET (row));
}

static void
cc_stream_list_box_set_property (GObject      *object,
                                 guint         property_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  CcStreamListBox *self = CC_STREAM_LIST_BOX (object);

  switch (property_id) {
  case PROP_LABEL_SIZE_GROUP:
    self->label_size_group = g_value_dup_object (value);
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }
}

static void
cc_stream_list_box_get_property (GObject    *object,
                                 guint       property_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  CcStreamListBox *self = CC_STREAM_LIST_BOX (object);

  switch (property_id) {
  case PROP_LABEL_SIZE_GROUP:
    g_value_set_object (value, self->label_size_group);
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }
}

static void
cc_stream_list_box_dispose (GObject *object)
{
  CcStreamListBox *self = CC_STREAM_LIST_BOX (object);

  g_clear_object (&self->mixer_control);

  G_OBJECT_CLASS (cc_stream_list_box_parent_class)->dispose (object);
}

void
cc_stream_list_box_class_init (CcStreamListBoxClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = cc_stream_list_box_set_property;
  object_class->get_property = cc_stream_list_box_get_property;
  object_class->dispose = cc_stream_list_box_dispose;

  g_object_class_install_property (object_class, PROP_LABEL_SIZE_GROUP,
                                   g_param_spec_object ("label-size-group",
                                                        NULL,
                                                        NULL,
                                                        GTK_TYPE_SIZE_GROUP,
                                                        G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
}

void
cc_stream_list_box_init (CcStreamListBox *self)
{
}

void
cc_stream_list_box_set_mixer_control (CcStreamListBox *self,
                                      GvcMixerControl *mixer_control)
{
  g_return_if_fail (CC_IS_STREAM_LIST_BOX (self));

  if (self->mixer_control != NULL) {
    g_signal_handler_disconnect (self->mixer_control, self->stream_added_handler);
    self->stream_added_handler = 0;
    g_signal_handler_disconnect (self->mixer_control, self->stream_removed_handler);
    self->stream_removed_handler = 0;
  }
  g_clear_object (&self->mixer_control);

  self->mixer_control = g_object_ref (mixer_control);

  self->stream_added_handler = g_signal_connect_object (self->mixer_control,
                                                        "stream-added",
                                                        G_CALLBACK (stream_added_cb),
                                                        self, G_CONNECT_SWAPPED);
  self->stream_removed_handler = g_signal_connect_object (self->mixer_control,
                                                          "stream-removed",
                                                          G_CALLBACK (stream_removed_cb),
                                                          self, G_CONNECT_SWAPPED);
}
