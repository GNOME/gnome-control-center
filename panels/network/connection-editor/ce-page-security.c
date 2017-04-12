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

#include "wireless-security.h"
#include "ce-page-security.h"

G_DEFINE_TYPE (CEPageSecurity, ce_page_security, CE_TYPE_PAGE)

enum {
        S_NAME_COLUMN,
        S_SEC_COLUMN,
        S_ADHOC_VALID_COLUMN
};

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

        /* No IEEE 802.1x */
        if (!strcmp (key_mgmt, "none"))
                return NMU_SEC_STATIC_WEP;

        if (!strcmp (key_mgmt, "ieee8021x")) {
                if (auth_alg && !strcmp (auth_alg, "leap"))
                        return NMU_SEC_LEAP;
                return NMU_SEC_DYNAMIC_WEP;
        }

        if (   !strcmp (key_mgmt, "wpa-none")
            || !strcmp (key_mgmt, "wpa-psk")) {
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

static WirelessSecurity *
security_combo_get_active (CEPageSecurity *page)
{
        GtkTreeIter iter;
        GtkTreeModel *model;
        WirelessSecurity *sec = NULL;

        model = gtk_combo_box_get_model (page->security_combo);
        gtk_combo_box_get_active_iter (page->security_combo, &iter);
        gtk_tree_model_get (model, &iter, S_SEC_COLUMN, &sec, -1);

        return sec;
}

static void
wsec_size_group_clear (GtkSizeGroup *group)
{
        GSList *children;
        GSList *iter;

        g_return_if_fail (group != NULL);

        children = gtk_size_group_get_widgets (group);
        for (iter = children; iter; iter = g_slist_next (iter))
                gtk_size_group_remove_widget (group, GTK_WIDGET (iter->data));
}

static void
security_combo_changed (GtkComboBox *combo,
                        gpointer     user_data)
{
        CEPageSecurity *page = CE_PAGE_SECURITY (user_data);
        GtkWidget *vbox;
        GList *l, *children;
        WirelessSecurity *sec;

        wsec_size_group_clear (page->group);

        vbox = GTK_WIDGET (gtk_builder_get_object (CE_PAGE (page)->builder, "vbox"));
        children = gtk_container_get_children (GTK_CONTAINER (vbox));
        for (l = children; l; l = l->next) {
                gtk_container_remove (GTK_CONTAINER (vbox), GTK_WIDGET (l->data));
        }

        sec = security_combo_get_active (page);
        if (sec) {
                GtkWidget *sec_widget;
                GtkWidget *parent;

                sec_widget = wireless_security_get_widget (sec);
                g_assert (sec_widget);
                parent = gtk_widget_get_parent (sec_widget);
                if (parent)
                        gtk_container_remove (GTK_CONTAINER (parent), sec_widget);

                gtk_size_group_add_widget (page->group, page->security_heading);
                wireless_security_add_to_size_group (sec, page->group);

                gtk_container_add (GTK_CONTAINER (vbox), sec_widget);
                wireless_security_unref (sec);
        }

        ce_page_changed (CE_PAGE (page));
}

static void
stuff_changed_cb (WirelessSecurity *sec, gpointer user_data)
{
        ce_page_changed (CE_PAGE (user_data));
}

static void
add_security_item (CEPageSecurity   *page,
                   WirelessSecurity *sec,
                   GtkListStore     *model,
                   GtkTreeIter      *iter,
                   const char       *text,
                   gboolean          adhoc_valid)
{
        wireless_security_set_changed_notify (sec, stuff_changed_cb, page);
        gtk_list_store_append (model, iter);
        gtk_list_store_set (model, iter,
                            S_NAME_COLUMN, text,
                            S_SEC_COLUMN, sec,
                            S_ADHOC_VALID_COLUMN, adhoc_valid,
                            -1);
        wireless_security_unref (sec);
}

static void
set_sensitive (GtkCellLayout *cell_layout,
               GtkCellRenderer *cell,
               GtkTreeModel *tree_model,
               GtkTreeIter *iter,
               gpointer data)
{
        gboolean *adhoc = data;
        gboolean sensitive = TRUE, adhoc_valid = TRUE;

        gtk_tree_model_get (tree_model, iter, S_ADHOC_VALID_COLUMN, &adhoc_valid, -1);
        if (*adhoc && !adhoc_valid)
                sensitive = FALSE;

        g_object_set (cell, "sensitive", sensitive, NULL);
}

static void
finish_setup (CEPageSecurity *page)
{
        NMConnection *connection = CE_PAGE (page)->connection;
        NMSettingWireless *sw;
        NMSettingWirelessSecurity *sws;
        gboolean is_adhoc = FALSE;
        GtkListStore *sec_model;
        GtkTreeIter iter;
        const gchar *mode;
        guint32 dev_caps = 0;
        NMUtilsSecurityType default_type = NMU_SEC_NONE;
        int active = -1;
        int item = 0;
        GtkComboBox *combo;
        GtkCellRenderer *renderer;

        sw = nm_connection_get_setting_wireless (connection);
        g_assert (sw);

        page->group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);

        page->security_heading = GTK_WIDGET (gtk_builder_get_object (CE_PAGE (page)->builder, "heading_sec"));
        page->security_combo = combo = GTK_COMBO_BOX (gtk_builder_get_object (CE_PAGE (page)->builder, "combo_sec"));

        dev_caps =   NM_WIFI_DEVICE_CAP_CIPHER_WEP40
                   | NM_WIFI_DEVICE_CAP_CIPHER_WEP104
                   | NM_WIFI_DEVICE_CAP_CIPHER_TKIP
                   | NM_WIFI_DEVICE_CAP_CIPHER_CCMP
                   | NM_WIFI_DEVICE_CAP_WPA
                   | NM_WIFI_DEVICE_CAP_RSN;

        mode = nm_setting_wireless_get_mode (sw);
        if (mode && !strcmp (mode, "adhoc"))
                is_adhoc = TRUE;
        page->adhoc = is_adhoc;

        sws = nm_connection_get_setting_wireless_security (connection);
        if (sws)
                default_type = get_default_type_for_security (sws);

        sec_model = gtk_list_store_new (3, G_TYPE_STRING, wireless_security_get_type (), G_TYPE_BOOLEAN);

        if (nm_utils_security_valid (NMU_SEC_NONE, dev_caps, FALSE, is_adhoc, 0, 0, 0)) {
                gtk_list_store_insert_with_values (sec_model, &iter, -1,
                                                   S_NAME_COLUMN, C_("Wi-Fi/Ethernet security", "None"),
                                                   S_ADHOC_VALID_COLUMN, TRUE,
                                                   -1);
                if (default_type == NMU_SEC_NONE)
                        active = item;
                item++;
        }

        if (nm_utils_security_valid (NMU_SEC_STATIC_WEP, dev_caps, FALSE, is_adhoc, 0, 0, 0)) {
                WirelessSecurityWEPKey *ws_wep;
                NMWepKeyType wep_type = NM_WEP_KEY_TYPE_KEY;

                if (default_type == NMU_SEC_STATIC_WEP) {
                        sws = nm_connection_get_setting_wireless_security (connection);
                        if (sws)
                                wep_type = nm_setting_wireless_security_get_wep_key_type (sws);
                        if (wep_type == NM_WEP_KEY_TYPE_UNKNOWN)
                                wep_type = NM_WEP_KEY_TYPE_KEY;
                }

                ws_wep = ws_wep_key_new (connection, NM_WEP_KEY_TYPE_KEY, FALSE, FALSE);
                if (ws_wep) {
                        add_security_item (page, WIRELESS_SECURITY (ws_wep), sec_model,
                                           &iter, _("WEP 40/128-bit Key (Hex or ASCII)"),
                                           TRUE);
                        if ((active < 0) && (default_type == NMU_SEC_STATIC_WEP) && (wep_type == NM_WEP_KEY_TYPE_KEY))
                                active = item;
                        item++;
                }

                ws_wep = ws_wep_key_new (connection, NM_WEP_KEY_TYPE_PASSPHRASE, FALSE, FALSE);
                if (ws_wep) {
                        add_security_item (page, WIRELESS_SECURITY (ws_wep), sec_model,
                                           &iter, _("WEP 128-bit Passphrase"), TRUE);
                        if ((active < 0) && (default_type == NMU_SEC_STATIC_WEP) && (wep_type == NM_WEP_KEY_TYPE_PASSPHRASE))
                                active = item;
                        item++;
                }
        }

        if (nm_utils_security_valid (NMU_SEC_LEAP, dev_caps, FALSE, is_adhoc, 0, 0, 0)) {
                WirelessSecurityLEAP *ws_leap;

                ws_leap = ws_leap_new (connection, FALSE);
                if (ws_leap) {
                        add_security_item (page, WIRELESS_SECURITY (ws_leap), sec_model,
                                           &iter, _("LEAP"), FALSE);
                        if ((active < 0) && (default_type == NMU_SEC_LEAP))
                                active = item;
                        item++;
                }
        }

        if (nm_utils_security_valid (NMU_SEC_DYNAMIC_WEP, dev_caps, FALSE, is_adhoc, 0, 0, 0)) {
                WirelessSecurityDynamicWEP *ws_dynamic_wep;

                ws_dynamic_wep = ws_dynamic_wep_new (connection, TRUE, FALSE);
                if (ws_dynamic_wep) {
                        add_security_item (page, WIRELESS_SECURITY (ws_dynamic_wep), sec_model,
                                           &iter, _("Dynamic WEP (802.1x)"), FALSE);
                        if ((active < 0) && (default_type == NMU_SEC_DYNAMIC_WEP))
                                active = item;
                        item++;
                }
        }

        if (nm_utils_security_valid (NMU_SEC_WPA_PSK, dev_caps, FALSE, is_adhoc, 0, 0, 0) ||
            nm_utils_security_valid (NMU_SEC_WPA2_PSK, dev_caps, FALSE, is_adhoc, 0, 0, 0)) {
                WirelessSecurityWPAPSK *ws_wpa_psk;

                ws_wpa_psk = ws_wpa_psk_new (connection, FALSE);
                if (ws_wpa_psk) {
                        add_security_item (page, WIRELESS_SECURITY (ws_wpa_psk), sec_model,
                                           &iter, _("WPA & WPA2 Personal"), FALSE);
                        if ((active < 0) && ((default_type == NMU_SEC_WPA_PSK) || (default_type == NMU_SEC_WPA2_PSK)))
                                active = item;
                        item++;
                }
        }

        if (nm_utils_security_valid (NMU_SEC_WPA_ENTERPRISE, dev_caps, FALSE, is_adhoc, 0, 0, 0) ||
            nm_utils_security_valid (NMU_SEC_WPA2_ENTERPRISE, dev_caps, FALSE, is_adhoc, 0, 0, 0)) {
                WirelessSecurityWPAEAP *ws_wpa_eap;

                ws_wpa_eap = ws_wpa_eap_new (connection, TRUE, FALSE);
                if (ws_wpa_eap) {
                        add_security_item (page, WIRELESS_SECURITY (ws_wpa_eap), sec_model,
                                           &iter, _("WPA & WPA2 Enterprise"), FALSE);
                        if ((active < 0) && ((default_type == NMU_SEC_WPA_ENTERPRISE) || (default_type == NMU_SEC_WPA2_ENTERPRISE)))
                                active = item;
                        item++;
                }
        }

        gtk_combo_box_set_model (combo, GTK_TREE_MODEL (sec_model));
        gtk_cell_layout_clear (GTK_CELL_LAYOUT (combo));

        renderer = gtk_cell_renderer_text_new ();
        gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (combo), renderer, TRUE);
        gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (combo), renderer, "text", S_NAME_COLUMN, NULL);
        gtk_cell_layout_set_cell_data_func (GTK_CELL_LAYOUT (combo), renderer, set_sensitive, &page->adhoc, NULL);

        gtk_combo_box_set_active (combo, active < 0 ? 0 : (guint32) active);
        g_object_unref (G_OBJECT (sec_model));

        page->security_combo = combo;

        security_combo_changed (combo, page);
        g_signal_connect (combo, "changed",
                          G_CALLBACK (security_combo_changed), page);
}

static gboolean
validate (CEPage        *page,
          NMConnection  *connection,
          GError       **error)
{
        NMSettingWireless *sw;
        WirelessSecurity *sec;
        gboolean valid = FALSE;
        const char *mode;

        sw = nm_connection_get_setting_wireless (connection);

        mode = nm_setting_wireless_get_mode (sw);
        if (g_strcmp0 (mode, NM_SETTING_WIRELESS_MODE_ADHOC) == 0)
                CE_PAGE_SECURITY (page)->adhoc = TRUE;
        else
                CE_PAGE_SECURITY (page)->adhoc = FALSE;

        sec = security_combo_get_active (CE_PAGE_SECURITY (page));
        if (sec) {
                GBytes *ssid = nm_setting_wireless_get_ssid (sw);

                if (ssid) {
                        /* FIXME: get failed property and error out of wifi security objects */
                        valid = wireless_security_validate (sec, error);
                        if (valid)
                                wireless_security_fill_connection (sec, connection);
                } else {
                        g_set_error (error, NM_CONNECTION_ERROR, NM_CONNECTION_ERROR_MISSING_SETTING, "Missing SSID");
                        valid = FALSE;
                }

                if (CE_PAGE_SECURITY (page)->adhoc) {
                        if (!wireless_security_adhoc_compatible (sec)) {
                                if (valid)
                                        g_set_error (error, NM_CONNECTION_ERROR, NM_CONNECTION_ERROR_INVALID_SETTING, "Security not compatible with Ad-Hoc mode");
                                valid = FALSE;
                        }
                }

                wireless_security_unref (sec);
        } else {
                /* No security, unencrypted */
                nm_connection_remove_setting (connection, NM_TYPE_SETTING_WIRELESS_SECURITY);
                nm_connection_remove_setting (connection, NM_TYPE_SETTING_802_1X);
                valid = TRUE;
        }

        return valid;
}

static void
ce_page_security_init (CEPageSecurity *page)
{
}

static void
dispose (GObject *object)
{
        CEPageSecurity *page = CE_PAGE_SECURITY (object);

        g_clear_object (&page->group);

        G_OBJECT_CLASS (ce_page_security_parent_class)->dispose (object);
}

static void
ce_page_security_class_init (CEPageSecurityClass *class)
{
        GObjectClass *object_class = G_OBJECT_CLASS (class);
        CEPageClass *page_class = CE_PAGE_CLASS (class);

        object_class->dispose = dispose;
        page_class->validate = validate;
}

CEPage *
ce_page_security_new (NMConnection      *connection,
                      NMClient          *client)
{
        CEPageSecurity *page;
        NMUtilsSecurityType default_type = NMU_SEC_NONE;
        NMSettingWirelessSecurity *sws;

        page = CE_PAGE_SECURITY (ce_page_new (CE_TYPE_PAGE_SECURITY,
                                              connection,
                                              client,
                                              "/org/gnome/control-center/network/security-page.ui",
                                              _("Security")));

        sws = nm_connection_get_setting_wireless_security (connection);
        if (sws)
                default_type = get_default_type_for_security (sws);

        if (default_type == NMU_SEC_STATIC_WEP ||
            default_type == NMU_SEC_LEAP ||
            default_type == NMU_SEC_WPA_PSK ||
            default_type == NMU_SEC_WPA2_PSK) {
                CE_PAGE (page)->security_setting = NM_SETTING_WIRELESS_SECURITY_SETTING_NAME;
        }

        if (default_type == NMU_SEC_DYNAMIC_WEP ||
            default_type == NMU_SEC_WPA_ENTERPRISE ||
            default_type == NMU_SEC_WPA2_ENTERPRISE) {
                CE_PAGE (page)->security_setting = NM_SETTING_802_1X_SETTING_NAME;
        }

        g_signal_connect (page, "initialized", G_CALLBACK (finish_setup), NULL);

        return CE_PAGE (page);
}
