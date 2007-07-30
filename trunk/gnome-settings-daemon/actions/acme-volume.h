/* acme-volume.h

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

#ifndef _ACME_VOLUME_H
#define _ACME_VOLUME_H

#include <glib.h>
#include <glib-object.h>

G_BEGIN_DECLS

#define ACME_TYPE_VOLUME		(acme_volume_get_type ())
#define ACME_VOLUME(obj)		(G_TYPE_CHECK_INSTANCE_CAST ((obj), ACME_TYPE_VOLUME, AcmeVolume))
#define ACME_VOLUME_CLASS(klass)	(G_TYPE_CHECK_CLASS_CAST ((klass), ACME_TYPE_VOLUME, AcmeVolumeClass))
#define ACME_IS_VOLUME(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), ACME_TYPE_VOLUME))
#define ACME_VOLUME_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS ((obj), ACME_TYPE_VOLUME, AcmeVolumeClass))

typedef struct {
	GObject parent;
} AcmeVolume;

typedef struct {
	GObjectClass parent;

	void (* set_volume) (AcmeVolume *self, int val);
	int (* get_volume) (AcmeVolume *self);
	void (* set_mute) (AcmeVolume *self, gboolean val);
	int (* get_mute) (AcmeVolume *self);
} AcmeVolumeClass;

GType acme_volume_get_type			(void);
int acme_volume_get_volume			(AcmeVolume *self);
void acme_volume_set_volume			(AcmeVolume *self, int val);
gboolean acme_volume_get_mute			(AcmeVolume *self);
void acme_volume_set_mute			(AcmeVolume *self,
						 gboolean val);
void acme_volume_mute_toggle			(AcmeVolume * self);
AcmeVolume *acme_volume_new			(void);

G_END_DECLS

#endif /* _ACME_VOLUME_H */
