/*
 * Copyright (C) 2023 Red Hat, Inc
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 */

#pragma once

#include <gio/gio.h>

#define REMOTE_DESKTOP_SERVICE "gnome-remote-desktop.service"

typedef enum {
  CC_SERVICE_STATE_ENABLED,
  CC_SERVICE_STATE_DISABLED,
  CC_SERVICE_STATE_STATIC,
  CC_SERVICE_STATE_MASKED,
  CC_SERVICE_STATE_NOT_FOUND
} CcServiceState;

CcServiceState cc_get_service_state (const char  *service,
                                     GBusType     bus_type);

gboolean cc_enable_service (const char  *service,
                            GBusType     bus_type,
                            GError     **error);

gboolean cc_disable_service (const char  *service,
                             GBusType     bus_type,
                             GError     **error);
