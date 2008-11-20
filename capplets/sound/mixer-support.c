/* mixer-support.c
 *
 * Copyright (C) 2007 Jan Arne Petersen <jap@gnome.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include <glib.h>
#include <glib/gi18n.h>
#include <gst/audio/mixerutils.h>

#include <gtk/gtk.h>

#include "mixer-support.h"

GtkTreeModel *
create_mixer_device_tree_model (void)
{
  GtkListStore *device_store;
  GList *mixer_list, *l;
  guint unknown = 0;

  device_store = gtk_list_store_new (MIXER_DEVICE_MODEL_COLUMN_COUNT,
      G_TYPE_STRING, G_TYPE_STRING, GST_TYPE_ELEMENT);

  mixer_list = gst_audio_default_registry_mixer_filter(NULL, FALSE, NULL);

  for (l = mixer_list; l != NULL; l = l->next) {
    GstElement *mixer = GST_ELEMENT (l->data);
    gchar *device_name = NULL, *device = NULL;
    GstElementFactory *factory;
    const gchar *longname, *factory_name;
    gchar *name;
    GtkTreeIter tree_iter;

    gst_element_set_state (mixer, GST_STATE_READY);

    /* fetch name */
    if (g_object_class_find_property (G_OBJECT_GET_CLASS (G_OBJECT (mixer)), "device-name")) {
      g_object_get (mixer, "device-name", &device_name, NULL);
    }
    if (g_object_class_find_property (G_OBJECT_GET_CLASS (G_OBJECT (mixer)), "device")) {
      g_object_get (mixer, "device", &device, NULL);
    }

    factory = gst_element_get_factory (mixer);
    longname = gst_element_factory_get_longname (factory);
    factory_name = gst_plugin_feature_get_name (GST_PLUGIN_FEATURE (factory));

    /* gst_element_set_state (mixer, GST_STATE_NULL); */

    if (device_name) {
      name = g_strdup_printf ("%s (%s)", device_name, longname);
      g_free (device_name);
    } else {
      gchar *title;

      unknown++;

      title = g_strdup_printf (_("Unknown Volume Control %d"), unknown);
      name = g_strdup_printf ("%s (%s)", title, longname);
      g_free (title);
    }

    if (device) {
      gchar *tmp;

      tmp = g_strdup_printf ("%s:%s", factory_name, device);
      g_free (device);
      device = tmp;
    } else {
      device = g_strdup (factory_name);
    }

    gtk_list_store_insert_with_values (device_store, &tree_iter, -1,
	MIXER_DEVICE_MODEL_NAME_COLUMN, name,
	MIXER_DEVICE_MODEL_DEVICE_COLUMN, device,
	MIXER_DEVICE_MODEL_MIXER_COLUMN, mixer,
	-1);

    gst_element_set_state (mixer, GST_STATE_NULL);
    gst_object_unref (GST_OBJECT (mixer));

    g_free (name);
    g_free (device);
  }

  g_list_free (mixer_list);

  gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (device_store),
					MIXER_DEVICE_MODEL_NAME_COLUMN,
					GTK_SORT_ASCENDING);

  return GTK_TREE_MODEL (device_store);
}

GtkTreeModel *
create_mixer_tracks_tree_model_for_mixer (GstMixer *mixer)
{
  GtkListStore *tracks_store;
  const GList *tracks, *l;

  tracks_store = gtk_list_store_new (MIXER_TRACKS_MODEL_COLUMN_COUNT,
      G_TYPE_STRING);

  tracks = gst_mixer_list_tracks (mixer);
  for (l = tracks; l != NULL; l = l->next) {
    GstMixerTrack *track = l->data;
    GtkTreeIter iter;

    if (track->num_channels <= 0) {
      continue;
    }

    gtk_list_store_insert_with_values (tracks_store, &iter, -1,
	MIXER_TRACKS_MODEL_LABEL_COLUMN, track->label,
	-1);
  }

  return GTK_TREE_MODEL (tracks_store);
}
