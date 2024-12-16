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

#include "ce-page-security.h"
#include "ce-page.h"
#include "nma-ws.h"

struct _CEPageSecurity {
    AdwPreferencesPage parent;

    AdwPreferencesGroup *group;
    AdwComboRow *security_combo;

    NMConnection *connection;
    const gchar *security_setting;
    gboolean adhoc;
    NMAWs *current_sec;
};

static void ce_page_iface_init (CEPageInterface *);

G_DEFINE_FINAL_TYPE_WITH_CODE (CEPageSecurity, ce_page_security, ADW_TYPE_PREFERENCES_PAGE,
                               G_IMPLEMENT_INTERFACE (CE_TYPE_PAGE, ce_page_iface_init))

static gboolean
find_proto (NMSettingWirelessSecurity *sec, const char *item)
{
    guint32 i;

    for (i = 0; i < nm_setting_wireless_security_get_num_protos (sec); i++) {
        if (!strcmp (item, nm_setting_wireless_security_get_proto (sec, i)))
            return TRUE;
    }
    return FALSE;
}

static NMUtilsSecurityType
get_default_type_for_security (NMSettingWirelessSecurity *sec)
{
    const char *key_mgmt, *auth_alg;

    g_return_val_if_fail (sec != NULL, NMU_SEC_NONE);

    key_mgmt = nm_setting_wireless_security_get_key_mgmt (sec);
    auth_alg = nm_setting_wireless_security_get_auth_alg (sec);

    if (!strcmp (key_mgmt, "ieee8021x")) {
        if (auth_alg && !strcmp (auth_alg, "leap"))
            return NMU_SEC_LEAP;
        return NMU_SEC_INVALID;
    }

#if NM_CHECK_VERSION(1, 24, 0)
    if (!strcmp (key_mgmt, "owe")) {
        return NMU_SEC_OWE;
    }
#endif

#if NM_CHECK_VERSION(1, 20, 6)
    if (!strcmp (key_mgmt, "sae")) {
        return NMU_SEC_SAE;
    }
#endif

    if (!strcmp (key_mgmt, "wpa-psk")) {
        if (find_proto (sec, "rsn"))
            return NMU_SEC_WPA2_PSK;
        else if (find_proto (sec, "wpa"))
            return NMU_SEC_WPA_PSK;
        else
            return NMU_SEC_WPA_PSK;
    }

    if (!strcmp (key_mgmt, "wpa-eap")) {
        if (find_proto (sec, "rsn"))
            return NMU_SEC_WPA2_ENTERPRISE;
        else if (find_proto (sec, "wpa"))
            return NMU_SEC_WPA_ENTERPRISE;
        else
            return NMU_SEC_WPA_ENTERPRISE;
    }

    return NMU_SEC_INVALID;
}

static NMAWs *
security_combo_get_selected_sec (CEPageSecurity *self)
{
    GObject *object;

    object = adw_combo_row_get_selected_item (self->security_combo);
    if (object == NULL)
        return NULL;

    return g_object_get_data (object, "sec");
}

static void
security_combo_changed (CEPageSecurity *self)
{
    if (self->current_sec != NULL) {
        gtk_widget_remove_css_class (GTK_WIDGET (self->current_sec), "security-config");
        adw_preferences_group_remove (self->group, GTK_WIDGET (self->current_sec));
    }

    self->current_sec = security_combo_get_selected_sec (self);
    if (self->current_sec != NULL) {
        gtk_widget_add_css_class (GTK_WIDGET (self->current_sec), "security-config");
        adw_preferences_group_add (self->group, GTK_WIDGET (self->current_sec));
    }

    ce_page_changed (CE_PAGE (self));
}

static void
security_item_changed_cb (CEPageSecurity *self)
{
    ce_page_changed (CE_PAGE (self));
}

static void
add_security_item (CEPageSecurity *self, NMAWs *sec, GListStore *model, const char *text, gboolean adhoc_valid)
{
    g_autoptr (GtkStringObject) object = NULL;

    if (G_IS_INITIALLY_UNOWNED (sec))
        g_object_ref_sink (sec);
    g_signal_connect_object (sec, "ws-changed", G_CALLBACK (security_item_changed_cb), self, G_CONNECT_SWAPPED);

    object = gtk_string_object_new (text);
    g_object_set_data_full (G_OBJECT (object), "sec", g_steal_pointer (&sec), g_object_unref);
    g_object_set_data (G_OBJECT (object), "adhoc-valid", GUINT_TO_POINTER (adhoc_valid));
    g_list_store_append (model, object);
}

static void
finish_setup (CEPageSecurity *self)
{
    NMSettingWireless *sw;
    NMSettingWirelessSecurity *sws;
    gboolean is_adhoc = FALSE;
    g_autoptr (GListStore) sec_model = NULL;
    const gchar *mode;
    guint32 dev_caps = 0;
    NMUtilsSecurityType default_type = NMU_SEC_NONE;
    guint active = GTK_INVALID_LIST_POSITION;
    guint item = 0;

    sw = nm_connection_get_setting_wireless (self->connection);
    g_assert (sw);

    dev_caps = NM_WIFI_DEVICE_CAP_CIPHER_TKIP | NM_WIFI_DEVICE_CAP_CIPHER_CCMP | NM_WIFI_DEVICE_CAP_WPA
               | NM_WIFI_DEVICE_CAP_RSN;

    mode = nm_setting_wireless_get_mode (sw);
    if (mode && !strcmp (mode, "adhoc"))
        is_adhoc = TRUE;
    self->adhoc = is_adhoc;

    sws = nm_connection_get_setting_wireless_security (self->connection);
    if (sws)
        default_type = get_default_type_for_security (sws);

    sec_model = g_list_store_new (GTK_TYPE_STRING_OBJECT);

    if (nm_utils_security_valid (NMU_SEC_NONE, dev_caps, FALSE, is_adhoc, 0, 0, 0)) {
        g_autoptr (GtkStringObject) object = gtk_string_object_new (C_("Wi-Fi/Ethernet security", "None"));

        g_object_set_data (G_OBJECT (object), "adhoc-valid", GUINT_TO_POINTER (TRUE));
        g_list_store_append (sec_model, object);
        if (default_type == NMU_SEC_NONE)
            active = item;
        item++;
    }

#if NM_CHECK_VERSION(1, 24, 0)
    if (nm_utils_security_valid (NMU_SEC_OWE, dev_caps, FALSE, is_adhoc, 0, 0, 0)) {
        g_autoptr (GtkStringObject) object = gtk_string_object_new (_("Enhanced Open"));

        g_object_set_data (G_OBJECT (object), "adhoc-valid", GUINT_TO_POINTER (FALSE));
        g_list_store_append (sec_model, object);
        if (active == GTK_INVALID_LIST_POSITION && default_type == NMU_SEC_OWE)
            active = item;
        item++;
    }
#endif

    if (nm_utils_security_valid (NMU_SEC_LEAP, dev_caps, FALSE, is_adhoc, 0, 0, 0)) {
        NMAWsLeap *ws_leap;

        ws_leap = nma_ws_leap_new (self->connection, FALSE);
        if (ws_leap) {
            add_security_item (self, NMA_WS (ws_leap), sec_model, _("LEAP"), FALSE);
            if ((active == GTK_INVALID_LIST_POSITION) && (default_type == NMU_SEC_LEAP))
                active = item;
            item++;
        }
    }

    if (nm_utils_security_valid (NMU_SEC_WPA_PSK, dev_caps, FALSE, is_adhoc, 0, 0, 0)
        || nm_utils_security_valid (NMU_SEC_WPA2_PSK, dev_caps, FALSE, is_adhoc, 0, 0, 0)) {
        NMAWsWpaPsk *ws_wpa_psk;

        ws_wpa_psk = nma_ws_wpa_psk_new (self->connection, FALSE);
        if (ws_wpa_psk) {
            add_security_item (self, NMA_WS (ws_wpa_psk), sec_model, _("WPA & WPA2 Personal"), FALSE);
            if ((active == GTK_INVALID_LIST_POSITION)
                && ((default_type == NMU_SEC_WPA_PSK) || (default_type == NMU_SEC_WPA2_PSK)))
                active = item;
            item++;
        }
    }

    if (nm_utils_security_valid (NMU_SEC_WPA_ENTERPRISE, dev_caps, FALSE, is_adhoc, 0, 0, 0)
        || nm_utils_security_valid (NMU_SEC_WPA2_ENTERPRISE, dev_caps, FALSE, is_adhoc, 0, 0, 0)) {
        NMAWsWpaEap *ws_wpa_eap;

        ws_wpa_eap = nma_ws_wpa_eap_new (self->connection, TRUE, FALSE, NULL);
        if (ws_wpa_eap) {
            add_security_item (self, NMA_WS (ws_wpa_eap), sec_model, _("WPA & WPA2 Enterprise"), FALSE);
            if ((active == GTK_INVALID_LIST_POSITION)
                && ((default_type == NMU_SEC_WPA_ENTERPRISE) || (default_type == NMU_SEC_WPA2_ENTERPRISE)))
                active = item;
            item++;
        }
    }

#if NM_CHECK_VERSION(1, 20, 6)
    if (nm_utils_security_valid (NMU_SEC_SAE, dev_caps, FALSE, is_adhoc, 0, 0, 0)) {
        NMAWsSae *ws_sae;

        ws_sae = nma_ws_sae_new (self->connection, FALSE);
        if (ws_sae) {
            add_security_item (self, NMA_WS (ws_sae), sec_model, _("WPA3 Personal"), FALSE);
            if ((active == GTK_INVALID_LIST_POSITION) && ((default_type == NMU_SEC_SAE)))
                active = item;
            item++;
        }
    }
#endif

    adw_combo_row_set_model (self->security_combo, G_LIST_MODEL (sec_model));
    adw_combo_row_set_selected (self->security_combo, active == GTK_INVALID_LIST_POSITION ? 0 : (guint32) active);

    security_combo_changed (self);
    g_signal_connect_object (self->security_combo, "notify::selected", G_CALLBACK (security_combo_changed), self,
                             G_CONNECT_SWAPPED);
}

static void
ce_page_security_dispose (GObject *object)
{
    CEPageSecurity *self = CE_PAGE_SECURITY (object);

    g_clear_object (&self->connection);

    G_OBJECT_CLASS (ce_page_security_parent_class)->dispose (object);
}

static const gchar *
ce_page_security_get_security_setting (CEPage *page)
{
    return CE_PAGE_SECURITY (page)->security_setting;
}

static gboolean
ce_page_security_validate (CEPage *page, NMConnection *connection, GError **error)
{
    CEPageSecurity *self = CE_PAGE_SECURITY (page);
    NMSettingWireless *sw;
    NMAWs *sec;
    gboolean valid = FALSE;
    const char *mode;

    sw = nm_connection_get_setting_wireless (connection);

    mode = nm_setting_wireless_get_mode (sw);
    if (g_strcmp0 (mode, NM_SETTING_WIRELESS_MODE_ADHOC) == 0)
        CE_PAGE_SECURITY (self)->adhoc = TRUE;
    else
        CE_PAGE_SECURITY (self)->adhoc = FALSE;

    sec = security_combo_get_selected_sec (CE_PAGE_SECURITY (self));
    if (sec) {
        GBytes *ssid = nm_setting_wireless_get_ssid (sw);

        if (ssid) {
            /* FIXME: get failed property and error out of wifi security objects */
            valid = nma_ws_validate (sec, error);
            if (valid)
                nma_ws_fill_connection (sec, connection);
        } else {
            g_set_error (error, NM_CONNECTION_ERROR, NM_CONNECTION_ERROR_MISSING_SETTING, "Missing SSID");
            valid = FALSE;
        }

        if (self->adhoc) {
            if (!nma_ws_adhoc_compatible (sec)) {
                if (valid)
                    g_set_error (error, NM_CONNECTION_ERROR, NM_CONNECTION_ERROR_INVALID_SETTING,
                                 "Security not compatible with Ad-Hoc mode");
                valid = FALSE;
            }
        }
    } else {

        if (adw_combo_row_get_selected ((CE_PAGE_SECURITY (self))->security_combo) == 0) {
            /* No security, unencrypted */
            nm_connection_remove_setting (connection, NM_TYPE_SETTING_WIRELESS_SECURITY);
            nm_connection_remove_setting (connection, NM_TYPE_SETTING_802_1X);
            valid = TRUE;
        } else {
            /* owe case:
             * fill the connection manually until libnma implements OWE wireless security
             */
            NMSetting *sws;

            sws = nm_setting_wireless_security_new ();
            g_object_set (sws, NM_SETTING_WIRELESS_SECURITY_KEY_MGMT, "owe", NULL);
            nm_connection_add_setting (connection, sws);
            nm_connection_remove_setting (connection, NM_TYPE_SETTING_802_1X);
            valid = TRUE;
        }
    }

    return valid;
}

static void
ce_page_security_init (CEPageSecurity *self)
{
    g_autoptr (GtkCssProvider) provider = NULL;

    gtk_widget_init_template (GTK_WIDGET (self));

    provider = gtk_css_provider_new ();
    gtk_css_provider_load_from_resource (provider, "/org/gnome/control-center/network/ce-page-security.css");
    gtk_style_context_add_provider_for_display (gdk_display_get_default (), GTK_STYLE_PROVIDER (provider),
                                                GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
}

static void
ce_page_security_class_init (CEPageSecurityClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

    object_class->dispose = ce_page_security_dispose;

    gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/network/ce-page-security.ui");

    gtk_widget_class_bind_template_child (widget_class, CEPageSecurity, group);
    gtk_widget_class_bind_template_child (widget_class, CEPageSecurity, security_combo);
}

static void
ce_page_iface_init (CEPageInterface *iface)
{
    iface->get_security_setting = ce_page_security_get_security_setting;
    iface->get_title = (const char *(*) (CEPage *) ) adw_preferences_page_get_title;
    iface->validate = ce_page_security_validate;
}

CEPageSecurity *
ce_page_security_new (NMConnection *connection)
{
    CEPageSecurity *self;
    NMUtilsSecurityType default_type = NMU_SEC_NONE;
    NMSettingWirelessSecurity *sws;

    self = g_object_new (CE_TYPE_PAGE_SECURITY, NULL);

    self->connection = g_object_ref (connection);

    sws = nm_connection_get_setting_wireless_security (connection);
    if (sws)
        default_type = get_default_type_for_security (sws);

    if (default_type == NMU_SEC_LEAP || default_type == NMU_SEC_WPA_PSK ||
#if NM_CHECK_VERSION(1, 20, 6)
        default_type == NMU_SEC_SAE ||
#endif
#if NM_CHECK_VERSION(1, 24, 0)
        default_type == NMU_SEC_OWE ||
#endif
        default_type == NMU_SEC_WPA2_PSK) {
        self->security_setting = NM_SETTING_WIRELESS_SECURITY_SETTING_NAME;
    }

    if (default_type == NMU_SEC_WPA_ENTERPRISE || default_type == NMU_SEC_WPA2_ENTERPRISE) {
        self->security_setting = NM_SETTING_802_1X_SETTING_NAME;
    }

    g_signal_connect (self, "initialized", G_CALLBACK (finish_setup), NULL);

    return self;
}
