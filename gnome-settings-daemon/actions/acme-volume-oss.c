/* acme-volume-oss.c

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
#include "acme-volume-oss.h"

#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>

#ifdef __NetBSD__
#include <soundcard.h>
#else
#include <sys/soundcard.h>
#endif /* __NetBSD__ */

struct AcmeVolumeOssPrivate
{
	gboolean use_pcm;
	gboolean mixerpb;
	int volume;
	int saved_volume;
	gboolean pcm_avail;
	gboolean mute;
};

static GObjectClass *parent_class = NULL;

static int acme_volume_oss_get_volume (AcmeVolume *self);
static void acme_volume_oss_set_volume (AcmeVolume *self, int val);
static gboolean acme_volume_oss_mixer_check (AcmeVolumeOss *self, int fd);

static void
acme_volume_oss_finalize (GObject *object)
{
	AcmeVolumeOss *self;

	g_return_if_fail (object != NULL);
	g_return_if_fail (ACME_IS_VOLUME_OSS (object));

	self = ACME_VOLUME_OSS (object);

	g_return_if_fail (self->_priv != NULL);
	g_free (self->_priv);

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static int
acme_volume_oss_vol_check (int volume)
{
	return CLAMP (volume, 0, 100);
}

static void
acme_volume_oss_set_mute (AcmeVolume *vol, gboolean val)
{
	AcmeVolumeOss *self = (AcmeVolumeOss *) vol;

	if (self->_priv->mute == FALSE)
	{
		self->_priv->saved_volume =
			acme_volume_oss_get_volume (vol);
		acme_volume_oss_set_volume (vol, 0);
		self->_priv->mute = TRUE;
	} else {
		acme_volume_oss_set_volume (vol, self->_priv->saved_volume);
		self->_priv->mute = FALSE;
	}
}

static gboolean
acme_volume_oss_get_mute (AcmeVolume *vol)
{
	AcmeVolumeOss *self = (AcmeVolumeOss *) vol;

	/* somebody else might have changed the volume */
	if ((self->_priv->mute == TRUE) && (self->_priv->volume != 0))
	{
		self->_priv->mute = FALSE;
	}

	return self->_priv->mute;
}

static int
acme_volume_oss_get_volume (AcmeVolume *vol)
{
	gint volume, r, l, fd;
	AcmeVolumeOss *self = (AcmeVolumeOss *) vol;

	fd  = open ("/dev/mixer", O_RDONLY);
	if (acme_volume_oss_mixer_check(self, fd) == FALSE)
	{
		volume = 0;
	} else {
		if (self->_priv->use_pcm && self->_priv->pcm_avail)
			ioctl (fd, MIXER_READ (SOUND_MIXER_PCM), &volume);
		else
			ioctl (fd, MIXER_READ (SOUND_MIXER_VOLUME), &volume);
		close (fd);

		r = (volume & 0xff);
		l = (volume & 0xff00) >> 8;
		volume = (r + l) / 2;
		volume = acme_volume_oss_vol_check (volume);
	}

	return volume;
}

static void
acme_volume_oss_set_volume (AcmeVolume *vol, int val)
{
	int fd, tvol, volume;
	AcmeVolumeOss *self = (AcmeVolumeOss *) vol;

	volume = acme_volume_oss_vol_check (val);

	fd = open ("/dev/mixer", O_RDONLY);
	if (acme_volume_oss_mixer_check (self, fd) == FALSE)
	{
		return;
	} else {
		tvol = (volume << 8) + volume;
		if (self->_priv->use_pcm && self->_priv->pcm_avail)
			ioctl (fd, MIXER_WRITE (SOUND_MIXER_PCM), &tvol);
		else
			ioctl (fd, MIXER_WRITE (SOUND_MIXER_VOLUME), &tvol);
		close (fd);
		self->_priv->volume = volume;
	}
}

static void
acme_volume_oss_init (AcmeVolume *vol)
{
	AcmeVolumeOss *self = (AcmeVolumeOss *) vol;
	int fd;

	self->_priv = g_new0 (AcmeVolumeOssPrivate, 1);

	fd  = open ("/dev/mixer", O_RDONLY);
	if (acme_volume_oss_mixer_check(self, fd) == FALSE)
	{
		self->_priv->pcm_avail = FALSE;
	} else {
		int mask = 0;

		ioctl (fd, SOUND_MIXER_READ_DEVMASK, &mask);
		if (mask & ( 1 << SOUND_MIXER_PCM))
			self->_priv->pcm_avail = TRUE;
		else
			self->_priv->pcm_avail = FALSE;
		if (!(mask & ( 1 << SOUND_MIXER_VOLUME)))
			self->_priv->use_pcm = TRUE;

		close (fd);
	}
}

static void
acme_volume_oss_class_init (AcmeVolumeOssClass *klass)
{
	AcmeVolumeClass *volume_class = ACME_VOLUME_CLASS (klass);
	G_OBJECT_CLASS (klass)->finalize = acme_volume_oss_finalize;

	parent_class = g_type_class_peek_parent (klass);

	volume_class->set_volume = acme_volume_oss_set_volume;
	volume_class->get_volume = acme_volume_oss_get_volume;
	volume_class->set_mute = acme_volume_oss_set_mute;
	volume_class->get_mute = acme_volume_oss_get_mute;
}

GType acme_volume_oss_get_type (void)
{
	static GType object_type = 0;

	if (!object_type)
	{
		static const GTypeInfo object_info =
		{
			sizeof (AcmeVolumeOssClass),
			NULL,         /* base_init */
			NULL,         /* base_finalize */
			(GClassInitFunc) acme_volume_oss_class_init,
			NULL,         /* class_finalize */
			NULL,         /* class_data */
			sizeof (AcmeVolumeOss),
			0,            /* n_preallocs */
			(GInstanceInitFunc) acme_volume_oss_init
		};

		object_type = g_type_register_static (ACME_TYPE_VOLUME,
				"AcmeVolumeOss", &object_info, 0);
	}

	return object_type;
}

static gboolean
acme_volume_oss_mixer_check (AcmeVolumeOss *self, int fd)
{
	gboolean retval;

	if (fd <0) {
		if (self->_priv->mixerpb == FALSE) {
			self->_priv->mixerpb = TRUE;
			//FIXME
			//volume_oss_fd_problem(self);
		}
	}
	retval = (!self->_priv->mixerpb);
	return retval;
}

