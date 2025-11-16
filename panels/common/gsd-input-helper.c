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
 *
 */

#include "config.h"

#include <string.h>

#include <gdk/gdk.h>

#include <sys/types.h>

#include "gsd-input-helper.h"
#include "gsd-device-manager.h"

static gboolean
device_type_is_present (GsdDeviceType type)
{
        g_autoptr(GList) l = gsd_device_manager_list_devices (gsd_device_manager_get (),
                                                              type);
        return l != NULL;
}

gboolean
touchscreen_is_present (void)
{
        return device_type_is_present (GSD_DEVICE_TYPE_TOUCHSCREEN);
}

gboolean
touchpad_is_present (void)
{
        return device_type_is_present (GSD_DEVICE_TYPE_TOUCHPAD);
}

gboolean
mouse_is_present (void)
{
        return device_type_is_present (GSD_DEVICE_TYPE_MOUSE);
}

gboolean
pointingstick_is_present (void)
{
        return device_type_is_present (GSD_DEVICE_TYPE_POINTINGSTICK);
}
