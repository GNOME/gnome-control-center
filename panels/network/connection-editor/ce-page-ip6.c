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

#include "shell/list-box-helper.h"
#include "ce-page-ip6.h"
#include "ui-helpers.h"

#define RADIO_IS_ACTIVE(x) (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (gtk_builder_get_object(CE_PAGE (page)->builder, x))))

static void ensure_empty_routes_row (CEPageIP6 *page);

G_DEFINE_TYPE (CEPageIP6, ce_page_ip6, CE_TYPE_PAGE)

enum {
        METHOD_COL_NAME,
        METHOD_COL_METHOD
};

enum {
        IP6_METHOD_AUTO,
        IP6_METHOD_DHCP,
        IP6_METHOD_MANUAL,
        IP6_METHOD_LINK_LOCAL,
        IP6_METHOD_SHARED,
        IP6_METHOD_IGNORE
};

static void
method_changed (GtkToggleButton *button, CEPageIP6 *page)
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
update_row_sensitivity (CEPageIP6 *page, GtkWidget *list)
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
remove_row (GtkButton *button, CEPageIP6 *page)
{
        GtkWidget *row;
        GtkWidget *row_box;
        GtkWidget *list;

        row_box = gtk_widget_get_parent (GTK_WIDGET (button));
        row = gtk_widget_get_parent (row_box);
        list = gtk_widget_get_parent (row);

        gtk_container_remove (GTK_CONTAINER (list), row);

        ce_page_changed (CE_PAGE (page));

        update_row_sensitivity (page, list);
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
add_address_row (CEPageIP6   *page,
                 const gchar *address,
                 const gchar *network,
                 const gchar *gateway)
{
        GtkWidget *row;
        GtkWidget *row_grid;
        GtkWidget *label;
        GtkWidget *widget;
        GtkWidget *delete_button;
        GtkWidget *image;

        row = gtk_list_box_row_new ();

        row_grid = gtk_grid_new ();
        label = gtk_label_new (_("Address"));
        gtk_widget_set_halign (label, GTK_ALIGN_END);
        gtk_grid_attach (GTK_GRID (row_grid), label, 1, 1, 1, 1);
        widget = gtk_entry_new ();
        gtk_label_set_mnemonic_widget (GTK_LABEL (label), widget);
        g_signal_connect_swapped (widget, "changed", G_CALLBACK (ce_page_changed), page);
        g_object_set_data (G_OBJECT (row), "address", widget);
        gtk_entry_set_text (GTK_ENTRY (widget), address);
        gtk_widget_set_margin_start (widget, 10);
        gtk_widget_set_margin_end (widget, 10);
        gtk_widget_set_hexpand (widget, TRUE);
        gtk_grid_attach (GTK_GRID (row_grid), widget, 2, 1, 1, 1);

        label = gtk_label_new (_("Prefix"));
        gtk_widget_set_halign (label, GTK_ALIGN_END);
        gtk_grid_attach (GTK_GRID (row_grid), label, 1, 2, 1, 1);
        widget = gtk_entry_new ();
        gtk_label_set_mnemonic_widget (GTK_LABEL (label), widget);
        g_signal_connect_swapped (widget, "changed", G_CALLBACK (ce_page_changed), page);
        g_object_set_data (G_OBJECT (row), "prefix", widget);
        gtk_entry_set_text (GTK_ENTRY (widget), network);
        gtk_widget_set_margin_start (widget, 10);
        gtk_widget_set_margin_end (widget, 10);
        gtk_widget_set_hexpand (widget, TRUE);
        gtk_grid_attach (GTK_GRID (row_grid), widget, 2, 2, 1, 1);

        label = gtk_label_new (_("Gateway"));
        gtk_widget_set_halign (label, GTK_ALIGN_END);
        gtk_grid_attach (GTK_GRID (row_grid), label, 1, 3, 1, 1);
        widget = gtk_entry_new ();
        gtk_label_set_mnemonic_widget (GTK_LABEL (label), widget);
        g_signal_connect_swapped (widget, "changed", G_CALLBACK (ce_page_changed), page);
        g_object_set_data (G_OBJECT (row), "gateway", widget);
        gtk_entry_set_text (GTK_ENTRY (widget), gateway ? gateway : "");
        gtk_widget_set_margin_start (widget, 10);
        gtk_widget_set_margin_end (widget, 10);
        gtk_widget_set_hexpand (widget, TRUE);
        gtk_grid_attach (GTK_GRID (row_grid), widget, 2, 3, 1, 1);

        delete_button = gtk_button_new ();
        gtk_style_context_add_class (gtk_widget_get_style_context (delete_button), "image-button");
        g_signal_connect (delete_button, "clicked", G_CALLBACK (remove_row), page);
        image = gtk_image_new_from_icon_name ("user-trash-symbolic", GTK_ICON_SIZE_MENU);
        atk_object_set_name (gtk_widget_get_accessible (delete_button), _("Delete Address"));
        gtk_button_set_image (GTK_BUTTON (delete_button), image);
        gtk_grid_attach (GTK_GRID (row_grid), delete_button, 3, 2, 1, 1);
        g_object_set_data (G_OBJECT (row), "delete-button", delete_button);

        gtk_grid_set_row_spacing (GTK_GRID (row_grid), 10);
        gtk_widget_set_margin_start (row_grid, 10);
        gtk_widget_set_margin_end (row_grid, 10);
        gtk_widget_set_margin_top (row_grid, 10);
        gtk_widget_set_margin_bottom (row_grid, 10);
        gtk_widget_set_halign (row_grid, GTK_ALIGN_FILL);

        gtk_container_add (GTK_CONTAINER (row), row_grid);
        gtk_widget_show_all (row);
        gtk_container_add (GTK_CONTAINER (page->address_list), row);

        update_row_sensitivity (page, page->address_list);
}

static void
add_empty_address_row (CEPageIP6 *page)
{
        add_address_row (page, "", "", "");
}

static void
add_section_toolbar (CEPageIP6 *page, GtkWidget *section, GCallback add_cb)
{
        GtkWidget *toolbar;
        GtkToolItem *item;
        GtkStyleContext *context;
        GtkWidget *box;
        GtkWidget *button;
        GtkWidget *image;

        toolbar = gtk_toolbar_new ();
        gtk_toolbar_set_style (GTK_TOOLBAR (toolbar), GTK_TOOLBAR_ICONS);
        gtk_toolbar_set_icon_size (GTK_TOOLBAR (toolbar), GTK_ICON_SIZE_MENU);
        context = gtk_widget_get_style_context (toolbar);
        gtk_style_context_set_junction_sides (context, GTK_JUNCTION_TOP);
        gtk_style_context_add_class (context, GTK_STYLE_CLASS_INLINE_TOOLBAR);
        gtk_container_add (GTK_CONTAINER (section), toolbar);

        item = gtk_separator_tool_item_new ();
        gtk_tool_item_set_expand (item, TRUE);
        gtk_separator_tool_item_set_draw (GTK_SEPARATOR_TOOL_ITEM (item), FALSE);
        gtk_toolbar_insert (GTK_TOOLBAR (toolbar), GTK_TOOL_ITEM (item), 0);

        box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
        item = gtk_tool_item_new ();
        gtk_container_add (GTK_CONTAINER (item), box);
        button = gtk_button_new ();
        g_signal_connect_swapped (button, "clicked", G_CALLBACK (add_cb), page);
        image = gtk_image_new_from_icon_name ("list-add-symbolic", GTK_ICON_SIZE_MENU);
        atk_object_set_name (gtk_widget_get_accessible (button), _("Add"));
        gtk_button_set_image (GTK_BUTTON (button), image);
        gtk_container_add (GTK_CONTAINER (box), button);
        gtk_toolbar_insert (GTK_TOOLBAR (toolbar), GTK_TOOL_ITEM (item), 1);
}

static void
add_address_section (CEPageIP6 *page)
{
        GtkWidget *widget;
        GtkWidget *frame;
        GtkWidget *list;
        gint i;

        widget = GTK_WIDGET (gtk_builder_get_object (CE_PAGE (page)->builder, "address_section"));

        frame = gtk_frame_new (NULL);
        gtk_container_add (GTK_CONTAINER (widget), frame);
        page->address_list = list = gtk_list_box_new ();
        gtk_list_box_set_selection_mode (GTK_LIST_BOX (list), GTK_SELECTION_NONE);
        gtk_list_box_set_header_func (GTK_LIST_BOX (list), cc_list_box_update_header_func, NULL, NULL);
        gtk_list_box_set_sort_func (GTK_LIST_BOX (list), (GtkListBoxSortFunc)sort_first_last, NULL, NULL);
        gtk_container_add (GTK_CONTAINER (frame), list);

        add_section_toolbar (page, widget, G_CALLBACK (add_empty_address_row));

        for (i = 0; i < nm_setting_ip_config_get_num_addresses (page->setting); i++) {
                NMIPAddress *addr;
                char *netmask;

                addr = nm_setting_ip_config_get_address (page->setting, i);
                netmask = g_strdup_printf ("%u", nm_ip_address_get_prefix (addr));
                add_address_row (page, nm_ip_address_get_address (addr), netmask,
                                 i == 0 ? nm_setting_ip_config_get_gateway (page->setting) : NULL);
                g_free (netmask);
        }
        if (nm_setting_ip_config_get_num_addresses (page->setting) == 0)
                add_empty_address_row (page);

        gtk_widget_show_all (widget);
}

static void
add_dns_section (CEPageIP6 *page)
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

        g_signal_connect_swapped (page->dns_entry, "notify::text", G_CALLBACK (ce_page_changed), page);

        g_string_free (string, TRUE);
}

static void
add_route_row (CEPageIP6   *page,
               const gchar *address,
               const gchar *prefix,
               const gchar *gateway,
               const gchar *metric)
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
        gtk_entry_set_width_chars (GTK_ENTRY (widget), 16);
        gtk_widget_set_hexpand (widget, TRUE);
        gtk_container_add (GTK_CONTAINER (row_box), widget);

        widget = gtk_entry_new ();
        g_signal_connect_swapped (widget, "changed", G_CALLBACK (ce_page_changed), page);
        g_signal_connect_swapped (widget, "activate", G_CALLBACK (ensure_empty_routes_row), page);
        g_object_set_data (G_OBJECT (row), "prefix", widget);
        gtk_entry_set_text (GTK_ENTRY (widget), prefix ? prefix : "");
        gtk_entry_set_width_chars (GTK_ENTRY (widget), 16);
        gtk_widget_set_hexpand (widget, TRUE);
        gtk_container_add (GTK_CONTAINER (row_box), widget);

        widget = gtk_entry_new ();
        g_signal_connect_swapped (widget, "changed", G_CALLBACK (ce_page_changed), page);
        g_signal_connect_swapped (widget, "activate", G_CALLBACK (ensure_empty_routes_row), page);
        g_object_set_data (G_OBJECT (row), "gateway", widget);
        gtk_entry_set_text (GTK_ENTRY (widget), gateway);
        gtk_entry_set_width_chars (GTK_ENTRY (widget), 16);
        gtk_widget_set_hexpand (widget, TRUE);
        gtk_container_add (GTK_CONTAINER (row_box), widget);

        widget = gtk_entry_new ();
        g_signal_connect_swapped (widget, "changed", G_CALLBACK (ce_page_changed), page);
        g_signal_connect_swapped (widget, "activate", G_CALLBACK (ensure_empty_routes_row), page);
        g_object_set_data (G_OBJECT (row), "metric", widget);
        gtk_entry_set_text (GTK_ENTRY (widget), metric ? metric : "");
        gtk_entry_set_width_chars (GTK_ENTRY (widget), 5);
        gtk_widget_set_hexpand (widget, TRUE);
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

        group = GTK_SIZE_GROUP (gtk_builder_get_object (CE_PAGE (page)->builder, "routes_sizegroup"));
        gtk_size_group_add_widget (group, delete_button);

        gtk_container_add (GTK_CONTAINER (row), row_box);
        gtk_widget_show_all (row);
        gtk_container_add (GTK_CONTAINER (page->routes_list), row);

        update_row_sensitivity (page, page->routes_list);
}

static void
ensure_empty_routes_row (CEPageIP6 *page)
{
        GList *children, *l;

        children = gtk_container_get_children (GTK_CONTAINER (page->routes_list));
        l = children;

        while (l && l->next)
                l = l->next;

        /* Add the last, stub row if needed*/
        if (!l || validate_row (l->data))
                add_route_row (page, "", NULL, "", NULL);

        g_list_free (children);
}

static void
add_empty_route_row (CEPageIP6 *page)
{
        add_route_row (page, "", NULL, "", NULL);
}

static void
add_routes_section (CEPageIP6 *page)
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
                char *prefix, *metric;

                route = nm_setting_ip_config_get_route (page->setting, i);
                prefix = g_strdup_printf ("%u", nm_ip_route_get_prefix (route));
                metric = g_strdup_printf ("%u", (guint32) MIN (0, nm_ip_route_get_metric (route)));
                add_route_row (page, nm_ip_route_get_dest (route),
                               prefix,
                               nm_ip_route_get_next_hop (route),
                               metric);
                g_free (prefix);
                g_free (metric);
        }
        if (nm_setting_ip_config_get_num_routes (page->setting) == 0)
                add_empty_route_row (page);

        gtk_widget_show_all (widget);
}

enum
{
        RADIO_AUTOMATIC,
        RADIO_DHCP,
        RADIO_LOCAL,
        RADIO_MANUAL,
        RADIO_DISABLED,
        N_RADIO
};

static void
connect_ip6_page (CEPageIP6 *page)
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
        disabled = g_strcmp0 (str_method, NM_SETTING_IP6_CONFIG_METHOD_IGNORE) == 0;
        gtk_toggle_button_set_active (page->disabled, disabled);
        g_signal_connect_swapped (page->disabled, "notify::active", G_CALLBACK (ce_page_changed), page);
        content = GTK_WIDGET (gtk_builder_get_object (CE_PAGE (page)->builder, "page_content"));
        g_object_bind_property (page->disabled, "active",
                                content, "sensitive",
                                G_BINDING_SYNC_CREATE | G_BINDING_INVERT_BOOLEAN);

        method = IP6_METHOD_AUTO;
        if (g_strcmp0 (str_method, NM_SETTING_IP6_CONFIG_METHOD_DHCP) == 0) {
                method = IP6_METHOD_DHCP;
        } else if (g_strcmp0 (str_method, NM_SETTING_IP6_CONFIG_METHOD_LINK_LOCAL) == 0) {
                method = IP6_METHOD_LINK_LOCAL;
        } else if (g_strcmp0 (str_method, NM_SETTING_IP6_CONFIG_METHOD_MANUAL) == 0) {
                method = IP6_METHOD_MANUAL;
        } else if (g_strcmp0 (str_method, NM_SETTING_IP6_CONFIG_METHOD_SHARED) == 0) {
                method = IP6_METHOD_SHARED;
        } else if (g_strcmp0 (str_method, NM_SETTING_IP6_CONFIG_METHOD_IGNORE) == 0) {
                method = IP6_METHOD_IGNORE;
        }

        page->never_default = GTK_WIDGET (gtk_builder_get_object (CE_PAGE (page)->builder, "never_default_check"));
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (page->never_default),
                                      nm_setting_ip_config_get_never_default (page->setting));
        g_signal_connect_swapped (page->never_default, "toggled", G_CALLBACK (ce_page_changed), page);


        /* Connect radio buttons */
        radios[RADIO_AUTOMATIC] = GTK_TOGGLE_BUTTON (gtk_builder_get_object (CE_PAGE (page)->builder, "radio_automatic"));
        radios[RADIO_DHCP] = GTK_TOGGLE_BUTTON (gtk_builder_get_object (CE_PAGE (page)->builder, "radio_dhcp"));
        radios[RADIO_LOCAL] = GTK_TOGGLE_BUTTON (gtk_builder_get_object (CE_PAGE (page)->builder, "radio_local"));
        radios[RADIO_MANUAL] = GTK_TOGGLE_BUTTON (gtk_builder_get_object (CE_PAGE (page)->builder, "radio_manual"));
        radios[RADIO_DISABLED] = page->disabled;

        for (i = RADIO_AUTOMATIC; i < RADIO_DISABLED; i++)
                g_signal_connect (radios[i], "toggled", G_CALLBACK (method_changed), page);

        switch (method) {
        case IP6_METHOD_AUTO:
                gtk_toggle_button_set_active (radios[RADIO_AUTOMATIC], TRUE);
                break;
        case IP6_METHOD_DHCP:
                gtk_toggle_button_set_active (radios[RADIO_DHCP], TRUE);
                break;
        case IP6_METHOD_LINK_LOCAL:
                gtk_toggle_button_set_active (radios[RADIO_LOCAL], TRUE);
                break;
        case IP6_METHOD_MANUAL:
                gtk_toggle_button_set_active (radios[RADIO_MANUAL], TRUE);
                break;
        case IP6_METHOD_IGNORE:
                gtk_toggle_button_set_active (radios[RADIO_DISABLED], TRUE);
                break;
        default:
                break;
        }

        method_changed (NULL, page);
}

static gboolean
ui_to_setting (CEPageIP6 *page)
{
        const gchar *method;
        gboolean ignore_auto_dns;
        gboolean ignore_auto_routes;
        gboolean never_default;
        GList *children, *l;
        gboolean ret = TRUE;
        GStrv dns_addresses = NULL;
        gchar *dns_text = NULL;
        guint i;

        if (gtk_toggle_button_get_active (page->disabled)) {
                method = NM_SETTING_IP6_CONFIG_METHOD_IGNORE;
        } else {
                if (RADIO_IS_ACTIVE ("radio_manual")) {
                        method = NM_SETTING_IP6_CONFIG_METHOD_MANUAL;
                } else if (RADIO_IS_ACTIVE ("radio_local")) {
                        method = NM_SETTING_IP6_CONFIG_METHOD_LINK_LOCAL;
                } else if (RADIO_IS_ACTIVE ("radio_dhcp")) {
                        method = NM_SETTING_IP6_CONFIG_METHOD_DHCP;
                } else if (RADIO_IS_ACTIVE ("radio_automatic")) {
                        method = NM_SETTING_IP6_CONFIG_METHOD_AUTO;
                }
        }

        nm_setting_ip_config_clear_addresses (page->setting);
        if (g_str_equal (method, NM_SETTING_IP6_CONFIG_METHOD_MANUAL)) {
                children = gtk_container_get_children (GTK_CONTAINER (page->address_list));
        } else {
                g_object_set (G_OBJECT (page->setting),
                              NM_SETTING_IP_CONFIG_GATEWAY, NULL,
                              NULL);
                children = NULL;
       }

        for (l = children; l; l = l->next) {
                GtkWidget *row = l->data;
                GtkEntry *entry;
                const gchar *text_address;
                const gchar *text_prefix;
                const gchar *text_gateway;
                guint32 prefix;
                gchar *end;
                NMIPAddress *addr;
                gboolean have_gateway = FALSE;

                entry = GTK_ENTRY (g_object_get_data (G_OBJECT (row), "address"));
                if (!entry)
                        continue;

                text_address = gtk_entry_get_text (entry);
                text_prefix = gtk_entry_get_text (GTK_ENTRY (g_object_get_data (G_OBJECT (row), "prefix")));
                text_gateway = gtk_entry_get_text (GTK_ENTRY (g_object_get_data (G_OBJECT (row), "gateway")));

                if (!*text_address && !*text_prefix && !*text_gateway) {
                        /* ignore empty rows */
                        widget_unset_error (GTK_WIDGET (entry));
                        widget_unset_error (g_object_get_data (G_OBJECT (row), "prefix"));
                        widget_unset_error (g_object_get_data (G_OBJECT (row), "gateway"));
                        continue;
                }

                if (!text_address || !nm_utils_ipaddr_valid (AF_INET6, text_address)) {
                        widget_set_error (GTK_WIDGET (entry));
                        ret = FALSE;
                } else {
                        widget_unset_error (GTK_WIDGET (entry));
                }

                prefix = strtoul (text_prefix, &end, 10);
                if (!end || *end || prefix == 0 || prefix > 128) {
                        widget_set_error (g_object_get_data (G_OBJECT (row), "prefix"));
                        ret = FALSE;
                } else {
                        widget_unset_error (g_object_get_data (G_OBJECT (row), "prefix"));
                }

                if (text_gateway && !nm_utils_ipaddr_valid (AF_INET6, text_gateway)) {
                        widget_set_error (g_object_get_data (G_OBJECT (row), "gateway"));
                        ret = FALSE;
                } else {
                        widget_unset_error (g_object_get_data (G_OBJECT (row), "gateway"));
                        have_gateway = TRUE;
                }

                if (!ret)
                        continue;

                addr = nm_ip_address_new (AF_INET6, text_address, prefix, NULL);
                if (have_gateway)
                        g_object_set (G_OBJECT (page->setting),
                                      NM_SETTING_IP_CONFIG_GATEWAY, text_gateway,
                                      NULL);
                nm_setting_ip_config_add_address (page->setting, addr);
        }
        g_list_free (children);

        nm_setting_ip_config_clear_dns (page->setting);
        dns_text = g_strstrip (g_strdup (gtk_entry_get_text (GTK_ENTRY (page->dns_entry))));

        if (g_str_equal (method, NM_SETTING_IP6_CONFIG_METHOD_AUTO) ||
            g_str_equal (method, NM_SETTING_IP6_CONFIG_METHOD_DHCP) ||
            g_str_equal (method, NM_SETTING_IP6_CONFIG_METHOD_MANUAL))
                dns_addresses = g_strsplit_set (dns_text, ", ", -1);
        else
                dns_addresses = NULL;

        for (i = 0; dns_addresses && dns_addresses[i]; i++) {
                const gchar *text;
                struct in6_addr tmp_addr;

                text = dns_addresses[i];

                if (!text || !*text)
                        continue;

                if (inet_pton (AF_INET6, text, &tmp_addr) <= 0) {
                        g_clear_pointer (&dns_addresses, g_strfreev);
                        widget_set_error (page->dns_entry);
                        ret = FALSE;
                        break;
                } else {
                        widget_unset_error (page->dns_entry);
                        nm_setting_ip_config_add_dns (page->setting, text);
                }
        }

        nm_setting_ip_config_clear_routes (page->setting);
        if (g_str_equal (method, NM_SETTING_IP6_CONFIG_METHOD_AUTO) ||
            g_str_equal (method, NM_SETTING_IP6_CONFIG_METHOD_DHCP) ||
            g_str_equal (method, NM_SETTING_IP6_CONFIG_METHOD_MANUAL))
                children = gtk_container_get_children (GTK_CONTAINER (page->routes_list));
        else
                children = NULL;

        for (l = children; l; l = l->next) {
                GtkWidget *row = l->data;
                GtkEntry *entry;
                const gchar *text_address;
                const gchar *text_prefix;
                const gchar *text_gateway;
                const gchar *text_metric;
                guint32 prefix, metric;
                gchar *end;
                NMIPRoute *route;

                entry = GTK_ENTRY (g_object_get_data (G_OBJECT (row), "address"));
                if (!entry)
                        continue;

                text_address = gtk_entry_get_text (entry);
                text_prefix = gtk_entry_get_text (GTK_ENTRY (g_object_get_data (G_OBJECT (row), "prefix")));
                text_gateway = gtk_entry_get_text (GTK_ENTRY (g_object_get_data (G_OBJECT (row), "gateway")));
                text_metric = gtk_entry_get_text (GTK_ENTRY (g_object_get_data (G_OBJECT (row), "metric")));

                if (!*text_address && !*text_prefix && !*text_gateway && !*text_metric) {
                        /* ignore empty rows */
                        widget_unset_error (GTK_WIDGET (entry));
                        widget_unset_error (g_object_get_data (G_OBJECT (row), "prefix"));
                        widget_unset_error (g_object_get_data (G_OBJECT (row), "gateway"));
                        widget_unset_error (g_object_get_data (G_OBJECT (row), "metric"));
                        continue;
                }

                if (!nm_utils_ipaddr_valid (AF_INET6, text_address)) {
                        widget_set_error (GTK_WIDGET (entry));
                        ret = FALSE;
                } else {
                        widget_unset_error (GTK_WIDGET (entry));
                }

                prefix = strtoul (text_prefix, &end, 10);
                if (!end || *end || prefix == 0 || prefix > 128) {
                        widget_set_error (g_object_get_data (G_OBJECT (row), "prefix"));
                        ret = FALSE;
                } else {
                        widget_unset_error (g_object_get_data (G_OBJECT (row), "prefix"));
                }

                if (!nm_utils_ipaddr_valid (AF_INET6, text_gateway)) {
                        widget_set_error (g_object_get_data (G_OBJECT (row), "gateway"));
                        ret = FALSE;
                } else {
                        widget_unset_error (g_object_get_data (G_OBJECT (row), "gateway"));
                }

                metric = 0;
                if (*text_metric) {
                        errno = 0;
                        metric = strtoul (text_metric, NULL, 10);
                        if (errno) {
                                widget_set_error (g_object_get_data (G_OBJECT (row), "metric"));
                                ret = FALSE;
                        } else {
                                widget_unset_error (g_object_get_data (G_OBJECT (row), "metric"));
                        }
                } else {
                        widget_unset_error (g_object_get_data (G_OBJECT (row), "metric"));
                }

                if (!ret)
                        continue;

                route = nm_ip_route_new (AF_INET6, text_address, prefix, text_gateway, metric, NULL);
                nm_setting_ip_config_add_route (page->setting, route);
                nm_ip_route_unref (route);

                if (!l || !l->next)
                        ensure_empty_routes_row (page);
        }
        g_list_free (children);

        if (!ret)
                goto out;

        ignore_auto_dns = !gtk_switch_get_active (page->auto_dns);
        ignore_auto_routes = !gtk_switch_get_active (page->auto_routes);
        never_default = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (page->never_default));

        g_object_set (page->setting,
                      NM_SETTING_IP_CONFIG_METHOD, method,
                      NM_SETTING_IP_CONFIG_IGNORE_AUTO_DNS, ignore_auto_dns,
                      NM_SETTING_IP_CONFIG_IGNORE_AUTO_ROUTES, ignore_auto_routes,
                      NM_SETTING_IP_CONFIG_NEVER_DEFAULT, never_default,
                      NULL);

out:
        g_clear_pointer (&dns_addresses, g_strfreev);
        g_clear_pointer (&dns_text, g_free);

        return ret;
}

static gboolean
validate (CEPage        *page,
          NMConnection  *connection,
          GError       **error)
{
        if (!ui_to_setting (CE_PAGE_IP6 (page)))
                return FALSE;

        return nm_setting_verify (NM_SETTING (CE_PAGE_IP6 (page)->setting), NULL, error);
}

static void
ce_page_ip6_init (CEPageIP6 *page)
{
}

static void
ce_page_ip6_class_init (CEPageIP6Class *class)
{
        CEPageClass *page_class= CE_PAGE_CLASS (class);

        page_class->validate = validate;
}

CEPage *
ce_page_ip6_new (NMConnection     *connection,
                 NMClient         *client)
{
        CEPageIP6 *page;

        page = CE_PAGE_IP6 (ce_page_new (CE_TYPE_PAGE_IP6,
                                           connection,
                                           client,
                                           "/org/gnome/control-center/network/ip6-page.ui",
                                           _("IPv6")));

        page->setting = nm_connection_get_setting_ip6_config (connection);
        if (!page->setting) {
                page->setting = NM_SETTING_IP_CONFIG (nm_setting_ip6_config_new ());
                nm_connection_add_setting (connection, NM_SETTING (page->setting));
        }

        connect_ip6_page (page);

        return CE_PAGE (page);
}
