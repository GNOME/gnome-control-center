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

#define NET_DEVICE_MOBILE_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), NET_TYPE_DEVICE_MOBILE, NetDeviceMobilePrivate))

static void nm_device_mobile_refresh_ui (NetDeviceMobile *device_mobile);

struct _NetDeviceMobilePrivate
{
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
device_mobile_proxy_add_to_notebook (NetObject *object,
                                     GtkNotebook *notebook,
                                     GtkSizeGroup *heading_size_group)
{
        GtkWidget *widget;
        NetDeviceMobile *device_mobile = NET_DEVICE_MOBILE (object);

        /* add widgets to size group */
        widget = GTK_WIDGET (gtk_builder_get_object (device_mobile->priv->builder,
                                                     "heading_imei"));
        gtk_size_group_add_widget (heading_size_group, widget);
        widget = GTK_WIDGET (gtk_builder_get_object (device_mobile->priv->builder,
                                                     "heading_network"));
        gtk_size_group_add_widget (heading_size_group, widget);

        widget = GTK_WIDGET (gtk_builder_get_object (device_mobile->priv->builder,
                                                     "vbox7"));
        gtk_notebook_append_page (notebook, widget, NULL);
        return widget;
}

static void
connection_activate_cb (GObject *source_object,
                        GAsyncResult *res,
                        gpointer user_data)
{
        GError *error = NULL;

        if (!nm_client_activate_connection_finish (NM_CLIENT (source_object), res, &error)) {
                /* failed to activate */
                nm_device_mobile_refresh_ui (user_data);
                g_error_free (error);
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
        CcNetworkPanel *panel;
        GtkWidget *toplevel;

        if (device_mobile->priv->updating_device)
                goto out;

        ret = gtk_combo_box_get_active_iter (combo_box, &iter);
        if (!ret)
                goto out;

        device = net_device_get_nm_device (NET_DEVICE (device_mobile));
        if (device == NULL)
                goto out;
        client = net_object_get_client (NET_OBJECT (device_mobile));

        /* get entry */
        model = gtk_combo_box_get_model (GTK_COMBO_BOX (combo_box));
        gtk_tree_model_get (model, &iter,
                            COLUMN_ID, &object_path,
                            -1);
        if (g_strcmp0 (object_path, NULL) == 0) {
                panel = net_object_get_panel (NET_OBJECT (device_mobile));
                toplevel = cc_shell_get_toplevel (cc_panel_get_shell (CC_PANEL (panel)));
                cc_network_panel_connect_to_3g_network (toplevel,
                                                        client,
                                                        device);
                goto out;
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

        if (nm_client_wwan_get_enabled (client)) {
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
}

static void
device_mobile_refresh_equipment_id (NetDeviceMobile *device_mobile)
{
        const gchar *equipment_id = NULL;

        if (device_mobile->priv->mm_object != NULL) {
                MMModem *modem;

                /* Modem interface should always be present */
                modem = mm_object_peek_modem (device_mobile->priv->mm_object);
                equipment_id = mm_modem_get_equipment_identifier (modem);

                /* Set equipment ID */
                if (equipment_id != NULL) {
                        g_debug ("[%s] Equipment ID set to '%s'",
                                 mm_object_get_path (device_mobile->priv->mm_object),
                                 equipment_id);
                }
        } else {
                /* Assume old MM handling */
                equipment_id = g_object_get_data (G_OBJECT (device_mobile),
                                                  "ControlCenter::EquipmentIdentifier");
        }

        panel_set_device_widget_details (device_mobile->priv->builder, "imei", equipment_id);
}

static gchar *
device_mobile_find_provider (NetDeviceMobile *device_mobile,
                             const gchar     *mccmnc,
                             guint32          sid)
{
        NMAMobileProvider *provider;
        GString *name = NULL;

        if (device_mobile->priv->mpd == NULL) {
                GError *error = NULL;

                /* Use defaults */
                device_mobile->priv->mpd = nma_mobile_providers_database_new_sync (NULL, NULL, NULL, &error);
                if (device_mobile->priv->mpd == NULL) {
                        g_debug ("Couldn't load mobile providers database: %s",
                                 error ? error->message : "");
                        g_clear_error (&error);
                        return NULL;
                }
        }

        if (mccmnc != NULL) {
                provider = nma_mobile_providers_database_lookup_3gpp_mcc_mnc (device_mobile->priv->mpd, mccmnc);
                if (provider != NULL)
                        name = g_string_new (nma_mobile_provider_get_name (provider));
        }

        if (sid != 0) {
                provider = nma_mobile_providers_database_lookup_cdma_sid (device_mobile->priv->mpd, sid);
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
        if (device_mobile->priv->mm_object != NULL) {
                gchar *operator_name = NULL;
                MMModem3gpp *modem_3gpp;
                MMModemCdma *modem_cdma;

                modem_3gpp = mm_object_peek_modem_3gpp (device_mobile->priv->mm_object);
                modem_cdma = mm_object_peek_modem_cdma (device_mobile->priv->mm_object);

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
                                 mm_object_get_path (device_mobile->priv->mm_object),
                                 operator_name);
                }

                panel_set_device_widget_details (device_mobile->priv->builder, "provider", operator_name);
                g_free (operator_name);
        } else {
                const gchar *gsm;
                const gchar *cdma;

                /* Assume old MM handling */
                gsm = g_object_get_data (G_OBJECT (device_mobile),
                                         "ControlCenter::OperatorNameGsm");
                cdma = g_object_get_data (G_OBJECT (device_mobile),
                                          "ControlCenter::OperatorNameCdma");

                if (gsm != NULL && cdma != NULL) {
                        gchar *both;

                        both = g_strdup_printf ("%s, %s", gsm, cdma);
                        panel_set_device_widget_details (device_mobile->priv->builder, "provider", both);
                        g_free (both);
                } else if (gsm != NULL) {
                        panel_set_device_widget_details (device_mobile->priv->builder, "provider", gsm);
                } else if (cdma != NULL) {
                        panel_set_device_widget_details (device_mobile->priv->builder, "provider", cdma);
                } else {
                        panel_set_device_widget_details (device_mobile->priv->builder, "provider", NULL);
                }
        }
}

static void
nm_device_mobile_refresh_ui (NetDeviceMobile *device_mobile)
{
        gboolean is_connected;
        GtkListStore *liststore;
        GtkWidget *widget;
        NetDeviceMobilePrivate *priv = device_mobile->priv;
        NMClient *client;
        NMDeviceModemCapabilities caps;
        NMDevice *nm_device;

        nm_device = net_device_get_nm_device (NET_DEVICE (device_mobile));

        /* set device kind */
        widget = GTK_WIDGET (gtk_builder_get_object (device_mobile->priv->builder, "label_device"));
        g_object_bind_property (device_mobile, "title", widget, "label", 0);

        /* set up the device on/off switch */
        widget = GTK_WIDGET (gtk_builder_get_object (device_mobile->priv->builder, "device_off_switch"));
        gtk_widget_show (widget);
        client = net_object_get_client (NET_OBJECT (device_mobile));
        mobilebb_enabled_toggled (client, NULL, device_mobile);

        /* set device state, with status */
        panel_set_device_status (device_mobile->priv->builder, "label_status", nm_device, NULL);

        /* sensitive for other connection types if the device is currently connected */
        widget = GTK_WIDGET (gtk_builder_get_object (device_mobile->priv->builder,
                                                     "button_options"));
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
        NetDeviceMobile *device_mobile = (NetDeviceMobile *)user_data;

        proxy = g_dbus_proxy_new_for_bus_finish (res, &error);
        if (!proxy) {
                g_warning ("Error creating ModemManager proxy: %s",
                           error->message);
                g_error_free (error);
                return;
        }

        /* get the IMEI */
        result = g_dbus_proxy_get_cached_property (proxy,
                                                   "EquipmentIdentifier");

        /* save */
        if (result) {
                g_object_set_data_full (G_OBJECT (device_mobile),
                                        "ControlCenter::EquipmentIdentifier",
                                        g_variant_dup_string (result, NULL),
                                        g_free);
                g_variant_unref (result);
        }

        device_mobile_refresh_equipment_id (device_mobile);
        g_object_unref (proxy);
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
        gchar *operator_code = NULL;
        GError *error = NULL;
        guint registration_status;
        GVariant *result = NULL;
        gchar *operator_name = NULL;
        NetDeviceMobile *device_mobile = (NetDeviceMobile *)user_data;

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

        /* If none give, try to guess it */
        if (operator_name == NULL || operator_name[0] == '\0') {
                g_free (operator_name);
                operator_name = device_mobile_find_provider (device_mobile, operator_code, 0);
        }

        /* save and refresh */
        device_mobile_save_operator_name (device_mobile,
                                          "ControlCenter::OperatorNameGsm",
                                          operator_name);

        g_free (operator_name);
        g_free (operator_code);
        g_variant_unref (result);
}

static void
device_mobile_gsm_signal_cb (GDBusProxy *proxy,
                             gchar      *sender_name,
                             gchar      *signal_name,
                             GVariant   *parameters,
                             gpointer    user_data)
{
        guint registration_status = 0;
        gchar *operator_code = NULL;
        gchar *operator_name = NULL;
        NetDeviceMobile *device_mobile = (NetDeviceMobile *)user_data;

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

        g_free (operator_code);
        g_free (operator_name);
}

static void
device_mobile_device_got_modem_manager_gsm_cb (GObject      *source_object,
                                               GAsyncResult *res,
                                               gpointer      user_data)
{
        GError *error = NULL;
        NetDeviceMobile *device_mobile = (NetDeviceMobile *)user_data;

        device_mobile->priv->gsm_proxy = g_dbus_proxy_new_for_bus_finish (res, &error);
        if (device_mobile->priv->gsm_proxy == NULL) {
                g_warning ("Error creating ModemManager GSM proxy: %s\n",
                           error->message);
                g_error_free (error);
                return;
        }

        /* Setup value updates */
        g_signal_connect (device_mobile->priv->gsm_proxy,
                          "g-signal",
                          G_CALLBACK (device_mobile_gsm_signal_cb),
                          device_mobile);

        /* Load initial value */
        g_dbus_proxy_call (device_mobile->priv->gsm_proxy,
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
        GVariant *result = NULL;
        GError *error = NULL;

        guint32 band_class;
        gchar *band;
        guint32 sid;
        gchar *operator_name;

        result = g_dbus_proxy_call_finish (G_DBUS_PROXY (source_object), res, &error);
        if (result == NULL) {
                g_warning ("Error getting serving system: %s\n",
                           error->message);
                g_error_free (error);
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

        g_free (band);
        g_variant_unref (result);
}

static void
device_mobile_device_got_modem_manager_cdma_cb (GObject      *source_object,
                                                GAsyncResult *res,
                                                gpointer      user_data)
{
        GError *error = NULL;
        NetDeviceMobile *device_mobile = (NetDeviceMobile *)user_data;

        device_mobile->priv->cdma_proxy = g_dbus_proxy_new_for_bus_finish (res, &error);
        if (device_mobile->priv->cdma_proxy == NULL) {
                g_warning ("Error creating ModemManager CDMA proxy: %s\n",
                           error->message);
                g_error_free (error);
                return;
        }

        /* Load initial value */
        g_dbus_proxy_call (device_mobile->priv->cdma_proxy,
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
                                 device_mobile, 0);
        nm_device_mobile_refresh_ui (device_mobile);
}

static void
operator_name_updated (MMModem3gpp     *modem_3gpp_iface,
                       GParamSpec      *pspec,
                       NetDeviceMobile *self)
{
        device_mobile_refresh_operator_name (self);
}

static void
net_device_mobile_setup_modem_object (NetDeviceMobile *self)
{
        MMModem3gpp *modem_3gpp;

        if (self->priv->mm_object == NULL)
                return;

        /* Load equipment ID initially */
        device_mobile_refresh_equipment_id (self);

        /* Follow changes in operator name and load initial values */
        modem_3gpp = mm_object_peek_modem_3gpp (self->priv->mm_object);
        if (modem_3gpp != NULL) {
                g_assert (self->priv->operator_name_updated == 0);
                self->priv->operator_name_updated = g_signal_connect (modem_3gpp,
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
                g_value_set_object (value, self->priv->mm_object);
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
                self->priv->mm_object = g_value_dup_object (value);
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
        NetDeviceMobilePrivate *priv = device_mobile->priv;

        g_clear_object (&priv->builder);
        g_clear_object (&priv->gsm_proxy);
        g_clear_object (&priv->cdma_proxy);

        if (priv->operator_name_updated) {
                g_assert (priv->mm_object != NULL);
                g_signal_handler_disconnect (mm_object_peek_modem_3gpp (priv->mm_object), priv->operator_name_updated);
                priv->operator_name_updated = 0;
        }
        g_clear_object (&priv->mm_object);
        g_clear_object (&priv->mpd);

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
        parent_class->add_to_notebook = device_mobile_proxy_add_to_notebook;
        parent_class->refresh = device_mobile_refresh;

        g_type_class_add_private (klass, sizeof (NetDeviceMobilePrivate));

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
        GError *error = NULL;
        GtkWidget *widget;
        GtkCellRenderer *renderer;
        GtkComboBox *combobox;

        device_mobile->priv = NET_DEVICE_MOBILE_GET_PRIVATE (device_mobile);

        device_mobile->priv->builder = gtk_builder_new ();
        gtk_builder_add_from_resource (device_mobile->priv->builder,
                                       "/org/gnome/control-center/network/network-mobile.ui",
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
}
