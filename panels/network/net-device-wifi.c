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

//#include <arpa/inet.h>
#include <netinet/ether.h>

#include <nm-client.h>
#include <nm-utils.h>
#include <nm-device.h>
#include <nm-device-wifi.h>
#include <nm-device-ethernet.h>
#include <nm-setting-wireless-security.h>
#include <nm-remote-connection.h>
#include <nm-setting-wireless.h>

#include "network-dialogs.h"
#include "panel-common.h"
#include "panel-cell-renderer-mode.h"
#include "panel-cell-renderer-signal.h"
#include "panel-cell-renderer-security.h"
#include "panel-cell-renderer-separator.h"
#include "panel-cell-renderer-text.h"
#include "panel-cell-renderer-pixbuf.h"

#include "net-device-wifi.h"

#define NET_DEVICE_WIFI_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), NET_TYPE_DEVICE_WIFI, NetDeviceWifiPrivate))

static void nm_device_wifi_refresh_ui (NetDeviceWifi *device_wifi);
static void show_wifi_list (NetDeviceWifi *device_wifi);

struct _NetDeviceWifiPrivate
{
        GtkBuilder              *builder;
        gboolean                 updating_device;
        gchar                   *selected_ssid_title;
        gchar                   *selected_connection_id;
        gchar                   *selected_ap_id;
};

G_DEFINE_TYPE (NetDeviceWifi, net_device_wifi, NET_TYPE_DEVICE)

enum {
        COLUMN_CONNECTION_ID,
        COLUMN_ACCESS_POINT_ID,
        COLUMN_TITLE,
        COLUMN_SORT,
        COLUMN_STRENGTH,
        COLUMN_MODE,
        COLUMN_SECURITY,
        COLUMN_ACTIVE,
        COLUMN_AP_IN_RANGE,
        COLUMN_AP_OUT_OF_RANGE,
        COLUMN_AP_IS_SAVED,
        COLUMN_LAST
};

static GtkWidget *
device_wifi_proxy_add_to_notebook (NetObject *object,
                                    GtkNotebook *notebook,
                                    GtkSizeGroup *heading_size_group)
{
        GtkWidget *widget;
        GtkWindow *window;
        NetDeviceWifi *device_wifi = NET_DEVICE_WIFI (object);

        /* add widgets to size group */
        widget = GTK_WIDGET (gtk_builder_get_object (device_wifi->priv->builder,
                                                     "heading_ipv4"));
        gtk_size_group_add_widget (heading_size_group, widget);

        /* reparent */
        window = GTK_WINDOW (gtk_builder_get_object (device_wifi->priv->builder,
                                                     "window_tmp"));
        widget = GTK_WIDGET (gtk_builder_get_object (device_wifi->priv->builder,
                                                     "notebook_view"));
        g_object_ref (widget);
        gtk_container_remove (GTK_CONTAINER (window), widget);
        gtk_notebook_append_page (notebook, widget, NULL);
        g_object_unref (widget);

        return widget;
}

static guint
get_access_point_security (NMAccessPoint *ap)
{
        NM80211ApFlags flags;
        NM80211ApSecurityFlags wpa_flags;
        NM80211ApSecurityFlags rsn_flags;
        guint type;

        flags = nm_access_point_get_flags (ap);
        wpa_flags = nm_access_point_get_wpa_flags (ap);
        rsn_flags = nm_access_point_get_rsn_flags (ap);

        if (!(flags & NM_802_11_AP_FLAGS_PRIVACY) &&
            wpa_flags == NM_802_11_AP_SEC_NONE &&
            rsn_flags == NM_802_11_AP_SEC_NONE)
                type = NM_AP_SEC_NONE;
        else if ((flags & NM_802_11_AP_FLAGS_PRIVACY) &&
                 wpa_flags == NM_802_11_AP_SEC_NONE &&
                 rsn_flags == NM_802_11_AP_SEC_NONE)
                type = NM_AP_SEC_WEP;
        else if (!(flags & NM_802_11_AP_FLAGS_PRIVACY) &&
                 wpa_flags != NM_802_11_AP_SEC_NONE &&
                 rsn_flags != NM_802_11_AP_SEC_NONE)
                type = NM_AP_SEC_WPA;
        else
                type = NM_AP_SEC_WPA2;

        return type;
}

static void
add_access_point (NetDeviceWifi *device_wifi, NMAccessPoint *ap, NMAccessPoint *active, NMDevice *device)
{
        const GByteArray *ssid;
        const gchar *object_path;
        const gchar *ssid_text;
        gboolean is_active_ap;
        gchar *title;
        GtkListStore *liststore_network;
        GtkTreeIter treeiter;
        NetDeviceWifiPrivate *priv = device_wifi->priv;

        ssid = nm_access_point_get_ssid (ap);
        if (ssid == NULL)
                return;
        ssid_text = nm_utils_escape_ssid (ssid->data, ssid->len);
        title = g_markup_escape_text (ssid_text, -1);

        is_active_ap = active && nm_utils_same_ssid (ssid, nm_access_point_get_ssid (active), TRUE);
        liststore_network = GTK_LIST_STORE (gtk_builder_get_object (priv->builder,
                                            "liststore_network"));

        object_path = nm_object_get_path (NM_OBJECT (ap));
        gtk_list_store_insert_with_values (liststore_network,
                                           &treeiter,
                                           -1,
                                           COLUMN_ACCESS_POINT_ID, object_path,
                                           COLUMN_TITLE, title,
                                           COLUMN_SORT, ssid_text,
                                           COLUMN_STRENGTH, nm_access_point_get_strength (ap),
                                           COLUMN_MODE, nm_access_point_get_mode (ap),
                                           COLUMN_SECURITY, get_access_point_security (ap),
                                           COLUMN_ACTIVE, is_active_ap,
                                           COLUMN_AP_IN_RANGE, TRUE,
                                           COLUMN_AP_OUT_OF_RANGE, FALSE,
                                           COLUMN_AP_IS_SAVED, FALSE,
                                           -1);
        g_free (title);
}

static GPtrArray *
panel_get_strongest_unique_aps (const GPtrArray *aps)
{
        const GByteArray *ssid;
        const GByteArray *ssid_tmp;
        GPtrArray *aps_unique = NULL;
        gboolean add_ap;
        guint i;
        guint j;
        NMAccessPoint *ap;
        NMAccessPoint *ap_tmp;

        /* we will have multiple entries for typical hotspots, just
         * filter to the one with the strongest signal */
        aps_unique = g_ptr_array_new_with_free_func ((GDestroyNotify) g_object_unref);
        if (aps != NULL)
                for (i = 0; i < aps->len; i++) {
                        ap = NM_ACCESS_POINT (g_ptr_array_index (aps, i));

                        /* Hidden SSIDs don't get shown in the list */
                        ssid = nm_access_point_get_ssid (ap);
                        if (!ssid)
                                continue;

                        add_ap = TRUE;

                        /* get already added list */
                        for (j=0; j<aps_unique->len; j++) {
                                ap_tmp = NM_ACCESS_POINT (g_ptr_array_index (aps_unique, j));
                                ssid_tmp = nm_access_point_get_ssid (ap_tmp);
                                g_assert (ssid_tmp);

                                /* is this the same type and data? */
                                if (nm_utils_same_ssid (ssid, ssid_tmp, TRUE)) {

                                        g_debug ("found duplicate: %s",
                                                 nm_utils_escape_ssid (ssid_tmp->data,
                                                                       ssid_tmp->len));

                                        /* the new access point is stronger */
                                        if (nm_access_point_get_strength (ap) >
                                            nm_access_point_get_strength (ap_tmp)) {
                                                g_debug ("removing %s",
                                                         nm_utils_escape_ssid (ssid_tmp->data,
                                                                               ssid_tmp->len));
                                                g_ptr_array_remove (aps_unique, ap_tmp);
                                                add_ap = TRUE;
                                        } else {
                                                add_ap = FALSE;
                                        }

                                        break;
                                }
                        }
                        if (add_ap) {
                                g_debug ("adding %s",
                                         nm_utils_escape_ssid (ssid->data,
                                                               ssid->len));
                                g_ptr_array_add (aps_unique, g_object_ref (ap));
                        }
                }
        return aps_unique;
}

static gchar *
get_ap_security_string (NMAccessPoint *ap)
{
        NM80211ApSecurityFlags wpa_flags, rsn_flags;
        NM80211ApFlags flags;
        GString *str;

        flags = nm_access_point_get_flags (ap);
        wpa_flags = nm_access_point_get_wpa_flags (ap);
        rsn_flags = nm_access_point_get_rsn_flags (ap);

        str = g_string_new ("");
        if ((flags & NM_802_11_AP_FLAGS_PRIVACY) &&
            (wpa_flags == NM_802_11_AP_SEC_NONE) &&
            (rsn_flags == NM_802_11_AP_SEC_NONE)) {
                /* TRANSLATORS: this WEP WiFi security */
                g_string_append_printf (str, "%s, ", _("WEP"));
        }
        if (wpa_flags != NM_802_11_AP_SEC_NONE) {
                /* TRANSLATORS: this WPA WiFi security */
                g_string_append_printf (str, "%s, ", _("WPA"));
        }
        if (rsn_flags != NM_802_11_AP_SEC_NONE) {
                /* TRANSLATORS: this WPA WiFi security */
                g_string_append_printf (str, "%s, ", _("WPA2"));
        }
        if ((wpa_flags & NM_802_11_AP_SEC_KEY_MGMT_802_1X) ||
            (rsn_flags & NM_802_11_AP_SEC_KEY_MGMT_802_1X)) {
                /* TRANSLATORS: this Enterprise WiFi security */
                g_string_append_printf (str, "%s, ", _("Enterprise"));
        }
        if (str->len > 0)
                g_string_set_size (str, str->len - 2);
        else {
                g_string_append (str, C_("Wifi security", "None"));
        }
        return g_string_free (str, FALSE);
}

static void
wireless_enabled_toggled (NMClient       *client,
                          GParamSpec     *pspec,
                          NetDeviceWifi *device_wifi)
{
        gboolean enabled;
        GtkSwitch *sw;
        NMDevice *device;

        device = net_device_get_nm_device (NET_DEVICE (device_wifi));
        if (nm_device_get_device_type (device) != NM_DEVICE_TYPE_WIFI)
                return;

        enabled = nm_client_wireless_get_enabled (client);
        sw = GTK_SWITCH (gtk_builder_get_object (device_wifi->priv->builder,
                                                 "device_off_switch"));

        device_wifi->priv->updating_device = TRUE;
        gtk_switch_set_active (sw, enabled);
        device_wifi->priv->updating_device = FALSE;
}

#if 0
static void
update_off_switch_from_device_state (GtkSwitch *sw,
                                     NMDeviceState state,
                                     NetDeviceWifi *device_wifi)
{
        device_wifi->priv->updating_device = TRUE;
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
        device_wifi->priv->updating_device = FALSE;
}
#endif

static NMConnection *
find_connection_for_device (NetDeviceWifi *device_wifi,
                            NMDevice       *device)
{
        NetDevice *tmp;
        NMConnection *connection;
        NMRemoteSettings *remote_settings;
        NMClient *client;

        client = net_object_get_client (NET_OBJECT (device_wifi));
        remote_settings = net_object_get_remote_settings (NET_OBJECT (device_wifi));
        tmp = g_object_new (NET_TYPE_DEVICE,
                            "client", client,
                            "remote-settings", remote_settings,
                            "nm-device", device,
                            NULL);
        connection = net_device_get_find_connection (tmp);
        g_object_unref (tmp);
        return connection;
}

static gboolean
connection_is_shared (NMConnection *c)
{
        NMSettingIP4Config *s_ip4;

        s_ip4 = nm_connection_get_setting_ip4_config (c);
        if (g_strcmp0 (nm_setting_ip4_config_get_method (s_ip4),
                       NM_SETTING_IP4_CONFIG_METHOD_SHARED) != 0) {
                return FALSE;
        }

        return TRUE;
}

static gboolean
device_is_hotspot (NetDeviceWifi *device_wifi)
{
        NMConnection *c;
        NMDevice *device;

        device = net_device_get_nm_device (NET_DEVICE (device_wifi));
        c = find_connection_for_device (device_wifi, device);
        if (c == NULL)
                return FALSE;

        return connection_is_shared (c);
}

static const GByteArray *
device_get_hotspot_ssid (NetDeviceWifi *device_wifi,
                         NMDevice *device)
{
        NMConnection *c;
        NMSettingWireless *sw;

        c = find_connection_for_device (device_wifi, device);
        if (c == NULL) {
                return FALSE;
        }

        sw = nm_connection_get_setting_wireless (c);
        return nm_setting_wireless_get_ssid (sw);
}

static void
get_secrets_cb (NMRemoteConnection *c,
                GHashTable         *secrets,
                GError             *error,
                gpointer            data)
{
        NetDeviceWifi *device_wifi = data;
        NMSettingWireless *sw;

        sw = nm_connection_get_setting_wireless (NM_CONNECTION (c));

        nm_connection_update_secrets (NM_CONNECTION (c),
                                      nm_setting_wireless_get_security (sw),
                                      secrets, NULL);

        nm_device_wifi_refresh_ui (device_wifi);
}

static void
device_get_hotspot_security_details (NetDeviceWifi *device_wifi,
                                     NMDevice *device,
                                     gchar **secret,
                                     gchar **security)
{
        NMConnection *c;
        NMSettingWireless *sw;
        NMSettingWirelessSecurity *sws;
        const gchar *key_mgmt;
        const gchar *tmp_secret;
        const gchar *tmp_security;

        c = find_connection_for_device (device_wifi, device);
        if (c == NULL)
                return;

        sw = nm_connection_get_setting_wireless (c);
        sws = nm_connection_get_setting_wireless_security (c);
        if (sw == NULL || sws == NULL)
                return;

        tmp_secret = NULL;
        tmp_security = C_("Wifi security", "None");

        key_mgmt = nm_setting_wireless_security_get_key_mgmt (sws);
        if (strcmp (key_mgmt, "none") == 0) {
                tmp_secret = nm_setting_wireless_security_get_wep_key (sws, 0);
                tmp_security = _("WEP");
        }
        else if (strcmp (key_mgmt, "wpa-none") == 0) {
                tmp_secret = nm_setting_wireless_security_get_psk (sws);
                tmp_security = _("WPA");
        } else {
                g_warning ("unhandled security key-mgmt: %s", key_mgmt);
        }

        /* If we don't have secrets, request them from NM and bail.
         * We'll refresh the UI when secrets arrive.
         */
        if (tmp_secret == NULL) {
                nm_remote_connection_get_secrets ((NMRemoteConnection*)c,
                                                  nm_setting_wireless_get_security (sw),
                                                  get_secrets_cb,
                                                  device_wifi);
                return;
        }

        if (secret)
                *secret = g_strdup (tmp_secret);
        if (security)
                *security = g_strdup (tmp_security);
}

static void
device_wifi_refresh_aps (NetDeviceWifi *device_wifi)
{
        const GPtrArray *aps;
        GPtrArray *aps_unique = NULL;
        GtkListStore *liststore_network;
        guint i;
        NMAccessPoint *active_ap;
        NMAccessPoint *ap;
        NMDevice *nm_device;

        /* populate access points */
        liststore_network = GTK_LIST_STORE (gtk_builder_get_object (device_wifi->priv->builder,
                                                                    "liststore_network"));
        device_wifi->priv->updating_device = TRUE;
        gtk_list_store_clear (liststore_network);
        nm_device = net_device_get_nm_device (NET_DEVICE (device_wifi));
        aps = nm_device_wifi_get_access_points (NM_DEVICE_WIFI (nm_device));
        aps_unique = panel_get_strongest_unique_aps (aps);
        active_ap = nm_device_wifi_get_active_access_point (NM_DEVICE_WIFI (nm_device));

        for (i = 0; i < aps_unique->len; i++) {
                ap = NM_ACCESS_POINT (g_ptr_array_index (aps_unique, i));
                add_access_point (device_wifi, ap, active_ap, nm_device);
        }

        device_wifi->priv->updating_device = FALSE;
        g_ptr_array_unref (aps_unique);
}

static gboolean
find_ssid_in_store (GtkTreeModel *model, GtkTreeIter *iter, const gchar *ssid)
{
        gboolean found;
        gchar *sort;

        found = gtk_tree_model_get_iter_first (model, iter);

        while (found) {
                gtk_tree_model_get (model, iter,
                                    COLUMN_SORT, &sort,
                                    -1);
                if (g_strcmp0 (ssid, sort) == 0) {
                        g_free (sort);
                        return TRUE;
                }
                g_free (sort);
                found = gtk_tree_model_iter_next (model, iter);
        }

        return FALSE;

}

static void
add_saved_connection (NetDeviceWifi *device_wifi, NMConnection *connection, NMDevice *nm_device)
{
        const GByteArray *ssid;
        const gchar *id;
        const gchar *ssid_text;
        gchar *title;
        GtkListStore *store;
        GtkTreeIter iter;
        NMSetting *setting;

        setting = nm_connection_get_setting_by_name (connection, NM_SETTING_WIRELESS_SETTING_NAME);

        if (setting == NULL)
                return;

        ssid = nm_setting_wireless_get_ssid (NM_SETTING_WIRELESS (setting));
        ssid_text = nm_utils_escape_ssid (ssid->data, ssid->len);
        title = g_markup_escape_text (ssid_text, -1);
        g_debug ("got saved %s", title);

        id = nm_connection_get_path (connection);

        store = GTK_LIST_STORE (gtk_builder_get_object (device_wifi->priv->builder,
                                                        "liststore_network"));
        if (find_ssid_in_store (GTK_TREE_MODEL (store), &iter, ssid_text))
                gtk_list_store_set (store, &iter,
                                    COLUMN_CONNECTION_ID, id,
                                    COLUMN_AP_IS_SAVED, TRUE,
                                    -1);
        else
                gtk_list_store_insert_with_values (store, &iter,
                                                   -1,
                                                   COLUMN_CONNECTION_ID, id,
                                                   COLUMN_TITLE, title,
                                                   COLUMN_SORT, ssid_text,
                                                   COLUMN_STRENGTH, 0,
                                                   COLUMN_MODE, 0,
                                                   COLUMN_SECURITY, 0,
                                                   COLUMN_ACTIVE, FALSE,
                                                   COLUMN_AP_IN_RANGE, FALSE,
                                                   COLUMN_AP_OUT_OF_RANGE, TRUE,
                                                   COLUMN_AP_IS_SAVED, TRUE,
                                                   -1);
        g_free (title);
}

static void
device_wifi_refresh_saved_connections (NetDeviceWifi *device_wifi)
{
        GSList *connections;
        GSList *filtered;
        GSList *l;
        NMDevice *nm_device;
        NMRemoteSettings *remote_settings;

        /* add stored connections */
        device_wifi->priv->updating_device = TRUE;
        remote_settings = net_object_get_remote_settings (NET_OBJECT (device_wifi));
        connections = nm_remote_settings_list_connections (remote_settings);
        nm_device = net_device_get_nm_device (NET_DEVICE (device_wifi));
        filtered = nm_device_filter_connections (nm_device, connections);
        for (l = filtered; l; l = l->next) {
                NMConnection *connection = l->data;
                if (!connection_is_shared (connection))
                        add_saved_connection (device_wifi, connection, nm_device);
        }
        device_wifi->priv->updating_device = FALSE;

        g_slist_free (connections);
        g_slist_free (filtered);
}

static void
nm_device_wifi_refresh_hotspot (NetDeviceWifi *device_wifi)
{
        const GByteArray *ssid;
        gchar *hotspot_secret = NULL;
        gchar *hotspot_security = NULL;
        gchar *hotspot_ssid = NULL;
        NMDevice *nm_device;

        /* refresh hotspot ui */
        nm_device = net_device_get_nm_device (NET_DEVICE (device_wifi));
        ssid = device_get_hotspot_ssid (device_wifi, nm_device);
        if (ssid)
                hotspot_ssid = nm_utils_ssid_to_utf8 (ssid);
        device_get_hotspot_security_details (device_wifi,
                                             nm_device,
                                             &hotspot_secret,
                                             &hotspot_security);

        panel_set_device_widget_details (device_wifi->priv->builder,
                                         "hotspot_network_name",
                                         hotspot_ssid);
        panel_set_device_widget_details (device_wifi->priv->builder,
                                         "hotspot_security_key",
                                         hotspot_secret);
        panel_set_device_widget_details (device_wifi->priv->builder,
                                         "hotspot_security",
                                         hotspot_security);
        panel_set_device_widget_details (device_wifi->priv->builder,
                                         "hotspot_connected",
                                         NULL);

        g_free (hotspot_secret);
        g_free (hotspot_security);
        g_free (hotspot_ssid);
}

static void
update_last_used (NetDeviceWifi *device_wifi)
{
        NetDeviceWifiPrivate *priv = device_wifi->priv;
        gchar *last_used = NULL;
        GDateTime *now = NULL;
        GDateTime *then = NULL;
        gint days;
        GTimeSpan diff;
        guint64 timestamp;
        NMRemoteConnection *connection;
        NMRemoteSettings *settings;
        NMSettingConnection *s_con;

        if (priv->selected_connection_id == NULL)
                goto out;

        settings = net_object_get_remote_settings (NET_OBJECT (device_wifi));
        connection = nm_remote_settings_get_connection_by_path (settings,
                                                                priv->selected_connection_id);
        s_con = nm_connection_get_setting_connection (NM_CONNECTION (connection));
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
        panel_set_device_widget_details (device_wifi->priv->builder,
                                         "last_used",
                                         last_used);
        if (now != NULL)
                g_date_time_unref (now);
        if (then != NULL)
                g_date_time_unref (then);
        g_free (last_used);
}

static void
nm_device_wifi_refresh_ui (NetDeviceWifi *device_wifi)
{
        const gchar *str;
        gboolean is_hotspot;
        gchar *str_tmp = NULL;
        GtkWidget *widget;
        gint strength = 0;
        guint speed = 0;
        NMAccessPoint *active_ap;
        NMDevice *nm_device;
        NMDeviceState state;
        NMClient *client;
        NMAccessPoint *ap;
        NetDeviceWifiPrivate *priv = device_wifi->priv;

        is_hotspot = device_is_hotspot (device_wifi);
        if (is_hotspot) {
                nm_device_wifi_refresh_hotspot (device_wifi);
                return;
        }

        nm_device = net_device_get_nm_device (NET_DEVICE (device_wifi));

        if (priv->selected_ap_id) {
                ap = nm_device_wifi_get_access_point_by_path (NM_DEVICE_WIFI (nm_device),
                                                              priv->selected_ap_id);
        }
        else {
                ap = NULL;
        }

        active_ap = nm_device_wifi_get_active_access_point (NM_DEVICE_WIFI (nm_device));

        state = nm_device_get_state (nm_device);

        /* keep this in sync with the signal handler setup in cc_network_panel_init */
        client = net_object_get_client (NET_OBJECT (device_wifi));
        wireless_enabled_toggled (client, NULL, device_wifi);

        if (ap != active_ap)
                speed = 0;
        else if (state != NM_DEVICE_STATE_UNAVAILABLE)
                speed = nm_device_wifi_get_bitrate (NM_DEVICE_WIFI (nm_device));
        speed /= 1000;
        if (speed > 0) {
                /* Translators: network device speed */
                str_tmp = g_strdup_printf (_("%d Mb/s"), speed);
        }
        panel_set_device_widget_details (device_wifi->priv->builder,
                                         "speed",
                                         str_tmp);

        /* set device state, with status and optionally speed */
        widget = GTK_WIDGET (gtk_builder_get_object (device_wifi->priv->builder, "label_status"));
        if (ap != active_ap) {
                if (ap)
                        gtk_label_set_label (GTK_LABEL (widget), _("Not connected"));
                else
                        gtk_label_set_label (GTK_LABEL (widget), _("Out of range"));
                gtk_widget_set_tooltip_text (widget, "");
        } else {
                gtk_label_set_label (GTK_LABEL (widget),
                                     panel_device_state_to_localized_string (nm_device));
                gtk_widget_set_tooltip_text (widget, panel_device_state_reason_to_localized_string (nm_device));
        }

        /* device MAC */
        str = nm_device_wifi_get_hw_address (NM_DEVICE_WIFI (nm_device));
        panel_set_device_widget_details (device_wifi->priv->builder,
                                         "mac",
                                         str);
        /* security */
        if (ap != active_ap)
                str_tmp = NULL;
        else if (active_ap != NULL)
                str_tmp = get_ap_security_string (active_ap);
        panel_set_device_widget_details (device_wifi->priv->builder,
                                         "security",
                                         str_tmp);
        g_free (str_tmp);

        /* signal strength */
        if (ap != NULL)
                strength = nm_access_point_get_strength (ap);
        else
                strength = 0;
        if (strength <= 0)
                str = NULL;
        else if (strength < 20)
                str = C_("Signal strength", "None");
        else if (strength < 40)
                str = C_("Signal strength", "Weak");
        else if (strength < 50)
                str = C_("Signal strength", "Ok");
        else if (strength < 80)
                str = C_("Signal strength", "Good");
        else
                str = C_("Signal strength", "Excellent");
        panel_set_device_widget_details (device_wifi->priv->builder,
                                         "strength",
                                         str);

        widget = GTK_WIDGET (gtk_builder_get_object (device_wifi->priv->builder, "label_device"));
        gtk_label_set_label (GTK_LABEL (widget),
                             priv->selected_ssid_title ? priv->selected_ssid_title : panel_device_to_localized_string (nm_device));

        /* only disconnect when connection active */
        if (ap == active_ap) {
                widget = GTK_WIDGET (gtk_builder_get_object (device_wifi->priv->builder,
                                                             "button_disconnect1"));
                gtk_widget_set_sensitive (widget, state == NM_DEVICE_STATE_ACTIVATED);
                gtk_widget_show (widget);
                widget = GTK_WIDGET (gtk_builder_get_object (device_wifi->priv->builder,
                                                             "button_connect1"));
                gtk_widget_hide (widget);
        } else {
                widget = GTK_WIDGET (gtk_builder_get_object (device_wifi->priv->builder,
                                                             "button_disconnect1"));
                gtk_widget_hide (widget);
                widget = GTK_WIDGET (gtk_builder_get_object (device_wifi->priv->builder,
                                                             "button_connect1"));
                gtk_widget_show (widget);
                gtk_widget_set_sensitive (widget, ap != NULL);
        }

        /* device MAC */
        if (ap != active_ap)
                str = NULL;
        else
                str = nm_device_wifi_get_hw_address (NM_DEVICE_WIFI (nm_device));
        panel_set_device_widget_details (priv->builder, "mac", str);

        /* set IP entries */
        if (ap != active_ap)
                panel_unset_device_widgets (priv->builder);
        else
                panel_set_device_widgets (priv->builder, nm_device);

        if (ap != active_ap)
                update_last_used (device_wifi);
        else
                panel_set_device_widget_details (priv->builder, "last_used", NULL);

        /* update list of APs */
        device_wifi_refresh_aps (device_wifi);
        device_wifi_refresh_saved_connections (device_wifi);
}

static void
device_wifi_refresh (NetObject *object)
{
        NetDeviceWifi *device_wifi = NET_DEVICE_WIFI (object);
        nm_device_wifi_refresh_ui (device_wifi);
}

static void
device_off_toggled (GtkSwitch *sw,
                    GParamSpec *pspec,
                    NetDeviceWifi *device_wifi)
{
        NMClient *client;
        gboolean active;

        if (device_wifi->priv->updating_device)
                return;

        client = net_object_get_client (NET_OBJECT (device_wifi));
        active = gtk_switch_get_active (sw);
        nm_client_wireless_set_enabled (client, active);
}


static gboolean
find_connection_id_in_store (GtkTreeModel *model,
                             GtkTreeIter  *iter,
                             const gchar  *connection_id)
{
        gboolean found;
        gchar *id;

        found = gtk_tree_model_get_iter_first (model, iter);
        while (found) {
                gtk_tree_model_get (model, iter,
                                    COLUMN_CONNECTION_ID, &id,
                                    -1);
                if (g_strcmp0 (connection_id, id) == 0) {
                        g_free (id);
                        return TRUE;
                }
                g_free (id);
                found = gtk_tree_model_iter_next (model, iter);
        }
        return FALSE;
}

static void
forget_network_connection_delete_cb (NMRemoteConnection *connection,
                                     GError *error,
                                     gpointer user_data)
{
        gboolean ret;
        GtkTreeIter iter;
        GtkTreeModel *model;
        GtkTreeView *treeview;

        NetDeviceWifi *device_wifi = NET_DEVICE_WIFI (user_data);

        if (error != NULL) {
                g_warning ("failed to delete connection %s: %s",
                           nm_object_get_path (NM_OBJECT (connection)),
                           error->message);
                return;
        }

        /* remove the entry from the list */
        treeview = GTK_TREE_VIEW (gtk_builder_get_object (device_wifi->priv->builder,
                                                         "treeview_list"));
        model = gtk_tree_view_get_model (treeview);
        ret = find_connection_id_in_store (model, &iter,
                                           device_wifi->priv->selected_connection_id);
        if (ret)
                gtk_list_store_remove (GTK_LIST_STORE (model), &iter);
        show_wifi_list (device_wifi);
}

static void
forget_network_response_cb (GtkWidget *dialog,
                            gint response,
                            NetDeviceWifi *device_wifi)
{
        NMRemoteConnection *connection;
        NMRemoteSettings *remote_settings;

        if (response != GTK_RESPONSE_OK)
                goto out;

        remote_settings = net_object_get_remote_settings (NET_OBJECT (device_wifi));
        connection = nm_remote_settings_get_connection_by_path (remote_settings, device_wifi->priv->selected_connection_id);
        if (connection == NULL) {
                g_warning ("failed to get remote connection");
                goto out;
        }

        /* delete the connection */
        g_debug ("deleting %s", device_wifi->priv->selected_connection_id);
        nm_remote_connection_delete (connection,
                                     forget_network_connection_delete_cb,
                                     device_wifi);
out:
        gtk_widget_destroy (dialog);
}

static void
disconnect_button_clicked_cb (GtkButton *button, NetDeviceWifi *device_wifi)
{
        NMDevice *device;
        device = net_device_get_nm_device (NET_DEVICE (device_wifi));
        if (device == NULL)
                return;
        nm_device_disconnect (device, NULL, NULL);
}

static void activate_connection (NetDeviceWifi *device, const gchar *id);

static void
connect_button_clicked_cb (GtkButton *button, NetDeviceWifi *device_wifi)
{
        if (device_wifi->priv->selected_connection_id)
                activate_connection (device_wifi, device_wifi->priv->selected_connection_id);
}

static void
forget_button_clicked_cb (GtkButton *button, NetDeviceWifi *device_wifi)
{
        gchar *ssid_pretty = NULL;
        gchar *warning = NULL;
        GtkWidget *dialog;
        GtkWidget *window;
        CcNetworkPanel *panel;

        ssid_pretty = g_strdup_printf ("<b>%s</b>", device_wifi->priv->selected_ssid_title);
        warning = g_strdup_printf (_("Network details for %s including password and any custom configuration will be lost."), ssid_pretty);
        panel = net_object_get_panel (NET_OBJECT (device_wifi));
        window = gtk_widget_get_toplevel (GTK_WIDGET (panel));
        dialog = gtk_message_dialog_new (GTK_WINDOW (window),
                                         GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                         GTK_MESSAGE_OTHER,
                                         GTK_BUTTONS_NONE,
                                         NULL);
        gtk_message_dialog_set_markup (GTK_MESSAGE_DIALOG (dialog), warning);
        gtk_dialog_add_buttons (GTK_DIALOG (dialog),
                                GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                _("Forget"), GTK_RESPONSE_OK,
                                NULL);
        g_signal_connect (dialog, "response",
                          G_CALLBACK (forget_network_response_cb), device_wifi);
        gtk_window_present (GTK_WINDOW (dialog));

        g_free (ssid_pretty);
        g_free (warning);
}


static void
connect_to_hidden_network (NetDeviceWifi *device_wifi)
{
        NMRemoteSettings *remote_settings;
        NMClient *client;
        CcNetworkPanel *panel;

        remote_settings = net_object_get_remote_settings (NET_OBJECT (device_wifi));
        client = net_object_get_client (NET_OBJECT (device_wifi));
        panel = net_object_get_panel (NET_OBJECT (device_wifi));
        cc_network_panel_connect_to_hidden_network (panel, client, remote_settings);
}

static void
connection_add_activate_cb (NMClient *client,
                            NMActiveConnection *connection,
                            const char *path,
                            GError *error,
                            gpointer user_data)
{
        NetDeviceWifi *device_wifi = user_data;

        if (connection == NULL) {
                /* failed to activate */
                g_debug ("Failed to add and activate connection '%d': %s",
                         error->code,
                         error->message);
                nm_device_wifi_refresh_ui (device_wifi);
        }
}

static void
connection_activate_cb (NMClient *client,
                        NMActiveConnection *connection,
                        GError *error,
                        gpointer user_data)
{
        NetDeviceWifi *device_wifi = user_data;

        if (connection == NULL) {
                /* failed to activate */
                g_debug ("Failed to activate connection '%d': %s",
                         error->code,
                         error->message);
                nm_device_wifi_refresh_ui (device_wifi);
        }
}

static void
activate_connection (NetDeviceWifi *device_wifi,
                     const gchar   *connection_id)
{
        NMDevice *device;
        NMClient *client;
        NMRemoteSettings *settings;
        NMRemoteConnection *connection;

        device = net_device_get_nm_device (NET_DEVICE (device_wifi));
        client = net_object_get_client (NET_OBJECT (device_wifi));
        settings = net_object_get_remote_settings (NET_OBJECT (device_wifi));
        connection = nm_remote_settings_get_connection_by_path (settings, connection_id);
        nm_client_activate_connection (client,
                                       NM_CONNECTION (connection),
                                       device, NULL,
                                       connection_activate_cb, device_wifi);
}

static gboolean
is_8021x (NMDevice   *device,
          const char *ap_object_path)
{
        NM80211ApSecurityFlags wpa_flags, rsn_flags;
        NMAccessPoint *ap;

        ap = nm_device_wifi_get_access_point_by_path (NM_DEVICE_WIFI (device),
                                                      ap_object_path);
        if (!ap)
                return FALSE;

        rsn_flags = nm_access_point_get_rsn_flags (ap);
        if (rsn_flags & NM_802_11_AP_SEC_KEY_MGMT_802_1X)
                return TRUE;

        wpa_flags = nm_access_point_get_wpa_flags (ap);
        if (wpa_flags & NM_802_11_AP_SEC_KEY_MGMT_802_1X)
                return TRUE;
        return FALSE;
}

static void
wireless_try_to_connect (NetDeviceWifi *device_wifi,
                         const gchar *ssid_target,
                         const gchar *ap_object_path)
{
        const GByteArray *ssid;
        const gchar *ssid_tmp;
        GSList *list, *l;
        GSList *filtered;
        NMConnection *connection_activate = NULL;
        NMDevice *device;
        NMSettingWireless *setting_wireless;
        NMRemoteSettings *remote_settings;
        NMClient *client;

        if (device_wifi->priv->updating_device)
                goto out;

        if (ap_object_path == NULL || ap_object_path[0] == 0)
                goto out;

        device = net_device_get_nm_device (NET_DEVICE (device_wifi));
        if (device == NULL)
                goto out;

        g_debug ("try to connect to WIFI network %s [%s]",
                 ssid_target, ap_object_path);

        /* look for an existing connection we can use */
        remote_settings = net_object_get_remote_settings (NET_OBJECT (device_wifi));
        list = nm_remote_settings_list_connections (remote_settings);
        g_debug ("%i existing remote connections available", g_slist_length (list));
        filtered = nm_device_filter_connections (device, list);
        g_debug ("%i suitable remote connections to check", g_slist_length (filtered));
        for (l = filtered; l; l = g_slist_next (l)) {
                NMConnection *connection;

                connection = NM_CONNECTION (l->data);
                setting_wireless = nm_connection_get_setting_wireless (connection);
                if (!NM_IS_SETTING_WIRELESS (setting_wireless))
                        continue;
                ssid = nm_setting_wireless_get_ssid (setting_wireless);
                if (ssid == NULL)
                        continue;
                ssid_tmp = nm_utils_escape_ssid (ssid->data, ssid->len);
                if (g_strcmp0 (ssid_target, ssid_tmp) == 0) {
                        g_debug ("we found an existing connection %s to activate!",
                                 nm_connection_get_id (connection));
                        connection_activate = connection;
                        break;
                }
        }

        g_slist_free (list);
        g_slist_free (filtered);

        /* activate the connection */
        client = net_object_get_client (NET_OBJECT (device_wifi));
        if (connection_activate != NULL) {
                nm_client_activate_connection (client,
                                               connection_activate,
                                               device, NULL,
                                               connection_activate_cb, device_wifi);
                goto out;
        }

        /* create one, as it's missing */
        g_debug ("no existing connection found for %s, creating", ssid_target);

        if (!is_8021x (device, ap_object_path)) {
                g_debug ("no existing connection found for %s, creating and activating one", ssid_target);
                nm_client_add_and_activate_connection (client,
                                                       NULL,
                                                       device, ap_object_path,
                                                       connection_add_activate_cb, device_wifi);
        } else {
                CcNetworkPanel *panel;
                GPtrArray *array;

                g_debug ("no existing connection found for %s, creating", ssid_target);
                array = g_ptr_array_new ();
                g_ptr_array_add (array, "connect-8021x-wifi");
                g_ptr_array_add (array, (gpointer) nm_object_get_path (NM_OBJECT (device)));
                g_ptr_array_add (array, (gpointer) ap_object_path);
                g_ptr_array_add (array, NULL);

                panel = net_object_get_panel (NET_OBJECT (device_wifi));
                g_object_set (G_OBJECT (panel), "argv", array->pdata, NULL);

                g_ptr_array_free (array, FALSE);
        }
out:
        return;
}

static gint
wireless_ap_model_sort_cb (GtkTreeModel *model,
                           GtkTreeIter *a,
                           GtkTreeIter *b,
                           gpointer user_data)
{
        gboolean active_a;
        gboolean active_b;
        gboolean ap_a;
        gboolean ap_b;
        gchar *str_a;
        gchar *str_b;
        gint retval;
        gint strength_a;
        gint strength_b;

        gtk_tree_model_get (model, a,
                            COLUMN_SORT, &str_a,
                            COLUMN_STRENGTH, &strength_a,
                            COLUMN_ACTIVE, &active_a,
                            COLUMN_AP_IN_RANGE, &ap_a,
                            -1);
        gtk_tree_model_get (model, b,
                            COLUMN_SORT, &str_b,
                            COLUMN_STRENGTH, &strength_b,
                            COLUMN_ACTIVE, &active_b,
                            COLUMN_AP_IN_RANGE, &ap_b,
                            -1);

        /* active entry first */
        if (active_a) {
                retval = -1;
                goto out;
        }

        if (active_b) {
                retval = 1;
                goto out;
        }

        /* aps before connections */
        if (ap_a && !ap_b) {
                retval = -1;
                goto out;
        }
        if (!ap_a && ap_b) {
                retval = 1;
                goto out;
        }

        /* case sensitive search like before */
        retval = strength_b - strength_a;
out:
        g_free (str_a);
        g_free (str_b);

        return retval;
}

static GByteArray *
ssid_to_byte_array (const gchar *ssid)
{
        guint32 len;
        GByteArray *ba;

        len = strlen (ssid);
        ba = g_byte_array_sized_new (len);
        g_byte_array_append (ba, (guchar *)ssid, len);

        return ba;
}

static gchar *
get_hostname (void)
{
        GDBusConnection *bus;
        GVariant *res;
        GVariant *inner;
        gchar *str;
        GError *error;

        error = NULL;
        bus = g_bus_get_sync (G_BUS_TYPE_SYSTEM, NULL, &error);
        if (error != NULL) {
                g_warning ("Failed to get system bus connection: %s", error->message);
                g_error_free (error);

                return NULL;
        }
        res = g_dbus_connection_call_sync (bus,
                                           "org.freedesktop.hostname1",
                                           "/org/freedesktop/hostname1",
                                           "org.freedesktop.DBus.Properties",
                                           "Get",
                                           g_variant_new ("(ss)",
                                                          "org.freedesktop.hostname1",
                                                          "PrettyHostname"),
                                           (GVariantType*)"(v)",
                                           G_DBUS_CALL_FLAGS_NONE,
                                           -1,
                                           NULL,
                                           &error);
        g_object_unref (bus);

        if (error != NULL) {
                g_warning ("Getting pretty hostname failed: %s", error->message);
                g_error_free (error);
        }

        str = NULL;

        if (res != NULL) {
                g_variant_get (res, "(v)", &inner);
                str = g_variant_dup_string (inner, NULL);
                g_variant_unref (res);
        }

        if (str == NULL || *str == '\0') {
                str = g_strdup (g_get_host_name ());
        }

        if (str == NULL || *str == '\0') {
                str = g_strdup ("GNOME");
	}

        return str;
}

static GByteArray *
generate_ssid_for_hotspot (NetDeviceWifi *device_wifi)
{
        GByteArray *ssid_array;
        gchar *ssid;

        ssid = get_hostname ();
        ssid_array = ssid_to_byte_array (ssid);
        g_free (ssid);

        return ssid_array;
}

static gchar *
generate_wep_key (NetDeviceWifi *device_wifi)
{
        gchar key[11];
        gint i;
        const gchar *hexdigits = "0123456789abcdef";

        /* generate a 10-digit hex WEP key */
        for (i = 0; i < 10; i++) {
                gint digit;
                digit = g_random_int_range (0, 16);
                key[i] = hexdigits[digit];
        }
        key[10] = 0;

        return g_strdup (key);
}

static gboolean
is_hotspot_connection (NMConnection *connection)
{
        NMSettingConnection *sc;
        NMSettingWireless *sw;
        NMSettingIP4Config *sip;

        sc = nm_connection_get_setting_connection (connection);
        if (g_strcmp0 (nm_setting_connection_get_connection_type (sc), "802-11-wireless") != 0) {
                return FALSE;
        }
        sw = nm_connection_get_setting_wireless (connection);
        if (g_strcmp0 (nm_setting_wireless_get_mode (sw), "adhoc") != 0) {
                return FALSE;
        }
        if (g_strcmp0 (nm_setting_wireless_get_security (sw), "802-11-wireless-security") != 0) {
                return FALSE;
        }
        sip = nm_connection_get_setting_ip4_config (connection);
        if (g_strcmp0 (nm_setting_ip4_config_get_method (sip), "shared") != 0) {
                return FALSE;
        }

        return TRUE;
}

static void
show_hotspot_ui (NetDeviceWifi *device_wifi)
{
        GtkWidget *widget;
        GtkSwitch *sw;

        /* show hotspot tab */
        widget = GTK_WIDGET (gtk_builder_get_object (device_wifi->priv->builder, "notebook_view"));
        gtk_notebook_set_current_page (GTK_NOTEBOOK (widget), 3);

        /* force switch to on as this succeeded */
        sw = GTK_SWITCH (gtk_builder_get_object (device_wifi->priv->builder,
                                                 "switch_hotspot_off"));
        device_wifi->priv->updating_device = TRUE;
        gtk_switch_set_active (sw, TRUE);
        device_wifi->priv->updating_device = FALSE;
}

static void
activate_cb (NMClient           *client,
             NMActiveConnection *connection,
             GError             *error,
             NetDeviceWifi     *device_wifi)
{
        if (error != NULL) {
                g_warning ("Failed to add new connection: (%d) %s",
                           error->code,
                           error->message);
                return;
        }

        /* show hotspot tab */
        nm_device_wifi_refresh_ui (device_wifi);
        show_hotspot_ui (device_wifi);
}

static void
activate_new_cb (NMClient           *client,
                 NMActiveConnection *connection,
                 const gchar        *path,
                 GError             *error,
                 NetDeviceWifi     *device_wifi)
{
        activate_cb (client, connection, error, device_wifi);
}

static void
start_shared_connection (NetDeviceWifi *device_wifi)
{
        NMConnection *c;
        NMConnection *tmp;
        NMSettingConnection *sc;
        NMSettingWireless *sw;
        NMSettingIP4Config *sip;
        NMSettingWirelessSecurity *sws;
        NMDevice *device;
        GByteArray *ssid_array;
        gchar *wep_key;
        const gchar *str_mac;
        struct ether_addr *bin_mac;
        GSList *connections;
        GSList *filtered;
        GSList *l;
        NMClient *client;
        NMRemoteSettings *remote_settings;

        device = net_device_get_nm_device (NET_DEVICE (device_wifi));
        g_assert (nm_device_get_device_type (device) == NM_DEVICE_TYPE_WIFI);

        remote_settings = net_object_get_remote_settings (NET_OBJECT (device_wifi));
        connections = nm_remote_settings_list_connections (remote_settings);
        filtered = nm_device_filter_connections (device, connections);
        g_slist_free (connections);
        c = NULL;
        for (l = filtered; l; l = l->next) {
                tmp = l->data;
                if (is_hotspot_connection (tmp)) {
                        c = tmp;
                        break;
                }
        }
        g_slist_free (filtered);

        client = net_object_get_client (NET_OBJECT (device_wifi));
        if (c != NULL) {
                g_debug ("activate existing hotspot connection\n");
                nm_client_activate_connection (client,
                                               c,
                                               device,
                                               NULL,
                                               (NMClientActivateFn)activate_cb,
                                               device_wifi);
                return;
        }

        g_debug ("create new hotspot connection\n");
        c = nm_connection_new ();

        sc = (NMSettingConnection *)nm_setting_connection_new ();
        g_object_set (sc,
                      "type", "802-11-wireless",
                      "id", "Hotspot",
                      "autoconnect", FALSE,
                      NULL);
        nm_connection_add_setting (c, (NMSetting *)sc);

        sw = (NMSettingWireless *)nm_setting_wireless_new ();
        g_object_set (sw,
                      "mode", "adhoc",
                      "security", "802-11-wireless-security",
                      NULL);

        str_mac = nm_device_wifi_get_permanent_hw_address (NM_DEVICE_WIFI (device));
        bin_mac = ether_aton (str_mac);
        if (bin_mac) {
                GByteArray *hw_address;

                hw_address = g_byte_array_sized_new (ETH_ALEN);
                g_byte_array_append (hw_address, bin_mac->ether_addr_octet, ETH_ALEN);
                g_object_set (sw,
                              "mac-address", hw_address,
                              NULL);
                g_byte_array_unref (hw_address);
        }
        nm_connection_add_setting (c, (NMSetting *)sw);

        sip = (NMSettingIP4Config*) nm_setting_ip4_config_new ();
        g_object_set (sip, "method", "shared", NULL);
        nm_connection_add_setting (c, (NMSetting *)sip);

        ssid_array = generate_ssid_for_hotspot (device_wifi);
        g_object_set (sw,
                      "ssid", ssid_array,
                      NULL);
        g_byte_array_unref (ssid_array);

        sws = (NMSettingWirelessSecurity*) nm_setting_wireless_security_new ();
        wep_key = generate_wep_key (device_wifi);
        g_object_set (sws,
                      "key-mgmt", "none",
                      "wep-key0", wep_key,
                      "wep-key-type", NM_WEP_KEY_TYPE_KEY,
                      NULL);
        g_free (wep_key);
        nm_connection_add_setting (c, (NMSetting *)sws);

        nm_client_add_and_activate_connection (client,
                                               c,
                                               device,
                                               NULL,
                                               (NMClientAddActivateFn)activate_new_cb,
                                               device_wifi);

        g_object_unref (c);
}

static void
start_hotspot_response_cb (GtkWidget *dialog, gint response, NetDeviceWifi *device_wifi)
{
        if (response == GTK_RESPONSE_OK) {
                start_shared_connection (device_wifi);
        }
        gtk_widget_hide (dialog);
}

static void
start_hotspot (GtkButton *button, NetDeviceWifi *device_wifi)
{
        NMDevice *device;
        const GPtrArray *connections;
        gchar *active_ssid;
        NMClient *client;
        GtkWidget *dialog;
        GtkWidget *window;
        GtkWidget *widget;
        GString *str;

        active_ssid = NULL;

        client = net_object_get_client (NET_OBJECT (device_wifi));
        device = net_device_get_nm_device (NET_DEVICE (device_wifi));
        connections = nm_client_get_active_connections (client);
        if (connections) {
                gint i;
                for (i = 0; i < connections->len; i++) {
                        NMActiveConnection *c;
                        const GPtrArray *devices;
                        c = (NMActiveConnection *)connections->pdata[i];
                        devices = nm_active_connection_get_devices (c);
                        if (devices && devices->pdata[0] == device) {
                                NMAccessPoint *ap;
                                ap = nm_device_wifi_get_active_access_point (NM_DEVICE_WIFI (device));
                                active_ssid = nm_utils_ssid_to_utf8 (nm_access_point_get_ssid (ap));
                                break;
                        }
                }
        }

        window = gtk_widget_get_toplevel (GTK_WIDGET (button));

        dialog = GTK_WIDGET (gtk_builder_get_object (device_wifi->priv->builder, "hotspot-dialog"));
        gtk_window_set_transient_for (GTK_WINDOW (dialog), GTK_WINDOW (window));

        str = g_string_new (_("If you have a connection to the Internet other than wireless, you can use it to share your internet connection with others."));
        g_string_append (str, "\n\n");

        if (active_ssid) {
                g_string_append_printf (str, _("Switching on the wireless hotspot will disconnect you from <b>%s</b>."), active_ssid);
                g_string_append (str, " ");
        }

        g_string_append (str, _("It is not possible to access the internet through your wireless while the hotspot is active."));

        widget = GTK_WIDGET (gtk_builder_get_object (device_wifi->priv->builder, "hotspot-dialog-content"));
        gtk_label_set_markup (GTK_LABEL (widget), str->str);
        g_string_free (str, TRUE);

        g_signal_connect (dialog, "response",
                          G_CALLBACK (start_hotspot_response_cb), device_wifi);
        gtk_window_present (GTK_WINDOW (dialog));
        g_free (active_ssid);
}

static void
stop_shared_connection (NetDeviceWifi *device_wifi)
{
        const GPtrArray *connections;
        const GPtrArray *devices;
        NMDevice *device;
        gint i;
        NMActiveConnection *c;
        NMClient *client;

        device = net_device_get_nm_device (NET_DEVICE (device_wifi));
        client = net_object_get_client (NET_OBJECT (device_wifi));
        connections = nm_client_get_active_connections (client);
        for (i = 0; i < connections->len; i++) {
                c = (NMActiveConnection *)connections->pdata[i];

                devices = nm_active_connection_get_devices (c);
                if (devices && devices->pdata[0] == device) {
                        nm_client_deactivate_connection (client, c);
                        break;
                }
        }

        nm_device_wifi_refresh_ui (device_wifi);
        show_wifi_list (device_wifi);
}

static void
stop_hotspot_response_cb (GtkWidget *dialog, gint response, NetDeviceWifi *device_wifi)
{
        if (response == GTK_RESPONSE_OK) {
                stop_shared_connection (device_wifi);
        }
        gtk_widget_destroy (dialog);
}

static void
switch_hotspot_changed_cb (GtkSwitch *sw,
                           GParamSpec *pspec,
                           NetDeviceWifi *device_wifi)
{
        GtkWidget *dialog;
        GtkWidget *window;
        CcNetworkPanel *panel;

        if (device_wifi->priv->updating_device)
                return;

        panel = net_object_get_panel (NET_OBJECT (device_wifi));
        window = gtk_widget_get_toplevel (GTK_WIDGET (panel));
        dialog = gtk_message_dialog_new (GTK_WINDOW (window),
                                         GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                         GTK_MESSAGE_OTHER,
                                         GTK_BUTTONS_NONE,
                                         _("Stop hotspot and disconnect any users?"));
        gtk_dialog_add_buttons (GTK_DIALOG (dialog),
                                GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                _("_Stop Hotspot"), GTK_RESPONSE_OK,
                                NULL);
        g_signal_connect (dialog, "response",
                          G_CALLBACK (stop_hotspot_response_cb), device_wifi);
        gtk_window_present (GTK_WINDOW (dialog));
}

static void
connect_wifi_network (NetDeviceWifi *device_wifi,
                      GtkTreeView *tv,
                      GtkTreePath *path)
{
        gboolean ap_in_range;
        gchar *ap_object_path;
        gchar *ssid;
        gchar *connection_id;
        GtkTreeIter iter;
        GtkTreeModel *model;
        NM80211Mode mode;

        model = gtk_tree_view_get_model (tv);
        gtk_tree_model_get_iter (model, &iter, path);

        gtk_tree_model_get (model, &iter,
                            COLUMN_ACCESS_POINT_ID, &ap_object_path,
                            COLUMN_CONNECTION_ID, &connection_id,
                            COLUMN_TITLE, &ssid,
                            COLUMN_AP_IN_RANGE, &ap_in_range,
                            COLUMN_MODE, &mode,
                            -1);
        if (ap_in_range) {
                if (connection_id)
                        activate_connection (device_wifi, connection_id);
                else
                        wireless_try_to_connect (device_wifi, ssid, ap_object_path);
        } else {
                g_warning ("can't connect");
        }

        g_free (ap_object_path);
        g_free (connection_id);
        g_free (ssid);
}

static void
show_wifi_details (NetDeviceWifi *device_wifi,
                   GtkTreeView *tv,
                   GtkTreePath *path)
{
        GtkWidget *widget;
        gboolean ret;
        gboolean in_range;
        GtkTreeModel *model;
        GtkTreeIter iter;
        gchar *path_str;

        model = gtk_tree_view_get_model (tv);
        path_str = gtk_tree_path_to_string (path);
        ret = gtk_tree_model_get_iter_from_string (model, &iter, path_str);
        if (!ret)
                goto out;

        /* get parameters about the selected connection */
        g_free (device_wifi->priv->selected_connection_id);
        g_free (device_wifi->priv->selected_ssid_title);
        gtk_tree_model_get (model, &iter,
                            COLUMN_ACCESS_POINT_ID, &device_wifi->priv->selected_ap_id,
                            COLUMN_CONNECTION_ID, &device_wifi->priv->selected_connection_id,
                            COLUMN_TITLE, &device_wifi->priv->selected_ssid_title,
                            COLUMN_AP_IN_RANGE, &in_range,
                            -1);
        g_debug ("ssid = %s, in-range = %i",
                 device_wifi->priv->selected_ssid_title, in_range);

        widget = GTK_WIDGET (gtk_builder_get_object (device_wifi->priv->builder, "notebook_view"));

        nm_device_wifi_refresh_ui (device_wifi);
        gtk_notebook_set_current_page (GTK_NOTEBOOK (widget), 1);

out:
        g_free (path_str);
}

static void
show_wifi_list (NetDeviceWifi *device_wifi)
{
        GtkWidget *widget;
        widget = GTK_WIDGET (gtk_builder_get_object (device_wifi->priv->builder, "notebook_view"));
        gtk_notebook_set_current_page (GTK_NOTEBOOK (widget), 0);
}

static gboolean
arrow_visible (GtkTreeModel *model,
               GtkTreeIter  *iter)
{
        gboolean active;
        gboolean ap_is_saved;
        gboolean ret;
        gchar *sort;

        gtk_tree_model_get (model, iter,
                            COLUMN_ACTIVE, &active,
                            COLUMN_AP_IS_SAVED, &ap_is_saved,
                            COLUMN_SORT, &sort,
                            -1);

        if (active || ap_is_saved)
                ret = TRUE;
        else
                ret = FALSE;

        g_free (sort);

        return ret;
}

static void
set_arrow_image (GtkCellLayout   *layout,
                 GtkCellRenderer *cell,
                 GtkTreeModel    *model,
                 GtkTreeIter     *iter,
                 gpointer         user_data)
{
        NetDeviceWifi *device = user_data;
        const gchar *icon;

        if (arrow_visible (model, iter)) {
                GtkWidget *widget;

                widget = GTK_WIDGET (gtk_builder_get_object (device->priv->builder,
                                                             "treeview_list"));

                if (gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL)
                        icon = "go-previous";
                else
                        icon = "go-next";
        }
        else {
                icon = "";
        }

        g_object_set (cell, "icon-name", icon, NULL);
}

static void
edit_connection (GtkButton *button, NetDeviceWifi *device_wifi)
{
        net_object_edit (NET_OBJECT (device_wifi));
}

static void
remote_settings_read_cb (NMRemoteSettings *remote_settings,
                         NetDeviceWifi *device_wifi)
{
        gboolean is_hotspot;

        device_wifi_refresh_saved_connections (device_wifi);

        /* go straight to the hotspot UI */
        is_hotspot = device_is_hotspot (device_wifi);
        if (is_hotspot) {
                nm_device_wifi_refresh_hotspot (device_wifi);
                show_hotspot_ui (device_wifi);
        }
}

static gboolean
separator_visible (GtkTreeModel *model,
                   GtkTreeIter  *iter)
{
        gboolean active;
        gboolean ap_is_saved;
        gboolean ap_in_range;
        gchar *sort;
        gboolean ret;

        gtk_tree_model_get (model, iter,
                            COLUMN_ACTIVE, &active,
                            COLUMN_AP_IS_SAVED, &ap_is_saved,
                            COLUMN_AP_IN_RANGE, &ap_in_range,
                            COLUMN_SORT, &sort,
                            -1);

        if (!active && ap_is_saved && ap_in_range)
                ret = TRUE;
        else
                ret = FALSE;

        g_free (sort);

        return ret;

}

static void
set_draw_separator (GtkCellLayout   *layout,
                    GtkCellRenderer *cell,
                    GtkTreeModel    *model,
                    GtkTreeIter     *iter,
                    gpointer         user_data)
{
        gboolean draw;

        draw = separator_visible (model, iter);

        g_object_set (cell, "draw", draw, NULL);
}

static void
switch_page_cb (GtkNotebook   *notebook,
                GtkWidget     *page,
                guint          page_num,
                NetDeviceWifi *device_wifi)
{
        GtkWidget *widget;

        if (page_num == 1) {
                widget = GTK_WIDGET (gtk_builder_get_object (device_wifi->priv->builder,
                                                             "button_back1"));
                gtk_widget_grab_focus (widget);
        }
}

static void
net_device_wifi_constructed (GObject *object)
{
        NetDeviceWifi *device_wifi = NET_DEVICE_WIFI (object);
        NMClient *client;
        NMRemoteSettings *remote_settings;
        NMClientPermissionResult perm;
        GtkWidget *widget;

        G_OBJECT_CLASS (net_device_wifi_parent_class)->constructed (object);

        client = net_object_get_client (NET_OBJECT (device_wifi));
        g_signal_connect (client, "notify::wireless-enabled",
                          G_CALLBACK (wireless_enabled_toggled), device_wifi);

        /* only show the button if the user can create a hotspot */
        perm = nm_client_get_permission_result (client, NM_CLIENT_PERMISSION_WIFI_SHARE_OPEN);
        widget = GTK_WIDGET (gtk_builder_get_object (device_wifi->priv->builder,
                                                     "start_hotspot_button"));
        gtk_widget_set_sensitive (widget, perm == NM_CLIENT_PERMISSION_RESULT_YES ||
                                          perm == NM_CLIENT_PERMISSION_RESULT_AUTH);

        remote_settings = net_object_get_remote_settings (NET_OBJECT (device_wifi));
        g_signal_connect (remote_settings, "connections-read",
                          G_CALLBACK (remote_settings_read_cb), device_wifi);

        nm_device_wifi_refresh_ui (device_wifi);
}

static void
net_device_wifi_finalize (GObject *object)
{
        NetDeviceWifi *device_wifi = NET_DEVICE_WIFI (object);
        NetDeviceWifiPrivate *priv = device_wifi->priv;

        g_object_unref (priv->builder);
        g_free (priv->selected_ssid_title);
        g_free (priv->selected_connection_id);
        g_free (priv->selected_ap_id);

        G_OBJECT_CLASS (net_device_wifi_parent_class)->finalize (object);
}

static void
device_wifi_edit (NetObject *object)
{
        const gchar *uuid;
        gchar *cmdline;
        GError *error = NULL;
        NetDeviceWifi *device = NET_DEVICE_WIFI (object);
        NMRemoteSettings *settings;
        NMRemoteConnection *connection;

        settings = net_object_get_remote_settings (object);
        connection = nm_remote_settings_get_connection_by_path (settings, device->priv->selected_connection_id);
        if (connection == NULL) {
                g_warning ("failed to get remote connection");
                return;
        }
        uuid = nm_connection_get_uuid (NM_CONNECTION (connection));
        cmdline = g_strdup_printf ("nm-connection-editor --edit %s", uuid);
        g_debug ("Launching '%s'\n", cmdline);
        if (!g_spawn_command_line_async (cmdline, &error)) {
                g_warning ("Failed to launch nm-connection-editor: %s", error->message);
                g_error_free (error);
        }
        g_free (cmdline);
}

static void
net_device_wifi_class_init (NetDeviceWifiClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);
        NetObjectClass *parent_class = NET_OBJECT_CLASS (klass);

        object_class->finalize = net_device_wifi_finalize;
        object_class->constructed = net_device_wifi_constructed;
        parent_class->add_to_notebook = device_wifi_proxy_add_to_notebook;
        parent_class->refresh = device_wifi_refresh;
        parent_class->edit = device_wifi_edit;

        g_type_class_add_private (klass, sizeof (NetDeviceWifiPrivate));
}

static void
activate_ssid_cb (PanelCellRendererText *cell,
                  const gchar           *path,
                  NetDeviceWifi         *device_wifi)
{
        GtkTreeView *tv;
        GtkTreePath *tpath;

        g_debug ("activate ssid!\n");

        tv = GTK_TREE_VIEW (gtk_builder_get_object (device_wifi->priv->builder,
                                                    "treeview_list"));
        tpath = gtk_tree_path_new_from_string (path);

        connect_wifi_network (device_wifi, tv, tpath);

        gtk_tree_path_free (tpath);
}

static void
activate_arrow_cb (PanelCellRendererText *cell,
                  const gchar           *path,
                  NetDeviceWifi         *device_wifi)
{
        GtkTreeView *tv;
        GtkTreeModel *model;
        GtkTreePath *tpath;
        GtkTreeIter iter;

        g_debug ("activate arrow!\n");

        tv = GTK_TREE_VIEW (gtk_builder_get_object (device_wifi->priv->builder,
                                                    "treeview_list"));
        model = gtk_tree_view_get_model (tv);
        tpath = gtk_tree_path_new_from_string (path);
        gtk_tree_model_get_iter (model, &iter, tpath);

        if (arrow_visible (model, &iter))
                show_wifi_details (device_wifi, tv, tpath);
        gtk_tree_path_free (tpath);
}

static void
net_device_wifi_init (NetDeviceWifi *device_wifi)
{
        GError *error = NULL;
        GtkWidget *widget;
        GtkCellRenderer *renderer1;
        GtkCellRenderer *renderer2;
        GtkCellRenderer *renderer3;
        GtkCellRenderer *renderer4;
        GtkCellRenderer *renderer5;
        GtkCellRenderer *renderer6;
        GtkCellRenderer *renderer7;
        GtkCellRenderer *renderer8;
        GtkTreeSortable *sortable;
        GtkTreeViewColumn *column;
        GtkCellArea *area;

        device_wifi->priv = NET_DEVICE_WIFI_GET_PRIVATE (device_wifi);

        device_wifi->priv->builder = gtk_builder_new ();
        gtk_builder_add_from_file (device_wifi->priv->builder,
                                   GNOMECC_UI_DIR "/network-wifi.ui",
                                   &error);
        if (error != NULL) {
                g_warning ("Could not load interface file: %s", error->message);
                g_error_free (error);
                return;
        }

        /* setup wifi views */
        widget = GTK_WIDGET (gtk_builder_get_object (device_wifi->priv->builder,
                                                     "device_off_switch"));
        g_signal_connect (widget, "notify::active",
                          G_CALLBACK (device_off_toggled), device_wifi);

        widget = GTK_WIDGET (gtk_builder_get_object (device_wifi->priv->builder,
                                                     "button_options1"));
        g_signal_connect (widget, "clicked",
                          G_CALLBACK (edit_connection), device_wifi);

        widget = GTK_WIDGET (gtk_builder_get_object (device_wifi->priv->builder,
                                                     "button_forget1"));
        g_signal_connect (widget, "clicked",
                          G_CALLBACK (forget_button_clicked_cb), device_wifi);

        widget = GTK_WIDGET (gtk_builder_get_object (device_wifi->priv->builder,
                                                     "button_disconnect1"));
        g_signal_connect (widget, "clicked",
                          G_CALLBACK (disconnect_button_clicked_cb), device_wifi);
        widget = GTK_WIDGET (gtk_builder_get_object (device_wifi->priv->builder,
                                                     "button_connect1"));
        g_signal_connect (widget, "clicked",
                          G_CALLBACK (connect_button_clicked_cb), device_wifi);

        widget = GTK_WIDGET (gtk_builder_get_object (device_wifi->priv->builder,
                                                     "treeview_list"));

        /* sort networks in drop down */
        sortable = GTK_TREE_SORTABLE (gtk_builder_get_object (device_wifi->priv->builder,
                                                              "liststore_network"));
        gtk_tree_sortable_set_sort_column_id (sortable,
                                              COLUMN_SORT,
                                              GTK_SORT_ASCENDING);
        gtk_tree_sortable_set_sort_func (sortable,
                                         COLUMN_SORT,
                                         wireless_ap_model_sort_cb,
                                         device_wifi,
                                         NULL);


        column = GTK_TREE_VIEW_COLUMN (gtk_builder_get_object (device_wifi->priv->builder,
                                                               "treeview_list_column"));
        area = gtk_cell_layout_get_area (GTK_CELL_LAYOUT (column));

        renderer1 = gtk_cell_renderer_pixbuf_new ();
        gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (column), renderer1, FALSE);
        g_object_set (renderer1,
                      "follow-state", TRUE,
                      "icon-name", "object-select-symbolic",
                      "xpad", 6,
                      "ypad", 6,
                      NULL);
        gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (column), renderer1,
                                        "visible", COLUMN_ACTIVE,
                                        NULL);
        gtk_cell_area_cell_set (area, renderer1, "align", TRUE, NULL);

        renderer2 = panel_cell_renderer_text_new ();
        g_object_set (renderer2,
                      "mode", GTK_CELL_RENDERER_MODE_ACTIVATABLE,
                      "ellipsize", PANGO_ELLIPSIZE_END,
                      NULL);
        gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (column), renderer2, TRUE);
        gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (column), renderer2,
                                        "markup", COLUMN_TITLE,
                                        NULL);
        gtk_cell_area_cell_set (area, renderer2,
                                "align", TRUE,
                                "expand", TRUE,
                                NULL);
        g_signal_connect (renderer2, "activate",
                          G_CALLBACK (activate_ssid_cb), device_wifi);

        renderer3 = panel_cell_renderer_mode_new ();
        gtk_cell_renderer_set_padding (renderer3, 4, 0);
        gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (column),
                                    renderer3,
                                    FALSE);
        g_object_set (renderer3, "follow-state", TRUE, NULL);
        gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (column), renderer3,
                                        "ap-mode", COLUMN_MODE,
                                        NULL);

        renderer4 = gtk_cell_renderer_text_new ();
        gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (column), renderer4, FALSE);
        gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (column), renderer4,
                                        "visible", COLUMN_AP_OUT_OF_RANGE,
                                        NULL);
        g_object_set (renderer4,
                      "text", _("Out of range"),
                      "mode", GTK_CELL_RENDERER_MODE_INERT,
                      "xalign", 1.0,
                      NULL);

        renderer5 = panel_cell_renderer_signal_new ();
        gtk_cell_renderer_set_padding (renderer5, 4, 0);
        gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (column),
                                    renderer5,
                                    FALSE);
        g_object_set (renderer5, "follow-state", TRUE, NULL);
        gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (column), renderer5,
                                        "signal", COLUMN_STRENGTH,
                                        "visible", COLUMN_AP_IN_RANGE,
                                        NULL);

        renderer6 = panel_cell_renderer_security_new ();
        gtk_cell_renderer_set_padding (renderer6, 4, 0);
        gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (column),
                                    renderer6,
                                    FALSE);
        g_object_set (renderer6, "follow-state", TRUE, NULL);
        gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (column), renderer6,
                                        "security", COLUMN_SECURITY,
                                        "visible", COLUMN_AP_IN_RANGE,
                                        NULL);

        renderer7 = panel_cell_renderer_separator_new ();
        gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (column), renderer7, FALSE);
        g_object_set (renderer7,
                      "visible", TRUE,
                      "sensitive", FALSE,
                      "draw", TRUE,
                      NULL);
        gtk_cell_renderer_set_fixed_size (renderer7, 1, -1);
        gtk_cell_layout_set_cell_data_func (GTK_CELL_LAYOUT (column), renderer7,
                                            set_draw_separator, device_wifi, NULL);

        renderer8 = panel_cell_renderer_pixbuf_new ();
        gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (column), renderer8, FALSE);
        g_object_set (renderer8,
                      "mode", GTK_CELL_RENDERER_MODE_ACTIVATABLE,
                      "follow-state", TRUE,
                      "visible", TRUE,
                      "xpad", 6,
                      "ypad", 6,
                      NULL);
        gtk_cell_layout_set_cell_data_func (GTK_CELL_LAYOUT (column), renderer8,
                                            set_arrow_image, device_wifi, NULL);
        g_signal_connect (renderer8, "activate",
                          G_CALLBACK (activate_arrow_cb), device_wifi);

        widget = GTK_WIDGET (gtk_builder_get_object (device_wifi->priv->builder,
                                                     "button_back1"));
        g_signal_connect_swapped (widget, "clicked",
                                  G_CALLBACK (show_wifi_list), device_wifi);

        /* draw focus around everything but the arrow */
        gtk_cell_area_add_focus_sibling (area, renderer2, renderer1);
        gtk_cell_area_add_focus_sibling (area, renderer2, renderer3);
        gtk_cell_area_add_focus_sibling (area, renderer2, renderer4);
        gtk_cell_area_add_focus_sibling (area, renderer2, renderer5);
        gtk_cell_area_add_focus_sibling (area, renderer2, renderer6);
        gtk_cell_area_add_focus_sibling (area, renderer2, renderer7);

        /* setup view */
        widget = GTK_WIDGET (gtk_builder_get_object (device_wifi->priv->builder,
                                                     "notebook_view"));
        gtk_notebook_set_show_tabs (GTK_NOTEBOOK (widget), FALSE);
        gtk_notebook_set_current_page (GTK_NOTEBOOK (widget), 0);
        g_signal_connect_after (widget, "switch-page",
                                G_CALLBACK (switch_page_cb), device_wifi);

        widget = GTK_WIDGET (gtk_builder_get_object (device_wifi->priv->builder,
                                                     "start_hotspot_button"));
        g_signal_connect (widget, "clicked",
                          G_CALLBACK (start_hotspot), device_wifi);

        widget = GTK_WIDGET (gtk_builder_get_object (device_wifi->priv->builder,
                                                     "connect_hidden_button"));
        g_signal_connect_swapped (widget, "clicked",
                                  G_CALLBACK (connect_to_hidden_network), device_wifi);

        widget = GTK_WIDGET (gtk_builder_get_object (device_wifi->priv->builder,
                                                     "switch_hotspot_off"));
        g_signal_connect (widget, "notify::active",
                          G_CALLBACK (switch_hotspot_changed_cb), device_wifi);
}
