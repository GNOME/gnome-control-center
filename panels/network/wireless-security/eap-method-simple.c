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
 * (C) Copyright 2007 - 2011 Red Hat, Inc.
 */

#include <ctype.h>
#include <string.h>
#include <nm-setting-8021x.h>
#include <nm-setting-connection.h>

#include "eap-method.h"
#include "wireless-security.h"
#include "helpers.h"

struct _EAPMethodSimple {
	EAPMethod parent;

	EAPMethodSimpleType type;
	gboolean is_editor;
	gboolean new_connection;
};

static void
show_toggled_cb (GtkCheckButton *button, EAPMethod *method)
{
	GtkWidget *widget;
	gboolean visible;

	widget = GTK_WIDGET (gtk_builder_get_object (method->builder, "eap_simple_password_entry"));
	g_assert (widget);

	visible = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button));
	gtk_entry_set_visibility (GTK_ENTRY (widget), visible);
}

static gboolean
validate (EAPMethod *parent)
{
	GtkWidget *widget;
	const char *text;

	widget = GTK_WIDGET (gtk_builder_get_object (parent->builder, "eap_simple_username_entry"));
	g_assert (widget);
	text = gtk_entry_get_text (GTK_ENTRY (widget));
	if (!text || !strlen (text))
		return FALSE;

	/* Check if the password should always be requested */
	widget = GTK_WIDGET (gtk_builder_get_object (parent->builder, "eap_password_always_ask"));
	g_assert (widget);
	if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget)))
		return TRUE;

	widget = GTK_WIDGET (gtk_builder_get_object (parent->builder, "eap_simple_password_entry"));
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

	widget = GTK_WIDGET (gtk_builder_get_object (parent->builder, "eap_simple_username_label"));
	g_assert (widget);
	gtk_size_group_add_widget (group, widget);

	widget = GTK_WIDGET (gtk_builder_get_object (parent->builder, "eap_simple_password_label"));
	g_assert (widget);
	gtk_size_group_add_widget (group, widget);
}

static void
fill_connection (EAPMethod *parent, NMConnection *connection)
{
	EAPMethodSimple *method = (EAPMethodSimple *) parent;
	NMSetting8021x *s_8021x;
	GtkWidget *widget;
	gboolean not_saved = FALSE;
	const char *eap = NULL;
	NMSettingSecretFlags flags = NM_SETTING_SECRET_FLAG_NONE;

	s_8021x = nm_connection_get_setting_802_1x (connection);
	g_assert (s_8021x);

	/* If this is the main EAP method, clear any existing methods because the
	 * user-selected on will replace it.
	 */
	if (parent->phase2 == FALSE)
		nm_setting_802_1x_clear_eap_methods (s_8021x);

	switch (method->type) {
		case EAP_METHOD_SIMPLE_TYPE_PAP:
			eap = "pap";
			break;
		case EAP_METHOD_SIMPLE_TYPE_MSCHAP:
			eap = "mschap";
			break;
		case EAP_METHOD_SIMPLE_TYPE_MSCHAP_V2:
			eap = "mschapv2";
			break;
		case EAP_METHOD_SIMPLE_TYPE_MD5:
			eap = "md5";
			break;
		case EAP_METHOD_SIMPLE_TYPE_CHAP:
			eap = "chap";
			break;
		case EAP_METHOD_SIMPLE_TYPE_GTC:
			eap = "gtc";
			break;
		default:
			g_assert_not_reached ();
			break;
	}

	if (parent->phase2)
		g_object_set (s_8021x, NM_SETTING_802_1X_PHASE2_AUTH, eap, NULL);
	else
		nm_setting_802_1x_add_eap_method (s_8021x, eap);

	widget = GTK_WIDGET (gtk_builder_get_object (parent->builder, "eap_simple_username_entry"));
	g_assert (widget);
	g_object_set (s_8021x, NM_SETTING_802_1X_IDENTITY, gtk_entry_get_text (GTK_ENTRY (widget)), NULL);

	/* Save the password always ask setting */
	widget = GTK_WIDGET (gtk_builder_get_object (parent->builder, "eap_password_always_ask"));
	g_assert (widget);
	not_saved = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));

	nm_setting_get_secret_flags (NM_SETTING (s_8021x), NM_SETTING_802_1X_PASSWORD, &flags, NULL);
	flags &= ~(NM_SETTING_SECRET_FLAG_NOT_SAVED);
	if (not_saved)
		flags |= NM_SETTING_SECRET_FLAG_NOT_SAVED;
	nm_setting_set_secret_flags (NM_SETTING (s_8021x), NM_SETTING_802_1X_PASSWORD, flags, NULL);

	/* Fill the connection's password if we're in the applet so that it'll get
	 * back to NM.  From the editor though, since the connection isn't going
	 * back to NM in response to a GetSecrets() call, we don't save it if the
	 * user checked "Always Ask".
	 */
	if (method->is_editor == FALSE || not_saved == FALSE) {
		widget = GTK_WIDGET (gtk_builder_get_object (parent->builder, "eap_simple_password_entry"));
		g_assert (widget);
		g_object_set (s_8021x, NM_SETTING_802_1X_PASSWORD, gtk_entry_get_text (GTK_ENTRY (widget)), NULL);
	}

	/* Default to agent-owned secrets for new connections */
	if (method->new_connection && (not_saved == FALSE)) {
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
	                          "eap_simple_password_entry",
	                          NM_TYPE_SETTING_802_1X,
	                          (HelperSecretFunc) nm_setting_802_1x_get_password);
}

static void
password_always_ask_changed (GtkButton *button, EAPMethodSimple *method)
{
	EAPMethod *parent = (EAPMethod *) method;
	GtkWidget *password_entry;
	GtkWidget *show_checkbox;
	gboolean always_ask;

	always_ask = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button));

	password_entry = GTK_WIDGET (gtk_builder_get_object (parent->builder, "eap_simple_password_entry"));
	g_assert (password_entry);

	show_checkbox = GTK_WIDGET (gtk_builder_get_object (parent->builder, "show_checkbutton_eapsimple"));
	g_assert (show_checkbox);

	if (always_ask) {
		gtk_entry_set_text (GTK_ENTRY (password_entry), "");
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (show_checkbox), FALSE);
	}

	gtk_widget_set_sensitive (password_entry, !always_ask);
	gtk_widget_set_sensitive (show_checkbox, !always_ask);
}

EAPMethodSimple *
eap_method_simple_new (WirelessSecurity *ws_parent,
                       NMConnection *connection,
                       EAPMethodSimpleType type,
                       gboolean phase2,
                       gboolean is_editor,
                       gboolean secrets_only)
{
	EAPMethod *parent;
	EAPMethodSimple *method;
	GtkWidget *widget;
	gboolean not_saved = FALSE;
	NMSetting8021x *s_8021x = NULL;

	parent = eap_method_init (sizeof (EAPMethodSimple),
	                          validate,
	                          add_to_size_group,
	                          fill_connection,
	                          update_secrets,
	                          NULL,
	                          UIDIR "/eap-method-simple.ui",
	                          "eap_simple_notebook",
	                          "eap_simple_username_entry",
	                          phase2);
	if (!parent)
		return NULL;

	method = (EAPMethodSimple *) parent;
	method->type = type;
	method->is_editor = is_editor;
	method->new_connection = secrets_only ? FALSE : TRUE;

	widget = GTK_WIDGET (gtk_builder_get_object (parent->builder, "eap_simple_username_entry"));
	g_assert (widget);
	g_signal_connect (G_OBJECT (widget), "changed",
	                  (GCallback) wireless_security_changed_cb,
	                  ws_parent);
	if (connection) {
		s_8021x = nm_connection_get_setting_802_1x (connection);
		if (s_8021x && nm_setting_802_1x_get_identity (s_8021x))
			gtk_entry_set_text (GTK_ENTRY (widget), nm_setting_802_1x_get_identity (s_8021x));
	}

	if (secrets_only)
		gtk_widget_set_sensitive (widget, FALSE);

	widget = GTK_WIDGET (gtk_builder_get_object (parent->builder, "eap_simple_password_entry"));
	g_assert (widget);
	g_signal_connect (G_OBJECT (widget), "changed",
	                  (GCallback) wireless_security_changed_cb,
	                  ws_parent);

	widget = GTK_WIDGET (gtk_builder_get_object (parent->builder, "eap_password_always_ask"));
	g_assert (widget);
	g_signal_connect (G_OBJECT (widget), "toggled",
	                  (GCallback) wireless_security_changed_cb,
	                  ws_parent);
	if (is_editor) {
		/* We only desensitize the password entry from the editor, because
		 * from nm-applet if the entry was desensitized, there'd be no way to
		 * get the password back to NetworkManager when NM asked for it.  Since
		 * the editor only sets up the initial connection though, it's safe to
		 * do there.
		 */
		g_signal_connect (G_OBJECT (widget), "toggled",
		                  G_CALLBACK (password_always_ask_changed),
		                  method);
	}

	if (secrets_only)
		gtk_widget_hide (widget);

	if (s_8021x) {
		NMSettingSecretFlags flags = NM_SETTING_SECRET_FLAG_NONE;

		nm_setting_get_secret_flags (NM_SETTING (s_8021x), NM_SETTING_802_1X_PASSWORD, &flags, NULL);
		not_saved = (flags & NM_SETTING_SECRET_FLAG_NOT_SAVED);
	}

	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), not_saved);

	/* Fill secrets if there's a static (ie, not OTP) password */
	if (connection && (not_saved == FALSE))
		update_secrets (EAP_METHOD (method), connection);

	widget = GTK_WIDGET (gtk_builder_get_object (parent->builder, "show_checkbutton_eapsimple"));
	g_assert (widget);
	g_signal_connect (G_OBJECT (widget), "toggled",
	                  (GCallback) show_toggled_cb,
	                  method);

	return method;
}

