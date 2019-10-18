/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2011-2012 Richard Hughes <richard@hughsie.com>
 * Copyright (C) 2013 Aleksander Morgado <aleksander@gnu.org>
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
#include <libmm-glib.h>
#include <nma-mobile-providers.h>

#include "panel-common.h"
#include "network-dialogs.h"
#include "net-device-mobile.h"

static void nm_device_mobile_refresh_ui (NetDeviceMobile *device_mobile);

struct _NetDeviceMobile
{
        NetDevice   parent;
        GtkBuilder *builder;
        gboolean    updating_device;

        /* Old MM < 0.7 support */
        GDBusProxy *gsm_proxy;
        GDBusProxy *cdma_proxy;

        /* New MM >= 0.7 support */
        MMObject   *mm_object;
        guint       operator_name_updated;

        NMAMobileProvidersDatabase *mpd;
};

enum {
        COLUMN_ID,
        COLUMN_TITLE,
        COLUMN_LAST
};

enum {
        PROP_0,
        PROP_MODEM_OBJECT,
        PROP_LAST
};

G_DEFINE_TYPE (NetDeviceMobile, net_device_mobile, NET_TYPE_DEVICE)

static GtkWidget *
device_mobile_proxy_add_to_stack (NetObject    *object,
                                  GtkStack     *stack,
                                  GtkSizeGroup *heading_size_group)
{
        GtkWidget *widget;
        NetDeviceMobile *device_mobile = NET_DEVICE_MOBILE (object);

        /* add widgets to size group */
        widget = GTK_WIDGET (gtk_builder_get_object (device_mobile->builder,
                                                     "imei_heading_label"));
        gtk_size_group_add_widget (heading_size_group, widget);
        widget = GTK_WIDGET (gtk_builder_get_object (device_mobile->builder,
                                                     "network_label"));
        gtk_size_group_add_widget (heading_size_group, widget);

        widget = GTK_WIDGET (gtk_builder_get_object (device_mobile->builder,
                                                     "box"));
        gtk_stack_add_named (stack, widget, net_object_get_id (object));
        return widget;
}

static void
connection_activate_cb (GObject *source_object,
                        GAsyncResult *res,
                        gpointer user_data)
{
        g_autoptr(GError) error = NULL;

        if (!nm_client_activate_connection_finish (NM_CLIENT (source_object), res, &error)) {
                /* failed to activate */
                nm_device_mobile_refresh_ui (user_data);
        }
}

static void
mobile_connection_changed_cb (NetDeviceMobile *device_mobile)
{
        gboolean ret;
        g_autofree gchar *object_path = NULL;
        GtkTreeIter iter;
        GtkTreeModel *model;
        NMConnection *connection;
        NMDevice *device;
        NMClient *client;
        CcNetworkPanel *panel;
        GtkWidget *toplevel;

        if (device_mobile->updating_device)
                return;

        ret = gtk_combo_box_get_active_iter (GTK_COMBO_BOX (gtk_builder_get_object (device_mobile->builder, "combobox_network")), &iter);
        if (!ret)
                return;

        device = net_device_get_nm_device (NET_DEVICE (device_mobile));
        if (device == NULL)
                return;
        client = net_object_get_client (NET_OBJECT (device_mobile));

        /* get entry */
        model = gtk_combo_box_get_model (GTK_COMBO_BOX (gtk_builder_get_object (device_mobile->builder, "combobox_network")));
        gtk_tree_model_get (model, &iter,
                            COLUMN_ID, &object_path,
                            -1);
        if (g_strcmp0 (object_path, NULL) == 0) {
                panel = net_object_get_panel (NET_OBJECT (device_mobile));
                toplevel = cc_shell_get_toplevel (cc_panel_get_shell (CC_PANEL (panel)));
                cc_network_panel_connect_to_3g_network (toplevel,
                                                        client,
                                                        device);
                return;
        }

        /* activate the connection */
        g_debug ("try to switch to connection %s", object_path);
        connection = (NMConnection*) nm_client_get_connection_by_path (client, object_path);
        if (connection != NULL) {
                nm_device_disconnect (device, NULL, NULL);
                nm_client_activate_connection_async (client,
                                                     connection,
                                                     device, NULL, NULL,
                                                     connection_activate_cb,
                                                     device_mobile);
                return;
        }
}

static void
mobilebb_enabled_toggled (NetDeviceMobile *device_mobile)
{
        gboolean enabled = FALSE;
        GtkSwitch *sw;
        NMDevice *device;

        device = net_device_get_nm_device (NET_DEVICE (device_mobile));
        if (nm_device_get_device_type (device) != NM_DEVICE_TYPE_MODEM)
                return;

        if (nm_client_wwan_get_enabled (net_object_get_client (NET_OBJECT (device_mobile)))) {
                NMDeviceState state;

                state = nm_device_get_state (device);
                if (state == NM_DEVICE_STATE_UNKNOWN ||
                    state == NM_DEVICE_STATE_UNMANAGED ||
                    state == NM_DEVICE_STATE_UNAVAILABLE ||
                    state == NM_DEVICE_STATE_DISCONNECTED ||
                    state == NM_DEVICE_STATE_DEACTIVATING ||
                    state == NM_DEVICE_STATE_FAILED) {
                        enabled = FALSE;
                } else {
                        enabled = TRUE;
                }
        }

        sw = GTK_SWITCH (gtk_builder_get_object (device_mobile->builder,
                                                 "device_off_switch"));

        device_mobile->updating_device = TRUE;
        gtk_switch_set_active (sw, enabled);
        device_mobile->updating_device = FALSE;
}

static void
device_add_device_connections (NetDeviceMobile *device_mobile,
                               NMDevice *nm_device,
                               GtkListStore *liststore,
                               GtkComboBox *combobox)
{
        GSList *list, *l;
        GtkTreeIter treeiter;
        NMActiveConnection *active_connection;
        NMConnection *connection;

        /* get the list of available connections for this device */
        list = net_device_get_valid_connections (NET_DEVICE (device_mobile));
        gtk_list_store_clear (liststore);
        active_connection = nm_device_get_active_connection (nm_device);
        for (l = list; l; l = g_slist_next (l)) {
                connection = NM_CONNECTION (l->data);
                gtk_list_store_append (liststore, &treeiter);
                gtk_list_store_set (liststore,
                                    &treeiter,
                                    COLUMN_ID, nm_connection_get_uuid (connection),
                                    COLUMN_TITLE, nm_connection_get_id (connection),
                                    -1);

                /* is this already activated? */
                if (active_connection != NULL &&
                    g_strcmp0 (nm_connection_get_uuid (connection),
                               nm_active_connection_get_uuid (active_connection)) == 0) {
                        device_mobile->updating_device = TRUE;
                        gtk_combo_box_set_active_iter (combobox, &treeiter);
                        device_mobile->updating_device = FALSE;
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
}

static void
device_mobile_refresh_equipment_id (NetDeviceMobile *device_mobile)
{
        const gchar *equipment_id = NULL;
        GtkWidget *heading, *widget;

        if (device_mobile->mm_object != NULL) {
                MMModem *modem;

                /* Modem interface should always be present */
                modem = mm_object_peek_modem (device_mobile->mm_object);
                equipment_id = mm_modem_get_equipment_identifier (modem);

                /* Set equipment ID */
                if (equipment_id != NULL) {
                        g_debug ("[%s] Equipment ID set to '%s'",
                                 mm_object_get_path (device_mobile->mm_object),
                                 equipment_id);
                }
        } else {
                /* Assume old MM handling */
                equipment_id = g_object_get_data (G_OBJECT (device_mobile),
                                                  "ControlCenter::EquipmentIdentifier");
        }

        heading = GTK_WIDGET (gtk_builder_get_object (device_mobile->builder, "imei_heading_label"));
        widget = GTK_WIDGET (gtk_builder_get_object (device_mobile->builder, "imei_label"));
        panel_set_device_widget_details (GTK_LABEL (heading), GTK_LABEL (widget), equipment_id);
}

static gchar *
device_mobile_find_provider (NetDeviceMobile *device_mobile,
                             const gchar     *mccmnc,
                             guint32          sid)
{
        NMAMobileProvider *provider;
        GString *name = NULL;

        if (device_mobile->mpd == NULL) {
                g_autoptr(GError) error = NULL;

                /* Use defaults */
                device_mobile->mpd = nma_mobile_providers_database_new_sync (NULL, NULL, NULL, &error);
                if (device_mobile->mpd == NULL) {
                        g_debug ("Couldn't load mobile providers database: %s",
                                 error ? error->message : "");
                        return NULL;
                }
        }

        if (mccmnc != NULL) {
                provider = nma_mobile_providers_database_lookup_3gpp_mcc_mnc (device_mobile->mpd, mccmnc);
                if (provider != NULL)
                        name = g_string_new (nma_mobile_provider_get_name (provider));
        }

        if (sid != 0) {
                provider = nma_mobile_providers_database_lookup_cdma_sid (device_mobile->mpd, sid);
                if (provider != NULL) {
                        if (name == NULL)
                                name = g_string_new (nma_mobile_provider_get_name (provider));
                        else
                                g_string_append_printf (name, ", %s", nma_mobile_provider_get_name (provider));
                }
        }

        return (name != NULL ? g_string_free (name, FALSE) : NULL);
}

static void
device_mobile_refresh_operator_name (NetDeviceMobile *device_mobile)
{
        GtkWidget *heading, *widget;

        heading = GTK_WIDGET (gtk_builder_get_object (device_mobile->builder, "provider_heading_label"));
        widget = GTK_WIDGET (gtk_builder_get_object (device_mobile->builder, "provider_label"));

        if (device_mobile->mm_object != NULL) {
                g_autofree gchar *operator_name = NULL;
                MMModem3gpp *modem_3gpp;
                MMModemCdma *modem_cdma;

                modem_3gpp = mm_object_peek_modem_3gpp (device_mobile->mm_object);
                modem_cdma = mm_object_peek_modem_cdma (device_mobile->mm_object);

                if (modem_3gpp != NULL) {
                        const gchar *operator_name_unsafe;

                        operator_name_unsafe = mm_modem_3gpp_get_operator_name (modem_3gpp);
                        if (operator_name_unsafe != NULL && operator_name_unsafe[0] != '\0')
                                operator_name = g_strescape (operator_name_unsafe, NULL);
                }

                /* If not directly given in the 3GPP interface, try to guess from
                 * MCCMNC/SID */
                if (operator_name == NULL) {
                        const gchar *mccmnc = NULL;
                        guint32 sid = 0;

                        if (modem_3gpp != NULL)
                                mccmnc = mm_modem_3gpp_get_operator_code (modem_3gpp);
                        if (modem_cdma != NULL)
                                sid = mm_modem_cdma_get_sid (modem_cdma);
                        operator_name = device_mobile_find_provider (device_mobile, mccmnc, sid);
                }

                /* Set operator name */
                if (operator_name != NULL) {
                        g_debug ("[%s] Operator name set to '%s'",
                                 mm_object_get_path (device_mobile->mm_object),
                                 operator_name);
                }

                panel_set_device_widget_details (GTK_LABEL (heading), GTK_LABEL (widget), operator_name);
        } else {
                const gchar *gsm;
                const gchar *cdma;

                /* Assume old MM handling */
                gsm = g_object_get_data (G_OBJECT (device_mobile),
                                         "ControlCenter::OperatorNameGsm");
                cdma = g_object_get_data (G_OBJECT (device_mobile),
                                          "ControlCenter::OperatorNameCdma");

                if (gsm != NULL && cdma != NULL) {
                        g_autofree gchar *both = g_strdup_printf ("%s, %s", gsm, cdma);
                        panel_set_device_widget_details (GTK_LABEL (heading), GTK_LABEL (widget), both);
                } else if (gsm != NULL) {
                        panel_set_device_widget_details (GTK_LABEL (heading), GTK_LABEL (widget), gsm);
                } else if (cdma != NULL) {
                        panel_set_device_widget_details (GTK_LABEL (heading), GTK_LABEL (widget), cdma);
                } else {
                        panel_set_device_widget_details (GTK_LABEL (heading), GTK_LABEL (widget), NULL);
                }
        }
}

static void
nm_device_mobile_refresh_ui (NetDeviceMobile *device_mobile)
{
        gboolean is_connected;
        GtkListStore *liststore;
        GtkWidget *widget;
        NMDeviceModemCapabilities caps;
        NMDevice *nm_device;
        g_autofree gchar *status = NULL;

        nm_device = net_device_get_nm_device (NET_DEVICE (device_mobile));

        /* set device kind */
        widget = GTK_WIDGET (gtk_builder_get_object (device_mobile->builder, "device_label"));
        g_object_bind_property (device_mobile, "title", widget, "label", 0);

        /* set up the device on/off switch */
        widget = GTK_WIDGET (gtk_builder_get_object (device_mobile->builder, "device_off_switch"));
        gtk_widget_show (widget);
        mobilebb_enabled_toggled (device_mobile);

        /* set device state, with status */
        widget = GTK_WIDGET (gtk_builder_get_object (device_mobile->builder, "status_label"));
        status = panel_device_status_to_localized_string (nm_device, NULL);
        gtk_label_set_label (GTK_LABEL (widget), status);

        /* sensitive for other connection types if the device is currently connected */
        widget = GTK_WIDGET (gtk_builder_get_object (device_mobile->builder,
                                                     "options_button"));
        is_connected = net_device_get_find_connection (NET_DEVICE (device_mobile)) != NULL;
        gtk_widget_set_sensitive (widget, is_connected);

        caps = nm_device_modem_get_current_capabilities (NM_DEVICE_MODEM (nm_device));
        if ((caps & NM_DEVICE_MODEM_CAPABILITY_GSM_UMTS) ||
            (caps & NM_DEVICE_MODEM_CAPABILITY_CDMA_EVDO) ||
            (caps & NM_DEVICE_MODEM_CAPABILITY_LTE)) {
                device_mobile_refresh_operator_name (device_mobile);
                device_mobile_refresh_equipment_id (device_mobile);
        }

        /* add possible connections to device */
        liststore = GTK_LIST_STORE (gtk_builder_get_object (device_mobile->builder,
                                                            "mobile_connections_list_store"));
        widget = GTK_WIDGET (gtk_builder_get_object (device_mobile->builder, "network_combo"));
        device_add_device_connections (device_mobile,
                                       nm_device,
                                       liststore,
                                       GTK_COMBO_BOX (widget));

        /* set IP entries */
        panel_set_device_widgets (GTK_LABEL (gtk_builder_get_object (device_mobile->builder, "ipv4_heading_label")),
                                  GTK_LABEL (gtk_builder_get_object (device_mobile->builder, "ipv4_label")),
                                  GTK_LABEL (gtk_builder_get_object (device_mobile->builder, "ipv6_heading_label")),
                                  GTK_LABEL (gtk_builder_get_object (device_mobile->builder, "ipv6_label")),
                                  GTK_LABEL (gtk_builder_get_object (device_mobile->builder, "dns_heading_label")),
                                  GTK_LABEL (gtk_builder_get_object (device_mobile->builder, "dns_label")),
                                  GTK_LABEL (gtk_builder_get_object (device_mobile->builder, "route_heading_label")),
                                  GTK_LABEL (gtk_builder_get_object (device_mobile->builder, "route_label")),
                                  nm_device);
}

static void
device_mobile_refresh (NetObject *object)
{
        NetDeviceMobile *device_mobile = NET_DEVICE_MOBILE (object);
        nm_device_mobile_refresh_ui (device_mobile);
}

static void
device_off_toggled (NetDeviceMobile *device_mobile)
{
        const GPtrArray *acs;
        gboolean active;
        gint i;
        NMActiveConnection *a;
        NMConnection *connection;
        NMClient *client;

        if (device_mobile->updating_device)
                return;

        active = gtk_switch_get_active (GTK_SWITCH (gtk_builder_get_object (device_mobile->builder, "device_off_switch")));
        if (active) {
                client = net_object_get_client (NET_OBJECT (device_mobile));
                connection = net_device_get_find_connection (NET_DEVICE (device_mobile));
                if (connection == NULL)
                        return;
                nm_client_activate_connection_async (client,
                                                     connection,
                                                     net_device_get_nm_device (NET_DEVICE (device_mobile)),
                                                     NULL, NULL, NULL, NULL);
        } else {
                const gchar *uuid;

                connection = net_device_get_find_connection (NET_DEVICE (device_mobile));
                if (connection == NULL)
                        return;
                uuid = nm_connection_get_uuid (connection);
                client = net_object_get_client (NET_OBJECT (device_mobile));
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
edit_connection (NetDeviceMobile *device_mobile)
{
        net_object_edit (NET_OBJECT (device_mobile));
}

static void
device_mobile_device_got_modem_manager_cb (GObject *source_object,
                                           GAsyncResult *res,
                                           gpointer user_data)
{
        g_autoptr(GError) error = NULL;
        g_autoptr(GVariant) result = NULL;
        g_autoptr(GDBusProxy) proxy = NULL;
        NetDeviceMobile *device_mobile = (NetDeviceMobile *)user_data;

        proxy = g_dbus_proxy_new_for_bus_finish (res, &error);
        if (!proxy) {
                g_warning ("Error creating ModemManager proxy: %s",
                           error->message);
                return;
        }

        /* get the IMEI */
        result = g_dbus_proxy_get_cached_property (proxy,
                                                   "EquipmentIdentifier");

        /* save */
        if (result)
                g_object_set_data_full (G_OBJECT (device_mobile),
                                        "ControlCenter::EquipmentIdentifier",
                                        g_variant_dup_string (result, NULL),
                                        g_free);

        device_mobile_refresh_equipment_id (device_mobile);
}

static void
device_mobile_save_operator_name (NetDeviceMobile *device_mobile,
                                  const gchar     *field,
                                  const gchar     *operator_name)
{
        gchar *operator_name_safe = NULL;

        if (operator_name != NULL && operator_name[0] != '\0')
                operator_name_safe = g_strescape (operator_name, NULL);

        /* save */
        g_object_set_data_full (G_OBJECT (device_mobile),
                                field,
                                operator_name_safe,
                                g_free);
        /* refresh */
        device_mobile_refresh_operator_name (device_mobile);
}

static void
device_mobile_get_registration_info_cb (GObject      *source_object,
                                        GAsyncResult *res,
                                        gpointer      user_data)
{
        g_autofree gchar *operator_code = NULL;
        g_autoptr(GError) error = NULL;
        guint registration_status;
        g_autoptr(GVariant) result = NULL;
        g_autofree gchar *operator_name = NULL;
        NetDeviceMobile *device_mobile = (NetDeviceMobile *)user_data;

        result = g_dbus_proxy_call_finish (G_DBUS_PROXY (source_object), res, &error);
        if (result == NULL) {
                g_warning ("Error getting registration info: %s\n",
                           error->message);
                return;
        }

        /* get values */
        g_variant_get (result, "((uss))",
                       &registration_status,
                       &operator_code,
                       &operator_name);

        /* If none give, try to guess it */
        if (operator_name == NULL || operator_name[0] == '\0') {
                g_free (operator_name);
                operator_name = device_mobile_find_provider (device_mobile, operator_code, 0);
        }

        /* save and refresh */
        device_mobile_save_operator_name (device_mobile,
                                          "ControlCenter::OperatorNameGsm",
                                          operator_name);
}

static void
device_mobile_gsm_signal_cb (NetDeviceMobile *device_mobile,
                             const gchar     *sender_name,
                             const gchar     *signal_name,
                             GVariant        *parameters)
{
        guint registration_status = 0;
        g_autofree gchar *operator_code = NULL;
        g_autofree gchar *operator_name = NULL;

        if (!g_str_equal (signal_name, "RegistrationInfo"))
                return;

        g_variant_get (parameters,
                       "(uss)",
                       &registration_status,
                       &operator_code,
                       &operator_name);

        /* If none given, try to guess it */
        if (operator_name == NULL || operator_name[0] == '\0') {
                g_free (operator_name);
                operator_name = device_mobile_find_provider (device_mobile, operator_code, 0);
        }

        /* save and refresh */
        device_mobile_save_operator_name (device_mobile,
                                          "ControlCenter::OperatorNameGsm",
                                          operator_name);
}

static void
device_mobile_device_got_modem_manager_gsm_cb (GObject      *source_object,
                                               GAsyncResult *res,
                                               gpointer      user_data)
{
        g_autoptr(GError) error = NULL;
        NetDeviceMobile *device_mobile = (NetDeviceMobile *)user_data;

        device_mobile->gsm_proxy = g_dbus_proxy_new_for_bus_finish (res, &error);
        if (device_mobile->gsm_proxy == NULL) {
                g_warning ("Error creating ModemManager GSM proxy: %s\n",
                           error->message);
                return;
        }

        /* Setup value updates */
        g_signal_connect_swapped (device_mobile->gsm_proxy,
                                  "g-signal",
                                  G_CALLBACK (device_mobile_gsm_signal_cb),
                                  device_mobile);

        /* Load initial value */
        g_dbus_proxy_call (device_mobile->gsm_proxy,
                           "GetRegistrationInfo",
                           NULL,
                           G_DBUS_CALL_FLAGS_NONE,
                           -1,
                           NULL,
                           device_mobile_get_registration_info_cb,
                           device_mobile);
}

static void
device_mobile_get_serving_system_cb (GObject      *source_object,
                                     GAsyncResult *res,
                                     gpointer      user_data)
{
        NetDeviceMobile *device_mobile = (NetDeviceMobile *)user_data;
        g_autoptr(GVariant) result = NULL;
        g_autoptr(GError) error = NULL;

        guint32 band_class;
        g_autofree gchar *band = NULL;
        guint32 sid;
        gchar *operator_name;

        result = g_dbus_proxy_call_finish (G_DBUS_PROXY (source_object), res, &error);
        if (result == NULL) {
                g_warning ("Error getting serving system: %s\n",
                           error->message);
                return;
        }

        /* get values */
        g_variant_get (result, "((usu))",
                       &band_class,
                       &band,
                       &sid);

        operator_name = device_mobile_find_provider (device_mobile, NULL, sid);

        /* save and refresh */
        device_mobile_save_operator_name (device_mobile,
                                          "ControlCenter::OperatorNameCdma",
                                          operator_name);
}

static void
device_mobile_device_got_modem_manager_cdma_cb (GObject      *source_object,
                                                GAsyncResult *res,
                                                gpointer      user_data)
{
        g_autoptr(GError) error = NULL;
        NetDeviceMobile *device_mobile = (NetDeviceMobile *)user_data;

        device_mobile->cdma_proxy = g_dbus_proxy_new_for_bus_finish (res, &error);
        if (device_mobile->cdma_proxy == NULL) {
                g_warning ("Error creating ModemManager CDMA proxy: %s\n",
                           error->message);
                return;
        }

        /* Load initial value */
        g_dbus_proxy_call (device_mobile->cdma_proxy,
                           "GetServingSystem",
                           NULL,
                           G_DBUS_CALL_FLAGS_NONE,
                           -1,
                           NULL,
                           device_mobile_get_serving_system_cb,
                           device_mobile);
}

static void
net_device_mobile_constructed (GObject *object)
{
        GCancellable *cancellable;
        NetDeviceMobile *device_mobile = NET_DEVICE_MOBILE (object);
        NMClient *client;
        NMDevice *device;
        NMDeviceModemCapabilities caps;

        G_OBJECT_CLASS (net_device_mobile_parent_class)->constructed (object);

        device = net_device_get_nm_device (NET_DEVICE (device_mobile));
        cancellable = net_object_get_cancellable (NET_OBJECT (device_mobile));

        caps = nm_device_modem_get_current_capabilities (NM_DEVICE_MODEM (device));

        /* Only load proxies if we have broadband modems of the OLD ModemManager interface */
        if (g_str_has_prefix (nm_device_get_udi (device), "/org/freedesktop/ModemManager/") &&
            ((caps & NM_DEVICE_MODEM_CAPABILITY_GSM_UMTS) ||
             (caps & NM_DEVICE_MODEM_CAPABILITY_CDMA_EVDO) ||
             (caps & NM_DEVICE_MODEM_CAPABILITY_LTE))) {
                g_dbus_proxy_new_for_bus (G_BUS_TYPE_SYSTEM,
                                          G_DBUS_PROXY_FLAGS_NONE,
                                          NULL,
                                          "org.freedesktop.ModemManager",
                                          nm_device_get_udi (device),
                                          "org.freedesktop.ModemManager.Modem",
                                          cancellable,
                                          device_mobile_device_got_modem_manager_cb,
                                          device_mobile);

                if ((caps & NM_DEVICE_MODEM_CAPABILITY_GSM_UMTS) ||
                    (caps & NM_DEVICE_MODEM_CAPABILITY_LTE)) {
                        g_dbus_proxy_new_for_bus (G_BUS_TYPE_SYSTEM,
                                                  G_DBUS_PROXY_FLAGS_NONE,
                                                  NULL,
                                                  "org.freedesktop.ModemManager",
                                                  nm_device_get_udi (device),
                                                  "org.freedesktop.ModemManager.Modem.Gsm.Network",
                                                  cancellable,
                                                  device_mobile_device_got_modem_manager_gsm_cb,
                                                  device_mobile);
                }

                if (caps & NM_DEVICE_MODEM_CAPABILITY_CDMA_EVDO) {
                        g_dbus_proxy_new_for_bus (G_BUS_TYPE_SYSTEM,
                                                  G_DBUS_PROXY_FLAGS_NONE,
                                                  NULL,
                                                  "org.freedesktop.ModemManager",
                                                  nm_device_get_udi (device),
                                                  "org.freedesktop.ModemManager.Modem.Cdma",
                                                  cancellable,
                                                  device_mobile_device_got_modem_manager_cdma_cb,
                                                  device_mobile);
                }
        }

        client = net_object_get_client (NET_OBJECT (device_mobile));
        g_signal_connect_object (client, "notify::wwan-enabled",
                                 G_CALLBACK (mobilebb_enabled_toggled),
                                 device_mobile, G_CONNECT_SWAPPED);
        nm_device_mobile_refresh_ui (device_mobile);
}

static void
operator_name_updated (NetDeviceMobile *self)
{
        device_mobile_refresh_operator_name (self);
}

static void
net_device_mobile_setup_modem_object (NetDeviceMobile *self)
{
        MMModem3gpp *modem_3gpp;

        if (self->mm_object == NULL)
                return;

        /* Load equipment ID initially */
        device_mobile_refresh_equipment_id (self);

        /* Follow changes in operator name and load initial values */
        modem_3gpp = mm_object_peek_modem_3gpp (self->mm_object);
        if (modem_3gpp != NULL) {
                g_assert (self->operator_name_updated == 0);
                self->operator_name_updated = g_signal_connect_swapped (modem_3gpp,
                                                                        "notify::operator-name",
                                                                        G_CALLBACK (operator_name_updated),
                                                                        self);
                device_mobile_refresh_operator_name (self);
        }
}


static void
net_device_mobile_get_property (GObject    *device_,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
        NetDeviceMobile *self = NET_DEVICE_MOBILE (device_);

        switch (prop_id) {
        case PROP_MODEM_OBJECT:
                g_value_set_object (value, self->mm_object);
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (self, prop_id, pspec);
                break;
        }
}

static void
net_device_mobile_set_property (GObject      *device_,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
        NetDeviceMobile *self = NET_DEVICE_MOBILE (device_);

        switch (prop_id) {
        case PROP_MODEM_OBJECT:
                self->mm_object = g_value_dup_object (value);
                net_device_mobile_setup_modem_object (self);
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (self, prop_id, pspec);
                break;
        }
}

static void
net_device_mobile_dispose (GObject *object)
{
        NetDeviceMobile *device_mobile = NET_DEVICE_MOBILE (object);

        g_clear_object (&device_mobile->builder);
        g_clear_object (&device_mobile->gsm_proxy);
        g_clear_object (&device_mobile->cdma_proxy);

        if (device_mobile->operator_name_updated) {
                g_assert (device_mobile->mm_object != NULL);
                g_signal_handler_disconnect (mm_object_peek_modem_3gpp (device_mobile->mm_object), device_mobile->operator_name_updated);
                device_mobile->operator_name_updated = 0;
        }
        g_clear_object (&device_mobile->mm_object);
        g_clear_object (&device_mobile->mpd);

        G_OBJECT_CLASS (net_device_mobile_parent_class)->dispose (object);
}

static void
net_device_mobile_class_init (NetDeviceMobileClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);
        NetObjectClass *parent_class = NET_OBJECT_CLASS (klass);

        object_class->dispose = net_device_mobile_dispose;
        object_class->constructed = net_device_mobile_constructed;
        object_class->get_property = net_device_mobile_get_property;
        object_class->set_property = net_device_mobile_set_property;
        parent_class->add_to_stack = device_mobile_proxy_add_to_stack;
        parent_class->refresh = device_mobile_refresh;

        g_object_class_install_property (object_class,
                                         PROP_MODEM_OBJECT,
                                         g_param_spec_object ("mm-object",
                                                              NULL,
                                                              NULL,
                                                              MM_TYPE_OBJECT,
                                                              G_PARAM_READWRITE));
}

static void
net_device_mobile_init (NetDeviceMobile *device_mobile)
{
        g_autoptr(GError) error = NULL;
        GtkWidget *widget;
        GtkCellRenderer *renderer;
        GtkComboBox *combobox;

        device_mobile->builder = gtk_builder_new ();
        gtk_builder_add_from_resource (device_mobile->builder,
                                       "/org/gnome/control-center/network/network-mobile.ui",
                                       &error);
        if (error != NULL) {
                g_warning ("Could not load interface file: %s", error->message);
                return;
        }

        /* setup mobile combobox model */
        combobox = GTK_COMBO_BOX (gtk_builder_get_object (device_mobile->builder,
                                                          "network_combo"));
        g_signal_connect_swapped (combobox, "changed",
                                  G_CALLBACK (mobile_connection_changed_cb),
                                  device_mobile);
        renderer = gtk_cell_renderer_text_new ();
        gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (combobox),
                                    renderer,
                                    FALSE);
        gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (combobox), renderer,
                                        "text", COLUMN_TITLE,
                                        NULL);

        widget = GTK_WIDGET (gtk_builder_get_object (device_mobile->builder,
                                                     "device_off_switch"));
        g_signal_connect_swapped (widget, "notify::active",
                                  G_CALLBACK (device_off_toggled), device_mobile);

        widget = GTK_WIDGET (gtk_builder_get_object (device_mobile->builder,
                                                     "options_button"));
        g_signal_connect_swapped (widget, "clicked",
                                  G_CALLBACK (edit_connection), device_mobile);
}
