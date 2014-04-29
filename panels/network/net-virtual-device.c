/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
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

#include <nm-client.h>
#include <nm-device.h>
#include <nm-remote-connection.h>

#include "panel-common.h"
#include "cc-network-panel.h"

#include "net-virtual-device.h"

#define NET_VIRTUAL_DEVICE_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), NET_TYPE_VIRTUAL_DEVICE, NetVirtualDevicePrivate))

struct _NetVirtualDevicePrivate {
        NMConnection *connection;
        const char *iface;

        GtkBuilder *builder;
        gboolean updating_device;
};

enum {
        PROP_0,
        PROP_CONNECTION,
        PROP_LAST
};

enum {
        SIGNAL_DEVICE_SET,
        SIGNAL_DEVICE_UNSET,
        SIGNAL_LAST
};

static guint signals[SIGNAL_LAST] = { 0 };

G_DEFINE_TYPE (NetVirtualDevice, net_virtual_device, NET_TYPE_DEVICE)

static void
connection_changed_cb (NMConnection *connection,
                       NetObject    *object)
{
        net_object_emit_changed (object);
}

static void
connection_removed_cb (NMConnection *connection,
                       NetObject    *object)
{
        net_object_emit_removed (object);
}

static void
net_virtual_device_set_connection (NetVirtualDevice *virtual_device,
                                   NMConnection     *connection)
{
        NetVirtualDevicePrivate *priv = virtual_device->priv;

        priv->connection = g_object_ref (connection);
        priv->iface = nm_connection_get_virtual_iface_name (priv->connection);

        g_signal_connect (priv->connection,
                          NM_REMOTE_CONNECTION_REMOVED,
                          G_CALLBACK (connection_removed_cb),
                          virtual_device);
        g_signal_connect (priv->connection,
                          NM_REMOTE_CONNECTION_UPDATED,
                          G_CALLBACK (connection_changed_cb),
                          virtual_device);
}

static void
net_virtual_device_get_property (GObject *object,
                                 guint prop_id,
                                 GValue *value,
                                 GParamSpec *pspec)
{
        NetVirtualDevice *virtual_device = NET_VIRTUAL_DEVICE (object);
        NetVirtualDevicePrivate *priv = virtual_device->priv;

        switch (prop_id) {
        case PROP_CONNECTION:
                g_value_set_object (value, priv->connection);
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (virtual_device, prop_id, pspec);
                break;

        }
}

static void
net_virtual_device_set_property (GObject *object,
                                 guint prop_id,
                                 const GValue *value,
                                 GParamSpec *pspec)
{
        NetVirtualDevice *virtual_device = NET_VIRTUAL_DEVICE (object);
        NMConnection *connection;

        switch (prop_id) {
        case PROP_CONNECTION:
                connection = NM_CONNECTION (g_value_get_object (value));
                net_virtual_device_set_connection (virtual_device, connection);
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (virtual_device, prop_id, pspec);
                break;

        }
}

static GtkWidget *
net_virtual_device_add_to_notebook (NetObject *object,
                                    GtkNotebook *notebook,
                                    GtkSizeGroup *heading_size_group)
{
        NetVirtualDevice *virtual_device = NET_VIRTUAL_DEVICE (object);
        GtkWidget *widget;

        /* add widgets to size group */
        widget = GTK_WIDGET (gtk_builder_get_object (virtual_device->priv->builder,
                                                     "heading_ipv4"));
        gtk_size_group_add_widget (heading_size_group, widget);

        widget = GTK_WIDGET (gtk_builder_get_object (virtual_device->priv->builder,
                                                     "vbox6"));
        gtk_notebook_append_page (notebook, widget, NULL);
        return widget;
}

static void
update_off_switch_from_device_state (GtkSwitch *sw,
                                     NMDeviceState state,
                                     NetVirtualDevice *virtual_device)
{
        virtual_device->priv->updating_device = TRUE;
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
        virtual_device->priv->updating_device = FALSE;
}

static void
net_virtual_device_refresh (NetObject *object)
{
        NetVirtualDevice *virtual_device = NET_VIRTUAL_DEVICE (object);
        NetVirtualDevicePrivate *priv = virtual_device->priv;
        char *hwaddr;
        GtkWidget *widget;
        NMDevice *nm_device;
        NMDeviceState state;
        gboolean disconnected;

        nm_device = net_device_get_nm_device (NET_DEVICE (virtual_device));
        state = nm_device ? nm_device_get_state (nm_device) : NM_DEVICE_STATE_DISCONNECTED;
        disconnected = (state == NM_DEVICE_STATE_DISCONNECTED || state == NM_DEVICE_STATE_UNAVAILABLE);

        /* set device kind */
        widget = GTK_WIDGET (gtk_builder_get_object (priv->builder, "image_device"));
        gtk_image_set_from_icon_name (GTK_IMAGE (widget),
                                      disconnected ? "network-wired-disconnected" : "network-wired",
                                      GTK_ICON_SIZE_DIALOG);

        /* set up the device on/off switch */
        widget = GTK_WIDGET (gtk_builder_get_object (priv->builder, "device_off_switch"));
        update_off_switch_from_device_state (GTK_SWITCH (widget), state, virtual_device);

        /* set device state, with status and optionally speed */
        if (nm_device) {
                panel_set_device_status (priv->builder, "label_status", nm_device, NULL);
        } else {
                widget = GTK_WIDGET (gtk_builder_get_object (priv->builder, "label_status"));
                gtk_label_set_label (GTK_LABEL (widget), "");
        }

        /* device MAC */
        if (nm_device) {
                g_object_get (G_OBJECT (nm_device),
                              "hw-address", &hwaddr,
                              NULL);
        } else
                hwaddr = g_strdup ("");
        panel_set_device_widget_details (priv->builder, "mac", hwaddr);
        g_free (hwaddr);

        /* set IP entries */
        if (nm_device)
                panel_set_device_widgets (priv->builder, nm_device);
        else
                panel_unset_device_widgets (priv->builder);
}

static void
net_virtual_device_delete (NetObject *object)
{
        NetVirtualDevice *virtual_device = NET_VIRTUAL_DEVICE (object);
        NetVirtualDevicePrivate *priv = virtual_device->priv;
        const char *path;
        NMRemoteSettings *settings;
        NMRemoteConnection *connection;

        settings = net_object_get_remote_settings (object);
        path = nm_connection_get_path (priv->connection);
        connection = nm_remote_settings_get_connection_by_path (settings, path);
        nm_remote_connection_delete (connection, NULL, NULL);
}

static void
device_added_cb (NMClient *client, NMDevice *nm_device, gpointer user_data)
{
        NetVirtualDevice *virtual_device = user_data;
        NetVirtualDevicePrivate *priv = virtual_device->priv;
        const char *iface;

        iface = nm_device_get_iface (nm_device);
        if (strcmp (iface, priv->iface) == 0) {
                g_object_set (G_OBJECT (virtual_device),
                              "nm-device", nm_device,
                              NULL);
                g_signal_emit (virtual_device, signals[SIGNAL_DEVICE_SET], 0, nm_device);
                net_object_emit_changed (NET_OBJECT (virtual_device));
                net_object_refresh (NET_OBJECT (virtual_device));
        }
}

static void
device_removed_cb (NMClient *client, NMDevice *nm_device, gpointer user_data)
{
        NetVirtualDevice *virtual_device = user_data;
        NetVirtualDevicePrivate *priv = virtual_device->priv;
        const char *iface;

        iface = nm_device_get_iface (nm_device);
        if (strcmp (iface, priv->iface) == 0) {
                g_object_set (G_OBJECT (virtual_device),
                              "nm-device", NULL,
                              NULL);
                g_signal_emit (virtual_device, signals[SIGNAL_DEVICE_UNSET], 0, nm_device);
                net_object_emit_changed (NET_OBJECT (virtual_device));
                net_object_refresh (NET_OBJECT (virtual_device));
        }
}

static void
device_off_toggled (GtkSwitch *sw,
                    GParamSpec *pspec,
                    gpointer user_data)
{
        NetVirtualDevice *virtual_device = user_data;
        gboolean active;
        NMActiveConnection *ac;
        NMClient *client;
        NMDevice *nm_device;

        if (virtual_device->priv->updating_device)
                return;

        client = net_object_get_client (NET_OBJECT (virtual_device));
        nm_device = net_device_get_nm_device (NET_DEVICE (virtual_device));

        active = gtk_switch_get_active (sw);
        if (active) {
                nm_client_activate_connection (client,
                                               virtual_device->priv->connection,
                                               nm_device,
                                               NULL, NULL, NULL);
        } else {
                g_return_if_fail (nm_device != NULL);

                ac = nm_device_get_active_connection (nm_device);
                g_return_if_fail (ac != NULL);

                nm_client_deactivate_connection (client, ac);
        }
}

static void
edit_connection (GtkButton *button, gpointer virtual_device)
{
        net_object_edit (NET_OBJECT (virtual_device));
}

static void
net_virtual_device_constructed (GObject *object)
{
        NetVirtualDevice *virtual_device = NET_VIRTUAL_DEVICE (object);
        NMClient *client;
        const GPtrArray *devices;
        int i;

        client = net_object_get_client (NET_OBJECT (virtual_device));

        g_signal_connect (client, "device-added",
                          G_CALLBACK (device_added_cb), virtual_device);
        g_signal_connect (client, "device-removed",
                          G_CALLBACK (device_removed_cb), virtual_device);
        devices = nm_client_get_devices (client);
        if (devices) {
                for (i = 0; i < devices->len; i++)
                        device_added_cb (client, devices->pdata[i], virtual_device);
        }

        net_object_refresh (NET_OBJECT (virtual_device));

        G_OBJECT_CLASS (net_virtual_device_parent_class)->constructed (object);
}

static void
net_virtual_device_finalize (GObject *object)
{
        NetVirtualDevice *virtual_device = NET_VIRTUAL_DEVICE (object);
        NetVirtualDevicePrivate *priv = virtual_device->priv;

        if (priv->connection)
                g_object_unref (priv->connection);
        g_object_unref (priv->builder);

        G_OBJECT_CLASS (net_virtual_device_parent_class)->finalize (object);
}

static NMConnection *
net_virtual_device_get_find_connection (NetDevice *device)
{
        NetVirtualDevice *virtual_device = NET_VIRTUAL_DEVICE (device);

        return virtual_device->priv->connection;
}

static void
net_virtual_device_class_init (NetVirtualDeviceClass *klass)
{
        NetDeviceClass *device_class = NET_DEVICE_CLASS (klass);
        NetObjectClass *net_object_class = NET_OBJECT_CLASS (klass);
        GObjectClass *object_class = G_OBJECT_CLASS (klass);
        GParamSpec *pspec;

        object_class->constructed = net_virtual_device_constructed;
        object_class->finalize = net_virtual_device_finalize;
        object_class->get_property = net_virtual_device_get_property;
        object_class->set_property = net_virtual_device_set_property;

        net_object_class->refresh = net_virtual_device_refresh;
        net_object_class->add_to_notebook = net_virtual_device_add_to_notebook;
        net_object_class->delete = net_virtual_device_delete;

        device_class->get_find_connection = net_virtual_device_get_find_connection;

        g_type_class_add_private (klass, sizeof (NetVirtualDevicePrivate));

        signals[SIGNAL_DEVICE_SET] =
                g_signal_new ("device-set",
                              G_TYPE_FROM_CLASS (object_class), G_SIGNAL_RUN_LAST,
                              G_STRUCT_OFFSET (NetVirtualDeviceClass, device_set),
                              NULL, NULL, g_cclosure_marshal_VOID__OBJECT,
                              G_TYPE_NONE, 1, G_TYPE_OBJECT);

        signals[SIGNAL_DEVICE_UNSET] =
                g_signal_new ("device-unset",
                              G_TYPE_FROM_CLASS (object_class), G_SIGNAL_RUN_LAST,
                              G_STRUCT_OFFSET (NetVirtualDeviceClass, device_unset),
                              NULL, NULL, g_cclosure_marshal_VOID__OBJECT,
                              G_TYPE_NONE, 1, G_TYPE_OBJECT);

        pspec = g_param_spec_object ("connection", NULL, NULL,
                                     NM_TYPE_CONNECTION,
                                     G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);
        g_object_class_install_property (object_class, PROP_CONNECTION, pspec);
}

static void
net_virtual_device_init (NetVirtualDevice *virtual_device)
{
        GError *error = NULL;
        GtkWidget *widget;

        virtual_device->priv = NET_VIRTUAL_DEVICE_GET_PRIVATE (virtual_device);

        virtual_device->priv->builder = gtk_builder_new ();
        gtk_builder_add_from_resource (virtual_device->priv->builder,
                                       "/org/gnome/control-center/network/network-simple.ui",
                                       &error);
        if (error != NULL) {
                g_warning ("Could not load interface file: %s", error->message);
                g_error_free (error);
                return;
        }

        widget = GTK_WIDGET (gtk_builder_get_object (virtual_device->priv->builder,
                                                     "label_device"));
        g_object_bind_property (virtual_device, "title", widget, "label", 0);

        widget = GTK_WIDGET (gtk_builder_get_object (virtual_device->priv->builder,
                                                     "device_off_switch"));
        g_signal_connect (widget, "notify::active",
                          G_CALLBACK (device_off_toggled), virtual_device);

        widget = GTK_WIDGET (gtk_builder_get_object (virtual_device->priv->builder,
                                                     "button_options"));
        g_signal_connect (widget, "clicked",
                          G_CALLBACK (edit_connection), virtual_device);
}

void
net_virtual_device_add_row (NetVirtualDevice *virtual_device,
                            const char       *label_string,
                            const char       *property_name)
{
        NetVirtualDevicePrivate *priv = virtual_device->priv;
        GtkGrid *grid;
        GtkWidget *label, *value;
        GtkStyleContext *context;
        gint top_attach;

        grid = GTK_GRID (gtk_builder_get_object (priv->builder, "grid"));

        label = gtk_label_new (label_string);
        gtk_misc_set_alignment (GTK_MISC (label), 1.0, 0.5);
        gtk_container_add (GTK_CONTAINER (grid), label);

        context = gtk_widget_get_style_context (label);
        gtk_style_context_add_class (context, "dim-label");
        gtk_widget_show (label);

        gtk_container_child_get (GTK_CONTAINER (grid), label,
                                 "top-attach", &top_attach,
                                 NULL);

        value = gtk_label_new (NULL);
        gtk_misc_set_alignment (GTK_MISC (value), 0.0, 0.5);
        g_object_bind_property (virtual_device, property_name, value, "label", 0);
        gtk_label_set_mnemonic_widget (GTK_LABEL (label), value);
        gtk_grid_attach (grid, value, 1, top_attach, 1, 1);
        gtk_widget_show (value);
}
