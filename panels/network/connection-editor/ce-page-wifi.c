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

#include <net/if_arp.h>

#include "ce-page-wifi.h"
#include "ui-helpers.h"

G_DEFINE_TYPE (CEPageWifi, ce_page_wifi, CE_TYPE_PAGE)

static void
connect_wifi_page (CEPageWifi *page)
{
        GtkWidget *widget;
        GBytes *ssid;
        gchar *utf8_ssid;
        GPtrArray *bssid_array;
        gchar **bssid_list;
        const char *s_bssid_str;
        gchar **mac_list;
        const gchar *s_mac_str;
        const gchar *cloned_mac;
        gint i;

        widget = GTK_WIDGET (gtk_builder_get_object (CE_PAGE (page)->builder,
                                                     "entry_ssid"));

        ssid = nm_setting_wireless_get_ssid (page->setting);
        if (ssid)
                utf8_ssid = nm_utils_ssid_to_utf8 (g_bytes_get_data (ssid, NULL), g_bytes_get_size (ssid));
        else
                utf8_ssid = g_strdup ("");
        gtk_entry_set_text (GTK_ENTRY (widget), utf8_ssid);
        g_free (utf8_ssid);

        g_signal_connect_swapped (widget, "changed", G_CALLBACK (ce_page_changed), page);

        widget = GTK_WIDGET (gtk_builder_get_object (CE_PAGE (page)->builder,
                                                     "combo_bssid"));

        bssid_array = g_ptr_array_new ();
        for (i = 0; i < nm_setting_wireless_get_num_seen_bssids (page->setting); i++) {
                g_ptr_array_add (bssid_array, g_strdup (nm_setting_wireless_get_seen_bssid (page->setting, i)));
        }
        g_ptr_array_add (bssid_array, NULL);
        bssid_list = (gchar **) g_ptr_array_free (bssid_array, FALSE);
        s_bssid_str = nm_setting_wireless_get_bssid (page->setting);
        ce_page_setup_mac_combo (GTK_COMBO_BOX_TEXT (widget), s_bssid_str, bssid_list);
        g_strfreev (bssid_list);
        g_signal_connect_swapped (widget, "changed", G_CALLBACK (ce_page_changed), page);

        widget = GTK_WIDGET (gtk_builder_get_object (CE_PAGE (page)->builder,
                                                     "combo_mac"));
        mac_list = ce_page_get_mac_list (CE_PAGE (page)->client, NM_TYPE_DEVICE_WIFI,
                                         NM_DEVICE_WIFI_PERMANENT_HW_ADDRESS);
        s_mac_str = nm_setting_wireless_get_mac_address (page->setting);
        ce_page_setup_mac_combo (GTK_COMBO_BOX_TEXT (widget), s_mac_str, mac_list);
        g_strfreev (mac_list);
        g_signal_connect_swapped (widget, "changed", G_CALLBACK (ce_page_changed), page);


        widget = GTK_WIDGET (gtk_builder_get_object (CE_PAGE (page)->builder,
                                                     "entry_cloned_mac"));
        cloned_mac = nm_setting_wireless_get_cloned_mac_address (page->setting);
        gtk_entry_set_text (GTK_ENTRY (widget), cloned_mac ? cloned_mac : "");
        g_signal_connect_swapped (widget, "changed", G_CALLBACK (ce_page_changed), page);
}

static void
ui_to_setting (CEPageWifi *page)
{
        GBytes *ssid;
        const gchar *utf8_ssid, *bssid;
        GtkWidget *entry;
        char *device_mac, *cloned_mac;

        entry = GTK_WIDGET (gtk_builder_get_object (CE_PAGE (page)->builder, "entry_ssid"));
        utf8_ssid = gtk_entry_get_text (GTK_ENTRY (entry));
        if (!utf8_ssid || !*utf8_ssid)
                ssid = NULL;
        else {
                ssid = g_bytes_new_static (utf8_ssid, strlen (utf8_ssid));
        }
        entry = gtk_bin_get_child (GTK_BIN (gtk_builder_get_object (CE_PAGE (page)->builder, "combo_bssid")));
        bssid = gtk_entry_get_text (GTK_ENTRY (entry));
        if (*bssid == '\0')
                bssid = NULL;
        entry = gtk_bin_get_child (GTK_BIN (gtk_builder_get_object (CE_PAGE (page)->builder, "combo_mac")));
        device_mac = ce_page_trim_address (gtk_entry_get_text (GTK_ENTRY (entry)));
        entry = GTK_WIDGET (gtk_builder_get_object (CE_PAGE (page)->builder, "entry_cloned_mac"));
        cloned_mac = ce_page_trim_address (gtk_entry_get_text (GTK_ENTRY (entry)));

        g_object_set (page->setting,
                      NM_SETTING_WIRELESS_SSID, ssid,
                      NM_SETTING_WIRELESS_BSSID, bssid,
                      NM_SETTING_WIRELESS_MAC_ADDRESS, device_mac,
                      NM_SETTING_WIRELESS_CLONED_MAC_ADDRESS, cloned_mac,
                      NULL);

        if (ssid)
                g_bytes_unref (ssid);
        g_free (cloned_mac);
        g_free (device_mac);
}

static gboolean
validate (CEPage        *page,
          NMConnection  *connection,
          GError       **error)
{
        GtkWidget *entry;
        gboolean ret = TRUE;

        entry = gtk_bin_get_child (GTK_BIN (gtk_builder_get_object (page->builder, "combo_bssid")));
        if (!ce_page_address_is_valid (gtk_entry_get_text (GTK_ENTRY (entry)))) {
                widget_set_error (entry);
                ret = FALSE;
        } else {
                widget_unset_error (entry);
        }

        entry = gtk_bin_get_child (GTK_BIN (gtk_builder_get_object (page->builder, "combo_mac")));
        if (!ce_page_address_is_valid (gtk_entry_get_text (GTK_ENTRY (entry)))) {
                widget_set_error (entry);
                ret = FALSE;
        } else {
                widget_unset_error (entry);
        }

        entry = GTK_WIDGET (gtk_builder_get_object (page->builder, "entry_cloned_mac"));
        if (!ce_page_address_is_valid (gtk_entry_get_text (GTK_ENTRY (entry)))) {
                widget_set_error (entry);
                ret = FALSE;
        } else {
                widget_unset_error (entry);
        }

        if (!ret)
                return ret;

        ui_to_setting (CE_PAGE_WIFI (page));

        return ret;
}

static void
ce_page_wifi_init (CEPageWifi *page)
{
}

static void
ce_page_wifi_class_init (CEPageWifiClass *class)
{
        CEPageClass *page_class= CE_PAGE_CLASS (class);

        page_class->validate = validate;
}

CEPage *
ce_page_wifi_new (NMConnection     *connection,
                  NMClient         *client)
{
        CEPageWifi *page;

        page = CE_PAGE_WIFI (ce_page_new (CE_TYPE_PAGE_WIFI,
                                          connection,
                                          client,
                                          "/org/gnome/control-center/network/wifi-page.ui",
                                          _("Identity")));

        page->setting = nm_connection_get_setting_wireless (connection);

        connect_wifi_page (page);

        return CE_PAGE (page);
}
