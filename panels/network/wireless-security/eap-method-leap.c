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
 * (C) Copyright 2007 - 2010 Red Hat, Inc.
 */

#include <ctype.h>
#include <string.h>
#include <nm-setting-8021x.h>

#include "eap-method.h"
#include "wireless-security.h"
#include "helpers.h"

struct _EAPMethodLEAP {
	EAPMethod parent;

	gboolean new_connection;
};

static void
show_toggled_cb (GtkCheckButton *button, EAPMethod *method)
{
	GtkWidget *widget;
	gboolean visible;

	widget = GTK_WIDGET (gtk_builder_get_object (method->builder, "eap_leap_password_entry"));
	g_assert (widget);

	visible = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button));
	gtk_entry_set_visibility (GTK_ENTRY (widget), visible);
}

static gboolean
validate (EAPMethod *parent)
{
	GtkWidget *widget;
	const char *text;

	widget = GTK_WIDGET (gtk_builder_get_object (parent->builder, "eap_leap_username_entry"));
	g_assert (widget);
	text = gtk_entry_get_text (GTK_ENTRY (widget));
	if (!text || !strlen (text))
		return FALSE;

	widget = GTK_WIDGET (gtk_builder_get_object (parent->builder, "eap_leap_password_entry"));
	g_assert (widget);
	text = gtk_entry_get_text (GTK_ENTRY (widget));
	if (!text || !strlen (text))
		return FALSE;

	return TRUE;
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
fill_connection (EAPMethod *parent, NMConnection *connection)
{
	EAPMethodLEAP *method = (EAPMethodLEAP *) parent;
	NMSetting8021x *s_8021x;
	GtkWidget *widget;

	s_8021x = nm_connection_get_setting_802_1x (connection);
	g_assert (s_8021x);

	nm_setting_802_1x_add_eap_method (s_8021x, "leap");

	widget = GTK_WIDGET (gtk_builder_get_object (parent->builder, "eap_leap_username_entry"));
	g_assert (widget);
	g_object_set (s_8021x, NM_SETTING_802_1X_IDENTITY, gtk_entry_get_text (GTK_ENTRY (widget)), NULL);

	widget = GTK_WIDGET (gtk_builder_get_object (parent->builder, "eap_leap_password_entry"));
	g_assert (widget);
	g_object_set (s_8021x, NM_SETTING_802_1X_PASSWORD, gtk_entry_get_text (GTK_ENTRY (widget)), NULL);

	/* Default to agent-owned secrets for new connections */
	if (method->new_connection) {
		g_object_set (s_8021x,
		              NM_SETTING_802_1X_PASSWORD_FLAGS, NM_SETTING_SECRET_FLAG_AGENT_OWNED,
		              NM_SETTING_802_1X_SYSTEM_CA_CERTS, TRUE,
		              NULL);
	}
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

EAPMethodLEAP *
eap_method_leap_new (WirelessSecurity *ws_parent,
                     NMConnection *connection,
                     gboolean secrets_only)
{
	EAPMethodLEAP *method;
	EAPMethod *parent;
	GtkWidget *widget;

	parent = eap_method_init (sizeof (EAPMethodLEAP),
	                          validate,
	                          add_to_size_group,
	                          fill_connection,
	                          update_secrets,
	                          NULL,
	                          UIDIR "/eap-method-leap.ui",
	                          "eap_leap_notebook",
	                          "eap_leap_username_entry",
	                          FALSE);
	if (!parent)
		return NULL;

	method = (EAPMethodLEAP *) parent;
	method->new_connection = secrets_only ? FALSE : TRUE;

	widget = GTK_WIDGET (gtk_builder_get_object (parent->builder, "eap_leap_username_entry"));
	g_assert (widget);
	g_signal_connect (G_OBJECT (widget), "changed",
	                  (GCallback) wireless_security_changed_cb,
	                  ws_parent);
	if (connection) {
		NMSetting8021x *s_8021x;

		s_8021x = nm_connection_get_setting_802_1x (connection);
		if (s_8021x && nm_setting_802_1x_get_identity (s_8021x))
			gtk_entry_set_text (GTK_ENTRY (widget), nm_setting_802_1x_get_identity (s_8021x));
	}

	if (secrets_only)
		gtk_widget_set_sensitive (widget, FALSE);

	widget = GTK_WIDGET (gtk_builder_get_object (parent->builder, "eap_leap_password_entry"));
	g_assert (widget);
	g_signal_connect (G_OBJECT (widget), "changed",
	                  (GCallback) wireless_security_changed_cb,
	                  ws_parent);

	/* Fill secrets, if any */
	if (connection)
		update_secrets (parent, connection);

	widget = GTK_WIDGET (gtk_builder_get_object (parent->builder, "show_checkbutton_eapleap"));
	g_assert (widget);
	g_signal_connect (G_OBJECT (widget), "toggled",
	                  (GCallback) show_toggled_cb,
	                  parent);

	return method;
}

