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

#include <nm-client.h>
#include <nm-device.h>
#include <nm-device-ethernet.h>
#include <nm-remote-connection.h>

#include "panel-common.h"

#include "net-device-wired.h"

#define NET_DEVICE_WIRED_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), NET_TYPE_DEVICE_WIRED, NetDeviceWiredPrivate))

struct _NetDeviceWiredPrivate
{
        GtkBuilder              *builder;
        gboolean                 updating_device;
};

G_DEFINE_TYPE (NetDeviceWired, net_device_wired, NET_TYPE_DEVICE)

static GtkWidget *
device_wired_proxy_add_to_notebook (NetObject *object,
                                    GtkNotebook *notebook,
                                    GtkSizeGroup *heading_size_group)
{
        GtkWidget *widget;
        GtkWindow *window;
        NetDeviceWired *device_wired = NET_DEVICE_WIRED (object);

        /* add widgets to size group */
        widget = GTK_WIDGET (gtk_builder_get_object (device_wired->priv->builder,
                                                     "heading_ipv4"));
        gtk_size_group_add_widget (heading_size_group, widget);

        /* reparent */
        window = GTK_WINDOW (gtk_builder_get_object (device_wired->priv->builder,
                                                     "window_tmp"));
        widget = GTK_WIDGET (gtk_builder_get_object (device_wired->priv->builder,
                                                     "vbox6"));
        g_object_ref (widget);
        gtk_container_remove (GTK_CONTAINER (window), widget);
        gtk_notebook_append_page (notebook, widget, NULL);
        g_object_unref (widget);
        return widget;
}

static void
update_off_switch_from_device_state (GtkSwitch *sw,
                                     NMDeviceState state,
                                     NetDeviceWired *device_wired)
{
        device_wired->priv->updating_device = TRUE;
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
        device_wired->priv->updating_device = FALSE;
}

static void
nm_device_wired_refresh_ui (NetDeviceWired *device_wired)
{
        const char *str;
        GString *status;
        GtkWidget *widget;
        guint speed = 0;
        NMDevice *nm_device;
        NMDeviceState state;
        NetDeviceWiredPrivate *priv = device_wired->priv;

        /* set device kind */
        nm_device = net_device_get_nm_device (NET_DEVICE (device_wired));
        widget = GTK_WIDGET (gtk_builder_get_object (priv->builder, "label_device"));
        gtk_label_set_label (GTK_LABEL (widget),
                             panel_device_to_localized_string (nm_device));

        /* set up the device on/off switch */
        widget = GTK_WIDGET (gtk_builder_get_object (priv->builder, "device_off_switch"));
        state = nm_device_get_state (nm_device);
        gtk_widget_set_visible (widget,
                                state != NM_DEVICE_STATE_UNAVAILABLE
                                && state != NM_DEVICE_STATE_UNMANAGED);
        update_off_switch_from_device_state (GTK_SWITCH (widget), state, device_wired);

        /* set device state, with status and optionally speed */
        widget = GTK_WIDGET (gtk_builder_get_object (priv->builder, "label_status"));
        status = g_string_new (panel_device_state_to_localized_string (nm_device));
        if (state != NM_DEVICE_STATE_UNAVAILABLE)
                speed = nm_device_ethernet_get_speed (NM_DEVICE_ETHERNET (nm_device));
        if (speed  > 0) {
                g_string_append (status, " - ");
                /* Translators: network device speed */
                g_string_append_printf (status, _("%d Mb/s"), speed);
        }
        gtk_label_set_label (GTK_LABEL (widget), status->str);
        g_string_free (status, TRUE);
        gtk_widget_set_tooltip_text (widget,
                                     panel_device_state_reason_to_localized_string (nm_device));

        /* device MAC */
        str = nm_device_ethernet_get_hw_address (NM_DEVICE_ETHERNET (nm_device));
        panel_set_device_widget_details (priv->builder, "mac", str);

        /* set IP entries */
        panel_set_device_widgets (priv->builder, nm_device);
}

static void
device_wired_refresh (NetObject *object)
{
        NetDeviceWired *device_wired = NET_DEVICE_WIRED (object);
        nm_device_wired_refresh_ui (device_wired);
}

static void
device_off_toggled (GtkSwitch *sw,
                    GParamSpec *pspec,
                    NetDeviceWired *device_wired)
{
        const gchar *path;
        const GPtrArray *acs;
        gboolean active;
        gint i;
        NMActiveConnection *a;
        NMConnection *connection;
        NMClient *client;

        if (device_wired->priv->updating_device)
                return;

        active = gtk_switch_get_active (sw);
        if (active) {
                client = net_object_get_client (NET_OBJECT (device_wired));
                connection = net_device_get_find_connection (NET_DEVICE (device_wired));
                if (connection == NULL)
                        return;
                nm_client_activate_connection (client,
                                               connection,
                                               net_device_get_nm_device (NET_DEVICE (device_wired)),
                                               NULL, NULL, NULL);
        } else {
                connection = net_device_get_find_connection (NET_DEVICE (device_wired));
                if (connection == NULL)
                        return;
                path = nm_connection_get_path (connection);
                client = net_object_get_client (NET_OBJECT (device_wired));
                acs = nm_client_get_active_connections (client);
                for (i = 0; i < acs->len; i++) {
                        a = (NMActiveConnection*)acs->pdata[i];
                        if (strcmp (nm_active_connection_get_connection (a), path) == 0) {
                                nm_client_deactivate_connection (client, a);
                                break;
                        }
                }
        }
}

static void
edit_connection (GtkButton *button, NetDeviceWired *device_wired)
{
        net_object_edit (NET_OBJECT (device_wired));
}

static void
net_device_wired_constructed (GObject *object)
{
        NetDeviceWired *device_wired = NET_DEVICE_WIRED (object);

        G_OBJECT_CLASS (net_device_wired_parent_class)->constructed (object);

        nm_device_wired_refresh_ui (device_wired);
}

static void
net_device_wired_finalize (GObject *object)
{
        NetDeviceWired *device_wired = NET_DEVICE_WIRED (object);
        NetDeviceWiredPrivate *priv = device_wired->priv;

        g_object_unref (priv->builder);

        G_OBJECT_CLASS (net_device_wired_parent_class)->finalize (object);
}

static void
net_device_wired_class_init (NetDeviceWiredClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);
        NetObjectClass *parent_class = NET_OBJECT_CLASS (klass);

        object_class->finalize = net_device_wired_finalize;
        object_class->constructed = net_device_wired_constructed;
        parent_class->add_to_notebook = device_wired_proxy_add_to_notebook;
        parent_class->refresh = device_wired_refresh;
        g_type_class_add_private (klass, sizeof (NetDeviceWiredPrivate));
}

static void
net_device_wired_init (NetDeviceWired *device_wired)
{
        GError *error = NULL;
        GtkWidget *widget;

        device_wired->priv = NET_DEVICE_WIRED_GET_PRIVATE (device_wired);

        device_wired->priv->builder = gtk_builder_new ();
        gtk_builder_add_from_file (device_wired->priv->builder,
                                   GNOMECC_UI_DIR "/network-wired.ui",
                                   &error);
        if (error != NULL) {
                g_warning ("Could not load interface file: %s", error->message);
                g_error_free (error);
                return;
        }

        /* setup wired combobox model */
        widget = GTK_WIDGET (gtk_builder_get_object (device_wired->priv->builder,
                                                     "device_off_switch"));
        g_signal_connect (widget, "notify::active",
                          G_CALLBACK (device_off_toggled), device_wired);

        widget = GTK_WIDGET (gtk_builder_get_object (device_wired->priv->builder,
                                                     "button_options"));
        g_signal_connect (widget, "clicked",
                          G_CALLBACK (edit_connection), device_wired);
}
