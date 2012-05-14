/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2010 Richard Hughes <richard@hughsie.com>
 * Copyright (C) 2012 Thomas Bechtold <thomasbechtold@jpberlin.de>
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

/**
 * panel_device_state_reason_to_localized_string:
 **/
const gchar *
panel_device_state_reason_to_localized_string (NMDevice *device)
{
        const gchar *value = NULL;
        NMDeviceStateReason state_reason;

        /* we only want the StateReason's we care about */
        nm_device_get_state_reason (device, &state_reason);
        switch (state_reason) {
        case NM_DEVICE_STATE_REASON_CONFIG_FAILED:
                /* TRANSLATORS: device status reason */
                value = _("Configuration failed");
                break;
        case NM_DEVICE_STATE_REASON_IP_CONFIG_UNAVAILABLE:
                /* TRANSLATORS: device status reason */
                value = _("IP configuration failed");
                break;
        case NM_DEVICE_STATE_REASON_IP_CONFIG_EXPIRED:
                /* TRANSLATORS: device status reason */
                value = _("IP configuration expired");
                break;
        case NM_DEVICE_STATE_REASON_NO_SECRETS:
                /* TRANSLATORS: device status reason */
                value = _("Secrets were required, but not provided");
                break;
        case NM_DEVICE_STATE_REASON_SUPPLICANT_DISCONNECT:
                /* TRANSLATORS: device status reason */
                value = _("802.1x supplicant disconnected");
                break;
        case NM_DEVICE_STATE_REASON_SUPPLICANT_CONFIG_FAILED:
                /* TRANSLATORS: device status reason */
                value = _("802.1x supplicant configuration failed");
                break;
        case NM_DEVICE_STATE_REASON_SUPPLICANT_FAILED:
                /* TRANSLATORS: device status reason */
                value = _("802.1x supplicant failed");
                break;
        case NM_DEVICE_STATE_REASON_SUPPLICANT_TIMEOUT:
                /* TRANSLATORS: device status reason */
                value = _("802.1x supplicant took too long to authenticate");
                break;
        case NM_DEVICE_STATE_REASON_PPP_START_FAILED:
                /* TRANSLATORS: device status reason */
                value = _("PPP service failed to start");
                break;
        case NM_DEVICE_STATE_REASON_PPP_DISCONNECT:
                /* TRANSLATORS: device status reason */
                value = _("PPP service disconnected");
                break;
        case NM_DEVICE_STATE_REASON_PPP_FAILED:
                /* TRANSLATORS: device status reason */
                value = _("PPP failed");
                break;
        case NM_DEVICE_STATE_REASON_DHCP_START_FAILED:
                /* TRANSLATORS: device status reason */
                value = _("DHCP client failed to start");
                break;
        case NM_DEVICE_STATE_REASON_DHCP_ERROR:
                /* TRANSLATORS: device status reason */
                value = _("DHCP client error");
                break;
        case NM_DEVICE_STATE_REASON_DHCP_FAILED:
                /* TRANSLATORS: device status reason */
                value = _("DHCP client failed");
                break;
        case NM_DEVICE_STATE_REASON_SHARED_START_FAILED:
                /* TRANSLATORS: device status reason */
                value = _("Shared connection service failed to start");
                break;
        case NM_DEVICE_STATE_REASON_SHARED_FAILED:
                /* TRANSLATORS: device status reason */
                value = _("Shared connection service failed");
                break;
        case NM_DEVICE_STATE_REASON_AUTOIP_START_FAILED:
                /* TRANSLATORS: device status reason */
                value = _("AutoIP service failed to start");
                break;
        case NM_DEVICE_STATE_REASON_AUTOIP_ERROR:
                /* TRANSLATORS: device status reason */
                value = _("AutoIP service error");
                break;
        case NM_DEVICE_STATE_REASON_AUTOIP_FAILED:
                /* TRANSLATORS: device status reason */
                value = _("AutoIP service failed");
                break;
        case NM_DEVICE_STATE_REASON_MODEM_BUSY:
                /* TRANSLATORS: device status reason */
                value = _("Line busy");
                break;
        case NM_DEVICE_STATE_REASON_MODEM_NO_DIAL_TONE:
                /* TRANSLATORS: device status reason */
                value = _("No dial tone");
                break;
        case NM_DEVICE_STATE_REASON_MODEM_NO_CARRIER:
                /* TRANSLATORS: device status reason */
                value = _("No carrier could be established");
                break;
        case NM_DEVICE_STATE_REASON_MODEM_DIAL_TIMEOUT:
                /* TRANSLATORS: device status reason */
                value = _("Dialing request timed out");
                break;
        case NM_DEVICE_STATE_REASON_MODEM_DIAL_FAILED:
                /* TRANSLATORS: device status reason */
                value = _("Dialing attempt failed");
                break;
        case NM_DEVICE_STATE_REASON_MODEM_INIT_FAILED:
                /* TRANSLATORS: device status reason */
                value = _("Modem initialization failed");
                break;
        case NM_DEVICE_STATE_REASON_GSM_APN_FAILED:
                /* TRANSLATORS: device status reason */
                value = _("Failed to select the specified APN");
                break;
        case NM_DEVICE_STATE_REASON_GSM_REGISTRATION_NOT_SEARCHING:
                /* TRANSLATORS: device status reason */
                value = _("Not searching for networks");
                break;
        case NM_DEVICE_STATE_REASON_GSM_REGISTRATION_DENIED:
                /* TRANSLATORS: device status reason */
                value = _("Network registration denied");
                break;
        case NM_DEVICE_STATE_REASON_GSM_REGISTRATION_TIMEOUT:
                /* TRANSLATORS: device status reason */
                value = _("Network registration timed out");
                break;
        case NM_DEVICE_STATE_REASON_GSM_REGISTRATION_FAILED:
                /* TRANSLATORS: device status reason */
                value = _("Failed to register with the requested network");
                break;
        case NM_DEVICE_STATE_REASON_GSM_PIN_CHECK_FAILED:
                /* TRANSLATORS: device status reason */
                value = _("PIN check failed");
                break;
        case NM_DEVICE_STATE_REASON_FIRMWARE_MISSING:
                /* TRANSLATORS: device status reason */
                value = _("Firmware for the device may be missing");
                break;
        case NM_DEVICE_STATE_REASON_CONNECTION_REMOVED:
                /* TRANSLATORS: device status reason */
                value = _("Connection disappeared");
                break;
        case NM_DEVICE_STATE_REASON_CARRIER:
                /* TRANSLATORS: device status reason */
                value = _("Carrier/link changed");
                break;
        case NM_DEVICE_STATE_REASON_CONNECTION_ASSUMED:
                /* TRANSLATORS: device status reason */
                value = _("Existing connection was assumed");
                break;
        case NM_DEVICE_STATE_REASON_MODEM_NOT_FOUND:
                /* TRANSLATORS: device status reason */
                value = _("Modem not found");
                break;
        case NM_DEVICE_STATE_REASON_BT_FAILED:
                /* TRANSLATORS: device status reason */
                value = _("Bluetooth connection failed");
                break;
        case NM_DEVICE_STATE_REASON_GSM_SIM_NOT_INSERTED:
                /* TRANSLATORS: device status reason */
                value = _("SIM Card not inserted");
                break;
        case NM_DEVICE_STATE_REASON_GSM_SIM_PIN_REQUIRED:
                /* TRANSLATORS: device status reason */
                value = _("SIM Pin required");
                break;
        case NM_DEVICE_STATE_REASON_GSM_SIM_PUK_REQUIRED:
                /* TRANSLATORS: device status reason */
                value = _("SIM Puk required");
                break;
        case NM_DEVICE_STATE_REASON_GSM_SIM_WRONG:
                /* TRANSLATORS: device status reason */
                value = _("SIM wrong");
                break;
        case NM_DEVICE_STATE_REASON_INFINIBAND_MODE:
                /* TRANSLATORS: device status reason */
                value = _("InfiniBand device does not support connected mode");
                break;
        case NM_DEVICE_STATE_REASON_DEPENDENCY_FAILED:
                /* TRANSLATORS: device status reason */
                value = _("Connection dependency failed");
                break;
        default:
                /* no StateReason to show */
                value = "";
                break;
        }
        return value;
}
