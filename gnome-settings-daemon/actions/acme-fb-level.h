/* acme-fb-level.c

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

#define ACME_TYPE_FBLEVEL		(acme_fblevel_get_type ())
#define ACME_FBLEVEL(obj)		(G_TYPE_CHECK_INSTANCE_CAST ((obj), ACME_TYPE_FBLEVEL, AcmeFblevel))
#define ACME_FBLEVEL_CLASS(klass)	(G_TYPE_CHECK_CLASS_CAST ((klass), ACME_TYPE_FBLEVEL, AcmeFblevelClass))
#define ACME_IS_FBLEVEL(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), ACME_TYPE_FBLEVEL))
#define ACME_FBLEVEL_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS ((obj), ACME_TYPE_FBLEVEL, AcmeFblevelClass))

typedef struct AcmeFblevelPrivate AcmeFblevelPrivate;
typedef struct AcmeFblevel AcmeFblevel;
typedef struct AcmeFblevelClass AcmeFblevelClass;

struct AcmeFblevel {
	GObject parent;
	int level;
	gboolean dim;
	AcmeFblevelPrivate *_priv;
};

struct AcmeFblevelClass {
	GObjectClass parent;
};

GType acme_fblevel_get_type			(void);
int acme_fblevel_get_level			(AcmeFblevel *self);
void acme_fblevel_set_level			(AcmeFblevel *self, int val);
gboolean acme_fblevel_get_dim			(AcmeFblevel *self);
void acme_fblevel_set_dim			(AcmeFblevel *self,
						 gboolean val);
AcmeFblevel *acme_fblevel_new			(void);
gboolean acme_fblevel_is_powerbook		(void);

