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

#include <ctype.h>
#include <string.h>

#include "eap-method.h"
#include "wireless-security.h"
#include "helpers.h"
#include "nma-ui-utils.h"
#include "utils.h"

struct _EAPMethodLEAP {
	EAPMethod parent;

	WirelessSecurity *ws_parent;

	gboolean editing_connection;

	GtkEntry *username_entry;
	GtkEntry *password_entry;
	GtkToggleButton *show_password;
};

static void
show_toggled_cb (GtkToggleButton *button, EAPMethodLEAP *method)
{
	gboolean visible;

	visible = gtk_toggle_button_get_active (button);
	gtk_entry_set_visibility (method->password_entry, visible);
}

static gboolean
validate (EAPMethod *parent, GError **error)
{
	EAPMethodLEAP *method = (EAPMethodLEAP *)parent;
	const char *text;
	gboolean ret = TRUE;

	text = gtk_entry_get_text (method->username_entry);
	if (!text || !strlen (text)) {
		widget_set_error (GTK_WIDGET (method->username_entry));
		g_set_error_literal (error, NMA_ERROR, NMA_ERROR_GENERIC, _("missing EAP-LEAP username"));
		ret = FALSE;
	} else
		widget_unset_error (GTK_WIDGET (method->username_entry));

	text = gtk_entry_get_text (method->password_entry);
	if (!text || !strlen (text)) {
		widget_set_error (GTK_WIDGET (method->password_entry));
		if (ret) {
			g_set_error_literal (error, NMA_ERROR, NMA_ERROR_GENERIC, _("missing EAP-LEAP password"));
			ret = FALSE;
		}
	} else
		widget_unset_error (GTK_WIDGET (method->password_entry));

	return ret;
}

static void
add_to_size_group (EAPMethod *parent, GtkSizeGroup *group)
{
	GtkWidget *widget;

	widget = GTK_WIDGET (gtk_builder_get_object (parent->builder, "eap_leap_username_label"));
	g_assert (widget);
	gtk_size_group_add_widget (group, widget);

	widget = GTK_WIDGET (gtk_builder_get_object (parent->builder, "eap_leap_password_label"));
	g_assert (widget);
	gtk_size_group_add_widget (group, widget);
}

static void
fill_connection (EAPMethod *parent, NMConnection *connection, NMSettingSecretFlags flags)
{
	EAPMethodLEAP *method = (EAPMethodLEAP *) parent;
	NMSetting8021x *s_8021x;
	NMSettingSecretFlags secret_flags;
	GtkWidget *passwd_entry;

	s_8021x = nm_connection_get_setting_802_1x (connection);
	g_assert (s_8021x);

	nm_setting_802_1x_add_eap_method (s_8021x, "leap");

	g_object_set (s_8021x, NM_SETTING_802_1X_IDENTITY, gtk_entry_get_text (method->username_entry), NULL);
	g_object_set (s_8021x, NM_SETTING_802_1X_PASSWORD, gtk_entry_get_text (method->password_entry), NULL);

	passwd_entry = GTK_WIDGET (gtk_builder_get_object (parent->builder, "eap_leap_password_entry"));
	g_assert (passwd_entry);

	/* Save 802.1X password flags to the connection */
	secret_flags = nma_utils_menu_to_secret_flags (passwd_entry);
	nm_setting_set_secret_flags (NM_SETTING (s_8021x), parent->password_flags_name,
	                             secret_flags, NULL);

	/* Update secret flags and popup when editing the connection */
	if (method->editing_connection)
		nma_utils_update_password_storage (passwd_entry, secret_flags,
		                                   NM_SETTING (s_8021x), parent->password_flags_name);
}

static void
update_secrets (EAPMethod *parent, NMConnection *connection)
{
	helper_fill_secret_entry (connection,
	                          parent->builder,
	                          "eap_leap_password_entry",
	                          NM_TYPE_SETTING_802_1X,
	                          (HelperSecretFunc) nm_setting_802_1x_get_password);
}

/* Set the UI fields for user, password and show_password to the
 * values as provided by method->ws_parent. */
static void
set_userpass_ui (EAPMethodLEAP *method)
{
	if (method->ws_parent->username)
		gtk_entry_set_text (method->username_entry, method->ws_parent->username);
	else
		gtk_entry_set_text (method->username_entry, "");

	if (method->ws_parent->password && !method->ws_parent->always_ask)
		gtk_entry_set_text (method->password_entry, method->ws_parent->password);
	else
		gtk_entry_set_text (method->password_entry, "");

	gtk_toggle_button_set_active (method->show_password, method->ws_parent->show_password);
}

static void
widgets_realized (GtkWidget *widget, EAPMethodLEAP *method)
{
	set_userpass_ui (method);
}

static void
widgets_unrealized (GtkWidget *widget, EAPMethodLEAP *method)
{
	wireless_security_set_userpass (method->ws_parent,
	                                gtk_entry_get_text (method->username_entry),
	                                gtk_entry_get_text (method->password_entry),
	                                (gboolean) -1,
	                                gtk_toggle_button_get_active (method->show_password));
}

static void
destroy (EAPMethod *parent)
{
	EAPMethodLEAP *method = (EAPMethodLEAP *) parent;
	GtkWidget *widget;

	widget = GTK_WIDGET (gtk_builder_get_object (parent->builder, "eap_leap_notebook"));
	g_assert (widget);
	g_signal_handlers_disconnect_by_data (widget, method);

	g_signal_handlers_disconnect_by_data (method->username_entry, method->ws_parent);
	g_signal_handlers_disconnect_by_data (method->password_entry, method->ws_parent);
	g_signal_handlers_disconnect_by_data (method->show_password, method);
}

EAPMethodLEAP *
eap_method_leap_new (WirelessSecurity *ws_parent,
                     NMConnection *connection,
                     gboolean secrets_only)
{
	EAPMethodLEAP *method;
	EAPMethod *parent;
	GtkWidget *widget;
	NMSetting8021x *s_8021x = NULL;

	parent = eap_method_init (sizeof (EAPMethodLEAP),
	                          validate,
	                          add_to_size_group,
	                          fill_connection,
	                          update_secrets,
	                          destroy,
	                          "/org/freedesktop/network-manager-applet/eap-method-leap.ui",
	                          "eap_leap_notebook",
	                          "eap_leap_username_entry",
	                          FALSE);
	if (!parent)
		return NULL;

	parent->password_flags_name = NM_SETTING_802_1X_PASSWORD;
	method = (EAPMethodLEAP *) parent;
	method->editing_connection = secrets_only ? FALSE : TRUE;
	method->ws_parent = ws_parent;

	widget = GTK_WIDGET (gtk_builder_get_object (parent->builder, "eap_leap_notebook"));
	g_assert (widget);
	g_signal_connect (G_OBJECT (widget), "realize",
	                  (GCallback) widgets_realized,
	                  method);
	g_signal_connect (G_OBJECT (widget), "unrealize",
	                  (GCallback) widgets_unrealized,
	                  method);

	widget = GTK_WIDGET (gtk_builder_get_object (parent->builder, "eap_leap_username_entry"));
	g_assert (widget);
	method->username_entry = GTK_ENTRY (widget);
	g_signal_connect (G_OBJECT (widget), "changed",
	                  (GCallback) wireless_security_changed_cb,
	                  ws_parent);

	if (secrets_only)
		gtk_widget_set_sensitive (widget, FALSE);

	widget = GTK_WIDGET (gtk_builder_get_object (parent->builder, "eap_leap_password_entry"));
	g_assert (widget);
	method->password_entry = GTK_ENTRY (widget);
	g_signal_connect (G_OBJECT (widget), "changed",
	                  (GCallback) wireless_security_changed_cb,
	                  ws_parent);

	/* Create password-storage popup menu for password entry under entry's secondary icon */
	if (connection)
		s_8021x = nm_connection_get_setting_802_1x (connection);
	nma_utils_setup_password_storage (widget, 0, (NMSetting *) s_8021x, parent->password_flags_name,
	                                  FALSE, secrets_only);

	widget = GTK_WIDGET (gtk_builder_get_object (parent->builder, "show_checkbutton_eapleap"));
	g_assert (widget);
	method->show_password = GTK_TOGGLE_BUTTON (widget);
	g_signal_connect (G_OBJECT (widget), "toggled",
	                  (GCallback) show_toggled_cb,
	                  parent);

	/* Initialize the UI fields with the security settings from method->ws_parent.
	 * This will be done again when the widget gets realized. It must be done here as well,
	 * because the outer dialog will ask to 'validate' the connection before the security tab
	 * is shown/realized (to enable the 'Apply' button).
	 * As 'validate' accesses the contents of the UI fields, they must be initialized now, even
	 * if the widgets are not yet visible. */
	set_userpass_ui (method);

	return method;
}

