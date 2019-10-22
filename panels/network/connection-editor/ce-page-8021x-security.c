/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* NetworkManager Connection editor -- Connection editor for NetworkManager
 *
 * Dan Williams <dcbw@redhat.com>
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
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * (C) Copyright 2008 - 2012 Red Hat, Inc.
 */

#include "config.h"

#include <string.h>

#include <gtk/gtk.h>
#include <glib/gi18n.h>

#include <NetworkManager.h>

#include "ws-wpa-eap.h"
#include "wireless-security.h"
#include "ce-page-ethernet.h"
#include "ce-page-8021x-security.h"

G_DEFINE_TYPE (CEPage8021xSecurity, ce_page_8021x_security, CE_TYPE_PAGE)

static void
enable_toggled (CEPage8021xSecurity *self)
{
	gtk_widget_set_sensitive (self->security_widget, gtk_switch_get_active (self->enable_8021x_switch));
	ce_page_changed (CE_PAGE (self));
}

static void
stuff_changed (WirelessSecurity *sec, gpointer user_data)
{
        ce_page_changed (CE_PAGE (user_data));
}

static void
finish_setup (CEPage8021xSecurity *self, gpointer unused, GError *error, gpointer user_data)
{
	GtkWidget *parent;

	if (error)
		return;

        self->group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);

	self->security = (WirelessSecurity *) ws_wpa_eap_new (CE_PAGE (self)->connection, TRUE, FALSE);
	if (!self->security) {
		g_warning ("Could not load 802.1x user interface.");
		return;
	}

	wireless_security_set_changed_notify (self->security, stuff_changed, self);
	self->security_widget = wireless_security_get_widget (self->security);
	parent = gtk_widget_get_parent (self->security_widget);
	if (parent)
		gtk_container_remove (GTK_CONTAINER (parent), self->security_widget);

	gtk_switch_set_active (self->enable_8021x_switch, self->initial_have_8021x);
	g_signal_connect_swapped (self->enable_8021x_switch, "notify::active", G_CALLBACK (enable_toggled), self);
	gtk_widget_set_sensitive (self->security_widget, self->initial_have_8021x);

        gtk_size_group_add_widget (self->group, GTK_WIDGET (self->security_label));
        wireless_security_add_to_size_group (self->security, self->group);

	gtk_container_add (GTK_CONTAINER (self->box), self->security_widget);

}

CEPage *
ce_page_8021x_security_new (NMConnection     *connection,
                            NMClient         *client)
{
	CEPage8021xSecurity *self;

	self = CE_PAGE_8021X_SECURITY (ce_page_new (CE_TYPE_PAGE_8021X_SECURITY,
	                                            connection,
	                                            client,
	                                            "/org/gnome/control-center/network/8021x-security-page.ui",
	                                            _("Security")));

        self->box = GTK_BOX (gtk_builder_get_object (CE_PAGE (self)->builder, "box"));
	self->enable_8021x_switch = GTK_SWITCH (gtk_builder_get_object (CE_PAGE (self)->builder, "enable_8021x_switch"));
        self->security_label = GTK_LABEL (gtk_builder_get_object (CE_PAGE (self)->builder, "security_label"));

	if (nm_connection_get_setting_802_1x (connection))
		self->initial_have_8021x = TRUE;

	g_signal_connect (self, "initialized", G_CALLBACK (finish_setup), NULL);

	if (self->initial_have_8021x)
                CE_PAGE (self)->security_setting = NM_SETTING_802_1X_SETTING_NAME;

	return CE_PAGE (self);
}

static gboolean
validate (CEPage *cepage, NMConnection *connection, GError **error)
{
	CEPage8021xSecurity *self = CE_PAGE_8021X_SECURITY (cepage);
	gboolean valid = TRUE;

	if (gtk_switch_get_active (self->enable_8021x_switch)) {
		NMSetting *s_8021x;

		/* FIXME: get failed property and error out of wireless security objects */
		valid = wireless_security_validate (self->security, error);
		if (valid) {
			g_autoptr(NMConnection) tmp_connection = NULL;
			NMSetting *s_con;

			/* Here's a nice hack to work around the fact that ws_802_1x_fill_connection needs wireless setting. */
			tmp_connection = nm_simple_connection_new ();
			nm_connection_add_setting (tmp_connection, nm_setting_wireless_new ());

			/* temp connection needs a 'connection' setting too, since most of
			 * the EAP methods need the UUID for CA cert ignore stuff.
			 */
			s_con = nm_connection_get_setting (connection, NM_TYPE_SETTING_CONNECTION);
			nm_connection_add_setting (tmp_connection, nm_setting_duplicate (s_con));

			ws_802_1x_fill_connection (GTK_COMBO_BOX (gtk_builder_get_object (self->security->builder, "auth_combo")), tmp_connection);

			s_8021x = nm_connection_get_setting (tmp_connection, NM_TYPE_SETTING_802_1X);
			nm_connection_add_setting (connection, NM_SETTING (g_object_ref (s_8021x)));
		}
	} else {
		nm_connection_remove_setting (connection, NM_TYPE_SETTING_802_1X);
		valid = TRUE;
	}

	return valid;
}

static void
ce_page_8021x_security_init (CEPage8021xSecurity *self)
{
}

static void
dispose (GObject *object)
{
	CEPage8021xSecurity *self = CE_PAGE_8021X_SECURITY (object);

	if (self->security) {
		wireless_security_unref (self->security);
                self->security = NULL;
        }

        g_clear_object (&self->group);

	G_OBJECT_CLASS (ce_page_8021x_security_parent_class)->dispose (object);
}

static void
ce_page_8021x_security_class_init (CEPage8021xSecurityClass *security_class)
{
	GObjectClass *object_class = G_OBJECT_CLASS (security_class);
	CEPageClass *parent_class = CE_PAGE_CLASS (security_class);

	/* virtual methods */
	object_class->dispose = dispose;

	parent_class->validate = validate;
}
