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
#include <glib-object.h>
#include <glib/gi18n.h>

#include "shell/list-box-helper.h"
#include "ce-page-ip6.h"
#include "ui-helpers.h"
#include <nm-utils.h>

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
method_changed (GtkComboBox *combo, CEPageIP6 *page)
{
        gboolean addr_enabled;
        gboolean dns_enabled;
        gboolean routes_enabled;
        guint method;
        GtkWidget *widget;

        method = gtk_combo_box_get_active (combo);
        switch (method) {
        case IP6_METHOD_AUTO:
        case IP6_METHOD_DHCP:
                addr_enabled = FALSE;
                dns_enabled = TRUE;
                routes_enabled = TRUE;
                break;
        case IP6_METHOD_MANUAL:
                addr_enabled = TRUE;
                dns_enabled = TRUE;
                routes_enabled = TRUE;
                break;
        case IP6_METHOD_LINK_LOCAL:
        default:
                addr_enabled = FALSE;
                dns_enabled = FALSE;
                routes_enabled = FALSE;
                break;
        }

        widget = GTK_WIDGET (gtk_builder_get_object (CE_PAGE (page)->builder, "address_section"));
        gtk_widget_set_visible (widget, addr_enabled);
        gtk_widget_set_sensitive (page->dns_list, dns_enabled);
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
        gint rows = 0;

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
                        gtk_widget_set_sensitive (button, rows > 1);
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
        gtk_misc_set_alignment (GTK_MISC (label), 1, 0.5);
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
        gtk_misc_set_alignment (GTK_MISC (label), 1, 0.5);
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
        gtk_misc_set_alignment (GTK_MISC (label), 1, 0.5);
        gtk_grid_attach (GTK_GRID (row_grid), label, 1, 3, 1, 1);
        widget = gtk_entry_new ();
        gtk_label_set_mnemonic_widget (GTK_LABEL (label), widget);
        g_signal_connect_swapped (widget, "changed", G_CALLBACK (ce_page_changed), page);
        g_object_set_data (G_OBJECT (row), "gateway", widget);
        gtk_entry_set_text (GTK_ENTRY (widget), gateway);
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

        for (i = 0; i < nm_setting_ip6_config_get_num_addresses (page->setting); i++) {
                NMIP6Address *addr;
                const struct in6_addr *tmp_addr;
                gchar address[INET6_ADDRSTRLEN + 1];
                gchar network[INET6_ADDRSTRLEN + 1];
                gchar gateway[INET6_ADDRSTRLEN + 1];

                addr = nm_setting_ip6_config_get_address (page->setting, i);
                if (!addr)
                        continue;

                tmp_addr = nm_ip6_address_get_address (addr);
                (void) inet_ntop (AF_INET6, tmp_addr, &address[0], sizeof (address));

                snprintf (network, sizeof (network), "%u", nm_ip6_address_get_prefix (addr));

                tmp_addr = nm_ip6_address_get_gateway (addr);
                if (tmp_addr && !IN6_IS_ADDR_UNSPECIFIED (tmp_addr))
                        (void) inet_ntop (AF_INET6, tmp_addr, &gateway[0], sizeof (gateway));
                else
                        gateway[0] = '\0';

                add_address_row (page, address, network, gateway);
        }
        if (nm_setting_ip6_config_get_num_addresses (page->setting) == 0)
                add_empty_address_row (page);

        gtk_widget_show_all (widget);
}

static void
add_dns_row (CEPageIP6   *page,
             const gchar *address)
{
        GtkWidget *row;
        GtkWidget *row_box;
        GtkWidget *label;
        GtkWidget *widget;
        GtkWidget *delete_button;
        GtkWidget *image;

        row = gtk_list_box_row_new ();
        gtk_widget_set_can_focus (row, FALSE);

        row_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
        label = gtk_label_new (_("Server"));
        gtk_misc_set_alignment (GTK_MISC (label), 1, 0.5);
        gtk_box_pack_start (GTK_BOX (row_box), label, FALSE, FALSE, 0);
        widget = gtk_entry_new ();
        gtk_label_set_mnemonic_widget (GTK_LABEL (label), widget);
        g_signal_connect_swapped (widget, "changed", G_CALLBACK (ce_page_changed), page);
        g_object_set_data (G_OBJECT (row), "address", widget);
        gtk_entry_set_text (GTK_ENTRY (widget), address);
        gtk_widget_set_margin_start (widget, 10);
        gtk_widget_set_margin_end (widget, 10);
        gtk_widget_set_hexpand (widget, TRUE);
        gtk_box_pack_start (GTK_BOX (row_box), widget, TRUE, TRUE, 0);

        delete_button = gtk_button_new ();
        gtk_style_context_add_class (gtk_widget_get_style_context (delete_button), "image-button");
        g_signal_connect (delete_button, "clicked", G_CALLBACK (remove_row), page);
        image = gtk_image_new_from_icon_name ("user-trash-symbolic", GTK_ICON_SIZE_MENU);
        atk_object_set_name (gtk_widget_get_accessible (delete_button), _("Delete DNS Server"));
        gtk_button_set_image (GTK_BUTTON (delete_button), image);
        gtk_box_pack_start (GTK_BOX (row_box), delete_button, FALSE, FALSE, 0);
        g_object_set_data (G_OBJECT (row), "delete-button", delete_button);

        gtk_widget_set_margin_start (row_box, 10);
        gtk_widget_set_margin_end (row_box, 10);
        gtk_widget_set_margin_top (row_box, 10);
        gtk_widget_set_margin_bottom (row_box, 10);
        gtk_widget_set_halign (row_box, GTK_ALIGN_FILL);

        gtk_container_add (GTK_CONTAINER (row), row_box);
        gtk_widget_show_all (row);
        gtk_container_add (GTK_CONTAINER (page->dns_list), row);

        update_row_sensitivity (page, page->dns_list);
}

static void
add_empty_dns_row (CEPageIP6 *page)
{
        add_dns_row (page, "");
}

static void
add_dns_section (CEPageIP6 *page)
{
        GtkWidget *widget;
        GtkWidget *frame;
        GtkWidget *list;
        gint i;

        widget = GTK_WIDGET (gtk_builder_get_object (CE_PAGE (page)->builder, "dns_section"));

        frame = gtk_frame_new (NULL);
        gtk_container_add (GTK_CONTAINER (widget), frame);
        page->dns_list = list = gtk_list_box_new ();
        gtk_list_box_set_selection_mode (GTK_LIST_BOX (list), GTK_SELECTION_NONE);
        gtk_list_box_set_header_func (GTK_LIST_BOX (list), cc_list_box_update_header_func, NULL, NULL);
        gtk_list_box_set_sort_func (GTK_LIST_BOX (list), (GtkListBoxSortFunc)sort_first_last, NULL, NULL);
        gtk_container_add (GTK_CONTAINER (frame), list);
        page->auto_dns = GTK_SWITCH (gtk_builder_get_object (CE_PAGE (page)->builder, "auto_dns_switch"));
        gtk_switch_set_active (page->auto_dns, !nm_setting_ip6_config_get_ignore_auto_dns (page->setting));
        g_signal_connect (page->auto_dns, "notify::active", G_CALLBACK (switch_toggled), page);

        add_section_toolbar (page, widget, G_CALLBACK (add_empty_dns_row));

        for (i = 0; i < nm_setting_ip6_config_get_num_dns (page->setting); i++) {
                const struct in6_addr *tmp_addr;
                gchar address[INET6_ADDRSTRLEN + 1];

                tmp_addr = nm_setting_ip6_config_get_dns (page->setting, i);
                (void) inet_ntop (AF_INET, tmp_addr, &address[0], sizeof (address));

                add_dns_row (page, address);
        }
        if (nm_setting_ip6_config_get_num_dns (page->setting) == 0)
                add_empty_dns_row (page);

        gtk_widget_show_all (widget);
}

static void
add_route_row (CEPageIP6   *page,
               const gchar *address,
               gint         prefix,
               const gchar *gateway,
               gint         metric)
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
        gtk_misc_set_alignment (GTK_MISC (label), 1, 0.5);
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
        gtk_misc_set_alignment (GTK_MISC (label), 1, 0.5);
        gtk_grid_attach (GTK_GRID (row_grid), label, 1, 2, 1, 1);
        widget = gtk_entry_new ();
        gtk_label_set_mnemonic_widget (GTK_LABEL (label), widget);
        g_signal_connect_swapped (widget, "changed", G_CALLBACK (ce_page_changed), page);
        g_object_set_data (G_OBJECT (row), "prefix", widget);
        if (prefix > 0) {
                gchar *s = g_strdup_printf ("%d", prefix);
                gtk_entry_set_text (GTK_ENTRY (widget), s);
                g_free (s);
        }
        gtk_widget_set_margin_start (widget, 10);
        gtk_widget_set_margin_end (widget, 10);
        gtk_widget_set_hexpand (widget, TRUE);
        gtk_grid_attach (GTK_GRID (row_grid), widget, 2, 2, 1, 1);

        label = gtk_label_new (_("Gateway"));
        gtk_misc_set_alignment (GTK_MISC (label), 1, 0.5);
        gtk_grid_attach (GTK_GRID (row_grid), label, 1, 3, 1, 1);
        widget = gtk_entry_new ();
        gtk_label_set_mnemonic_widget (GTK_LABEL (label), widget);
        g_signal_connect_swapped (widget, "changed", G_CALLBACK (ce_page_changed), page);
        g_object_set_data (G_OBJECT (row), "gateway", widget);
        gtk_entry_set_text (GTK_ENTRY (widget), gateway);
        gtk_widget_set_margin_start (widget, 10);
        gtk_widget_set_margin_end (widget, 10);
        gtk_widget_set_hexpand (widget, TRUE);
        gtk_grid_attach (GTK_GRID (row_grid), widget, 2, 3, 1, 1);

        /* Translators: Please see https://en.wikipedia.org/wiki/Metrics_(networking) */
        label = gtk_label_new (C_("network parameters", "Metric"));
        gtk_misc_set_alignment (GTK_MISC (label), 1, 0.5);
        gtk_grid_attach (GTK_GRID (row_grid), label, 1, 4, 1, 1);
        widget = gtk_entry_new ();
        gtk_label_set_mnemonic_widget (GTK_LABEL (label), widget);
        g_signal_connect_swapped (widget, "changed", G_CALLBACK (ce_page_changed), page);
        g_object_set_data (G_OBJECT (row), "metric", widget);
        if (metric > 0) {
                gchar *s = g_strdup_printf ("%d", metric);
                gtk_entry_set_text (GTK_ENTRY (widget), s);
                g_free (s);
        }
        gtk_widget_set_margin_start (widget, 10);
        gtk_widget_set_margin_end (widget, 10);
        gtk_widget_set_hexpand (widget, TRUE);
        gtk_grid_attach (GTK_GRID (row_grid), widget, 2, 4, 1, 1);

        delete_button = gtk_button_new ();
        gtk_style_context_add_class (gtk_widget_get_style_context (delete_button), "image-button");
        g_signal_connect (delete_button, "clicked", G_CALLBACK (remove_row), page);
        image = gtk_image_new_from_icon_name ("user-trash-symbolic", GTK_ICON_SIZE_MENU);
        atk_object_set_name (gtk_widget_get_accessible (delete_button), _("Delete Route"));
        gtk_button_set_image (GTK_BUTTON (delete_button), image);
        gtk_widget_set_halign (delete_button, GTK_ALIGN_CENTER);
        gtk_widget_set_valign (delete_button, GTK_ALIGN_CENTER);
        gtk_grid_attach (GTK_GRID (row_grid), delete_button, 3, 1, 1, 4);
        g_object_set_data (G_OBJECT (row), "delete-button", delete_button);

        gtk_grid_set_row_spacing (GTK_GRID (row_grid), 10);
        gtk_widget_set_margin_start (row_grid, 10);
        gtk_widget_set_margin_end (row_grid, 10);
        gtk_widget_set_margin_top (row_grid, 10);
        gtk_widget_set_margin_bottom (row_grid, 10);
        gtk_widget_set_halign (row_grid, GTK_ALIGN_FILL);

        gtk_container_add (GTK_CONTAINER (row), row_grid);
        gtk_widget_show_all (row);
        gtk_container_add (GTK_CONTAINER (page->routes_list), row);

        update_row_sensitivity (page, page->routes_list);
}

static void
add_empty_route_row (CEPageIP6 *page)
{
        add_route_row (page, "", 0, "", 0);
}

static void
add_routes_section (CEPageIP6 *page)
{
        GtkWidget *widget;
        GtkWidget *frame;
        GtkWidget *list;
        gint i;

        widget = GTK_WIDGET (gtk_builder_get_object (CE_PAGE (page)->builder, "routes_section"));

        frame = gtk_frame_new (NULL);
        gtk_container_add (GTK_CONTAINER (widget), frame);
        page->routes_list = list = gtk_list_box_new ();
        gtk_list_box_set_selection_mode (GTK_LIST_BOX (list), GTK_SELECTION_NONE);
        gtk_list_box_set_header_func (GTK_LIST_BOX (list), cc_list_box_update_header_func, NULL, NULL);
        gtk_list_box_set_sort_func (GTK_LIST_BOX (list), (GtkListBoxSortFunc)sort_first_last, NULL, NULL);
        gtk_container_add (GTK_CONTAINER (frame), list);
        page->auto_routes = GTK_SWITCH (gtk_builder_get_object (CE_PAGE (page)->builder, "auto_routes_switch"));
        gtk_switch_set_active (page->auto_routes, !nm_setting_ip6_config_get_ignore_auto_routes (page->setting));
        g_signal_connect (page->auto_routes, "notify::active", G_CALLBACK (switch_toggled), page);

        add_section_toolbar (page, widget, G_CALLBACK (add_empty_route_row));

        for (i = 0; i < nm_setting_ip6_config_get_num_routes (page->setting); i++) {
                NMIP6Route *route;
                const struct in6_addr *tmp_addr;
                gchar address[INET6_ADDRSTRLEN + 1];
                gchar gateway[INET6_ADDRSTRLEN + 1];
                gint prefix, metric;

                route = nm_setting_ip6_config_get_route (page->setting, i);
                if (!route)
                        continue;

                tmp_addr = nm_ip6_route_get_dest (route);
                (void) inet_ntop (AF_INET6, tmp_addr, &address[0], sizeof (address));
                prefix = nm_ip6_route_get_prefix (route);
                tmp_addr = nm_ip6_route_get_next_hop (route);
                (void) inet_ntop (AF_INET6, tmp_addr, &gateway[0], sizeof (gateway));
                metric = nm_ip6_route_get_metric (route);
                add_route_row (page, address, prefix, gateway, metric);
        }
        if (nm_setting_ip6_config_get_num_routes (page->setting) == 0)
                add_empty_route_row (page);

        gtk_widget_show_all (widget);
}

static void
connect_ip6_page (CEPageIP6 *page)
{
        GtkWidget *content;
        const gchar *str_method;
        gboolean disabled;
        GtkListStore *store;
        GtkTreeIter iter;
        guint method;

        add_address_section (page);
        add_dns_section (page);
        add_routes_section (page);

        page->enabled = GTK_SWITCH (gtk_builder_get_object (CE_PAGE (page)->builder, "switch_enable"));
        g_signal_connect (page->enabled, "notify::active", G_CALLBACK (switch_toggled), page);

        str_method = nm_setting_ip6_config_get_method (page->setting);
        disabled = g_strcmp0 (str_method, NM_SETTING_IP6_CONFIG_METHOD_IGNORE) == 0;
        gtk_switch_set_active (page->enabled, !disabled);
        g_signal_connect_swapped (page->enabled, "notify::active", G_CALLBACK (ce_page_changed), page);
        content = GTK_WIDGET (gtk_builder_get_object (CE_PAGE (page)->builder, "page_content"));
        g_object_bind_property (page->enabled, "active",
                                content, "sensitive",
                                G_BINDING_SYNC_CREATE);

        page->method = GTK_COMBO_BOX (gtk_builder_get_object (CE_PAGE (page)->builder, "combo_addresses"));

        store = gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_UINT);
        gtk_list_store_insert_with_values (store, &iter, -1,
                                           METHOD_COL_NAME, _("Automatic"),
                                           METHOD_COL_METHOD, IP6_METHOD_AUTO,
                                           -1);
        gtk_list_store_insert_with_values (store, &iter, -1,
                                           METHOD_COL_NAME, _("Automatic, DHCP only"),
                                           METHOD_COL_METHOD, IP6_METHOD_DHCP,
                                           -1);
        gtk_list_store_insert_with_values (store, &iter, -1,
                                           METHOD_COL_NAME, _("Manual"),
                                           METHOD_COL_METHOD, IP6_METHOD_MANUAL,
                                           -1);
        gtk_list_store_insert_with_values (store, &iter, -1,
                                           METHOD_COL_NAME, _("Link-Local Only"),
                                           METHOD_COL_METHOD, IP6_METHOD_LINK_LOCAL,
                                           -1);

        gtk_combo_box_set_model (page->method, GTK_TREE_MODEL (store));

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
                                      nm_setting_ip6_config_get_never_default (page->setting));

        g_signal_connect (page->method, "changed", G_CALLBACK (method_changed), page);
        if (method != IP6_METHOD_SHARED && method != IP6_METHOD_IGNORE)
                gtk_combo_box_set_active (page->method, method);
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

        if (!gtk_switch_get_active (page->enabled)) {
                method = NM_SETTING_IP6_CONFIG_METHOD_IGNORE;
        } else {
                switch (gtk_combo_box_get_active (page->method)) {
                case IP6_METHOD_MANUAL:
                        method = NM_SETTING_IP6_CONFIG_METHOD_MANUAL;
                        break;
                case IP6_METHOD_LINK_LOCAL:
                        method = NM_SETTING_IP6_CONFIG_METHOD_LINK_LOCAL;
                        break;
                case IP6_METHOD_DHCP:
                        method = NM_SETTING_IP6_CONFIG_METHOD_DHCP;
                        break;
                default:
                case IP6_METHOD_AUTO:
                        method = NM_SETTING_IP6_CONFIG_METHOD_AUTO;
                        break;
                }
        }

        nm_setting_ip6_config_clear_addresses (page->setting);
        children = gtk_container_get_children (GTK_CONTAINER (page->address_list));
        for (l = children; l; l = l->next) {
                GtkWidget *row = l->data;
                GtkEntry *entry;
                const gchar *text_address;
                const gchar *text_prefix;
                const gchar *text_gateway;
                struct in6_addr tmp_addr;
                struct in6_addr tmp_gateway;
                guint32 prefix;
                gchar *end;
                NMIP6Address *addr;
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

                if (inet_pton (AF_INET6, text_address, &tmp_addr) <= 0) {
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

                if (text_gateway && *text_gateway) {
                        if (inet_pton (AF_INET6, text_gateway, &tmp_gateway) <= 0) {
                                widget_set_error (g_object_get_data (G_OBJECT (row), "gateway"));
                                ret = FALSE;
                        } else {
                                if (!IN6_IS_ADDR_UNSPECIFIED (&tmp_gateway))
                                        have_gateway = TRUE;
                                widget_unset_error (g_object_get_data (G_OBJECT (row), "gateway"));
                        }
                } else {
                        widget_unset_error (g_object_get_data (G_OBJECT (row), "gateway"));
                }

                if (!ret)
                        continue;

                addr = nm_ip6_address_new ();
                nm_ip6_address_set_address (addr, &tmp_addr);
                nm_ip6_address_set_prefix (addr, prefix);
                if (have_gateway)
                        nm_ip6_address_set_gateway (addr, &tmp_gateway);
                nm_setting_ip6_config_add_address (page->setting, addr);
        }
        g_list_free (children);

        nm_setting_ip6_config_clear_dns (page->setting);
        children = gtk_container_get_children (GTK_CONTAINER (page->dns_list));
        for (l = children; l; l = l->next) {
                GtkWidget *row = l->data;
                GtkEntry *entry;
                const gchar *text;
                struct in6_addr tmp_addr;

                entry = GTK_ENTRY (g_object_get_data (G_OBJECT (row), "address"));
                if (!entry)
                        continue;

                text = gtk_entry_get_text (entry);
                if (!*text) {
                        /* ignore empty rows */
                        widget_unset_error (GTK_WIDGET (entry));
                        continue;
                }

                if (inet_pton (AF_INET6, text, &tmp_addr) <= 0) {
                        widget_set_error (GTK_WIDGET (entry));
                        ret = FALSE;
                } else {
                        widget_unset_error (GTK_WIDGET (entry));
                        nm_setting_ip6_config_add_dns (page->setting, &tmp_addr);
                }
        }
        g_list_free (children);

        nm_setting_ip6_config_clear_routes (page->setting);
        children = gtk_container_get_children (GTK_CONTAINER (page->routes_list));
        for (l = children; l; l = l->next) {
                GtkWidget *row = l->data;
                GtkEntry *entry;
                const gchar *text_address;
                const gchar *text_prefix;
                const gchar *text_gateway;
                const gchar *text_metric;
                struct in6_addr dest, gateway;
                guint32 prefix, metric;
                gchar *end;
                NMIP6Route *route;

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

                if (inet_pton (AF_INET6, text_address, &dest) <= 0) {
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

                if (inet_pton (AF_INET6, text_gateway, &gateway) <= 0) {
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

                route = nm_ip6_route_new ();
                nm_ip6_route_set_dest (route, &dest);
                nm_ip6_route_set_prefix (route, prefix);
                nm_ip6_route_set_next_hop (route, &gateway);
                nm_ip6_route_set_metric (route, metric);
                nm_setting_ip6_config_add_route (page->setting, route);
                nm_ip6_route_unref (route);
        }
        g_list_free (children);

        if (!ret)
                goto out;

        ignore_auto_dns = !gtk_switch_get_active (page->auto_dns);
        ignore_auto_routes = !gtk_switch_get_active (page->auto_routes);
        never_default = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (page->never_default));

        g_object_set (page->setting,
                      NM_SETTING_IP6_CONFIG_METHOD, method,
                      NM_SETTING_IP6_CONFIG_IGNORE_AUTO_DNS, ignore_auto_dns,
                      NM_SETTING_IP6_CONFIG_IGNORE_AUTO_ROUTES, ignore_auto_routes,
                      NM_SETTING_IP6_CONFIG_NEVER_DEFAULT, never_default,
                      NULL);

out:

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
                   NMClient         *client,
                   NMRemoteSettings *settings)
{
        CEPageIP6 *page;

        page = CE_PAGE_IP6 (ce_page_new (CE_TYPE_PAGE_IP6,
                                           connection,
                                           client,
                                           settings,
                                           "/org/gnome/control-center/network/ip6-page.ui",
                                           _("IPv6")));

        page->setting = nm_connection_get_setting_ip6_config (connection);
        if (!page->setting) {
                page->setting = NM_SETTING_IP6_CONFIG (nm_setting_ip6_config_new ());
                nm_connection_add_setting (connection, NM_SETTING (page->setting));
        }

        connect_ip6_page (page);

        return CE_PAGE (page);
}
