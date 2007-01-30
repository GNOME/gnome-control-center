/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/* acme-volume-gstreamer.c

   Copyright (C) 2002, 2003 Bastien Nocera
   Copyright (C) 2004 Novell, Inc.

   The Gnome Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The Gnome Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with the Gnome Library; see the file COPYING.LIB.  If not,
   write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.

   Author: Bastien Nocera <hadess@hadess.net>
           Jon Trowbridge <trow@ximian.com>
 */

#include "config.h"
#include "acme-volume-gstreamer.h"

#include <gst/gst.h>
#include <gst/audio/mixerutils.h>
#include <gst/interfaces/mixer.h>
#include <gst/interfaces/propertyprobe.h>

#include <gconf/gconf-client.h>

#include <string.h>

#define TIMEOUT	4000

#define DEFAULT_MIXER_DEVICE_KEY   "/desktop/gnome/sound/default_mixer_device"
#define DEFAULT_MIXER_TRACKS_KEY   "/desktop/gnome/sound/default_mixer_tracks"
 
struct AcmeVolumeGStreamerPrivate
{
  	GstMixer      *mixer;
	GList         *mixer_tracks;
 	guint timer_id;
	gdouble      volume;
	gboolean     mute;
	GConfClient *gconf_client;
};

static GObjectClass *parent_class = NULL;

G_DEFINE_TYPE (AcmeVolumeGStreamer, acme_volume_gstreamer, ACME_TYPE_VOLUME)

static int acme_volume_gstreamer_get_volume (AcmeVolume *self);
static void acme_volume_gstreamer_set_volume (AcmeVolume *self, int val);
static gboolean acme_volume_gstreamer_open (AcmeVolumeGStreamer *self);
static void acme_volume_gstreamer_close (AcmeVolumeGStreamer *self);
static gboolean acme_volume_gstreamer_close_real (AcmeVolumeGStreamer *self);

static void
acme_volume_gstreamer_finalize (GObject *object)
{
	AcmeVolumeGStreamer *self;

	g_return_if_fail (object != NULL);
	g_return_if_fail (ACME_IS_VOLUME_GSTREAMER (object));

	self = ACME_VOLUME_GSTREAMER (object);

	g_return_if_fail (self->_priv != NULL);
	
	if (self->_priv->timer_id != 0)
	{
		g_source_remove (self->_priv->timer_id);
		self->_priv->timer_id = 0;
	}
	acme_volume_gstreamer_close_real (self);

	if (self->_priv->gconf_client != NULL) {
		g_object_unref (G_OBJECT (self->_priv->gconf_client));
		self->_priv->gconf_client = NULL;
	}

	g_free (self->_priv);

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
acme_volume_gstreamer_set_mute (AcmeVolume *vol, gboolean val)
{
	AcmeVolumeGStreamer *self = (AcmeVolumeGStreamer *) vol;
	GList *t;
	
	if (acme_volume_gstreamer_open (self) == FALSE)
		return;

	for (t = self->_priv->mixer_tracks; t != NULL; t = t->next)
	{
		GstMixerTrack *track = GST_MIXER_TRACK (t->data);
		gst_mixer_set_mute (self->_priv->mixer, track, val);
	}

	if (val)
	{
		self->_priv->mute = TRUE;
	} else {
		self->_priv->mute = FALSE;

		for (t = self->_priv->mixer_tracks; t != NULL; t = t->next)
		{
			GstMixerTrack *track = GST_MIXER_TRACK (t->data);
			gint *volumes, n;
			gdouble scale = (track->max_volume - track->min_volume) / 100.0;
			gint vol = (gint) self->_priv->volume * scale + track->min_volume;

			volumes = g_new0 (gint, track->num_channels);
			for (n = 0; n < track->num_channels; n++)
				volumes[n] = vol;
			gst_mixer_set_volume (self->_priv->mixer, track, volumes);
			g_free (volumes);
		}
	}

	acme_volume_gstreamer_close (self);
}

static void
update_state (AcmeVolumeGStreamer * self)
{
	gint *volumes, n;
	gdouble vol = 0;
	GstMixerTrack *track = GST_MIXER_TRACK(self->_priv->mixer_tracks->data);

	/* update mixer by getting volume */
	volumes = g_new0 (gint, track->num_channels);
	gst_mixer_get_volume (self->_priv->mixer, track, volumes);
	for (n = 0; n < track->num_channels; n++)
		vol += volumes[n];
	g_free (volumes);
	vol /= track->num_channels;
	vol = 100 * vol / (track->max_volume - track->min_volume);

	/* update mute flag, and volume if not muted */
	if (GST_MIXER_TRACK_HAS_FLAG (track, GST_MIXER_TRACK_MUTE) ||
	    (vol == 0 && self->_priv->volume != 0))
		self->_priv->mute = TRUE;
	else
		self->_priv->volume = vol;
}

static gboolean
acme_volume_gstreamer_get_mute (AcmeVolume *vol)
{
	AcmeVolumeGStreamer *self = (AcmeVolumeGStreamer *) vol;

	if (acme_volume_gstreamer_open (self) == FALSE)
		return FALSE;

	update_state (self);
	acme_volume_gstreamer_close (self);

	return self->_priv->mute;
}

static int
acme_volume_gstreamer_get_volume (AcmeVolume *vol)
{
	AcmeVolumeGStreamer *self = (AcmeVolumeGStreamer *) vol;
	
	if (acme_volume_gstreamer_open (self) == FALSE)
		return 0;

	update_state (self);

	acme_volume_gstreamer_close (self);

	return (gint) self->_priv->volume;
}

static void
acme_volume_gstreamer_set_volume (AcmeVolume *vol, int val)
{
	AcmeVolumeGStreamer *self = (AcmeVolumeGStreamer *) vol;
	GList *t;

	if (acme_volume_gstreamer_open (self) == FALSE)
		return;

	val = CLAMP (val, 0, 100);

	for (t = self->_priv->mixer_tracks; t != NULL; t = t->next)
	{
		GstMixerTrack *track = GST_MIXER_TRACK (t->data);
		gint *volumes, n;
		gdouble scale = (track->max_volume - track->min_volume) / 100.0;
		gint vol = (gint) val * scale + track->min_volume;

		volumes = g_new0 (gint, track->num_channels);
		for (n = 0; n < track->num_channels; n++)
			volumes[n] = vol;
		gst_mixer_set_volume (self->_priv->mixer, track, volumes);
		g_free (volumes);
	}
 	
	/* update state */
	self->_priv->volume = val;

 	acme_volume_gstreamer_close (self);
}
 
static gboolean
acme_volume_gstreamer_close_real (AcmeVolumeGStreamer *self)
{
	if (self->_priv == NULL)
		return FALSE;
	
	if (self->_priv->mixer != NULL)
	{
		gst_element_set_state (GST_ELEMENT(self->_priv->mixer), GST_STATE_NULL);
		gst_object_unref (GST_OBJECT (self->_priv->mixer));
		g_list_foreach (self->_priv->mixer_tracks, (GFunc)g_object_unref, NULL);
		g_list_free (self->_priv->mixer_tracks);
		self->_priv->mixer=NULL;
		self->_priv->mixer_tracks=NULL;
	}
	
	self->_priv->timer_id = 0;
	return FALSE;
}

/*
 * _acme_set_mixer
 * Arguments: mixer - pointer to mixer element
 *            data - pointer to user data (AcmeVolumeGStreamer to be modified)
 * Returns: gboolean indicating success
 */
static gboolean
_acme_set_mixer(GstMixer *mixer, gpointer user_data)
{
   const GList *tracks;

   tracks = gst_mixer_list_tracks (mixer);

   while (tracks != NULL) {
      GstMixerTrack *track = GST_MIXER_TRACK (tracks->data);

      if (GST_MIXER_TRACK_HAS_FLAG (track, GST_MIXER_TRACK_MASTER)) {
         AcmeVolumeGStreamer *self;

         self = ACME_VOLUME_GSTREAMER (user_data);

         self->_priv->mixer = mixer;
         self->_priv->mixer_tracks = g_list_append (self->_priv->mixer_tracks, g_object_ref (track));
         return TRUE;
      }

      tracks = tracks->next;
   }

   return FALSE;
}

/* This is a modified version of code from gnome-media's gst-mixer */
static gboolean
acme_volume_gstreamer_open (AcmeVolumeGStreamer *vol)
{
  	AcmeVolumeGStreamer *self = (AcmeVolumeGStreamer *) vol;
	gchar *mixer_device, **factory_and_device = NULL;
	GList *mixer_list;

	if (self->_priv == NULL)
		return FALSE;
	
	if (self->_priv->timer_id != 0)
	{
		g_source_remove (self->_priv->timer_id);
		self->_priv->timer_id = 0;
		return TRUE;
	}

	mixer_device = gconf_client_get_string (self->_priv->gconf_client, DEFAULT_MIXER_DEVICE_KEY, NULL);
	if (mixer_device != NULL)
	{
		factory_and_device = g_strsplit (mixer_device, ":", 2);
	}

	if (factory_and_device != NULL && factory_and_device[0] != NULL)
	{
		GstElement *element;

		element = gst_element_factory_make (factory_and_device[0], NULL);

		if (element != NULL) {
			gst_element_set_state (element, GST_STATE_READY);

			if (!GST_IS_MIXER (element))
			{
				gst_element_set_state (element, GST_STATE_NULL);
				gst_object_unref (element);
			} else {
				self->_priv->mixer = GST_MIXER (element);
			
				if (factory_and_device[1] != NULL && 
						g_object_class_find_property (G_OBJECT_GET_CLASS (self->_priv->mixer), "device"))
				{
					g_object_set (G_OBJECT (self->_priv->mixer), "device", &factory_and_device[1], NULL);
				}
			}
		}
	}

	g_free (mixer_device);
	g_strfreev (factory_and_device);

	if (self->_priv->mixer != NULL)
	{
		const GList *m;
		GSList *tracks, *t;

		/* Try to use tracks saved in GConf */
		tracks = gconf_client_get_list (self->_priv->gconf_client, DEFAULT_MIXER_TRACKS_KEY, GCONF_VALUE_STRING, NULL);

		for (m = gst_mixer_list_tracks (self->_priv->mixer); m != NULL; m = m->next)
		{
			GstMixerTrack *track = GST_MIXER_TRACK (m->data);

			for (t = tracks; t != NULL; t = t->next)
			{
				if (!strcmp (t->data, track->label))
				{
					self->_priv->mixer_tracks = g_list_append (self->_priv->mixer_tracks, g_object_ref (track));
				}
			}
			
		}

		g_slist_foreach (tracks, (GFunc)g_free, NULL);
		g_slist_free (tracks);

		/* If no track stored in GConf is avaiable try to use master track */
		if (self->_priv->mixer_tracks == NULL)
		{
			for (m = gst_mixer_list_tracks (self->_priv->mixer); m != NULL; m = m->next)
			{
				GstMixerTrack *track = GST_MIXER_TRACK (m->data);

				if (GST_MIXER_TRACK_HAS_FLAG (track, GST_MIXER_TRACK_MASTER)) {
					self->_priv->mixer_tracks = g_list_append (self->_priv->mixer_tracks, track);
					break;
				}
			}
		}
	}

	if (self->_priv->mixer != NULL)
	{
		if (self->_priv->mixer_tracks != NULL)
		{
			return TRUE;
		} else {
			gst_element_set_state (GST_ELEMENT (self->_priv->mixer), GST_STATE_NULL);
			gst_object_unref (self->_priv->mixer);
		}
	}

	/* Go through all elements of a certain class and check whether
	 * they implement a mixer. If so, walk through the tracks and look
	 * for first one named "volume".
	 * 
	 * We should probably do something intelligent if we don't find an
	 * appropriate mixer/track.  But now we do something stupid...
	 * everything just becomes a no-op.
	 */
	mixer_list = gst_audio_default_registry_mixer_filter (_acme_set_mixer,
			TRUE,
			self);

	if (mixer_list == NULL)
		return FALSE;

	/* do not unref the mixer as we keep the ref for self->priv->mixer */
	g_list_free (mixer_list);

	return TRUE;
}

static void
acme_volume_gstreamer_close (AcmeVolumeGStreamer *self)
{
	self->_priv->timer_id = g_timeout_add (TIMEOUT,
			(GSourceFunc) acme_volume_gstreamer_close_real, self);
}

static void
acme_volume_gstreamer_init (AcmeVolumeGStreamer *self)
{
	
	self->_priv = g_new0 (AcmeVolumeGStreamerPrivate, 1);

	self->_priv->gconf_client = gconf_client_get_default ();

	if (acme_volume_gstreamer_open (self) == FALSE)
	{
		g_free (self->_priv);
		self->_priv = NULL;
		return;
	}
	
	if (self->_priv->mixer != NULL) {
		acme_volume_gstreamer_close_real (self);
		return;
	}
}


static void
acme_volume_gstreamer_class_init (AcmeVolumeGStreamerClass *klass)
{
	AcmeVolumeClass *volume_class = ACME_VOLUME_CLASS (klass);
	G_OBJECT_CLASS (klass)->finalize = acme_volume_gstreamer_finalize;

	gst_init (NULL, NULL);

	parent_class = g_type_class_peek_parent (klass);

	volume_class->set_volume = acme_volume_gstreamer_set_volume;
	volume_class->get_volume = acme_volume_gstreamer_get_volume;
	volume_class->set_mute = acme_volume_gstreamer_set_mute;
	volume_class->get_mute = acme_volume_gstreamer_get_mute;
}

