/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2011-2012 Richard Hughes <richard@hughsie.com>
 * Copyright (C) 2012 Red Hat, Inc.
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

#include "net-device-bluetooth.h"

struct _NetDeviceBluetooth
{
        GtkBox        parent;

        GtkLabel     *device_label;
        GtkSwitch    *device_off_switch;
        GtkButton    *options_button;
        GtkSeparator *separator;

        NMClient     *client;
        NMDevice     *device;
        gboolean      updating_device;
};

G_DEFINE_TYPE (NetDeviceBluetooth, net_device_bluetooth, GTK_TYPE_BOX)

void
net_device_bluetooth_set_show_separator (NetDeviceBluetooth *self,
                                         gboolean            show_separator)
{
        /* add widgets to size group */
        gtk_widget_set_visible (GTK_WIDGET (self->separator), show_separator);
}

static void
update_off_switch_from_device_state (GtkSwitch *sw,
                                     NMDeviceState state,
                                     NetDeviceBluetooth *self)
{
        self->updating_device = TRUE;
        switch (state) {
                case NM_DEVICE_STATE_UNMANAGED:
                case NM_DEVICE_STATE_UNAVAILABLE:
                case NM_DEVICE_STATE_DISCONNECTED:
                case NM_DEVICE_STATE_DEACTIVATING:
                case NM_DEVICE_STATE_FAILED:
                        gtk_switch_set_active (sw, FALSE);
                        break;
                default:
                        gtk_switch_set_active (sw, TRUE);
                        break;
        }
        self->updating_device = FALSE;
}

static void
nm_device_bluetooth_refresh_ui (NetDeviceBluetooth *self)
{
        NMDeviceState state;
        g_autofree gchar *path = NULL;

        /* set up the device on/off switch */
        state = nm_device_get_state (self->device);
        gtk_widget_set_visible (GTK_WIDGET (self->device_off_switch),
                                state != NM_DEVICE_STATE_UNAVAILABLE
                                && state != NM_DEVICE_STATE_UNMANAGED);
        update_off_switch_from_device_state (self->device_off_switch, state, self);

        /* set up the Options button */
        path = g_find_program_in_path ("nm-connection-editor");
        gtk_widget_set_visible (GTK_WIDGET (self->options_button), state != NM_DEVICE_STATE_UNMANAGED && path != NULL);
}

static void
device_off_switch_changed_cb (NetDeviceBluetooth *self)
{
        const GPtrArray *acs;
        gboolean active;
        gint i;
        NMActiveConnection *a;
        NMConnection *connection;

        if (self->updating_device)
                return;

        connection = net_device_get_find_connection (self->client, self->device);
        if (connection == NULL)
                return;

        active = gtk_switch_get_active (self->device_off_switch);
        if (active) {
                nm_client_activate_connection_async (self->client,
                                                     connection,
                                                     self->device,
                                                     NULL, NULL, NULL, NULL);
        } else {
                const gchar *uuid;

                uuid = nm_connection_get_uuid (connection);
                acs = nm_client_get_active_connections (self->client);
                for (i = 0; acs && i < acs->len; i++) {
                        a = (NMActiveConnection*)acs->pdata[i];
                        if (strcmp (nm_active_connection_get_uuid (a), uuid) == 0) {
                                nm_client_deactivate_connection (self->client, a, NULL, NULL);
                                break;
                        }
                }
        }
}

static void
options_button_clicked_cb (NetDeviceBluetooth *self)
{
        const gchar *uuid;
        g_autofree gchar *cmdline = NULL;
        g_autoptr(GError) error = NULL;
        NMConnection *connection;

        connection = net_device_get_find_connection (self->client, self->device);
        uuid = nm_connection_get_uuid (connection);
        cmdline = g_strdup_printf ("nm-connection-editor --edit %s", uuid);
        g_debug ("Launching '%s'\n", cmdline);
        if (!g_spawn_command_line_async (cmdline, &error))
                g_warning ("Failed to launch nm-connection-editor: %s", error->message);
}

static void
net_device_bluetooth_finalize (GObject *object)
{
        NetDeviceBluetooth *self = NET_DEVICE_BLUETOOTH (object);

        g_clear_object (&self->client);
        g_clear_object (&self->device);

        G_OBJECT_CLASS (net_device_bluetooth_parent_class)->finalize (object);
}

static void
net_device_bluetooth_class_init (NetDeviceBluetoothClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);
        GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

        object_class->finalize = net_device_bluetooth_finalize;

        gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/network/network-bluetooth.ui");

        gtk_widget_class_bind_template_child (widget_class, NetDeviceBluetooth, device_label);
        gtk_widget_class_bind_template_child (widget_class, NetDeviceBluetooth, device_off_switch);
        gtk_widget_class_bind_template_child (widget_class, NetDeviceBluetooth, options_button);
        gtk_widget_class_bind_template_child (widget_class, NetDeviceBluetooth, separator);

        gtk_widget_class_bind_template_callback (widget_class, device_off_switch_changed_cb);
        gtk_widget_class_bind_template_callback (widget_class, options_button_clicked_cb);

}

static void
net_device_bluetooth_init (NetDeviceBluetooth *self)
{
        g_autofree gchar *path = NULL;

        gtk_widget_init_template (GTK_WIDGET (self));

        path = g_find_program_in_path ("nm-connection-editor");
        gtk_widget_set_visible (GTK_WIDGET (self->options_button), path != NULL);
}

NetDeviceBluetooth *
net_device_bluetooth_new (NMClient *client, NMDevice *device)
{
        NetDeviceBluetooth *self;

        self = g_object_new (net_device_bluetooth_get_type (), NULL);
        self->client = g_object_ref (client);
        self->device = g_object_ref (device);

        g_signal_connect_object (device, "state-changed", G_CALLBACK (nm_device_bluetooth_refresh_ui), self, G_CONNECT_SWAPPED);

        nm_device_bluetooth_refresh_ui (self);

        return self;
}

NMDevice *
net_device_bluetooth_get_device (NetDeviceBluetooth *self)
{
        g_return_val_if_fail (NET_IS_DEVICE_BLUETOOTH (self), NULL);
        return self->device;
}

void
net_device_bluetooth_set_title (NetDeviceBluetooth *self, const gchar *title)
{
        g_return_if_fail (NET_IS_DEVICE_BLUETOOTH (self));
        gtk_label_set_label (self->device_label, title);
}
