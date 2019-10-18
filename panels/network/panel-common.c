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
#define HANDY_USE_UNSTABLE_API
#include <handy.h>
#include <NetworkManager.h>

#include "panel-common.h"

static const gchar *
device_state_to_localized_string (NMDeviceState state)
{
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
                value = _("Unavailable");
                break;
        case NM_DEVICE_STATE_DISCONNECTED:
                value = NULL;
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

static const gchar *
device_state_reason_to_localized_string (NMDevice *device)
{
        const gchar *value = NULL;
        NMDeviceStateReason state_reason;

        /* This only covers NMDeviceStateReasons that explain why a connection
         * failed / can't be attempted, and aren't redundant with the state
         * (eg, NM_DEVICE_STATE_REASON_CARRIER).
         */

        state_reason = nm_device_get_state_reason (device);
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

gchar *
panel_device_status_to_localized_string (NMDevice *nm_device,
                                         const gchar *speed)
{
        NMDeviceState state;
        GString *string;
        const gchar *state_str = NULL, *reason_str = NULL;

        string = g_string_new (NULL);

        state = nm_device_get_state (nm_device);
        if (state == NM_DEVICE_STATE_UNAVAILABLE) {
                if (nm_device_get_firmware_missing (nm_device)) {
                        /* TRANSLATORS: device status */
                        state_str = _("Firmware missing");
                } else if (NM_IS_DEVICE_ETHERNET (nm_device) &&
                           !nm_device_ethernet_get_carrier (NM_DEVICE_ETHERNET (nm_device))) {
                        /* TRANSLATORS: device status */
                        state_str = _("Cable unplugged");
                }
        }
        if (!state_str)
                state_str = device_state_to_localized_string (state);
        if (state_str)
                g_string_append (string, state_str);

        if (state > NM_DEVICE_STATE_UNAVAILABLE && speed) {
                if (string->len)
                        g_string_append (string, " - ");
                g_string_append (string, speed);
        } else if (state == NM_DEVICE_STATE_UNAVAILABLE ||
                   state == NM_DEVICE_STATE_DISCONNECTED ||
                   state == NM_DEVICE_STATE_DEACTIVATING ||
                   state == NM_DEVICE_STATE_FAILED) {
                reason_str = device_state_reason_to_localized_string (nm_device);
                if (*reason_str) {
                        if (string->len)
                                g_string_append (string, " - ");
                        g_string_append (string, reason_str);
                }
        }

        return g_string_free (string, FALSE);
}

static gboolean
panel_set_device_widget_details (GtkLabel *heading,
                                 GtkLabel *widget,
                                 const gchar *value)
{
        /* hide the row if there is no value */
        if (value == NULL) {
                gtk_widget_hide (GTK_WIDGET (heading));
                gtk_widget_hide (GTK_WIDGET (widget));
        } else {
                /* there exists a value */
                gtk_widget_show (GTK_WIDGET (heading));
                gtk_widget_show (GTK_WIDGET (widget));
                gtk_label_set_label (widget, value);
        }
        return TRUE;
}

gchar *
panel_get_ip4_address_as_string (NMIPConfig *ip4_config, const char *what)
{
        /* we only care about one address */
        if (!strcmp (what, "address")) {
                GPtrArray *array;
                NMIPAddress *address;

                array = nm_ip_config_get_addresses (ip4_config);
                if (array->len < 1)
                        return NULL;
                address = array->pdata[0];
                return g_strdup (nm_ip_address_get_address (address));
        } else if (!strcmp (what, "gateway")) {
                return g_strdup (nm_ip_config_get_gateway (ip4_config));
        }

        return NULL;
}

gchar *
panel_get_ip4_dns_as_string (NMIPConfig *ip4_config)
{
        return g_strjoinv (" ",
                           (char **) nm_ip_config_get_nameservers (ip4_config));
}

gchar *
panel_get_ip6_address_as_string (NMIPConfig *ip6_config)
{
        GPtrArray *array;
        NMIPAddress *address;

        array = nm_ip_config_get_addresses (ip6_config);
        if (array->len < 1)
                return NULL;
        address = array->pdata[0];
        return g_strdup (nm_ip_address_get_address (address));
}

void
panel_set_device_widgets (GtkLabel *ipv4_heading_label, GtkLabel *ipv4_label,
                          GtkLabel *ipv6_heading_label, GtkLabel *ipv6_label,
                          GtkLabel *heading_dns, GtkLabel *dns_label,
                          GtkLabel *route_heading_label, GtkLabel *route_label,
                          NMDevice *device)
{
        g_autofree gchar *ipv4_text = NULL;
        g_autofree gchar *ipv6_text = NULL;
        g_autofree gchar *dns_text = NULL;
        g_autofree gchar *route_text = NULL;
        gboolean has_ip4, has_ip6;

        if (device != NULL) {
                NMIPConfig *ip4_config, *ip6_config;

                ip4_config = nm_device_get_ip4_config (device);
                if (ip4_config != NULL) {
                        ipv4_text = panel_get_ip4_address_as_string (ip4_config, "address");
                        dns_text = panel_get_ip4_dns_as_string (ip4_config);
                        route_text = panel_get_ip4_address_as_string (ip4_config, "gateway");
                }
                ip6_config = nm_device_get_ip6_config (device);
                if (ip6_config != NULL)
                        ipv6_text = panel_get_ip6_address_as_string (ip6_config);
        }

        panel_set_device_widget_details (ipv4_heading_label, ipv4_label, ipv4_text);
        panel_set_device_widget_details (ipv6_heading_label, ipv6_label, ipv6_text);
        panel_set_device_widget_details (heading_dns, dns_label, dns_text);
        panel_set_device_widget_details (route_heading_label, route_label, route_text);

        has_ip4 = ipv4_text != NULL;
        has_ip6 = ipv6_text != NULL;
        if (has_ip4 && has_ip6) {
                gtk_label_set_label (ipv4_heading_label, _("IPv4 Address"));
                gtk_label_set_label (ipv6_heading_label, _("IPv6 Address"));
        } else if (has_ip4) {
                gtk_label_set_label (ipv4_heading_label, _("IP Address"));
        } else if (has_ip6) {
                gtk_label_set_label (ipv6_heading_label, _("IP Address"));
        }
}
