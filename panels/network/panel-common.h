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

G_BEGIN_DECLS

typedef enum {
        NM_DEVICE_TYPE_UNKNOWN,
        NM_DEVICE_TYPE_ETHERNET,
        NM_DEVICE_TYPE_WIFI,
        NM_DEVICE_TYPE_GSM,
        NM_DEVICE_TYPE_CDMA,
        NM_DEVICE_TYPE_BLUETOOTH,
        NM_DEVICE_TYPE_MESH
} NMDeviceType;

typedef enum {
        NM_DEVICE_STATE_UNKNOWN,
        NM_DEVICE_STATE_UNMANAGED,
        NM_DEVICE_STATE_UNAVAILABLE,
        NM_DEVICE_STATE_DISCONNECTED,
        NM_DEVICE_STATE_PREPARE,
        NM_DEVICE_STATE_CONFIG,
        NM_DEVICE_STATE_NEED_AUTH,
        NM_DEVICE_STATE_IP_CONFIG,
        NM_DEVICE_STATE_ACTIVATED,
        NM_DEVICE_STATE_FAILED
} NMDeviceState;

typedef enum {
        NM_802_11_MODE_UNKNOWN = 0,
        NM_802_11_MODE_ADHOC,
        NM_802_11_MODE_INFRA
} NM80211Mode;

const gchar     *panel_device_type_to_icon_name                 (guint type);
const gchar     *panel_device_type_to_localized_string          (guint type);
const gchar     *panel_device_type_to_sortable_string           (guint type);
const gchar     *panel_ap_mode_to_localized_string              (guint mode);
const gchar     *panel_device_state_to_localized_string         (guint type);
gchar           *panel_ipv4_to_string                           (GVariant *variant);
gchar           *panel_ipv6_to_string                           (GVariant *variant);

G_END_DECLS

#endif /* PANEL_COMMON_H */

