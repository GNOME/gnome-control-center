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

#include <NetworkManager.h>

#include "ce-page-vpn.h"
#include "vpn-helpers.h"

G_DEFINE_TYPE (CEPageVpn, ce_page_vpn, CE_TYPE_PAGE)

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
                GtkStyleContext *style;
                const char *text;
                gfloat xalign;
                char *newtext;
                int len;

                style = gtk_widget_get_style_context (widget);
                xalign = gtk_label_get_xalign (GTK_LABEL (widget));
                /* If the label is aligned at the end, it's likely a field name. */
                if (xalign == 1.0)
                        gtk_style_context_add_class (style, "dim-label");
                if (xalign != 0.0)
                        return;
                text = gtk_label_get_text (GTK_LABEL (widget));
                len = strlen (text);
                if (len < 2 || text[len - 1] != ':')
                        return;

                gtk_style_context_add_class (style, "dim-label");
                newtext = g_strndup (text, len - 1);
                gtk_label_set_text (GTK_LABEL (widget), newtext);
                g_free (newtext);
                gtk_label_set_xalign (GTK_LABEL (widget), 1.0);
        }
}

static void
load_vpn_plugin (CEPageVpn *page, NMConnection *connection)
{
	CEPage *parent = CE_PAGE (page);
        GtkWidget *ui_widget, *failure;

        page->editor = nm_vpn_editor_plugin_get_editor (page->plugin,
                                                        connection,
                                                        NULL);
        ui_widget = NULL;
        if (page->editor)
                ui_widget = GTK_WIDGET (nm_vpn_editor_get_widget (page->editor));

	if (!ui_widget) {
		g_clear_object (&page->editor);
                page->plugin = NULL;
		return;
	}
        vpn_gnome3ify_editor (ui_widget);

        failure = GTK_WIDGET (gtk_builder_get_object (parent->builder, "failure_label"));
        gtk_widget_destroy (failure);

        gtk_box_pack_start (page->box, ui_widget, TRUE, TRUE, 0);
	gtk_widget_show_all (ui_widget);

        g_signal_connect_swapped (page->editor, "changed", G_CALLBACK (ce_page_changed), page);
}

static void
connect_vpn_page (CEPageVpn *page)
{
        const gchar *name;

        name = nm_setting_connection_get_id (page->setting_connection);
        gtk_entry_set_text (page->name, name);
        g_signal_connect_swapped (page->name, "changed", G_CALLBACK (ce_page_changed), page);
}

static gboolean
validate (CEPage        *page,
          NMConnection  *connection,
          GError       **error)
{
        CEPageVpn *self = CE_PAGE_VPN (page);

        g_object_set (self->setting_connection,
                      NM_SETTING_CONNECTION_ID, gtk_entry_get_text (self->name),
                      NULL);

        if (!nm_setting_verify (NM_SETTING (self->setting_connection), NULL, error))
                return FALSE;

        if (!self->editor)
                return TRUE;

	return nm_vpn_editor_update_connection (self->editor, connection, error);
}

static void
ce_page_vpn_init (CEPageVpn *page)
{
}

static void
dispose (GObject *object)
{
        CEPageVpn *page = CE_PAGE_VPN (object);

        g_clear_object (&page->editor);

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

static void
finish_setup (CEPageVpn *page, gpointer unused, GError *error, gpointer user_data)
{
        NMConnection *connection = CE_PAGE (page)->connection;
        const char *vpn_type;

        page->setting_connection = nm_connection_get_setting_connection (connection);
        page->setting_vpn = nm_connection_get_setting_vpn (connection);
        vpn_type = nm_setting_vpn_get_service_type (page->setting_vpn);

        page->plugin = vpn_get_plugin_by_service (vpn_type);
        if (page->plugin)
                load_vpn_plugin (page, connection);

        connect_vpn_page (page);
}

CEPage *
ce_page_vpn_new (NMConnection     *connection,
		 NMClient         *client)
{
        CEPageVpn *page;

        page = CE_PAGE_VPN (ce_page_new (CE_TYPE_PAGE_VPN,
					 connection,
					 client,
					 "/org/gnome/control-center/network/vpn-page.ui",
					 _("Identity")));

        page->name = GTK_ENTRY (gtk_builder_get_object (CE_PAGE (page)->builder, "entry_name"));
        page->box = GTK_BOX (gtk_builder_get_object (CE_PAGE (page)->builder, "page"));

        g_signal_connect (page, "initialized", G_CALLBACK (finish_setup), NULL);

        CE_PAGE (page)->security_setting = NM_SETTING_VPN_SETTING_NAME;

        return CE_PAGE (page);
}
