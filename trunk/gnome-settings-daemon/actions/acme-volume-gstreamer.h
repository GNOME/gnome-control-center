/* acme-volume-gstreamer.h

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

#include <glib.h>
#include <glib-object.h>
#include "acme-volume.h"

#define ACME_TYPE_VOLUME_GSTREAMER		(acme_volume_get_type ())
#define ACME_VOLUME_GSTREAMER(obj)		(G_TYPE_CHECK_INSTANCE_CAST ((obj), ACME_TYPE_VOLUME_GSTREAMER, AcmeVolumeGStreamer))
#define ACME_VOLUME_GSTREAMER_CLASS(klass)	(G_TYPE_CHECK_CLASS_CAST ((klass), ACME_TYPE_VOLUME_GSTREAMER, AcmeVolumeGStreamerClass))
#define ACME_IS_VOLUME_GSTREAMER(obj)	(G_TYPE_CHECK_INSTANCE_TYPE ((obj), ACME_TYPE_VOLUME_GSTREAMER))
#define ACME_VOLUME_GSTREAMER_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS ((obj), ACME_TYPE_VOLUME_GSTREAMER, AcmeVolumeGStreamerClass))

typedef struct AcmeVolumeGStreamer AcmeVolumeGStreamer;
typedef struct AcmeVolumeGStreamerClass AcmeVolumeGStreamerClass;
typedef struct AcmeVolumeGStreamerPrivate AcmeVolumeGStreamerPrivate;

struct AcmeVolumeGStreamer {
	AcmeVolume parent;
	AcmeVolumeGStreamerPrivate *_priv;
};

struct AcmeVolumeGStreamerClass {
	AcmeVolumeClass parent;
};

GType acme_volume_gstreamer_get_type		(void);

