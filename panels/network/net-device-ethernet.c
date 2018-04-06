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

#include "list-box-helper.h"
#include "connection-editor/net-connection-editor.h"
#include "connection-editor/ce-page.h"

#include "net-device-ethernet.h"

G_DEFINE_TYPE (NetDeviceEthernet, net_device_ethernet, NET_TYPE_DEVICE_SIMPLE)

static char *
device_ethernet_get_speed (NetDeviceSimple *device_simple)
{
        NMDevice *nm_device;
        guint speed;

        nm_device = net_device_get_nm_device (NET_DEVICE (device_simple));

        speed = nm_device_ethernet_get_speed (NM_DEVICE_ETHERNET (nm_device));
        if (speed > 0) {
                /* Translators: network device speed */
                return g_strdup_printf (_("%d Mb/s"), speed);
        } else
                return NULL;
}

static GtkWidget *
device_ethernet_add_to_stack (NetObject    *object,
                              GtkStack     *stack,
                              GtkSizeGroup *heading_size_group)
{
        NetDeviceEthernet *device = NET_DEVICE_ETHERNET (object);
        GtkWidget *vbox;

        vbox = GTK_WIDGET (gtk_builder_get_object (device->builder, "vbox6"));
        gtk_stack_add_named (stack, vbox, net_object_get_id (object));
        return vbox;
}

static void
add_details_row (GtkWidget *details, gint top, const gchar *heading, const gchar *value)
{
        GtkWidget *heading_label;
        GtkWidget *value_label;

        heading_label = gtk_label_new (heading);
        gtk_style_context_add_class (gtk_widget_get_style_context (heading_label), "dim-label");
        gtk_widget_set_halign (heading_label, GTK_ALIGN_END);
        gtk_widget_set_hexpand (heading_label, TRUE);

        gtk_grid_attach (GTK_GRID (details), heading_label, 0, top, 1, 1);

        value_label = gtk_label_new (value);
        gtk_widget_set_halign (value_label, GTK_ALIGN_START);
        gtk_widget_set_hexpand (value_label, TRUE);
        gtk_label_set_selectable (GTK_LABEL (value_label), TRUE);

        gtk_label_set_mnemonic_widget (GTK_LABEL (heading_label), value_label);

        gtk_grid_attach (GTK_GRID (details), value_label, 1, top, 1, 1);
}

static gchar *
get_last_used_string (NMConnection *connection)
{
        gchar *last_used = NULL;
        GDateTime *now = NULL;
        GDateTime *then = NULL;
        gint days;
        GTimeSpan diff;
        guint64 timestamp;
        NMSettingConnection *s_con;

        s_con = nm_connection_get_setting_connection (connection);
        if (s_con == NULL)
                goto out;
        timestamp = nm_setting_connection_get_timestamp (s_con);
        if (timestamp == 0) {
                last_used = g_strdup (_("never"));
                goto out;
        }

        /* calculate the amount of time that has elapsed */
        now = g_date_time_new_now_utc ();
        then = g_date_time_new_from_unix_utc (timestamp);
        diff = g_date_time_difference  (now, then);
        days = diff / G_TIME_SPAN_DAY;
        if (days == 0)
                last_used = g_strdup (_("today"));
        else if (days == 1)
                last_used = g_strdup (_("yesterday"));
        else
                last_used = g_strdup_printf (ngettext ("%i day ago", "%i days ago", days), days);
out:
        if (now != NULL)
                g_date_time_unref (now);
        if (then != NULL)
                g_date_time_unref (then);

        return last_used;
}

static void
add_details (GtkWidget *details, NMDevice *device, NMConnection *connection)
{
        NMIPConfig *ip4_config = NULL;
        NMIPConfig *ip6_config = NULL;
        gchar *ip4_address = NULL;
        gchar *ip4_route = NULL;
        gchar *ip4_dns = NULL;
        gchar *ip6_address = NULL;
        gint i = 0;

        ip4_config = nm_device_get_ip4_config (device);
        if (ip4_config) {
                ip4_address = panel_get_ip4_address_as_string (ip4_config, "address");
                ip4_route = panel_get_ip4_address_as_string (ip4_config, "gateway");
                ip4_dns = panel_get_ip4_dns_as_string (ip4_config);
        }
        ip6_config = nm_device_get_ip6_config (device);
        if (ip6_config) {
                ip6_address = panel_get_ip6_address_as_string (ip6_config);
        }

        if (ip4_address && ip6_address) {
                add_details_row (details, i++, _("IPv4 Address"), ip4_address);
                add_details_row (details, i++, _("IPv6 Address"), ip6_address);
        } else if (ip4_address) {
                add_details_row (details, i++, _("IP Address"), ip4_address);
        } else if (ip6_address) {
                add_details_row (details, i++, _("IPv6 Address"), ip6_address);
        }

        add_details_row (details, i++, _("Hardware Address"),
                         nm_device_ethernet_get_hw_address (NM_DEVICE_ETHERNET (device)));

        if (ip4_route)
                add_details_row (details, i++, _("Default Route"), ip4_route);
        if (ip4_dns)
                add_details_row (details, i++, _("DNS"), ip4_dns);

        if (nm_device_get_state (device) != NM_DEVICE_STATE_ACTIVATED) {
                gchar *last_used;
                last_used = get_last_used_string (connection);
                add_details_row (details, i++, _("Last used"), last_used);
                g_free (last_used);
        }

        g_free (ip4_address);
        g_free (ip4_route);
        g_free (ip4_dns);
        g_free (ip6_address);
}

static void populate_ui (NetDeviceEthernet *device);

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
device_ethernet_refresh_ui (NetDeviceEthernet *device)
{
        NMDevice *nm_device;
        NMDeviceState state;
        GtkWidget *widget;
        gchar *speed = NULL;

        nm_device = net_device_get_nm_device (NET_DEVICE (device));

        widget = GTK_WIDGET (gtk_builder_get_object (device->builder, "label_device"));
        gtk_label_set_label (GTK_LABEL (widget), net_object_get_title (NET_OBJECT (device)));

        widget = GTK_WIDGET (gtk_builder_get_object (device->builder, "device_off_switch"));
        state = nm_device_get_state (nm_device);
        gtk_widget_set_sensitive (widget,
                                  state != NM_DEVICE_STATE_UNAVAILABLE
                                  && state != NM_DEVICE_STATE_UNMANAGED);
        device->updating_device = TRUE;
        gtk_switch_set_active (GTK_SWITCH (widget), device_state_to_off_switch (state));
        device->updating_device = FALSE;

        if (state != NM_DEVICE_STATE_UNAVAILABLE)
                speed = net_device_simple_get_speed (NET_DEVICE_SIMPLE (device));
        panel_set_device_status (device->builder, "label_status", nm_device, speed);

        populate_ui (device);
}

static void
editor_done (NetConnectionEditor *editor,
             gboolean             success,
             NetDeviceEthernet   *device)
{
        g_object_unref (editor);
        device_ethernet_refresh_ui (device);
}

static void
show_details (GtkButton *button, NetDeviceEthernet *device, const gchar *title)
{
        GtkWidget *row;
        NMConnection *connection;
        GtkWidget *window;
        NetConnectionEditor *editor;
        NMClient *client;
        NMDevice *nmdev;

        window = gtk_widget_get_toplevel (GTK_WIDGET (button));

        row = g_object_get_data (G_OBJECT (button), "row");
        connection = NM_CONNECTION (g_object_get_data (G_OBJECT (row), "connection"));

        nmdev = net_device_get_nm_device (NET_DEVICE (device));
        client = net_object_get_client (NET_OBJECT (device));
        editor = net_connection_editor_new (GTK_WINDOW (window), connection, nmdev, NULL, client);
        if (title)
                net_connection_editor_set_title (editor, title);
        g_signal_connect (editor, "done", G_CALLBACK (editor_done), device);
        net_connection_editor_run (editor);
}

static void
show_details_for_row (GtkButton *button, NetDeviceEthernet *device)
{
        show_details (button, device, NULL);
}

static void
show_details_for_wired (GtkButton *button, NetDeviceEthernet *device)
{
        /* Translators: This is used as the title of the connection
         * details window for ethernet, if there is only a single
         * profile. It is also used to display ethernet in the
         * device list.
         */
        show_details (button, device, _("Wired"));
}

static void
add_row (NetDeviceEthernet *device, NMConnection *connection)
{
        GtkWidget *row;
        GtkWidget *widget;
        GtkWidget *box;
        GtkWidget *details;
        NMDevice *nmdev;
        NMActiveConnection *aconn;
        gboolean active;
        GtkWidget *image;

        active = FALSE;

        nmdev = net_device_get_nm_device (NET_DEVICE (device));
        aconn = nm_device_get_active_connection (nmdev);
        if (aconn) {
                const gchar *uuid1, *uuid2;
                uuid1 = nm_active_connection_get_uuid (aconn);
                uuid2 = nm_connection_get_uuid (connection);
                active = g_strcmp0 (uuid1, uuid2) == 0;
        }

        row = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
        box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
        gtk_box_pack_start (GTK_BOX (row), box, FALSE, TRUE, 0);
        widget = gtk_label_new (nm_connection_get_id (connection));
        gtk_widget_set_margin_start (widget, 12);
        gtk_widget_set_margin_end (widget, 12);
        gtk_widget_set_margin_top (widget, 12);
        gtk_widget_set_margin_bottom (widget, 12);
        gtk_box_pack_start (GTK_BOX (box), widget, FALSE, TRUE, 0);

        if (active) {
                widget = gtk_image_new_from_icon_name ("object-select-symbolic", GTK_ICON_SIZE_MENU);
                gtk_widget_set_halign (widget, GTK_ALIGN_CENTER);
                gtk_widget_set_valign (widget, GTK_ALIGN_CENTER);
                gtk_box_pack_start (GTK_BOX (box), widget, FALSE, TRUE, 0);

                details = gtk_grid_new ();
                gtk_grid_set_row_spacing (GTK_GRID (details), 10);
                gtk_grid_set_column_spacing (GTK_GRID (details), 10);

                gtk_box_pack_start (GTK_BOX (row), details, FALSE, TRUE, 0);

                add_details (details, nmdev, connection);
        }

        /* filler */
        widget = gtk_label_new ("");
        gtk_widget_set_hexpand (widget, TRUE);
        gtk_box_pack_start (GTK_BOX (box), widget, TRUE, TRUE, 0);

        image = gtk_image_new_from_icon_name ("emblem-system-symbolic", GTK_ICON_SIZE_MENU);
        gtk_widget_show (image);
        widget = gtk_button_new ();
        gtk_style_context_add_class (gtk_widget_get_style_context (widget), "image-button");
        gtk_widget_set_margin_start (widget, 12);
        gtk_widget_set_margin_end (widget, 12);
        gtk_widget_set_margin_top (widget, 12);
        gtk_widget_set_margin_bottom (widget, 12);
        gtk_widget_show (widget);
        gtk_container_add (GTK_CONTAINER (widget), image);
        gtk_widget_set_halign (widget, GTK_ALIGN_CENTER);
        gtk_widget_set_valign (widget, GTK_ALIGN_CENTER);
        atk_object_set_name (gtk_widget_get_accessible (widget), _("Options…"));
        gtk_box_pack_start (GTK_BOX (box), widget, FALSE, TRUE, 0);
        g_object_set_data (G_OBJECT (widget), "edit", widget);
        g_object_set_data (G_OBJECT (widget), "row", row);
        g_signal_connect (widget, "clicked",
                          G_CALLBACK (show_details_for_row), device);

        gtk_widget_show_all (row);

        g_object_set_data (G_OBJECT (row), "connection", connection);

        gtk_container_add (GTK_CONTAINER (device->list), row);
}

static void
connection_removed (NMClient           *client,
                    NMRemoteConnection *connection,
                    NetDeviceEthernet  *device)
{
        if (g_hash_table_remove (device->connections, connection))
                device_ethernet_refresh_ui (device);
}

static void
populate_ui (NetDeviceEthernet *device)
{
        GList *children, *c;
        GSList *connections, *l;
        NMConnection *connection;
        gint n_connections;

        children = gtk_container_get_children (GTK_CONTAINER (device->list));
        for (c = children; c; c = c->next) {
                gtk_container_remove (GTK_CONTAINER (device->list), c->data);
        }
        g_list_free (children);

        connections = net_device_get_valid_connections (NET_DEVICE (device));
        for (l = connections; l; l = l->next) {
                NMConnection *connection = l->data;
                if (!g_hash_table_contains (device->connections, connection)) {
                        g_hash_table_add (device->connections, connection);
                }
        }
        n_connections = g_slist_length (connections);

        if (n_connections > 1) {
                gtk_widget_hide (device->details);
                for (l = connections; l; l = l->next) {
                        NMConnection *connection = l->data;
                        add_row (device, connection);
                }
                gtk_widget_show (device->scrolled_window);
        } else if (n_connections == 1) {
                connection = connections->data;
                gtk_widget_hide (device->scrolled_window);
                gtk_widget_show_all (device->details);
                g_object_set_data (G_OBJECT (device->details_button), "row", device->details_button);
                g_object_set_data (G_OBJECT (device->details_button), "connection", connection);

        } else {
                gtk_widget_hide (device->scrolled_window);
                gtk_widget_hide (device->details);
        }

        g_slist_free (connections);
}

static void
client_connection_added_cb (NMClient           *client,
                            NMRemoteConnection *connection,
                            NetDeviceEthernet  *device)
{
        device_ethernet_refresh_ui (device);
}

static void
add_profile (GtkButton *button, NetDeviceEthernet *device)
{
        NMConnection *connection;
        NMSettingConnection *sc;
        gchar *uuid, *id;
        NetConnectionEditor *editor;
        GtkWidget *window;
        NMClient *client;
        NMDevice *nmdev;
        const GPtrArray *connections;

        connection = nm_simple_connection_new ();
        sc = NM_SETTING_CONNECTION (nm_setting_connection_new ());
        nm_connection_add_setting (connection, NM_SETTING (sc));

        uuid = nm_utils_uuid_generate ();

        client = net_object_get_client (NET_OBJECT (device));
        connections = nm_client_get_connections (client);
        id = ce_page_get_next_available_name (connections, NAME_FORMAT_PROFILE, NULL);

        g_object_set (sc,
                      NM_SETTING_CONNECTION_UUID, uuid,
                      NM_SETTING_CONNECTION_ID, id,
                      NM_SETTING_CONNECTION_TYPE, NM_SETTING_WIRED_SETTING_NAME,
                      NM_SETTING_CONNECTION_AUTOCONNECT, TRUE,
                      NULL);

        nm_connection_add_setting (connection, nm_setting_wired_new ());

        g_free (uuid);
        g_free (id);

        window = gtk_widget_get_toplevel (GTK_WIDGET (button));

        nmdev = net_device_get_nm_device (NET_DEVICE (device));
        editor = net_connection_editor_new (GTK_WINDOW (window), connection, nmdev, NULL, client);
        g_signal_connect (editor, "done", G_CALLBACK (editor_done), device);
        net_connection_editor_run (editor);
}

static void
device_off_toggled (GtkSwitch         *sw,
                    GParamSpec        *pspec,
                    NetDeviceEthernet *device)
{
        NMClient *client;
        NMDevice *nm_device;
        NMConnection *connection;

        if (device->updating_device)
                return;

        client = net_object_get_client (NET_OBJECT (device));
        nm_device = net_device_get_nm_device (NET_DEVICE (device));

        if (gtk_switch_get_active (sw)) {
                connection = net_device_get_find_connection (NET_DEVICE (device));
                if (connection != NULL) {
                        nm_client_activate_connection_async (client,
                                                             connection,
                                                             nm_device,
                                                             NULL, NULL, NULL, NULL);
                }
        } else {
                nm_device_disconnect (nm_device, NULL, NULL);
        }
}

static void
device_title_changed (NetDeviceEthernet *device,
                      GParamSpec        *pspec,
                      gpointer           user_data)
{
        device_ethernet_refresh_ui (device);
}

static void
connection_activated (GtkListBox *list, GtkListBoxRow *row, NetDeviceEthernet *device)
{
        NMClient *client;
        NMDevice *nm_device;
        NMConnection *connection;

        client = net_object_get_client (NET_OBJECT (device));
        nm_device = net_device_get_nm_device (NET_DEVICE (device));

        if (!NM_IS_DEVICE_ETHERNET (nm_device) ||
            !nm_device_ethernet_get_carrier (NM_DEVICE_ETHERNET (nm_device)))
                return;

        connection = NM_CONNECTION (g_object_get_data (G_OBJECT (gtk_bin_get_child (GTK_BIN (row))), "connection"));

        nm_client_activate_connection_async (client,
                                             connection,
                                             nm_device,
                                             NULL, NULL, NULL, NULL);
}

static void
device_ethernet_constructed (GObject *object)
{
        NetDeviceEthernet *device = NET_DEVICE_ETHERNET (object);
        NMClient *client;
        GtkWidget *list;
        GtkWidget *swin;
        GtkWidget *widget;

        widget = GTK_WIDGET (gtk_builder_get_object (device->builder,
                                                     "device_off_switch"));
        g_signal_connect (widget, "notify::active",
                          G_CALLBACK (device_off_toggled), device);

        device->scrolled_window = swin = GTK_WIDGET (gtk_builder_get_object (device->builder, "list"));
        device->list = list = GTK_WIDGET (gtk_list_box_new ());
        gtk_list_box_set_selection_mode (GTK_LIST_BOX (list), GTK_SELECTION_NONE);
        gtk_list_box_set_header_func (GTK_LIST_BOX (list), cc_list_box_update_header_func, NULL, NULL);
        gtk_container_add (GTK_CONTAINER (swin), list);
        g_signal_connect (list, "row-activated",
                          G_CALLBACK (connection_activated), device);
        gtk_widget_show (list);

        device->details = GTK_WIDGET (gtk_builder_get_object (device->builder, "details"));

        device->details_button = GTK_WIDGET (gtk_builder_get_object (device->builder, "details_button"));
        g_signal_connect (device->details_button, "clicked",
                          G_CALLBACK (show_details_for_wired), device);

        device->add_profile_button = GTK_WIDGET (gtk_builder_get_object (device->builder, "add_profile_button"));
        g_signal_connect (device->add_profile_button, "clicked",
                          G_CALLBACK (add_profile), device);

        client = net_object_get_client (NET_OBJECT (object));
        g_signal_connect (client, NM_CLIENT_CONNECTION_ADDED,
                          G_CALLBACK (client_connection_added_cb), object);
        g_signal_connect_object (client, NM_CLIENT_CONNECTION_REMOVED,
                                 G_CALLBACK (connection_removed), device, 0);

        device_ethernet_refresh_ui (device);
}

static void
device_ethernet_finalize (GObject *object)
{
        NetDeviceEthernet *device = NET_DEVICE_ETHERNET (object);

        g_object_unref (device->builder);
        g_hash_table_destroy (device->connections);

        G_OBJECT_CLASS (net_device_ethernet_parent_class)->finalize (object);
}

static void
device_ethernet_refresh (NetObject *object)
{
        NetDeviceEthernet *device = NET_DEVICE_ETHERNET (object);
        device_ethernet_refresh_ui (device);
}

static void
net_device_ethernet_class_init (NetDeviceEthernetClass *klass)
{
        NetDeviceSimpleClass *simple_class = NET_DEVICE_SIMPLE_CLASS (klass);
        NetObjectClass *obj_class = NET_OBJECT_CLASS (klass);
        GObjectClass *object_class = G_OBJECT_CLASS (klass);

        simple_class->get_speed = device_ethernet_get_speed;
        obj_class->refresh = device_ethernet_refresh;
        obj_class->add_to_stack = device_ethernet_add_to_stack;
        object_class->constructed = device_ethernet_constructed;
        object_class->finalize = device_ethernet_finalize;
}

static void
net_device_ethernet_init (NetDeviceEthernet *device)
{
        GError *error = NULL;

        device->builder = gtk_builder_new ();
        gtk_builder_add_from_resource (device->builder,
                                       "/org/gnome/control-center/network/network-ethernet.ui",
                                       &error);
        if (error != NULL) {
                g_warning ("Could not load interface file: %s", error->message);
                g_error_free (error);
                return;
        }

        device->connections = g_hash_table_new (NULL, NULL);

        g_signal_connect (device, "notify::title", G_CALLBACK (device_title_changed), NULL);
}
