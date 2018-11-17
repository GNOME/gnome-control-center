/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2012 Red Hat, Inc
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

#include "../panel-common.h"
#include "ce-page-details.h"

G_DEFINE_TYPE (CEPageDetails, ce_page_details, CE_TYPE_PAGE)

static void
forget_cb (GtkButton *button, CEPageDetails *page)
{
        net_connection_editor_forget (page->editor);
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
update_last_used (CEPageDetails *page, NMConnection *connection)
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
                last_used = g_strdup (_("Never"));
                goto out;
        }

        /* calculate the amount of time that has elapsed */
        now = g_date_time_new_now_utc ();
        then = g_date_time_new_from_unix_utc (timestamp);

        diff = g_date_time_difference  (now, then);
        days = diff / G_TIME_SPAN_DAY;
        if (days == 0)
                last_used = g_strdup (_("Today"));
        else if (days == 1)
                last_used = g_strdup (_("Yesterday"));
        else
                last_used = g_strdup_printf (ngettext ("%i day ago", "%i days ago", days), days);
out:
        panel_set_device_widget_details (CE_PAGE (page)->builder, "last_used", last_used);
        if (now != NULL)
                g_date_time_unref (now);
        if (then != NULL)
                g_date_time_unref (then);
        g_free (last_used);
}

static void
all_user_changed (GtkToggleButton *b, CEPageDetails *page)
{
        gboolean all_users;
        NMSettingConnection *sc;

        sc = nm_connection_get_setting_connection (CE_PAGE (page)->connection);
        all_users = gtk_toggle_button_get_active (b);

        g_object_set (sc, "permissions", NULL, NULL);
        if (!all_users)
                nm_setting_connection_add_permission (sc, "user", g_get_user_name (), NULL);
}

static void
restrict_data_changed (GtkToggleButton *toggle, GParamSpec *pspec, CEPageDetails *page)
{
        NMSettingConnection *s_con;
        NMMetered metered;

        s_con = nm_connection_get_setting_connection (CE_PAGE (page)->connection);

        if (gtk_toggle_button_get_active (toggle))
                metered = NM_METERED_YES;
        else
                metered = NM_METERED_NO;

        g_object_set (s_con, "metered", metered, NULL);
}

static void
update_restrict_data (CEPageDetails *page)
{
        NMSettingConnection *s_con;
        NMMetered metered;
        GtkWidget *widget;
        const gchar *type;

        s_con = nm_connection_get_setting_connection (CE_PAGE (page)->connection);

        if (s_con == NULL)
                return;

        /* Disable for VPN; NetworkManager does not implement that yet (see
         * bug https://bugzilla.gnome.org/show_bug.cgi?id=792618) */
        type = nm_setting_connection_get_connection_type (s_con);
        if (g_str_equal (type, NM_SETTING_VPN_SETTING_NAME))
                return;

        metered = nm_setting_connection_get_metered (s_con);

        widget = GTK_WIDGET (gtk_builder_get_object (CE_PAGE (page)->builder, "restrict_data_check"));
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget),
                                      metered == NM_METERED_YES || metered == NM_METERED_GUESS_YES);
        gtk_widget_show (widget);

        g_signal_connect (widget, "notify::active", G_CALLBACK (restrict_data_changed), page);
        g_signal_connect_swapped (widget, "notify::active", G_CALLBACK (ce_page_changed), page);
}

static void
connect_details_page (CEPageDetails *page)
{
        NMSettingConnection *sc;
        GtkWidget *widget;
        guint speed;
        NMDeviceWifiCapabilities wifi_caps;
        guint strength;
        NMDeviceState state;
        NMAccessPoint *active_ap;
        g_autofree gchar *speed_label = NULL;
        const gchar *type;
        const gchar *hw_address = NULL;
        g_autofree gchar *security_string = NULL;
        g_autofree gchar *freq_string = NULL;
        const gchar *strength_label;
        gboolean device_is_active;

        sc = nm_connection_get_setting_connection (CE_PAGE (page)->connection);
        type = nm_setting_connection_get_connection_type (sc);

        if (NM_IS_DEVICE_WIFI (page->device))
                active_ap = nm_device_wifi_get_active_access_point (NM_DEVICE_WIFI (page->device));
        else
                active_ap = NULL;

        state = page->device ? nm_device_get_state (page->device) : NM_DEVICE_STATE_DISCONNECTED;

        device_is_active = FALSE;
        speed = 0;
        wifi_caps = 0;
        if (active_ap && page->ap == active_ap && state != NM_DEVICE_STATE_UNAVAILABLE) {
                device_is_active = TRUE;
                if (NM_IS_DEVICE_WIFI (page->device)) {
                        speed = nm_device_wifi_get_bitrate (NM_DEVICE_WIFI (page->device)) / 1000;
                        wifi_caps = nm_device_wifi_get_capabilities (NM_DEVICE_WIFI (page->device));
                }
        } else if (page->device) {
                NMActiveConnection *ac;
                const gchar *p1, *p2;

                ac = nm_device_get_active_connection (page->device);
                p1 = ac ? nm_active_connection_get_uuid (ac) : NULL;
                p2 = nm_connection_get_uuid (CE_PAGE (page)->connection);
                if (g_strcmp0 (p1, p2) == 0) {
                        device_is_active = TRUE;
                        if (NM_IS_DEVICE_WIFI (page->device)) {
                                speed = nm_device_wifi_get_bitrate (NM_DEVICE_WIFI (page->device)) / 1000;
                                wifi_caps = nm_device_wifi_get_capabilities (NM_DEVICE_WIFI (page->device));
                        } else if (NM_IS_DEVICE_ETHERNET (page->device))
                                speed = nm_device_ethernet_get_speed (NM_DEVICE_ETHERNET (page->device));
                }
        }
        if (speed > 0)
                speed_label = g_strdup_printf (_("%d Mb/s"), speed);
        panel_set_device_widget_details (CE_PAGE (page)->builder, "speed", speed_label);

        if (NM_IS_DEVICE_WIFI (page->device))
                hw_address = nm_device_wifi_get_hw_address (NM_DEVICE_WIFI (page->device));
        else if (NM_IS_DEVICE_ETHERNET (page->device))
                hw_address = nm_device_ethernet_get_hw_address (NM_DEVICE_ETHERNET (page->device));

        panel_set_device_widget_details (CE_PAGE (page)->builder, "mac", hw_address);

        if (wifi_caps & NM_WIFI_DEVICE_CAP_FREQ_VALID) {
                if (wifi_caps & NM_WIFI_DEVICE_CAP_FREQ_2GHZ &&
                    wifi_caps & NM_WIFI_DEVICE_CAP_FREQ_5GHZ)
                        freq_string = g_strdup (_("2.4 GHz / 5 GHz"));
                else if (wifi_caps & NM_WIFI_DEVICE_CAP_FREQ_2GHZ)
                        freq_string = g_strdup (_("2.4 GHz"));
                else if (wifi_caps & NM_WIFI_DEVICE_CAP_FREQ_5GHZ)
                        freq_string = g_strdup (_("5 GHz"));
        }

        panel_set_device_widget_details (CE_PAGE (page)->builder, "freq", freq_string);

        if (device_is_active && active_ap)
                security_string = get_ap_security_string (active_ap);
        panel_set_device_widget_details (CE_PAGE (page)->builder, "security", security_string);

        strength = 0;
        if (page->ap != NULL)
                strength = nm_access_point_get_strength (page->ap);

        if (strength <= 0)
                strength_label = NULL;
        else if (strength < 20)
                strength_label = C_("Signal strength", "None");
        else if (strength < 40)
                strength_label = C_("Signal strength", "Weak");
        else if (strength < 50)
                strength_label = C_("Signal strength", "Ok");
        else if (strength < 80)
                strength_label = C_("Signal strength", "Good");
        else
                strength_label = C_("Signal strength", "Excellent");
        panel_set_device_widget_details (CE_PAGE (page)->builder, "strength", strength_label);

        /* set IP entries */
        if (device_is_active)
                panel_set_device_widgets (CE_PAGE (page)->builder, page->device);
        else
                panel_unset_device_widgets (CE_PAGE (page)->builder);

        if (!device_is_active && CE_PAGE (page)->connection)
                update_last_used (page, CE_PAGE (page)->connection);
        else
                panel_set_device_widget_details (CE_PAGE (page)->builder, "last_used", NULL);

        /* Auto connect check */
        widget = GTK_WIDGET (gtk_builder_get_object (CE_PAGE (page)->builder,
                                                     "auto_connect_check"));
        if (g_str_equal (type, NM_SETTING_VPN_SETTING_NAME)) {
                gtk_widget_hide (widget);
        } else {
                g_object_bind_property (sc, "autoconnect",
                                        widget, "active",
                                        G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE);
                g_signal_connect_swapped (widget, "toggled", G_CALLBACK (ce_page_changed), page);
        }

        /* All users check */
        widget = GTK_WIDGET (gtk_builder_get_object (CE_PAGE (page)->builder,
                                                     "all_user_check"));
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget),
                                      nm_setting_connection_get_num_permissions (sc) == 0);
        g_signal_connect (widget, "toggled",
                          G_CALLBACK (all_user_changed), page);
        g_signal_connect_swapped (widget, "toggled", G_CALLBACK (ce_page_changed), page);

        /* Restrict Data check */
        update_restrict_data (page);

        /* Forget button */
        widget = GTK_WIDGET (gtk_builder_get_object (CE_PAGE (page)->builder, "button_forget"));
        g_signal_connect (widget, "clicked", G_CALLBACK (forget_cb), page);

        if (g_str_equal (type, NM_SETTING_WIRELESS_SETTING_NAME))
                gtk_button_set_label (GTK_BUTTON (widget), _("Forget Connection"));
        else if (g_str_equal (type, NM_SETTING_WIRED_SETTING_NAME))
                gtk_button_set_label (GTK_BUTTON (widget), _("Remove Connection Profile"));
        else if (g_str_equal (type, NM_SETTING_VPN_SETTING_NAME))
                gtk_button_set_label (GTK_BUTTON (widget), _("Remove VPN"));
        else
                gtk_widget_hide (widget);
}

static void
ce_page_details_init (CEPageDetails *page)
{
}

static void
ce_page_details_class_init (CEPageDetailsClass *class)
{
}

CEPage *
ce_page_details_new (NMConnection        *connection,
                     NMClient            *client,
                     NMDevice            *device,
                     NMAccessPoint       *ap,
                     NetConnectionEditor *editor)
{
        CEPageDetails *page;

        page = CE_PAGE_DETAILS (ce_page_new (CE_TYPE_PAGE_DETAILS,
                                             connection,
                                             client,
                                             "/org/gnome/control-center/network/details-page.ui",
                                             _("Details")));

        page->editor = editor;
        page->device = device;
        page->ap = ap;

        connect_details_page (page);

        return CE_PAGE (page);
}
