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

#include "ce-page-details.h"

G_DEFINE_TYPE (CEPageDetails, ce_page_details, CE_TYPE_PAGE)

static void
forget_cb (CEPageDetails *self)
{
        net_connection_editor_forget (self->editor);
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
update_last_used (CEPageDetails *self, NMConnection *connection)
{
        g_autofree gchar *last_used = NULL;
        g_autoptr(GDateTime) now = NULL;
        g_autoptr(GDateTime) then = NULL;
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
        gtk_label_set_label (self->last_used_label, last_used);
        gtk_widget_set_visible (GTK_WIDGET (self->last_used_heading_label), last_used != NULL);
        gtk_widget_set_visible (GTK_WIDGET (self->last_used_label), last_used != NULL);
}

static void
all_user_changed (CEPageDetails *self)
{
        gboolean all_users;
        NMSettingConnection *sc;

        sc = nm_connection_get_setting_connection (CE_PAGE (self)->connection);
        all_users = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (self->all_user_check));

        g_object_set (sc, "permissions", NULL, NULL);
        if (!all_users)
                nm_setting_connection_add_permission (sc, "user", g_get_user_name (), NULL);
}

static void
restrict_data_changed (CEPageDetails *self)
{
        NMSettingConnection *s_con;
        NMMetered metered;

        s_con = nm_connection_get_setting_connection (CE_PAGE (self)->connection);

        if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (self->restrict_data_check)))
                metered = NM_METERED_YES;
        else
                metered = NM_METERED_NO;

        g_object_set (s_con, "metered", metered, NULL);
}

static void
update_restrict_data (CEPageDetails *self)
{
        NMSettingConnection *s_con;
        NMMetered metered;
        const gchar *type;

        s_con = nm_connection_get_setting_connection (CE_PAGE (self)->connection);

        if (s_con == NULL)
                return;

        /* Disable for VPN; NetworkManager does not implement that yet (see
         * bug https://bugzilla.gnome.org/show_bug.cgi?id=792618) */
        type = nm_setting_connection_get_connection_type (s_con);
        if (g_str_equal (type, NM_SETTING_VPN_SETTING_NAME))
                return;

        metered = nm_setting_connection_get_metered (s_con);

        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (self->restrict_data_check),
                                      metered == NM_METERED_YES || metered == NM_METERED_GUESS_YES);
        gtk_widget_show (GTK_WIDGET (self->restrict_data_check));

        g_signal_connect_swapped (self->restrict_data_check, "notify::active", G_CALLBACK (restrict_data_changed), self);
        g_signal_connect_swapped (self->restrict_data_check, "notify::active", G_CALLBACK (ce_page_changed), self);
}

static void
connect_details_page (CEPageDetails *self)
{
        NMSettingConnection *sc;
        guint speed;
        guint strength;
        NMDeviceState state;
        NMAccessPoint *active_ap;
        g_autofree gchar *speed_label = NULL;
        const gchar *type;
        const gchar *hw_address = NULL;
        g_autofree gchar *security_string = NULL;
        const gchar *strength_label;
        gboolean device_is_active;
        NMIPConfig *ipv4_config = NULL, *ipv6_config = NULL;
        gboolean have_ipv4_address = FALSE, have_ipv6_address = FALSE;

        sc = nm_connection_get_setting_connection (CE_PAGE (self)->connection);
        type = nm_setting_connection_get_connection_type (sc);

        if (NM_IS_DEVICE_WIFI (self->device))
                active_ap = nm_device_wifi_get_active_access_point (NM_DEVICE_WIFI (self->device));
        else
                active_ap = NULL;

        state = self->device ? nm_device_get_state (self->device) : NM_DEVICE_STATE_DISCONNECTED;

        device_is_active = FALSE;
        speed = 0;
        if (active_ap && self->ap == active_ap && state != NM_DEVICE_STATE_UNAVAILABLE) {
                device_is_active = TRUE;
                if (NM_IS_DEVICE_WIFI (self->device))
                        speed = nm_device_wifi_get_bitrate (NM_DEVICE_WIFI (self->device)) / 1000;
        } else if (self->device) {
                NMActiveConnection *ac;
                const gchar *p1, *p2;

                ac = nm_device_get_active_connection (self->device);
                p1 = ac ? nm_active_connection_get_uuid (ac) : NULL;
                p2 = nm_connection_get_uuid (CE_PAGE (self)->connection);
                if (g_strcmp0 (p1, p2) == 0) {
                        device_is_active = TRUE;
                        if (NM_IS_DEVICE_WIFI (self->device))
                                speed = nm_device_wifi_get_bitrate (NM_DEVICE_WIFI (self->device)) / 1000;
                        else if (NM_IS_DEVICE_ETHERNET (self->device))
                                speed = nm_device_ethernet_get_speed (NM_DEVICE_ETHERNET (self->device));
                }
        }
        if (speed > 0)
                speed_label = g_strdup_printf (_("%d Mb/s"), speed);
        gtk_label_set_label (self->speed_label, speed_label);
        gtk_widget_set_visible (GTK_WIDGET (self->speed_heading_label), speed_label != NULL);
        gtk_widget_set_visible (GTK_WIDGET (self->speed_label), speed_label != NULL);

        if (NM_IS_DEVICE_WIFI (self->device))
                hw_address = nm_device_wifi_get_hw_address (NM_DEVICE_WIFI (self->device));
        else if (NM_IS_DEVICE_ETHERNET (self->device))
                hw_address = nm_device_ethernet_get_hw_address (NM_DEVICE_ETHERNET (self->device));

        gtk_label_set_label (self->mac_label, hw_address);
        gtk_widget_set_visible (GTK_WIDGET (self->mac_heading_label), hw_address != NULL);
        gtk_widget_set_visible (GTK_WIDGET (self->mac_label), hw_address != NULL);

        if (device_is_active && active_ap)
                security_string = get_ap_security_string (active_ap);
        gtk_label_set_label (self->security_label, security_string);
        gtk_widget_set_visible (GTK_WIDGET (self->security_heading_label), security_string != NULL);
        gtk_widget_set_visible (GTK_WIDGET (self->security_label), security_string != NULL);

        strength = 0;
        if (self->ap != NULL)
                strength = nm_access_point_get_strength (self->ap);

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
        gtk_label_set_label (self->strength_label, strength_label);
        gtk_widget_set_visible (GTK_WIDGET (self->strength_heading_label), strength_label != NULL);
        gtk_widget_set_visible (GTK_WIDGET (self->strength_label), strength_label != NULL);

        if (device_is_active && self->device != NULL)
                ipv4_config = nm_device_get_ip4_config (self->device);
        if (ipv4_config != NULL) {
                GPtrArray *addresses;
                const gchar *ipv4_text = NULL;
                g_autofree gchar *dns_text = NULL;
                const gchar *route_text;

                addresses = nm_ip_config_get_addresses (ipv4_config);
                if (addresses->len > 0)
                        ipv4_text = nm_ip_address_get_address (g_ptr_array_index (addresses, 0));
                gtk_label_set_label (self->ipv4_label, ipv4_text);
                gtk_widget_set_visible (GTK_WIDGET (self->ipv4_heading_label), ipv4_text != NULL);
                gtk_widget_set_visible (GTK_WIDGET (self->ipv4_label), ipv4_text != NULL);
                have_ipv4_address = ipv4_text != NULL;

                dns_text = g_strjoinv (" ", (char **) nm_ip_config_get_nameservers (ipv4_config));
                gtk_label_set_label (self->dns_label, dns_text);
                gtk_widget_set_visible (GTK_WIDGET (self->dns_heading_label), dns_text != NULL);
                gtk_widget_set_visible (GTK_WIDGET (self->dns_label), dns_text != NULL);

                route_text = nm_ip_config_get_gateway (ipv4_config);
                gtk_label_set_label (self->route_label, route_text);
                gtk_widget_set_visible (GTK_WIDGET (self->route_heading_label), route_text != NULL);
                gtk_widget_set_visible (GTK_WIDGET (self->route_label), route_text != NULL);
        } else {
                gtk_widget_hide (GTK_WIDGET (self->ipv4_heading_label));
                gtk_widget_hide (GTK_WIDGET (self->ipv4_label));
                gtk_widget_hide (GTK_WIDGET (self->dns_heading_label));
                gtk_widget_hide (GTK_WIDGET (self->dns_label));
                gtk_widget_hide (GTK_WIDGET (self->route_heading_label));
                gtk_widget_hide (GTK_WIDGET (self->route_label));
        }

        if (device_is_active && self->device != NULL)
                ipv6_config = nm_device_get_ip6_config (self->device);
        if (ipv6_config != NULL) {
                GPtrArray *addresses;
                const gchar *ipv6_text = NULL;

                addresses = nm_ip_config_get_addresses (ipv6_config);
                if (addresses->len > 0)
                        ipv6_text = nm_ip_address_get_address (g_ptr_array_index (addresses, 0));
                gtk_label_set_label (self->ipv6_label, ipv6_text);
                gtk_widget_set_visible (GTK_WIDGET (self->ipv6_heading_label), ipv6_text != NULL);
                gtk_widget_set_visible (GTK_WIDGET (self->ipv6_label), ipv6_text != NULL);
                have_ipv6_address = ipv6_text != NULL;
        } else {
                gtk_widget_hide (GTK_WIDGET (self->ipv6_heading_label));
                gtk_widget_hide (GTK_WIDGET (self->ipv6_label));
        }

        if (have_ipv4_address && have_ipv6_address) {
                gtk_label_set_label (self->ipv4_heading_label, _("IPv4 Address"));
                gtk_label_set_label (self->ipv6_heading_label, _("IPv6 Address"));
        }
        else {
                gtk_label_set_label (self->ipv4_heading_label, _("IP Address"));
                gtk_label_set_label (self->ipv6_heading_label, _("IP Address"));
        }

        if (!device_is_active && CE_PAGE (self)->connection)
                update_last_used (self, CE_PAGE (self)->connection);
        else {
                gtk_widget_set_visible (GTK_WIDGET (self->last_used_heading_label), FALSE);
                gtk_widget_set_visible (GTK_WIDGET (self->last_used_label), FALSE);
        }

        /* Auto connect check */
        if (g_str_equal (type, NM_SETTING_VPN_SETTING_NAME)) {
                gtk_widget_hide (GTK_WIDGET (self->auto_connect_check));
        } else {
                g_object_bind_property (sc, "autoconnect",
                                        self->auto_connect_check, "active",
                                        G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE);
                g_signal_connect_swapped (self->auto_connect_check, "toggled", G_CALLBACK (ce_page_changed), self);
        }

        /* All users check */
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (self->all_user_check),
                                      nm_setting_connection_get_num_permissions (sc) == 0);
        g_signal_connect_swapped (self->all_user_check, "toggled",
                                  G_CALLBACK (all_user_changed), self);
        g_signal_connect_swapped (self->all_user_check, "toggled", G_CALLBACK (ce_page_changed), self);

        /* Restrict Data check */
        update_restrict_data (self);

        /* Forget button */
        g_signal_connect_swapped (self->forget_button, "clicked", G_CALLBACK (forget_cb), self);

        if (g_str_equal (type, NM_SETTING_WIRELESS_SETTING_NAME))
                gtk_button_set_label (self->forget_button, _("Forget Connection"));
        else if (g_str_equal (type, NM_SETTING_WIRED_SETTING_NAME))
                gtk_button_set_label (self->forget_button, _("Remove Connection Profile"));
        else if (g_str_equal (type, NM_SETTING_VPN_SETTING_NAME))
                gtk_button_set_label (self->forget_button, _("Remove VPN"));
        else
                gtk_widget_hide (GTK_WIDGET (self->forget_button));
}

static const gchar *
ce_page_details_get_title (CEPage *page)
{
        return _("Details");
}

static void
ce_page_details_init (CEPageDetails *self)
{
}

static void
ce_page_details_class_init (CEPageDetailsClass *class)
{
        CEPageClass *page_class = CE_PAGE_CLASS (class);

        page_class->get_title = ce_page_details_get_title;
}

CEPage *
ce_page_details_new (NMConnection        *connection,
                     NMClient            *client,
                     NMDevice            *device,
                     NMAccessPoint       *ap,
                     NetConnectionEditor *editor)
{
        CEPageDetails *self;

        self = CE_PAGE_DETAILS (ce_page_new (CE_TYPE_PAGE_DETAILS,
                                             connection,
                                             client,
                                             "/org/gnome/control-center/network/details-page.ui"));

        self->all_user_check = GTK_CHECK_BUTTON (gtk_builder_get_object (CE_PAGE (self)->builder, "all_user_check"));
        self->auto_connect_check = GTK_CHECK_BUTTON (gtk_builder_get_object (CE_PAGE (self)->builder, "auto_connect_check"));
        self->dns_heading_label = GTK_LABEL (gtk_builder_get_object (CE_PAGE (self)->builder, "dns_heading_label"));
        self->dns_label = GTK_LABEL (gtk_builder_get_object (CE_PAGE (self)->builder, "dns_label"));
        self->forget_button = GTK_BUTTON (gtk_builder_get_object (CE_PAGE (self)->builder, "forget_button"));
        self->ipv4_heading_label = GTK_LABEL (gtk_builder_get_object (CE_PAGE (self)->builder, "ipv4_heading_label"));
        self->ipv4_label = GTK_LABEL (gtk_builder_get_object (CE_PAGE (self)->builder, "ipv4_label"));
        self->ipv6_heading_label = GTK_LABEL (gtk_builder_get_object (CE_PAGE (self)->builder, "ipv6_heading_label"));
        self->ipv6_label = GTK_LABEL (gtk_builder_get_object (CE_PAGE (self)->builder, "ipv6_label"));
        self->last_used_heading_label = GTK_LABEL (gtk_builder_get_object (CE_PAGE (self)->builder, "last_used_heading_label"));
        self->last_used_label = GTK_LABEL (gtk_builder_get_object (CE_PAGE (self)->builder, "last_used_label"));
        self->mac_heading_label = GTK_LABEL (gtk_builder_get_object (CE_PAGE (self)->builder, "mac_heading_label"));
        self->mac_label = GTK_LABEL (gtk_builder_get_object (CE_PAGE (self)->builder, "mac_label"));
        self->restrict_data_check = GTK_CHECK_BUTTON (gtk_builder_get_object (CE_PAGE (self)->builder, "restrict_data_check"));
        self->route_heading_label = GTK_LABEL (gtk_builder_get_object (CE_PAGE (self)->builder, "route_heading_label"));
        self->route_label = GTK_LABEL (gtk_builder_get_object (CE_PAGE (self)->builder, "route_label"));
        self->security_heading_label = GTK_LABEL (gtk_builder_get_object (CE_PAGE (self)->builder, "security_heading_label"));
        self->security_label = GTK_LABEL (gtk_builder_get_object (CE_PAGE (self)->builder, "security_label"));
        self->speed_heading_label = GTK_LABEL (gtk_builder_get_object (CE_PAGE (self)->builder, "speed_heading_label"));
        self->speed_label = GTK_LABEL (gtk_builder_get_object (CE_PAGE (self)->builder, "speed_label"));
        self->strength_heading_label = GTK_LABEL (gtk_builder_get_object (CE_PAGE (self)->builder, "strength_heading_label"));
        self->strength_label = GTK_LABEL (gtk_builder_get_object (CE_PAGE (self)->builder, "strength_label"));

        self->editor = editor;
        self->device = device;
        self->ap = ap;

        connect_details_page (self);

        return CE_PAGE (self);
}
