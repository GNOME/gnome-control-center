/* acme-volume-alsa.c

   Copyright (C) 2002, 2003 Bastien Nocera

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
 */

#include "config.h"
#include "acme-volume-alsa.h"

#include <alsa/asoundlib.h>

#ifndef DEFAULT_CARD
#define DEFAULT_CARD "default"
#endif

#undef LOG
#ifdef LOG
#define D(x...) g_message (x)
#else
#define D(x...)
#endif

#define ROUND(x) ((x - (int)x > 0.5) ? x+1 : x)

struct AcmeVolumeAlsaPrivate
{
	long pmin, pmax;
	snd_mixer_t *handle;
	snd_mixer_elem_t *elem;
	gboolean mixerpb;
	int saved_volume;
};

static GObjectClass *parent_class = NULL;

static int acme_volume_alsa_get_volume (AcmeVolume *self);
static void acme_volume_alsa_set_volume (AcmeVolume *self, int val);

G_DEFINE_TYPE (AcmeVolumeAlsa, acme_volume_alsa, ACME_TYPE_VOLUME)

static void
acme_volume_alsa_finalize (GObject *object)
{
	AcmeVolumeAlsa *self;

	g_return_if_fail (object != NULL);
	g_return_if_fail (ACME_IS_VOLUME_ALSA (object));

	self = ACME_VOLUME_ALSA (object);

	if (self->_priv)
		g_free (self->_priv);

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
acme_volume_alsa_set_mute (AcmeVolume *vol, gboolean val)
{
	AcmeVolumeAlsa *self = (AcmeVolumeAlsa *) vol;

	snd_mixer_selem_set_playback_switch(self->_priv->elem,
			SND_MIXER_SCHN_FRONT_LEFT, !val);
	if (val == TRUE)
	{
		self->_priv->saved_volume = acme_volume_alsa_get_volume (vol);
		acme_volume_alsa_set_volume (vol, 0);
	} else {
		if (self->_priv->saved_volume != -1)
			acme_volume_alsa_set_volume (vol,
					self->_priv->saved_volume);
	}
}

static gboolean
acme_volume_alsa_get_mute (AcmeVolume *vol)
{
	AcmeVolumeAlsa *self = (AcmeVolumeAlsa *) vol;
	int ival;

	snd_mixer_selem_get_playback_switch(self->_priv->elem,
			SND_MIXER_SCHN_FRONT_LEFT, &ival);

	return !ival;
}

static int
acme_volume_alsa_get_volume (AcmeVolume *vol)
{
	AcmeVolumeAlsa *self = (AcmeVolumeAlsa *) vol;
	long lval, rval;
	int tmp;
	float alsa_vol;

	snd_mixer_selem_get_playback_volume(self->_priv->elem,
			SND_MIXER_SCHN_FRONT_LEFT, &lval);
	snd_mixer_selem_get_playback_volume(self->_priv->elem,
			SND_MIXER_SCHN_FRONT_RIGHT, &rval);

	alsa_vol = (lval + rval) / 2;
	alsa_vol = alsa_vol * 100 / (self->_priv->pmax - self->_priv->pmin);
	tmp = ROUND (alsa_vol);

	return tmp;
}

static void
acme_volume_alsa_set_volume (AcmeVolume *vol, int val)
{
	AcmeVolumeAlsa *self = (AcmeVolumeAlsa *) vol;
	float volume;
	int tmp;

	volume = (float) val / 100 * (self->_priv->pmax - self->_priv->pmin);
	volume = CLAMP (volume, self->_priv->pmin, self->_priv->pmax);
	tmp = ROUND (volume);

	snd_mixer_selem_set_playback_volume(self->_priv->elem,
			SND_MIXER_SCHN_FRONT_LEFT, tmp);
	snd_mixer_selem_set_playback_volume(self->_priv->elem,
			SND_MIXER_SCHN_FRONT_RIGHT, tmp);
}

static void
acme_volume_alsa_init (AcmeVolumeAlsa *self)
{
	snd_mixer_selem_id_t *sid;
	snd_mixer_t *handle;
	snd_mixer_elem_t *elem;

	self->_priv = g_new0 (AcmeVolumeAlsaPrivate, 1);

	 /* open the mixer */
	if (snd_mixer_open (&handle, 0) < 0)
	{
		D("snd_mixer_open");
		return;
	}
	/* attach the handle to the default card */
	if (snd_mixer_attach(handle, DEFAULT_CARD) <0)
	{
		D("snd_mixer_attach");
		goto bail;
	}
	/* ? */
	if (snd_mixer_selem_register(handle, NULL, NULL) < 0)
	{
		D("snd_mixer_selem_register");
		goto bail;
	}
	if (snd_mixer_load(handle) < 0)
	{
		D("snd_mixer_load");
		goto bail;
	}

	snd_mixer_selem_id_alloca(&sid);
	snd_mixer_selem_id_set_name (sid, "Master");
	elem = snd_mixer_find_selem(handle, sid);
	if (!elem)
	{
		D("snd_mixer_find_selem");
		goto bail;
	}

	if (!snd_mixer_selem_has_playback_volume(elem))
	{
		D("snd_mixer_selem_has_capture_volume");
		goto bail;
	}

	snd_mixer_selem_get_playback_volume_range (elem,
			&(self->_priv->pmin),
			&(self->_priv->pmax));

	self->_priv->handle = handle;
	self->_priv->elem = elem;

	return;
bail:
	g_free (self->_priv);
	self->_priv = NULL;
}

static void
acme_volume_alsa_class_init (AcmeVolumeAlsaClass *klass)
{
	AcmeVolumeClass *volume_class = ACME_VOLUME_CLASS (klass);
	G_OBJECT_CLASS (klass)->finalize = acme_volume_alsa_finalize;

	parent_class = g_type_class_peek_parent (klass);

	volume_class->set_volume = acme_volume_alsa_set_volume;
	volume_class->get_volume = acme_volume_alsa_get_volume;
	volume_class->set_mute = acme_volume_alsa_set_mute;
	volume_class->get_mute = acme_volume_alsa_get_mute;
}

