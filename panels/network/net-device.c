/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2011 Richard Hughes <richard@hughsie.com>
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
#include <glib/gi18n.h>
#include <arpa/inet.h>
#include <netinet/ether.h>

#include <nm-device-ethernet.h>
#include <nm-device-modem.h>
#include <nm-device-wifi.h>
#include <nm-device-wimax.h>
#include <nm-device-infiniband.h>
#include <nm-utils.h>

#include "net-device.h"

#define NET_DEVICE_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), NET_TYPE_DEVICE, NetDevicePrivate))

struct _NetDevicePrivate
{
        NMDevice                        *nm_device;
        guint                            changed_id;
};

enum {
        PROP_0,
        PROP_DEVICE,
        PROP_LAST
};

G_DEFINE_TYPE (NetDevice, net_device, NET_TYPE_OBJECT)

/* return value must be freed by caller with g_free() */
static gchar *
get_mac_address_of_connection (NMConnection *connection)
{
        if (!connection)
                return NULL;

        const GByteArray *mac = NULL;

        /* check the connection type */
        if (nm_connection_is_type (connection,
                                   NM_SETTING_WIRELESS_SETTING_NAME)) {
                /* check wireless settings */
                NMSettingWireless *s_wireless = nm_connection_get_setting_wireless (connection);
                if (!s_wireless)
                        return NULL;
                mac = nm_setting_wireless_get_mac_address (s_wireless);
                if (mac)
                        return nm_utils_hwaddr_ntoa (mac->data, ARPHRD_ETHER);
        } else if (nm_connection_is_type (connection,
                                          NM_SETTING_WIRED_SETTING_NAME)) {
                /* check wired settings */
                NMSettingWired *s_wired = nm_connection_get_setting_wired (connection);
                if (!s_wired)
                        return NULL;
                mac = nm_setting_wired_get_mac_address (s_wired);
                if (mac)
                        return nm_utils_hwaddr_ntoa (mac->data, ARPHRD_ETHER);
        } else if (nm_connection_is_type (connection,
                                          NM_SETTING_WIMAX_SETTING_NAME)) {
                /* check wimax settings */
                NMSettingWimax *s_wimax = nm_connection_get_setting_wimax (connection);
                if (!s_wimax)
                        return NULL;
                mac = nm_setting_wimax_get_mac_address (s_wimax);
                if (mac)
                        return nm_utils_hwaddr_ntoa (mac->data, ARPHRD_ETHER);
        } else if (nm_connection_is_type (connection,
                                          NM_SETTING_INFINIBAND_SETTING_NAME)) {
                /* check infiniband settings */
                NMSettingInfiniband *s_infiniband = \
                        nm_connection_get_setting_infiniband (connection);
                if (!s_infiniband)
                        return NULL;
                mac = nm_setting_infiniband_get_mac_address (s_infiniband);
                if (mac)
                        return nm_utils_hwaddr_ntoa (mac->data,
                                                     ARPHRD_INFINIBAND);
        }
        /* no MAC address found */
        return NULL;
}

/* return value must not be freed! */
static const gchar *
get_mac_address_of_device (NMDevice *device)
{
        const gchar *mac = NULL;
        switch (nm_device_get_device_type (device)) {
        case NM_DEVICE_TYPE_WIFI:
        {
                NMDeviceWifi *device_wifi = NM_DEVICE_WIFI (device);
                mac = nm_device_wifi_get_hw_address (device_wifi);
                break;
        }
        case NM_DEVICE_TYPE_ETHERNET:
        {
                NMDeviceEthernet *device_ethernet = NM_DEVICE_ETHERNET (device);
                mac = nm_device_ethernet_get_hw_address (device_ethernet);
                break;
        }
        case NM_DEVICE_TYPE_WIMAX:
        {
                NMDeviceWimax *device_wimax = NM_DEVICE_WIMAX (device);
                mac = nm_device_wimax_get_hw_address (device_wimax);
                break;
        }
        case NM_DEVICE_TYPE_INFINIBAND:
        {
                NMDeviceInfiniband *device_infiniband = \
                        NM_DEVICE_INFINIBAND (device);
                mac = nm_device_infiniband_get_hw_address (device_infiniband);
                break;
        }
        default:
                break;
        }
        /* no MAC address found */
        return mac;
}

/* returns TRUE if both MACs are equal */
static gboolean
compare_mac_device_with_mac_connection (NMDevice *device,
                                        NMConnection *connection)
{
        const gchar *mac_dev = NULL;
        gchar *mac_conn = NULL;

        mac_dev = get_mac_address_of_device (device);
        if (mac_dev != NULL) {
                mac_conn = get_mac_address_of_connection (connection);
                if (mac_conn) {
                        /* compare both MACs */
                        if (g_strcmp0 (mac_dev, mac_conn) == 0) {
                                g_free (mac_conn);
                                return TRUE;
                        }
                        g_free (mac_conn);
                }
        }
        return FALSE;
}

static GSList *
valid_connections_for_device (NMRemoteSettings *remote_settings,
                              NetDevice *device)
{
        GSList *all, *filtered, *iterator, *valid;
        NMConnection *connection;
        NMSettingConnection *s_con;

        all = nm_remote_settings_list_connections (remote_settings);
        filtered = nm_device_filter_connections (device->priv->nm_device, all);
        g_slist_free (all);

        valid = NULL;
        for (iterator = filtered; iterator; iterator = iterator->next) {
                connection = iterator->data;
                s_con = nm_connection_get_setting_connection (connection);
                if (!s_con)
                        continue;

                if (nm_setting_connection_get_master (s_con))
                        continue;

                valid = g_slist_prepend (valid, connection);
        }
        g_slist_free (filtered);

        return g_slist_reverse (valid);
}

NMConnection *
net_device_get_find_connection (NetDevice *device)
{
        GSList *list, *iterator;
        NMConnection *connection = NULL;
        NMActiveConnection *ac;
        NMRemoteSettings *remote_settings;

        /* is the device available in a active connection? */
        remote_settings = net_object_get_remote_settings (NET_OBJECT (device));
        ac = nm_device_get_active_connection (device->priv->nm_device);
        if (ac) {
                return (NMConnection*)nm_remote_settings_get_connection_by_path (remote_settings,
                                        nm_active_connection_get_connection (ac));
        }

        /* not found in active connections - check all available connections */
        list = valid_connections_for_device (remote_settings, device);
        if (list != NULL) {
                /* if list has only one connection, use this connection */
                if (g_slist_length (list) == 1) {
                        connection = list->data;
                        goto out;
                }

                /* is there connection with the MAC address of the device? */
                for (iterator = list; iterator; iterator = iterator->next) {
                        connection = iterator->data;
                        if (compare_mac_device_with_mac_connection (device->priv->nm_device,
                                                                    connection)) {
                                goto out;
                        }
                }
        }

        /* no connection found for the given device */
        connection = NULL;
out:
        g_slist_free (list);
        return connection;
}

static void
state_changed_cb (NMDevice *device,
                  NMDeviceState new_state,
                  NMDeviceState old_state,
                  NMDeviceStateReason reason,
                  NetDevice *net_device)
{
        net_object_emit_changed (NET_OBJECT (net_device));
        net_object_refresh (NET_OBJECT (net_device));
}

NMDevice *
net_device_get_nm_device (NetDevice *device)
{
        return device->priv->nm_device;
}

static void
net_device_edit (NetObject *object)
{
        const gchar *uuid;
        gchar *cmdline;
        GError *error = NULL;
        NetDevice *device = NET_DEVICE (object);
        NMConnection *connection;

        connection = net_device_get_find_connection (device);
        uuid = nm_connection_get_uuid (connection);
        cmdline = g_strdup_printf ("nm-connection-editor --edit %s", uuid);
        g_debug ("Launching '%s'\n", cmdline);
        if (!g_spawn_command_line_async (cmdline, &error)) {
                g_warning ("Failed to launch nm-connection-editor: %s", error->message);
                g_error_free (error);
        }
        g_free (cmdline);
}

/**
 * net_device_get_property:
 **/
static void
net_device_get_property (GObject *device_,
                         guint prop_id,
                         GValue *value,
                         GParamSpec *pspec)
{
        NetDevice *net_device = NET_DEVICE (device_);
        NetDevicePrivate *priv = net_device->priv;

        switch (prop_id) {
        case PROP_DEVICE:
                g_value_set_object (value, priv->nm_device);
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (net_device, prop_id, pspec);
                break;
        }
}

/**
 * net_device_set_property:
 **/
static void
net_device_set_property (GObject *device_,
                         guint prop_id,
                         const GValue *value,
                         GParamSpec *pspec)
{
        NetDevice *net_device = NET_DEVICE (device_);
        NetDevicePrivate *priv = net_device->priv;

        switch (prop_id) {
        case PROP_DEVICE:
                if (priv->changed_id != 0) {
                        g_signal_handler_disconnect (priv->nm_device,
                                                     priv->changed_id);
                }
                priv->nm_device = g_value_dup_object (value);
                priv->changed_id = g_signal_connect (priv->nm_device,
                                                     "state-changed",
                                                     G_CALLBACK (state_changed_cb),
                                                     net_device);
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (net_device, prop_id, pspec);
                break;
        }
}

static void
net_device_finalize (GObject *object)
{
        NetDevice *device = NET_DEVICE (object);
        NetDevicePrivate *priv = device->priv;

        if (priv->changed_id != 0) {
                g_signal_handler_disconnect (priv->nm_device,
                                             priv->changed_id);
        }
        if (priv->nm_device != NULL)
                g_object_unref (priv->nm_device);

        G_OBJECT_CLASS (net_device_parent_class)->finalize (object);
}

static void
net_device_class_init (NetDeviceClass *klass)
{
        GParamSpec *pspec;
        GObjectClass *object_class = G_OBJECT_CLASS (klass);
        NetObjectClass *parent_class = NET_OBJECT_CLASS (klass);

        object_class->finalize = net_device_finalize;
        object_class->get_property = net_device_get_property;
        object_class->set_property = net_device_set_property;
        parent_class->edit = net_device_edit;

        pspec = g_param_spec_object ("nm-device", NULL, NULL,
                                     NM_TYPE_DEVICE,
                                     G_PARAM_READWRITE | G_PARAM_CONSTRUCT);
        g_object_class_install_property (object_class, PROP_DEVICE, pspec);

        g_type_class_add_private (klass, sizeof (NetDevicePrivate));
}

static void
net_device_init (NetDevice *device)
{
        device->priv = NET_DEVICE_GET_PRIVATE (device);
}

NetDevice *
net_device_new (void)
{
        NetDevice *device;
        device = g_object_new (NET_TYPE_DEVICE,
                               "removable", FALSE,
                               NULL);
        return NET_DEVICE (device);
}

