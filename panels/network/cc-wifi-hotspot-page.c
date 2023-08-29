/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* cc-wifi-hotspot-page.c
 *
 * Copyright 2023 Red Hat, Inc
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Author(s):
 *   Mohammed Sadiq <sadiq@sadiqpk.org>
 *   Felipe Borges <felipeborges@gnome.org>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "cc-wifi-hotspot-page"

#include <config.h>
#include <glib/gi18n.h>
#include <libmm-glib.h>

#include "cc-wifi-hotspot-page.h"
#include "cc-list-row.h"
#include "cc-network-resources.h"
#include "cc-qr-code.h"
#include "ui-helpers.h"
#include "panel-common.h"
#include "shell/cc-object-storage.h"
#include "net-device-wifi.h"

#define QR_IMAGE_SIZE 180

struct _CcWifiHotspotPage
{
    AdwNavigationPage parent_instance;

    GCancellable    *cancellable;

    CcQrCode        *qr_code;
    NetDeviceWifi   *device;
    NMConnection    *connection;
    gchar           *host_name;
    gboolean         wpa_supported; /* WPA/WPA2 supported */
  
    AdwStatusPage   *qrcode_status_page;
    CcListRow       *hotspot_name_row;
    CcListRow       *hotspot_security_row;
    CcListRow       *hotspot_password_row;
};

G_DEFINE_TYPE (CcWifiHotspotPage, cc_wifi_hotspot_page, ADW_TYPE_NAVIGATION_PAGE)
static NMConnection *
wifi_device_get_hotspot (CcWifiHotspotPage *self)
{
    NMClient *client;
    NMSettingIPConfig *ip4_setting;
    NMConnection *c;
    NMDevice *nm_device;

    nm_device = net_device_wifi_get_device (self->device);
    if (nm_device_get_active_connection (nm_device) == NULL)
        return NULL;

    client = cc_object_storage_get_object (CC_OBJECT_NMCLIENT);
    c = net_device_get_find_connection (client, nm_device);
    if (c == NULL)
        return NULL;

    ip4_setting = nm_connection_get_setting_ip4_config (c);
    if (g_strcmp0 (nm_setting_ip_config_get_method (ip4_setting),
                   NM_SETTING_IP4_CONFIG_METHOD_SHARED) != 0)
        return NULL;

    return c;
}

static NMConnection *
find_connection_for_device (NMDevice *device)
{
    NMClient *client;

    client = cc_object_storage_get_object (CC_OBJECT_NMCLIENT);
    if (client == NULL)
        return NULL;
    return net_device_get_find_connection (client, device);
}

static GBytes *
device_get_hotspot_ssid (NMDevice *device)
{
    NMConnection *c;
    NMSettingWireless *sw;

    c = find_connection_for_device (device);
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
    g_autoptr(GVariant) secrets = NULL;
    g_autoptr(GError) error = NULL;

    secrets = nm_remote_connection_get_secrets_finish (NM_REMOTE_CONNECTION (source_object), res, &error);
    if (!secrets)
        if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
            g_warning ("Could not get secrets: %s", error->message);
            return;
        }

    nm_connection_update_secrets (NM_CONNECTION (source_object),
                                  NM_SETTING_WIRELESS_SECURITY_SETTING_NAME,
                                  secrets, NULL);
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

    c = find_connection_for_device (device);
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
                                                NULL,
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
update_credential_rows (CcWifiHotspotPage *self)
{
    GBytes *ssid;
    g_autofree gchar *hotspot_secret = NULL;
    g_autofree gchar *hotspot_security = NULL;
    g_autofree gchar *hotspot_ssid = NULL;

    ssid = device_get_hotspot_ssid (net_device_wifi_get_device (self->device));
    if (ssid)
        hotspot_ssid = nm_utils_ssid_to_utf8 (g_bytes_get_data (ssid, NULL), g_bytes_get_size (ssid));
    device_get_hotspot_security_details (self->device,
                                         net_device_wifi_get_device (self->device),
                                         &hotspot_secret,
                                         &hotspot_security);

    g_debug ("Refreshing hotspot labels to name: '%s', security key: '%s', security: '%s'",
             hotspot_ssid, hotspot_secret, hotspot_security);

    cc_list_row_set_secondary_label (self->hotspot_name_row, hotspot_ssid);
    gtk_widget_set_visible (GTK_WIDGET (self->hotspot_name_row), hotspot_ssid != NULL);

    cc_list_row_set_secondary_label (self->hotspot_password_row, hotspot_secret);
    gtk_widget_set_visible (GTK_WIDGET (self->hotspot_password_row), hotspot_secret != NULL);

    cc_list_row_set_secondary_label (self->hotspot_security_row, hotspot_security);
    gtk_widget_set_visible (GTK_WIDGET (self->hotspot_security_row), hotspot_security != NULL);
}

static void
update_qr_image (CcWifiHotspotPage *self)
{
    NMConnection *hotspot;
    g_autofree gchar *str = NULL;
    g_autoptr (GVariant) secrets = NULL;
    g_autoptr (GError) error = NULL;

    hotspot = wifi_device_get_hotspot (self);
    if (!hotspot)
        return;

    secrets = nm_remote_connection_get_secrets (NM_REMOTE_CONNECTION (hotspot),
                                                NM_SETTING_WIRELESS_SECURITY_SETTING_NAME,
                                                NULL, &error);
    if (error) {
        g_warning ("Error: %s", error->message);

        return;
    }

    nm_connection_update_secrets (hotspot,
                                  NM_SETTING_WIRELESS_SECURITY_SETTING_NAME,
                                  secrets, &error);
    if (error) {
        g_warning ("Error: %s", error->message);

        return;
    }

    if (!is_qr_code_supported (hotspot))
        return;

    if (!self->qr_code)
        self->qr_code = cc_qr_code_new ();

    str = get_qr_string_for_connection (hotspot);
    if (cc_qr_code_set_text (self->qr_code, str)) {
        GdkPaintable *paintable;
        gint scale;

        adw_status_page_set_description (self->qrcode_status_page,
                                         _("Mobile devices can scan the QR code to connect."));

        scale = gtk_widget_get_scale_factor (GTK_WIDGET (self->qrcode_status_page));
        paintable = cc_qr_code_get_paintable (self->qr_code, QR_IMAGE_SIZE * scale);
        adw_status_page_set_paintable (self->qrcode_status_page, paintable);
    }
}

static void
cc_wifi_hotspot_page_finalize (GObject *object)
{
    CcWifiHotspotPage *self = (CcWifiHotspotPage *)object;

    g_cancellable_cancel(self->cancellable);
    g_clear_object (&self->cancellable);
    g_clear_pointer (&self->host_name, g_free);
    g_clear_object (&self->device);
    g_clear_object (&self->connection);

    G_OBJECT_CLASS (cc_wifi_hotspot_page_parent_class)->finalize (object);
}

static void
update_hotspot_info (CcWifiHotspotPage *self)
{
    update_qr_image (self);
    update_credential_rows (self);
}

static void
on_hotspot_turned_off_cb (CcWifiHotspotPage *self)
{
    net_device_wifi_turn_off_hotspot (NET_DEVICE_WIFI (self->device));
}

static void
cc_wifi_hotspot_page_class_init (CcWifiHotspotPageClass *klass)
{
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
    GObjectClass   *object_class = G_OBJECT_CLASS (klass);

    object_class->finalize = cc_wifi_hotspot_page_finalize;

    gtk_widget_class_set_template_from_resource (widget_class,
                                                 "/org/gnome/control-center/network/cc-wifi-hotspot-page.ui");

    gtk_widget_class_bind_template_child (widget_class, CcWifiHotspotPage, hotspot_name_row);
    gtk_widget_class_bind_template_child (widget_class, CcWifiHotspotPage, hotspot_security_row);
    gtk_widget_class_bind_template_child (widget_class, CcWifiHotspotPage, hotspot_password_row);
    gtk_widget_class_bind_template_child (widget_class, CcWifiHotspotPage, qrcode_status_page);

    gtk_widget_class_bind_template_callback (widget_class, update_hotspot_info);
    gtk_widget_class_bind_template_callback (widget_class, on_hotspot_turned_off_cb);
}

static void
cc_wifi_hotspot_page_init (CcWifiHotspotPage *self)
{
    self->cancellable = g_cancellable_new ();

    gtk_widget_init_template (GTK_WIDGET (self));
}

static void
cc_wifi_hotspot_page_set_device (CcWifiHotspotPage *self,
                                 NetDeviceWifi     *device)
{
    NMDevice *nm_device;
    NMDeviceWifiCapabilities caps;

    g_return_if_fail (CC_IS_WIFI_HOTSPOT_PAGE (self));
    g_return_if_fail (NET_IS_DEVICE_WIFI (device));

    g_set_object (&self->device, device);
    nm_device = net_device_wifi_get_device (device);
    g_return_if_fail (NM_IS_DEVICE_WIFI (nm_device));

    caps = nm_device_wifi_get_capabilities (NM_DEVICE_WIFI (nm_device));
    self->wpa_supported = FALSE;

    if (caps & NM_WIFI_DEVICE_CAP_AP)
      if (caps & (NM_WIFI_DEVICE_CAP_RSN | NM_WIFI_DEVICE_CAP_WPA))
        self->wpa_supported = TRUE;

    g_signal_connect_object (nm_device, "state-changed",
                             G_CALLBACK (update_hotspot_info),
                             self,
                             G_CONNECT_SWAPPED);
}

CcWifiHotspotPage *
cc_wifi_hotspot_page_new (NetDeviceWifi *device)
{
    CcWifiHotspotPage *self = g_object_new (CC_TYPE_WIFI_HOTSPOT_PAGE, NULL);
    cc_wifi_hotspot_page_set_device (self, device);;

    return self;
}
