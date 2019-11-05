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

#include "net-device-simple.h"

typedef struct
{
        GtkBuilder   *builder;
        GtkBox       *box;
        GtkLabel     *device_label;
        GtkSwitch    *device_off_switch;
        GtkGrid      *grid;
        GtkButton    *options_button;
        GtkSeparator *separator;

        gboolean    updating_device;
} NetDeviceSimplePrivate;

G_DEFINE_TYPE_WITH_PRIVATE (NetDeviceSimple, net_device_simple, NET_TYPE_DEVICE)

void
net_device_simple_set_show_separator (NetDeviceSimple *self,
                                      gboolean         show_separator)
{
        NetDeviceSimplePrivate *priv = net_device_simple_get_instance_private (self);

        /* add widgets to size group */
        gtk_widget_set_visible (GTK_WIDGET (priv->separator), show_separator);
}

static GtkWidget *
device_simple_proxy_add_to_stack (NetObject    *object,
                                  GtkStack     *stack,
                                  GtkSizeGroup *heading_size_group)
{
        NetDeviceSimple *self = NET_DEVICE_SIMPLE (object);
        NetDeviceSimplePrivate *priv = net_device_simple_get_instance_private (self);

        /* add widgets to size group */
        gtk_stack_add_named (stack, GTK_WIDGET (priv->box), net_object_get_id (object));
        return GTK_WIDGET (priv->box);
}

static void
update_off_switch_from_device_state (GtkSwitch *sw,
                                     NMDeviceState state,
                                     NetDeviceSimple *self)
{
        NetDeviceSimplePrivate *priv = net_device_simple_get_instance_private (self);

        priv->updating_device = TRUE;
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
        priv->updating_device = FALSE;
}

static void
nm_device_simple_refresh_ui (NetDeviceSimple *self)
{
        NetDeviceSimplePrivate *priv = net_device_simple_get_instance_private (self);
        NMDevice *nm_device;
        NMDeviceState state;

        nm_device = net_device_get_nm_device (NET_DEVICE (self));

        /* set device kind */
        g_object_bind_property (self, "title", priv->device_label, "label", 0);

        /* set up the device on/off switch */
        state = nm_device_get_state (nm_device);
        gtk_widget_set_visible (GTK_WIDGET (priv->device_off_switch),
                                state != NM_DEVICE_STATE_UNAVAILABLE
                                && state != NM_DEVICE_STATE_UNMANAGED);
        update_off_switch_from_device_state (priv->device_off_switch, state, self);

        /* set up the Options button */
        gtk_widget_set_visible (GTK_WIDGET (priv->options_button), state != NM_DEVICE_STATE_UNMANAGED && g_find_program_in_path ("nm-connection-editor") != NULL);
}

static void
device_simple_refresh (NetObject *object)
{
        NetDeviceSimple *self = NET_DEVICE_SIMPLE (object);
        nm_device_simple_refresh_ui (self);
}

static void
device_off_toggled (NetDeviceSimple *self)
{
        NetDeviceSimplePrivate *priv = net_device_simple_get_instance_private (self);
        const GPtrArray *acs;
        gboolean active;
        gint i;
        NMActiveConnection *a;
        NMConnection *connection;
        NMClient *client;

        if (priv->updating_device)
                return;

        active = gtk_switch_get_active (priv->device_off_switch);
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
edit_connection (NetDeviceSimple *self)
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
net_device_simple_constructed (GObject *object)
{
        NetDeviceSimple *self = NET_DEVICE_SIMPLE (object);

        G_OBJECT_CLASS (net_device_simple_parent_class)->constructed (object);

        net_object_refresh (NET_OBJECT (self));
}

static void
net_device_simple_finalize (GObject *object)
{
        NetDeviceSimple *self = NET_DEVICE_SIMPLE (object);
        NetDeviceSimplePrivate *priv = net_device_simple_get_instance_private (self);

        g_object_unref (priv->builder);

        G_OBJECT_CLASS (net_device_simple_parent_class)->finalize (object);
}

static char *
device_simple_get_speed (NetDeviceSimple *simple)
{
        return NULL;
}

static void
net_device_simple_class_init (NetDeviceSimpleClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);
        NetObjectClass *parent_class = NET_OBJECT_CLASS (klass);
        NetDeviceSimpleClass *simple_class = NET_DEVICE_SIMPLE_CLASS (klass);

        object_class->finalize = net_device_simple_finalize;
        object_class->constructed = net_device_simple_constructed;
        parent_class->add_to_stack = device_simple_proxy_add_to_stack;
        parent_class->refresh = device_simple_refresh;
        simple_class->get_speed = device_simple_get_speed;
}

static void
net_device_simple_init (NetDeviceSimple *self)
{
        NetDeviceSimplePrivate *priv = net_device_simple_get_instance_private (self);
        g_autoptr(GError) error = NULL;

        priv->builder = gtk_builder_new ();
        gtk_builder_add_from_resource (priv->builder,
                                       "/org/gnome/control-center/network/network-simple.ui",
                                       &error);
        if (error != NULL) {
                g_warning ("Could not load interface file: %s", error->message);
                return;
        }

        priv->box = GTK_BOX (gtk_builder_get_object (priv->builder, "box"));
        priv->device_label = GTK_LABEL (gtk_builder_get_object (priv->builder, "device_label"));
        priv->device_off_switch = GTK_SWITCH (gtk_builder_get_object (priv->builder, "device_off_switch"));
        priv->grid = GTK_GRID (gtk_builder_get_object (priv->builder, "grid"));
        priv->options_button = GTK_BUTTON (gtk_builder_get_object (priv->builder, "options_button"));
        priv->separator = GTK_SEPARATOR (gtk_builder_get_object (priv->builder, "separator"));

        /* setup simple combobox model */
        g_signal_connect_swapped (priv->device_off_switch, "notify::active",
                                  G_CALLBACK (device_off_toggled), self);

        g_signal_connect_swapped (priv->options_button, "clicked",
                                  G_CALLBACK (edit_connection), self);
        gtk_widget_set_visible (GTK_WIDGET (priv->options_button), g_find_program_in_path ("nm-connection-editor") != NULL);
}

char *
net_device_simple_get_speed (NetDeviceSimple *self)
{
        NetDeviceSimpleClass *klass = NET_DEVICE_SIMPLE_GET_CLASS (self);

        return klass->get_speed (self);
}

void
net_device_simple_add_row (NetDeviceSimple *self,
                           const char      *label_string,
                           const char      *property_name)
{
        NetDeviceSimplePrivate *priv = net_device_simple_get_instance_private (self);
        GtkWidget *label, *value;
        GtkStyleContext *context;
        gint top_attach;

        label = gtk_label_new (label_string);
        gtk_widget_set_halign (label, GTK_ALIGN_END);
        gtk_container_add (GTK_CONTAINER (priv->grid), label);

        context = gtk_widget_get_style_context (label);
        gtk_style_context_add_class (context, "dim-label");
        gtk_widget_show (label);

        gtk_container_child_get (GTK_CONTAINER (priv->grid), label,
                                 "top-attach", &top_attach,
                                 NULL);

        value = gtk_label_new (NULL);
        gtk_widget_set_halign (value, GTK_ALIGN_START);
        g_object_bind_property (self, property_name, value, "label", 0);
        gtk_label_set_mnemonic_widget (GTK_LABEL (label), value);
        gtk_grid_attach (priv->grid, value, 1, top_attach, 1, 1);
        gtk_widget_show (value);
}

