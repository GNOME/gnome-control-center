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

#pragma once

#include <NetworkManager.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

gchar           *panel_device_status_to_localized_string       (NMDevice *nm_device,
                                                                const gchar *speed);
gchar           *panel_get_ip4_address_as_string               (NMIPConfig *config, const gchar *what);
gchar           *panel_get_ip4_dns_as_string                   (NMIPConfig *config);
gchar           *panel_get_ip6_address_as_string               (NMIPConfig *config);

G_END_DECLS
