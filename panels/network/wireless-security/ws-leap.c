/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* NetworkManager Applet -- allow user control over networking
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
 * Copyright 2007 - 2014 Red Hat, Inc.
 */

#include <glib/gi18n.h>

#include "helpers.h"
#include "nma-ui-utils.h"
#include "ui-helpers.h"
#include "wireless-security.h"
#include "ws-leap.h"

struct _WirelessSecurityLEAP {
	GtkGrid parent;

	GtkEntry       *password_entry;
	GtkLabel       *password_label;
	GtkCheckButton *show_password_check;
	GtkEntry       *username_entry;
	GtkLabel       *username_label;
};

static void wireless_security_iface_init (WirelessSecurityInterface *);

G_DEFINE_TYPE_WITH_CODE (WirelessSecurityLEAP, ws_leap, GTK_TYPE_GRID,
                         G_IMPLEMENT_INTERFACE (wireless_security_get_type (), wireless_security_iface_init));

static void
show_toggled_cb (WirelessSecurityLEAP *self)
{
	gboolean visible;

	visible = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (self->show_password_check));
	gtk_entry_set_visibility (self->password_entry, visible);
}

static gboolean
validate (WirelessSecurity *security, GError **error)
{
	WirelessSecurityLEAP *self = WS_LEAP (security);
	NMSettingSecretFlags secret_flags;
	const char *text;
	gboolean ret = TRUE;

	text = gtk_entry_get_text (self->username_entry);
	if (!text || !strlen (text)) {
		widget_set_error (GTK_WIDGET (self->username_entry));
		g_set_error_literal (error, NMA_ERROR, NMA_ERROR_GENERIC, _("missing leap-username"));
		ret = FALSE;
	} else
		widget_unset_error (GTK_WIDGET (self->username_entry));

	secret_flags = nma_utils_menu_to_secret_flags (GTK_WIDGET (self->password_entry));
	if (secret_flags & NM_SETTING_SECRET_FLAG_NOT_SAVED) {
		widget_unset_error (GTK_WIDGET (self->password_entry));
		return TRUE;
	}

	text = gtk_entry_get_text (self->password_entry);
	if (!text || !strlen (text)) {
		widget_set_error (GTK_WIDGET (self->password_entry));
		if (ret) {
			g_set_error_literal (error, NMA_ERROR, NMA_ERROR_GENERIC, _("missing leap-password"));
			ret = FALSE;
		}
	} else
		widget_unset_error (GTK_WIDGET (self->password_entry));

	return ret;
}

static void
add_to_size_group (WirelessSecurity *security, GtkSizeGroup *group)
{
	WirelessSecurityLEAP *self = WS_LEAP (security);
	gtk_size_group_add_widget (group, GTK_WIDGET (self->username_label));
	gtk_size_group_add_widget (group, GTK_WIDGET (self->password_label));
}

static void
fill_connection (WirelessSecurity *security, NMConnection *connection)
{
	WirelessSecurityLEAP *self = WS_LEAP (security);
	NMSettingWirelessSecurity *s_wireless_sec;
	NMSettingSecretFlags secret_flags;
	const char *leap_password = NULL, *leap_username = NULL;

	/* Blow away the old security setting by adding a clear one */
	s_wireless_sec = (NMSettingWirelessSecurity *) nm_setting_wireless_security_new ();
	nm_connection_add_setting (connection, (NMSetting *) s_wireless_sec);

	leap_username = gtk_entry_get_text (self->username_entry);
	leap_password = gtk_entry_get_text (self->password_entry);

	g_object_set (s_wireless_sec,
	              NM_SETTING_WIRELESS_SECURITY_KEY_MGMT, "ieee8021x",
	              NM_SETTING_WIRELESS_SECURITY_AUTH_ALG, "leap",
	              NM_SETTING_WIRELESS_SECURITY_LEAP_USERNAME, leap_username,
	              NM_SETTING_WIRELESS_SECURITY_LEAP_PASSWORD, leap_password,
	              NULL);

	/* Save LEAP_PASSWORD_FLAGS to the connection */
	secret_flags = nma_utils_menu_to_secret_flags (GTK_WIDGET (self->password_entry));
	nm_setting_set_secret_flags (NM_SETTING (s_wireless_sec), NM_SETTING_WIRELESS_SECURITY_LEAP_PASSWORD,
	                             secret_flags, NULL);

	/* Update secret flags and popup when editing the connection */
	nma_utils_update_password_storage (GTK_WIDGET (self->password_entry), secret_flags,
	                                   NM_SETTING (s_wireless_sec), NM_SETTING_WIRELESS_SECURITY_LEAP_PASSWORD);
}

static gboolean
adhoc_compatible (WirelessSecurity *security)
{
	return FALSE;
}

static void
changed_cb (WirelessSecurityLEAP *self)
{
	wireless_security_notify_changed ((WirelessSecurity *) self);
}

void
ws_leap_init (WirelessSecurityLEAP *self)
{
	gtk_widget_init_template (GTK_WIDGET (self));
}

void
ws_leap_class_init (WirelessSecurityLEAPClass *klass)
{
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

	gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/ControlCenter/network/ws-leap.ui");

	gtk_widget_class_bind_template_child (widget_class, WirelessSecurityLEAP, password_entry);
	gtk_widget_class_bind_template_child (widget_class, WirelessSecurityLEAP, password_label);
	gtk_widget_class_bind_template_child (widget_class, WirelessSecurityLEAP, show_password_check);
	gtk_widget_class_bind_template_child (widget_class, WirelessSecurityLEAP, username_entry);
	gtk_widget_class_bind_template_child (widget_class, WirelessSecurityLEAP, username_label);
}

static void
wireless_security_iface_init (WirelessSecurityInterface *iface)
{
	iface->validate = validate;
	iface->add_to_size_group = add_to_size_group;
	iface->fill_connection = fill_connection;
	iface->adhoc_compatible = adhoc_compatible;
}

WirelessSecurityLEAP *
ws_leap_new (NMConnection *connection)
{
	WirelessSecurityLEAP *self;
	NMSettingWirelessSecurity *wsec = NULL;

	self = g_object_new (ws_leap_get_type (), NULL);

	if (connection) {
		wsec = nm_connection_get_setting_wireless_security (connection);
		if (wsec) {
			const char *auth_alg;

			/* Ignore if wireless security doesn't specify LEAP */
			auth_alg = nm_setting_wireless_security_get_auth_alg (wsec);
			if (!auth_alg || strcmp (auth_alg, "leap"))
				wsec = NULL;
		}
	}

	g_signal_connect_swapped (self->password_entry, "changed", G_CALLBACK (changed_cb), self);

	/* Create password-storage popup menu for password entry under entry's secondary icon */
	nma_utils_setup_password_storage (GTK_WIDGET (self->password_entry), 0, (NMSetting *) wsec, NM_SETTING_WIRELESS_SECURITY_LEAP_PASSWORD,
	                                  FALSE, FALSE);

	if (wsec)
		helper_fill_secret_entry (connection,
		                          self->password_entry,
		                          NM_TYPE_SETTING_WIRELESS_SECURITY,
		                          (HelperSecretFunc) nm_setting_wireless_security_get_leap_password);

	g_signal_connect_swapped (self->username_entry, "changed", G_CALLBACK (changed_cb), self);
	if (wsec)
		gtk_entry_set_text (self->username_entry, nm_setting_wireless_security_get_leap_username (wsec));

	g_signal_connect_swapped (self->show_password_check, "toggled", G_CALLBACK (show_toggled_cb), self);

	return self;
}

