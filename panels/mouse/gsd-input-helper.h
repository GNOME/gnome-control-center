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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef __GSD_INPUT_HELPER_H
#define __GSD_INPUT_HELPER_H

G_BEGIN_DECLS

#include <glib.h>

#include <X11/extensions/XInput.h>
#include <X11/extensions/XIproto.h>

typedef enum {
	COMMAND_DEVICE_ADDED,
	COMMAND_DEVICE_REMOVED,
	COMMAND_DEVICE_PRESENT
} CustomCommand;

gboolean  supports_xinput_devices (void);
gboolean  device_is_touchpad      (XDevice                *xdevice);
gboolean  touchpad_is_present     (void);

gboolean  run_custom_command      (GdkDevice              *device,
			           CustomCommand           command);

G_END_DECLS

#endif /* __GSD_INPUT_HELPER_H */
