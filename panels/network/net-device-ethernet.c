/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2011-2012 Richard Hughes <richard@hughsie.com>
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
#include <NetworkManager.h>

#include "panel-common.h"

#include "connection-editor/net-connection-editor.h"

#include "net-device-ethernet.h"

struct _NetDeviceEthernet
{
        AdwActionRow        parent;

        GtkSwitch          *device_off_switch;

        NMClient           *client;
        NMDevice           *device;
        gboolean            updating_device;
        NMConnection       *connection;

};

G_DEFINE_TYPE (NetDeviceEthernet, net_device_ethernet, ADW_TYPE_ACTION_ROW)

static gboolean
device_state_to_off_switch (NMDeviceState state)
{
        switch (state) {
                case NM_DEVICE_STATE_UNMANAGED:
                case NM_DEVICE_STATE_UNAVAILABLE:
                case NM_DEVICE_STATE_DISCONNECTED:
                case NM_DEVICE_STATE_DEACTIVATING:
                case NM_DEVICE_STATE_FAILED:
                        return FALSE;
                default:
                        return TRUE;
        }
}

static void
device_off_switch_changed_cb (NetDeviceEthernet *self)
{
        NMConnection *connection;

        if (self->updating_device)
                return;

        if (gtk_switch_get_active (self->device_off_switch)) {
                connection = net_device_get_find_connection (self->client, self->device);
                if (connection != NULL) {
                        nm_client_activate_connection_async (self->client,
                                                             connection,
                                                             self->device,
                                                             NULL, NULL, NULL, NULL);
                }
        } else {
                nm_device_disconnect_async (self->device, NULL, NULL, NULL);
        }
}

static const gchar *
device_state_to_icon_name (NMDeviceState state)
{
        const gchar *icon_name = NULL;

        switch (state) {
        case NM_DEVICE_STATE_ACTIVATED:
                icon_name = "network-wired-symbolic";
                break;
        case NM_DEVICE_STATE_DISCONNECTED:
                icon_name = "network-wired-disconnected-symbolic";
                break;
        case NM_DEVICE_STATE_PREPARE:
        case NM_DEVICE_STATE_CONFIG:
        case NM_DEVICE_STATE_IP_CONFIG:
        case NM_DEVICE_STATE_IP_CHECK:
        case NM_DEVICE_STATE_NEED_AUTH:
        case NM_DEVICE_STATE_DEACTIVATING:
                icon_name = "network-wired-acquiring-symbolic";
                break;
        default:
                icon_name = "network-wired-no-route-symbolic";
                break;

        }

        return icon_name;
}

static void
device_ethernet_refresh_ui (NetDeviceEthernet *self)
{
        NMDeviceState state;
        g_autofree gchar *speed_text = NULL;
        g_autofree gchar *status = NULL;

        state = nm_device_get_state (self->device);
        gtk_widget_set_sensitive (GTK_WIDGET (self->device_off_switch),
                                  state != NM_DEVICE_STATE_UNAVAILABLE
                                  && state != NM_DEVICE_STATE_UNMANAGED);
        self->updating_device = TRUE;
        gtk_switch_set_active (self->device_off_switch, device_state_to_off_switch (state));
        self->updating_device = FALSE;

        if (state != NM_DEVICE_STATE_UNAVAILABLE) {
                guint speed = nm_device_ethernet_get_speed (NM_DEVICE_ETHERNET (self->device));
                if (speed > 0) {
                        /* Translators: the %'d is replaced by the network device speed with
                         * thousands separator, so do not change to %d */
                        speed_text = g_strdup_printf (_("%'d Mb/s"), speed);
                }
        }
        status = panel_device_status_to_localized_string (self->device, speed_text);
        gtk_widget_set_tooltip_text (GTK_WIDGET (self), status);
        adw_action_row_set_icon_name (ADW_ACTION_ROW (self), device_state_to_icon_name (state));
}

static void
details_button_clicked_cb (NetDeviceEthernet *self)
{
        g_signal_emit_by_name (self, "activated");
}

static void
device_ethernet_finalize (GObject *object)
{
        NetDeviceEthernet *self = NET_DEVICE_ETHERNET (object);

        g_clear_object (&self->client);
        g_clear_object (&self->device);

        G_OBJECT_CLASS (net_device_ethernet_parent_class)->finalize (object);
}

static void
net_device_ethernet_class_init (NetDeviceEthernetClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);
        GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

        object_class->finalize = device_ethernet_finalize;

        gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/network/network-ethernet.ui");

        gtk_widget_class_bind_template_child (widget_class, NetDeviceEthernet, device_off_switch);

        gtk_widget_class_bind_template_callback (widget_class, device_off_switch_changed_cb);
        gtk_widget_class_bind_template_callback (widget_class, details_button_clicked_cb);
}

static void
net_device_ethernet_init (NetDeviceEthernet *self)
{
        gtk_widget_init_template (GTK_WIDGET (self));

}

GtkWidget *
net_device_ethernet_new (NMClient *client, NMDevice *device, NMConnection *connection)
{
        NetDeviceEthernet *self;

        self = g_object_new (net_device_ethernet_get_type (), NULL);
        self->client = g_object_ref (client);
        self->device = g_object_ref (device);
        self->connection = g_object_ref (connection);

        device_ethernet_refresh_ui (self);
        g_signal_connect_object (device, "state-changed", G_CALLBACK (device_ethernet_refresh_ui), self, G_CONNECT_SWAPPED);

        return GTK_WIDGET (self);
}

NMDevice *
net_device_ethernet_get_device (NetDeviceEthernet *self)
{
        g_return_val_if_fail (NET_IS_DEVICE_ETHERNET (self), NULL);
        return self->device;
 }
