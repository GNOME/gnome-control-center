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
#include <gtk/gtk.h>

G_BEGIN_DECLS

const gchar     *panel_device_to_icon_name                     (NMDevice *device,
                                                                gboolean  symbolic);
gint             panel_device_get_sort_category                (NMDevice *device);
const gchar     *panel_ap_mode_to_localized_string             (NM80211Mode mode);
const gchar     *panel_vpn_state_to_localized_string           (NMVpnConnectionState type);
void             panel_set_device_status                       (GtkBuilder *builder,
                                                                const gchar *label_name,
                                                                NMDevice *nm_device,
                                                                const gchar *speed);
gboolean         panel_set_device_widget_details               (GtkBuilder *builder,
                                                                const gchar *widget_suffix,
                                                                const gchar *value);
gboolean         panel_set_device_widget_header                (GtkBuilder *builder,
                                                                const gchar *widget_suffix,
                                                                const gchar *value);
void             panel_set_device_widgets                      (GtkBuilder *builder,
                                                                NMDevice *device);
void             panel_unset_device_widgets                    (GtkBuilder *builder);
gchar           *panel_get_ip4_address_as_string               (NMIPConfig *config, const gchar *what);
gchar           *panel_get_ip4_dns_as_string                   (NMIPConfig *config);
gchar           *panel_get_ip6_address_as_string               (NMIPConfig *config);

G_END_DECLS

#endif /* PANEL_COMMON_H */
