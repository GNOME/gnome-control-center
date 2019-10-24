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

#define HANDY_USE_UNSTABLE_API
#include <handy.h>

#include <glib-object.h>
#include <glib/gi18n.h>

#include <netinet/ether.h>

#include <NetworkManager.h>
#include <polkit/polkit.h>

#include "cc-wifi-hotspot-dialog.h"
#include "list-box-helper.h"
#include "hostname-helper.h"
#include "network-dialogs.h"
#include "panel-common.h"

#include "connection-editor/net-connection-editor.h"
#include "net-device-wifi.h"

#include "cc-wifi-connection-list.h"
#include "cc-wifi-connection-row.h"

#define PERIODIC_WIFI_SCAN_TIMEOUT 15

static void nm_device_wifi_refresh_ui (NetDeviceWifi *self);
static void show_wifi_list (NetDeviceWifi *self);
static void show_hotspot_ui (NetDeviceWifi *self);
static void ap_activated (NetDeviceWifi *self, GtkListBoxRow *row);
static gint ap_sort (gconstpointer a, gconstpointer b, gpointer data);
static void show_details_for_row (NetDeviceWifi *self, CcWifiConnectionRow *row, CcWifiConnectionList *list );


struct _NetDeviceWifi
{
        GtkStack                 parent;

        GtkBox                  *center_box;
        GtkSwitch               *device_off_switch;
        GtkBox                  *header_box;
        GtkBox                  *hotspot_box;
        GtkLabel                *hotspot_network_name_heading_label;
        GtkLabel                *hotspot_network_name_label;
        GtkSwitch               *hotspot_off_switch;
        GtkLabel                *hotspot_security_heading_label;
        GtkLabel                *hotspot_security_key_heading_label;
        GtkLabel                *hotspot_security_key_label;
        GtkLabel                *hotspot_security_label;
        GtkBox                  *listbox_box;
        GtkButton               *start_hotspot_button;
        GtkLabel                *status_label;
        GtkLabel                *title_label;

        CcPanel                 *panel;
        NMClient                *client;
        NMDevice                *device;
        gboolean                 updating_device;
        gchar                   *selected_ssid_title;
        gchar                   *selected_connection_id;
        gchar                   *selected_ap_id;
        CcWifiHotspotDialog     *hotspot_dialog;

        gint64                   last_scan;
        gboolean                 scanning;

        guint                    monitor_scanning_id;
        guint                    scan_id;
        GCancellable            *cancellable;
};

enum {
        PROP_0,
        PROP_SCANNING,
        PROP_LAST,
};

G_DEFINE_TYPE (NetDeviceWifi, net_device_wifi, GTK_TYPE_STACK)

static void
disable_scan_timeout (NetDeviceWifi *self)
{
        g_debug ("Disabling periodic Wi-Fi scan");
        if (self->monitor_scanning_id > 0) {
                g_source_remove (self->monitor_scanning_id);
                self->monitor_scanning_id = 0;
        }
        if (self->scan_id > 0) {
                g_source_remove (self->scan_id);
                self->scan_id = 0;
        }
}

static void
wireless_enabled_toggled (NetDeviceWifi *self)
{
        gboolean enabled;

        enabled = nm_client_wireless_get_enabled (self->client);

        self->updating_device = TRUE;
        gtk_switch_set_active (self->device_off_switch, enabled);
        if (!enabled)
                disable_scan_timeout (self);
        self->updating_device = FALSE;
}

static NMConnection *
find_connection_for_device (NetDeviceWifi *self,
                            NMDevice       *device)
{
        return net_device_get_find_connection (self->client, device);
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
device_is_hotspot (NetDeviceWifi *self)
{
        NMConnection *c;

        if (nm_device_get_active_connection (self->device) == NULL)
                return FALSE;

        c = find_connection_for_device (self, self->device);
        if (c == NULL)
                return FALSE;

        return connection_is_shared (c);
}

static GBytes *
device_get_hotspot_ssid (NetDeviceWifi *self,
                         NMDevice *device)
{
        NMConnection *c;
        NMSettingWireless *sw;

        c = find_connection_for_device (self, device);
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
        NetDeviceWifi *self = data;
        GVariant *secrets;
        g_autoptr(GError) error = NULL;

        secrets = nm_remote_connection_get_secrets_finish (NM_REMOTE_CONNECTION (source_object), res, &error);
        if (!secrets) {
                if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
                        g_warning ("Could not get secrets: %s", error->message);
                return;
        }

        nm_connection_update_secrets (NM_CONNECTION (source_object),
                                      NM_SETTING_WIRELESS_SECURITY_SETTING_NAME,
                                      secrets, NULL);

        nm_device_wifi_refresh_ui (self);
}

static void
device_get_hotspot_security_details (NetDeviceWifi *self,
                                     NMDevice *device,
                                     gchar **secret,
                                     gchar **security)
{
        NMConnection *c;
        NMSettingWirelessSecurity *sws;
        const gchar *key_mgmt;
        const gchar *tmp_secret;
        const gchar *tmp_security;

        c = find_connection_for_device (self, device);
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
                nm_remote_connection_get_secrets_async ((NMRemoteConnection*)c,
                                                        NM_SETTING_WIRELESS_SECURITY_SETTING_NAME,
                                                        self->cancellable,
                                                        get_secrets_cb,
                                                        self);
                return;
        }

        if (secret)
                *secret = g_strdup (tmp_secret);
        if (security)
                *security = g_strdup (tmp_security);
}

static void
nm_device_wifi_refresh_hotspot (NetDeviceWifi *self)
{
        GBytes *ssid;
        g_autofree gchar *hotspot_secret = NULL;
        g_autofree gchar *hotspot_security = NULL;
        g_autofree gchar *hotspot_ssid = NULL;

        /* refresh hotspot ui */
        ssid = device_get_hotspot_ssid (self, self->device);
        if (ssid)
                hotspot_ssid = nm_utils_ssid_to_utf8 (g_bytes_get_data (ssid, NULL), g_bytes_get_size (ssid));
        device_get_hotspot_security_details (self,
                                             self->device,
                                             &hotspot_secret,
                                             &hotspot_security);

        g_debug ("Refreshing hotspot labels to name: '%s', security key: '%s', security: '%s'",
                 hotspot_ssid, hotspot_secret, hotspot_security);

        gtk_label_set_label (self->hotspot_network_name_label, hotspot_ssid);
        gtk_widget_set_visible (GTK_WIDGET (self->hotspot_network_name_heading_label), hotspot_ssid != NULL);
        gtk_widget_set_visible (GTK_WIDGET (self->hotspot_network_name_label), hotspot_ssid != NULL);

        gtk_label_set_label (self->hotspot_security_key_label, hotspot_secret);
        gtk_widget_set_visible (GTK_WIDGET (self->hotspot_security_key_heading_label), hotspot_secret != NULL);
        gtk_widget_set_visible (GTK_WIDGET (self->hotspot_security_key_label), hotspot_secret != NULL);

        gtk_label_set_label (self->hotspot_security_label, hotspot_security);
        gtk_widget_set_visible (GTK_WIDGET (self->hotspot_security_heading_label), hotspot_security != NULL);
        gtk_widget_set_visible (GTK_WIDGET (self->hotspot_security_label), hotspot_security != NULL);
}

static void
set_scanning (NetDeviceWifi *self,
              gboolean       scanning,
              gint64         last_scan)
{
        gboolean scanning_changed = self->scanning != scanning;

        self->scanning = scanning;
        self->last_scan = last_scan;

        if (scanning_changed)
                g_object_notify (G_OBJECT (self), "scanning");
}

static gboolean
update_scanning (gpointer user_data)
{
        NetDeviceWifi *self = user_data;
        gint64 last_scan;

        last_scan = nm_device_wifi_get_last_scan (NM_DEVICE_WIFI (self->device));

        /* The last_scan property is updated after the device finished scanning,
         * so notify about it and stop monitoring for changes.
         */
        if (self->last_scan != last_scan) {
                set_scanning (self, FALSE, last_scan);
                self->monitor_scanning_id = 0;
                return G_SOURCE_REMOVE;
        }

        return G_SOURCE_CONTINUE;
}

static gboolean
request_scan (gpointer user_data)
{
        NetDeviceWifi *self = user_data;

        g_debug ("Periodic Wi-Fi scan requested");

        set_scanning (self, TRUE,
                      nm_device_wifi_get_last_scan (NM_DEVICE_WIFI (self->device)));

        if (self->monitor_scanning_id == 0) {
                self->monitor_scanning_id = g_timeout_add (1500, update_scanning,
                                                                  self);
        }

        nm_device_wifi_request_scan_async (NM_DEVICE_WIFI (self->device),
                                           self->cancellable, NULL, NULL);

        return G_SOURCE_CONTINUE;
}

static void
nm_device_wifi_refresh_ui (NetDeviceWifi *self)
{
        g_autofree gchar *status = NULL;

        if (device_is_hotspot (self)) {
                nm_device_wifi_refresh_hotspot (self);
                show_hotspot_ui (self);
                disable_scan_timeout (self);
                return;
        }

        if (self->scan_id == 0 &&
            nm_client_wireless_get_enabled (self->client)) {
                self->scan_id = g_timeout_add_seconds (PERIODIC_WIFI_SCAN_TIMEOUT,
                                                       request_scan, self);
                request_scan (self);
        }

        /* keep this in sync with the signal handler setup in cc_network_panel_init */
        wireless_enabled_toggled (self);

        status = panel_device_status_to_localized_string (self->device, NULL);
        gtk_label_set_label (self->status_label, status);

        /* update list of APs */
        show_wifi_list (self);
}

static void
device_off_switch_changed_cb (NetDeviceWifi *self)
{
        gboolean active;

        if (self->updating_device)
                return;

        active = gtk_switch_get_active (self->device_off_switch);
        nm_client_wireless_set_enabled (self->client, active);
        if (!active)
                disable_scan_timeout (self);
}

static void
connect_hidden_button_clicked_cb (NetDeviceWifi *self)
{
        GtkWidget *toplevel;

        toplevel = gtk_widget_get_toplevel (GTK_WIDGET (self));
        cc_network_panel_connect_to_hidden_network (toplevel, self->client);
}

static void
connection_add_activate_cb (GObject *source_object,
                            GAsyncResult *res,
                            gpointer user_data)
{
        NMActiveConnection *conn;
        g_autoptr(GError) error = NULL;

        conn = nm_client_add_and_activate_connection_finish (NM_CLIENT (source_object), res, &error);
        if (!conn) {
                if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
                        nm_device_wifi_refresh_ui (user_data);
                        /* failed to activate */
                        g_warning ("Failed to add and activate connection '%d': %s",
                                   error->code,
                                   error->message);
                }
                return;
        }
}

static void
connection_activate_cb (GObject *source_object,
                        GAsyncResult *res,
                        gpointer user_data)
{
        g_autoptr(GError) error = NULL;

        if (!nm_client_activate_connection_finish (NM_CLIENT (source_object), res, &error)) {
                if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
                        nm_device_wifi_refresh_ui (user_data);
                        /* failed to activate */
                        g_debug ("Failed to add and activate connection '%d': %s",
                                 error->code,
                                 error->message);
                }
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
wireless_try_to_connect (NetDeviceWifi *self,
                         GBytes *ssid,
                         const gchar *ap_object_path)
{
        const gchar *ssid_target;

        if (self->updating_device)
                return;

        if (ap_object_path == NULL || ap_object_path[0] == 0)
                return;

        ssid_target = nm_utils_escape_ssid ((gpointer) g_bytes_get_data (ssid, NULL), g_bytes_get_size (ssid));
        g_debug ("try to connect to WIFI network %s [%s]",
                 ssid_target, ap_object_path);

        /* activate the connection */
        if (!is_8021x (self->device, ap_object_path)) {
                g_autoptr(GPermission) permission = NULL;
                gboolean allowed_to_share = FALSE;
                g_autoptr(NMConnection) partial = NULL;

                permission = polkit_permission_new_sync ("org.freedesktop.NetworkManager.settings.modify.system",
                                                         NULL, NULL, NULL);
                if (permission)
                        allowed_to_share = g_permission_get_allowed (permission);

                if (!allowed_to_share) {
                        NMSettingConnection *s_con;

                        s_con = (NMSettingConnection *)nm_setting_connection_new ();
                        nm_setting_connection_add_permission (s_con, "user", g_get_user_name (), NULL);
                        partial = nm_simple_connection_new ();
                        nm_connection_add_setting (partial, NM_SETTING (s_con));
                }

                g_debug ("no existing connection found for %s, creating and activating one", ssid_target);
                nm_client_add_and_activate_connection_async (self->client,
                                                             partial,
                                                             self->device,
                                                             ap_object_path,
                                                             self->cancellable,
                                                             connection_add_activate_cb,
                                                             self);
        } else {
                g_autoptr(GVariantBuilder) builder = NULL;
                GVariant *parameters;

                g_debug ("no existing connection found for %s, creating", ssid_target);
                builder = g_variant_builder_new (G_VARIANT_TYPE ("av"));
                g_variant_builder_add (builder, "v", g_variant_new_string ("connect-8021x-wifi"));
                g_variant_builder_add (builder, "v", g_variant_new_string (nm_object_get_path (NM_OBJECT (self->device))));
                g_variant_builder_add (builder, "v", g_variant_new_string (ap_object_path));
                parameters = g_variant_new ("av", builder);

                g_object_set (self->panel, "parameters", parameters, NULL);
        }
}

static gchar *
get_hostname (void)
{
        g_autoptr(GDBusConnection) bus = NULL;
        g_autoptr(GVariant) res = NULL;
        g_autoptr(GVariant) inner = NULL;
        g_autoptr(GError) error = NULL;

        bus = g_bus_get_sync (G_BUS_TYPE_SYSTEM, NULL, &error);
        if (bus == NULL) {
                g_warning ("Failed to get system bus connection: %s", error->message);
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

        if (res == NULL) {
                g_warning ("Getting pretty hostname failed: %s", error->message);
                return NULL;
        }

        g_variant_get (res, "(v)", &inner);
        return g_variant_dup_string (inner, NULL);
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
show_hotspot_ui (NetDeviceWifi *self)
{
        /* show hotspot tab */
        gtk_stack_set_visible_child (GTK_STACK (self), GTK_WIDGET (self->hotspot_box));

        /* force switch to on as this succeeded */
        self->updating_device = TRUE;
        gtk_switch_set_active (self->hotspot_off_switch, TRUE);
        self->updating_device = FALSE;
}

static void
activate_cb (GObject            *source_object,
             GAsyncResult       *res,
             gpointer            user_data)
{
        g_autoptr(GError) error = NULL;

        if (nm_client_activate_connection_finish (NM_CLIENT (source_object), res, &error) == NULL) {
                g_warning ("Failed to add new connection: (%d) %s",
                           error->code,
                           error->message);
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
        g_autoptr(GError) error = NULL;

        conn = nm_client_add_and_activate_connection_finish (NM_CLIENT (source_object),
                                                             res, &error);
        if (!conn) {
                g_warning ("Failed to add new connection: (%d) %s",
                           error->code,
                           error->message);
                return;
        }

        /* show hotspot tab */
        nm_device_wifi_refresh_ui (user_data);
}

static NMConnection *
net_device_wifi_get_hotspot_connection (NetDeviceWifi *self)
{
        GSList *connections, *l;
        NMConnection *c = NULL;

        connections = net_device_get_valid_connections (self->client, self->device);
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
        g_autoptr(GError) error = NULL;
        NMRemoteConnection *connection;
        NMConnection *c;
        NetDeviceWifi *self;

        connection = NM_REMOTE_CONNECTION (source_object);

        if (!nm_remote_connection_commit_changes_finish (connection, res, &error)) {
                if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
                        g_warning ("Failed to save hotspot's settings to disk: %s",
                                   error->message);
                return;
        }

        self = user_data;
        c = net_device_wifi_get_hotspot_connection (self);

        g_debug ("activate existing hotspot connection\n");
        nm_client_activate_connection_async (self->client,
                                             c,
                                             self->device,
                                             NULL,
                                             self->cancellable,
                                             activate_cb,
                                             self);
}

static void
start_hotspot_button_clicked_cb (NetDeviceWifi *self)
{
        g_autofree gchar *active_ssid = NULL;
        GtkWidget *window;
        NMConnection *c;
        g_autofree gchar *hostname = NULL;
        g_autofree gchar *ssid = NULL;
        gint response;

        window = gtk_widget_get_toplevel (GTK_WIDGET (self));

        if (!self->hotspot_dialog)
                self->hotspot_dialog = cc_wifi_hotspot_dialog_new (GTK_WINDOW (window));
        cc_wifi_hotspot_dialog_set_device (self->hotspot_dialog, NM_DEVICE_WIFI (self->device));
        hostname = get_hostname ();
        ssid =  pretty_hostname_to_ssid (hostname);
        cc_wifi_hotspot_dialog_set_hostname (self->hotspot_dialog, ssid);
                c = net_device_wifi_get_hotspot_connection (self);
        if (c)
                cc_wifi_hotspot_dialog_set_connection (self->hotspot_dialog, c);

        response = gtk_dialog_run (GTK_DIALOG (self->hotspot_dialog));

        if (response == GTK_RESPONSE_APPLY) {
                NMConnection *connection;

                connection = cc_wifi_hotspot_dialog_get_connection (self->hotspot_dialog);
                if (NM_IS_REMOTE_CONNECTION (connection))
                        nm_remote_connection_commit_changes_async (NM_REMOTE_CONNECTION (connection),
                                                                   TRUE,
                                                                   self->cancellable,
                                                                   overwrite_ssid_cb,
                                                                   self);
                else
                        nm_client_add_and_activate_connection_async (self->client,
                                                                     connection,
                                                                     self->device,
                                                                     NULL,
                                                                     self->cancellable,
                                                                     activate_new_cb,
                                                                     self);
        }

        gtk_widget_hide (GTK_WIDGET (self->hotspot_dialog));
}

static void
stop_shared_connection (NetDeviceWifi *self)
{
        const GPtrArray *connections;
        const GPtrArray *devices;
        gint i;
        NMActiveConnection *c;
        gboolean found = FALSE;

        connections = nm_client_get_active_connections (self->client);
        for (i = 0; connections && i < connections->len; i++) {
                c = (NMActiveConnection *)connections->pdata[i];

                devices = nm_active_connection_get_devices (c);
                if (devices && devices->pdata[0] == self->device) {
                        nm_client_deactivate_connection (self->client, c, NULL, NULL);
                        found = TRUE;
                        break;
                }
        }

        if (!found) {
                g_warning ("Could not stop hotspot connection as no connection attached to the device could be found.");
                self->updating_device = TRUE;
                gtk_switch_set_active (self->hotspot_off_switch, TRUE);
                self->updating_device = FALSE;
                return;
        }

        nm_device_wifi_refresh_ui (self);
}

static void
stop_hotspot_response_cb (NetDeviceWifi *self, gint response, GtkWidget *dialog)
{
        if (response == GTK_RESPONSE_OK) {
                stop_shared_connection (self);
        } else {
                self->updating_device = TRUE;
                gtk_switch_set_active (self->hotspot_off_switch, TRUE);
                self->updating_device = FALSE;
        }
        gtk_widget_destroy (dialog);
}

static void
hotspot_off_switch_changed_cb (NetDeviceWifi *self)
{
        GtkWidget *dialog;
        GtkWidget *window;

        if (self->updating_device)
                return;

        window = gtk_widget_get_toplevel (GTK_WIDGET (self));
        dialog = gtk_message_dialog_new (GTK_WINDOW (window),
                                         GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                         GTK_MESSAGE_OTHER,
                                         GTK_BUTTONS_NONE,
                                         _("Stop hotspot and disconnect any users?"));
        gtk_dialog_add_buttons (GTK_DIALOG (dialog),
                                _("_Cancel"), GTK_RESPONSE_CANCEL,
                                _("_Stop Hotspot"), GTK_RESPONSE_OK,
                                NULL);
        g_signal_connect_swapped (dialog, "response",
                                  G_CALLBACK (stop_hotspot_response_cb), self);
        gtk_window_present (GTK_WINDOW (dialog));
}

static void
show_wifi_list (NetDeviceWifi *self)
{
        gtk_stack_set_visible_child (GTK_STACK (self), GTK_WIDGET (self->listbox_box));
}

static void
net_device_wifi_finalize (GObject *object)
{
        NetDeviceWifi *self = NET_DEVICE_WIFI (object);

        g_cancellable_cancel (self->cancellable);
        g_clear_object (&self->cancellable);
        disable_scan_timeout (self);

        g_clear_object (&self->client);
        g_clear_object (&self->device);
        g_clear_pointer (&self->selected_ssid_title, g_free);
        g_clear_pointer (&self->selected_connection_id, g_free);
        g_clear_pointer (&self->selected_ap_id, g_free);

        G_OBJECT_CLASS (net_device_wifi_parent_class)->finalize (object);
}

static void
net_device_wifi_get_property (GObject    *object,
                              guint       prop_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
        NetDeviceWifi *self = NET_DEVICE_WIFI (object);

        switch (prop_id) {
        case PROP_SCANNING:
                g_value_set_boolean (value, self->scanning);
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
                break;
        }
}

static void
really_forgotten (GObject              *source_object,
                  GAsyncResult         *res,
                  gpointer              user_data)
{
        g_autoptr(CcWifiConnectionList) list = user_data;
        g_autoptr(GError) error = NULL;

        cc_wifi_connection_list_thaw (list);

        if (!nm_remote_connection_delete_finish (NM_REMOTE_CONNECTION (source_object), res, &error))
                g_warning ("failed to delete connection %s: %s",
                           nm_object_get_path (NM_OBJECT (source_object)),
                           error->message);
}

static void
really_forget (GtkDialog *dialog, gint response, gpointer data)
{
        GtkWidget *forget = data;
        CcWifiConnectionRow *row;
        GList *rows;
        GList *r;
        NMRemoteConnection *connection;
        NetDeviceWifi *self;

        gtk_widget_destroy (GTK_WIDGET (dialog));

        if (response != GTK_RESPONSE_OK)
                return;

        self = NET_DEVICE_WIFI (g_object_get_data (G_OBJECT (forget), "net"));
        rows = g_object_steal_data (G_OBJECT (forget), "rows");
        for (r = rows; r; r = r->next) {
                row = r->data;
                connection = NM_REMOTE_CONNECTION (cc_wifi_connection_row_get_connection (row));
                nm_remote_connection_delete_async (connection, self->cancellable, really_forgotten, g_object_ref (data));
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
show_details_for_row (NetDeviceWifi *self, CcWifiConnectionRow *row, CcWifiConnectionList *list)
{
        NMConnection *connection;
        NMAccessPoint *ap;
        GtkWidget *window;
        NetConnectionEditor *editor;

        window = gtk_widget_get_toplevel (GTK_WIDGET (row));

        connection = cc_wifi_connection_row_get_connection (row);
        ap = cc_wifi_connection_row_best_access_point (row);

        editor = net_connection_editor_new (GTK_WINDOW (window), connection, self->device, ap, self->client);
        net_connection_editor_run (editor);
}

static void
on_connection_list_row_added_cb (NetDeviceWifi        *self,
                                 CcWifiConnectionRow  *row,
                                 CcWifiConnectionList *list)
{
        g_signal_connect (row, "notify::checked",
                          G_CALLBACK (check_toggled), list);
}

static void
on_connection_list_row_removed_cb (NetDeviceWifi        *self,
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
on_connection_list_row_activated_cb (NetDeviceWifi        *self,
                                     CcWifiConnectionRow  *row,
                                     CcWifiConnectionList *list)
{
  cc_wifi_connection_row_set_checked (row, !cc_wifi_connection_row_get_checked (row));
}

static void
history_button_clicked_cb (NetDeviceWifi *self)
{
        GtkWidget *dialog;
        GtkWidget *window;
        GtkWidget *forget;
        GtkWidget *header;
        GtkWidget *swin;
        GtkWidget *content_area;
        GtkWidget *separator;
        GtkWidget *list;
        GList *list_rows;

        dialog = g_object_new (HDY_TYPE_DIALOG, "use-header-bar", 1, NULL);
        window = gtk_widget_get_toplevel (GTK_WIDGET (self));
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

        list = GTK_WIDGET (cc_wifi_connection_list_new (self->client,
                                                        NM_DEVICE_WIFI (self->device),
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
        g_object_set_data (G_OBJECT (list), "net", self);

        g_signal_connect_swapped (list, "add",
                                  G_CALLBACK (on_connection_list_row_added_cb),
                                  self);
        g_signal_connect_swapped (list, "remove",
                                  G_CALLBACK (on_connection_list_row_removed_cb),
                                  self);
        g_signal_connect_swapped (list, "row-activated",
                                  G_CALLBACK (on_connection_list_row_activated_cb),
                                  self);
        g_signal_connect_swapped (list, "configure",
                                  G_CALLBACK (show_details_for_row),
                                  self);

        list_rows = gtk_container_get_children (GTK_CONTAINER (list));
        while (list_rows)
          {
            on_connection_list_row_added_cb (self,
                                             CC_WIFI_CONNECTION_ROW (list_rows->data),
                                             CC_WIFI_CONNECTION_LIST (list));
            list_rows = g_list_delete_link (list_rows, list_rows);
          }

        gtk_window_present (GTK_WINDOW (dialog));
}

static void
ap_activated (NetDeviceWifi *self, GtkListBoxRow *row)
{
        CcWifiConnectionRow *c_row;
        NMConnection *connection;
        NMAccessPoint *ap;

        /* The mockups want a row to connecto hidden networks; this could
         * be handeled here. */
        if (!CC_IS_WIFI_CONNECTION_ROW (row))
                return;

        c_row = CC_WIFI_CONNECTION_ROW (row);

        connection = cc_wifi_connection_row_get_connection (c_row);
        ap = cc_wifi_connection_row_best_access_point (c_row);

        if (ap != NULL) {
                if (connection != NULL) {
                        nm_client_activate_connection_async (self->client,
                                                             connection,
                                                             self->device, NULL, self->cancellable,
                                                             connection_activate_cb, self);
                } else {
                        GBytes *ssid;
                        const gchar *object_path;

                        ssid = nm_access_point_get_ssid (ap);
                        object_path = nm_object_get_path (NM_OBJECT (ap));
                        wireless_try_to_connect (self, ssid, object_path);
                }
        }
}

static void
net_device_wifi_class_init (NetDeviceWifiClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);
        GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

        object_class->finalize = net_device_wifi_finalize;
        object_class->get_property = net_device_wifi_get_property;

        g_object_class_install_property (object_class,
                                         PROP_SCANNING,
                                         g_param_spec_boolean ("scanning",
                                                               "Scanning",
                                                               "Whether the device is scanning for access points",
                                                               FALSE,
                                                               G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

        gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/network/network-wifi.ui");

        gtk_widget_class_bind_template_child (widget_class, NetDeviceWifi, center_box);
        gtk_widget_class_bind_template_child (widget_class, NetDeviceWifi, device_off_switch);
        gtk_widget_class_bind_template_child (widget_class, NetDeviceWifi, header_box);
        gtk_widget_class_bind_template_child (widget_class, NetDeviceWifi, hotspot_box);
        gtk_widget_class_bind_template_child (widget_class, NetDeviceWifi, hotspot_network_name_heading_label);
        gtk_widget_class_bind_template_child (widget_class, NetDeviceWifi, hotspot_network_name_label);
        gtk_widget_class_bind_template_child (widget_class, NetDeviceWifi, hotspot_off_switch);
        gtk_widget_class_bind_template_child (widget_class, NetDeviceWifi, hotspot_security_heading_label);
        gtk_widget_class_bind_template_child (widget_class, NetDeviceWifi, hotspot_security_key_heading_label);
        gtk_widget_class_bind_template_child (widget_class, NetDeviceWifi, hotspot_security_key_label);
        gtk_widget_class_bind_template_child (widget_class, NetDeviceWifi, hotspot_security_label);
        gtk_widget_class_bind_template_child (widget_class, NetDeviceWifi, listbox_box);
        gtk_widget_class_bind_template_child (widget_class, NetDeviceWifi, start_hotspot_button);
        gtk_widget_class_bind_template_child (widget_class, NetDeviceWifi, status_label);
        gtk_widget_class_bind_template_child (widget_class, NetDeviceWifi, title_label);

        gtk_widget_class_bind_template_callback (widget_class, connect_hidden_button_clicked_cb);
        gtk_widget_class_bind_template_callback (widget_class, device_off_switch_changed_cb);
        gtk_widget_class_bind_template_callback (widget_class, history_button_clicked_cb);
        gtk_widget_class_bind_template_callback (widget_class, hotspot_off_switch_changed_cb);
        gtk_widget_class_bind_template_callback (widget_class, start_hotspot_button_clicked_cb);
}

static void
net_device_wifi_init (NetDeviceWifi *self)
{
        gtk_widget_init_template (GTK_WIDGET (self));

        self->cancellable = g_cancellable_new ();
}

NetDeviceWifi *
net_device_wifi_new (CcPanel *panel, NMClient *client, NMDevice *device)
{
        NetDeviceWifi *self;
        NMClientPermissionResult perm;
        NMDeviceWifiCapabilities caps;
        GtkWidget *list;

        self = g_object_new (net_device_wifi_get_type (), NULL);
        self->panel = panel;
        self->client = g_object_ref (client);
        self->device = g_object_ref (device);

        g_signal_connect_object (client, "notify::wireless-enabled",
                                 G_CALLBACK (wireless_enabled_toggled), self, G_CONNECT_SWAPPED);

        g_signal_connect_object (device, "state-changed", G_CALLBACK (nm_device_wifi_refresh_ui), self, G_CONNECT_SWAPPED);

        list = GTK_WIDGET (cc_wifi_connection_list_new (client, NM_DEVICE_WIFI (device), TRUE, TRUE, FALSE));
        gtk_widget_show (list);
        gtk_container_add (GTK_CONTAINER (self->listbox_box), list);

        gtk_list_box_set_header_func (GTK_LIST_BOX (list), cc_list_box_update_header_func, NULL, NULL);
        gtk_list_box_set_sort_func (GTK_LIST_BOX (list), (GtkListBoxSortFunc)ap_sort, NULL, NULL);

        g_signal_connect_swapped (list, "row-activated",
                                  G_CALLBACK (ap_activated), self);
        g_signal_connect_swapped (list, "configure",
                                  G_CALLBACK (show_details_for_row),
                                  self);

        /* only enable the button if the user can create a hotspot */
        perm = nm_client_get_permission_result (client, NM_CLIENT_PERMISSION_WIFI_SHARE_OPEN);
        caps = nm_device_wifi_get_capabilities (NM_DEVICE_WIFI (self->device));
        if (perm != NM_CLIENT_PERMISSION_RESULT_YES &&
            perm != NM_CLIENT_PERMISSION_RESULT_AUTH) {
                gtk_widget_set_tooltip_text (GTK_WIDGET (self->start_hotspot_button), _("System policy prohibits use as a Hotspot"));
                gtk_widget_set_sensitive (GTK_WIDGET (self->start_hotspot_button), FALSE);
        } else if (!(caps & (NM_WIFI_DEVICE_CAP_AP | NM_WIFI_DEVICE_CAP_ADHOC))) {
                gtk_widget_set_tooltip_text (GTK_WIDGET (self->start_hotspot_button), _("Wireless device does not support Hotspot mode"));
                gtk_widget_set_sensitive (GTK_WIDGET (self->start_hotspot_button), FALSE);
        } else
                gtk_widget_set_sensitive (GTK_WIDGET (self->start_hotspot_button), TRUE);

        nm_device_wifi_refresh_ui (self);

        return self;
}

NMDevice *
net_device_wifi_get_device (NetDeviceWifi *self)
{
        g_return_val_if_fail (NET_IS_DEVICE_WIFI (self), NULL);
        return self->device;
}

void
net_device_wifi_set_title (NetDeviceWifi *self, const gchar *title)
{
        g_return_if_fail (NET_IS_DEVICE_WIFI (self));
        gtk_label_set_label (self->title_label, title);
}

GtkWidget *
net_device_wifi_get_header_widget (NetDeviceWifi *self)
{
        g_return_val_if_fail (NET_IS_DEVICE_WIFI (self), NULL);
        return GTK_WIDGET (self->header_box);
}

GtkWidget *
net_device_wifi_get_title_widget (NetDeviceWifi *self)
{
        g_return_val_if_fail (NET_IS_DEVICE_WIFI (self), NULL);
        return GTK_WIDGET (self->center_box);
}
