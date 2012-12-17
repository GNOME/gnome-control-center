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
#include <nm-setting-connection.h>
#include <nm-setting-wired.h>
#include <nm-setting-8021x.h>
#include <nm-setting-wireless.h>
#include <nm-utils.h>

#include "wireless-security.h"
#include "ce-page-ethernet.h"
#include "ce-page-8021x-security.h"

G_DEFINE_TYPE (CEPage8021xSecurity, ce_page_8021x_security, CE_TYPE_PAGE)

static void
enable_toggled (GtkToggleButton *button, gpointer user_data)
{
	CEPage8021xSecurity *page = CE_PAGE_8021X_SECURITY (user_data);

	gtk_widget_set_sensitive (page->security_widget, gtk_toggle_button_get_active (page->enabled));
	ce_page_changed (CE_PAGE (page));
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

	gtk_toggle_button_set_active (self->enabled, self->initial_have_8021x);
	g_signal_connect (self->enabled, "toggled", G_CALLBACK (enable_toggled), self);
	gtk_widget_set_sensitive (self->security_widget, self->initial_have_8021x);

	gtk_box_pack_start (GTK_BOX (CE_PAGE (self)->page), GTK_WIDGET (self->enabled), FALSE, TRUE, 12);
	gtk_box_pack_start (GTK_BOX (CE_PAGE (self)->page), self->security_widget, TRUE, TRUE, 0);
	gtk_widget_show_all (CE_PAGE (self)->page);
}

CEPage *
ce_page_8021x_security_new (NMConnection     *connection,
                            NMClient         *client,
                            NMRemoteSettings *settings)
{
	CEPage8021xSecurity *self;

	self = CE_PAGE_8021X_SECURITY (ce_page_new (CE_TYPE_PAGE_8021X_SECURITY,
	                                            connection,
	                                            client,
	                                            settings,
	                                            NULL,
	                                            _("Security")));

	CE_PAGE (self)->page = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);

	g_object_ref_sink (G_OBJECT (CE_PAGE (self)->page));
	gtk_container_set_border_width (GTK_CONTAINER (CE_PAGE (self)->page), 6);

	if (nm_connection_get_setting_802_1x (connection))
		self->initial_have_8021x = TRUE;

	self->enabled = GTK_TOGGLE_BUTTON (gtk_check_button_new_with_mnemonic (_("Use 802.1_X security for this connection")));

	g_signal_connect (self, "initialized", G_CALLBACK (finish_setup), NULL);

	if (self->initial_have_8021x)
                CE_PAGE (self)->security_setting = NM_SETTING_802_1X_SETTING_NAME;

	return CE_PAGE (self);
}

static gboolean
validate (CEPage *page, NMConnection *connection, GError **error)
{
	CEPage8021xSecurity *self = CE_PAGE_8021X_SECURITY (page);
	gboolean valid = TRUE;

	if (gtk_toggle_button_get_active (self->enabled)) {
		NMConnection *tmp_connection;
		NMSetting *s_8021x;

		/* FIXME: get failed property and error out of wireless security objects */
		valid = wireless_security_validate (self->security, NULL);
		if (valid) {
			NMSetting *s_con;

			/* Here's a nice hack to work around the fact that ws_802_1x_fill_connection needs wireless setting. */
			tmp_connection = nm_connection_new ();
			nm_connection_add_setting (tmp_connection, nm_setting_wireless_new ());

			/* temp connection needs a 'connection' setting too, since most of
			 * the EAP methods need the UUID for CA cert ignore stuff.
			 */
			s_con = nm_connection_get_setting (connection, NM_TYPE_SETTING_CONNECTION);
			nm_connection_add_setting (tmp_connection, nm_setting_duplicate (s_con));

			ws_802_1x_fill_connection (self->security, "wpa_eap_auth_combo", tmp_connection);

			s_8021x = nm_connection_get_setting (tmp_connection, NM_TYPE_SETTING_802_1X);
			nm_connection_add_setting (connection, NM_SETTING (g_object_ref (s_8021x)));

			g_object_unref (tmp_connection);
		} else
			g_set_error (error, NM_CONNECTION_ERROR, NM_CONNECTION_ERROR_UNKNOWN, "Invalid 802.1x security");
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
