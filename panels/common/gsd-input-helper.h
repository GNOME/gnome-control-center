/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2010 Bastien Nocera <hadess@hadess.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

G_BEGIN_DECLS

#include <gdk/gdk.h>
#include <glib.h>

gboolean  touchpad_is_present      (void);
gboolean  touchscreen_is_present   (void);
gboolean  mouse_is_present         (void);
gboolean  pointingstick_is_present (void);

#ifdef GDK_WINDOWING_X11
char     *xdevice_get_device_node (int deviceid);
#endif

G_END_DECLS
