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

	GtkGrid        *grid;
	GtkEntry       *password_entry;
	GtkLabel       *password_label;
	GtkCheckButton *show_password_check;
	GtkEntry       *username_entry;
	GtkLabel       *username_label;

	WirelessSecurity *ws_parent;

	gboolean editing_connection;
};

static void
show_toggled_cb (EAPMethodLEAP *self)
{
	gboolean visible;
	visible = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (self->show_password_check));
	gtk_entry_set_visibility (self->password_entry, visible);
}

static gboolean
validate (EAPMethod *parent, GError **error)
{
	EAPMethodLEAP *self = (EAPMethodLEAP *)parent;
	const char *text;
	gboolean ret = TRUE;

	text = gtk_entry_get_text (self->username_entry);
	if (!text || !strlen (text)) {
		widget_set_error (GTK_WIDGET (self->username_entry));
		g_set_error_literal (error, NMA_ERROR, NMA_ERROR_GENERIC, _("missing EAP-LEAP username"));
		ret = FALSE;
	} else
		widget_unset_error (GTK_WIDGET (self->username_entry));

	text = gtk_entry_get_text (self->password_entry);
	if (!text || !strlen (text)) {
		widget_set_error (GTK_WIDGET (self->password_entry));
		if (ret) {
			g_set_error_literal (error, NMA_ERROR, NMA_ERROR_GENERIC, _("missing EAP-LEAP password"));
			ret = FALSE;
		}
	} else
		widget_unset_error (GTK_WIDGET (self->password_entry));

	return ret;
}

static void
add_to_size_group (EAPMethod *parent, GtkSizeGroup *group)
{
	EAPMethodLEAP *self = (EAPMethodLEAP *) parent;
	gtk_size_group_add_widget (group, GTK_WIDGET (self->username_label));
	gtk_size_group_add_widget (group, GTK_WIDGET (self->password_label));
}

static void
fill_connection (EAPMethod *parent, NMConnection *connection, NMSettingSecretFlags flags)
{
	EAPMethodLEAP *self = (EAPMethodLEAP *) parent;
	NMSetting8021x *s_8021x;
	NMSettingSecretFlags secret_flags;

	s_8021x = nm_connection_get_setting_802_1x (connection);
	g_assert (s_8021x);

	nm_setting_802_1x_add_eap_method (s_8021x, "leap");

	g_object_set (s_8021x, NM_SETTING_802_1X_IDENTITY, gtk_entry_get_text (self->username_entry), NULL);
	g_object_set (s_8021x, NM_SETTING_802_1X_PASSWORD, gtk_entry_get_text (self->password_entry), NULL);

	/* Save 802.1X password flags to the connection */
	secret_flags = nma_utils_menu_to_secret_flags (GTK_WIDGET (self->password_entry));
	nm_setting_set_secret_flags (NM_SETTING (s_8021x), parent->password_flags_name,
	                             secret_flags, NULL);

	/* Update secret flags and popup when editing the connection */
	if (self->editing_connection)
		nma_utils_update_password_storage (GTK_WIDGET (self->password_entry), secret_flags,
		                                   NM_SETTING (s_8021x), parent->password_flags_name);
}

static void
update_secrets (EAPMethod *parent, NMConnection *connection)
{
	EAPMethodLEAP *self = (EAPMethodLEAP *) parent;
	helper_fill_secret_entry (connection,
	                          self->password_entry,
	                          NM_TYPE_SETTING_802_1X,
	                          (HelperSecretFunc) nm_setting_802_1x_get_password);
}

/* Set the UI fields for user, password and show_password to the
 * values as provided by self->ws_parent. */
static void
set_userpass_ui (EAPMethodLEAP *self)
{
	if (wireless_security_get_username (self->ws_parent))
		gtk_entry_set_text (self->username_entry, wireless_security_get_username (self->ws_parent));
	else
		gtk_entry_set_text (self->username_entry, "");

	if (wireless_security_get_password (self->ws_parent) && !wireless_security_get_always_ask (self->ws_parent))
		gtk_entry_set_text (self->password_entry, wireless_security_get_password (self->ws_parent));
	else
		gtk_entry_set_text (self->password_entry, "");

	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (self->show_password_check), wireless_security_get_show_password (self->ws_parent));
}

static void
widgets_realized (EAPMethodLEAP *self)
{
	set_userpass_ui (self);
}

static void
widgets_unrealized (EAPMethodLEAP *self)
{
	wireless_security_set_userpass (self->ws_parent,
	                                gtk_entry_get_text (self->username_entry),
	                                gtk_entry_get_text (self->password_entry),
	                                (gboolean) -1,
	                                gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (self->show_password_check)));
}

static void
destroy (EAPMethod *parent)
{
	EAPMethodLEAP *self = (EAPMethodLEAP *) parent;

	g_signal_handlers_disconnect_by_data (self->grid, self);
	g_signal_handlers_disconnect_by_data (self->username_entry, self->ws_parent);
	g_signal_handlers_disconnect_by_data (self->password_entry, self->ws_parent);
	g_signal_handlers_disconnect_by_data (self->show_password_check, self);
}

static void
changed_cb (EAPMethodLEAP *self)
{
	wireless_security_notify_changed (self->ws_parent);
}

EAPMethodLEAP *
eap_method_leap_new (WirelessSecurity *ws_parent,
                     NMConnection *connection,
                     gboolean secrets_only)
{
	EAPMethodLEAP *self;
	EAPMethod *parent;
	NMSetting8021x *s_8021x = NULL;

	parent = eap_method_init (sizeof (EAPMethodLEAP),
	                          validate,
	                          add_to_size_group,
	                          fill_connection,
	                          update_secrets,
	                          destroy,
	                          "/org/gnome/ControlCenter/network/eap-method-leap.ui",
	                          "grid",
	                          "username_entry",
	                          FALSE);
	if (!parent)
		return NULL;

	parent->password_flags_name = NM_SETTING_802_1X_PASSWORD;
	self = (EAPMethodLEAP *) parent;
	self->editing_connection = secrets_only ? FALSE : TRUE;
	self->ws_parent = ws_parent;

	self->grid = GTK_GRID (gtk_builder_get_object (parent->builder, "grid"));
	self->password_entry = GTK_ENTRY (gtk_builder_get_object (parent->builder, "password_entry"));
	self->password_label = GTK_LABEL (gtk_builder_get_object (parent->builder, "password_label"));
	self->show_password_check = GTK_CHECK_BUTTON (gtk_builder_get_object (parent->builder, "show_password_check"));
	self->username_entry = GTK_ENTRY (gtk_builder_get_object (parent->builder, "username_entry"));
	self->username_label = GTK_LABEL (gtk_builder_get_object (parent->builder, "username_label"));

	g_signal_connect_swapped (self->grid, "realize", G_CALLBACK (widgets_realized), self);
	g_signal_connect_swapped (self->grid, "unrealize", G_CALLBACK (widgets_unrealized), self);

	g_signal_connect_swapped (self->username_entry, "changed", G_CALLBACK (changed_cb), self);

	if (secrets_only)
		gtk_widget_set_sensitive (GTK_WIDGET (self->username_entry), FALSE);

	g_signal_connect_swapped (self->password_entry, "changed", G_CALLBACK (changed_cb), self);

	/* Create password-storage popup menu for password entry under entry's secondary icon */
	if (connection)
		s_8021x = nm_connection_get_setting_802_1x (connection);
	nma_utils_setup_password_storage (GTK_WIDGET (self->password_entry), 0, (NMSetting *) s_8021x, parent->password_flags_name,
	                                  FALSE, secrets_only);

	g_signal_connect_swapped (self->show_password_check, "toggled", G_CALLBACK (show_toggled_cb), self);

	/* Initialize the UI fields with the security settings from self->ws_parent.
	 * This will be done again when the widget gets realized. It must be done here as well,
	 * because the outer dialog will ask to 'validate' the connection before the security tab
	 * is shown/realized (to enable the 'Apply' button).
	 * As 'validate' accesses the contents of the UI fields, they must be initialized now, even
	 * if the widgets are not yet visible. */
	set_userpass_ui (self);

	return self;
}

