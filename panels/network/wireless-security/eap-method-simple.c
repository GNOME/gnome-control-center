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

	GtkGrid         *grid;
	GtkEntry        *password_entry;
	GtkLabel        *password_label;
	GtkToggleButton *show_password_check;
	GtkEntry        *username_entry;
	GtkLabel        *username_label;

	WirelessSecurity *ws_parent;

	EAPMethodSimpleType type;
	EAPMethodSimpleFlags flags;

	guint idle_func_id;
};

static void
show_toggled_cb (EAPMethodSimple *self)
{
	gboolean visible;

	visible = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (self->show_password_check));
	gtk_entry_set_visibility (self->password_entry, visible);
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
	EAPMethodSimple *self = (EAPMethodSimple *)parent;
	const char *text;
	gboolean ret = TRUE;

	text = gtk_entry_get_text (self->username_entry);
	if (!text || !strlen (text)) {
		widget_set_error (GTK_WIDGET (self->username_entry));
		g_set_error_literal (error, NMA_ERROR, NMA_ERROR_GENERIC, _("missing EAP username"));
		ret = FALSE;
	} else
		widget_unset_error (GTK_WIDGET (self->username_entry));

	/* Check if the password should always be requested */
	if (always_ask_selected (self->password_entry))
		widget_unset_error (GTK_WIDGET (self->password_entry));
	else {
		text = gtk_entry_get_text (self->password_entry);
		if (!text || !strlen (text)) {
			widget_set_error (GTK_WIDGET (self->password_entry));
			if (ret) {
				g_set_error_literal (error, NMA_ERROR, NMA_ERROR_GENERIC, _("missing EAP password"));
				ret = FALSE;
			}
		} else
			widget_unset_error (GTK_WIDGET (self->password_entry));
	}

	return ret;
}

static void
add_to_size_group (EAPMethod *parent, GtkSizeGroup *group)
{
	EAPMethodSimple *self = (EAPMethodSimple *) parent;
	gtk_size_group_add_widget (group, GTK_WIDGET (self->username_label));
	gtk_size_group_add_widget (group, GTK_WIDGET (self->password_label));
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
	EAPMethodSimple *self = (EAPMethodSimple *) parent;
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

	eap_type = &eap_table[self->type];
	if (parent->phase2) {
		/* If the outer EAP method (TLS, TTLS, PEAP, etc) allows inner/phase2
		 * EAP methods (which only TTLS allows) *and* the inner/phase2 method
		 * supports being an inner EAP method, then set PHASE2_AUTHEAP.
		 * Otherwise the inner/phase2 method goes into PHASE2_AUTH.
		 */
		if ((self->flags & EAP_METHOD_SIMPLE_FLAG_AUTHEAP_ALLOWED) && eap_type->autheap_allowed) {
			g_object_set (s_8021x, NM_SETTING_802_1X_PHASE2_AUTHEAP, eap_type->name, NULL);
			g_object_set (s_8021x, NM_SETTING_802_1X_PHASE2_AUTH, NULL, NULL);
		} else {
			g_object_set (s_8021x, NM_SETTING_802_1X_PHASE2_AUTH, eap_type->name, NULL);
			g_object_set (s_8021x, NM_SETTING_802_1X_PHASE2_AUTHEAP, NULL, NULL);
		}
	} else
		nm_setting_802_1x_add_eap_method (s_8021x, eap_type->name);

	g_object_set (s_8021x, NM_SETTING_802_1X_IDENTITY, gtk_entry_get_text (self->username_entry), NULL);

	/* Save the password always ask setting */
	not_saved = always_ask_selected (self->password_entry);
	flags = nma_utils_menu_to_secret_flags (GTK_WIDGET (self->password_entry));
	nm_setting_set_secret_flags (NM_SETTING (s_8021x), NM_SETTING_802_1X_PASSWORD, flags, NULL);

	/* Fill the connection's password if we're in the applet so that it'll get
	 * back to NM.  From the editor though, since the connection isn't going
	 * back to NM in response to a GetSecrets() call, we don't save it if the
	 * user checked "Always Ask".
	 */
	if (!(self->flags & EAP_METHOD_SIMPLE_FLAG_IS_EDITOR) || not_saved == FALSE)
		g_object_set (s_8021x, NM_SETTING_802_1X_PASSWORD, gtk_entry_get_text (self->password_entry), NULL);

	/* Update secret flags and popup when editing the connection */
	if (!(self->flags & EAP_METHOD_SIMPLE_FLAG_SECRETS_ONLY)) {
		nma_utils_update_password_storage (GTK_WIDGET (self->password_entry), flags,
		                                   NM_SETTING (s_8021x), parent->password_flags_name);
	}
}

static void
update_secrets (EAPMethod *parent, NMConnection *connection)
{
	EAPMethodSimple *self = (EAPMethodSimple *) parent;
	helper_fill_secret_entry (connection,
	                          self->password_entry,
	                          NM_TYPE_SETTING_802_1X,
	                          (HelperSecretFunc) nm_setting_802_1x_get_password);
}

static gboolean
stuff_changed (EAPMethodSimple *self)
{
	wireless_security_notify_changed (self->ws_parent);
	self->idle_func_id = 0;
	return FALSE;
}

static void
password_storage_changed (EAPMethodSimple *self)
{
	gboolean always_ask;
	gboolean secrets_only = self->flags & EAP_METHOD_SIMPLE_FLAG_SECRETS_ONLY;

	always_ask = always_ask_selected (self->password_entry);

	if (always_ask && !secrets_only) {
		/* we always clear this button and do not restore it
		 * (because we want to hide the password). */
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (self->show_password_check), FALSE);
	}

	gtk_widget_set_sensitive (GTK_WIDGET (self->show_password_check),
	                          !always_ask || secrets_only);

	if (!self->idle_func_id)
		self->idle_func_id = g_idle_add ((GSourceFunc) stuff_changed, self);
}

/* Set the UI fields for user, password, always_ask and show_password to the
 * values as provided by self->ws_parent. */
static void
set_userpass_ui (EAPMethodSimple *self)
{
	if (wireless_security_get_username (self->ws_parent))
		gtk_entry_set_text (self->username_entry, wireless_security_get_username (self->ws_parent));
	else
		gtk_entry_set_text (self->username_entry, "");

	if (wireless_security_get_password (self->ws_parent) && !wireless_security_get_always_ask (self->ws_parent))
		gtk_entry_set_text (self->password_entry, wireless_security_get_password (self->ws_parent));
	else
		gtk_entry_set_text (self->password_entry, "");

	gtk_toggle_button_set_active (self->show_password_check, wireless_security_get_show_password (self->ws_parent));
}

static void
widgets_realized (EAPMethodSimple *self)
{
	set_userpass_ui (self);
}

static void
widgets_unrealized (EAPMethodSimple *self)
{
	wireless_security_set_userpass (self->ws_parent,
	                                gtk_entry_get_text (self->username_entry),
	                                gtk_entry_get_text (self->password_entry),
	                                always_ask_selected (self->password_entry),
	                                gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (self->show_password_check)));
}

static void
destroy (EAPMethod *parent)
{
	EAPMethodSimple *self = (EAPMethodSimple *) parent;

	g_signal_handlers_disconnect_by_data (self->grid, self);
	g_signal_handlers_disconnect_by_data (self->username_entry, self->ws_parent);
	g_signal_handlers_disconnect_by_data (self->password_entry, self->ws_parent);
	g_signal_handlers_disconnect_by_data (self->password_entry, self);
	g_signal_handlers_disconnect_by_data (self->show_password_check, self);

	nm_clear_g_source (&self->idle_func_id);
}

static void
changed_cb (EAPMethodSimple *self)
{
	wireless_security_notify_changed (self->ws_parent);
}

EAPMethodSimple *
eap_method_simple_new (WirelessSecurity *ws_parent,
                       NMConnection *connection,
                       EAPMethodSimpleType type,
                       EAPMethodSimpleFlags flags)
{
	EAPMethod *parent;
	EAPMethodSimple *self;
	NMSetting8021x *s_8021x = NULL;

	parent = eap_method_init (sizeof (EAPMethodSimple),
	                          validate,
	                          add_to_size_group,
	                          fill_connection,
	                          update_secrets,
	                          destroy,
	                          "/org/gnome/ControlCenter/network/eap-method-simple.ui",
	                          "grid",
	                          "username_entry",
	                          flags & EAP_METHOD_SIMPLE_FLAG_PHASE2);
	if (!parent)
		return NULL;

	parent->password_flags_name = NM_SETTING_802_1X_PASSWORD;
	self = (EAPMethodSimple *) parent;
	self->ws_parent = ws_parent;
	self->flags = flags;
	self->type = type;
	g_assert (type < EAP_METHOD_SIMPLE_TYPE_LAST);

	self->grid = GTK_GRID (gtk_builder_get_object (parent->builder, "grid"));
	self->password_label = GTK_LABEL (gtk_builder_get_object (parent->builder, "password_label"));
	self->username_label = GTK_LABEL (gtk_builder_get_object (parent->builder, "username_label"));
	self->password_entry = GTK_ENTRY (gtk_builder_get_object (parent->builder, "password_entry"));
	self->show_password_check = GTK_TOGGLE_BUTTON (gtk_builder_get_object (parent->builder, "show_password_check"));
	self->username_entry = GTK_ENTRY (gtk_builder_get_object (parent->builder, "username_entry"));

	g_signal_connect_swapped (self->grid, "realize", G_CALLBACK (widgets_realized), self);
	g_signal_connect_swapped (self->grid, "unrealize", G_CALLBACK (widgets_unrealized), self);

	g_signal_connect_swapped (self->username_entry, "changed", G_CALLBACK (changed_cb), self);

	if (self->flags & EAP_METHOD_SIMPLE_FLAG_SECRETS_ONLY)
		gtk_widget_set_sensitive (GTK_WIDGET (self->username_entry), FALSE);

	g_signal_connect_swapped (self->password_entry, "changed", G_CALLBACK (changed_cb), self);

	/* Create password-storage popup menu for password entry under entry's secondary icon */
	if (connection)
		s_8021x = nm_connection_get_setting_802_1x (connection);
	nma_utils_setup_password_storage (GTK_WIDGET (self->password_entry), 0, (NMSetting *) s_8021x, parent->password_flags_name,
	                                  FALSE, flags & EAP_METHOD_SIMPLE_FLAG_SECRETS_ONLY);

	g_signal_connect_swapped (self->password_entry, "notify::secondary-icon-name", G_CALLBACK (password_storage_changed), self);

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

