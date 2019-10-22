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
        NetDevice     parent;

        GtkBuilder   *builder;
        GtkBox       *box;
        GtkLabel     *device_label;
        GtkSwitch    *device_off_switch;
        GtkButton    *options_button;
        GtkSeparator *separator;

        gboolean    updating_device;
};

G_DEFINE_TYPE (NetDeviceBluetooth, net_device_bluetooth, NET_TYPE_DEVICE)

void
net_device_bluetooth_set_show_separator (NetDeviceBluetooth *self,
                                         gboolean            show_separator)
{
        /* add widgets to size group */
        gtk_widget_set_visible (GTK_WIDGET (self->separator), show_separator);
}

static GtkWidget *
device_bluetooth_get_widget (NetObject    *object,
                             GtkSizeGroup *heading_size_group)
{
        NetDeviceBluetooth *self = NET_DEVICE_BLUETOOTH (object);

        return GTK_WIDGET (self->box);
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
        NMDevice *nm_device;
        NMDeviceState state;

        nm_device = net_device_get_nm_device (NET_DEVICE (self));

        /* set device kind */
        g_object_bind_property (self, "title", self->device_label, "label", 0);

        /* set up the device on/off switch */
        state = nm_device_get_state (nm_device);
        gtk_widget_set_visible (GTK_WIDGET (self->device_off_switch),
                                state != NM_DEVICE_STATE_UNAVAILABLE
                                && state != NM_DEVICE_STATE_UNMANAGED);
        update_off_switch_from_device_state (self->device_off_switch, state, self);

        /* set up the Options button */
        gtk_widget_set_visible (GTK_WIDGET (self->options_button), state != NM_DEVICE_STATE_UNMANAGED);
}

static void
device_bluetooth_refresh (NetObject *object)
{
        NetDeviceBluetooth *self = NET_DEVICE_BLUETOOTH (object);
        nm_device_bluetooth_refresh_ui (self);
}

static void
device_off_toggled (NetDeviceBluetooth *self)
{
        const GPtrArray *acs;
        gboolean active;
        gint i;
        NMActiveConnection *a;
        NMConnection *connection;
        NMClient *client;

        if (self->updating_device)
                return;

        active = gtk_switch_get_active (self->device_off_switch);
        if (active) {
                client = net_object_get_client (NET_OBJECT (self));
                connection = net_device_get_find_connection (NET_DEVICE (self));
                if (connection == NULL)
                        return;
                nm_client_activate_connection_async (client,
                                                     connection,
                                                     net_device_get_nm_device (NET_DEVICE (self)),
                                                     NULL, NULL, NULL, NULL);
        } else {
                const gchar *uuid;

                connection = net_device_get_find_connection (NET_DEVICE (self));
                if (connection == NULL)
                        return;
                uuid = nm_connection_get_uuid (connection);
                client = net_object_get_client (NET_OBJECT (self));
                acs = nm_client_get_active_connections (client);
                for (i = 0; acs && i < acs->len; i++) {
                        a = (NMActiveConnection*)acs->pdata[i];
                        if (strcmp (nm_active_connection_get_uuid (a), uuid) == 0) {
                                nm_client_deactivate_connection (client, a, NULL, NULL);
                                break;
                        }
                }
        }
}

static void
edit_connection (NetDeviceBluetooth *self)
{
        const gchar *uuid;
        g_autofree gchar *cmdline = NULL;
        g_autoptr(GError) error = NULL;
        NMConnection *connection;

        connection = net_device_get_find_connection (NET_DEVICE (self));
        uuid = nm_connection_get_uuid (connection);
        cmdline = g_strdup_printf ("nm-connection-editor --edit %s", uuid);
        g_debug ("Launching '%s'\n", cmdline);
        if (!g_spawn_command_line_async (cmdline, &error))
                g_warning ("Failed to launch nm-connection-editor: %s", error->message);
}

static void
net_device_bluetooth_constructed (GObject *object)
{
        NetDeviceBluetooth *self = NET_DEVICE_BLUETOOTH (object);

        G_OBJECT_CLASS (net_device_bluetooth_parent_class)->constructed (object);

        net_object_refresh (NET_OBJECT (self));
}

static void
net_device_bluetooth_finalize (GObject *object)
{
        NetDeviceBluetooth *self = NET_DEVICE_BLUETOOTH (object);

        g_clear_object (&self->builder);

        G_OBJECT_CLASS (net_device_bluetooth_parent_class)->finalize (object);
}

static void
net_device_bluetooth_class_init (NetDeviceBluetoothClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);
        NetObjectClass *parent_class = NET_OBJECT_CLASS (klass);

        object_class->finalize = net_device_bluetooth_finalize;
        object_class->constructed = net_device_bluetooth_constructed;
        parent_class->get_widget = device_bluetooth_get_widget;
        parent_class->refresh = device_bluetooth_refresh;
}

static void
net_device_bluetooth_init (NetDeviceBluetooth *self)
{
        g_autoptr(GError) error = NULL;

        self->builder = gtk_builder_new ();
        gtk_builder_add_from_resource (self->builder,
                                       "/org/gnome/control-center/network/network-bluetooth.ui",
                                       &error);
        if (error != NULL) {
                g_warning ("Could not load interface file: %s", error->message);
                return;
        }

        self->box = GTK_BOX (gtk_builder_get_object (self->builder, "box"));
        self->device_label = GTK_LABEL (gtk_builder_get_object (self->builder, "device_label"));
        self->device_off_switch = GTK_SWITCH (gtk_builder_get_object (self->builder, "device_off_switch"));
        self->options_button = GTK_BUTTON (gtk_builder_get_object (self->builder, "options_button"));
        self->separator = GTK_SEPARATOR (gtk_builder_get_object (self->builder, "separator"));

        g_signal_connect_swapped (self->device_off_switch, "notify::active",
                                  G_CALLBACK (device_off_toggled), self);

        g_signal_connect_swapped (self->options_button, "clicked",
                                  G_CALLBACK (edit_connection), self);
}

NetDeviceBluetooth *
net_device_bluetooth_new (GCancellable *cancellable,
                          NMClient     *client,
                          NMDevice     *device,
                          const gchar  *id)
{
        return g_object_new (NET_TYPE_DEVICE_BLUETOOTH,
                             "cancellable", cancellable,
                             "client", client,
                             "nm-device", device,
                             "id", id,
                             NULL);
}
