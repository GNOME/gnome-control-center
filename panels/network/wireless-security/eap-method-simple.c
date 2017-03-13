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

struct _EAPMethodSimple {
	EAPMethod parent;

	WirelessSecurity *ws_parent;

	EAPMethodSimpleType type;
	EAPMethodSimpleFlags flags;

	GtkEntry *username_entry;
	GtkEntry *password_entry;
	GtkToggleButton *show_password;
	guint idle_func_id;
};

static void
show_toggled_cb (GtkToggleButton *button, EAPMethodSimple *method)
{
	gboolean visible;

	visible = gtk_toggle_button_get_active (button);
	gtk_entry_set_visibility (method->password_entry, visible);
}

static gboolean
always_ask_selected (GtkEntry *passwd_entry)
{
	return !!(  nma_utils_menu_to_secret_flags (GTK_WIDGET (passwd_entry))
	          & NM_SETTING_SECRET_FLAG_NOT_SAVED);
}

static gboolean
validate (EAPMethod *parent, GError **error)
{
	EAPMethodSimple *method = (EAPMethodSimple *)parent;
	const char *text;
	gboolean ret = TRUE;

	text = gtk_entry_get_text (method->username_entry);
	if (!text || !strlen (text)) {
		widget_set_error (GTK_WIDGET (method->username_entry));
		g_set_error_literal (error, NMA_ERROR, NMA_ERROR_GENERIC, _("missing EAP username"));
		ret = FALSE;
	} else
		widget_unset_error (GTK_WIDGET (method->username_entry));

	/* Check if the password should always be requested */
	if (always_ask_selected (method->password_entry))
		widget_unset_error (GTK_WIDGET (method->password_entry));
	else {
		text = gtk_entry_get_text (method->password_entry);
		if (!text || !strlen (text)) {
			widget_set_error (GTK_WIDGET (method->password_entry));
			if (ret) {
				g_set_error_literal (error, NMA_ERROR, NMA_ERROR_GENERIC, _("missing EAP password"));
				ret = FALSE;
			}
		} else
			widget_unset_error (GTK_WIDGET (method->password_entry));
	}

	return ret;
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

typedef struct {
	const char *name;
	gboolean autheap_allowed;
} EapType;

/* Indexed by EAP_METHOD_SIMPLE_TYPE_* */
static const EapType eap_table[EAP_METHOD_SIMPLE_TYPE_LAST] = {
	[EAP_METHOD_SIMPLE_TYPE_PAP]             = { "pap",      FALSE },
	[EAP_METHOD_SIMPLE_TYPE_MSCHAP]          = { "mschap",   FALSE },
	[EAP_METHOD_SIMPLE_TYPE_MSCHAP_V2]       = { "mschapv2", TRUE  },
	[EAP_METHOD_SIMPLE_TYPE_PLAIN_MSCHAP_V2] = { "mschapv2", FALSE },
	[EAP_METHOD_SIMPLE_TYPE_MD5]             = { "md5",      TRUE  },
	[EAP_METHOD_SIMPLE_TYPE_PWD]             = { "pwd",      TRUE  },
	[EAP_METHOD_SIMPLE_TYPE_CHAP]            = { "chap",     FALSE },
	[EAP_METHOD_SIMPLE_TYPE_GTC]             = { "gtc",      TRUE  },
};

static void
fill_connection (EAPMethod *parent, NMConnection *connection, NMSettingSecretFlags prev_flags)
{
	EAPMethodSimple *method = (EAPMethodSimple *) parent;
	NMSetting8021x *s_8021x;
	gboolean not_saved = FALSE;
	NMSettingSecretFlags flags;
	const EapType *eap_type;

	s_8021x = nm_connection_get_setting_802_1x (connection);
	g_assert (s_8021x);

	/* If this is the main EAP method, clear any existing methods because the
	 * user-selected on will replace it.
	 */
	if (parent->phase2 == FALSE)
		nm_setting_802_1x_clear_eap_methods (s_8021x);

	eap_type = &eap_table[method->type];
	if (parent->phase2) {
		/* If the outer EAP method (TLS, TTLS, PEAP, etc) allows inner/phase2
		 * EAP methods (which only TTLS allows) *and* the inner/phase2 method
		 * supports being an inner EAP method, then set PHASE2_AUTHEAP.
		 * Otherwise the inner/phase2 method goes into PHASE2_AUTH.
		 */
		if ((method->flags & EAP_METHOD_SIMPLE_FLAG_AUTHEAP_ALLOWED) && eap_type->autheap_allowed) {
			g_object_set (s_8021x, NM_SETTING_802_1X_PHASE2_AUTHEAP, eap_type->name, NULL);
			g_object_set (s_8021x, NM_SETTING_802_1X_PHASE2_AUTH, NULL, NULL);
		} else {
			g_object_set (s_8021x, NM_SETTING_802_1X_PHASE2_AUTH, eap_type->name, NULL);
			g_object_set (s_8021x, NM_SETTING_802_1X_PHASE2_AUTHEAP, NULL, NULL);
		}
	} else
		nm_setting_802_1x_add_eap_method (s_8021x, eap_type->name);

	g_object_set (s_8021x, NM_SETTING_802_1X_IDENTITY, gtk_entry_get_text (method->username_entry), NULL);

	/* Save the password always ask setting */
	not_saved = always_ask_selected (method->password_entry);
	flags = nma_utils_menu_to_secret_flags (GTK_WIDGET (method->password_entry));
	nm_setting_set_secret_flags (NM_SETTING (s_8021x), NM_SETTING_802_1X_PASSWORD, flags, NULL);

	/* Fill the connection's password if we're in the applet so that it'll get
	 * back to NM.  From the editor though, since the connection isn't going
	 * back to NM in response to a GetSecrets() call, we don't save it if the
	 * user checked "Always Ask".
	 */
	if (!(method->flags & EAP_METHOD_SIMPLE_FLAG_IS_EDITOR) || not_saved == FALSE)
		g_object_set (s_8021x, NM_SETTING_802_1X_PASSWORD, gtk_entry_get_text (method->password_entry), NULL);

	/* Update secret flags and popup when editing the connection */
	if (!(method->flags & EAP_METHOD_SIMPLE_FLAG_SECRETS_ONLY)) {
		GtkWidget *passwd_entry = GTK_WIDGET (gtk_builder_get_object (parent->builder, "eap_simple_password_entry"));
		g_assert (passwd_entry);

		nma_utils_update_password_storage (passwd_entry, flags,
		                                   NM_SETTING (s_8021x), parent->password_flags_name);
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

static gboolean
stuff_changed (EAPMethodSimple *method)
{
	wireless_security_changed_cb (NULL, method->ws_parent);
	method->idle_func_id = 0;
	return FALSE;
}

static void
password_storage_changed (GObject *entry,
                          GParamSpec *pspec,
                          EAPMethodSimple *method)
{
	gboolean always_ask;
	gboolean secrets_only = method->flags & EAP_METHOD_SIMPLE_FLAG_SECRETS_ONLY;

	always_ask = always_ask_selected (method->password_entry);

	if (always_ask && !secrets_only) {
		/* we always clear this button and do not restore it
		 * (because we want to hide the password). */
		gtk_toggle_button_set_active (method->show_password, FALSE);
	}

	gtk_widget_set_sensitive (GTK_WIDGET (method->show_password),
	                          !always_ask || secrets_only);

	if (!method->idle_func_id)
		method->idle_func_id = g_idle_add ((GSourceFunc) stuff_changed, method);
}

/* Set the UI fields for user, password, always_ask and show_password to the
 * values as provided by method->ws_parent. */
static void
set_userpass_ui (EAPMethodSimple *method)
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
	password_storage_changed (NULL, NULL, method);
}

static void
widgets_realized (GtkWidget *widget, EAPMethodSimple *method)
{
	set_userpass_ui (method);
}

static void
widgets_unrealized (GtkWidget *widget, EAPMethodSimple *method)
{
	wireless_security_set_userpass (method->ws_parent,
	                                gtk_entry_get_text (method->username_entry),
	                                gtk_entry_get_text (method->password_entry),
	                                always_ask_selected (method->password_entry),
	                                gtk_toggle_button_get_active (method->show_password));
}

static void
destroy (EAPMethod *parent)
{
	EAPMethodSimple *method = (EAPMethodSimple *) parent;
	GtkWidget *widget;

	widget = GTK_WIDGET (gtk_builder_get_object (parent->builder, "eap_simple_notebook"));
	g_assert (widget);
	g_signal_handlers_disconnect_by_data (widget, method);

	g_signal_handlers_disconnect_by_data (method->username_entry, method->ws_parent);
	g_signal_handlers_disconnect_by_data (method->password_entry, method->ws_parent);
	g_signal_handlers_disconnect_by_data (method->password_entry, method);
	g_signal_handlers_disconnect_by_data (method->show_password, method);

	nm_clear_g_source (&method->idle_func_id);
}

EAPMethodSimple *
eap_method_simple_new (WirelessSecurity *ws_parent,
                       NMConnection *connection,
                       EAPMethodSimpleType type,
                       EAPMethodSimpleFlags flags)
{
	EAPMethod *parent;
	EAPMethodSimple *method;
	GtkWidget *widget;
	NMSetting8021x *s_8021x = NULL;

	parent = eap_method_init (sizeof (EAPMethodSimple),
	                          validate,
	                          add_to_size_group,
	                          fill_connection,
	                          update_secrets,
	                          destroy,
	                          "/org/freedesktop/network-manager-applet/eap-method-simple.ui",
	                          "eap_simple_notebook",
	                          "eap_simple_username_entry",
	                          flags & EAP_METHOD_SIMPLE_FLAG_PHASE2);
	if (!parent)
		return NULL;

	parent->password_flags_name = NM_SETTING_802_1X_PASSWORD;
	method = (EAPMethodSimple *) parent;
	method->ws_parent = ws_parent;
	method->flags = flags;
	method->type = type;
	g_assert (type < EAP_METHOD_SIMPLE_TYPE_LAST);

	widget = GTK_WIDGET (gtk_builder_get_object (parent->builder, "eap_simple_notebook"));
	g_assert (widget);
	g_signal_connect (G_OBJECT (widget), "realize",
	                  (GCallback) widgets_realized,
	                  method);
	g_signal_connect (G_OBJECT (widget), "unrealize",
	                  (GCallback) widgets_unrealized,
	                  method);

	widget = GTK_WIDGET (gtk_builder_get_object (parent->builder, "eap_simple_username_entry"));
	g_assert (widget);
	method->username_entry = GTK_ENTRY (widget);
	g_signal_connect (G_OBJECT (widget), "changed",
	                  (GCallback) wireless_security_changed_cb,
	                  ws_parent);

	if (method->flags & EAP_METHOD_SIMPLE_FLAG_SECRETS_ONLY)
		gtk_widget_set_sensitive (widget, FALSE);

	widget = GTK_WIDGET (gtk_builder_get_object (parent->builder, "eap_simple_password_entry"));
	g_assert (widget);
	method->password_entry = GTK_ENTRY (widget);
	g_signal_connect (G_OBJECT (widget), "changed",
	                  (GCallback) wireless_security_changed_cb,
	                  ws_parent);

	/* Create password-storage popup menu for password entry under entry's secondary icon */
	if (connection)
		s_8021x = nm_connection_get_setting_802_1x (connection);
	nma_utils_setup_password_storage (widget, 0, (NMSetting *) s_8021x, parent->password_flags_name,
	                                  FALSE, flags & EAP_METHOD_SIMPLE_FLAG_SECRETS_ONLY);

	g_signal_connect (method->password_entry, "notify::secondary-icon-name",
	                  G_CALLBACK (password_storage_changed),
	                  method);

	widget = GTK_WIDGET (gtk_builder_get_object (parent->builder, "show_checkbutton_eapsimple"));
	g_assert (widget);
	method->show_password = GTK_TOGGLE_BUTTON (widget);
	g_signal_connect (G_OBJECT (widget), "toggled",
	                  (GCallback) show_toggled_cb,
	                  method);

	/* Initialize the UI fields with the security settings from method->ws_parent.
	 * This will be done again when the widget gets realized. It must be done here as well,
	 * because the outer dialog will ask to 'validate' the connection before the security tab
	 * is shown/realized (to enable the 'Apply' button).
	 * As 'validate' accesses the contents of the UI fields, they must be initialized now, even
	 * if the widgets are not yet visible. */
	set_userpass_ui (method);

	return method;
}

