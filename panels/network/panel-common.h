/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2010 Richard Hughes <richard@hughsie.com>
 *
 * Licensed under the GNU General Public License Version 2
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
 */

#ifndef PANEL_COMMON_H
#define PANEL_COMMON_H

#include <glib-object.h>
#include <NetworkManager.h>
#include <NetworkManagerVPN.h>
#include <nm-device.h>

G_BEGIN_DECLS

const gchar     *panel_device_to_icon_name              (NMDevice *device);
const gchar     *panel_device_to_localized_string       (NMDevice *device);
const gchar     *panel_device_to_sortable_string        (NMDevice *device);
const gchar     *panel_ap_mode_to_localized_string      (NM80211Mode mode);
const gchar     *panel_device_state_to_localized_string (NMDevice *device);
const gchar     *panel_vpn_state_to_localized_string    (NMVPNConnectionState type);

G_END_DECLS

#endif /* PANEL_COMMON_H */

