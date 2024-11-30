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

#include <glib/gi18n.h>

#include <NetworkManager.h>

#include "ce-page-details.h"
#include "ce-page.h"

#include "../panel-common.h"

struct _CEPageDetails {
    AdwPreferencesPage parent;

    AdwSwitchRow *all_user_switch;
    AdwSwitchRow *auto_connect_switch;
    AdwActionRow *dns4_row;
    AdwActionRow *dns6_row;
    AdwButtonRow *forget_button;
    AdwActionRow *freq_row;
    AdwActionRow *ipv4_row;
    AdwActionRow *ipv6_row;
    AdwActionRow *last_used_row;
    AdwActionRow *mac_row;
    AdwPreferencesGroup *properties_group;
    AdwSwitchRow *restrict_data_switch;
    AdwActionRow *route_row;
    AdwActionRow *security_row;
    AdwActionRow *speed_row;
    AdwActionRow *strength_row;
    AdwPreferencesGroup *switches_group;

    NMConnection *connection;
    NMDevice *device;
    NMAccessPoint *ap;
    NetConnectionEditor *editor;
    gboolean is_new_connection;
};

static void ce_page_iface_init (CEPageInterface *);

G_DEFINE_FINAL_TYPE_WITH_CODE (CEPageDetails, ce_page_details, ADW_TYPE_PREFERENCES_PAGE,
                               G_IMPLEMENT_INTERFACE (CE_TYPE_PAGE, ce_page_iface_init))

static gboolean
get_has_visible_widgets (GtkWidget **widgets)
{
    for (; widgets != NULL && *widgets != NULL; widgets++)
        if (gtk_widget_get_visible (*widgets))
            return TRUE;

    return FALSE;
}

static void
forget_cb (CEPageDetails *self)
{
    AdwDialog *dialog;
    g_autofree gchar *message = NULL;

    /* Translators: "%s" is the user visible name of the network */
    message = g_strdup_printf (
        _("Saved details for “%s” will be permanently lost. This includes passwords and any network changes."),
          nm_connection_get_id (self->connection));
    dialog = adw_alert_dialog_new (_("Forget Connection?"), message);

    adw_alert_dialog_add_responses (ADW_ALERT_DIALOG (dialog), "cancel", _("_Cancel"), "forget", _("_Forget"), NULL);
    adw_alert_dialog_set_response_appearance (ADW_ALERT_DIALOG (dialog), "forget", ADW_RESPONSE_DESTRUCTIVE);
    adw_alert_dialog_set_default_response (ADW_ALERT_DIALOG (dialog), "cancel");
    adw_alert_dialog_set_close_response (ADW_ALERT_DIALOG (dialog), "cancel");

    g_signal_connect_swapped (dialog, "response::forget", G_CALLBACK (net_connection_editor_forget), self->editor);
    adw_dialog_present (dialog, GTK_WIDGET (self));
}

static gchar *
get_ap_security_string (NMAccessPoint *ap)
{
    NM80211ApSecurityFlags wpa_flags, rsn_flags;
    GString *str;

    wpa_flags = nm_access_point_get_wpa_flags (ap);
    rsn_flags = nm_access_point_get_rsn_flags (ap);

    str = g_string_new ("");
    if (wpa_flags != NM_802_11_AP_SEC_NONE) {
        /* TRANSLATORS: this WPA WiFi security */
        g_string_append_printf (str, "%s, ", _("WPA"));
    }
    if (rsn_flags != NM_802_11_AP_SEC_NONE) {
#if NM_CHECK_VERSION(1, 20, 6)
        if (rsn_flags & NM_802_11_AP_SEC_KEY_MGMT_SAE) {
            /* TRANSLATORS: this WPA3 WiFi security */
            g_string_append_printf (str, "%s, ", _("WPA3"));
        }
#if NM_CHECK_VERSION(1, 24, 0)
        else if (rsn_flags & NM_802_11_AP_SEC_KEY_MGMT_OWE) {
            /* TRANSLATORS: this Enhanced Open WiFi security */
            g_string_append_printf (str, "%s, ", _("Enhanced Open"));
        }
#endif
#if NM_CHECK_VERSION(1, 26, 0)
        else if (rsn_flags & NM_802_11_AP_SEC_KEY_MGMT_OWE_TM) {
            /* Connected to open OWE-TM network. */
        }
#endif
        else
#endif
        {
            /* TRANSLATORS: this WPA WiFi security */
            g_string_append_printf (str, "%s, ", _("WPA2"));
        }
    }
    if ((wpa_flags & NM_802_11_AP_SEC_KEY_MGMT_802_1X) || (rsn_flags & NM_802_11_AP_SEC_KEY_MGMT_802_1X)) {
        /* TRANSLATORS: this Enterprise WiFi security */
        g_string_append_printf (str, "%s, ", _("Enterprise"));
    }
    if (str->len > 0)
        g_string_set_size (str, str->len - 2);
    else {
        g_string_append (str, C_("Wifi security", "None"));
    }
    return g_string_free_and_steal (str);
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

    diff = g_date_time_difference (now, then);
    days = diff / G_TIME_SPAN_DAY;
    if (days == 0)
        last_used = g_strdup (_("Today"));
    else if (days == 1)
        last_used = g_strdup (_("Yesterday"));
    else
        last_used = g_strdup_printf (ngettext ("%i day ago", "%i days ago", days), days);
out:
    adw_action_row_set_subtitle (self->last_used_row, last_used);
    gtk_widget_set_visible (GTK_WIDGET (self->last_used_row), last_used != NULL);
}

static void
all_user_changed (CEPageDetails *self)
{
    gboolean all_users;
    NMSettingConnection *sc;

    sc = nm_connection_get_setting_connection (self->connection);
    all_users = adw_switch_row_get_active (self->all_user_switch);

    g_object_set (sc, "permissions", NULL, NULL);
    if (!all_users)
        nm_setting_connection_add_permission (sc, "user", g_get_user_name (), NULL);
}

static void
restrict_data_changed (CEPageDetails *self)
{
    NMSettingConnection *s_con;
    NMMetered metered;

    s_con = nm_connection_get_setting_connection (self->connection);

    if (adw_switch_row_get_active (self->restrict_data_switch))
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

    s_con = nm_connection_get_setting_connection (self->connection);

    if (s_con == NULL)
        return;

    /* Disable for VPN; NetworkManager does not implement that yet (see
     * bug https://bugzilla.gnome.org/show_bug.cgi?id=792618) */
    type = nm_setting_connection_get_connection_type (s_con);
    if (g_str_equal (type, NM_SETTING_VPN_SETTING_NAME) || g_str_equal (type, NM_SETTING_WIREGUARD_SETTING_NAME)) {
        gtk_widget_set_visible (GTK_WIDGET (self->restrict_data_switch), FALSE);
        return;
    }

    metered = nm_setting_connection_get_metered (s_con);

    adw_switch_row_set_active (self->restrict_data_switch,
                               metered == NM_METERED_YES || metered == NM_METERED_GUESS_YES);

    g_signal_connect_object (self->restrict_data_switch, "notify::active", G_CALLBACK (restrict_data_changed), self,
                             G_CONNECT_SWAPPED);
    g_signal_connect_object (self->restrict_data_switch, "notify::active", G_CALLBACK (ce_page_changed), self,
                             G_CONNECT_SWAPPED);
}

static void
connect_details_page (CEPageDetails *self)
{
    NMSettingConnection *sc;
    guint speed;
    NMDeviceWifiCapabilities wifi_caps;
    guint frequency;
    guint strength;
    NMDeviceState state;
    NMAccessPoint *active_ap;
    g_autofree gchar *speed_label = NULL;
    const gchar *type;
    const gchar *hw_address = NULL;
    g_autofree gchar *security_string = NULL;
    const gchar *freq_string = NULL;
    const gchar *strength_label;
    gboolean device_is_active;
    NMIPConfig *ipv4_config = NULL, *ipv6_config = NULL;
    gboolean have_ipv4_address = FALSE, have_ipv6_address = FALSE;
    gboolean have_dns4 = FALSE, have_dns6 = FALSE;
    const gchar *route4_text = NULL, *route6_text = NULL;
    GtkWidget *properties_group_rows[] = { GTK_WIDGET (self->strength_row),  GTK_WIDGET (self->speed_row),
                                           GTK_WIDGET (self->security_row),  GTK_WIDGET (self->ipv4_row),
                                           GTK_WIDGET (self->ipv6_row),      GTK_WIDGET (self->mac_row),
                                           GTK_WIDGET (self->freq_row),      GTK_WIDGET (self->route_row),
                                           GTK_WIDGET (self->dns4_row),      GTK_WIDGET (self->dns6_row),
                                           GTK_WIDGET (self->last_used_row), NULL };
    GtkWidget *switches_group_rows[] = { GTK_WIDGET (self->auto_connect_switch), GTK_WIDGET (self->all_user_switch),
                                         GTK_WIDGET (self->restrict_data_switch), NULL };

    sc = nm_connection_get_setting_connection (self->connection);
    type = nm_setting_connection_get_connection_type (sc);

    if (NM_IS_DEVICE_WIFI (self->device))
        active_ap = nm_device_wifi_get_active_access_point (NM_DEVICE_WIFI (self->device));
    else
        active_ap = NULL;
    frequency = active_ap ? nm_access_point_get_frequency (active_ap) : 0;

    state = self->device ? nm_device_get_state (self->device) : NM_DEVICE_STATE_DISCONNECTED;

    device_is_active = FALSE;
    speed = 0;
    wifi_caps = 0;
    if (active_ap && self->ap == active_ap && state != NM_DEVICE_STATE_UNAVAILABLE) {
        device_is_active = TRUE;
        if (NM_IS_DEVICE_WIFI (self->device)) {
            speed = nm_device_wifi_get_bitrate (NM_DEVICE_WIFI (self->device)) / 1000;
            wifi_caps = nm_device_wifi_get_capabilities (NM_DEVICE_WIFI (self->device));
        }
    } else if (self->device) {
        NMActiveConnection *ac;
        const gchar *p1, *p2;

        ac = nm_device_get_active_connection (self->device);
        p1 = ac ? nm_active_connection_get_uuid (ac) : NULL;
        p2 = nm_connection_get_uuid (self->connection);
        if (g_strcmp0 (p1, p2) == 0) {
            device_is_active = TRUE;
            if (NM_IS_DEVICE_WIFI (self->device)) {
                speed = nm_device_wifi_get_bitrate (NM_DEVICE_WIFI (self->device)) / 1000;
                wifi_caps = nm_device_wifi_get_capabilities (NM_DEVICE_WIFI (self->device));
            } else if (NM_IS_DEVICE_ETHERNET (self->device))
                speed = nm_device_ethernet_get_speed (NM_DEVICE_ETHERNET (self->device));
        }
    }

    if (speed > 0 && frequency > 0)
        speed_label = g_strdup_printf (_("%d Mb/s (%1.1f GHz)"), speed, (float) (frequency) / 1000.0);
    else if (speed > 0)
        speed_label = g_strdup_printf (_("%d Mb/s"), speed);
    adw_action_row_set_subtitle (self->speed_row, speed_label);
    gtk_widget_set_visible (GTK_WIDGET (self->speed_row), speed_label != NULL);

    if (self->device)
        hw_address = nm_device_get_hw_address (self->device);

    adw_action_row_set_subtitle (self->mac_row, hw_address);
    gtk_widget_set_visible (GTK_WIDGET (self->mac_row), hw_address != NULL);

    if (wifi_caps & NM_WIFI_DEVICE_CAP_FREQ_VALID) {
/* Check 6 GHz support in Network Manager */
#if NM_CHECK_VERSION(1, 45, 4)
        if (wifi_caps & NM_WIFI_DEVICE_CAP_FREQ_2GHZ && wifi_caps & NM_WIFI_DEVICE_CAP_FREQ_5GHZ
            && wifi_caps & NM_WIFI_DEVICE_CAP_FREQ_6GHZ)
            freq_string = _("2.4 GHz / 5 GHz / 6 GHz");
        else if (wifi_caps & NM_WIFI_DEVICE_CAP_FREQ_5GHZ && wifi_caps & NM_WIFI_DEVICE_CAP_FREQ_6GHZ)
            freq_string = _("5 GHz / 6 GHz");
        else
#endif
            if (wifi_caps & NM_WIFI_DEVICE_CAP_FREQ_2GHZ && wifi_caps & NM_WIFI_DEVICE_CAP_FREQ_5GHZ)
            freq_string = _("2.4 GHz / 5 GHz");
        else if (wifi_caps & NM_WIFI_DEVICE_CAP_FREQ_2GHZ)
            freq_string = _("2.4 GHz");
        else if (wifi_caps & NM_WIFI_DEVICE_CAP_FREQ_5GHZ)
            freq_string = _("5 GHz");
    }

    adw_action_row_set_subtitle (self->freq_row, freq_string);
    gtk_widget_set_visible (GTK_WIDGET (self->freq_row), freq_string != NULL);

    if (device_is_active && active_ap)
        security_string = get_ap_security_string (active_ap);
    adw_action_row_set_subtitle (self->security_row, security_string);
    gtk_widget_set_visible (GTK_WIDGET (self->security_row), security_string != NULL);

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
    adw_action_row_set_subtitle (self->strength_row, strength_label);
    gtk_widget_set_visible (GTK_WIDGET (self->strength_row), strength_label != NULL);

    if (device_is_active && self->device != NULL) {
        ipv4_config = nm_device_get_ip4_config (self->device);
        ipv6_config = nm_device_get_ip6_config (self->device);
    }

    if (ipv4_config != NULL) {
        GPtrArray *addresses;
        const gchar *ipv4_text = NULL;
        g_autofree gchar *ip4_dns = NULL;

        addresses = nm_ip_config_get_addresses (ipv4_config);
        if (addresses->len > 0)
            ipv4_text = nm_ip_address_get_address (g_ptr_array_index (addresses, 0));
        adw_action_row_set_subtitle (self->ipv4_row, ipv4_text);
        gtk_widget_set_visible (GTK_WIDGET (self->ipv4_row), ipv4_text != NULL);
        have_ipv4_address = ipv4_text != NULL;

        ip4_dns = g_strjoinv (" ", (char **) nm_ip_config_get_nameservers (ipv4_config));
        if (!*ip4_dns)
            ip4_dns = NULL;
        adw_action_row_set_subtitle (self->dns4_row, ip4_dns);
        gtk_widget_set_visible (GTK_WIDGET (self->dns4_row), ip4_dns != NULL);
        have_dns4 = ip4_dns != NULL;

        route4_text = nm_ip_config_get_gateway (ipv4_config);
    } else {
        gtk_widget_set_visible (GTK_WIDGET (self->ipv4_row), FALSE);
        gtk_widget_set_visible (GTK_WIDGET (self->dns4_row), FALSE);
    }

    if (ipv6_config != NULL) {
        g_autofree gchar *ipv6_text = NULL;
        g_autofree gchar *ip6_dns = NULL;

        ipv6_text = net_device_get_ip6_addresses (ipv6_config);
        adw_action_row_set_subtitle (self->ipv6_row, ipv6_text);
        gtk_widget_set_visible (GTK_WIDGET (self->ipv6_row), ipv6_text != NULL);
        gtk_widget_set_valign (GTK_WIDGET (self->ipv6_row), GTK_ALIGN_START);
        have_ipv6_address = ipv6_text != NULL;

        ip6_dns = g_strjoinv (" ", (char **) nm_ip_config_get_nameservers (ipv6_config));
        if (!*ip6_dns)
            ip6_dns = NULL;
        adw_action_row_set_subtitle (self->dns6_row, ip6_dns);
        gtk_widget_set_visible (GTK_WIDGET (self->dns6_row), ip6_dns != NULL);
        have_dns6 = ip6_dns != NULL;

        route6_text = nm_ip_config_get_gateway (ipv6_config);
    } else {
        gtk_widget_set_visible (GTK_WIDGET (self->ipv6_row), FALSE);
        gtk_widget_set_visible (GTK_WIDGET (self->dns6_row), FALSE);
    }

    if (have_ipv4_address && have_ipv6_address) {
        adw_preferences_row_set_title (ADW_PREFERENCES_ROW (self->ipv4_row), _("IPv4 Address"));
        adw_preferences_row_set_title (ADW_PREFERENCES_ROW (self->ipv6_row), _("IPv6 Address"));
    } else {
        adw_preferences_row_set_title (ADW_PREFERENCES_ROW (self->ipv4_row), _("IP Address"));
        adw_preferences_row_set_title (ADW_PREFERENCES_ROW (self->ipv6_row), _("IP Address"));
    }

    if (have_dns4 && have_dns6) {
        adw_preferences_row_set_title (ADW_PREFERENCES_ROW (self->dns4_row), _("DNS4"));
        adw_preferences_row_set_title (ADW_PREFERENCES_ROW (self->dns6_row), _("DNS6"));
    } else {
        adw_preferences_row_set_title (ADW_PREFERENCES_ROW (self->dns4_row), _("DNS"));
        adw_preferences_row_set_title (ADW_PREFERENCES_ROW (self->dns6_row), _("DNS"));
    }

    if (route4_text != NULL || route6_text != NULL) {
        g_autofree const gchar *routes_text = NULL;

        if (route4_text == NULL) {
            routes_text = g_strdup (route6_text);
        } else if (route6_text == NULL) {
            routes_text = g_strdup (route4_text);
        } else {
            routes_text = g_strjoin ("\n", route4_text, route6_text, NULL);
        }
        adw_action_row_set_subtitle (self->route_row, routes_text);
        gtk_widget_set_visible (GTK_WIDGET (self->route_row), routes_text != NULL);
    } else {
        gtk_widget_set_visible (GTK_WIDGET (self->route_row), FALSE);
    }

    if (!device_is_active && self->connection && !self->is_new_connection)
        update_last_used (self, self->connection);
    else {
        gtk_widget_set_visible (GTK_WIDGET (self->last_used_row), FALSE);
    }

    /* Auto connect check */
    if (g_str_equal (type, NM_SETTING_VPN_SETTING_NAME) || g_str_equal (type, NM_SETTING_WIREGUARD_SETTING_NAME)) {
        gtk_widget_set_visible (GTK_WIDGET (self->auto_connect_switch), FALSE);
    } else {
        g_object_bind_property (sc, "autoconnect", self->auto_connect_switch, "active",
                                G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE);
        g_signal_connect_object (self->auto_connect_switch, "notify::active", G_CALLBACK (ce_page_changed), self,
                                 G_CONNECT_SWAPPED);
    }

    /* All users check */
    adw_switch_row_set_active (self->all_user_switch, nm_setting_connection_get_num_permissions (sc) == 0);
    g_signal_connect_object (self->all_user_switch, "notify::active", G_CALLBACK (all_user_changed), self,
                             G_CONNECT_SWAPPED);
    g_signal_connect_object (self->all_user_switch, "notify::active", G_CALLBACK (ce_page_changed), self,
                             G_CONNECT_SWAPPED);

    /* Restrict Data check */
    update_restrict_data (self);

    /* Forget button */
    if (!self->is_new_connection) {
        g_signal_connect_object (self->forget_button, "activated", G_CALLBACK (forget_cb), self, G_CONNECT_SWAPPED);

        if (g_str_equal (type, NM_SETTING_WIRELESS_SETTING_NAME))
            adw_preferences_row_set_title (ADW_PREFERENCES_ROW (self->forget_button), _("Forget Connection…"));
        else if (g_str_equal (type, NM_SETTING_WIRED_SETTING_NAME))
            adw_preferences_row_set_title (ADW_PREFERENCES_ROW (self->forget_button), _("Remove Connection Profile…"));
        else if (g_str_equal (type, NM_SETTING_BLUETOOTH_SETTING_NAME))
            adw_preferences_row_set_title (ADW_PREFERENCES_ROW (self->forget_button), _("Remove Connection…"));
        else if (g_str_equal (type, NM_SETTING_VPN_SETTING_NAME)
                 || g_str_equal (type, NM_SETTING_WIREGUARD_SETTING_NAME))
            adw_preferences_row_set_title (ADW_PREFERENCES_ROW (self->forget_button), _("Remove VPN…"));
        else
            gtk_widget_set_visible (GTK_WIDGET (self->forget_button), FALSE);
    } else {
        gtk_widget_set_visible (GTK_WIDGET (self->forget_button), FALSE);
    }

    /* Hide empty groups */
    gtk_widget_set_visible (GTK_WIDGET (self->properties_group), get_has_visible_widgets (properties_group_rows));
    gtk_widget_set_visible (GTK_WIDGET (self->switches_group), get_has_visible_widgets (switches_group_rows));
    /* The last group's visibility is handled in the template as it has only one child */
}

static void
ce_page_details_dispose (GObject *object)
{
    CEPageDetails *self = CE_PAGE_DETAILS (object);

    g_clear_object (&self->connection);

    G_OBJECT_CLASS (ce_page_details_parent_class)->dispose (object);
}

static void
ce_page_details_init (CEPageDetails *self)
{
    gtk_widget_init_template (GTK_WIDGET (self));
}

static void
ce_page_details_class_init (CEPageDetailsClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

    object_class->dispose = ce_page_details_dispose;

    gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/network/ce-page-details.ui");

    gtk_widget_class_bind_template_child (widget_class, CEPageDetails, all_user_switch);
    gtk_widget_class_bind_template_child (widget_class, CEPageDetails, auto_connect_switch);
    gtk_widget_class_bind_template_child (widget_class, CEPageDetails, dns4_row);
    gtk_widget_class_bind_template_child (widget_class, CEPageDetails, dns6_row);
    gtk_widget_class_bind_template_child (widget_class, CEPageDetails, forget_button);
    gtk_widget_class_bind_template_child (widget_class, CEPageDetails, freq_row);
    gtk_widget_class_bind_template_child (widget_class, CEPageDetails, ipv4_row);
    gtk_widget_class_bind_template_child (widget_class, CEPageDetails, ipv6_row);
    gtk_widget_class_bind_template_child (widget_class, CEPageDetails, last_used_row);
    gtk_widget_class_bind_template_child (widget_class, CEPageDetails, mac_row);
    gtk_widget_class_bind_template_child (widget_class, CEPageDetails, properties_group);
    gtk_widget_class_bind_template_child (widget_class, CEPageDetails, restrict_data_switch);
    gtk_widget_class_bind_template_child (widget_class, CEPageDetails, route_row);
    gtk_widget_class_bind_template_child (widget_class, CEPageDetails, security_row);
    gtk_widget_class_bind_template_child (widget_class, CEPageDetails, speed_row);
    gtk_widget_class_bind_template_child (widget_class, CEPageDetails, strength_row);
    gtk_widget_class_bind_template_child (widget_class, CEPageDetails, switches_group);
}

static void
ce_page_iface_init (CEPageInterface *iface)
{
    iface->get_title = (const char *(*) (CEPage *) ) adw_preferences_page_get_title;
}

CEPageDetails *
ce_page_details_new (NMConnection *connection, NMDevice *device, NMAccessPoint *ap, NetConnectionEditor *editor,
                     gboolean is_new_connection)
{
    CEPageDetails *self;

    self = g_object_new (CE_TYPE_PAGE_DETAILS, NULL);

    self->connection = g_object_ref (connection);
    self->editor = editor;
    self->device = device;
    self->ap = ap;
    self->is_new_connection = is_new_connection;

    connect_details_page (self);

    return self;
}
