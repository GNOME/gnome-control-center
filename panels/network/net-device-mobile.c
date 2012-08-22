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
#include <nm-device-modem.h>
#include <nm-remote-connection.h>

#include "panel-common.h"
#include "network-dialogs.h"

#include "net-device-mobile.h"

#define NET_DEVICE_MOBILE_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), NET_TYPE_DEVICE_MOBILE, NetDeviceMobilePrivate))

static void nm_device_mobile_refresh_ui (NetDeviceMobile *device_mobile);

struct _NetDeviceMobilePrivate
{
        GtkBuilder              *builder;
        gboolean                 updating_device;
};

enum {
        COLUMN_ID,
        COLUMN_TITLE,
        COLUMN_LAST
};

G_DEFINE_TYPE (NetDeviceMobile, net_device_mobile, NET_TYPE_DEVICE)

static GtkWidget *
device_mobile_proxy_add_to_notebook (NetObject *object,
                                     GtkNotebook *notebook,
                                     GtkSizeGroup *heading_size_group)
{
        GtkWidget *widget;
        GtkWindow *window;
        NetDeviceMobile *device_mobile = NET_DEVICE_MOBILE (object);

        /* add widgets to size group */
        widget = GTK_WIDGET (gtk_builder_get_object (device_mobile->priv->builder,
                                                     "heading_imei"));
        gtk_size_group_add_widget (heading_size_group, widget);
        widget = GTK_WIDGET (gtk_builder_get_object (device_mobile->priv->builder,
                                                     "heading_network"));
        gtk_size_group_add_widget (heading_size_group, widget);

        /* reparent */
        window = GTK_WINDOW (gtk_builder_get_object (device_mobile->priv->builder,
                                                     "window_tmp"));
        widget = GTK_WIDGET (gtk_builder_get_object (device_mobile->priv->builder,
                                                     "vbox7"));
        g_object_ref (widget);
        gtk_container_remove (GTK_CONTAINER (window), widget);
        gtk_notebook_append_page (notebook, widget, NULL);
        g_object_unref (widget);
        return widget;
}

static void
connection_activate_cb (NMClient *client,
                        NMActiveConnection *connection,
                        GError *error,
                        gpointer user_data)
{
        NetDeviceMobile *device_mobile = NET_DEVICE_MOBILE (user_data);

        if (connection == NULL) {
                /* failed to activate */
                nm_device_mobile_refresh_ui (device_mobile);
        }
}

static void
mobile_connection_changed_cb (GtkComboBox *combo_box, NetDeviceMobile *device_mobile)
{
        gboolean ret;
        gchar *object_path = NULL;
        GtkTreeIter iter;
        GtkTreeModel *model;
        NMConnection *connection;
        NMDevice *device;
        NMClient *client;
        NMRemoteSettings *remote_settings;
        CcNetworkPanel *panel;

        if (device_mobile->priv->updating_device)
                goto out;

        ret = gtk_combo_box_get_active_iter (combo_box, &iter);
        if (!ret)
                goto out;

        device = net_device_get_nm_device (NET_DEVICE (device_mobile));
        if (device == NULL)
                goto out;
        client = net_object_get_client (NET_OBJECT (device_mobile));
        remote_settings = net_object_get_remote_settings (NET_OBJECT (device_mobile));

        /* get entry */
        model = gtk_combo_box_get_model (GTK_COMBO_BOX (combo_box));
        gtk_tree_model_get (model, &iter,
                            COLUMN_ID, &object_path,
                            -1);
        if (g_strcmp0 (object_path, NULL) == 0) {
                panel = net_object_get_panel (NET_OBJECT (device_mobile));
                cc_network_panel_connect_to_3g_network (panel,
                                                        client,
                                                        remote_settings,
                                                        device);
                goto out;
        }

        /* activate the connection */
        g_debug ("try to switch to connection %s", object_path);
        connection = (NMConnection*) nm_remote_settings_get_connection_by_path (remote_settings,
                                                                                object_path);
        if (connection != NULL) {
                nm_device_disconnect (device, NULL, NULL);
                nm_client_activate_connection (client,
                                               connection,
                                               device, NULL,
                                               connection_activate_cb,
                                               device_mobile);
                goto out;
        }
out:
        g_free (object_path);
}

static void
mobilebb_enabled_toggled (NMClient       *client,
                          GParamSpec     *pspec,
                          NetDeviceMobile *device_mobile)
{
        gboolean enabled;
        GtkSwitch *sw;
        NMDevice *device;

        device = net_device_get_nm_device (NET_DEVICE (device_mobile));
        if (nm_device_get_device_type (device) != NM_DEVICE_TYPE_MODEM)
                return;

        enabled = nm_client_wwan_get_enabled (client);
        sw = GTK_SWITCH (gtk_builder_get_object (device_mobile->priv->builder,
                                                 "device_off_switch"));

        device_mobile->priv->updating_device = TRUE;
        gtk_switch_set_active (sw, enabled);
        device_mobile->priv->updating_device = FALSE;
}

static void
device_add_device_connections (NetDeviceMobile *device_mobile,
                               NMDevice *nm_device,
                               GtkListStore *liststore,
                               GtkComboBox *combobox)
{
        NetDeviceMobilePrivate *priv = device_mobile->priv;
        GSList *filtered;
        GSList *list, *l;
        GtkTreeIter treeiter;
        NMActiveConnection *active_connection;
        NMConnection *connection;
        NMRemoteSettings *remote_settings;

        /* get the list of available connections for this device */
        remote_settings = net_object_get_remote_settings (NET_OBJECT (device_mobile));
        g_assert (remote_settings != NULL);
        list = nm_remote_settings_list_connections (remote_settings);
        filtered = nm_device_filter_connections (nm_device, list);
        gtk_list_store_clear (liststore);
        active_connection = nm_device_get_active_connection (nm_device);
        for (l = filtered; l; l = g_slist_next (l)) {
                connection = NM_CONNECTION (l->data);
                gtk_list_store_append (liststore, &treeiter);
                gtk_list_store_set (liststore,
                                    &treeiter,
                                    COLUMN_ID, nm_connection_get_uuid (connection),
                                    COLUMN_TITLE, nm_connection_get_id (connection),
                                    -1);

                /* is this already activated? */
                if (active_connection != NULL &&
                    g_strcmp0 (nm_connection_get_path (connection),
                               nm_active_connection_get_connection (active_connection)) == 0) {
                        priv->updating_device = TRUE;
                        gtk_combo_box_set_active_iter (combobox, &treeiter);
                        priv->updating_device = FALSE;
                }
        }

        /* add new connection entry */
        gtk_list_store_append (liststore, &treeiter);
        gtk_list_store_set (liststore,
                            &treeiter,
                            COLUMN_ID, NULL,
                            COLUMN_TITLE, _("Add new connection"),
                            -1);

        g_slist_free (list);
        g_slist_free (filtered);
}

static void
nm_device_mobile_refresh_ui (NetDeviceMobile *device_mobile)
{
        const char *str;
        gboolean is_connected;
        GString *status;
        GtkListStore *liststore;
        GtkWidget *widget;
        guint speed = 0;
        NetDeviceMobilePrivate *priv = device_mobile->priv;
        NMClient *client;
        NMDeviceModemCapabilities caps;
        NMDevice *nm_device;

        /* set device kind */
        nm_device = net_device_get_nm_device (NET_DEVICE (device_mobile));
        widget = GTK_WIDGET (gtk_builder_get_object (device_mobile->priv->builder, "label_device"));
        gtk_label_set_label (GTK_LABEL (widget),
                             panel_device_to_localized_string (nm_device));

        /* set up the device on/off switch */
        widget = GTK_WIDGET (gtk_builder_get_object (device_mobile->priv->builder, "device_off_switch"));
        gtk_widget_show (widget);
        client = net_object_get_client (NET_OBJECT (device_mobile));
        mobilebb_enabled_toggled (client, NULL, device_mobile);

        /* set device state, with status and optionally speed */
        widget = GTK_WIDGET (gtk_builder_get_object (device_mobile->priv->builder, "label_status"));
        status = g_string_new (panel_device_state_to_localized_string (nm_device));
        if (speed  > 0) {
                g_string_append (status, " - ");
                /* Translators: network device speed */
                g_string_append_printf (status, _("%d Mb/s"), speed);
        }
        gtk_label_set_label (GTK_LABEL (widget), status->str);
        g_string_free (status, TRUE);
        gtk_widget_set_tooltip_text (widget, panel_device_state_reason_to_localized_string (nm_device));

        /* sensitive for other connection types if the device is currently connected */
        widget = GTK_WIDGET (gtk_builder_get_object (device_mobile->priv->builder,
                                                     "button_options"));
        is_connected = net_device_get_find_connection (NET_DEVICE (device_mobile)) != NULL;
        gtk_widget_set_sensitive (widget, is_connected);

        caps = nm_device_modem_get_current_capabilities (NM_DEVICE_MODEM (nm_device));
        if ((caps & NM_DEVICE_MODEM_CAPABILITY_GSM_UMTS) ||
            (caps & NM_DEVICE_MODEM_CAPABILITY_CDMA_EVDO)) {
                /* IMEI */
                str = g_object_get_data (G_OBJECT (nm_device),
                                         "ControlCenter::EquipmentIdentifier");
                panel_set_device_widget_details (device_mobile->priv->builder,
                                                 "imei",
                                                 str);

                /* operator name */
                str = g_object_get_data (G_OBJECT (nm_device),
                                         "ControlCenter::OperatorName");
                panel_set_device_widget_details (device_mobile->priv->builder,
                                                 "provider",
                                                 str);
        }

        /* add possible connections to device */
        liststore = GTK_LIST_STORE (gtk_builder_get_object (priv->builder,
                                                            "liststore_mobile_connections"));
        widget = GTK_WIDGET (gtk_builder_get_object (priv->builder, "combobox_network"));
        device_add_device_connections (device_mobile,
                                       nm_device,
                                       liststore,
                                       GTK_COMBO_BOX (widget));

        /* set IP entries */
        panel_set_device_widgets (priv->builder, nm_device);
}

static void
device_mobile_refresh (NetObject *object)
{
        NetDeviceMobile *device_mobile = NET_DEVICE_MOBILE (object);
        nm_device_mobile_refresh_ui (device_mobile);
}

static void
device_off_toggled (GtkSwitch *sw,
                    GParamSpec *pspec,
                    NetDeviceMobile *device_mobile)
{
        const gchar *path;
        const GPtrArray *acs;
        gboolean active;
        gint i;
        NMActiveConnection *a;
        NMConnection *connection;
        NMClient *client;

        if (device_mobile->priv->updating_device)
                return;

        active = gtk_switch_get_active (sw);
        if (active) {
                client = net_object_get_client (NET_OBJECT (device_mobile));
                connection = net_device_get_find_connection (NET_DEVICE (device_mobile));
                if (connection == NULL)
                        return;
                nm_client_activate_connection (client,
                                               connection,
                                               net_device_get_nm_device (NET_DEVICE (device_mobile)),
                                               NULL, NULL, NULL);
        } else {
                connection = net_device_get_find_connection (NET_DEVICE (device_mobile));
                if (connection == NULL)
                        return;
                path = nm_connection_get_path (connection);
                client = net_object_get_client (NET_OBJECT (device_mobile));
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
edit_connection (GtkButton *button, NetDeviceMobile *device_mobile)
{
        net_object_edit (NET_OBJECT (device_mobile));
}

static void
device_mobile_device_got_modem_manager_cb (GObject *source_object,
                                   GAsyncResult *res,
                                   gpointer user_data)
{
        GError *error = NULL;
        GVariant *result = NULL;
        GDBusProxy *proxy;
        NMDevice *device = (NMDevice *) user_data;

        proxy = g_dbus_proxy_new_for_bus_finish (res, &error);
        if (proxy == NULL) {
                g_warning ("Error creating ModemManager proxy: %s",
                           error->message);
                g_error_free (error);
                goto out;
        }

        /* get the IMEI */
        result = g_dbus_proxy_get_cached_property (proxy,
                                                   "EquipmentIdentifier");

        /* save */
        g_object_set_data_full (G_OBJECT (device),
                                "ControlCenter::EquipmentIdentifier",
                                g_variant_dup_string (result, NULL),
                                g_free);
out:
        if (result != NULL)
                g_variant_unref (result);
        if (proxy != NULL)
                g_object_unref (proxy);
}

static void
device_mobile_get_registration_info_cb (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
        gchar *operator_code = NULL;
        GError *error = NULL;
        guint registration_status;
        GVariant *result = NULL;
        gchar *operator_name = NULL;
        gchar *operator_name_safe = NULL;
        NMDevice *device = (NMDevice *) user_data;

        result = g_dbus_proxy_call_finish (G_DBUS_PROXY (source_object), res, &error);
        if (result == NULL) {
                g_warning ("Error getting registration info: %s\n",
                           error->message);
                g_error_free (error);
                return;
        }

        /* get values */
        g_variant_get (result, "((uss))",
                       &registration_status,
                       &operator_code,
                       &operator_name);
        if (operator_name != NULL && operator_name[0] != '\0')
                operator_name_safe = g_strescape (operator_name, NULL);

        /* save */
        g_object_set_data_full (G_OBJECT (device),
                                "ControlCenter::OperatorName",
                                operator_name_safe,
                                g_free);

        g_free (operator_name);
        g_free (operator_code);
        g_variant_unref (result);
}

static void
device_mobile_device_got_modem_manager_gsm_cb (GObject *source_object,
                                       GAsyncResult *res,
                                       gpointer user_data)
{
        GError *error = NULL;
        GDBusProxy *proxy;
        NMDevice *device = (NMDevice *) user_data;

        proxy = g_dbus_proxy_new_for_bus_finish (res, &error);
        if (proxy == NULL) {
                g_warning ("Error creating ModemManager GSM proxy: %s\n",
                           error->message);
                g_error_free (error);
                goto out;
        }

        g_dbus_proxy_call (proxy,
                           "GetRegistrationInfo",
                           NULL,
                           G_DBUS_CALL_FLAGS_NONE,
                           -1,
                           NULL,
                           device_mobile_get_registration_info_cb,
                           device);
out:
        if (proxy != NULL)
                g_object_unref (proxy);
}

static void
net_device_mobile_constructed (GObject *object)
{
        GCancellable *cancellable;
        NetDeviceMobile *device_mobile = NET_DEVICE_MOBILE (object);
        NMClient *client;
        NMDevice *device;

        G_OBJECT_CLASS (net_device_mobile_parent_class)->constructed (object);

        device = net_device_get_nm_device (NET_DEVICE (device_mobile));
        cancellable = net_object_get_cancellable (NET_OBJECT (device_mobile));
        g_dbus_proxy_new_for_bus (G_BUS_TYPE_SYSTEM,
                                  G_DBUS_PROXY_FLAGS_NONE,
                                  NULL,
                                  "org.freedesktop.ModemManager",
                                  nm_device_get_udi (device),
                                  "org.freedesktop.ModemManager.Modem",
                                  cancellable,
                                  device_mobile_device_got_modem_manager_cb,
                                  device);
        g_dbus_proxy_new_for_bus (G_BUS_TYPE_SYSTEM,
                                  G_DBUS_PROXY_FLAGS_NONE,
                                  NULL,
                                  "org.freedesktop.ModemManager",
                                  nm_device_get_udi (device),
                                  "org.freedesktop.ModemManager.Modem.Gsm.Network",
                                  cancellable,
                                  device_mobile_device_got_modem_manager_gsm_cb,
                                  device);

        client = net_object_get_client (NET_OBJECT (device_mobile));
        g_signal_connect (client, "notify::wwan-enabled",
                          G_CALLBACK (mobilebb_enabled_toggled),
                          device_mobile);
        nm_device_mobile_refresh_ui (device_mobile);
}

static void
net_device_mobile_finalize (GObject *object)
{
        NetDeviceMobile *device_mobile = NET_DEVICE_MOBILE (object);
        NetDeviceMobilePrivate *priv = device_mobile->priv;

        g_object_unref (priv->builder);

        G_OBJECT_CLASS (net_device_mobile_parent_class)->finalize (object);
}

static void
net_device_mobile_class_init (NetDeviceMobileClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);
        NetObjectClass *parent_class = NET_OBJECT_CLASS (klass);

        object_class->finalize = net_device_mobile_finalize;
        object_class->constructed = net_device_mobile_constructed;
        parent_class->add_to_notebook = device_mobile_proxy_add_to_notebook;
        parent_class->refresh = device_mobile_refresh;
        g_type_class_add_private (klass, sizeof (NetDeviceMobilePrivate));
}

static void
net_device_mobile_init (NetDeviceMobile *device_mobile)
{
        GError *error = NULL;
        GtkWidget *widget;
        GtkCellRenderer *renderer;
        GtkComboBox *combobox;

        device_mobile->priv = NET_DEVICE_MOBILE_GET_PRIVATE (device_mobile);

        device_mobile->priv->builder = gtk_builder_new ();
        gtk_builder_add_from_file (device_mobile->priv->builder,
                                   GNOMECC_UI_DIR "/network-mobile.ui",
                                   &error);
        if (error != NULL) {
                g_warning ("Could not load interface file: %s", error->message);
                g_error_free (error);
                return;
        }

        /* setup mobile combobox model */
        combobox = GTK_COMBO_BOX (gtk_builder_get_object (device_mobile->priv->builder,
                                                          "combobox_network"));
        g_signal_connect (combobox, "changed",
                          G_CALLBACK (mobile_connection_changed_cb),
                          device_mobile);
        renderer = gtk_cell_renderer_text_new ();
        gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (combobox),
                                    renderer,
                                    FALSE);
        gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (combobox), renderer,
                                        "text", COLUMN_TITLE,
                                        NULL);

        widget = GTK_WIDGET (gtk_builder_get_object (device_mobile->priv->builder,
                                                     "device_off_switch"));
        g_signal_connect (widget, "notify::active",
                          G_CALLBACK (device_off_toggled), device_mobile);

        widget = GTK_WIDGET (gtk_builder_get_object (device_mobile->priv->builder,
                                                     "button_options"));
        g_signal_connect (widget, "clicked",
                          G_CALLBACK (edit_connection), device_mobile);

        widget = GTK_WIDGET (gtk_builder_get_object (device_mobile->priv->builder,
                                                     "device_off_switch"));
        g_signal_connect (widget, "notify::active",
                          G_CALLBACK (device_off_toggled), device_mobile);
}
