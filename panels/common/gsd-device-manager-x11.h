/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2015 Red Hat
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
 *
 * Author: Carlos Garnacho <carlosg@gnome.org>
 */

#ifndef __GSD_X11_DEVICE_MANAGER_H__
#define __GSD_X11_DEVICE_MANAGER_H__

#include <gdk/gdk.h>
#include "gsd-device-manager.h"

G_BEGIN_DECLS

#define GSD_TYPE_X11_DEVICE_MANAGER         (gsd_x11_device_manager_get_type ())
#define GSD_X11_DEVICE_MANAGER(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), GSD_TYPE_X11_DEVICE_MANAGER, GsdX11DeviceManager))
#define GSD_X11_DEVICE_MANAGER_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), GSD_TYPE_X11_DEVICE_MANAGER, GsdX11DeviceManagerClass))
#define GSD_IS_X11_DEVICE_MANAGER(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), GSD_TYPE_X11_DEVICE_MANAGER))
#define GSD_IS_X11_DEVICE_MANAGER_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), GSD_TYPE_X11_DEVICE_MANAGER))
#define GSD_X11_DEVICE_MANAGER_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), GSD_TYPE_X11_DEVICE_MANAGER, GsdX11DeviceManagerClass))

typedef struct _GsdX11DeviceManager GsdX11DeviceManager;
typedef struct _GsdX11DeviceManagerClass GsdX11DeviceManagerClass;

GType          gsd_x11_device_manager_get_type	        (void) G_GNUC_CONST;

G_END_DECLS

#endif /* __GSD_X11_DEVICE_MANAGER_H__ */
