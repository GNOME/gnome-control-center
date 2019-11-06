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
                const char *text;
                gfloat xalign;
                g_autofree gchar *newtext = NULL;
                int len;

                xalign = gtk_label_get_xalign (GTK_LABEL (widget));
                if (xalign != 0.0)
                        return;
                text = gtk_label_get_text (GTK_LABEL (widget));
                len = strlen (text);
                if (len < 2 || text[len - 1] != ':')
                        return;

                newtext = g_strndup (text, len - 1);
                gtk_label_set_text (GTK_LABEL (widget), newtext);
                gtk_label_set_xalign (GTK_LABEL (widget), 1.0);
        }
}

static void
load_vpn_plugin (CEPageVpn *self, NMConnection *connection)
{
        GtkWidget *ui_widget;

        self->editor = nm_vpn_editor_plugin_get_editor (self->plugin,
                                                        connection,
                                                        NULL);
        ui_widget = NULL;
        if (self->editor)
                ui_widget = GTK_WIDGET (nm_vpn_editor_get_widget (self->editor));

	if (!ui_widget) {
		g_clear_object (&self->editor);
                self->plugin = NULL;
		return;
	}
        vpn_gnome3ify_editor (ui_widget);

        gtk_widget_destroy (GTK_WIDGET (self->failure_label));

        gtk_box_pack_start (self->page, ui_widget, TRUE, TRUE, 0);
	gtk_widget_show_all (ui_widget);

        g_signal_connect_swapped (self->editor, "changed", G_CALLBACK (ce_page_changed), self);
}

static void
connect_vpn_page (CEPageVpn *self)
{
        const gchar *name;

        name = nm_setting_connection_get_id (self->setting_connection);
        gtk_entry_set_text (self->name_entry, name);
        g_signal_connect_swapped (self->name_entry, "changed", G_CALLBACK (ce_page_changed), self);
}

static void
ce_page_vpn_dispose (GObject *object)
{
        CEPageVpn *self = CE_PAGE_VPN (object);

        g_clear_object (&self->editor);

        G_OBJECT_CLASS (ce_page_vpn_parent_class)->dispose (object);
}

static const gchar *
ce_page_vpn_get_title (CEPage *page)
{
        return _("Identity");
}

static gboolean
ce_page_vpn_validate (CEPage        *page,
                      NMConnection  *connection,
                      GError       **error)
{
        CEPageVpn *self = CE_PAGE_VPN (page);

        g_object_set (self->setting_connection,
                      NM_SETTING_CONNECTION_ID, gtk_entry_get_text (self->name_entry),
                      NULL);

        if (!nm_setting_verify (NM_SETTING (self->setting_connection), NULL, error))
                return FALSE;

        if (!self->editor)
                return TRUE;

	return nm_vpn_editor_update_connection (self->editor, connection, error);
}

static void
ce_page_vpn_init (CEPageVpn *self)
{
}

static void
ce_page_vpn_class_init (CEPageVpnClass *class)
{
        CEPageClass *page_class = CE_PAGE_CLASS (class);
        GObjectClass *object_class = G_OBJECT_CLASS (class);

        object_class->dispose = ce_page_vpn_dispose;
        page_class->get_title = ce_page_vpn_get_title;
        page_class->validate = ce_page_vpn_validate;
}

static void
finish_setup (CEPageVpn *self, gpointer unused, GError *error, gpointer user_data)
{
        NMConnection *connection = CE_PAGE (self)->connection;
        const char *vpn_type;

        self->setting_connection = nm_connection_get_setting_connection (connection);
        self->setting_vpn = nm_connection_get_setting_vpn (connection);
        vpn_type = nm_setting_vpn_get_service_type (self->setting_vpn);

        self->plugin = vpn_get_plugin_by_service (vpn_type);
        if (self->plugin)
                load_vpn_plugin (self, connection);

        connect_vpn_page (self);
}

CEPage *
ce_page_vpn_new (NMConnection     *connection,
		 NMClient         *client)
{
        CEPageVpn *self;

        self = CE_PAGE_VPN (ce_page_new (CE_TYPE_PAGE_VPN,
					 connection,
					 client,
					 "/org/gnome/control-center/network/vpn-page.ui"));

        self->failure_label = GTK_LABEL (gtk_builder_get_object (CE_PAGE (self)->builder, "failure_label"));
        self->name_entry = GTK_ENTRY (gtk_builder_get_object (CE_PAGE (self)->builder, "name_entry"));
        self->page = GTK_BOX (gtk_builder_get_object (CE_PAGE (self)->builder, "page"));

        g_signal_connect (self, "initialized", G_CALLBACK (finish_setup), NULL);

        CE_PAGE (self)->security_setting = NM_SETTING_VPN_SETTING_NAME;

        return CE_PAGE (self);
}
