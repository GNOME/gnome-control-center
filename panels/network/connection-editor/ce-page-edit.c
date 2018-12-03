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
 * GNU General Public License for more edit.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "config.h"

#include <glib-object.h>
#include <glib/gi18n.h>

#include <NetworkManager.h>

#include "../panel-common.h"
#include "ce-page-edit.h"
#include "list-box-helper.h"

G_DEFINE_TYPE (CEPageEdit, ce_page_edit, CE_TYPE_PAGE)

static void
forget_cb (GtkButton *button, CEPageEdit *page)
{
        net_connection_editor_forget (page->editor);
}

static void
all_user_changed (GtkSwitch *s, GParamSpec *pspec, CEPageEdit *page)
{
        gboolean all_users;
        NMSettingConnection *sc;

        sc = nm_connection_get_setting_connection (CE_PAGE (page)->connection);
        all_users = gtk_switch_get_active (s);

        g_object_set (sc, "permissions", NULL, NULL);
        if (!all_users)
                nm_setting_connection_add_permission (sc, "user", g_get_user_name (), NULL);
}

static void
restrict_data_changed (GtkSwitch *s, GParamSpec *pspec, CEPageEdit *page)
{
        NMSettingConnection *s_con;
        NMMetered metered;

        s_con = nm_connection_get_setting_connection (CE_PAGE (page)->connection);

        if (gtk_switch_get_active (s))
                metered = NM_METERED_YES;
        else
                metered = NM_METERED_NO;

        g_object_set (s_con, "metered", metered, NULL);
}

static void
update_restrict_data (CEPageEdit *page)
{
        NMSettingConnection *s_con;
        NMMetered metered;
        GtkWidget *widget;
        const gchar *type;

        s_con = nm_connection_get_setting_connection (CE_PAGE (page)->connection);

        if (s_con == NULL)
                return;

        /* Disable for VPN; NetworkManager does not implement that yet (see
         * bug https://bugzilla.gnome.org/show_bug.cgi?id=792618) */
        type = nm_setting_connection_get_connection_type (s_con);
        if (g_str_equal (type, NM_SETTING_VPN_SETTING_NAME))
                return;

        metered = nm_setting_connection_get_metered (s_con);

        widget = GTK_WIDGET (gtk_builder_get_object (CE_PAGE (page)->builder, "restrict_data_switch"));
        gtk_switch_set_active (GTK_SWITCH (widget),
                               metered == NM_METERED_YES || metered == NM_METERED_GUESS_YES);
        gtk_widget_show (widget);

        g_signal_connect (widget, "notify::active", G_CALLBACK (restrict_data_changed), page);
        g_signal_connect_swapped (widget, "notify::active", G_CALLBACK (ce_page_changed), page);
}

static void
connect_edit_page (CEPageEdit *page)
{
        NMSettingConnection *sc;
        GtkWidget *widget;
        const gchar *type;

        sc = nm_connection_get_setting_connection (CE_PAGE (page)->connection);
        type = nm_setting_connection_get_connection_type (sc);

        widget = GTK_WIDGET (gtk_builder_get_object (CE_PAGE (page)->builder,
                                                     "list_box"));
        gtk_list_box_set_header_func (GTK_LIST_BOX (widget), cc_list_box_update_header_func, NULL, NULL);

        /* Auto connect check */
        widget = GTK_WIDGET (gtk_builder_get_object (CE_PAGE (page)->builder,
                                                     "auto_connect_switch"));
        if (g_str_equal (type, NM_SETTING_VPN_SETTING_NAME)) {
                gtk_widget_hide (widget);
        } else {
                g_object_bind_property (sc, "autoconnect",
                                        widget, "active",
                                        G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE);
                g_signal_connect_swapped (widget, "notify::active", G_CALLBACK (ce_page_changed), page);
        }

        /* All users check */
        widget = GTK_WIDGET (gtk_builder_get_object (CE_PAGE (page)->builder,
                                                     "all_user_switch"));
        gtk_switch_set_active (GTK_SWITCH (widget),
                               nm_setting_connection_get_num_permissions (sc) == 0);
        g_signal_connect (widget, "notify::active",
                          G_CALLBACK (all_user_changed), page);
        g_signal_connect_swapped (widget, "notify::active", G_CALLBACK (ce_page_changed), page);

        /* Restrict Data check */
        update_restrict_data (page);

        /* Forget button */
        widget = GTK_WIDGET (gtk_builder_get_object (CE_PAGE (page)->builder, "button_forget"));
        g_signal_connect (widget, "clicked", G_CALLBACK (forget_cb), page);

        if (g_str_equal (type, NM_SETTING_WIRELESS_SETTING_NAME))
                gtk_button_set_label (GTK_BUTTON (widget), _("Forget Connection"));
        else if (g_str_equal (type, NM_SETTING_WIRED_SETTING_NAME))
                gtk_button_set_label (GTK_BUTTON (widget), _("Remove Connection Profile"));
        else if (g_str_equal (type, NM_SETTING_VPN_SETTING_NAME))
                gtk_button_set_label (GTK_BUTTON (widget), _("Remove VPN"));
        else
                gtk_widget_hide (widget);
}

static void
ce_page_edit_init (CEPageEdit *page)
{
}

static void
ce_page_edit_class_init (CEPageEditClass *class)
{
}

CEPage *
ce_page_edit_new (NMConnection        *connection,
                  NMClient            *client,
                  NetConnectionEditor *editor)
{
        CEPageEdit *page;

        page = CE_PAGE_EDIT (ce_page_new (CE_TYPE_PAGE_EDIT,
                                          connection,
                                          client,
                                          "/org/gnome/control-center/network/edit-page.ui",
                                          _("Edit")));

        page->editor = editor;

        connect_edit_page (page);

        return CE_PAGE (page);
}
