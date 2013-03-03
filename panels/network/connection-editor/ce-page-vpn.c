/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2013 Red Hat, Inc
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

#include <nm-utils.h>

#include "ce-page-vpn.h"
#include "vpn-helpers.h"
#include "firewall-helpers.h"

G_DEFINE_TYPE (CEPageVpn, ce_page_vpn, CE_TYPE_PAGE)

static void
all_user_changed (GtkToggleButton *b, CEPageVpn *page)
{
        gboolean all_users;
        NMSettingConnection *sc;

        sc = nm_connection_get_setting_connection (CE_PAGE (page)->connection);
        all_users = gtk_toggle_button_get_active (b);

        g_object_set (sc, "permissions", NULL, NULL);
        if (!all_users)
                nm_setting_connection_add_permission (sc, "user", g_get_user_name (), NULL);
}

/* Hack to make the plugin-provided editor widget fit in better with
 * the control center by changing
 *
 *     Foo:     [__________]
 *     Bar baz: [__________]
 *
 * to
 *
 *          Foo [__________]
 *      Bar baz [__________]
 */
static void
vpn_gnome3ify_editor (GtkWidget *widget)
{
        if (GTK_IS_CONTAINER (widget)) {
                GList *children, *iter;

                children = gtk_container_get_children (GTK_CONTAINER (widget));
                for (iter = children; iter; iter = iter->next)
                        vpn_gnome3ify_editor (iter->data);
                g_list_free (children);
        } else if (GTK_IS_LABEL (widget)) {
                const char *text;
                gfloat xalign, yalign;
                char *newtext;
                int len;

                gtk_misc_get_alignment (GTK_MISC (widget), &xalign, &yalign);
                if (xalign != 0.0)
                        return;
                text = gtk_label_get_text (GTK_LABEL (widget));
                len = strlen (text);
                if (len < 2 || text[len - 1] != ':')
                        return;

                newtext = g_strndup (text, len - 1);
                gtk_label_set_text (GTK_LABEL (widget), newtext);
                g_free (newtext);
                gtk_misc_set_alignment (GTK_MISC (widget), 1.0, yalign);
        }
}

static void
load_vpn_plugin (CEPageVpn *page, NMConnection *connection)
{
	CEPage *parent = CE_PAGE (page);
        GtkWidget *ui_widget, *failure;

	page->ui = nm_vpn_plugin_ui_interface_ui_factory (page->plugin, connection, NULL);
	if (!page->ui) {
                page->plugin = NULL;
		return;
        }
	ui_widget = GTK_WIDGET (nm_vpn_plugin_ui_widget_interface_get_widget (page->ui));
	if (!ui_widget) {
		g_clear_object (&page->ui);
                page->plugin = NULL;
		return;
	}
        vpn_gnome3ify_editor (ui_widget);

        failure = GTK_WIDGET (gtk_builder_get_object (parent->builder, "failure_label"));
        gtk_widget_destroy (failure);

        gtk_box_pack_start (page->box, ui_widget, TRUE, TRUE, 0);
	gtk_widget_show_all (ui_widget);

        g_signal_connect_swapped (page->ui, "changed", G_CALLBACK (ce_page_changed), page);
}

static void
connect_vpn_page (CEPageVpn *page)
{
        const gchar *name;
        GtkWidget *widget;

        name = nm_setting_connection_get_id (page->setting_connection);
        gtk_entry_set_text (page->name, name);
        g_signal_connect_swapped (page->name, "changed", G_CALLBACK (ce_page_changed), page);

        widget = GTK_WIDGET (gtk_builder_get_object (CE_PAGE (page)->builder,
                                                     "all_user_check"));
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget),
                                      nm_setting_connection_get_num_permissions (page->setting_connection) == 0);
        g_signal_connect (widget, "toggled",
                          G_CALLBACK (all_user_changed), page);

        widget = GTK_WIDGET (gtk_builder_get_object (CE_PAGE (page)->builder, "combo_zone"));
        firewall_ui_setup (page->setting_connection, widget, CE_PAGE (page)->cancellable);
        g_signal_connect_swapped (widget, "changed", G_CALLBACK (ce_page_changed), page);

}

static gboolean
validate (CEPage        *page,
          NMConnection  *connection,
          GError       **error)
{
        CEPageVpn *self = CE_PAGE_VPN (page);
        GtkWidget *widget;

        g_object_set (self->setting_connection,
                      NM_SETTING_CONNECTION_ID, gtk_entry_get_text (self->name),
                      NULL);
        widget = GTK_WIDGET (gtk_builder_get_object (CE_PAGE (page)->builder, "combo_zone"));
        firewall_ui_to_setting (self->setting_connection, widget);

        if (!nm_setting_verify (NM_SETTING (self->setting_connection), NULL, error))
                return FALSE;

        if (!self->ui)
                return TRUE;

	return nm_vpn_plugin_ui_widget_interface_update_connection (self->ui, connection, error);
}

static void
ce_page_vpn_init (CEPageVpn *page)
{
}

static void
dispose (GObject *object)
{
        CEPageVpn *page = CE_PAGE_VPN (object);

        g_clear_object (&page->ui);

        G_OBJECT_CLASS (ce_page_vpn_parent_class)->dispose (object);
}

static void
ce_page_vpn_class_init (CEPageVpnClass *class)
{
        CEPageClass *page_class = CE_PAGE_CLASS (class);
        GObjectClass *object_class = G_OBJECT_CLASS (class);

        object_class->dispose = dispose;

        page_class->validate = validate;
}

CEPage *
ce_page_vpn_new (NMConnection     *connection,
		 NMClient         *client,
		 NMRemoteSettings *settings)
{
        CEPageVpn *page;
	const char *vpn_type;

        page = CE_PAGE_VPN (ce_page_new (CE_TYPE_PAGE_VPN,
					 connection,
					 client,
					 settings,
					 "/org/gnome/control-center/network/vpn-page.ui",
					 _("Identity")));

        page->name = GTK_ENTRY (gtk_builder_get_object (CE_PAGE (page)->builder, "entry_name"));
        page->box = GTK_BOX (gtk_builder_get_object (CE_PAGE (page)->builder, "page"));

	page->setting_connection = nm_connection_get_setting_connection (connection);
	page->setting_vpn = nm_connection_get_setting_vpn (connection);
	vpn_type = nm_setting_vpn_get_service_type (page->setting_vpn);

	page->plugin = vpn_get_plugin_by_service (vpn_type);
	if (page->plugin)
                load_vpn_plugin (page, connection);

        connect_vpn_page (page);

        return CE_PAGE (page);
}
