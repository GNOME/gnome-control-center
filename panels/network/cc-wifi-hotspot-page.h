/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* wifi-hotspot-page.h
 *
 * Copyright 2023 Red Hat, Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Author(s):
 *   Felipe Borges <felipeborges@gnome.org>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <adwaita.h>
#include "net-device-wifi.h"

G_BEGIN_DECLS

#define CC_TYPE_WIFI_HOTSPOT_PAGE (cc_wifi_hotspot_page_get_type())
G_DECLARE_FINAL_TYPE (CcWifiHotspotPage, cc_wifi_hotspot_page, CC, WIFI_HOTSPOT_PAGE, AdwNavigationPage)

CcWifiHotspotPage *cc_wifi_hotspot_page_new (NetDeviceWifi *wifi);

G_END_DECLS
