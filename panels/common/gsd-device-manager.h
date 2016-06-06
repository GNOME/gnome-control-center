/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2014 Carlos Garnacho <carlosg@gnome.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef __GSD_DEVICE_MANAGER_H__
#define __GSD_DEVICE_MANAGER_H__

#include <gdk/gdk.h>
#include <glib-object.h>
#include <gio/gio.h>

G_BEGIN_DECLS

#define GSD_TYPE_DEVICE		(gsd_device_get_type ())
#define GSD_DEVICE(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), GSD_TYPE_DEVICE, GsdDevice))
#define GSD_DEVICE_CLASS(k)	(G_TYPE_CHECK_CLASS_CAST((k), GSD_TYPE_DEVICE, GsdDeviceClass))
#define GSD_IS_DEVICE(o)	(G_TYPE_CHECK_INSTANCE_TYPE ((o), GSD_TYPE_DEVICE))
#define GSD_IS_DEVICE_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), GSD_TYPE_DEVICE))
#define GSD_DEVICE_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), GSD_TYPE_DEVICE, GsdDeviceClass))

#define GSD_TYPE_DEVICE_MANAGER		(gsd_device_manager_get_type ())
#define GSD_DEVICE_MANAGER(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), GSD_TYPE_DEVICE_MANAGER, GsdDeviceManager))
#define GSD_DEVICE_MANAGER_CLASS(k)	(G_TYPE_CHECK_CLASS_CAST((k), GSD_TYPE_DEVICE_MANAGER, GsdDeviceManagerClass))
#define GSD_IS_DEVICE_MANAGER(o)	(G_TYPE_CHECK_INSTANCE_TYPE ((o), GSD_TYPE_DEVICE_MANAGER))
#define GSD_IS_DEVICE_MANAGER_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), GSD_TYPE_DEVICE_MANAGER))
#define GSD_DEVICE_MANAGER_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), GSD_TYPE_DEVICE_MANAGER, GsdDeviceManagerClass))

typedef struct _GsdDevice GsdDevice;
typedef struct _GsdDeviceClass GsdDeviceClass;
typedef struct _GsdDeviceManager GsdDeviceManager;
typedef struct _GsdDeviceManagerClass GsdDeviceManagerClass;

typedef enum {
	GSD_DEVICE_TYPE_MOUSE	     = 1 << 0,
	GSD_DEVICE_TYPE_KEYBOARD     = 1 << 1,
	GSD_DEVICE_TYPE_TOUCHPAD     = 1 << 2,
	GSD_DEVICE_TYPE_TABLET	     = 1 << 3,
	GSD_DEVICE_TYPE_TOUCHSCREEN  = 1 << 4,
	GSD_DEVICE_TYPE_PAD          = 1 << 5
} GsdDeviceType;

struct _GsdDevice {
	GObject parent_instance;
};

struct _GsdDeviceClass {
	GObjectClass parent_class;
};

struct _GsdDeviceManager
{
	GObject parent_instance;
};

struct _GsdDeviceManagerClass
{
	GObjectClass parent_class;

	GList * (* list_devices) (GsdDeviceManager *manager,
				  GsdDeviceType	    type);

	void (* device_added)	(GsdDeviceManager *manager,
				 GsdDevice	  *device);
	void (* device_removed) (GsdDeviceManager *manager,
				 GsdDevice	  *device);
	void (* device_changed) (GsdDeviceManager *manager,
				 GsdDevice	  *device);

	GsdDevice * (* lookup_device) (GsdDeviceManager *manager,
				       GdkDevice	*gdk_device);
};

GType		   gsd_device_get_type		      (void) G_GNUC_CONST;
GType		   gsd_device_manager_get_type	      (void) G_GNUC_CONST;
GsdDeviceManager * gsd_device_manager_get	      (void);
GList *		   gsd_device_manager_list_devices    (GsdDeviceManager *manager,
						       GsdDeviceType	 type);

const gchar *	   gsd_device_get_name	      (GsdDevice  *device);
GsdDeviceType	   gsd_device_get_device_type (GsdDevice  *device);
void		   gsd_device_get_device_ids  (GsdDevice    *device,
					       const gchar **vendor,
					       const gchar **product);
GSettings *	   gsd_device_get_settings    (GsdDevice  *device);

const gchar *	   gsd_device_get_device_file (GsdDevice  *device);
gboolean	   gsd_device_get_dimensions  (GsdDevice  *device,
					       guint	  *width,
					       guint	  *height);

GsdDevice *	   gsd_device_manager_lookup_gdk_device (GsdDeviceManager *manager,
							 GdkDevice	  *gdk_device);

G_END_DECLS

#endif /* __GSD_DEVICE_MANAGER_H__ */
