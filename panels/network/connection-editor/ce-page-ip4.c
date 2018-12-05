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

#include <errno.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <glib-object.h>
#include <glib/gi18n.h>
#include <NetworkManager.h>

#include "list-box-helper.h"
#include "ce-page-ip4.h"
#include "ui-helpers.h"

#define RADIO_IS_ACTIVE(x) (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (gtk_builder_get_object(CE_PAGE (page)->builder, x))))

static void ensure_empty_address_row (CEPageIP4 *page);
static void ensure_empty_routes_row (CEPageIP4 *page);

G_DEFINE_TYPE (CEPageIP4, ce_page_ip4, CE_TYPE_PAGE)

enum {
        METHOD_COL_NAME,
        METHOD_COL_METHOD
};

enum {
        IP4_METHOD_AUTO,
        IP4_METHOD_MANUAL,
        IP4_METHOD_LINK_LOCAL,
        IP4_METHOD_SHARED,
        IP4_METHOD_DISABLED
};

static void
method_changed (GtkToggleButton *radio, CEPageIP4 *page)
{
        gboolean addr_enabled;
        gboolean dns_enabled;
        gboolean routes_enabled;
        GtkWidget *widget;

        if (RADIO_IS_ACTIVE ("radio_disabled")) {
                addr_enabled = FALSE;
                dns_enabled = FALSE;
                routes_enabled = FALSE;
        } else {
                addr_enabled = RADIO_IS_ACTIVE ("radio_manual");
                dns_enabled = !RADIO_IS_ACTIVE ("radio_local");
                routes_enabled = !RADIO_IS_ACTIVE ("radio_local");
        }

        widget = GTK_WIDGET (gtk_builder_get_object (CE_PAGE (page)->builder, "address_section"));
        gtk_widget_set_visible (widget, addr_enabled);
        gtk_widget_set_sensitive (page->dns_entry, dns_enabled);
        gtk_widget_set_sensitive (page->routes_list, routes_enabled);
        gtk_widget_set_sensitive (page->never_default, routes_enabled);

        ce_page_changed (CE_PAGE (page));
}

static void
switch_toggled (GObject    *object,
                GParamSpec *pspec,
                CEPage     *page)
{
        ce_page_changed (page);
}

static void
update_row_sensitivity (CEPageIP4 *page, GtkWidget *list)
{
        GList *children, *l;
        gint rows = 0, i = 0;

        children = gtk_container_get_children (GTK_CONTAINER (list));
        for (l = children; l; l = l->next) {
                GtkWidget *row = l->data;
                GtkWidget *button;

                button = GTK_WIDGET (g_object_get_data (G_OBJECT (row), "delete-button"));
                if (button != NULL)
                        rows++;
        }
        for (l = children; l; l = l->next) {
                GtkWidget *row = l->data;
                GtkWidget *button;

                button = GTK_WIDGET (g_object_get_data (G_OBJECT (row), "delete-button"));
                if (button != NULL)
                        gtk_widget_set_sensitive (button, rows > 1 && ++i < rows);
        }
        g_list_free (children);
}

static void
update_row_gateway_sensitivity (CEPageIP4 *page)
{
        GList *children, *l;
        gint rows = 0;

        children = gtk_container_get_children (GTK_CONTAINER (page->address_list));
        for (l = children; l; l = l->next) {
                GtkWidget *row = l->data;
                GtkWidget *entry;

                entry = GTK_WIDGET (g_object_get_data (G_OBJECT (row), "gateway"));

                gtk_widget_set_sensitive (entry, (rows == 0));

                rows++;
        }
        g_list_free (children);
}

static void
remove_row (GtkButton *button, CEPageIP4 *page)
{
        GtkWidget *list;
        GtkWidget *row;
        GtkWidget *row_box;

        row_box = gtk_widget_get_parent (GTK_WIDGET (button));
        row = gtk_widget_get_parent (row_box);
        list = gtk_widget_get_parent (row);

        gtk_container_remove (GTK_CONTAINER (list), row);

        ce_page_changed (CE_PAGE (page));

        update_row_sensitivity (page, list);
        if (list == page->address_list)
                update_row_gateway_sensitivity (page);
}

static gboolean
validate_row (GtkWidget *row)
{
        GtkWidget *box;
        GList *children, *l;
        gboolean valid;

        valid = FALSE;
        box = gtk_bin_get_child (GTK_BIN (row));
        children = gtk_container_get_children (GTK_CONTAINER (box));

        for (l = children; l != NULL; l = l->next) {
                if (!GTK_IS_ENTRY (l->data))
                        continue;

                valid = valid || gtk_entry_get_text_length (l->data) > 0;
        }

        g_list_free (children);

        return valid;
}

static gint
sort_first_last (gconstpointer a, gconstpointer b, gpointer data)
{
        gboolean afirst, bfirst, alast, blast;

        afirst = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (a), "first"));
        bfirst = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (b), "first"));
        alast = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (a), "last"));
        blast = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (b), "last"));

        if (afirst)
                return -1;
        if (bfirst)
                return 1;
        if (alast)
                return 1;
        if (blast)
                return -1;

        return 0;
}

static void
add_address_row (CEPageIP4   *page,
                 const gchar *address,
                 const gchar *network,
                 const gchar *gateway)
{
        GtkSizeGroup *group;
        GtkWidget *row;
        GtkWidget *row_box;
        GtkWidget *widget;
        GtkWidget *delete_button;
        GtkWidget *image;

        row = gtk_list_box_row_new ();

        row_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
        gtk_style_context_add_class (gtk_widget_get_style_context (row_box), "linked");

        widget = gtk_entry_new ();
        g_signal_connect_swapped (widget, "changed", G_CALLBACK (ce_page_changed), page);
        g_signal_connect_swapped (widget, "activate", G_CALLBACK (ensure_empty_address_row), page);
        g_object_set_data (G_OBJECT (row), "address", widget);
        gtk_entry_set_text (GTK_ENTRY (widget), address);
        gtk_entry_set_width_chars (GTK_ENTRY (widget), 16);
        gtk_widget_set_hexpand (widget, TRUE);
        gtk_container_add (GTK_CONTAINER (row_box), widget);

        widget = gtk_entry_new ();
        g_signal_connect_swapped (widget, "changed", G_CALLBACK (ce_page_changed), page);
        g_signal_connect_swapped (widget, "activate", G_CALLBACK (ensure_empty_address_row), page);
        g_object_set_data (G_OBJECT (row), "network", widget);
        gtk_entry_set_text (GTK_ENTRY (widget), network);
        gtk_entry_set_width_chars (GTK_ENTRY (widget), 16);
        gtk_widget_set_hexpand (widget, TRUE);
        gtk_container_add (GTK_CONTAINER (row_box), widget);

        widget = gtk_entry_new ();
        g_signal_connect_swapped (widget, "changed", G_CALLBACK (ce_page_changed), page);
        g_signal_connect_swapped (widget, "activate", G_CALLBACK (ensure_empty_address_row), page);
        g_object_set_data (G_OBJECT (row), "gateway", widget);
        gtk_entry_set_text (GTK_ENTRY (widget), gateway ? gateway : "");
        gtk_entry_set_width_chars (GTK_ENTRY (widget), 16);
        gtk_widget_set_hexpand (widget, TRUE);
        gtk_container_add (GTK_CONTAINER (row_box), widget);

        delete_button = gtk_button_new ();
        gtk_widget_set_sensitive (delete_button, FALSE);
        gtk_style_context_add_class (gtk_widget_get_style_context (delete_button), "image-button");
        g_signal_connect (delete_button, "clicked", G_CALLBACK (remove_row), page);
        image = gtk_image_new_from_icon_name ("edit-delete-symbolic", GTK_ICON_SIZE_MENU);
        atk_object_set_name (gtk_widget_get_accessible (delete_button), _("Delete Address"));
        gtk_button_set_image (GTK_BUTTON (delete_button), image);
        gtk_container_add (GTK_CONTAINER (row_box), delete_button);
        g_object_set_data (G_OBJECT (row), "delete-button", delete_button);

        group = GTK_SIZE_GROUP (gtk_builder_get_object (CE_PAGE (page)->builder, "address_sizegroup"));
        gtk_size_group_add_widget (group, delete_button);

        gtk_container_add (GTK_CONTAINER (row), row_box);
        gtk_widget_show_all (row);
        gtk_container_add (GTK_CONTAINER (page->address_list), row);

        update_row_gateway_sensitivity (page);
        update_row_sensitivity (page, page->address_list);
}

static void
ensure_empty_address_row (CEPageIP4 *page)
{
        GList *children, *l;

        children = gtk_container_get_children (GTK_CONTAINER (page->address_list));
        l = children;

        while (l && l->next)
                l = l->next;

        /* Add the last, stub row if needed*/
        if (!l || validate_row (l->data))
                add_address_row (page, "", "", "");

        g_list_free (children);
}

static void
add_address_section (CEPageIP4 *page)
{
        GtkWidget *widget;
        GtkWidget *list;
        gint i;

        widget = GTK_WIDGET (gtk_builder_get_object (CE_PAGE (page)->builder, "address_section"));

        page->address_list = list = gtk_list_box_new ();
        gtk_list_box_set_selection_mode (GTK_LIST_BOX (list), GTK_SELECTION_NONE);
        gtk_list_box_set_header_func (GTK_LIST_BOX (list), cc_list_box_update_header_func, NULL, NULL);
        gtk_list_box_set_sort_func (GTK_LIST_BOX (list), (GtkListBoxSortFunc)sort_first_last, NULL, NULL);
        gtk_container_add (GTK_CONTAINER (widget), list);

        for (i = 0; i < nm_setting_ip_config_get_num_addresses (page->setting); i++) {
                NMIPAddress *addr;
                struct in_addr tmp_addr;
                gchar network[INET_ADDRSTRLEN + 1];

                addr = nm_setting_ip_config_get_address (page->setting, i);
                if (!addr)
                        continue;

                tmp_addr.s_addr = nm_utils_ip4_prefix_to_netmask (nm_ip_address_get_prefix (addr));
                (void) inet_ntop (AF_INET, &tmp_addr, &network[0], sizeof (network));

                add_address_row (page,
                                 nm_ip_address_get_address (addr),
                                 network,
                                 i == 0 ? nm_setting_ip_config_get_gateway (page->setting) : "");
        }
        if (nm_setting_ip_config_get_num_addresses (page->setting) == 0)
                ensure_empty_address_row (page);

        gtk_widget_show_all (widget);
}

static void
add_dns_section (CEPageIP4 *page)
{
        GtkEntry *entry;
        GString *string;
        gint i;

        page->auto_dns = GTK_SWITCH (gtk_builder_get_object (CE_PAGE (page)->builder, "auto_dns_switch"));
        gtk_switch_set_active (page->auto_dns, !nm_setting_ip_config_get_ignore_auto_dns (page->setting));
        g_signal_connect (page->auto_dns, "notify::active", G_CALLBACK (switch_toggled), page);

        page->dns_entry = GTK_WIDGET (gtk_builder_get_object (CE_PAGE (page)->builder, "dns_entry"));
        entry = GTK_ENTRY (page->dns_entry);
        string = g_string_new ("");

        for (i = 0; i < nm_setting_ip_config_get_num_dns (page->setting); i++) {
                const char *address;

                address = nm_setting_ip_config_get_dns (page->setting, i);

                if (i > 0)
                        g_string_append (string, ", ");

                g_string_append (string, address);
        }

        gtk_entry_set_text (entry, string->str);

        g_signal_connect_swapped (entry, "notify::text", G_CALLBACK (ce_page_changed), page);

        g_string_free (string, TRUE);
}

static void
add_route_row (CEPageIP4   *page,
               const gchar *address,
               const gchar *netmask,
               const gchar *gateway,
               gint         metric)
{
        GtkSizeGroup *group;
        GtkWidget *row;
        GtkWidget *row_box;
        GtkWidget *widget;
        GtkWidget *delete_button;
        GtkWidget *image;

        row = gtk_list_box_row_new ();

        row_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
        gtk_style_context_add_class (gtk_widget_get_style_context (row_box), "linked");

        widget = gtk_entry_new ();
        g_signal_connect_swapped (widget, "changed", G_CALLBACK (ce_page_changed), page);
        g_signal_connect_swapped (widget, "activate", G_CALLBACK (ensure_empty_routes_row), page);
        g_object_set_data (G_OBJECT (row), "address", widget);
        gtk_entry_set_text (GTK_ENTRY (widget), address);
        gtk_entry_set_width_chars (GTK_ENTRY (widget), 0);
        gtk_widget_set_hexpand (widget, TRUE);
        gtk_container_add (GTK_CONTAINER (row_box), widget);

        group = GTK_SIZE_GROUP (gtk_builder_get_object (CE_PAGE (page)->builder, "routes_sizegroup"));
        gtk_size_group_add_widget (group, widget);

        widget = gtk_entry_new ();
        g_signal_connect_swapped (widget, "changed", G_CALLBACK (ce_page_changed), page);
        g_signal_connect_swapped (widget, "activate", G_CALLBACK (ensure_empty_routes_row), page);
        g_object_set_data (G_OBJECT (row), "netmask", widget);
        gtk_entry_set_text (GTK_ENTRY (widget), netmask);
        gtk_entry_set_width_chars (GTK_ENTRY (widget), 0);
        gtk_widget_set_hexpand (widget, TRUE);
        gtk_container_add (GTK_CONTAINER (row_box), widget);

        group = GTK_SIZE_GROUP (gtk_builder_get_object (CE_PAGE (page)->builder, "routes_sizegroup"));
        gtk_size_group_add_widget (group, widget);

        widget = gtk_entry_new ();
        g_signal_connect_swapped (widget, "changed", G_CALLBACK (ce_page_changed), page);
        g_signal_connect_swapped (widget, "activate", G_CALLBACK (ensure_empty_routes_row), page);
        g_object_set_data (G_OBJECT (row), "gateway", widget);
        gtk_entry_set_text (GTK_ENTRY (widget), gateway ? gateway : "");
        gtk_entry_set_width_chars (GTK_ENTRY (widget), 0);
        gtk_widget_set_hexpand (widget, TRUE);
        gtk_container_add (GTK_CONTAINER (row_box), widget);

        group = GTK_SIZE_GROUP (gtk_builder_get_object (CE_PAGE (page)->builder, "routes_sizegroup"));
        gtk_size_group_add_widget (group, widget);

        widget = gtk_entry_new ();
        g_signal_connect_swapped (widget, "changed", G_CALLBACK (ce_page_changed), page);
        g_signal_connect_swapped (widget, "activate", G_CALLBACK (ensure_empty_routes_row), page);
        g_object_set_data (G_OBJECT (row), "metric", widget);
        if (metric >= 0) {
                gchar *s = g_strdup_printf ("%d", metric);
                gtk_entry_set_text (GTK_ENTRY (widget), s);
                g_free (s);
        }
        gtk_entry_set_width_chars (GTK_ENTRY (widget), 5);
        gtk_container_add (GTK_CONTAINER (row_box), widget);

        group = GTK_SIZE_GROUP (gtk_builder_get_object (CE_PAGE (page)->builder, "routes_metric_sizegroup"));
        gtk_size_group_add_widget (group, widget);

        delete_button = gtk_button_new ();
        gtk_style_context_add_class (gtk_widget_get_style_context (delete_button), "image-button");
        g_signal_connect (delete_button, "clicked", G_CALLBACK (remove_row), page);
        image = gtk_image_new_from_icon_name ("edit-delete-symbolic", GTK_ICON_SIZE_MENU);
        atk_object_set_name (gtk_widget_get_accessible (delete_button), _("Delete Route"));
        gtk_button_set_image (GTK_BUTTON (delete_button), image);
        gtk_widget_set_halign (delete_button, GTK_ALIGN_CENTER);
        gtk_widget_set_valign (delete_button, GTK_ALIGN_CENTER);
        gtk_container_add (GTK_CONTAINER (row_box), delete_button);
        g_object_set_data (G_OBJECT (row), "delete-button", delete_button);

        group = GTK_SIZE_GROUP (gtk_builder_get_object (CE_PAGE (page)->builder, "routes_stub_sizegroup"));
        gtk_size_group_add_widget (group, delete_button);

        gtk_container_add (GTK_CONTAINER (row), row_box);
        gtk_widget_show_all (row);
        gtk_container_add (GTK_CONTAINER (page->routes_list), row);

        update_row_sensitivity (page, page->routes_list);
}

static void
ensure_empty_routes_row (CEPageIP4 *page)
{
        GList *children, *l;

        children = gtk_container_get_children (GTK_CONTAINER (page->routes_list));
        l = children;

        while (l && l->next)
                l = l->next;

        /* Add the last, stub row if needed*/
        if (!l || validate_row (l->data))
                add_route_row (page, "", "", "", -1);

        g_list_free (children);
}

static void
add_routes_section (CEPageIP4 *page)
{
        GtkWidget *widget;
        GtkWidget *list;
        gint i;

        widget = GTK_WIDGET (gtk_builder_get_object (CE_PAGE (page)->builder, "routes_section"));

        page->routes_list = list = gtk_list_box_new ();
        gtk_list_box_set_selection_mode (GTK_LIST_BOX (list), GTK_SELECTION_NONE);
        gtk_list_box_set_header_func (GTK_LIST_BOX (list), cc_list_box_update_header_func, NULL, NULL);
        gtk_list_box_set_sort_func (GTK_LIST_BOX (list), (GtkListBoxSortFunc)sort_first_last, NULL, NULL);
        gtk_container_add (GTK_CONTAINER (widget), list);
        page->auto_routes = GTK_SWITCH (gtk_builder_get_object (CE_PAGE (page)->builder, "auto_routes_switch"));
        gtk_switch_set_active (page->auto_routes, !nm_setting_ip_config_get_ignore_auto_routes (page->setting));
        g_signal_connect (page->auto_routes, "notify::active", G_CALLBACK (switch_toggled), page);


        for (i = 0; i < nm_setting_ip_config_get_num_routes (page->setting); i++) {
                NMIPRoute *route;
                struct in_addr tmp_addr;
                gchar netmask[INET_ADDRSTRLEN + 1];

                route = nm_setting_ip_config_get_route (page->setting, i);
                if (!route)
                        continue;

                tmp_addr.s_addr = nm_utils_ip4_prefix_to_netmask (nm_ip_route_get_prefix (route));
                (void) inet_ntop (AF_INET, &tmp_addr, &netmask[0], sizeof (netmask));

                add_route_row (page,
                               nm_ip_route_get_dest (route),
                               netmask,
                               nm_ip_route_get_next_hop (route),
                               nm_ip_route_get_metric (route));
        }
        if (nm_setting_ip_config_get_num_routes (page->setting) == 0)
                ensure_empty_routes_row (page);

        gtk_widget_show_all (widget);
}

enum
{
        RADIO_AUTOMATIC,
        RADIO_LOCAL,
        RADIO_MANUAL,
        RADIO_DISABLED,
        N_RADIO
};

static void
connect_ip4_page (CEPageIP4 *page)
{
        GtkToggleButton *radios[N_RADIO];
        GtkWidget *content;
        const gchar *str_method;
        gboolean disabled;
        guint method, i;

        add_address_section (page);
        add_dns_section (page);
        add_routes_section (page);

        page->disabled = GTK_TOGGLE_BUTTON (gtk_builder_get_object (CE_PAGE (page)->builder, "radio_disabled"));

        str_method = nm_setting_ip_config_get_method (page->setting);
        disabled = g_strcmp0 (str_method, NM_SETTING_IP4_CONFIG_METHOD_DISABLED) == 0;
        gtk_toggle_button_set_active (page->disabled, disabled);
        g_signal_connect_swapped (page->disabled, "notify::active", G_CALLBACK (ce_page_changed), page);
        content = GTK_WIDGET (gtk_builder_get_object (CE_PAGE (page)->builder, "page_content"));
        g_object_bind_property (page->disabled, "active",
                                content, "sensitive",
                                G_BINDING_SYNC_CREATE | G_BINDING_INVERT_BOOLEAN);

        method = IP4_METHOD_AUTO;
        if (g_strcmp0 (str_method, NM_SETTING_IP4_CONFIG_METHOD_LINK_LOCAL) == 0) {
                method = IP4_METHOD_LINK_LOCAL;
        } else if (g_strcmp0 (str_method, NM_SETTING_IP4_CONFIG_METHOD_MANUAL) == 0) {
                method = IP4_METHOD_MANUAL;
        } else if (g_strcmp0 (str_method, NM_SETTING_IP4_CONFIG_METHOD_SHARED) == 0) {
                method = IP4_METHOD_SHARED;
        } else if (g_strcmp0 (str_method, NM_SETTING_IP4_CONFIG_METHOD_DISABLED) == 0) {
                method = IP4_METHOD_DISABLED;
        }

        page->never_default = GTK_WIDGET (gtk_builder_get_object (CE_PAGE (page)->builder, "never_default_check"));
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (page->never_default),
                                      nm_setting_ip_config_get_never_default (page->setting));
        g_signal_connect_swapped (page->never_default, "toggled", G_CALLBACK (ce_page_changed), page);

        /* Connect radio buttons */
        radios[RADIO_AUTOMATIC] = GTK_TOGGLE_BUTTON (gtk_builder_get_object (CE_PAGE (page)->builder, "radio_automatic"));
        radios[RADIO_LOCAL] = GTK_TOGGLE_BUTTON (gtk_builder_get_object (CE_PAGE (page)->builder, "radio_local"));
        radios[RADIO_MANUAL] = GTK_TOGGLE_BUTTON (gtk_builder_get_object (CE_PAGE (page)->builder, "radio_manual"));
        radios[RADIO_DISABLED] = page->disabled;

        for (i = RADIO_AUTOMATIC; i < RADIO_DISABLED; i++)
                g_signal_connect (radios[i], "toggled", G_CALLBACK (method_changed), page);

        switch (method) {
        case IP4_METHOD_AUTO:
                gtk_toggle_button_set_active (radios[RADIO_AUTOMATIC], TRUE);
                break;
        case IP4_METHOD_LINK_LOCAL:
                gtk_toggle_button_set_active (radios[RADIO_LOCAL], TRUE);
                break;
        case IP4_METHOD_MANUAL:
                gtk_toggle_button_set_active (radios[RADIO_MANUAL], TRUE);
                break;
        case IP4_METHOD_DISABLED:
                gtk_toggle_button_set_active (radios[RADIO_DISABLED], TRUE);
                break;
        default:
                break;
        }

        method_changed (NULL, page);
}

static gboolean
parse_netmask (const char *str, guint32 *prefix)
{
        struct in_addr tmp_addr;
        glong tmp_prefix;

        errno = 0;

        /* Is it a prefix? */
        if (!strchr (str, '.')) {
                tmp_prefix = strtol (str, NULL, 10);
                if (!errno && tmp_prefix >= 0 && tmp_prefix <= 32) {
                        *prefix = tmp_prefix;
                        return TRUE;
                }
        }

        /* Is it a netmask? */
        if (inet_pton (AF_INET, str, &tmp_addr) > 0) {
                *prefix = nm_utils_ip4_netmask_to_prefix (tmp_addr.s_addr);
                return TRUE;
        }

        return FALSE;
}

static gboolean
ui_to_setting (CEPageIP4 *page)
{
        const gchar *method;
        gboolean ignore_auto_dns;
        gboolean ignore_auto_routes;
        gboolean never_default;
        GPtrArray *addresses = NULL;
        GPtrArray *dns_servers = NULL;
        GPtrArray *routes = NULL;
        GStrv dns_addresses = NULL;
        GList *children, *l;
        gboolean ret = TRUE;
        const char *default_gateway = NULL;
        gchar *dns_text = NULL;
        guint i;

        if (gtk_toggle_button_get_active (page->disabled)) {
                method = NM_SETTING_IP4_CONFIG_METHOD_DISABLED;
        } else {
                if (RADIO_IS_ACTIVE ("radio_automatic"))
                        method = NM_SETTING_IP4_CONFIG_METHOD_AUTO;
                else if (RADIO_IS_ACTIVE ("radio_local"))
                        method = NM_SETTING_IP4_CONFIG_METHOD_LINK_LOCAL;
                else if (RADIO_IS_ACTIVE ("radio_manual"))
                        method = NM_SETTING_IP4_CONFIG_METHOD_MANUAL;
        }

        addresses = g_ptr_array_new_with_free_func ((GDestroyNotify) nm_ip_address_unref);
        if (g_str_equal (method, NM_SETTING_IP4_CONFIG_METHOD_MANUAL))
                children = gtk_container_get_children (GTK_CONTAINER (page->address_list));
        else
                children = NULL;

        for (l = children; l; l = l->next) {
                GtkWidget *row = l->data;
                GtkEntry *entry;
                GtkEntry *gateway_entry;
                const gchar *text_address;
                const gchar *text_netmask;
                const gchar *text_gateway = "";
                NMIPAddress *addr;
                guint32 prefix;

                entry = GTK_ENTRY (g_object_get_data (G_OBJECT (row), "address"));
                if (!entry)
                        continue;

                text_address = gtk_entry_get_text (entry);
                text_netmask = gtk_entry_get_text (GTK_ENTRY (g_object_get_data (G_OBJECT (row), "network")));
                gateway_entry = g_object_get_data (G_OBJECT (row), "gateway");
                if (gtk_widget_is_visible (GTK_WIDGET (gateway_entry)))
                        text_gateway = gtk_entry_get_text (gateway_entry);

                if (!*text_address && !*text_netmask && !*text_gateway) {
                        /* ignore empty rows */
                        widget_unset_error (GTK_WIDGET (entry));
                        widget_unset_error (g_object_get_data (G_OBJECT (row), "network"));
                        widget_unset_error (GTK_WIDGET (gateway_entry));
                        continue;
                }

                if (!nm_utils_ipaddr_valid (AF_INET, text_address)) {
                        widget_set_error (GTK_WIDGET (entry));
                        ret = FALSE;
                } else {
                        widget_unset_error (GTK_WIDGET (entry));
                }

                if (!parse_netmask (text_netmask, &prefix)) {
                        widget_set_error (g_object_get_data (G_OBJECT (row), "network"));
                        ret = FALSE;
                } else {
                        widget_unset_error (g_object_get_data (G_OBJECT (row), "network"));
                }

                if (gtk_widget_is_visible (GTK_WIDGET (gateway_entry)) &&
                    *text_gateway &&
                    !nm_utils_ipaddr_valid (AF_INET, text_gateway)) {
                        widget_set_error (g_object_get_data (G_OBJECT (row), "gateway"));
                        ret = FALSE;
                } else {
                         widget_unset_error (GTK_WIDGET (gateway_entry));
                         if (gtk_widget_is_visible (GTK_WIDGET (gateway_entry)) && *text_gateway) {
                                 g_assert (default_gateway == NULL);
                                 default_gateway = text_gateway;
                         }
                }

                if (!ret)
                        continue;

                addr = nm_ip_address_new (AF_INET, text_address, prefix, NULL);
                if (addr)
                        g_ptr_array_add (addresses, addr);

                if (!l || !l->next)
                        ensure_empty_address_row (page);
        }
        g_list_free (children);

        if (addresses->len == 0) {
                g_ptr_array_free (addresses, TRUE);
                addresses = NULL;
        }

        dns_servers = g_ptr_array_new_with_free_func (g_free);
        dns_text = g_strstrip (g_strdup (gtk_entry_get_text (GTK_ENTRY (page->dns_entry))));
        if (g_str_equal (method, NM_SETTING_IP4_CONFIG_METHOD_AUTO) ||
            g_str_equal (method, NM_SETTING_IP4_CONFIG_METHOD_MANUAL))
                dns_addresses = g_strsplit_set (dns_text, ", ", -1);
        else
                dns_addresses = NULL;

        for (i = 0; dns_addresses && dns_addresses[i]; i++) {
                const gchar *text;

                text = dns_addresses[i];

                if (!text || !*text)
                        continue;

                if (!nm_utils_ipaddr_valid (AF_INET, text)) {
                        g_ptr_array_remove_range (dns_servers, 0, dns_servers->len);
                        widget_set_error (page->dns_entry);
                        ret = FALSE;
                        break;
                } else {
                        widget_unset_error (page->dns_entry);
                        g_ptr_array_add (dns_servers, g_strdup (text));
                }
        }
        g_clear_pointer (&dns_addresses, g_strfreev);

        if (dns_servers->len == 0) {
                g_ptr_array_free (dns_servers, TRUE);
                dns_servers = NULL;
        } else {
                g_ptr_array_add (dns_servers, NULL);
        }

        routes = g_ptr_array_new_with_free_func ((GDestroyNotify) nm_ip_route_unref);
        if (g_str_equal (method, NM_SETTING_IP4_CONFIG_METHOD_AUTO) ||
            g_str_equal (method, NM_SETTING_IP4_CONFIG_METHOD_MANUAL))
                children = gtk_container_get_children (GTK_CONTAINER (page->routes_list));
        else
                children = NULL;

        for (l = children; l; l = l->next) {
                GtkWidget *row = l->data;
                GtkEntry *entry;
                const gchar *text_address;
                const gchar *text_netmask;
                const gchar *text_gateway;
                const gchar *text_metric;
                gint64 metric;
                guint32 netmask;
                NMIPRoute *route;

                entry = GTK_ENTRY (g_object_get_data (G_OBJECT (row), "address"));
                if (!entry)
                        continue;

                text_address = gtk_entry_get_text (entry);
                text_netmask = gtk_entry_get_text (GTK_ENTRY (g_object_get_data (G_OBJECT (row), "netmask")));
                text_gateway = gtk_entry_get_text (GTK_ENTRY (g_object_get_data (G_OBJECT (row), "gateway")));
                text_metric = gtk_entry_get_text (GTK_ENTRY (g_object_get_data (G_OBJECT (row), "metric")));

                if (!*text_address && !*text_netmask && !*text_gateway && !*text_metric) {
                        /* ignore empty rows */
                        continue;
                }

                if (text_address && !nm_utils_ipaddr_valid (AF_INET, text_address)) {
                        widget_set_error (GTK_WIDGET (entry));
                        ret = FALSE;
                } else {
                        widget_unset_error (GTK_WIDGET (entry));
                }

                if (!parse_netmask (text_netmask, &netmask)) {
                        widget_set_error (GTK_WIDGET (g_object_get_data (G_OBJECT (row), "netmask")));
                        ret = FALSE;
                } else {
                        widget_unset_error (GTK_WIDGET (g_object_get_data (G_OBJECT (row), "netmask")));
                }

                if (text_gateway && !nm_utils_ipaddr_valid (AF_INET, text_gateway)) {
                        widget_set_error (GTK_WIDGET (g_object_get_data (G_OBJECT (row), "gateway")));
                        ret = FALSE;
                } else {
                        widget_unset_error (GTK_WIDGET (g_object_get_data (G_OBJECT (row), "gateway")));
                }

                metric = -1;
                if (*text_metric) {
                        errno = 0;
                        metric = g_ascii_strtoull (text_metric, NULL, 10);
                        if (errno || metric < 0 || metric > G_MAXUINT32) {
                                widget_set_error (GTK_WIDGET (g_object_get_data (G_OBJECT (row), "metric")));
                                ret = FALSE;
                        } else {
                                widget_unset_error (GTK_WIDGET (g_object_get_data (G_OBJECT (row), "metric")));
                        }
                } else {
                        widget_unset_error (GTK_WIDGET (g_object_get_data (G_OBJECT (row), "metric")));
                }

                if (!ret)
                        continue;

                route = nm_ip_route_new (AF_INET, text_address, netmask, text_gateway, metric, NULL);
                if (route)
                        g_ptr_array_add (routes, route);

                if (!l || !l->next)
                        ensure_empty_routes_row (page);
        }
        g_list_free (children);

        if (routes->len == 0) {
                g_ptr_array_free (routes, TRUE);
                routes = NULL;
        }

        if (!ret)
                goto out;

        ignore_auto_dns = !gtk_switch_get_active (page->auto_dns);
        ignore_auto_routes = !gtk_switch_get_active (page->auto_routes);
        never_default = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (page->never_default));

        g_object_set (page->setting,
                      NM_SETTING_IP_CONFIG_METHOD, method,
                      NM_SETTING_IP_CONFIG_ADDRESSES, addresses,
                      NM_SETTING_IP_CONFIG_GATEWAY, default_gateway,
                      NM_SETTING_IP_CONFIG_DNS, dns_servers ? dns_servers->pdata : NULL,
                      NM_SETTING_IP_CONFIG_ROUTES, routes,
                      NM_SETTING_IP_CONFIG_IGNORE_AUTO_DNS, ignore_auto_dns,
                      NM_SETTING_IP_CONFIG_IGNORE_AUTO_ROUTES, ignore_auto_routes,
                      NM_SETTING_IP_CONFIG_NEVER_DEFAULT, never_default,
                      NULL);

out:
        if (addresses)
                g_ptr_array_free (addresses, TRUE);

        if (dns_servers)
                g_ptr_array_free (dns_servers, TRUE);

        if (routes)
                g_ptr_array_free (routes, TRUE);

        g_clear_pointer (&dns_text, g_free);

        return ret;
}

static gboolean
validate (CEPage        *page,
          NMConnection  *connection,
          GError       **error)
{
        if (!ui_to_setting (CE_PAGE_IP4 (page)))
                return FALSE;

        return nm_setting_verify (NM_SETTING (CE_PAGE_IP4 (page)->setting), NULL, error);
}

static void
ce_page_ip4_init (CEPageIP4 *page)
{
}

static void
ce_page_ip4_class_init (CEPageIP4Class *class)
{
        CEPageClass *page_class= CE_PAGE_CLASS (class);

        page_class->validate = validate;
}

CEPage *
ce_page_ip4_new (NMConnection     *connection,
                 NMClient         *client)
{
        CEPageIP4 *page;

        page = CE_PAGE_IP4 (ce_page_new (CE_TYPE_PAGE_IP4,
                                           connection,
                                           client,
                                           "/org/gnome/control-center/network/ip4-page.ui",
                                           _("IPv4")));

        page->setting = nm_connection_get_setting_ip4_config (connection);
        if (!page->setting) {
                page->setting = NM_SETTING_IP_CONFIG (nm_setting_ip4_config_new ());
                nm_connection_add_setting (connection, NM_SETTING (page->setting));
        }

        connect_ip4_page (page);

        return CE_PAGE (page);
}
