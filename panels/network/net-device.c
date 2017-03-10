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

#include <NetworkManager.h>

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

        /* check the connection type */
        if (nm_connection_is_type (connection,
                                   NM_SETTING_WIRELESS_SETTING_NAME)) {
                /* check wireless settings */
                NMSettingWireless *s_wireless = nm_connection_get_setting_wireless (connection);
                if (!s_wireless)
                        return NULL;
                return g_strdup (nm_setting_wireless_get_mac_address (s_wireless));
        } else if (nm_connection_is_type (connection,
                                          NM_SETTING_WIRED_SETTING_NAME)) {
                /* check wired settings */
                NMSettingWired *s_wired = nm_connection_get_setting_wired (connection);
                if (!s_wired)
                        return NULL;
                return g_strdup (nm_setting_wired_get_mac_address (s_wired));
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

static NMConnection *
net_device_real_get_find_connection (NetDevice *device)
{
        GSList *list, *iterator;
        NMConnection *connection = NULL;
        NMActiveConnection *ac;

        /* is the device available in a active connection? */
        ac = nm_device_get_active_connection (device->priv->nm_device);
        if (ac)
                return (NMConnection*) nm_active_connection_get_connection (ac);

        /* not found in active connections - check all available connections */
        list = net_device_get_valid_connections (device);
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

NMConnection *
net_device_get_find_connection (NetDevice *device)
{
        return NET_DEVICE_GET_CLASS (device)->get_find_connection (device);
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
        g_return_val_if_fail (NET_IS_DEVICE (device), NULL);
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
                if (priv->nm_device) {
                        priv->changed_id = g_signal_connect (priv->nm_device,
                                                             "state-changed",
                                                             G_CALLBACK (state_changed_cb),
                                                             net_device);
                } else
                        priv->changed_id = 0;
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
        g_clear_object (&priv->nm_device);

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
        klass->get_find_connection = net_device_real_get_find_connection;

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

GSList *
net_device_get_valid_connections (NetDevice *device)
{
        GSList *valid;
        NMConnection *connection;
        NMSettingConnection *s_con;
        NMActiveConnection *active_connection;
        const char *active_uuid;
        const GPtrArray *all;
        GPtrArray *filtered;
        guint i;

        all = nm_client_get_connections (net_object_get_client (NET_OBJECT (device)));
        filtered = nm_device_filter_connections (net_device_get_nm_device (device), all);

        active_connection = nm_device_get_active_connection (net_device_get_nm_device (device));
        active_uuid = active_connection ? nm_active_connection_get_uuid (active_connection) : NULL;

        valid = NULL;
        for (i = 0; i < filtered->len; i++) {
                connection = g_ptr_array_index (filtered, i);
                s_con = nm_connection_get_setting_connection (connection);
                if (!s_con)
                        continue;

                if (nm_setting_connection_get_master (s_con) &&
                    g_strcmp0 (nm_setting_connection_get_uuid (s_con), active_uuid) != 0)
                        continue;

                valid = g_slist_prepend (valid, connection);
        }
        g_ptr_array_free (filtered, FALSE);

        return g_slist_reverse (valid);
}
