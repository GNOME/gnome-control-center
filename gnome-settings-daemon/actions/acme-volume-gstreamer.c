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
#include <gst/mixer/mixer.h>
#include <gst/propertyprobe/propertyprobe.h>

#include <string.h>

#define TIMEOUT	4000
 
struct AcmeVolumeGStreamerPrivate
{
  	GstMixer      *mixer;
  	GstMixerTrack *track;
 	guint timer_id;
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
				
	g_free (self->_priv);

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
acme_volume_gstreamer_set_mute (AcmeVolume *vol, gboolean val)
{
	AcmeVolumeGStreamer *self = (AcmeVolumeGStreamer *) vol;
	
	if (acme_volume_gstreamer_open (self) == FALSE)
		return;
		
	gst_mixer_set_mute (self->_priv->mixer,
			    self->_priv->track,
			    val);
	
	acme_volume_gstreamer_close (self);
}

static gboolean
acme_volume_gstreamer_get_mute (AcmeVolume *vol)
{
	AcmeVolumeGStreamer *self = (AcmeVolumeGStreamer *) vol;
	gboolean mute;

	if (acme_volume_gstreamer_open (self) == FALSE)
		return FALSE;

	mute = GST_MIXER_TRACK_HAS_FLAG (self->_priv->track,
					 GST_MIXER_TRACK_MUTE);

	acme_volume_gstreamer_close (self);

	return mute;
}

static int
acme_volume_gstreamer_get_volume (AcmeVolume *vol)
{
	gint i, vol_total = 0, *volumes;
	double volume;
	AcmeVolumeGStreamer *self = (AcmeVolumeGStreamer *) vol;
	GstMixerTrack *track;
	
	if (acme_volume_gstreamer_open (self) == FALSE)
		return 0;

	track = self->_priv->track;

	volumes = g_new0 (gint, track->num_channels);
	gst_mixer_get_volume (self->_priv->mixer, track, volumes);
	for (i = 0; i < track->num_channels; ++i)
		vol_total += volumes[i];
	g_free (volumes);

	acme_volume_gstreamer_close (self);
	
	volume = vol_total / (double)track->num_channels;

	/* Normalize the volume to the [0, 100] scale that acme expects. */
	volume = 100 * (volume - track->min_volume) / (track->max_volume - track->min_volume);

	return (gint) volume;
}

static void
acme_volume_gstreamer_set_volume (AcmeVolume *vol, int val)
{
	gint i, *volumes;
	double volume;
	AcmeVolumeGStreamer *self = (AcmeVolumeGStreamer *) vol;
	GstMixerTrack *track;

	if (acme_volume_gstreamer_open (self) == FALSE)
		return;

	track = self->_priv->track;
	val = CLAMP (val, 0, 100);

	/* Rescale the volume from [0, 100] to [track min, track max]. */
	volume = (val / 100.0) * (track->max_volume - track->min_volume) + track->min_volume;

	volumes = g_new (gint, track->num_channels);
	for (i = 0; i < track->num_channels; ++i)
		volumes[i] = (gint) volume;
	gst_mixer_set_volume (self->_priv->mixer, track, volumes);
	g_free (volumes);
 	
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
		gst_element_set_state (GST_ELEMENT(self->_priv->track), GST_STATE_NULL);

		g_object_unref (GST_OBJECT (self->_priv->mixer));	    
		g_object_unref (GST_OBJECT (self->_priv->track));	    
		self->_priv->mixer=NULL;
		self->_priv->track=NULL;
	}
	
	self->_priv->timer_id = 0;
	return FALSE;
}

/* This is a modified version of code from gnome-media's gst-mixer */
static gboolean
acme_volume_gstreamer_open (AcmeVolumeGStreamer *vol)
{
  	AcmeVolumeGStreamer *self = (AcmeVolumeGStreamer *) vol;
  	const GList *elements;
  	gint num = 0;
  
	if (self->_priv == NULL)
		return FALSE;
	
	if (self->_priv->timer_id != 0)
	{
		g_source_remove (self->_priv->timer_id);
		self->_priv->timer_id = 0;
		return TRUE;
	}
		
	/* Go through all elements of a certain class and check whether
	 * they implement a mixer. If so, walk through the tracks and look
	 * for first one named "volume".
	 * 
	 * We should probably do something intelligent if we don't find an
	 * appropriate mixer/track.  But now we do something stupid... everything
	 * just becomes a no-op.
	 */
	elements = gst_registry_pool_feature_list (GST_TYPE_ELEMENT_FACTORY);
	for ( ; elements != NULL && self->_priv->mixer == NULL; elements = elements->next) {
		GstElementFactory *factory = GST_ELEMENT_FACTORY (elements->data);
		gchar *title = NULL;
		const gchar *klass;
		GstElement *element = NULL;
		const GParamSpec *devspec;
		GstPropertyProbe *probe;
		GValueArray *array = NULL;
		gint n;
		const GList *tracks;
		
		/* check category */
		klass = gst_element_factory_get_klass (factory);
		if (strcmp (klass, "Generic/Audio"))
			goto next;
		
		/* create element */
		title = g_strdup_printf ("gst-mixer-%d", num);
		element = gst_element_factory_create (factory, title);
		if (!element)
			goto next;
		
		if (!GST_IS_PROPERTY_PROBE (element))
			goto next;
		
		probe = GST_PROPERTY_PROBE (element);
		devspec = gst_property_probe_get_property (probe, "device");
		if (devspec == NULL)
			goto next;
		array = gst_property_probe_probe_and_get_values (probe, devspec);
		if (array == NULL)
			goto next;

		/* set all devices and test for mixer */
		for (n = 0; n < array->n_values; n++) {
			GValue *device = g_value_array_get_nth (array, n);
			
			/* set this device */
			g_object_set_property (G_OBJECT (element), "device", device);
			if (gst_element_set_state (element,
						   GST_STATE_READY) == GST_STATE_FAILURE)
				continue;
			
			/* Is this device a mixer?  If so, add it to the list. */
			if (!GST_IS_MIXER (element)) {
				gst_element_set_state (element, GST_STATE_NULL);
				continue;
			}
			
			tracks = gst_mixer_list_tracks (GST_MIXER (element));
			for (; tracks != NULL; tracks = tracks->next) {
				GstMixerTrack *track = tracks->data;
				
				if (GST_MIXER_TRACK_HAS_FLAG (track, GST_MIXER_TRACK_MASTER)) {
					self->_priv->mixer = GST_MIXER (element);
					self->_priv->track = track;

					g_object_ref (self->_priv->mixer);
					g_object_ref (self->_priv->track);

					break;
				}
			}
			
			num++;
			
			/* and recreate this object, since we give it to the mixer */
			title = g_strdup_printf ("gst-mixer-%d", num);
			element = gst_element_factory_create (factory, title);
		}
		
	next:
		if (element)
			gst_object_unref (GST_OBJECT (element));
		if (array)
			g_value_array_free (array);
		g_free (title);
	}
	return FALSE;
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

