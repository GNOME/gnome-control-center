/*
 *
 *  gnome-bluetooth - Bluetooth integration for GNOME
 *
 *  Copyright (C) 2012  Bastien Nocera <hadess@hadess.net>
 *
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#ifndef __CC_RFKILL_GLIB_H
#define __CC_RFKILL_GLIB_H

#include <glib-object.h>
#include "rfkill.h"

G_BEGIN_DECLS

#define CC_RFKILL_TYPE_GLIB (cc_rfkill_glib_get_type())
#define CC_RFKILL_GLIB(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), \
		CC_RFKILL_TYPE_GLIB, CcRfkillGlib))
#define CC_RFKILL_GLIB_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass), \
		CC_RFKILL_TYPE_GLIB, CcRfkillGlibClass))
#define RFKILL_IS_GLIB(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), \
		CC_RFKILL_TYPE_GLIB))
#define RFKILL_IS_GLIB_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), \
		CC_RFKILL_TYPE_GLIB))
#define RFKILL_GET_GLIB_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS((obj), \
		CC_RFKILL_TYPE_GLIB, CcRfkillGlibClass))

typedef struct CcRfkillGlibPrivate CcRfkillGlibPrivate;

typedef struct _CcRfkillGlib {
	GObject parent;
	CcRfkillGlibPrivate *priv;
} CcRfkillGlib;

typedef struct _CcRfkillGlibClass {
	GObjectClass parent_class;

	void (*changed) (CcRfkillGlib *rfkill, GList *events);
} CcRfkillGlibClass;

GType cc_rfkill_glib_get_type(void);

CcRfkillGlib *cc_rfkill_glib_new (void);
int cc_rfkill_glib_open (CcRfkillGlib *rfkill);
int cc_rfkill_glib_send_event (CcRfkillGlib *rfkill, struct rfkill_event *event);

G_END_DECLS

#endif /* __CC_RFKILL_GLIB_H */
