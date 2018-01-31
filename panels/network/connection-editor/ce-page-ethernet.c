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
#include <net/if_arp.h>

#include <NetworkManager.h>


#include "ce-page-ethernet.h"
#include "ui-helpers.h"

G_DEFINE_TYPE (CEPageEthernet, ce_page_ethernet, CE_TYPE_PAGE)

static void
mtu_changed (GtkSpinButton *mtu, CEPageEthernet *page)
{
        if (gtk_spin_button_get_value_as_int (mtu) == 0)
                gtk_widget_hide (page->mtu_label);
        else
                gtk_widget_show (page->mtu_label);
}

static void
connect_ethernet_page (CEPageEthernet *page)
{
        NMSettingWired *setting = page->setting_wired;
        int mtu_def;
        char **mac_list;
        const char *s_mac_str;
        const gchar *name;
        const gchar *cloned_mac;

        name = nm_setting_connection_get_id (page->setting_connection);
        gtk_entry_set_text (page->name, name);

        /* Device MAC address */
        mac_list = ce_page_get_mac_list (CE_PAGE (page)->client, NM_TYPE_DEVICE_ETHERNET,
                                         NM_DEVICE_ETHERNET_PERMANENT_HW_ADDRESS);
        s_mac_str = nm_setting_wired_get_mac_address (setting);
        ce_page_setup_mac_combo (page->device_mac, s_mac_str, mac_list);
        g_strfreev (mac_list);
        g_signal_connect_swapped (page->device_mac, "changed", G_CALLBACK (ce_page_changed), page);

        /* Cloned MAC address */
        cloned_mac = nm_setting_wired_get_cloned_mac_address (setting);
        gtk_entry_set_text (GTK_ENTRY (page->cloned_mac), cloned_mac ? cloned_mac : "");
        g_signal_connect_swapped (page->cloned_mac, "changed", G_CALLBACK (ce_page_changed), page);

        /* MTU */
        mtu_def = ce_get_property_default (NM_SETTING (setting), NM_SETTING_WIRED_MTU);
        g_signal_connect (page->mtu, "output",
                          G_CALLBACK (ce_spin_output_with_default),
                          GINT_TO_POINTER (mtu_def));
        gtk_spin_button_set_value (page->mtu, (gdouble) nm_setting_wired_get_mtu (setting));
        g_signal_connect (page->mtu, "value-changed",
                          G_CALLBACK (mtu_changed), page);
        mtu_changed (page->mtu, page);

        g_signal_connect_swapped (page->name, "changed", G_CALLBACK (ce_page_changed), page);
        g_signal_connect_swapped (page->mtu, "value-changed", G_CALLBACK (ce_page_changed), page);
}

static void
ui_to_setting (CEPageEthernet *page)
{
        gchar *device_mac = NULL;
        gchar *cloned_mac = NULL;
        const gchar *text;
        GtkWidget *entry;

        entry = gtk_bin_get_child (GTK_BIN (page->device_mac));
        if (entry) {
                text = gtk_entry_get_text (GTK_ENTRY (entry));
                device_mac = ce_page_trim_address (text);
        }

        entry = gtk_bin_get_child (GTK_BIN (page->cloned_mac));
        if (entry) {
                text = gtk_entry_get_text (GTK_ENTRY (entry));
                cloned_mac = ce_page_trim_address (text);
        }

        g_object_set (page->setting_wired,
                      NM_SETTING_WIRED_MAC_ADDRESS, device_mac,
                      NM_SETTING_WIRED_CLONED_MAC_ADDRESS, cloned_mac,
                      NM_SETTING_WIRED_MTU, (guint32) gtk_spin_button_get_value_as_int (page->mtu),
                      NULL);

        g_object_set (page->setting_connection,
                      NM_SETTING_CONNECTION_ID, gtk_entry_get_text (page->name),
                      NULL);

        g_free (cloned_mac);
        g_free (device_mac);
}

static gboolean
validate (CEPage        *page,
          NMConnection  *connection,
          GError       **error)
{
        CEPageEthernet *self = CE_PAGE_ETHERNET (page);
        GtkWidget *entry;
        gboolean ret = TRUE;

        entry = gtk_bin_get_child (GTK_BIN (self->device_mac));
        if (entry) {
                if (!ce_page_address_is_valid (gtk_entry_get_text (GTK_ENTRY (entry)))) {
                        widget_set_error (entry);
                        ret = FALSE;
                } else {
                        widget_unset_error (entry);
                }
        }

        if (!ce_page_address_is_valid (gtk_entry_get_text (GTK_ENTRY (self->cloned_mac)))) {
                widget_set_error (GTK_WIDGET (self->cloned_mac));
                ret = FALSE;
        } else {
                widget_unset_error (GTK_WIDGET (self->cloned_mac));
        }

        if (!ret)
                return ret;

        ui_to_setting (self);

        return nm_setting_verify (NM_SETTING (self->setting_connection), NULL, error) &&
               nm_setting_verify (NM_SETTING (self->setting_wired), NULL, error);
}

static void
ce_page_ethernet_init (CEPageEthernet *page)
{
}

static void
ce_page_ethernet_class_init (CEPageEthernetClass *class)
{
        CEPageClass *page_class= CE_PAGE_CLASS (class);

        page_class->validate = validate;
}

CEPage *
ce_page_ethernet_new (NMConnection     *connection,
                      NMClient         *client)
{
        CEPageEthernet *page;

        page = CE_PAGE_ETHERNET (ce_page_new (CE_TYPE_PAGE_ETHERNET,
                                              connection,
                                              client,
                                              "/org/gnome/control-center/network/ethernet-page.ui",
                                              _("Identity")));

        page->name = GTK_ENTRY (gtk_builder_get_object (CE_PAGE (page)->builder, "entry_name"));
        page->device_mac = GTK_COMBO_BOX_TEXT (gtk_builder_get_object (CE_PAGE (page)->builder, "combo_mac"));
        page->cloned_mac = GTK_ENTRY (gtk_builder_get_object (CE_PAGE (page)->builder, "entry_cloned_mac"));
        page->mtu = GTK_SPIN_BUTTON (gtk_builder_get_object (CE_PAGE (page)->builder, "spin_mtu"));
        page->mtu_label = GTK_WIDGET (gtk_builder_get_object (CE_PAGE (page)->builder, "label_mtu"));

        page->setting_connection = nm_connection_get_setting_connection (connection);
        page->setting_wired = nm_connection_get_setting_wired (connection);

        connect_ethernet_page (page);

        return CE_PAGE (page);
}
