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

#include "config.h"

#include <glib.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include "panel-common.h"
#include "nm-device-ethernet.h"
#include "nm-device-modem.h"

/**
 * panel_device_to_icon_name:
 **/
const gchar *
panel_device_to_icon_name (NMDevice *device)
{
        const gchar *value = NULL;
        NMDeviceState state;
        NMDeviceModemCapabilities caps;
        switch (nm_device_get_device_type (device)) {
        case NM_DEVICE_TYPE_ETHERNET:
                state = nm_device_get_state (device);
                if (state == NM_DEVICE_STATE_UNAVAILABLE) {
                        value = "network-wired-disconnected";
                } else {
                        value = "network-wired";
                }
                break;
        case NM_DEVICE_TYPE_WIFI:
        case NM_DEVICE_TYPE_BT:
        case NM_DEVICE_TYPE_OLPC_MESH:
                value = "network-wireless";
                break;
        case NM_DEVICE_TYPE_MODEM:
                caps = nm_device_modem_get_current_capabilities (NM_DEVICE_MODEM (device));
                if ((caps & NM_DEVICE_MODEM_CAPABILITY_GSM_UMTS) ||
                    (caps & NM_DEVICE_MODEM_CAPABILITY_CDMA_EVDO)) {
                        value = "network-wireless";
                }
                break;
        default:
                break;
        }
        return value;
}

/**
 * panel_device_to_localized_string:
 **/
const gchar *
panel_device_to_localized_string (NMDevice *device)
{
        const gchar *value = NULL;
        NMDeviceModemCapabilities caps;
        switch (nm_device_get_device_type (device)) {
        case NM_DEVICE_TYPE_UNKNOWN:
                /* TRANSLATORS: device type */
                value = _("Unknown");
                break;
        case NM_DEVICE_TYPE_ETHERNET:
                /* TRANSLATORS: device type */
                value = _("Wired");
                break;
        case NM_DEVICE_TYPE_WIFI:
                /* TRANSLATORS: device type */
                value = _("Wireless");
                break;
        case NM_DEVICE_TYPE_MODEM:
                caps = nm_device_modem_get_current_capabilities (NM_DEVICE_MODEM (device));
                if ((caps & NM_DEVICE_MODEM_CAPABILITY_GSM_UMTS) ||
                    (caps & NM_DEVICE_MODEM_CAPABILITY_CDMA_EVDO)) {
                        /* TRANSLATORS: device type */
                        value = _("Mobile broadband");
                }
                break;
        case NM_DEVICE_TYPE_BT:
                /* TRANSLATORS: device type */
                value = _("Bluetooth");
                break;
        case NM_DEVICE_TYPE_OLPC_MESH:
                /* TRANSLATORS: device type */
                value = _("Mesh");
                break;
        default:
                break;
        }
        return value;
}

/**
 * panel_device_to_sortable_string:
 *
 * Try to return order of approximate connection speed.
 **/
const gchar *
panel_device_to_sortable_string (NMDevice *device)
{
        const gchar *value = NULL;
        NMDeviceModemCapabilities caps;
        switch (nm_device_get_device_type (device)) {
        case NM_DEVICE_TYPE_ETHERNET:
                value = "1";
                break;
        case NM_DEVICE_TYPE_WIFI:
                value = "2";
                break;
        case NM_DEVICE_TYPE_MODEM:
                caps = nm_device_modem_get_current_capabilities (NM_DEVICE_MODEM (device));
                if ((caps & NM_DEVICE_MODEM_CAPABILITY_GSM_UMTS) ||
                    (caps & NM_DEVICE_MODEM_CAPABILITY_CDMA_EVDO)) {
                        value = "3";
                }
                break;
        case NM_DEVICE_TYPE_BT:
                value = "4";
                break;
        case NM_DEVICE_TYPE_OLPC_MESH:
                value = "5";
                break;
        default:
                value = "6";
                break;
        }
        return value;
}

/**
 * panel_ap_mode_to_localized_string:
 **/
const gchar *
panel_ap_mode_to_localized_string (NM80211Mode mode)
{
        const gchar *value = NULL;
        switch (mode) {
        case NM_802_11_MODE_UNKNOWN:
                /* TRANSLATORS: AP type */
                value = _("Unknown");
                break;
        case NM_802_11_MODE_ADHOC:
                /* TRANSLATORS: AP type */
                value = _("Ad-hoc");
                break;
        case NM_802_11_MODE_INFRA:
                /* TRANSLATORS: AP type */
                value = _("Infrastructure");
                break;
        default:
                break;
        }
        return value;
}

/**
 * panel_device_state_to_localized_string:
 **/
const gchar *
panel_device_state_to_localized_string (NMDevice *device)
{
        NMDeviceType type;
        NMDeviceState state;

        type = nm_device_get_device_type (device);
        state = nm_device_get_state (device);

        const gchar *value = NULL;
        switch (state) {
        case NM_DEVICE_STATE_UNKNOWN:
                /* TRANSLATORS: device status */
                value = _("Status unknown");
                break;
        case NM_DEVICE_STATE_UNMANAGED:
                /* TRANSLATORS: device status */
                value = _("Unmanaged");
                break;
        case NM_DEVICE_STATE_UNAVAILABLE:
                /* TRANSLATORS: device status */
                if (nm_device_get_firmware_missing (device))
                        value = _("Firmware missing");
                else if (type == NM_DEVICE_TYPE_ETHERNET &&
                         !nm_device_ethernet_get_carrier (NM_DEVICE_ETHERNET (device)))
                        value = _("Cable unplugged");
                else
                        value = _("Unavailable");
                break;
        case NM_DEVICE_STATE_DISCONNECTED:
                /* TRANSLATORS: device status */
                value = _("Disconnected");
                break;
        case NM_DEVICE_STATE_PREPARE:
        case NM_DEVICE_STATE_CONFIG:
        case NM_DEVICE_STATE_IP_CONFIG:
        case NM_DEVICE_STATE_IP_CHECK:
                /* TRANSLATORS: device status */
                value = _("Connecting");
                break;
        case NM_DEVICE_STATE_NEED_AUTH:
                /* TRANSLATORS: device status */
                value = _("Authentication required");
                break;
        case NM_DEVICE_STATE_ACTIVATED:
                /* TRANSLATORS: device status */
                value = _("Connected");
                break;
        case NM_DEVICE_STATE_DEACTIVATING:
                /* TRANSLATORS: device status */
                value = _("Disconnecting");
                break;
        case NM_DEVICE_STATE_FAILED:
                /* TRANSLATORS: device status */
                value = _("Connection failed");
                break;
        default:
                /* TRANSLATORS: device status */
                value = _("Status unknown (missing)");
                break;
        }
        return value;
}

/**
 * panel_vpn_state_to_localized_string:
 **/
const gchar *
panel_vpn_state_to_localized_string (NMVPNConnectionState type)
{
        const gchar *value = NULL;
        switch (type) {
        case NM_DEVICE_STATE_UNKNOWN:
                /* TRANSLATORS: VPN status */
                value = _("Status unknown");
                break;
        case NM_VPN_CONNECTION_STATE_PREPARE:
        case NM_VPN_CONNECTION_STATE_CONNECT:
        case NM_VPN_CONNECTION_STATE_IP_CONFIG_GET:
                /* TRANSLATORS: VPN status */
                value = _("Connecting");
                break;
        case NM_VPN_CONNECTION_STATE_NEED_AUTH:
                /* TRANSLATORS: VPN status */
                value = _("Authentication required");
                break;
        case NM_VPN_CONNECTION_STATE_ACTIVATED:
                /* TRANSLATORS: VPN status */
                value = _("Connected");
                break;
        case NM_VPN_CONNECTION_STATE_FAILED:
                /* TRANSLATORS: VPN status */
                value = _("Connection failed");
                break;
        case NM_VPN_CONNECTION_STATE_DISCONNECTED:
                /* TRANSLATORS: VPN status */
                value = _("Not connected");
                break;
        default:
                /* TRANSLATORS: VPN status */
                value = _("Status unknown (missing)");
                break;
        }
        return value;
}
