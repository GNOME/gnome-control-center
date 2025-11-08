/*
 * Copyright (C) 2018 Canonical Ltd.
 * Copyright (C) 2023 Marco Melorio
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
#include <gvc-mixer-source-output.h>

#include "cc-stream-row.h"
#include "cc-volume-levels-page.h"

struct _CcVolumeLevelsPage
{
  AdwNavigationPage parent_instance;

  AdwPreferencesGroup *input_group;
  AdwPreferencesGroup *output_group;
  GtkStack *stack;
  GtkSizeGroup *label_size_group;

  GvcMixerControl *mixer_control;
  GListStore *stream_list;
  GtkFilterListModel *input_model;
  GtkFilterListModel *output_model;
};

G_DEFINE_TYPE (CcVolumeLevelsPage, cc_volume_levels_page, ADW_TYPE_NAVIGATION_PAGE)

static gint
sort_stream (gconstpointer a,
             gconstpointer b,
             gpointer      user_data)
{
  CcVolumeLevelsPage *self = user_data;
  GvcMixerStream *stream_a, *stream_b, *event_sink;
  g_autofree gchar *name_a = NULL;
  g_autofree gchar *name_b = NULL;

  stream_a = GVC_MIXER_STREAM (a);
  stream_b = GVC_MIXER_STREAM (b);

  /* Put the system sound events control at the top */
  event_sink = gvc_mixer_control_get_event_sink_input (self->mixer_control);
  if (stream_a == event_sink)
    return -1;
  else if (stream_b == event_sink)
    return 1;

  name_a = g_utf8_casefold (gvc_mixer_stream_get_name (stream_a), -1);
  name_b = g_utf8_casefold (gvc_mixer_stream_get_name (stream_b), -1);

  return g_strcmp0 (name_a, name_b);
}

static gboolean
filter_stream (gpointer item,
               gpointer user_data)
{
  GvcMixerStream *stream = item;
  const gchar *app_id = gvc_mixer_stream_get_application_id (stream);

  /* Filter out master volume controls */
  if (g_strcmp0 (app_id, "org.gnome.VolumeControl") == 0 ||
      g_strcmp0 (app_id, "org.PulseAudio.pavucontrol") == 0)
    {
      return FALSE;
    }

  /* Filter out streams that aren't volume controls */
  if (GVC_IS_MIXER_SOURCE (stream) ||
      GVC_IS_MIXER_SINK (stream) ||
      gvc_mixer_stream_is_virtual (stream) ||
      gvc_mixer_stream_is_event_stream (stream))
    {
      return FALSE;
    }

  /* We can't present streams without a name in the UI. */
  if (gvc_mixer_stream_get_name (stream) == NULL)
    return FALSE;

  return TRUE;
}

static CcStreamType
get_stream_type (GvcMixerStream *stream)
{
  if (GVC_IS_MIXER_SOURCE_OUTPUT (stream))
    return CC_STREAM_TYPE_INPUT;
  else
    return CC_STREAM_TYPE_OUTPUT;
}

static gboolean
filter_stream_type (gpointer item,
                    gpointer user_data)
{
  GvcMixerStream *stream = item;
  CcStreamType stream_type = GPOINTER_TO_INT (user_data);

  return get_stream_type (stream) == stream_type;
}

static GtkWidget *
create_stream_row (gpointer item,
                   gpointer user_data)
{
  CcVolumeLevelsPage *self = user_data;
  GvcMixerStream *stream = item;
  guint id;
  CcStreamRow *row;

  id = gvc_mixer_stream_get_id (stream);
  row = cc_stream_row_new (self->label_size_group, stream, id, get_stream_type (stream), self->mixer_control);

  return GTK_WIDGET (row);
}

static void
items_changed_cb (CcVolumeLevelsPage *self)
{
  gboolean has_inputs = g_list_model_get_n_items (G_LIST_MODEL (self->input_model)) > 0;
  gboolean has_outputs = g_list_model_get_n_items (G_LIST_MODEL (self->output_model)) > 0;

  gtk_widget_set_visible (GTK_WIDGET (self->input_group), has_inputs);
  gtk_widget_set_visible (GTK_WIDGET (self->output_group), has_outputs);

  gtk_stack_set_visible_child_name (self->stack,
                                    has_inputs || has_outputs ? "streams-page"
                                                              : "no-streams-found-page");
}

static void
stream_added_cb (CcVolumeLevelsPage *self,
                 guint                 id)
{
  GvcMixerStream *stream = gvc_mixer_control_lookup_stream_id (self->mixer_control, id);

  if (stream == NULL)
    return;

  g_list_store_append (self->stream_list, G_OBJECT (stream));
}

static void
stream_removed_cb (CcVolumeLevelsPage *self,
                   guint                 id)
{
  guint n_items = g_list_model_get_n_items (G_LIST_MODEL (self->stream_list));

  for (guint i = 0; i < n_items; i++)
    {
      g_autoptr(GObject) item;
      guint stream_id;

      item = g_list_model_get_item (G_LIST_MODEL (self->stream_list), i);
      stream_id = gvc_mixer_stream_get_id (GVC_MIXER_STREAM (item));

      if (id == stream_id)
        {
          g_list_store_remove (self->stream_list, i);
          return;
        }
    }
}

static void
add_stream (gpointer data,
            gpointer user_data)
{
  CcVolumeLevelsPage *self = user_data;
  GvcMixerStream *stream = data;

  g_list_store_append (self->stream_list, G_OBJECT (stream));
}

static void
cc_volume_levels_page_dispose (GObject *object)
{
  CcVolumeLevelsPage *self = CC_VOLUME_LEVELS_PAGE (object);

  g_clear_object (&self->mixer_control);
  g_clear_object (&self->input_model);
  g_clear_object (&self->output_model);

  G_OBJECT_CLASS (cc_volume_levels_page_parent_class)->dispose (object);
}

void
cc_volume_levels_page_class_init (CcVolumeLevelsPageClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = cc_volume_levels_page_dispose;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/sound/cc-volume-levels-page.ui");

  gtk_widget_class_bind_template_child (widget_class, CcVolumeLevelsPage, label_size_group);
  gtk_widget_class_bind_template_child (widget_class, CcVolumeLevelsPage, stack);
  gtk_widget_class_bind_template_child (widget_class, CcVolumeLevelsPage, input_group);
  gtk_widget_class_bind_template_child (widget_class, CcVolumeLevelsPage, output_group);
}

void
cc_volume_levels_page_init (CcVolumeLevelsPage *self)
{
  GtkFilter *general_filter, *input_filter, *output_filter;
  GtkFilterListModel *filter_model;
  GtkSorter *sorter;
  g_autoptr(GtkSortListModel) sort_model = NULL;

  gtk_widget_init_template (GTK_WIDGET (self));

  self->stream_list = g_list_store_new (GVC_TYPE_MIXER_STREAM);

  general_filter = GTK_FILTER (gtk_custom_filter_new (filter_stream, NULL, NULL));
  filter_model = gtk_filter_list_model_new (G_LIST_MODEL (self->stream_list), general_filter);

  sorter = GTK_SORTER (gtk_custom_sorter_new (sort_stream, self, NULL));
  sort_model = gtk_sort_list_model_new (G_LIST_MODEL (filter_model), sorter);

  input_filter = GTK_FILTER (gtk_custom_filter_new (filter_stream_type,
                                                    GINT_TO_POINTER (CC_STREAM_TYPE_INPUT), NULL));
  output_filter = GTK_FILTER (gtk_custom_filter_new (filter_stream_type,
                                                     GINT_TO_POINTER (CC_STREAM_TYPE_OUTPUT), NULL));
  self->input_model = gtk_filter_list_model_new (G_LIST_MODEL (g_object_ref (sort_model)), input_filter);
  self->output_model = gtk_filter_list_model_new (G_LIST_MODEL (g_object_ref (sort_model)), output_filter);

  g_signal_connect_swapped (self->input_model,
                            "items-changed",
                            G_CALLBACK (items_changed_cb),
                            self);

  g_signal_connect_swapped (self->output_model,
                            "items-changed",
                            G_CALLBACK (items_changed_cb),
                            self);

  adw_preferences_group_bind_model (self->input_group,
                                    G_LIST_MODEL (self->input_model),
                                    create_stream_row,
                                    self, NULL);

  adw_preferences_group_bind_model (self->output_group,
                                    G_LIST_MODEL (self->output_model),
                                    create_stream_row,
                                    self, NULL);
}

CcVolumeLevelsPage *
cc_volume_levels_page_new (GvcMixerControl *mixer_control)
{
  CcVolumeLevelsPage *self;
  g_autoptr(GSList) streams = NULL;

  self = g_object_new (CC_TYPE_VOLUME_LEVELS_PAGE, NULL);

  self->mixer_control = g_object_ref (mixer_control);

  streams = gvc_mixer_control_get_streams (self->mixer_control);
  g_slist_foreach (streams, add_stream, self);

  g_signal_connect_object (self->mixer_control,
                           "stream-added",
                           G_CALLBACK (stream_added_cb),
                           self, G_CONNECT_SWAPPED);
  g_signal_connect_object (self->mixer_control,
                           "stream-removed",
                           G_CALLBACK (stream_removed_cb),
                           self, G_CONNECT_SWAPPED);

  return self;
}
