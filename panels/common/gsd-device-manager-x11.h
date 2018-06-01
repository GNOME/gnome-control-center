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

#define GSD_TYPE_X11_DEVICE_MANAGER (gsd_x11_device_manager_get_type ())
G_DECLARE_FINAL_TYPE (GsdX11DeviceManager, gsd_x11_device_manager, GSD, X11_DEVICE_MANAGER, GsdDeviceManager)

G_END_DECLS

#endif /* __GSD_X11_DEVICE_MANAGER_H__ */
