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

#include <netinet/ether.h>

#include <NetworkManager.h>
#include <polkit/polkit.h>

#include "list-box-helper.h"
#include "hostname-helper.h"
#include "network-dialogs.h"
#include "panel-common.h"

#include "connection-editor/net-connection-editor.h"
#include "net-device-wifi.h"

#include "cc-wifi-connection-list.h"
#include "cc-wifi-connection-row.h"

#define PERIODIC_WIFI_SCAN_TIMEOUT 15

static void nm_device_wifi_refresh_ui (NetDeviceWifi *device_wifi);
static void show_wifi_list (NetDeviceWifi *device_wifi);
static void show_hotspot_ui (NetDeviceWifi *device_wifi);
static void ap_activated (GtkListBox *list, GtkListBoxRow *row, NetDeviceWifi *device_wifi);
static gint ap_sort (gconstpointer a, gconstpointer b, gpointer data);
static void show_details_for_row (NetDeviceWifi *device_wifi, CcWifiConnectionRow *row, CcWifiConnectionList *list );


struct _NetDeviceWifi
{
        NetDevice                parent;

        GtkBuilder              *builder;
        GtkWidget               *details_dialog;
        GtkSwitch               *hotspot_switch;
        gboolean                 updating_device;
        gchar                   *selected_ssid_title;
        gchar                   *selected_connection_id;
        gchar                   *selected_ap_id;
        guint                    scan_id;
        GCancellable            *cancellable;
};

G_DEFINE_TYPE (NetDeviceWifi, net_device_wifi, NET_TYPE_DEVICE)

GtkWidget *
net_device_wifi_get_header_widget (NetDeviceWifi *device_wifi)
{
        return GTK_WIDGET (gtk_builder_get_object (device_wifi->builder, "header_box"));
}

GtkWidget *
net_device_wifi_get_title_widget (NetDeviceWifi *device_wifi)
{
        return GTK_WIDGET (gtk_builder_get_object (device_wifi->builder, "center_box"));
}

static GtkWidget *
device_wifi_proxy_add_to_stack (NetObject    *object,
                                GtkStack     *stack,
                                GtkSizeGroup *heading_size_group)
{
        NMDevice *nmdevice;
        GtkWidget *widget;
        NetDeviceWifi *device_wifi = NET_DEVICE_WIFI (object);

        nmdevice = net_device_get_nm_device (NET_DEVICE (object));

        /* add widgets to size group */
        widget = GTK_WIDGET (gtk_builder_get_object (device_wifi->builder,
                                                     "heading_ipv4"));
        gtk_size_group_add_widget (heading_size_group, widget);

        widget = GTK_WIDGET (gtk_builder_get_object (device_wifi->builder,
                                                     "notebook_view"));
        gtk_stack_add_titled (stack, widget,
                              net_object_get_id (object),
                              nm_device_get_description (nmdevice));

        return widget;
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
disable_scan_timeout (NetDeviceWifi *device_wifi)
{
        g_debug ("Disabling periodic Wi-Fi scan");
        if (device_wifi->scan_id > 0) {
                g_source_remove (device_wifi->scan_id);
                device_wifi->scan_id = 0;
        }
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
        sw = GTK_SWITCH (gtk_builder_get_object (device_wifi->builder,
                                                 "device_off_switch"));

        device_wifi->updating_device = TRUE;
        gtk_switch_set_active (sw, enabled);
        if (!enabled)
                disable_scan_timeout (device_wifi);
        device_wifi->updating_device = FALSE;
}

static NMConnection *
find_connection_for_device (NetDeviceWifi *device_wifi,
                            NMDevice       *device)
{
        NetDevice *tmp;
        NMConnection *connection;
        NMClient *client;

        client = net_object_get_client (NET_OBJECT (device_wifi));
        tmp = g_object_new (NET_TYPE_DEVICE,
                            "client", client,
                            "nm-device", device,
                            NULL);
        connection = net_device_get_find_connection (tmp);
        g_object_unref (tmp);
        return connection;
}

static gboolean
connection_is_shared (NMConnection *c)
{
        NMSettingIPConfig *s_ip4;

        s_ip4 = nm_connection_get_setting_ip4_config (c);
        if (g_strcmp0 (nm_setting_ip_config_get_method (s_ip4),
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
        if (nm_device_get_active_connection (device) == NULL)
                return FALSE;

        c = find_connection_for_device (device_wifi, device);
        if (c == NULL)
                return FALSE;

        return connection_is_shared (c);
}

static GBytes *
device_get_hotspot_ssid (NetDeviceWifi *device_wifi,
                         NMDevice *device)
{
        NMConnection *c;
        NMSettingWireless *sw;

        c = find_connection_for_device (device_wifi, device);
        if (c == NULL)
                return NULL;

        sw = nm_connection_get_setting_wireless (c);
        return nm_setting_wireless_get_ssid (sw);
}

static void
get_secrets_cb (GObject            *source_object,
                GAsyncResult       *res,
                gpointer            data)
{
        NetDeviceWifi *device_wifi = data;
        GVariant *secrets;
        GError *error = NULL;

        secrets = nm_remote_connection_get_secrets_finish (NM_REMOTE_CONNECTION (source_object), res, &error);
        if (!secrets) {
                if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
                        g_warning ("Could not get secrets: %s", error->message);
                g_error_free (error);
                return;
        }

        nm_connection_update_secrets (NM_CONNECTION (source_object),
                                      NM_SETTING_WIRELESS_SECURITY_SETTING_NAME,
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
        NMSettingWirelessSecurity *sws;
        const gchar *key_mgmt;
        const gchar *tmp_secret;
        const gchar *tmp_security;

        c = find_connection_for_device (device_wifi, device);
        if (c == NULL)
                return;

        sws = nm_connection_get_setting_wireless_security (c);
        if (sws == NULL)
                return;

        tmp_secret = NULL;
        tmp_security = C_("Wifi security", "None");

        /* Key management values:
         * "none" = WEP
         * "wpa-none" = WPAv1 Ad-Hoc mode (not supported in NM >= 0.9.4)
         * "wpa-psk" = WPAv2 Ad-Hoc mode (eg IBSS RSN) and AP-mode WPA v1 and v2
         */
        key_mgmt = nm_setting_wireless_security_get_key_mgmt (sws);
        if (strcmp (key_mgmt, "none") == 0) {
                tmp_secret = nm_setting_wireless_security_get_wep_key (sws, 0);
                tmp_security = _("WEP");
        }
        else if (strcmp (key_mgmt, "wpa-none") == 0 ||
                 strcmp (key_mgmt, "wpa-psk") == 0) {
                tmp_secret = nm_setting_wireless_security_get_psk (sws);
                tmp_security = _("WPA");
        } else {
                g_warning ("unhandled security key-mgmt: %s", key_mgmt);
        }

        /* If we don't have secrets, request them from NM and bail.
         * We'll refresh the UI when secrets arrive.
         */
        if (tmp_secret == NULL) {
                GCancellable *cancellable = net_object_get_cancellable (NET_OBJECT (device_wifi));
                nm_remote_connection_get_secrets_async ((NMRemoteConnection*)c,
                                                        NM_SETTING_WIRELESS_SECURITY_SETTING_NAME,
                                                        cancellable,
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
nm_device_wifi_refresh_hotspot (NetDeviceWifi *device_wifi)
{
        GBytes *ssid;
        gchar *hotspot_secret = NULL;
        gchar *hotspot_security = NULL;
        gchar *hotspot_ssid = NULL;
        NMDevice *nm_device;

        /* refresh hotspot ui */
        nm_device = net_device_get_nm_device (NET_DEVICE (device_wifi));
        ssid = device_get_hotspot_ssid (device_wifi, nm_device);
        if (ssid)
                hotspot_ssid = nm_utils_ssid_to_utf8 (g_bytes_get_data (ssid, NULL), g_bytes_get_size (ssid));
        device_get_hotspot_security_details (device_wifi,
                                             nm_device,
                                             &hotspot_secret,
                                             &hotspot_security);

        g_debug ("Refreshing hotspot labels to name: '%s', security key: '%s', security: '%s'",
                 hotspot_ssid, hotspot_secret, hotspot_security);

        panel_set_device_widget_details (device_wifi->builder,
                                         "hotspot_network_name",
                                         hotspot_ssid);
        panel_set_device_widget_details (device_wifi->builder,
                                         "hotspot_security_key",
                                         hotspot_secret);
        panel_set_device_widget_details (device_wifi->builder,
                                         "hotspot_security",
                                         hotspot_security);
        panel_set_device_widget_details (device_wifi->builder,
                                         "hotspot_connected",
                                         NULL);

        g_free (hotspot_secret);
        g_free (hotspot_security);
        g_free (hotspot_ssid);
}

static void
update_last_used (NetDeviceWifi *device_wifi, NMConnection *connection)
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
        panel_set_device_widget_details (device_wifi->builder,
                                         "last_used",
                                         last_used);
        if (now != NULL)
                g_date_time_unref (now);
        if (then != NULL)
                g_date_time_unref (then);
        g_free (last_used);
}

static gboolean
request_scan (gpointer user_data)
{
        NetDeviceWifi *device_wifi = user_data;
        NMDevice *nm_device;

        g_debug ("Periodic Wi-Fi scan requested");

        nm_device = net_device_get_nm_device (NET_DEVICE (device_wifi));
        nm_device_wifi_request_scan_async (NM_DEVICE_WIFI (nm_device),
                                           device_wifi->cancellable, NULL, NULL);

        return G_SOURCE_CONTINUE;
}

static void
nm_device_wifi_refresh_ui (NetDeviceWifi *device_wifi)
{
        const gchar *str;
        gchar *str_tmp = NULL;
        gint strength = 0;
        guint speed = 0;
        NMAccessPoint *active_ap;
        NMDevice *nm_device;
        NMDeviceState state;
        NMClient *client;
        NMAccessPoint *ap;
        NMConnection *connection;
        GtkWidget *dialog;

        if (device_is_hotspot (device_wifi)) {
                nm_device_wifi_refresh_hotspot (device_wifi);
                show_hotspot_ui (device_wifi);
                disable_scan_timeout (device_wifi);
                return;
        }

        client = net_object_get_client (NET_OBJECT (device_wifi));

        if (device_wifi->scan_id == 0 &&
            nm_client_wireless_get_enabled (client)) {
                device_wifi->scan_id = g_timeout_add_seconds (PERIODIC_WIFI_SCAN_TIMEOUT,
                                                              request_scan, device_wifi);
                request_scan (device_wifi);
        }

        dialog = device_wifi->details_dialog;

        ap = g_object_get_data (G_OBJECT (dialog), "ap");
        connection = g_object_get_data (G_OBJECT (dialog), "connection");

        nm_device = net_device_get_nm_device (NET_DEVICE (device_wifi));
        active_ap = nm_device_wifi_get_active_access_point (NM_DEVICE_WIFI (nm_device));

        state = nm_device_get_state (nm_device);

        /* keep this in sync with the signal handler setup in cc_network_panel_init */
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
        panel_set_device_widget_details (device_wifi->builder,
                                         "speed",
                                         str_tmp);

        /* device MAC */
        str = nm_device_wifi_get_hw_address (NM_DEVICE_WIFI (nm_device));
        panel_set_device_widget_details (device_wifi->builder,
                                         "mac",
                                         str);
        /* security */
        if (ap != active_ap)
                str_tmp = NULL;
        else if (active_ap != NULL)
                str_tmp = get_ap_security_string (active_ap);
        panel_set_device_widget_details (device_wifi->builder,
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
        panel_set_device_widget_details (device_wifi->builder,
                                         "strength",
                                         str);

        /* device MAC */
        if (ap != active_ap)
                str = NULL;
        else
                str = nm_device_wifi_get_hw_address (NM_DEVICE_WIFI (nm_device));
        panel_set_device_widget_details (device_wifi->builder, "mac", str);

        /* set IP entries */
        if (ap != active_ap)
                panel_unset_device_widgets (device_wifi->builder);
        else
                panel_set_device_widgets (device_wifi->builder, nm_device);

        if (ap != active_ap && connection)
                update_last_used (device_wifi, connection);
        else
                panel_set_device_widget_details (device_wifi->builder, "last_used", NULL);

        panel_set_device_status (device_wifi->builder, "heading_status", nm_device, NULL);

        /* update list of APs */
        show_wifi_list (device_wifi);
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

        if (device_wifi->updating_device)
                return;

        client = net_object_get_client (NET_OBJECT (device_wifi));
        active = gtk_switch_get_active (sw);
        nm_client_wireless_set_enabled (client, active);
        if (!active)
                disable_scan_timeout (device_wifi);
}

static void
connect_to_hidden_network (NetDeviceWifi *device_wifi)
{
        NMClient *client;
        CcNetworkPanel *panel;
        GtkWidget *toplevel;

        client = net_object_get_client (NET_OBJECT (device_wifi));
        panel = net_object_get_panel (NET_OBJECT (device_wifi));
        toplevel = cc_shell_get_toplevel (cc_panel_get_shell (CC_PANEL (panel)));
        cc_network_panel_connect_to_hidden_network (toplevel, client);
}

static void
connection_add_activate_cb (GObject *source_object,
                            GAsyncResult *res,
                            gpointer user_data)
{
        NMActiveConnection *conn;
        GError *error = NULL;

        conn = nm_client_add_and_activate_connection_finish (NM_CLIENT (source_object), res, &error);
        if (!conn) {
                if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
                        nm_device_wifi_refresh_ui (user_data);
                        /* failed to activate */
                        g_warning ("Failed to add and activate connection '%d': %s",
                                   error->code,
                                   error->message);
                }
                g_error_free (error);
                return;
        }
}

static void
connection_activate_cb (GObject *source_object,
                        GAsyncResult *res,
                        gpointer user_data)
{
        GError *error = NULL;

        if (!nm_client_activate_connection_finish (NM_CLIENT (source_object), res, &error)) {
                if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
                        nm_device_wifi_refresh_ui (user_data);
                        /* failed to activate */
                        g_debug ("Failed to add and activate connection '%d': %s",
                                 error->code,
                                 error->message);
                }
                g_error_free (error);
                return;
        }
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
                         GBytes *ssid,
                         const gchar *ap_object_path)
{
        const gchar *ssid_target;
        NMDevice *device;
        NMClient *client;
        GCancellable *cancellable;

        if (device_wifi->updating_device)
                goto out;

        if (ap_object_path == NULL || ap_object_path[0] == 0)
                goto out;

        device = net_device_get_nm_device (NET_DEVICE (device_wifi));
        if (device == NULL)
                goto out;

        ssid_target = nm_utils_escape_ssid ((gpointer) g_bytes_get_data (ssid, NULL), g_bytes_get_size (ssid));
        g_debug ("try to connect to WIFI network %s [%s]",
                 ssid_target, ap_object_path);

        /* activate the connection */
        client = net_object_get_client (NET_OBJECT (device_wifi));
        cancellable = net_object_get_cancellable (NET_OBJECT (device_wifi));

        if (!is_8021x (device, ap_object_path)) {
                GPermission *permission;
                gboolean allowed_to_share = FALSE;
                NMConnection *partial = NULL;

                permission = polkit_permission_new_sync ("org.freedesktop.NetworkManager.settings.modify.system",
                                                         NULL, NULL, NULL);
                if (permission) {
                        allowed_to_share = g_permission_get_allowed (permission);
                        g_object_unref (permission);
                }

                if (!allowed_to_share) {
                        NMSettingConnection *s_con;

                        s_con = (NMSettingConnection *)nm_setting_connection_new ();
                        nm_setting_connection_add_permission (s_con, "user", g_get_user_name (), NULL);
                        partial = nm_simple_connection_new ();
                        nm_connection_add_setting (partial, NM_SETTING (s_con));
                }

                g_debug ("no existing connection found for %s, creating and activating one", ssid_target);
                nm_client_add_and_activate_connection_async (client,
                                                             partial,
                                                             device,
                                                             ap_object_path,
                                                             cancellable,
                                                             connection_add_activate_cb,
                                                             device_wifi);
                if (!allowed_to_share)
                        g_object_unref (partial);
        } else {
                CcNetworkPanel *panel;
                GVariantBuilder *builder;
                GVariant *parameters;

                g_debug ("no existing connection found for %s, creating", ssid_target);
                builder = g_variant_builder_new (G_VARIANT_TYPE ("av"));
                g_variant_builder_add (builder, "v", g_variant_new_string ("connect-8021x-wifi"));
                g_variant_builder_add (builder, "v", g_variant_new_string (nm_object_get_path (NM_OBJECT (device))));
                g_variant_builder_add (builder, "v", g_variant_new_string (ap_object_path));
                parameters = g_variant_new ("av", builder);

                panel = net_object_get_panel (NET_OBJECT (device_wifi));
                g_object_set (G_OBJECT (panel), "parameters", parameters, NULL);

                g_variant_builder_unref (builder);
        }
out:
        return;
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
        if (bus == NULL) {
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

        if (res == NULL) {
                g_warning ("Getting pretty hostname failed: %s", error->message);
                g_error_free (error);
        }

        str = NULL;

        if (res != NULL) {
                g_variant_get (res, "(v)", &inner);
                str = g_variant_dup_string (inner, NULL);
                g_variant_unref (res);
        }

        return str;
}

static GBytes *
generate_ssid_for_hotspot (NetDeviceWifi *device_wifi)
{
        GBytes *ssid_bytes;
        gchar *hostname, *ssid;

        hostname = get_hostname ();
        ssid = pretty_hostname_to_ssid (hostname);
        g_free (hostname);

        ssid_bytes = g_bytes_new_with_free_func (ssid,
                                                 strlen (ssid),
                                                 g_free,
                                                 NULL);

        return ssid_bytes;
}

#define WPA_PASSKEY_SIZE 8
static void
set_wpa_key (NMSettingWirelessSecurity *sws)
{
        /* generate a 8-chars ASCII WPA key */
        char key[WPA_PASSKEY_SIZE + 1];
        guint i;

        for (i = 0; i < WPA_PASSKEY_SIZE; i++) {
                gint c;
                c = g_random_int_range (33, 126);
                /* too many non alphanumeric characters are hard to remember for humans */
                while (!g_ascii_isalnum (c))
                        c = g_random_int_range (33, 126);

                key[i] = (gchar) c;
        }
        key[WPA_PASSKEY_SIZE] = '\0';

        g_object_set (sws,
                      "key-mgmt", "wpa-psk",
                      "psk", key,
                      NULL);
}

static void
set_wep_key (NMSettingWirelessSecurity *sws)
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

        g_object_set (sws,
                      "key-mgmt", "none",
                      "wep-key0", key,
                      "wep-key-type", NM_WEP_KEY_TYPE_KEY,
                      NULL);
}

static gboolean
is_hotspot_connection (NMConnection *connection)
{
        NMSettingConnection *sc;
        NMSettingWireless *sw;
        NMSettingIPConfig *sip;
        NMSetting *setting;

        sc = nm_connection_get_setting_connection (connection);
        if (g_strcmp0 (nm_setting_connection_get_connection_type (sc), "802-11-wireless") != 0) {
                return FALSE;
        }
        sw = nm_connection_get_setting_wireless (connection);
        if (g_strcmp0 (nm_setting_wireless_get_mode (sw), "adhoc") != 0 &&
            g_strcmp0 (nm_setting_wireless_get_mode (sw), "ap") != 0) {
                return FALSE;
        }
        setting = nm_connection_get_setting_by_name (connection, NM_SETTING_WIRELESS_SETTING_NAME);
        if (!setting)
                return FALSE;

        sip = nm_connection_get_setting_ip4_config (connection);
        if (g_strcmp0 (nm_setting_ip_config_get_method (sip), "shared") != 0) {
                return FALSE;
        }

        return TRUE;
}

static void
show_hotspot_ui (NetDeviceWifi *device_wifi)
{
        GtkWidget *widget;

        /* show hotspot tab */
        widget = GTK_WIDGET (gtk_builder_get_object (device_wifi->builder, "notebook_view"));
        gtk_notebook_set_current_page (GTK_NOTEBOOK (widget), 1);

        /* force switch to on as this succeeded */
        device_wifi->updating_device = TRUE;
        gtk_switch_set_active (device_wifi->hotspot_switch, TRUE);
        device_wifi->updating_device = FALSE;
}

static void
activate_cb (GObject            *source_object,
             GAsyncResult       *res,
             gpointer            user_data)
{
        GError *error = NULL;

        if (nm_client_activate_connection_finish (NM_CLIENT (source_object), res, &error) == NULL) {
                g_warning ("Failed to add new connection: (%d) %s",
                           error->code,
                           error->message);
                g_error_free (error);
                return;
        }

        /* show hotspot tab */
        nm_device_wifi_refresh_ui (user_data);
}

static void
activate_new_cb (GObject            *source_object,
                 GAsyncResult       *res,
                 gpointer            user_data)
{
        NMActiveConnection *conn;
        GError *error = NULL;

        conn = nm_client_add_and_activate_connection_finish (NM_CLIENT (source_object),
                                                             res, &error);
        if (!conn) {
                g_warning ("Failed to add new connection: (%d) %s",
                           error->code,
                           error->message);
                g_error_free (error);
                return;
        }

        /* show hotspot tab */
        nm_device_wifi_refresh_ui (user_data);
}

static NMConnection *
net_device_wifi_get_hotspot_connection (NetDeviceWifi *device_wifi)
{
        GSList *connections, *l;
        NMConnection *c = NULL;

        connections = net_device_get_valid_connections (NET_DEVICE (device_wifi));
        for (l = connections; l; l = l->next) {
                NMConnection *tmp = l->data;
                if (is_hotspot_connection (tmp)) {
                        c = tmp;
                        break;
                }
        }
        g_slist_free (connections);

        return c;
}

static void
overwrite_ssid_cb (GObject      *source_object,
                   GAsyncResult *res,
                   gpointer      user_data)
{
        GError *error = NULL;
        NMClient *client;
        NMRemoteConnection *connection;
        NMDevice *device;
        NMConnection *c;
        NetDeviceWifi *device_wifi;
        GCancellable *cancellable;

        connection = NM_REMOTE_CONNECTION (source_object);

        if (!nm_remote_connection_commit_changes_finish (connection, res, &error)) {
                if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
                        g_warning ("Failed to save hotspot's settings to disk: %s",
                                   error->message);
                g_error_free (error);
                return;
        }

        device_wifi = user_data;
        device = net_device_get_nm_device (NET_DEVICE (device_wifi));
        client = net_object_get_client (NET_OBJECT (device_wifi));
        cancellable = net_object_get_cancellable (NET_OBJECT (device_wifi));
        c = net_device_wifi_get_hotspot_connection (device_wifi);

        g_debug ("activate existing hotspot connection\n");
        nm_client_activate_connection_async (client,
                                             c,
                                             device,
                                             NULL,
                                             cancellable,
                                             activate_cb,
                                             device_wifi);
}

static void
start_shared_connection (NetDeviceWifi *device_wifi)
{
        NMConnection *c;
        NMSettingConnection *sc;
        NMSettingWireless *sw;
        NMSettingIP4Config *sip;
        NMSettingWirelessSecurity *sws;
        NMDevice *device;
        GBytes *ssid;
        const gchar *str_mac;
        struct ether_addr *bin_mac;
        NMClient *client;
        const char *mode;
        NMDeviceWifiCapabilities caps;
        GCancellable *cancellable;

        device = net_device_get_nm_device (NET_DEVICE (device_wifi));
        g_assert (nm_device_get_device_type (device) == NM_DEVICE_TYPE_WIFI);

        c = net_device_wifi_get_hotspot_connection (device_wifi);

        ssid = generate_ssid_for_hotspot (device_wifi);

        client = net_object_get_client (NET_OBJECT (device_wifi));
        cancellable = net_object_get_cancellable (NET_OBJECT (device_wifi));
        if (c != NULL) {
                NMSettingWireless *sw;
                const char *c_path;
                NMRemoteConnection *connection;

                sw = nm_connection_get_setting_wireless (c);
                g_object_set (sw, "ssid", ssid, NULL);
                g_bytes_unref (ssid);

                c_path = nm_connection_get_path (c);
                connection = nm_client_get_connection_by_path (client, c_path);

                g_debug ("overwriting ssid to %s", (char *) g_bytes_get_data (ssid, NULL));

                nm_remote_connection_commit_changes_async (connection,
                                                           TRUE,
                                                           cancellable,
                                                           overwrite_ssid_cb,
                                                           device_wifi);
                return;
        }

        g_debug ("create new hotspot connection with SSID '%s'",
                 (char *) g_bytes_get_data (ssid, NULL));
        c = nm_simple_connection_new ();

        sc = (NMSettingConnection *)nm_setting_connection_new ();
        g_object_set (sc,
                      "type", "802-11-wireless",
                      "id", "Hotspot",
                      "autoconnect", FALSE,
                      NULL);
        nm_connection_add_setting (c, (NMSetting *)sc);

        sw = (NMSettingWireless *)nm_setting_wireless_new ();

	/* Use real AP mode if the device supports it */
        caps = nm_device_wifi_get_capabilities (NM_DEVICE_WIFI (device));
        if (caps & NM_WIFI_DEVICE_CAP_AP)
		mode = NM_SETTING_WIRELESS_MODE_AP;
        else
                mode = NM_SETTING_WIRELESS_MODE_ADHOC;

        g_object_set (sw,
                      "mode", mode,
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

        g_object_set (sw, "ssid", ssid, NULL);
        g_bytes_unref (ssid);

        sws = (NMSettingWirelessSecurity*) nm_setting_wireless_security_new ();

        if (g_strcmp0 (mode, NM_SETTING_WIRELESS_MODE_AP) == 0) {
                if (caps & NM_WIFI_DEVICE_CAP_RSN) {
                        set_wpa_key (sws);
                        nm_setting_wireless_security_add_proto (sws, "rsn");
                        nm_setting_wireless_security_add_pairwise (sws, "ccmp");
                        nm_setting_wireless_security_add_group (sws, "ccmp");
                } else if (caps & NM_WIFI_DEVICE_CAP_WPA) {
                        set_wpa_key (sws);
                        nm_setting_wireless_security_add_proto (sws, "wpa");
                        nm_setting_wireless_security_add_pairwise (sws, "tkip");
                        nm_setting_wireless_security_add_group (sws, "tkip");
                } else {
                        set_wep_key (sws);
                }
        } else {
                set_wep_key (sws);
        }

        nm_connection_add_setting (c, (NMSetting *)sws);

        nm_client_add_and_activate_connection_async (client,
                                                     c,
                                                     device,
                                                     NULL,
                                                     cancellable,
                                                     activate_new_cb,
                                                     device_wifi);

        g_object_unref (c);
}

static void
start_hotspot_response_cb (GtkWidget *dialog, gint response, NetDeviceWifi *device_wifi)
{
        if (response == GTK_RESPONSE_OK) {
                start_shared_connection (device_wifi);
        }
        gtk_widget_destroy (dialog);
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
        GtkWidget *message_area;
        GtkWidget *label;
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
                                GBytes *ssid;
                                ap = nm_device_wifi_get_active_access_point (NM_DEVICE_WIFI (device));
                                ssid = nm_access_point_get_ssid (ap);
                                active_ssid = nm_utils_ssid_to_utf8 (g_bytes_get_data (ssid, NULL), g_bytes_get_size (ssid));
                                break;
                        }
                }
        }

        window = gtk_widget_get_toplevel (GTK_WIDGET (button));

        str = g_string_new ("");

        if (active_ssid) {
                g_string_append_printf (str, _("Switching on the wireless hotspot will disconnect you from <b>%s</b>."), active_ssid);
                g_string_append (str, " ");
        }

        g_string_append (str, _("It is not possible to access the Internet through your wireless while the hotspot is active."));

        dialog = gtk_message_dialog_new_with_markup (GTK_WINDOW (window),
                                                     GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                                     GTK_MESSAGE_OTHER,
                                                     GTK_BUTTONS_NONE,
                                                     "<big><b>%s</b></big>",
                                                     _("Turn On Wi-Fi Hotspot?"));

        /* Because we can't control the text alignment with markup, add
         * the strings as labels directly */
        message_area = gtk_message_dialog_get_message_area (GTK_MESSAGE_DIALOG (dialog));

        label = g_object_new (GTK_TYPE_LABEL,
                              "use-markup", TRUE,
                              "xalign", 0.5,
                              "max-width-chars", 50,
                              "wrap", TRUE,
                              "label", str->str,
                              "justify", GTK_JUSTIFY_CENTER,
                              "halign", GTK_ALIGN_CENTER,
                              NULL);
        gtk_container_add (GTK_CONTAINER (message_area), label);

        label = g_object_new (GTK_TYPE_LABEL,
                              "use-markup", TRUE,
                              "xalign", 0.5,
                              "max-width-chars", 50,
                              "wrap", TRUE,
                              "label", _("Wi-Fi hotspots are usually used to share an additional Internet connection over Wi-Fi."),
                              "justify", GTK_JUSTIFY_CENTER,
                              "halign", GTK_ALIGN_CENTER,
                              NULL);
        gtk_style_context_add_class (gtk_widget_get_style_context (label), "dim-label");
        gtk_container_add (GTK_CONTAINER (message_area), label);

        gtk_widget_show_all (message_area);

        gtk_dialog_add_buttons (GTK_DIALOG (dialog),
                                _("_Cancel"), GTK_RESPONSE_CANCEL,
                                _("_Turn On"), GTK_RESPONSE_OK,
                                NULL);
        g_signal_connect (dialog, "response",
                          G_CALLBACK (start_hotspot_response_cb), device_wifi);

        gtk_window_present (GTK_WINDOW (dialog));
        g_free (active_ssid);
        g_string_free (str, TRUE);
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
        gboolean found = FALSE;

        device = net_device_get_nm_device (NET_DEVICE (device_wifi));
        client = net_object_get_client (NET_OBJECT (device_wifi));
        connections = nm_client_get_active_connections (client);
        for (i = 0; connections && i < connections->len; i++) {
                c = (NMActiveConnection *)connections->pdata[i];

                devices = nm_active_connection_get_devices (c);
                if (devices && devices->pdata[0] == device) {
                        nm_client_deactivate_connection (client, c, NULL, NULL);
                        found = TRUE;
                        break;
                }
        }

        if (!found) {
                g_warning ("Could not stop hotspot connection as no connection attached to the device could be found.");
                device_wifi->updating_device = TRUE;
                gtk_switch_set_active (device_wifi->hotspot_switch, TRUE);
                device_wifi->updating_device = FALSE;
                return;
        }

        nm_device_wifi_refresh_ui (device_wifi);
}

static void
stop_hotspot_response_cb (GtkWidget *dialog, gint response, NetDeviceWifi *device_wifi)
{
        if (response == GTK_RESPONSE_OK) {
                stop_shared_connection (device_wifi);
        } else {
                device_wifi->updating_device = TRUE;
                gtk_switch_set_active (device_wifi->hotspot_switch, TRUE);
                device_wifi->updating_device = FALSE;
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

        if (device_wifi->updating_device)
                return;

        panel = net_object_get_panel (NET_OBJECT (device_wifi));
        window = gtk_widget_get_toplevel (GTK_WIDGET (panel));
        dialog = gtk_message_dialog_new (GTK_WINDOW (window),
                                         GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                         GTK_MESSAGE_OTHER,
                                         GTK_BUTTONS_NONE,
                                         _("Stop hotspot and disconnect any users?"));
        gtk_dialog_add_buttons (GTK_DIALOG (dialog),
                                _("_Cancel"), GTK_RESPONSE_CANCEL,
                                _("_Stop Hotspot"), GTK_RESPONSE_OK,
                                NULL);
        g_signal_connect (dialog, "response",
                          G_CALLBACK (stop_hotspot_response_cb), device_wifi);
        gtk_window_present (GTK_WINDOW (dialog));
}

static void
show_wifi_list (NetDeviceWifi *device_wifi)
{
        GtkWidget *widget;
        widget = GTK_WIDGET (gtk_builder_get_object (device_wifi->builder, "notebook_view"));
        gtk_notebook_set_current_page (GTK_NOTEBOOK (widget), 0);
}

static void
net_device_wifi_constructed (GObject *object)
{
        NetDeviceWifi *device_wifi = NET_DEVICE_WIFI (object);
        NMClient *client;
        NMClientPermissionResult perm;
        NMDevice *nm_device;
        NMDeviceWifiCapabilities caps;
        GtkWidget *list_container;
        GtkWidget *list;
        GtkWidget *widget;

        G_OBJECT_CLASS (net_device_wifi_parent_class)->constructed (object);

        client = net_object_get_client (NET_OBJECT (device_wifi));
        g_signal_connect_object (client, "notify::wireless-enabled",
                                 G_CALLBACK (wireless_enabled_toggled), device_wifi, 0);

        nm_device = net_device_get_nm_device (NET_DEVICE (device_wifi));

        list_container = GTK_WIDGET (gtk_builder_get_object (device_wifi->builder, "listbox_box"));
        list = GTK_WIDGET (cc_wifi_connection_list_new (client, NM_DEVICE_WIFI (nm_device), TRUE, TRUE, FALSE));
        gtk_widget_show (list);
        gtk_container_add (GTK_CONTAINER (list_container), list);

        gtk_list_box_set_header_func (GTK_LIST_BOX (list), cc_list_box_update_header_func, NULL, NULL);
        gtk_list_box_set_sort_func (GTK_LIST_BOX (list), (GtkListBoxSortFunc)ap_sort, NULL, NULL);

        g_signal_connect (list, "row-activated",
                          G_CALLBACK (ap_activated), device_wifi);
        g_signal_connect_swapped (list, "configure",
                                  G_CALLBACK (show_details_for_row),
                                  device_wifi);

        /* only enable the button if the user can create a hotspot */
        widget = GTK_WIDGET (gtk_builder_get_object (device_wifi->builder,
                                                     "start_hotspot_button"));
        perm = nm_client_get_permission_result (client, NM_CLIENT_PERMISSION_WIFI_SHARE_OPEN);
        caps = nm_device_wifi_get_capabilities (NM_DEVICE_WIFI (nm_device));
        if (perm != NM_CLIENT_PERMISSION_RESULT_YES &&
            perm != NM_CLIENT_PERMISSION_RESULT_AUTH) {
                gtk_widget_set_tooltip_text (widget, _("System policy prohibits use as a Hotspot"));
                gtk_widget_set_sensitive (widget, FALSE);
        } else if (!(caps & (NM_WIFI_DEVICE_CAP_AP | NM_WIFI_DEVICE_CAP_ADHOC))) {
                gtk_widget_set_tooltip_text (widget, _("Wireless device does not support Hotspot mode"));
                gtk_widget_set_sensitive (widget, FALSE);
        } else
                gtk_widget_set_sensitive (widget, TRUE);

        widget = GTK_WIDGET (gtk_builder_get_object (device_wifi->builder, "heading_list"));
        g_object_bind_property (device_wifi, "title", widget, "label", 0);

        nm_device_wifi_refresh_ui (device_wifi);
}

static void
net_device_wifi_finalize (GObject *object)
{
        NetDeviceWifi *device_wifi = NET_DEVICE_WIFI (object);

        if (device_wifi->cancellable) {
                g_cancellable_cancel (device_wifi->cancellable);
                g_clear_object (&device_wifi->cancellable);
        }
        disable_scan_timeout (device_wifi);

        g_clear_pointer (&device_wifi->details_dialog, gtk_widget_destroy);
        g_object_unref (device_wifi->builder);
        g_free (device_wifi->selected_ssid_title);
        g_free (device_wifi->selected_connection_id);
        g_free (device_wifi->selected_ap_id);

        G_OBJECT_CLASS (net_device_wifi_parent_class)->finalize (object);
}

static void
device_wifi_edit (NetObject *object)
{
        const gchar *uuid;
        gchar *cmdline;
        GError *error = NULL;
        NetDeviceWifi *device = NET_DEVICE_WIFI (object);
        NMClient *client;
        NMRemoteConnection *connection;

        client = net_object_get_client (object);
        connection = nm_client_get_connection_by_path (client, device->selected_connection_id);
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
        parent_class->add_to_stack = device_wifi_proxy_add_to_stack;
        parent_class->refresh = device_wifi_refresh;
        parent_class->edit = device_wifi_edit;
}

static void
really_forgotten (GObject              *source_object,
                  GAsyncResult         *res,
                  gpointer              user_data)
{
        CcWifiConnectionList *list = user_data;
        GError *error = NULL;

        cc_wifi_connection_list_thaw (list);
        g_object_unref (list);

        if (!nm_remote_connection_delete_finish (NM_REMOTE_CONNECTION (source_object), res, &error)) {
                g_warning ("failed to delete connection %s: %s",
                           nm_object_get_path (NM_OBJECT (source_object)),
                           error->message);
                g_error_free (error);
                return;
        }
}

static void
really_forget (GtkDialog *dialog, gint response, gpointer data)
{
        GtkWidget *forget = data;
        CcWifiConnectionRow *row;
        GList *rows;
        GList *r;
        NMRemoteConnection *connection;
        NetDeviceWifi *device_wifi;
        GCancellable *cancellable;

        gtk_widget_destroy (GTK_WIDGET (dialog));

        if (response != GTK_RESPONSE_OK)
                return;

        device_wifi = NET_DEVICE_WIFI (g_object_get_data (G_OBJECT (forget), "net"));
        cancellable = net_object_get_cancellable (NET_OBJECT (device_wifi));
        rows = g_object_steal_data (G_OBJECT (forget), "rows");
        for (r = rows; r; r = r->next) {
                row = r->data;
                connection = NM_REMOTE_CONNECTION (cc_wifi_connection_row_get_connection (row));
                nm_remote_connection_delete_async (connection, cancellable, really_forgotten, g_object_ref (data));
                gtk_widget_hide (GTK_WIDGET (row));
        }
        g_list_free (rows);
}

static void
forget_selected (GtkButton *forget, CcWifiConnectionList *list)
{
        GtkWidget *window;
        GtkWidget *dialog;

        window = gtk_widget_get_toplevel (GTK_WIDGET (forget));
        dialog = gtk_message_dialog_new (GTK_WINDOW (window),
                                         GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                         GTK_MESSAGE_OTHER,
                                         GTK_BUTTONS_NONE,
                                         NULL);
        gtk_message_dialog_set_markup (GTK_MESSAGE_DIALOG (dialog),
                                       _("Network details for the selected networks, including passwords and any custom configuration will be lost."));

        gtk_dialog_add_buttons (GTK_DIALOG (dialog),
                                _("_Cancel"), GTK_RESPONSE_CANCEL,
                                _("_Forget"), GTK_RESPONSE_OK,
                                NULL);
        g_signal_connect (dialog, "response",
                          G_CALLBACK (really_forget), list);
        gtk_window_present (GTK_WINDOW (dialog));
}

static void
check_toggled (CcWifiConnectionRow *row, GParamSpec *pspec, CcWifiConnectionList *list)
{
        GtkWidget *forget;
        gboolean active;
        GList *rows;

        active = cc_wifi_connection_row_get_checked (row);
        rows = g_object_steal_data (G_OBJECT (list), "rows");

        if (active) {
                cc_wifi_connection_list_freeze (list);
                rows = g_list_prepend (rows, row);
        } else {
                cc_wifi_connection_list_thaw (list);
                rows = g_list_remove (rows, row);
        }

        g_object_set_data_full (G_OBJECT (list), "rows", rows, (GDestroyNotify)g_list_free);
        forget = g_object_get_data (G_OBJECT (list), "forget");
        gtk_widget_set_sensitive (forget, rows != NULL);
}

static gint
history_sort (gconstpointer a, gconstpointer b, gpointer data)
{
        guint64 ta, tb;
        NMConnection *ca, *cb;
        NMSettingConnection *sc;

        ca = cc_wifi_connection_row_get_connection (CC_WIFI_CONNECTION_ROW ((gpointer) a));
        cb = cc_wifi_connection_row_get_connection (CC_WIFI_CONNECTION_ROW ((gpointer) b));

        if (ca) {
                sc = nm_connection_get_setting_connection (ca);
                ta = nm_setting_connection_get_timestamp (sc);
        } else {
                ta = 0;
        }

        if (cb) {
                sc = nm_connection_get_setting_connection (cb);
                tb = nm_setting_connection_get_timestamp (sc);
        } else {
                tb = 0;
        }

        if (ta > tb) return -1;
        if (tb > ta) return 1;

        return 0;
}

static gint
ap_sort (gconstpointer a, gconstpointer b, gpointer data)
{
        NMAccessPoint *apa, *apb;
        guint sa, sb;

        apa = cc_wifi_connection_row_best_access_point (CC_WIFI_CONNECTION_ROW ((gpointer) a));
        apb = cc_wifi_connection_row_best_access_point (CC_WIFI_CONNECTION_ROW ((gpointer) b));

        if (apa)
                sa = nm_access_point_get_strength (apa);
        else
                sa = 0;

        if (apb)
                sb = nm_access_point_get_strength (apb);
        else
                sb = 0;

        if (sa > sb) return -1;
        if (sb > sa) return 1;

        return 0;
}

static void
show_details_for_row (NetDeviceWifi *device_wifi, CcWifiConnectionRow *row, CcWifiConnectionList *list)
{
        NMConnection *connection;
        NMAccessPoint *ap;
        GtkWidget *window;
        NetConnectionEditor *editor;
        NMClient *client;
        NMDevice *device;

        window = gtk_widget_get_toplevel (GTK_WIDGET (row));

        connection = cc_wifi_connection_row_get_connection (row);
        ap = cc_wifi_connection_row_best_access_point (row);

        device = net_device_get_nm_device (NET_DEVICE (device_wifi));
        client = net_object_get_client (NET_OBJECT (device_wifi));
        editor = net_connection_editor_new (GTK_WINDOW (window), connection, device, ap, client);
        net_connection_editor_run (editor);
}

static void
on_connection_list_row_added_cb (NetDeviceWifi        *device_wifi,
                                 CcWifiConnectionRow  *row,
                                 CcWifiConnectionList *list)
{
        g_signal_connect (row, "notify::checked",
                          G_CALLBACK (check_toggled), list);
}

static void
on_connection_list_row_removed_cb (NetDeviceWifi        *device_wifi,
                                   CcWifiConnectionRow  *row,
                                   CcWifiConnectionList *list)
{
        GList *rows = g_object_steal_data (G_OBJECT (list), "rows");
        GList *item;
        GtkWidget *forget;

        item = g_list_find (rows, row);
        if (item)
          {
            rows = g_list_remove (rows, row);
            cc_wifi_connection_list_thaw (list);
          }
        g_object_set_data_full (G_OBJECT (list), "rows", rows, (GDestroyNotify)g_list_free);

        forget = g_object_get_data (G_OBJECT (list), "forget");
        gtk_widget_set_sensitive (forget, rows != NULL);
}

static void
on_connection_list_row_activated_cb (NetDeviceWifi        *device_wifi,
                                     CcWifiConnectionRow  *row,
                                     CcWifiConnectionList *list)
{
  cc_wifi_connection_row_set_checked (row, !cc_wifi_connection_row_get_checked (row));
}

static void
open_history (NetDeviceWifi *device_wifi)
{
        GtkWidget *dialog;
        GtkWidget *window;
        CcNetworkPanel *panel;
        GtkWidget *forget;
        GtkWidget *header;
        GtkWidget *swin;
        GtkWidget *content_area;
        GtkWidget *separator;
        NMDevice *nm_device;
        GtkWidget *list;
        GList *list_rows;

        dialog = g_object_new (GTK_TYPE_DIALOG, "use-header-bar", 1, NULL);
        panel = net_object_get_panel (NET_OBJECT (device_wifi));
        window = gtk_widget_get_toplevel (GTK_WIDGET (panel));
        gtk_window_set_transient_for (GTK_WINDOW (dialog), GTK_WINDOW (window));
        gtk_window_set_title (GTK_WINDOW (dialog), _("Known Wi-Fi Networks"));
        gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);
        gtk_window_set_default_size (GTK_WINDOW (dialog), 500, 400);

        /* Dialog's header */
        header = gtk_header_bar_new ();
        gtk_widget_show (header);
        gtk_header_bar_set_title (GTK_HEADER_BAR (header), _("Known Wi-Fi Networks"));
        gtk_header_bar_set_show_close_button (GTK_HEADER_BAR (header), TRUE);
        gtk_window_set_titlebar (GTK_WINDOW (dialog), header);

        g_signal_connect_swapped (dialog, "response",
                                  G_CALLBACK (gtk_widget_destroy), dialog);

        g_signal_connect_swapped (dialog, "delete-event",
                                  G_CALLBACK (gtk_widget_destroy), dialog);

        content_area = gtk_dialog_get_content_area (GTK_DIALOG (dialog));
        gtk_container_set_border_width (GTK_CONTAINER (content_area), 0);

        swin = gtk_scrolled_window_new (NULL, NULL);
        gtk_widget_show (swin);
        gtk_widget_set_vexpand (swin, TRUE);
        gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (swin), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
        gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (swin), GTK_SHADOW_NONE);
        gtk_container_add (GTK_CONTAINER (content_area), swin);

        nm_device = net_device_get_nm_device (NET_DEVICE (device_wifi));

        list = GTK_WIDGET (cc_wifi_connection_list_new (net_object_get_client (NET_OBJECT (device_wifi)),
                                                        NM_DEVICE_WIFI (nm_device),
                                                        FALSE, FALSE, TRUE));
        gtk_widget_show (list);
        gtk_list_box_set_selection_mode (GTK_LIST_BOX (list), GTK_SELECTION_NONE);
        gtk_list_box_set_header_func (GTK_LIST_BOX (list), cc_list_box_update_header_func, NULL, NULL);
        gtk_list_box_set_sort_func (GTK_LIST_BOX (list), (GtkListBoxSortFunc)history_sort, NULL, NULL);
        gtk_container_add (GTK_CONTAINER (swin), list);

        /* Horizontal separator */
        separator = gtk_separator_new (GTK_ORIENTATION_HORIZONTAL);
        gtk_widget_show (separator);
        gtk_container_add (GTK_CONTAINER (content_area), separator);

        /* translators: This is the label for the "Forget wireless network" functionality */
        forget = gtk_button_new_with_mnemonic (C_("Wi-Fi Network", "_Forget"));
        gtk_widget_show (forget);
        gtk_widget_set_halign (forget, GTK_ALIGN_START);
        gtk_widget_set_margin_top (forget, 6);
        gtk_widget_set_margin_bottom (forget, 6);
        gtk_widget_set_margin_start (forget, 6);
        gtk_widget_set_sensitive (forget, FALSE);
        gtk_style_context_add_class (gtk_widget_get_style_context (forget), "destructive-action");

        g_signal_connect (forget, "clicked",
                          G_CALLBACK (forget_selected), list);
        gtk_container_add (GTK_CONTAINER (content_area), forget);

        g_object_set_data (G_OBJECT (list), "forget", forget);
        g_object_set_data (G_OBJECT (list), "net", device_wifi);

        g_signal_connect_swapped (list, "add",
                                  G_CALLBACK (on_connection_list_row_added_cb),
                                  device_wifi);
        g_signal_connect_swapped (list, "remove",
                                  G_CALLBACK (on_connection_list_row_removed_cb),
                                  device_wifi);
        g_signal_connect_swapped (list, "row-activated",
                                  G_CALLBACK (on_connection_list_row_activated_cb),
                                  device_wifi);
        g_signal_connect_swapped (list, "configure",
                                  G_CALLBACK (show_details_for_row),
                                  device_wifi);

        list_rows = gtk_container_get_children (GTK_CONTAINER (list));
        while (list_rows)
          {
            on_connection_list_row_added_cb (device_wifi,
                                             CC_WIFI_CONNECTION_ROW (list_rows->data),
                                             CC_WIFI_CONNECTION_LIST (list));
            list_rows = g_list_delete_link (list_rows, list_rows);
          }

        gtk_window_present (GTK_WINDOW (dialog));
}

static void
ap_activated (GtkListBox *list, GtkListBoxRow *row, NetDeviceWifi *device_wifi)
{
        CcWifiConnectionRow *c_row;
        NMConnection *connection;
        NMAccessPoint *ap;
        NMClient *client;
        NMDevice *nm_device;
        GCancellable *cancellable;

        /* The mockups want a row to connecto hidden networks; this could
         * be handeled here. */
        if (!CC_IS_WIFI_CONNECTION_ROW (row))
                return;

        c_row = CC_WIFI_CONNECTION_ROW (row);

        connection = cc_wifi_connection_row_get_connection (c_row);
        ap = cc_wifi_connection_row_best_access_point (c_row);

        if (ap != NULL) {
                if (connection != NULL) {
                        client = net_object_get_client (NET_OBJECT (device_wifi));
                        nm_device = net_device_get_nm_device (NET_DEVICE (device_wifi));
                        cancellable = net_object_get_cancellable (NET_OBJECT (device_wifi));
                        nm_client_activate_connection_async (client,
                                                             connection,
                                                             nm_device, NULL, cancellable,
                                                             connection_activate_cb, device_wifi);
                } else {
                        GBytes *ssid;
                        const gchar *object_path;

                        ssid = nm_access_point_get_ssid (ap);
                        object_path = nm_object_get_path (NM_OBJECT (ap));
                        wireless_try_to_connect (device_wifi, ssid, object_path);
                }
        }
}

static void
net_device_wifi_init (NetDeviceWifi *device_wifi)
{
        GError *error = NULL;
        GtkWidget *widget;

        device_wifi->builder = gtk_builder_new ();
        gtk_builder_add_from_resource (device_wifi->builder,
                                       "/org/gnome/control-center/network/network-wifi.ui",
                                       &error);
        if (error != NULL) {
                g_warning ("Could not load interface file: %s", error->message);
                g_error_free (error);
                return;
        }

        device_wifi->cancellable = g_cancellable_new ();

        widget = GTK_WIDGET (gtk_builder_get_object (device_wifi->builder,
                                                     "details_dialog"));
        device_wifi->details_dialog = widget;

        /* setup wifi views */
        widget = GTK_WIDGET (gtk_builder_get_object (device_wifi->builder,
                                                     "device_off_switch"));
        g_signal_connect (widget, "notify::active",
                          G_CALLBACK (device_off_toggled), device_wifi);

        /* setup view */
        widget = GTK_WIDGET (gtk_builder_get_object (device_wifi->builder,
                                                     "notebook_view"));
        gtk_notebook_set_show_tabs (GTK_NOTEBOOK (widget), FALSE);
        gtk_notebook_set_current_page (GTK_NOTEBOOK (widget), 0);

        widget = GTK_WIDGET (gtk_builder_get_object (device_wifi->builder,
                                                     "start_hotspot_button"));
        g_signal_connect (widget, "clicked",
                          G_CALLBACK (start_hotspot), device_wifi);

        widget = GTK_WIDGET (gtk_builder_get_object (device_wifi->builder,
                                                     "connect_hidden_button"));
        g_signal_connect_swapped (widget, "clicked",
                                  G_CALLBACK (connect_to_hidden_network), device_wifi);

        widget = GTK_WIDGET (gtk_builder_get_object (device_wifi->builder,
                                                     "history_button"));
        g_signal_connect_swapped (widget, "clicked",
                                  G_CALLBACK (open_history), device_wifi);

        widget = GTK_WIDGET (gtk_builder_get_object (device_wifi->builder,
                                                     "switch_hotspot_off"));
        device_wifi->hotspot_switch = GTK_SWITCH (widget);
        g_signal_connect (widget, "notify::active",
                          G_CALLBACK (switch_hotspot_changed_cb), device_wifi);
}
