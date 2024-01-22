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
#include "connection-editor/ce-page.h"

#include "net-device-ethernet.h"

struct _NetDeviceEthernet
{
        AdwPreferencesGroup parent;

        GtkListBox         *connection_list;
        GtkStack           *connection_stack;
        GtkButton          *details_button;
        GtkListBox         *details_listbox;
        AdwSwitchRow       *details_row;

        NMClient           *client;
        NMDevice           *device;
        gboolean            updating_device;
        GHashTable         *connections;
};

G_DEFINE_TYPE (NetDeviceEthernet, net_device_ethernet, ADW_TYPE_PREFERENCES_GROUP)

static void populate_ui (NetDeviceEthernet *self);

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
device_ethernet_refresh_ui (NetDeviceEthernet *self)
{
        NMDeviceState state;
        g_autofree gchar *speed_text = NULL;
        g_autofree gchar *status = NULL;

        state = nm_device_get_state (self->device);
        gtk_widget_set_sensitive (GTK_WIDGET (self->details_row),
                                  state != NM_DEVICE_STATE_UNAVAILABLE
                                  && state != NM_DEVICE_STATE_UNMANAGED);
        self->updating_device = TRUE;
        adw_switch_row_set_active (self->details_row, device_state_to_off_switch (state));
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
        adw_preferences_row_set_title (ADW_PREFERENCES_ROW (self->details_row), status);

        populate_ui (self);
}

static void
editor_done (NetDeviceEthernet *self)
{
        device_ethernet_refresh_ui (self);
}

static void
show_details (NetDeviceEthernet *self, GtkButton *button, const gchar *title)
{
        GtkWidget *row;
        NMConnection *connection;
        NetConnectionEditor *editor;

        row = g_object_get_data (G_OBJECT (button), "row");
        connection = NM_CONNECTION (g_object_get_data (G_OBJECT (row), "connection"));

        editor = net_connection_editor_new (connection, self->device, NULL, self->client);
        gtk_window_set_transient_for (GTK_WINDOW (editor), GTK_WINDOW (gtk_widget_get_native (GTK_WIDGET (self))));
        if (title)
                net_connection_editor_set_title (editor, title);
        g_signal_connect_object (editor, "done", G_CALLBACK (editor_done), self, G_CONNECT_SWAPPED);
        gtk_window_present (GTK_WINDOW (editor));
}

static void
show_details_for_row (NetDeviceEthernet *self, GtkButton *button)
{
        show_details (self, button, NULL);
}

static void
details_button_clicked_cb (NetDeviceEthernet *self)
{
        /* Translators: This is used as the title of the connection
         * details window for ethernet, if there is only a single
         * profile. It is also used to display ethernet in the
         * device list.
         */
        show_details (self, self->details_button, NULL);
}

static void
add_row (NetDeviceEthernet *self, NMConnection *connection)
{
        GtkWidget *row;
        GtkWidget *widget;
        NMActiveConnection *aconn;
        gboolean active;

        active = FALSE;

        aconn = nm_device_get_active_connection (self->device);
        if (aconn) {
                const gchar *uuid1, *uuid2;
                uuid1 = nm_active_connection_get_uuid (aconn);
                uuid2 = nm_connection_get_uuid (connection);
                active = g_strcmp0 (uuid1, uuid2) == 0;
        }

        row = adw_switch_row_new ();
        adw_preferences_row_set_title (ADW_PREFERENCES_ROW (row), nm_connection_get_id (connection));
        adw_action_row_set_icon_name (ADW_ACTION_ROW (row), active ? "network-wired-symbolic" : "network-wired-disconnected-symbolic");
        adw_switch_row_set_active (ADW_SWITCH_ROW (row), active);

        widget = gtk_button_new_from_icon_name ("emblem-system-symbolic");
        gtk_widget_add_css_class (widget, "flat");
        gtk_widget_set_valign (widget, GTK_ALIGN_CENTER);
        gtk_accessible_update_property (GTK_ACCESSIBLE (widget),
                                        GTK_ACCESSIBLE_PROPERTY_LABEL, _("Options…"),
                                        -1);
        g_object_set_data (G_OBJECT (widget), "edit", widget);
        g_object_set_data (G_OBJECT (widget), "row", row);
        g_signal_connect_object (widget, "clicked", G_CALLBACK (show_details_for_row), self, G_CONNECT_SWAPPED);
        adw_action_row_add_suffix (ADW_ACTION_ROW (row), widget);

        g_object_set_data (G_OBJECT (row), "connection", connection);

        gtk_list_box_append (self->connection_list, row);
}

static void
connection_removed (NetDeviceEthernet  *self, NMRemoteConnection *connection)
{
        if (g_hash_table_remove (self->connections, connection))
                device_ethernet_refresh_ui (self);
}

static void
populate_ui (NetDeviceEthernet *self)
{
        GSList *connections, *l;
        NMConnection *connection;
        GtkWidget *child;
        gint n_connections;

        while ((child = gtk_widget_get_first_child (GTK_WIDGET (self->connection_list))) != NULL)
               gtk_list_box_remove (self->connection_list, child);

        connections = net_device_get_valid_connections (self->client, self->device);
        for (l = connections; l; l = l->next) {
                NMConnection *connection = l->data;
                if (!g_hash_table_contains (self->connections, connection)) {
                        g_hash_table_add (self->connections, connection);
                }
        }
        n_connections = g_slist_length (connections);

        if (n_connections > 1) {
                for (l = connections; l; l = l->next) {
                        NMConnection *connection = l->data;
                        add_row (self, connection);
                }
                gtk_stack_set_visible_child (self->connection_stack,
                                             GTK_WIDGET (self->connection_list));
        } else if (n_connections == 1) {
                connection = connections->data;
                gtk_stack_set_visible_child (self->connection_stack,
                                             GTK_WIDGET (self->details_listbox));
                g_object_set_data (G_OBJECT (self->details_button), "row", self->details_button);
                g_object_set_data (G_OBJECT (self->details_button), "connection", connection);

        }

        gtk_widget_set_visible (GTK_WIDGET (self->connection_stack), n_connections >= 1);

        g_slist_free (connections);
}

static void
client_connection_added_cb (NetDeviceEthernet  *self)
{
        device_ethernet_refresh_ui (self);
}

static void
add_profile_button_clicked_cb (NetDeviceEthernet *self)
{
        NMConnection *connection;
        NMSettingConnection *sc;
        g_autofree gchar *uuid = NULL;
        g_autofree gchar *id = NULL;
        NetConnectionEditor *editor;
        const GPtrArray *connections;
        const char *iface;

        connection = nm_simple_connection_new ();
        sc = NM_SETTING_CONNECTION (nm_setting_connection_new ());
        nm_connection_add_setting (connection, NM_SETTING (sc));

        uuid = nm_utils_uuid_generate ();

        connections = nm_client_get_connections (self->client);
        id = ce_page_get_next_available_name (connections, NAME_FORMAT_PROFILE, NULL);

        g_object_set (sc,
                      NM_SETTING_CONNECTION_UUID, uuid,
                      NM_SETTING_CONNECTION_ID, id,
                      NM_SETTING_CONNECTION_TYPE, NM_SETTING_WIRED_SETTING_NAME,
                      NM_SETTING_CONNECTION_AUTOCONNECT, TRUE,
                      NULL);

        iface = nm_device_get_iface (self->device);
        g_object_set (sc,
                      NM_SETTING_CONNECTION_INTERFACE_NAME, iface,
                      NULL);

        nm_connection_add_setting (connection, nm_setting_wired_new ());

        editor = net_connection_editor_new (connection, self->device, NULL, self->client);
        gtk_window_set_transient_for (GTK_WINDOW (editor), GTK_WINDOW (gtk_widget_get_native (GTK_WIDGET (self))));
        g_signal_connect_object (editor, "done", G_CALLBACK (editor_done), self, G_CONNECT_SWAPPED);
        gtk_window_present (GTK_WINDOW (editor));
}

static void
device_off_switch_changed_cb (NetDeviceEthernet *self)
{
        NMConnection *connection;

        if (self->updating_device)
                return;

        if (adw_switch_row_get_active (self->details_row)) {
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

static void
connection_list_row_activated_cb (NetDeviceEthernet *self, GtkListBoxRow *row)
{
        NMConnection *connection;
        GtkWidget *child;

        if (!NM_IS_DEVICE_ETHERNET (self->device) ||
            !nm_device_ethernet_get_carrier (NM_DEVICE_ETHERNET (self->device)))
                return;

        child = gtk_list_box_row_get_child (GTK_LIST_BOX_ROW (row));
        connection = NM_CONNECTION (g_object_get_data (G_OBJECT (child), "connection"));

        nm_client_activate_connection_async (self->client,
                                             connection,
                                             self->device,
                                             NULL, NULL, NULL, NULL);
}

static void
device_ethernet_finalize (GObject *object)
{
        NetDeviceEthernet *self = NET_DEVICE_ETHERNET (object);

        g_clear_object (&self->client);
        g_clear_object (&self->device);
        g_hash_table_destroy (self->connections);

        G_OBJECT_CLASS (net_device_ethernet_parent_class)->finalize (object);
}

static void
net_device_ethernet_class_init (NetDeviceEthernetClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);
        GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

        object_class->finalize = device_ethernet_finalize;

        gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/network/network-ethernet.ui");

        gtk_widget_class_bind_template_child (widget_class, NetDeviceEthernet, connection_list);
        gtk_widget_class_bind_template_child (widget_class, NetDeviceEthernet, connection_stack);
        gtk_widget_class_bind_template_child (widget_class, NetDeviceEthernet, details_button);
        gtk_widget_class_bind_template_child (widget_class, NetDeviceEthernet, details_listbox);
        gtk_widget_class_bind_template_child (widget_class, NetDeviceEthernet, details_row);

        gtk_widget_class_bind_template_callback (widget_class, connection_list_row_activated_cb);
        gtk_widget_class_bind_template_callback (widget_class, device_off_switch_changed_cb);
        gtk_widget_class_bind_template_callback (widget_class, details_button_clicked_cb);
        gtk_widget_class_bind_template_callback (widget_class, add_profile_button_clicked_cb);
}

static void
net_device_ethernet_init (NetDeviceEthernet *self)
{
        gtk_widget_init_template (GTK_WIDGET (self));

        self->connections = g_hash_table_new (NULL, NULL);
}

NetDeviceEthernet *
net_device_ethernet_new (NMClient *client, NMDevice *device)
{
        NetDeviceEthernet *self;

        self = g_object_new (net_device_ethernet_get_type (), NULL);
        self->client = g_object_ref (client);
        self->device = g_object_ref (device);

        g_signal_connect_object (client, NM_CLIENT_CONNECTION_ADDED,
                                 G_CALLBACK (client_connection_added_cb), self, G_CONNECT_SWAPPED);
        g_signal_connect_object (client, NM_CLIENT_CONNECTION_REMOVED,
                                 G_CALLBACK (connection_removed), self, G_CONNECT_SWAPPED);

        g_signal_connect_object (device, "state-changed", G_CALLBACK (device_ethernet_refresh_ui), self, G_CONNECT_SWAPPED);

        device_ethernet_refresh_ui (self);

        return self;
}

NMDevice *
net_device_ethernet_get_device (NetDeviceEthernet *self)
{
        g_return_val_if_fail (NET_IS_DEVICE_ETHERNET (self), NULL);
        return self->device;
}

void
net_device_ethernet_set_title (NetDeviceEthernet *self,
                               const gchar       *title)
{
        g_return_if_fail (NET_IS_DEVICE_ETHERNET (self));

        adw_preferences_row_set_title (ADW_PREFERENCES_ROW (self->details_row), title);
        if (gtk_stack_get_visible_child (self->connection_stack) == GTK_WIDGET (self->connection_list)) {
                adw_preferences_group_set_title (ADW_PREFERENCES_GROUP (self), title);
        }
}
