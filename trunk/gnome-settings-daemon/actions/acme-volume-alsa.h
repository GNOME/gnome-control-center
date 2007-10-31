/* acme-volume-alsa.h

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

#include <glib.h>
#include <glib-object.h>
#include "acme-volume.h"

#define ACME_TYPE_VOLUME_ALSA		(acme_volume_get_type ())
#define ACME_VOLUME_ALSA(obj)		(G_TYPE_CHECK_INSTANCE_CAST ((obj), ACME_TYPE_VOLUME_ALSA, AcmeVolumeAlsa))
#define ACME_VOLUME_ALSA_CLASS(klass)	(G_TYPE_CHECK_CLASS_CAST ((klass), ACME_TYPE_VOLUME_ALSA, AcmeVolumeAlsaClass))
#define ACME_IS_VOLUME_ALSA(obj)	(G_TYPE_CHECK_INSTANCE_TYPE ((obj), ACME_TYPE_VOLUME_ALSA))
#define ACME_VOLUME_ALSA_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS ((obj), ACME_TYPE_VOLUME_ALSA, AcmeVolumeAlsaClass))

typedef struct AcmeVolumeAlsa AcmeVolumeAlsa;
typedef struct AcmeVolumeAlsaClass AcmeVolumeAlsaClass;
typedef struct AcmeVolumeAlsaPrivate AcmeVolumeAlsaPrivate;

struct AcmeVolumeAlsa {
	AcmeVolume parent;
	AcmeVolumeAlsaPrivate *_priv;
};

struct AcmeVolumeAlsaClass {
	AcmeVolumeClass parent;
};

GType acme_volume_alsa_get_type		(void);

