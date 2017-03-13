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

#include "nm-default.h"

#include <string.h>

#include "wireless-security.h"
#include "helpers.h"
#include "nma-ui-utils.h"
#include "utils.h"

struct _WirelessSecurityLEAP {
	WirelessSecurity parent;
	gboolean editing_connection;
	const char *password_flags_name;
};

static void
show_toggled_cb (GtkCheckButton *button, WirelessSecurity *sec)
{
	GtkWidget *widget;
	gboolean visible;

	widget = GTK_WIDGET (gtk_builder_get_object (sec->builder, "leap_password_entry"));
	g_assert (widget);

	visible = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button));
	gtk_entry_set_visibility (GTK_ENTRY (widget), visible);
}

static gboolean
validate (WirelessSecurity *parent, GError **error)
{
	GtkWidget *entry;
	const char *text;
	gboolean ret = TRUE;

	entry = GTK_WIDGET (gtk_builder_get_object (parent->builder, "leap_username_entry"));
	g_assert (entry);
	text = gtk_entry_get_text (GTK_ENTRY (entry));
	if (!text || !strlen (text)) {
		widget_set_error (entry);
		g_set_error_literal (error, NMA_ERROR, NMA_ERROR_GENERIC, _("missing leap-username"));
		ret = FALSE;
	} else
		widget_unset_error (entry);

	entry = GTK_WIDGET (gtk_builder_get_object (parent->builder, "leap_password_entry"));
	g_assert (entry);
	text = gtk_entry_get_text (GTK_ENTRY (entry));
	if (!text || !strlen (text)) {
		widget_set_error (entry);
		if (ret) {
			g_set_error_literal (error, NMA_ERROR, NMA_ERROR_GENERIC, _("missing leap-password"));
			ret = FALSE;
		}
	} else
		widget_unset_error (entry);

	return ret;
}

static void
add_to_size_group (WirelessSecurity *parent, GtkSizeGroup *group)
{
	GtkWidget *widget;

	widget = GTK_WIDGET (gtk_builder_get_object (parent->builder, "leap_username_label"));
	gtk_size_group_add_widget (group, widget);

	widget = GTK_WIDGET (gtk_builder_get_object (parent->builder, "leap_password_label"));
	gtk_size_group_add_widget (group, widget);
}

static void
fill_connection (WirelessSecurity *parent, NMConnection *connection)
{
	WirelessSecurityLEAP *sec = (WirelessSecurityLEAP *) parent;
	NMSettingWirelessSecurity *s_wireless_sec;
	NMSettingSecretFlags secret_flags;
	GtkWidget *widget, *passwd_entry;
	const char *leap_password = NULL, *leap_username = NULL;

	/* Blow away the old security setting by adding a clear one */
	s_wireless_sec = (NMSettingWirelessSecurity *) nm_setting_wireless_security_new ();
	nm_connection_add_setting (connection, (NMSetting *) s_wireless_sec);

	widget = GTK_WIDGET (gtk_builder_get_object (parent->builder, "leap_username_entry"));
	leap_username = gtk_entry_get_text (GTK_ENTRY (widget));

	widget = GTK_WIDGET (gtk_builder_get_object (parent->builder, "leap_password_entry"));
	passwd_entry = widget;
	leap_password = gtk_entry_get_text (GTK_ENTRY (widget));

	g_object_set (s_wireless_sec,
	              NM_SETTING_WIRELESS_SECURITY_KEY_MGMT, "ieee8021x",
	              NM_SETTING_WIRELESS_SECURITY_AUTH_ALG, "leap",
	              NM_SETTING_WIRELESS_SECURITY_LEAP_USERNAME, leap_username,
	              NM_SETTING_WIRELESS_SECURITY_LEAP_PASSWORD, leap_password,
	              NULL);

	/* Save LEAP_PASSWORD_FLAGS to the connection */
	secret_flags = nma_utils_menu_to_secret_flags (passwd_entry);
	nm_setting_set_secret_flags (NM_SETTING (s_wireless_sec), sec->password_flags_name,
	                             secret_flags, NULL);

	/* Update secret flags and popup when editing the connection */
	if (sec->editing_connection)
		nma_utils_update_password_storage (passwd_entry, secret_flags,
		                                   NM_SETTING (s_wireless_sec), sec->password_flags_name);
}

static void
update_secrets (WirelessSecurity *parent, NMConnection *connection)
{
	helper_fill_secret_entry (connection,
	                          parent->builder,
	                          "leap_password_entry",
	                          NM_TYPE_SETTING_WIRELESS_SECURITY,
	                          (HelperSecretFunc) nm_setting_wireless_security_get_leap_password);
}

WirelessSecurityLEAP *
ws_leap_new (NMConnection *connection, gboolean secrets_only)
{
	WirelessSecurity *parent;
	WirelessSecurityLEAP *sec;
	GtkWidget *widget;
	NMSettingWirelessSecurity *wsec = NULL;

	parent = wireless_security_init (sizeof (WirelessSecurityLEAP),
	                                 validate,
	                                 add_to_size_group,
	                                 fill_connection,
	                                 update_secrets,
	                                 NULL,
	                                 "/org/freedesktop/network-manager-applet/ws-leap.ui",
	                                 "leap_notebook",
	                                 "leap_username_entry");
	if (!parent)
		return NULL;

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

	parent->adhoc_compatible = FALSE;
	parent->hotspot_compatible = FALSE;
	sec = (WirelessSecurityLEAP *) parent;
	sec->editing_connection = secrets_only ? FALSE : TRUE;
	sec->password_flags_name = NM_SETTING_WIRELESS_SECURITY_LEAP_PASSWORD;

	widget = GTK_WIDGET (gtk_builder_get_object (parent->builder, "leap_password_entry"));
	g_assert (widget);
	g_signal_connect (G_OBJECT (widget), "changed",
	                  (GCallback) wireless_security_changed_cb,
	                  sec);

	/* Create password-storage popup menu for password entry under entry's secondary icon */
	nma_utils_setup_password_storage (widget, 0, (NMSetting *) wsec, sec->password_flags_name,
	                                  FALSE, secrets_only);

	if (wsec)
		update_secrets (WIRELESS_SECURITY (sec), connection);

	widget = GTK_WIDGET (gtk_builder_get_object (parent->builder, "leap_username_entry"));
	g_assert (widget);
	g_signal_connect (G_OBJECT (widget), "changed",
	                  (GCallback) wireless_security_changed_cb,
	                  sec);
	if (wsec)
		gtk_entry_set_text (GTK_ENTRY (widget), nm_setting_wireless_security_get_leap_username (wsec));

	if (secrets_only)
		gtk_widget_hide (widget);

	widget = GTK_WIDGET (gtk_builder_get_object (parent->builder, "show_checkbutton_leap"));
	g_assert (widget);
	g_signal_connect (G_OBJECT (widget), "toggled",
	                  (GCallback) show_toggled_cb,
	                  sec);

	return sec;
}

