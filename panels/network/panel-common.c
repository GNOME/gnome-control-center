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

#include <nm-device-ethernet.h>
#include <nm-device-modem.h>
#include <nm-utils.h>

#include "panel-common.h"

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
 * But sort wifi first, since thats the common case.
 **/
const gchar *
panel_device_to_sortable_string (NMDevice *device)
{
        const gchar *value = NULL;
        NMDeviceModemCapabilities caps;
        switch (nm_device_get_device_type (device)) {
        case NM_DEVICE_TYPE_ETHERNET:
                value = "2";
                break;
        case NM_DEVICE_TYPE_WIFI:
                value = "1";
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

gboolean
panel_set_device_widget_details (GtkBuilder *builder,
                                 const gchar *widget_suffix,
                                 const gchar *value)
{
        gchar *heading_id;
        gchar *label_id;
        GtkWidget *heading;
        GtkWidget *widget;

        /* hide the row if there is no value */
        heading_id = g_strdup_printf ("heading_%s", widget_suffix);
        label_id = g_strdup_printf ("label_%s", widget_suffix);
        heading = GTK_WIDGET (gtk_builder_get_object (builder, heading_id));
        widget = GTK_WIDGET (gtk_builder_get_object (builder, label_id));
        if (heading == NULL || widget == NULL) {
                g_critical ("no widgets %s, %s found", heading_id, label_id);
                return FALSE;
        }
        g_free (heading_id);
        g_free (label_id);

        if (value == NULL) {
                gtk_widget_hide (heading);
                gtk_widget_hide (widget);
        } else {
                /* there exists a value */
                gtk_widget_show (heading);
                gtk_widget_show (widget);
                gtk_label_set_label (GTK_LABEL (widget), value);
        }
        return TRUE;
}


gboolean
panel_set_device_widget_header (GtkBuilder *builder,
                                const gchar *widget_suffix,
                                const gchar *heading)
{
        gchar *label_id = NULL;
        GtkWidget *widget;

        label_id = g_strdup_printf ("heading_%s", widget_suffix);
        widget = GTK_WIDGET (gtk_builder_get_object (builder, label_id));
        if (widget == NULL) {
                g_critical ("no widget %s found", label_id);
                return FALSE;
        }
        gtk_label_set_label (GTK_LABEL (widget), heading);
        g_free (label_id);
        return TRUE;
}

static gchar *
get_ipv4_config_address_as_string (NMIP4Config *ip4_config, const char *what)
{
        const GSList *list;
        struct in_addr addr;
        gchar *str = NULL;
        gchar tmp[INET_ADDRSTRLEN];
        NMIP4Address *address;

        /* get address */
        list = nm_ip4_config_get_addresses (ip4_config);
        if (list == NULL)
                goto out;

        /* we only care about one address */
        address = list->data;
        if (!strcmp (what, "address"))
                addr.s_addr = nm_ip4_address_get_address (address);
        else if (!strcmp (what, "gateway"))
                addr.s_addr = nm_ip4_address_get_gateway (address);
        else if (!strcmp (what, "netmask"))
                addr.s_addr = nm_utils_ip4_prefix_to_netmask (nm_ip4_address_get_prefix (address));
        else
                goto out;

        if (!inet_ntop (AF_INET, &addr, tmp, sizeof(tmp)))
                goto out;
        if (g_strcmp0 (tmp, "0.0.0.0") == 0)
                goto out;
        str = g_strdup (tmp);
out:
        return str;
}

static gchar *
get_ipv4_config_name_servers_as_string (NMIP4Config *ip4_config)
{
        const GArray *array;
        GString *dns;
        struct in_addr addr;
        gchar tmp[INET_ADDRSTRLEN];
        int i;
        gchar *str = NULL;

        array = nm_ip4_config_get_nameservers (ip4_config);
        if (array == NULL || array->len == 0)
                goto out;

        dns = g_string_new (NULL);
        for (i = 0; i < array->len; i++) {
                addr.s_addr = g_array_index (array, guint32, i);
                if (inet_ntop (AF_INET, &addr, tmp, sizeof(tmp)))
                        g_string_append_printf (dns, "%s ", tmp);
        }
        str = g_string_free (dns, FALSE);
out:
        return str;
}

static gchar *
get_ipv6_config_address_as_string (NMIP6Config *ip6_config)
{
        const GSList *list;
        const struct in6_addr *addr;
        gchar *str = NULL;
        gchar tmp[INET6_ADDRSTRLEN];
        NMIP6Address *address;

        /* get address */
        list = nm_ip6_config_get_addresses (ip6_config);
        if (list == NULL)
                goto out;

        /* we only care about one address */
        address = list->data;
        addr = nm_ip6_address_get_address (address);
        if (addr == NULL)
                goto out;
        inet_ntop (AF_INET6, addr, tmp, sizeof(tmp));
        str = g_strdup (tmp);
out:
        return str;
}

void
panel_set_device_widgets (GtkBuilder *builder, NMDevice *device)
{
        NMIP4Config *ip4_config = NULL;
        NMIP6Config *ip6_config = NULL;
        gboolean has_ip4;
        gboolean has_ip6;
        gchar *str_tmp;

        /* get IPv4 parameters */
        ip4_config = nm_device_get_ip4_config (device);
        if (ip4_config != NULL) {

                /* IPv4 address */
                str_tmp = get_ipv4_config_address_as_string (ip4_config, "address");
                panel_set_device_widget_details (builder,
                                                 "ipv4",
                                                 str_tmp);
                has_ip4 = str_tmp != NULL;
                g_free (str_tmp);

                /* IPv4 DNS */
                str_tmp = get_ipv4_config_name_servers_as_string (ip4_config);
                panel_set_device_widget_details (builder,
                                                 "dns",
                                                 str_tmp);
                g_free (str_tmp);

                /* IPv4 route */
                str_tmp = get_ipv4_config_address_as_string (ip4_config, "gateway");
                panel_set_device_widget_details (builder,
                                                 "route",
                                                 str_tmp);
                g_free (str_tmp);

        } else {
                /* IPv4 address */
                panel_set_device_widget_details (builder,
                                                 "ipv4",
                                                 NULL);
                has_ip4 = FALSE;

                /* IPv4 DNS */
                panel_set_device_widget_details (builder,
                                                 "dns",
                                                 NULL);

                /* IPv4 route */
                panel_set_device_widget_details (builder,
                                                 "route",
                                                 NULL);
        }

        /* get IPv6 parameters */
        ip6_config = nm_device_get_ip6_config (device);
        if (ip6_config != NULL) {
                str_tmp = get_ipv6_config_address_as_string (ip6_config);
                panel_set_device_widget_details (builder, "ipv6", str_tmp);
                has_ip6 = str_tmp != NULL;
                g_free (str_tmp);
        } else {
                panel_set_device_widget_details (builder, "ipv6", NULL);
                has_ip6 = FALSE;
        }

        if (has_ip4 && has_ip6) {
                panel_set_device_widget_header (builder, "ipv4", _("IPv4 Address"));
                panel_set_device_widget_header (builder, "ipv6", _("IPv6 Address"));
        } else if (has_ip4) {
                panel_set_device_widget_header (builder, "ipv4", _("IP Address"));
        } else if (has_ip6) {
                panel_set_device_widget_header (builder, "ipv6", _("IP Address"));
        }
}

void
panel_unset_device_widgets (GtkBuilder *builder)
{
        panel_set_device_widget_details (builder, "ipv4", NULL);
        panel_set_device_widget_details (builder, "ipv6", NULL);
        panel_set_device_widget_details (builder, "dns", NULL);
        panel_set_device_widget_details (builder, "route", NULL);
}
