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
#include <glib/gi18n.h>
#include <NetworkManager.h>

#include "list-box-helper.h"
#include "ce-page.h"
#include "ce-page-ip4.h"
#include "ui-helpers.h"

static void ensure_empty_address_row (CEPageIP4 *self);
static void ensure_empty_routes_row (CEPageIP4 *self);

struct _CEPageIP4
{
        GtkScrolledWindow parent;

        GtkBox            *address_box;
        GtkSizeGroup      *address_sizegroup;
        GtkSwitch         *auto_dns_switch;
        GtkSwitch         *auto_routes_switch;
        GtkRadioButton    *automatic_radio;
        GtkBox            *content_box;
        GtkRadioButton    *disabled_radio;
        GtkEntry          *dns_entry;
        GtkRadioButton    *local_radio;
        GtkRadioButton    *manual_radio;
        GtkCheckButton    *never_default_check;
        GtkBox            *routes_box;
        GtkSizeGroup      *routes_metric_sizegroup;
        GtkSizeGroup      *routes_sizegroup;
        GtkRadioButton    *shared_radio;

        NMSettingIPConfig *setting;

        GtkWidget      *address_list;
        GtkWidget      *routes_list;
};

static void ce_page_iface_init (CEPageInterface *);

G_DEFINE_TYPE_WITH_CODE (CEPageIP4, ce_page_ip4, GTK_TYPE_SCROLLED_WINDOW,
                         G_IMPLEMENT_INTERFACE (ce_page_get_type (), ce_page_iface_init))

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
method_changed (CEPageIP4 *self)
{
        gboolean addr_enabled;
        gboolean dns_enabled;
        gboolean routes_enabled;

        if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (self->disabled_radio)) ||
            gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (self->shared_radio))) {
                addr_enabled = FALSE;
                dns_enabled = FALSE;
                routes_enabled = FALSE;
        } else {
                addr_enabled = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (self->manual_radio));
                dns_enabled = !gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (self->local_radio));
                routes_enabled = !gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (self->local_radio));
        }

        gtk_widget_set_visible (GTK_WIDGET (self->address_box), addr_enabled);
        gtk_widget_set_sensitive (GTK_WIDGET (self->dns_entry), dns_enabled);
        gtk_widget_set_sensitive (GTK_WIDGET (self->routes_list), routes_enabled);
        gtk_widget_set_sensitive (GTK_WIDGET (self->never_default_check), routes_enabled);

        ce_page_changed (CE_PAGE (self));
}

static void
update_row_sensitivity (CEPageIP4 *self, GtkWidget *list)
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
update_row_gateway_sensitivity (CEPageIP4 *self)
{
        GList *children, *l;
        gint rows = 0;

        children = gtk_container_get_children (GTK_CONTAINER (self->address_list));
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
remove_row (CEPageIP4 *self, GtkButton *button)
{
        GtkWidget *list;
        GtkWidget *row;
        GtkWidget *row_box;

        row_box = gtk_widget_get_parent (GTK_WIDGET (button));
        row = gtk_widget_get_parent (row_box);
        list = gtk_widget_get_parent (row);

        gtk_container_remove (GTK_CONTAINER (list), row);

        ce_page_changed (CE_PAGE (self));

        update_row_sensitivity (self, list);
        if (list == self->address_list)
                update_row_gateway_sensitivity (self);
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

static void
add_address_row (CEPageIP4   *self,
                 const gchar *address,
                 const gchar *network,
                 const gchar *gateway)
{
        GtkWidget *row;
        GtkWidget *row_box;
        GtkWidget *widget;
        GtkWidget *delete_button;
        GtkWidget *image;

        row = gtk_list_box_row_new ();

        row_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
        gtk_style_context_add_class (gtk_widget_get_style_context (row_box), "linked");

        widget = gtk_entry_new ();
        g_signal_connect_object (widget, "changed", G_CALLBACK (ce_page_changed), self, G_CONNECT_SWAPPED);
        g_signal_connect_object (widget, "activate", G_CALLBACK (ensure_empty_address_row), self, G_CONNECT_SWAPPED);
        g_object_set_data (G_OBJECT (row), "address", widget);
        gtk_entry_set_text (GTK_ENTRY (widget), address);
        gtk_entry_set_width_chars (GTK_ENTRY (widget), 16);
        gtk_widget_set_hexpand (widget, TRUE);
        gtk_container_add (GTK_CONTAINER (row_box), widget);

        widget = gtk_entry_new ();
        g_signal_connect_object (widget, "changed", G_CALLBACK (ce_page_changed), self, G_CONNECT_SWAPPED);
        g_signal_connect_object (widget, "activate", G_CALLBACK (ensure_empty_address_row), self, G_CONNECT_SWAPPED);
        g_object_set_data (G_OBJECT (row), "network", widget);
        gtk_entry_set_text (GTK_ENTRY (widget), network);
        gtk_entry_set_width_chars (GTK_ENTRY (widget), 16);
        gtk_widget_set_hexpand (widget, TRUE);
        gtk_container_add (GTK_CONTAINER (row_box), widget);

        widget = gtk_entry_new ();
        g_signal_connect_object (widget, "changed", G_CALLBACK (ce_page_changed), self, G_CONNECT_SWAPPED);
        g_signal_connect_object (widget, "activate", G_CALLBACK (ensure_empty_address_row), self, G_CONNECT_SWAPPED);
        g_object_set_data (G_OBJECT (row), "gateway", widget);
        gtk_entry_set_text (GTK_ENTRY (widget), gateway ? gateway : "");
        gtk_entry_set_width_chars (GTK_ENTRY (widget), 16);
        gtk_widget_set_hexpand (widget, TRUE);
        gtk_container_add (GTK_CONTAINER (row_box), widget);

        delete_button = gtk_button_new ();
        gtk_widget_set_sensitive (delete_button, FALSE);
        gtk_style_context_add_class (gtk_widget_get_style_context (delete_button), "image-button");
        g_signal_connect_object (delete_button, "clicked", G_CALLBACK (remove_row), self, G_CONNECT_SWAPPED);
        image = gtk_image_new_from_icon_name ("edit-delete-symbolic", GTK_ICON_SIZE_MENU);
        atk_object_set_name (gtk_widget_get_accessible (delete_button), _("Delete Address"));
        gtk_button_set_image (GTK_BUTTON (delete_button), image);
        gtk_container_add (GTK_CONTAINER (row_box), delete_button);
        g_object_set_data (G_OBJECT (row), "delete-button", delete_button);

        gtk_size_group_add_widget (self->address_sizegroup, delete_button);

        gtk_container_add (GTK_CONTAINER (row), row_box);
        gtk_widget_show_all (row);
        gtk_container_add (GTK_CONTAINER (self->address_list), row);

        update_row_gateway_sensitivity (self);
        update_row_sensitivity (self, self->address_list);
}

static void
ensure_empty_address_row (CEPageIP4 *self)
{
        GList *children, *l;

        children = gtk_container_get_children (GTK_CONTAINER (self->address_list));
        l = children;

        while (l && l->next)
                l = l->next;

        /* Add the last, stub row if needed*/
        if (!l || validate_row (l->data))
                add_address_row (self, "", "", "");

        g_list_free (children);
}

static void
add_address_box (CEPageIP4 *self)
{
        GtkWidget *list;
        gint i;

        self->address_list = list = gtk_list_box_new ();
        gtk_list_box_set_selection_mode (GTK_LIST_BOX (list), GTK_SELECTION_NONE);
        gtk_list_box_set_header_func (GTK_LIST_BOX (list), cc_list_box_update_header_func, NULL, NULL);
        gtk_container_add (GTK_CONTAINER (self->address_box), list);

        for (i = 0; i < nm_setting_ip_config_get_num_addresses (self->setting); i++) {
                NMIPAddress *addr;
                struct in_addr tmp_addr;
                gchar network[INET_ADDRSTRLEN + 1];

                addr = nm_setting_ip_config_get_address (self->setting, i);
                if (!addr)
                        continue;

                tmp_addr.s_addr = nm_utils_ip4_prefix_to_netmask (nm_ip_address_get_prefix (addr));
                (void) inet_ntop (AF_INET, &tmp_addr, &network[0], sizeof (network));

                add_address_row (self,
                                 nm_ip_address_get_address (addr),
                                 network,
                                 i == 0 ? nm_setting_ip_config_get_gateway (self->setting) : "");
        }
        if (nm_setting_ip_config_get_num_addresses (self->setting) == 0)
                ensure_empty_address_row (self);

        gtk_widget_show_all (GTK_WIDGET (self->address_box));
}

static void
add_dns_section (CEPageIP4 *self)
{
        GString *string;
        gint i;

        gtk_switch_set_active (self->auto_dns_switch, !nm_setting_ip_config_get_ignore_auto_dns (self->setting));
        g_signal_connect_object (self->auto_dns_switch, "notify::active", G_CALLBACK (ce_page_changed), self, G_CONNECT_SWAPPED);

        string = g_string_new ("");

        for (i = 0; i < nm_setting_ip_config_get_num_dns (self->setting); i++) {
                const char *address;

                address = nm_setting_ip_config_get_dns (self->setting, i);

                if (i > 0)
                        g_string_append (string, ", ");

                g_string_append (string, address);
        }

        gtk_entry_set_text (self->dns_entry, string->str);

        g_signal_connect_object (self->dns_entry, "notify::text", G_CALLBACK (ce_page_changed), self, G_CONNECT_SWAPPED);

        g_string_free (string, TRUE);
}

static void
add_route_row (CEPageIP4   *self,
               const gchar *address,
               const gchar *netmask,
               const gchar *gateway,
               gint         metric)
{
        GtkWidget *row;
        GtkWidget *row_box;
        GtkWidget *widget;
        GtkWidget *delete_button;
        GtkWidget *image;

        row = gtk_list_box_row_new ();

        row_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
        gtk_style_context_add_class (gtk_widget_get_style_context (row_box), "linked");

        widget = gtk_entry_new ();
        g_signal_connect_object (widget, "changed", G_CALLBACK (ce_page_changed), self, G_CONNECT_SWAPPED);
        g_signal_connect_object (widget, "activate", G_CALLBACK (ensure_empty_routes_row), self, G_CONNECT_SWAPPED);
        g_object_set_data (G_OBJECT (row), "address", widget);
        gtk_entry_set_text (GTK_ENTRY (widget), address);
        gtk_entry_set_width_chars (GTK_ENTRY (widget), 16);
        gtk_widget_set_hexpand (widget, TRUE);
        gtk_container_add (GTK_CONTAINER (row_box), widget);

        widget = gtk_entry_new ();
        g_signal_connect_object (widget, "changed", G_CALLBACK (ce_page_changed), self, G_CONNECT_SWAPPED);
        g_signal_connect_object (widget, "activate", G_CALLBACK (ensure_empty_routes_row), self, G_CONNECT_SWAPPED);
        g_object_set_data (G_OBJECT (row), "netmask", widget);
        gtk_entry_set_text (GTK_ENTRY (widget), netmask);
        gtk_entry_set_width_chars (GTK_ENTRY (widget), 16);
        gtk_widget_set_hexpand (widget, TRUE);
        gtk_container_add (GTK_CONTAINER (row_box), widget);

        widget = gtk_entry_new ();
        g_signal_connect_object (widget, "changed", G_CALLBACK (ce_page_changed), self, G_CONNECT_SWAPPED);
        g_signal_connect_object (widget, "activate", G_CALLBACK (ensure_empty_routes_row), self, G_CONNECT_SWAPPED);
        g_object_set_data (G_OBJECT (row), "gateway", widget);
        gtk_entry_set_text (GTK_ENTRY (widget), gateway ? gateway : "");
        gtk_entry_set_width_chars (GTK_ENTRY (widget), 16);
        gtk_widget_set_hexpand (widget, TRUE);
        gtk_container_add (GTK_CONTAINER (row_box), widget);

        widget = gtk_entry_new ();
        g_signal_connect_object (widget, "changed", G_CALLBACK (ce_page_changed), self, G_CONNECT_SWAPPED);
        g_signal_connect_object (widget, "activate", G_CALLBACK (ensure_empty_routes_row), self, G_CONNECT_SWAPPED);
        g_object_set_data (G_OBJECT (row), "metric", widget);
        if (metric >= 0) {
                g_autofree gchar *s = g_strdup_printf ("%d", metric);
                gtk_entry_set_text (GTK_ENTRY (widget), s);
        }
        gtk_entry_set_width_chars (GTK_ENTRY (widget), 5);
        gtk_widget_set_hexpand (widget, TRUE);
        gtk_container_add (GTK_CONTAINER (row_box), widget);

        gtk_size_group_add_widget (self->routes_metric_sizegroup, widget);

        delete_button = gtk_button_new ();
        gtk_style_context_add_class (gtk_widget_get_style_context (delete_button), "image-button");
        g_signal_connect_object (delete_button, "clicked", G_CALLBACK (remove_row), self, G_CONNECT_SWAPPED);
        image = gtk_image_new_from_icon_name ("edit-delete-symbolic", GTK_ICON_SIZE_MENU);
        atk_object_set_name (gtk_widget_get_accessible (delete_button), _("Delete Route"));
        gtk_button_set_image (GTK_BUTTON (delete_button), image);
        gtk_widget_set_halign (delete_button, GTK_ALIGN_CENTER);
        gtk_widget_set_valign (delete_button, GTK_ALIGN_CENTER);
        gtk_container_add (GTK_CONTAINER (row_box), delete_button);
        g_object_set_data (G_OBJECT (row), "delete-button", delete_button);

        gtk_size_group_add_widget (self->routes_sizegroup, delete_button);

        gtk_container_add (GTK_CONTAINER (row), row_box);
        gtk_widget_show_all (row);
        gtk_container_add (GTK_CONTAINER (self->routes_list), row);

        update_row_sensitivity (self, self->routes_list);
}

static void
ensure_empty_routes_row (CEPageIP4 *self)
{
        GList *children, *l;

        children = gtk_container_get_children (GTK_CONTAINER (self->routes_list));
        l = children;

        while (l && l->next)
                l = l->next;

        /* Add the last, stub row if needed*/
        if (!l || validate_row (l->data))
                add_route_row (self, "", "", "", -1);

        g_list_free (children);
}

static void
add_routes_box (CEPageIP4 *self)
{
        GtkWidget *list;
        gint i;

        self->routes_list = list = gtk_list_box_new ();
        gtk_list_box_set_selection_mode (GTK_LIST_BOX (list), GTK_SELECTION_NONE);
        gtk_list_box_set_header_func (GTK_LIST_BOX (list), cc_list_box_update_header_func, NULL, NULL);
        gtk_container_add (GTK_CONTAINER (self->routes_box), list);
        gtk_switch_set_active (self->auto_routes_switch, !nm_setting_ip_config_get_ignore_auto_routes (self->setting));
        g_signal_connect_object (self->auto_routes_switch, "notify::active", G_CALLBACK (ce_page_changed), self, G_CONNECT_SWAPPED);

        for (i = 0; i < nm_setting_ip_config_get_num_routes (self->setting); i++) {
                NMIPRoute *route;
                struct in_addr tmp_addr;
                gchar netmask[INET_ADDRSTRLEN + 1];

                route = nm_setting_ip_config_get_route (self->setting, i);
                if (!route)
                        continue;

                tmp_addr.s_addr = nm_utils_ip4_prefix_to_netmask (nm_ip_route_get_prefix (route));
                (void) inet_ntop (AF_INET, &tmp_addr, &netmask[0], sizeof (netmask));

                add_route_row (self,
                               nm_ip_route_get_dest (route),
                               netmask,
                               nm_ip_route_get_next_hop (route),
                               nm_ip_route_get_metric (route));
        }
        if (nm_setting_ip_config_get_num_routes (self->setting) == 0)
                ensure_empty_routes_row (self);

        gtk_widget_show_all (GTK_WIDGET (self->routes_box));
}

static void
connect_ip4_page (CEPageIP4 *self)
{
        const gchar *str_method;
        guint method;

        add_address_box (self);
        add_dns_section (self);
        add_routes_box (self);

        str_method = nm_setting_ip_config_get_method (self->setting);
        g_signal_connect_object (self->disabled_radio, "notify::active", G_CALLBACK (ce_page_changed), self, G_CONNECT_SWAPPED);
        g_object_bind_property (self->disabled_radio, "active",
                                self->content_box, "sensitive",
                                G_BINDING_SYNC_CREATE | G_BINDING_INVERT_BOOLEAN);

        g_signal_connect_object (self->shared_radio, "notify::active", G_CALLBACK (ce_page_changed), self, G_CONNECT_SWAPPED);
        g_object_bind_property (self->shared_radio, "active",
                                self->content_box, "sensitive",
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

        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (self->never_default_check),
                                      nm_setting_ip_config_get_never_default (self->setting));
        g_signal_connect_object (self->never_default_check, "toggled", G_CALLBACK (ce_page_changed), self, G_CONNECT_SWAPPED);

        g_signal_connect_object (self->automatic_radio, "toggled", G_CALLBACK (method_changed), self, G_CONNECT_SWAPPED);
        g_signal_connect_object (self->local_radio, "toggled", G_CALLBACK (method_changed), self, G_CONNECT_SWAPPED);
        g_signal_connect_object (self->manual_radio, "toggled", G_CALLBACK (method_changed), self, G_CONNECT_SWAPPED);
        g_signal_connect_object (self->disabled_radio, "toggled", G_CALLBACK (method_changed), self, G_CONNECT_SWAPPED);

        switch (method) {
        case IP4_METHOD_AUTO:
                gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (self->automatic_radio), TRUE);
                break;
        case IP4_METHOD_LINK_LOCAL:
                gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (self->local_radio), TRUE);
                break;
        case IP4_METHOD_MANUAL:
                gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (self->manual_radio), TRUE);
                break;
        case IP4_METHOD_SHARED:
                gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (self->shared_radio), TRUE);
                break;
        case IP4_METHOD_DISABLED:
                gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (self->disabled_radio), TRUE);
                break;
        default:
                break;
        }

        method_changed (self);
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
ui_to_setting (CEPageIP4 *self)
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

        if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (self->disabled_radio)))
                method = NM_SETTING_IP4_CONFIG_METHOD_DISABLED;
        else if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (self->automatic_radio)))
                method = NM_SETTING_IP4_CONFIG_METHOD_AUTO;
        else if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (self->local_radio)))
                method = NM_SETTING_IP4_CONFIG_METHOD_LINK_LOCAL;
        else if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (self->manual_radio)))
                method = NM_SETTING_IP4_CONFIG_METHOD_MANUAL;
        else if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (self->shared_radio)))
                method = NM_SETTING_IP4_CONFIG_METHOD_SHARED;

        addresses = g_ptr_array_new_with_free_func ((GDestroyNotify) nm_ip_address_unref);
        if (g_str_equal (method, NM_SETTING_IP4_CONFIG_METHOD_MANUAL))
                children = gtk_container_get_children (GTK_CONTAINER (self->address_list));
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
                        ensure_empty_address_row (self);
        }
        g_list_free (children);

        if (addresses->len == 0) {
                g_ptr_array_free (addresses, TRUE);
                addresses = NULL;
        }

        dns_servers = g_ptr_array_new_with_free_func (g_free);
        dns_text = g_strstrip (g_strdup (gtk_entry_get_text (GTK_ENTRY (self->dns_entry))));
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
                        widget_set_error (GTK_WIDGET (self->dns_entry));
                        ret = FALSE;
                        break;
                } else {
                        widget_unset_error (GTK_WIDGET (self->dns_entry));
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
                children = gtk_container_get_children (GTK_CONTAINER (self->routes_list));
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
                        ensure_empty_routes_row (self);
        }
        g_list_free (children);

        if (routes->len == 0) {
                g_ptr_array_free (routes, TRUE);
                routes = NULL;
        }

        if (!ret)
                goto out;

        ignore_auto_dns = !gtk_switch_get_active (self->auto_dns_switch);
        ignore_auto_routes = !gtk_switch_get_active (self->auto_routes_switch);
        never_default = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (self->never_default_check));

        g_object_set (self->setting,
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

static const gchar *
ce_page_ip4_get_title (CEPage *page)
{
        return _("IPv4");
}

static gboolean
ce_page_ip4_validate (CEPage        *self,
                      NMConnection  *connection,
                      GError       **error)
{
        if (!ui_to_setting (CE_PAGE_IP4 (self)))
                return FALSE;

        return nm_setting_verify (NM_SETTING (CE_PAGE_IP4 (self)->setting), NULL, error);
}

static void
ce_page_ip4_init (CEPageIP4 *self)
{
        gtk_widget_init_template (GTK_WIDGET (self));
}

static void
ce_page_ip4_class_init (CEPageIP4Class *klass)
{
        GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

        gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/network/ip4-page.ui");

        gtk_widget_class_bind_template_child (widget_class, CEPageIP4, address_box);
        gtk_widget_class_bind_template_child (widget_class, CEPageIP4, address_sizegroup);
        gtk_widget_class_bind_template_child (widget_class, CEPageIP4, auto_dns_switch);
        gtk_widget_class_bind_template_child (widget_class, CEPageIP4, auto_routes_switch);
        gtk_widget_class_bind_template_child (widget_class, CEPageIP4, automatic_radio);
        gtk_widget_class_bind_template_child (widget_class, CEPageIP4, content_box);
        gtk_widget_class_bind_template_child (widget_class, CEPageIP4, disabled_radio);
        gtk_widget_class_bind_template_child (widget_class, CEPageIP4, dns_entry);
        gtk_widget_class_bind_template_child (widget_class, CEPageIP4, local_radio);
        gtk_widget_class_bind_template_child (widget_class, CEPageIP4, manual_radio);
        gtk_widget_class_bind_template_child (widget_class, CEPageIP4, never_default_check);
        gtk_widget_class_bind_template_child (widget_class, CEPageIP4, routes_box);
        gtk_widget_class_bind_template_child (widget_class, CEPageIP4, routes_metric_sizegroup);
        gtk_widget_class_bind_template_child (widget_class, CEPageIP4, routes_sizegroup);
        gtk_widget_class_bind_template_child (widget_class, CEPageIP4, shared_radio);
}

static void
ce_page_iface_init (CEPageInterface *iface)
{
        iface->get_title = ce_page_ip4_get_title;
        iface->validate = ce_page_ip4_validate;
}

CEPageIP4 *
ce_page_ip4_new (NMConnection     *connection,
                 NMClient         *client)
{
        CEPageIP4 *self;

        self = CE_PAGE_IP4 (g_object_new (ce_page_ip4_get_type (), NULL));

        self->setting = nm_connection_get_setting_ip4_config (connection);
        if (!self->setting) {
                self->setting = NM_SETTING_IP_CONFIG (nm_setting_ip4_config_new ());
                nm_connection_add_setting (connection, NM_SETTING (self->setting));
        }

        connect_ip4_page (self);

        return self;
}
