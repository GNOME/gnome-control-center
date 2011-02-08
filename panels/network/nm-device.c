/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2010-2011 Richard Hughes <richard@hughsie.com>
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

#include <glib-object.h>
#include <gio/gio.h>
#include <glib/gi18n.h>

#include "nm-device.h"
#include "nm-access-point.h"

#define NM_DEVICE_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), NM_TYPE_DEVICE, NmDevicePrivate))

/**
 * NmDevicePrivate:
 *
 * Private #NmDevice data
 **/
struct _NmDevicePrivate
{
        GCancellable                        *cancellable;
        gchar                           *active_access_point;
        gchar                           *dhcp4_config;
        gchar                           *ip4_config;
        gchar                           *ip4_address;
        gchar                           *ip4_nameserver;
        gchar                           *ip4_route;
        gchar                           *ip4_subnet_mask;
        gchar                           *ip6_config;
        gchar                           *ip6_address;
        gchar                           *ip6_nameserver;
        gchar                           *ip6_route;
        gchar                           *mac_address;
        gchar                           *modem_imei;
        gchar                                *object_path;
        gchar                           *operator_name;
        gchar                           *speed;
        gchar                           *udi;
        GPtrArray                       *access_points;
        NmDeviceKind                         kind;
        NmDeviceState                         state;
        GDBusProxy                      *proxy;
        GDBusProxy                      *proxy_additional;
        GDBusProxy                      *proxy_dhcp4;
        GDBusProxy                      *proxy_ip4;
        GDBusProxy                      *proxy_ip6;
        guint                            device_add_refcount;
};

enum {
        SIGNAL_READY,
        SIGNAL_CHANGED,
        SIGNAL_LAST
};

enum {
        PROP_0,
        PROP_LAST
};

static guint signals[SIGNAL_LAST] = { 0 };
G_DEFINE_TYPE (NmDevice, nm_device, G_TYPE_OBJECT)

/**
 * nm_device_kind_to_icon_name:
 **/
const gchar *
nm_device_kind_to_icon_name (NmDeviceKind type)
{
        const gchar *value = NULL;
        switch (type) {
        case NM_DEVICE_KIND_ETHERNET:
                value = "network-wired";
                break;
        case NM_DEVICE_KIND_WIFI:
        case NM_DEVICE_KIND_GSM:
        case NM_DEVICE_KIND_CDMA:
        case NM_DEVICE_KIND_BLUETOOTH:
        case NM_DEVICE_KIND_MESH:
                value = "network-wireless";
                break;
        default:
                break;
        }
        return value;
}

/**
 * nm_device_kind_to_localized_string:
 **/
const gchar *
nm_device_kind_to_localized_string (NmDeviceKind type)
{
        const gchar *value = NULL;
        switch (type) {
        case NM_DEVICE_KIND_UNKNOWN:
                /* TRANSLATORS: device type */
                value = _("Unknown");
                break;
        case NM_DEVICE_KIND_ETHERNET:
                /* TRANSLATORS: device type */
                value = _("Wired");
                break;
        case NM_DEVICE_KIND_WIFI:
                /* TRANSLATORS: device type */
                value = _("Wireless");
                break;
        case NM_DEVICE_KIND_GSM:
        case NM_DEVICE_KIND_CDMA:
                /* TRANSLATORS: device type */
                value = _("Mobile broadband");
                break;
        case NM_DEVICE_KIND_BLUETOOTH:
                /* TRANSLATORS: device type */
                value = _("Bluetooth");
                break;
        case NM_DEVICE_KIND_MESH:
                /* TRANSLATORS: device type */
                value = _("Mesh");
                break;

        default:
                break;
        }
        return value;
}

/**
 * nm_device_kind_to_sortable_string:
 *
 * Try to return order of approximate connection speed.
 **/
const gchar *
nm_device_kind_to_sortable_string (NmDeviceKind type)
{
        const gchar *value = NULL;
        switch (type) {
        case NM_DEVICE_KIND_ETHERNET:
                value = "1";
                break;
        case NM_DEVICE_KIND_WIFI:
                value = "2";
                break;
        case NM_DEVICE_KIND_GSM:
        case NM_DEVICE_KIND_CDMA:
                value = "3";
                break;
        case NM_DEVICE_KIND_BLUETOOTH:
                value = "4";
                break;
        case NM_DEVICE_KIND_MESH:
                value = "5";
                break;
        default:
                value = "6";
                break;
        }
        return value;
}

/**
 * nm_device_state_to_localized_string:
 **/
const gchar *
nm_device_state_to_localized_string (NmDeviceState type)
{
        const gchar *value = NULL;
        switch (type) {
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
                /* TRANSLATORS: device status */
                value = _("Disconnected");
                break;
        case NM_DEVICE_STATE_PREPARE:
                /* TRANSLATORS: device status */
                value = _("Preparing connection");
                break;
        case NM_DEVICE_STATE_CONFIG:
                /* TRANSLATORS: device status */
                value = _("Configuring connection");
                break;
        case NM_DEVICE_STATE_NEED_AUTH:
                /* TRANSLATORS: device status */
                value = _("Authenticating");
                break;
        case NM_DEVICE_STATE_IP_CONFIG:
                /* TRANSLATORS: device status */
                value = _("Getting network address");
                break;
        case NM_DEVICE_STATE_ACTIVATED:
                /* TRANSLATORS: device status */
                value = _("Connected");
                break;
        case NM_DEVICE_STATE_FAILED:
                /* TRANSLATORS: device status */
                value = _("Failed to connect");
                break;
        default:
                break;
        }
        return value;
}

/**
 * nm_device_get_object_path:
 **/
const gchar *
nm_device_get_object_path (NmDevice *device)
{
        g_return_val_if_fail (NM_IS_DEVICE (device), NULL);
        return device->priv->object_path;
}

/**
 * nm_device_get_object_path:
 **/
GPtrArray *
nm_device_get_access_points (NmDevice *device)
{
        g_return_val_if_fail (NM_IS_DEVICE (device), NULL);
        return g_ptr_array_ref (device->priv->access_points);
}

/**
 * nm_device_get_active_access_point:
 **/
const gchar *
nm_device_get_active_access_point (NmDevice *device)
{
        g_return_val_if_fail (NM_IS_DEVICE (device), NULL);
        return device->priv->active_access_point;
}

/**
 * nm_device_get_from_options:
 **/
static const gchar *
nm_device_get_from_options (GVariant *variant, const gchar *key)
{
        const gchar *prop_key;
        const gchar *value = NULL;
        GVariantIter *iter = NULL;
        GVariant *prop_value;

        /* insert the new metadata */
        g_variant_get (variant, "a{sv}",
                       &iter);
        if (iter == NULL)
                goto out;
        while (g_variant_iter_loop (iter, "{sv}",
                                    &prop_key, &prop_value)) {
                if (g_strcmp0 (prop_key, key) == 0) {
                        value = g_variant_get_string (prop_value, NULL);
                        break;
                }
        }
out:
        return value;
}

/**
 * nm_device_ipv6_to_string:
 *
 * Formats an 'ay' variant into a IPv6 address you recognise, e.g.
 * "fe80::21c:bfff:fe81:e8de"
 **/
static gchar *
nm_device_ipv6_to_string (GVariant *variant)
{
        gchar tmp1;
        gchar tmp2;
        guint i = 0;
        gboolean ret = FALSE;
        GString *string;

        if (g_variant_n_children (variant) != 16)
                return NULL;

        string = g_string_new ("");
        for (i=0; i<16; i+=2) {
                g_variant_get_child (variant, i+0, "y", &tmp1);
                g_variant_get_child (variant, i+1, "y", &tmp2);
                if (tmp1 == 0 && tmp2 == 0) {
                        if (!ret) {
                                g_string_append (string, ":");
                                ret = TRUE;
                        }
                } else {
                        g_string_append_printf (string,
                                                "%x%x%x%x:",
                                                (tmp1 & 0xf0) / 16,
                                                 tmp1 & 0x0f,
                                                (tmp2 & 0xf0) / 16,
                                                 tmp2 & 0x0f);
                        ret = FALSE;
                }
        }
        g_string_set_size (string, string->len - 1);
        return g_string_free (string, FALSE);
}

/**
 * nm_device_ipv6_prefixed_array_to_string:
 *
 * This is some crazy shit. NM sends us the following data type:
 * "array of [struct of (array of [byte], uint32, array of [byte])]"
 **/
static gchar *
nm_device_ipv6_prefixed_array_to_string (GVariant *variant)
{
        GString *string;
        gchar *tmp;
        GVariant *outer;
        GVariantIter iter;
        gsize len;
        GVariant *address;
        guint32 prefix;

        string = g_string_new ("");

        /* get an iter of the outer array */
        len = g_variant_iter_init (&iter, variant);
        if (len == 0) {
                g_debug ("no ipv6 address");
                goto out;
        }

        /* unwrap the outer array */
        outer = g_variant_iter_next_value (&iter);
        while (outer != NULL) {

                /* format the address and add to the string */
                address = g_variant_get_child_value (outer, 0);
                tmp = nm_device_ipv6_to_string (address);
                g_variant_get_child (outer, 1, "u", &prefix);
                g_string_append_printf (string, "%s/%i, ", tmp, prefix);

                outer = g_variant_iter_next_value (&iter);
        }

        /* remove trailing space comma */
        if (string->len > 2)
                g_string_set_size (string, string->len - 2);
out:
        return g_string_free (string, FALSE);
}

/**
 * nm_device_ipv6_array_to_string:
 *
 * NM sends us the following data type:
 * "array of [array of (byte)]"
 **/
static gchar *
nm_device_ipv6_array_to_string (GVariant *variant)
{
        GString *string;
        gchar *tmp;
        GVariantIter iter;
        gsize len;
        GVariant *address;

        string = g_string_new ("");

        /* get an iter of the outer array */
        len = g_variant_iter_init (&iter, variant);
        if (len == 0) {
                g_debug ("no ipv6 address");
                goto out;
        }

        /* unwrap the outer array */
        address = g_variant_iter_next_value (&iter);
        while (address != NULL) {

                /* format the address and add to the string */
                tmp = nm_device_ipv6_to_string (address);
                g_string_append_printf (string, "%s, ", tmp);

                address = g_variant_iter_next_value (&iter);
        }

        /* remove trailing space comma */
        if (string->len > 2)
                g_string_set_size (string, string->len - 2);
out:
        return g_string_free (string, FALSE);
}

/**
 * nm_device_get_ip6_address:
 **/
const gchar *
nm_device_get_ip6_address (NmDevice *device)
{
        GVariant *value = NULL;

        g_return_val_if_fail (NM_IS_DEVICE (device), NULL);

        /* invalidate */
        g_free (device->priv->ip6_address);
        device->priv->ip6_address = NULL;

        /* array of (ipdata, prefix, route) */
        if (device->priv->proxy_ip6 == NULL)
                goto out;
        value = g_dbus_proxy_get_cached_property (device->priv->proxy_ip6,
                                                  "Addresses");
        device->priv->ip6_address = nm_device_ipv6_prefixed_array_to_string (value);
out:
        if (value != NULL)
                g_variant_unref (value);
        return device->priv->ip6_address;
}

/**
 * nm_device_get_ip6_nameserver:
 **/
const gchar *
nm_device_get_ip6_nameserver (NmDevice *device)
{
        GVariant *value = NULL;

        g_return_val_if_fail (NM_IS_DEVICE (device), NULL);

        /* invalidate */
        g_free (device->priv->ip6_nameserver);
        device->priv->ip6_nameserver = NULL;

        /* array of ipdata */
        if (device->priv->proxy_ip6 == NULL)
                goto out;
        value = g_dbus_proxy_get_cached_property (device->priv->proxy_ip6,
                                                  "Nameservers");
        device->priv->ip6_nameserver = nm_device_ipv6_array_to_string (value);
out:
        if (value != NULL)
                g_variant_unref (value);
        return device->priv->ip6_nameserver;
}

/**
 * nm_device_get_ip6_route:
 **/
const gchar *
nm_device_get_ip6_route (NmDevice *device)
{
        GVariant *value = NULL;

        g_return_val_if_fail (NM_IS_DEVICE (device), NULL);

        /* invalidate */
        g_free (device->priv->ip6_route);
        device->priv->ip6_route = NULL;

        /* array of (ipdata, prefix, route) */
        if (device->priv->proxy_ip6 == NULL)
                goto out;
        value = g_dbus_proxy_get_cached_property (device->priv->proxy_ip6,
                                                  "Routes");
        device->priv->ip6_route = nm_device_ipv6_prefixed_array_to_string (value);
out:
        if (value != NULL)
                g_variant_unref (value);
        return device->priv->ip6_route;
}

/**
 * nm_device_ipv4_to_string:
 **/
static gchar *
nm_device_ipv4_to_string (GVariant *variant)
{
        gchar *ip_str;
        guint32 ip;

        g_variant_get (variant, "u", &ip);
        ip_str = g_strdup_printf ("%i.%i.%i.%i",
                                    ip & 0x000000ff,
                                   (ip & 0x0000ff00) / 0x100,
                                   (ip & 0x00ff0000) / 0x10000,
                                   (ip & 0xff000000) / 0x1000000);
        return ip_str;
}

/**
 * nm_device_ipv4_array_to_string_array:
 *
 * This is some crazy shit. NM sends us the following data type:
 * "array of [array of [uint32]]"
 **/
static gchar *
nm_device_ipv4_array_to_string_array (GVariant *variant)
{
        gchar *tmp;
        gsize len;
        GString *string;
        guint i;
        GVariantIter iter;
        GVariant *outer;
        GVariant *value;

        string = g_string_new ("");

        /* get an iter of the outer array */
        len = g_variant_iter_init (&iter, variant);

        /* unwrap the outer array */
        outer = g_variant_iter_next_value (&iter);
        while (outer != NULL) {

                /* unwrap the inner array */
                len = g_variant_n_children (outer);
                if (len == 0) {
                        g_warning ("invalid ipv4 address on inner?!");
                        goto out;
                }
                for (i=0; i<len; i++) {
                        value = g_variant_get_child_value (outer, i);
                        tmp = nm_device_ipv4_to_string (value);

                        /* ignore invalid entries: TODO why? */
                        if (g_str_has_suffix (tmp, ".0")) {
                                g_debug ("ignoring IP %s", tmp);
                        } else {
                                g_debug ("got IP %s", tmp);
                                g_string_append_printf (string,
                                                        "%s, ",
                                                        tmp);
                        }
                        g_free (tmp);
                        g_variant_unref (value);
                }
                outer = g_variant_iter_next_value (&iter);
        }

        /* remove trailing space comma */
        if (string->len > 2)
                g_string_set_size (string, string->len - 2);
out:
        return g_string_free (string, FALSE);
}

/**
 * nm_device_ipv4_array_to_string:
 *
 * TNM sends us the following data type "array of [uint32]"
 **/
static gchar *
nm_device_ipv4_array_to_string (GVariant *variant)
{
        gchar *tmp;
        gsize len;
        GString *string;
        guint i;
        GVariant *value;

        string = g_string_new ("");

        /* unwrap the array */
        len = g_variant_n_children (variant);
        if (len == 0) {
                g_warning ("invalid ipv4 address on inner?!");
                goto out;
        }
        for (i=0; i<len; i++) {
                value = g_variant_get_child_value (variant, i);
                tmp = nm_device_ipv4_to_string (value);

                /* ignore invalid entries: TODO why? */
                if (g_str_has_suffix (tmp, ".0")) {
                        g_debug ("ignoring IP %s", tmp);
                } else {
                        g_debug ("got IP %s", tmp);
                        g_string_append_printf (string,
                                                "%s, ",
                                                tmp);
                }
                g_free (tmp);
                g_variant_unref (value);
        }

        /* remove trailing space comma */
        if (string->len > 2)
                g_string_set_size (string, string->len - 2);
out:
        return g_string_free (string, FALSE);
}

/**
 * nm_device_get_ip4_address:
 **/
const gchar *
nm_device_get_ip4_address (NmDevice *device)
{
        const gchar *tmp;
        GVariant *options = NULL;
        GVariant *value = NULL;

        g_return_val_if_fail (NM_IS_DEVICE (device), NULL);

        /* invalidate */
        g_free (device->priv->ip4_address);
        device->priv->ip4_address = NULL;

        /* set from DHCPv4 */
        if (device->priv->proxy_dhcp4 != NULL) {
                options = g_dbus_proxy_get_cached_property (device->priv->proxy_dhcp4,
                                                            "Options");
                if (options != NULL) {
                        tmp = nm_device_get_from_options (options,
                                                          "ip_address");
                        device->priv->ip4_address = g_strdup (tmp);
                        goto out;
                }
        }

        /* set IPv4 */
        if (device->priv->proxy_ip4 != NULL) {
                /* array of (array of uint32) */
                value = g_dbus_proxy_get_cached_property (device->priv->proxy_ip4,
                                                          "Addresses");
                device->priv->ip4_address = nm_device_ipv4_array_to_string_array (value);
                goto out;
        }

out:
        if (value != NULL)
                g_variant_unref (value);
        if (options != NULL)
                g_variant_unref (options);
        return device->priv->ip4_address;
}

/**
 * nm_device_get_ip4_nameserver:
 **/
const gchar *
nm_device_get_ip4_nameserver (NmDevice *device)
{
        const gchar *tmp;
        GVariant *options = NULL;
        GVariant *value = NULL;

        g_return_val_if_fail (NM_IS_DEVICE (device), NULL);

        /* invalidate */
        g_free (device->priv->ip4_nameserver);
        device->priv->ip4_nameserver = NULL;

        /* set from DHCPv4 */
        if (device->priv->proxy_dhcp4 != NULL) {
                options = g_dbus_proxy_get_cached_property (device->priv->proxy_dhcp4,
                                                            "Options");
                if (options != NULL) {
                        tmp = nm_device_get_from_options (options,
                                                          "domain_name_servers");
                        device->priv->ip4_nameserver = g_strdup (tmp);
                        goto out;
                }
        }

        /* set IPv4 */
        if (device->priv->proxy_ip4 != NULL) {
                /* array of uint32 */
                value = g_dbus_proxy_get_cached_property (device->priv->proxy_ip4,
                                                                "Nameservers");
                device->priv->ip4_nameserver = nm_device_ipv4_array_to_string (value);
        }
out:
        if (value != NULL)
                g_variant_unref (value);
        if (options != NULL)
                g_variant_unref (options);
        return device->priv->ip4_nameserver;
}

/**
 * nm_device_get_ip4_route:
 **/
const gchar *
nm_device_get_ip4_route (NmDevice *device)
{
        const gchar *tmp;
        GVariant *options = NULL;
        GVariant *value = NULL;

        g_return_val_if_fail (NM_IS_DEVICE (device), NULL);

        /* invalidate */
        g_free (device->priv->ip4_route);
        device->priv->ip4_route = NULL;

        /* set from DHCPv4 */
        if (device->priv->proxy_dhcp4 != NULL) {
                options = g_dbus_proxy_get_cached_property (device->priv->proxy_dhcp4,
                                                            "Options");
                if (options != NULL) {
                        tmp = nm_device_get_from_options (options,
                                                          "routers");
                        device->priv->ip4_route = g_strdup (tmp);
                        goto out;
                }
        }

        /* set IPv4 */
        if (device->priv->proxy_ip4 != NULL) {
                /* array of (array of uint32) */
                value = g_dbus_proxy_get_cached_property (device->priv->proxy_ip4,
                                                           "Routes");
                device->priv->ip4_route = nm_device_ipv4_array_to_string_array (value);
        }
out:
        if (options != NULL)
                g_variant_unref (options);
        if (value != NULL)
                g_variant_unref (value);
        return device->priv->ip4_route;
}

/**
 * nm_device_get_ip4_subnet_mask:
 **/
const gchar *
nm_device_get_ip4_subnet_mask (NmDevice *device)
{
        const gchar *tmp;
        GVariant *options = NULL;

        g_return_val_if_fail (NM_IS_DEVICE (device), NULL);

        /* invalidate */
        g_free (device->priv->ip4_subnet_mask);
        device->priv->ip4_subnet_mask = NULL;

        /* set from DHCPv4 */
        if (device->priv->proxy_dhcp4 != NULL) {
                options = g_dbus_proxy_get_cached_property (device->priv->proxy_dhcp4,
                                                            "Options");
                if (options != NULL) {
                        tmp = nm_device_get_from_options (options,
                                                          "subnet_mask");
                        device->priv->ip4_subnet_mask = g_strdup (tmp);
                        goto out;
                }
        }
out:
        if (options != NULL)
                g_variant_unref (options);
        return device->priv->ip4_subnet_mask;
}

/**
 * nm_device_get_mac_address:
 **/
const gchar *
nm_device_get_mac_address (NmDevice *device)
{
        GVariant *value;

        g_return_val_if_fail (NM_IS_DEVICE (device), NULL);

        /* invalidate */
        g_free (device->priv->mac_address);
        device->priv->mac_address = NULL;

        /* get HwAddress */
        value = g_dbus_proxy_get_cached_property (device->priv->proxy_additional,
                                                  "HwAddress");
        device->priv->mac_address = g_variant_dup_string (value, NULL);
        g_variant_unref (value);
        return device->priv->mac_address;
}

/**
 * nm_device_get_modem_imei:
 **/
const gchar *
nm_device_get_modem_imei (NmDevice *device)
{
        g_return_val_if_fail (NM_IS_DEVICE (device), NULL);
        return device->priv->modem_imei;
}

/**
 * nm_device_get_operator_name:
 **/
const gchar *
nm_device_get_operator_name (NmDevice *device)
{
        g_return_val_if_fail (NM_IS_DEVICE (device), NULL);
        return device->priv->operator_name;
}

/**
 * nm_device_value_to_string_bitrate:
 **/
static gchar *
nm_device_value_to_string_bitrate (GVariant *variant)
{
        guint bitrate;
        gchar *tmp;

        /* format with correct scale */
        g_variant_get (variant, "u", &bitrate);
        if (bitrate < 1000) {
                tmp = g_strdup_printf (_("%i kb/s"), bitrate);
        } else {
                tmp = g_strdup_printf (_("%i Mb/s"), bitrate / 1000);
        }
        return tmp;
}

/**
 * nm_device_value_to_string_speed:
 **/
static gchar *
nm_device_value_to_string_speed (GVariant *variant)
{
        guint speed;
        gchar *tmp;

        /* format with correct scale */
        g_variant_get (variant, "u", &speed);
        if (speed < 1000) {
                tmp = g_strdup_printf (_("%i Mb/s"), speed);
        } else {
                tmp = g_strdup_printf (_("%i Gb/s"), speed / 1000);
        }
        return tmp;
}

/**
 * nm_device_get_speed:
 **/
const gchar *
nm_device_get_speed (NmDevice *device)
{
        GVariant *value;

        g_return_val_if_fail (NM_IS_DEVICE (device), NULL);

        /* invalidate */
        g_free (device->priv->speed);
        device->priv->speed = NULL;

        value = g_dbus_proxy_get_cached_property (device->priv->proxy_additional, "Speed");
        if (value != NULL) {
                device->priv->speed = nm_device_value_to_string_speed (value);
                goto out;
        }
        value = g_dbus_proxy_get_cached_property (device->priv->proxy_additional, "Bitrate");
        if (value != NULL) {
                device->priv->speed = nm_device_value_to_string_bitrate (value);
                goto out;
        }
out:
        return device->priv->speed;
}

/**
 * nm_device_get_kind:
 **/
NmDeviceKind
nm_device_get_kind (NmDevice *device)
{
        g_return_val_if_fail (NM_IS_DEVICE (device), 0);
        return device->priv->kind;
}

/**
 * nm_device_get_state:
 **/
NmDeviceState
nm_device_get_state (NmDevice *device)
{
        GVariant *variant_state;
        g_return_val_if_fail (NM_IS_DEVICE (device), 0);

        variant_state = g_dbus_proxy_get_cached_property (device->priv->proxy,
                                                          "State");
        g_variant_get (variant_state, "u", &device->priv->state);
        g_variant_unref (variant_state);

        return device->priv->state;
}

/**
 * nm_device_emit_ready:
 **/
static void
nm_device_emit_ready (NmDevice *device)
{
        g_debug ("NmDevice: emit 'ready' for %s",
                 device->priv->object_path);
        g_signal_emit (device, signals[SIGNAL_READY], 0);
}

/**
 * nm_device_emit_changed:
 **/
static void
nm_device_emit_changed (NmDevice *device)
{
        g_debug ("NmDevice: emit 'changed' for %s",
                 device->priv->object_path);
        g_signal_emit (device, signals[SIGNAL_CHANGED], 0);
}

/**
 * nm_device_get_active_access_point_data:
 **/
static void
nm_device_get_active_access_point_data (NmDevice *device, const gchar *access_point_id)
{
        NmAccessPoint *access_point;
        access_point = nm_access_point_new ();
        g_ptr_array_add (device->priv->access_points, access_point);
        nm_access_point_refresh (access_point,
                                 access_point_id,
                                 device->priv->cancellable);
}

/**
 * nm_device_get_access_points_cb:
 **/
static void
nm_device_get_access_points_cb (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
        const gchar *object_path;
        GError *error = NULL;
        gsize len;
        GVariantIter iter;
        GVariant *result = NULL;
        GVariant *test;
        NmDevice *device = (NmDevice *) user_data;

        result = g_dbus_proxy_call_finish (G_DBUS_PROXY (source_object), res, &error);
        if (result == NULL) {
                g_printerr ("Error getting access points: %s\n", error->message);
                g_error_free (error);
                return;
        }

        /* clear list of access points */
        g_ptr_array_set_size (device->priv->access_points, 0);

        test = g_variant_get_child_value (result, 0);
        len = g_variant_iter_init (&iter, test);
        if (len == 0) {
                g_warning ("no access points?!");
                goto out;
        }

        /* for each entry in the array */
        while (g_variant_iter_loop (&iter, "o", &object_path)) {
                g_debug ("adding access point %s", object_path);
                nm_device_get_active_access_point_data (device, object_path);
        }

        /* emit */
        nm_device_emit_changed (device);
out:
        g_variant_unref (result);
        g_variant_unref (test);
}

/**
 * nm_device_get_registration_info_cb:
 **/
static void
nm_device_get_registration_info_cb (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
        gchar *operator_code = NULL;
        GError *error = NULL;
        guint registration_status;
        GVariant *result = NULL;
        NmDevice *device = (NmDevice *) user_data;

        result = g_dbus_proxy_call_finish (G_DBUS_PROXY (source_object), res, &error);
        if (result == NULL) {
                g_printerr ("Error getting registration info: %s\n", error->message);
                g_error_free (error);
                return;
        }

        /* get values */
        g_variant_get (result, "((uss))",
                       &registration_status,
                       &operator_code,
                       &device->priv->operator_name);

        g_free (operator_code);
        g_variant_unref (result);
}

/**
 * nm_device_got_device_proxy_modem_manager_gsm_network_cb:
 **/
static void
nm_device_got_device_proxy_modem_manager_gsm_network_cb (GObject *source_object,
                                                     GAsyncResult *res,
                                                     gpointer user_data)
{
        GError *error = NULL;
        GVariant *result = NULL;
        NmDevice *device = (NmDevice *) user_data;

        device->priv->proxy_additional = g_dbus_proxy_new_for_bus_finish (res, &error);
        if (device->priv->proxy_additional == NULL) {
                g_printerr ("Error creating additional proxy: %s\n", error->message);
                g_error_free (error);
                goto out;
        }

        /* get the currently active access point */
        result = g_dbus_proxy_get_cached_property (device->priv->proxy_additional, "AccessTechnology");
//      device->priv->active_access_point = g_variant_dup_string (result, NULL);

        g_dbus_proxy_call (device->priv->proxy_additional,
                           "GetRegistrationInfo",
                           NULL,
                           G_DBUS_CALL_FLAGS_NONE,
                           -1,
                           device->priv->cancellable,
                           nm_device_get_registration_info_cb,
                           device);

        /* add device if there are no more pending actions */
        if (--device->priv->device_add_refcount == 0)
                nm_device_emit_ready (device);
out:
        if (result != NULL)
                g_variant_unref (result);
        return;
}

/**
 * nm_device_got_device_proxy_modem_manager_cb:
 **/
static void
nm_device_got_device_proxy_modem_manager_cb (GObject *source_object,
                                         GAsyncResult *res,
                                         gpointer user_data)
{
        GError *error = NULL;
        GVariant *result = NULL;
        NmDevice *device = (NmDevice *) user_data;

        device->priv->proxy_additional = g_dbus_proxy_new_for_bus_finish (res, &error);
        if (device->priv->proxy_additional == NULL) {
                g_printerr ("Error creating additional proxy: %s\n", error->message);
                g_error_free (error);
                goto out;
        }

        /* get the IMEI */
        result = g_dbus_proxy_get_cached_property (device->priv->proxy_additional,
                                                   "EquipmentIdentifier");
        device->priv->modem_imei = g_variant_dup_string (result, NULL);

        /* add device if there are no more pending actions */
        if (--device->priv->device_add_refcount == 0)
                nm_device_emit_ready (device);
out:
        if (result != NULL)
                g_variant_unref (result);
        return;
}

/**
 * nm_device_got_device_proxy_additional_cb:
 **/
static void
nm_device_got_device_proxy_additional_cb (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
        GError *error = NULL;
        GVariant *result = NULL;
        NmDevice *device = (NmDevice *) user_data;

        device->priv->proxy_additional = g_dbus_proxy_new_for_bus_finish (res, &error);
        if (device->priv->proxy_additional == NULL) {
                g_printerr ("Error creating additional proxy: %s\n", error->message);
                g_error_free (error);
                goto out;
        }

        /* async populate the list of access points */
        if (device->priv->kind == NM_DEVICE_KIND_WIFI) {

                /* get the currently active access point */
                result = g_dbus_proxy_get_cached_property (device->priv->proxy_additional, "ActiveAccessPoint");
                device->priv->active_access_point = g_variant_dup_string (result, NULL);

                g_dbus_proxy_call (device->priv->proxy_additional,
                                   "GetAccessPoints",
                                   NULL,
                                   G_DBUS_CALL_FLAGS_NONE,
                                   -1,
                                   device->priv->cancellable,
                                   nm_device_get_access_points_cb,
                                   device);
        }

        /* add device if there are no more pending actions */
        if (--device->priv->device_add_refcount == 0)
                nm_device_emit_ready (device);
out:
        if (result != NULL)
                g_variant_unref (result);
        return;
}

/**
 * nm_device_got_device_proxy_ip4_cb:
 **/
static void
nm_device_got_device_proxy_ip4_cb (GObject *source_object,
                               GAsyncResult *res,
                               gpointer user_data)
{
        GError *error = NULL;
        NmDevice *device = (NmDevice *) user_data;

        device->priv->proxy_ip4 = g_dbus_proxy_new_for_bus_finish (res, &error);
        if (device->priv->proxy_ip4 == NULL) {
                g_printerr ("Error creating ip4 proxy: %s\n", error->message);
                g_error_free (error);
                goto out;
        }

        /* add device if there are no more pending actions */
        if (--device->priv->device_add_refcount == 0)
                nm_device_emit_ready (device);
out:
        return;
}

/**
 * nm_device_got_device_proxy_dhcp4_cb:
 **/
static void
nm_device_got_device_proxy_dhcp4_cb (GObject *source_object,
                                 GAsyncResult *res,
                                 gpointer user_data)
{
        GError *error = NULL;
        NmDevice *device = (NmDevice *) user_data;

        device->priv->proxy_dhcp4 = g_dbus_proxy_new_for_bus_finish (res, &error);
        if (device->priv->proxy_dhcp4 == NULL) {
                g_printerr ("Error creating dhcp4 proxy: %s\n", error->message);
                g_error_free (error);
                goto out;
        }

        /* add device if there are no more pending actions */
        if (--device->priv->device_add_refcount == 0)
                nm_device_emit_ready (device);
out:
        return;
}

/**
 * nm_device_got_device_proxy_ip6_cb:
 **/
static void
nm_device_got_device_proxy_ip6_cb (GObject *source_object,
                               GAsyncResult *res,
                               gpointer user_data)
{
        GError *error = NULL;
        NmDevice *device = (NmDevice *) user_data;

        device->priv->proxy_ip6 = g_dbus_proxy_new_for_bus_finish (res, &error);
        if (device->priv->proxy_ip6 == NULL) {
                g_printerr ("Error creating ip6 proxy: %s\n", error->message);
                g_error_free (error);
                goto out;
        }

        /* add device if there are no more pending actions */
        if (--device->priv->device_add_refcount == 0)
                nm_device_emit_ready (device);
out:
        return;
}

/**
 * nm_device_properties_changed_cb:
 **/
static void
nm_device_properties_changed_cb (GDBusProxy *proxy,
                                 GVariant *changed_properties,
                                 const gchar* const *invalidated_properties,
                                 gpointer user_data)
{
        NmDevice *device = (NmDevice *) user_data;
        nm_device_emit_changed (device);
}

/**
 * nm_device_got_device_proxy_cb:
 **/
static void
nm_device_got_device_proxy_cb (GObject *source_object,
                           GAsyncResult *res,
                           gpointer user_data)
{
        GError *error = NULL;
        GVariant *variant_ip4 = NULL;
        GVariant *variant_dhcp4 = NULL;
        GVariant *variant_ip6 = NULL;
        GVariant *variant_type = NULL;
        GVariant *variant_udi = NULL;
        NmDevice *device = (NmDevice *) user_data;

        device->priv->proxy = g_dbus_proxy_new_for_bus_finish (res, &error);
        if (device->priv->proxy == NULL) {
                g_printerr ("Error creating proxy: %s\n", error->message);
                g_error_free (error);
                goto out;
        }

        /* get the UDI, so we can query ModemManager devices */
        variant_udi = g_dbus_proxy_get_cached_property (device->priv->proxy, "Udi");
        g_variant_get (variant_udi, "s", &device->priv->udi);

        /* get the IP object paths */
        variant_ip4 = g_dbus_proxy_get_cached_property (device->priv->proxy, "Ip4Config");
        g_variant_get (variant_ip4, "o", &device->priv->ip4_config);
        variant_ip6 = g_dbus_proxy_get_cached_property (device->priv->proxy, "Ip6Config");
        g_variant_get (variant_ip6, "o", &device->priv->ip6_config);

        /* get the IP DHCP object paths */
        variant_dhcp4 = g_dbus_proxy_get_cached_property (device->priv->proxy, "Dhcp4Config");
        g_variant_get (variant_dhcp4, "o", &device->priv->dhcp4_config);

        /* get the IP information */
        if (g_strcmp0 (device->priv->dhcp4_config, "/") != 0) {
                device->priv->device_add_refcount++;
                g_dbus_proxy_new_for_bus (G_BUS_TYPE_SYSTEM,
                                          G_DBUS_PROXY_FLAGS_NONE,
                                          NULL,
                                          "org.freedesktop.NetworkManager",
                                          device->priv->dhcp4_config,
                                          "org.freedesktop.NetworkManager.DHCP4Config",
                                          device->priv->cancellable,
                                          nm_device_got_device_proxy_dhcp4_cb,
                                          device);
        } else if (g_strcmp0 (device->priv->ip4_config, "/") != 0) {
                device->priv->device_add_refcount++;
                g_dbus_proxy_new_for_bus (G_BUS_TYPE_SYSTEM,
                                          G_DBUS_PROXY_FLAGS_NONE,
                                          NULL,
                                          "org.freedesktop.NetworkManager",
                                          device->priv->ip4_config,
                                          "org.freedesktop.NetworkManager.IP4Config",
                                          device->priv->cancellable,
                                          nm_device_got_device_proxy_ip4_cb,
                                          device);
        }
        if (g_strcmp0 (device->priv->ip6_config, "/") != 0) {
                device->priv->device_add_refcount++;
                g_dbus_proxy_new_for_bus (G_BUS_TYPE_SYSTEM,
                                          G_DBUS_PROXY_FLAGS_NONE,
                                          NULL,
                                          "org.freedesktop.NetworkManager",
                                          device->priv->ip6_config,
                                          "org.freedesktop.NetworkManager.IP6Config",
                                          device->priv->cancellable,
                                          nm_device_got_device_proxy_ip6_cb,
                                          device);
        }

        /* get the additional interface for this device type */
        variant_type = g_dbus_proxy_get_cached_property (device->priv->proxy, "DeviceType");
        g_variant_get (variant_type, "u", &device->priv->kind);
        if (device->priv->kind == NM_DEVICE_KIND_ETHERNET) {
                device->priv->device_add_refcount++;
                g_dbus_proxy_new_for_bus (G_BUS_TYPE_SYSTEM,
                                          G_DBUS_PROXY_FLAGS_NONE,
                                          NULL,
                                          "org.freedesktop.NetworkManager",
                                          device->priv->object_path,
                                          "org.freedesktop.NetworkManager.Device.Wired",
                                          device->priv->cancellable,
                                          nm_device_got_device_proxy_additional_cb,
                                          device);
        } else if (device->priv->kind == NM_DEVICE_KIND_WIFI) {
                device->priv->device_add_refcount++;
                g_dbus_proxy_new_for_bus (G_BUS_TYPE_SYSTEM,
                                          G_DBUS_PROXY_FLAGS_NONE,
                                          NULL,
                                          "org.freedesktop.NetworkManager",
                                          device->priv->object_path,
                                          "org.freedesktop.NetworkManager.Device.Wireless",
                                          device->priv->cancellable,
                                          nm_device_got_device_proxy_additional_cb,
                                          device);
        } else if (device->priv->kind == NM_DEVICE_KIND_GSM ||
                   device->priv->kind == NM_DEVICE_KIND_CDMA) {
                device->priv->device_add_refcount++;
                g_dbus_proxy_new_for_bus (G_BUS_TYPE_SYSTEM,
                                          G_DBUS_PROXY_FLAGS_NONE,
                                          NULL,
                                          "org.freedesktop.ModemManager",
                                          device->priv->udi,
                                          "org.freedesktop.ModemManager.Modem",
                                          device->priv->cancellable,
                                          nm_device_got_device_proxy_modem_manager_cb,
                                          device);
                device->priv->device_add_refcount++;
                g_dbus_proxy_new_for_bus (G_BUS_TYPE_SYSTEM,
                                          G_DBUS_PROXY_FLAGS_NONE,
                                          NULL,
                                          "org.freedesktop.ModemManager",
                                          device->priv->udi,
                                          "org.freedesktop.ModemManager.Modem.Gsm.Network",
                                          device->priv->cancellable,
                                          nm_device_got_device_proxy_modem_manager_gsm_network_cb,
                                          device);
        }

        /* add device if there are no more pending actions */
        if (--device->priv->device_add_refcount == 0)
                nm_device_emit_ready (device);

        /* we want to update the UI */
        g_signal_connect (device->priv->proxy, "g-properties-changed",
                          G_CALLBACK (nm_device_properties_changed_cb),
                          device);
out:
        if (variant_ip4 != NULL)
                g_variant_unref (variant_ip4);
        if (variant_dhcp4 != NULL)
                g_variant_unref (variant_dhcp4);
        if (variant_ip6 != NULL)
                g_variant_unref (variant_ip6);
        if (variant_udi != NULL)
                g_variant_unref (variant_udi);
        if (variant_type != NULL)
                g_variant_unref (variant_type);
        return;
}

/**
 * nm_device_refresh:
 *
 * 100% async. Object emits ::ready when device has been refreshed
 **/
void
nm_device_refresh (NmDevice *device,
                   const gchar *object_path,
                   GCancellable *cancellable)
{
        device->priv->object_path = g_strdup (object_path);
        if (cancellable != NULL)
                device->priv->cancellable = g_object_ref (cancellable);
        device->priv->device_add_refcount++;
        g_dbus_proxy_new_for_bus (G_BUS_TYPE_SYSTEM,
                                  G_DBUS_PROXY_FLAGS_NONE,
                                  NULL,
                                  "org.freedesktop.NetworkManager",
                                  object_path,
                                  "org.freedesktop.NetworkManager.Device",
                                  device->priv->cancellable,
                                  nm_device_got_device_proxy_cb,
                                  device);
}

/**
 * nm_device_finalize:
 **/
static void
nm_device_finalize (GObject *object)
{
        NmDevice *device = NM_DEVICE (object);
        NmDevicePrivate *priv = device->priv;

        if (priv->proxy != NULL)
                g_object_unref (priv->proxy);
        if (priv->proxy_additional != NULL)
                g_object_unref (priv->proxy_additional);
        if (priv->proxy_ip4 != NULL)
                g_object_unref (priv->proxy_ip4);
        if (priv->proxy_ip6 != NULL)
                g_object_unref (priv->proxy_ip6);
        if (priv->cancellable != NULL)
                g_object_unref (priv->cancellable);
        g_free (priv->active_access_point);
        g_free (priv->ip4_config);
        g_free (priv->ip4_address);
        g_free (priv->ip6_address);
        g_free (priv->ip6_nameserver);
        g_free (priv->ip6_route);
        g_free (priv->ip4_nameserver);
        g_free (priv->ip4_route);
        g_free (priv->ip4_subnet_mask);
        g_free (priv->ip6_config);
        g_free (priv->mac_address);
        g_free (priv->modem_imei);
        g_free (priv->modem_imei);
        g_free (priv->object_path);
        g_free (priv->operator_name);
        g_free (priv->speed);
        g_free (priv->udi);
        g_ptr_array_unref (priv->access_points);

        G_OBJECT_CLASS (nm_device_parent_class)->finalize (object);
}

/**
 * nm_device_class_init:
 **/
static void
nm_device_class_init (NmDeviceClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);
        object_class->finalize = nm_device_finalize;

        /**
         * NmDevice::ready:
         **/
        signals[SIGNAL_READY] =
                g_signal_new ("ready",
                              G_TYPE_FROM_CLASS (object_class), G_SIGNAL_RUN_LAST,
                              G_STRUCT_OFFSET (NmDeviceClass, ready),
                              NULL, NULL, g_cclosure_marshal_VOID__VOID,
                              G_TYPE_NONE, 0);

        /**
         * NmDevice::changed:
         **/
        signals[SIGNAL_CHANGED] =
                g_signal_new ("changed",
                              G_TYPE_FROM_CLASS (object_class), G_SIGNAL_RUN_LAST,
                              G_STRUCT_OFFSET (NmDeviceClass, changed),
                              NULL, NULL, g_cclosure_marshal_VOID__VOID,
                              G_TYPE_NONE, 0);

        g_type_class_add_private (klass, sizeof (NmDevicePrivate));
}

/**
 * nm_device_init:
 **/
static void
nm_device_init (NmDevice *device)
{
        device->priv = NM_DEVICE_GET_PRIVATE (device);
        device->priv->access_points = g_ptr_array_new_with_free_func ((GDestroyNotify) g_object_unref);
}

/**
 * nm_device_new:
 **/
NmDevice *
nm_device_new (void)
{
        NmDevice *device;
        device = g_object_new (NM_TYPE_DEVICE, NULL);
        return NM_DEVICE (device);
}

