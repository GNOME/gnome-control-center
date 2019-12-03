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

#include "eap-method.h"
#include "eap-method-simple.h"
#include "helpers.h"
#include "nma-ui-utils.h"
#include "ui-helpers.h"

struct _EAPMethodSimple {
	GtkGrid parent;

	GtkEntry        *password_entry;
	GtkLabel        *password_label;
	GtkToggleButton *show_password_check;
	GtkEntry        *username_entry;
	GtkLabel        *username_label;

	gchar *name;
	gboolean phase2;
	gboolean autheap_allowed;

	guint idle_func_id;
};

static void eap_method_iface_init (EAPMethodInterface *);

G_DEFINE_TYPE_WITH_CODE (EAPMethodSimple, eap_method_simple, GTK_TYPE_GRID,
                         G_IMPLEMENT_INTERFACE (eap_method_get_type (), eap_method_iface_init))

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
validate (EAPMethod *method, GError **error)
{
	EAPMethodSimple *self = EAP_METHOD_SIMPLE (method);
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
add_to_size_group (EAPMethod *method, GtkSizeGroup *group)
{
	EAPMethodSimple *self = EAP_METHOD_SIMPLE (method);
	gtk_size_group_add_widget (group, GTK_WIDGET (self->username_label));
	gtk_size_group_add_widget (group, GTK_WIDGET (self->password_label));
}

static void
fill_connection (EAPMethod *method, NMConnection *connection, NMSettingSecretFlags prev_flags)
{
	EAPMethodSimple *self = EAP_METHOD_SIMPLE (method);
	NMSetting8021x *s_8021x;
	gboolean not_saved = FALSE;
	NMSettingSecretFlags flags;

	s_8021x = nm_connection_get_setting_802_1x (connection);
	g_assert (s_8021x);

	/* If this is the main EAP method, clear any existing methods because the
	 * user-selected on will replace it.
	 */
	if (eap_method_get_phase2 (method) == FALSE)
		nm_setting_802_1x_clear_eap_methods (s_8021x);

	if (eap_method_get_phase2 (method)) {
		/* If the outer EAP method (TLS, TTLS, PEAP, etc) allows inner/phase2
		 * EAP methods (which only TTLS allows) *and* the inner/phase2 method
		 * supports being an inner EAP method, then set PHASE2_AUTHEAP.
		 * Otherwise the inner/phase2 method goes into PHASE2_AUTH.
		 */
		if (self->autheap_allowed) {
			g_object_set (s_8021x, NM_SETTING_802_1X_PHASE2_AUTHEAP, self->name, NULL);
			g_object_set (s_8021x, NM_SETTING_802_1X_PHASE2_AUTH, NULL, NULL);
		} else {
			g_object_set (s_8021x, NM_SETTING_802_1X_PHASE2_AUTH, self->name, NULL);
			g_object_set (s_8021x, NM_SETTING_802_1X_PHASE2_AUTHEAP, NULL, NULL);
		}
	} else
		nm_setting_802_1x_add_eap_method (s_8021x, self->name);

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
	if (not_saved == FALSE)
		g_object_set (s_8021x, NM_SETTING_802_1X_PASSWORD, gtk_entry_get_text (self->password_entry), NULL);

	/* Update secret flags and popup when editing the connection */
	nma_utils_update_password_storage (GTK_WIDGET (self->password_entry), flags,
	                                   NM_SETTING (s_8021x), NM_SETTING_802_1X_PASSWORD);
}

static void
update_secrets (EAPMethod *method, NMConnection *connection)
{
	EAPMethodSimple *self = EAP_METHOD_SIMPLE (method);
	helper_fill_secret_entry (connection,
	                          self->password_entry,
	                          NM_TYPE_SETTING_802_1X,
	                          (HelperSecretFunc) nm_setting_802_1x_get_password);
}

static GtkWidget *
get_default_field (EAPMethod *method)
{
	EAPMethodSimple *self = EAP_METHOD_SIMPLE (method);
	return GTK_WIDGET (self->username_entry);
}

static const gchar *
get_password_flags_name (EAPMethod *method)
{
	return NM_SETTING_802_1X_PASSWORD;
}

static const gboolean
get_phase2 (EAPMethod *method)
{
	EAPMethodSimple *self = EAP_METHOD_SIMPLE (method);
	return self->phase2;
}

static const gchar *
get_username (EAPMethod *method)
{
	EAPMethodSimple *self = EAP_METHOD_SIMPLE (method);
	return gtk_entry_get_text (self->username_entry);
}

static void
set_username (EAPMethod *method, const gchar *username)
{
	EAPMethodSimple *self = EAP_METHOD_SIMPLE (method);
	gtk_entry_set_text (self->username_entry, username);
}

static const gchar *
get_password (EAPMethod *method)
{
	EAPMethodSimple *self = EAP_METHOD_SIMPLE (method);
	return gtk_entry_get_text (self->password_entry);
}

static void
set_password (EAPMethod *method, const gchar *password)
{
	EAPMethodSimple *self = EAP_METHOD_SIMPLE (method);
	gtk_entry_set_text (self->password_entry, password);
}

static gboolean
get_show_password (EAPMethod *method)
{
	EAPMethodSimple *self = EAP_METHOD_SIMPLE (method);
	return gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (self->show_password_check));
}

static void
set_show_password (EAPMethod *method, gboolean show_password)
{
	EAPMethodSimple *self = EAP_METHOD_SIMPLE (method);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (self->show_password_check), show_password);
}

static gboolean
stuff_changed (EAPMethodSimple *self)
{
	eap_method_emit_changed (EAP_METHOD (self));
	self->idle_func_id = 0;
	return FALSE;
}

static void
password_storage_changed (EAPMethodSimple *self)
{
	gboolean always_ask;

	always_ask = always_ask_selected (self->password_entry);

	if (always_ask) {
		/* we always clear this button and do not restore it
		 * (because we want to hide the password). */
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (self->show_password_check), FALSE);
	}

	gtk_widget_set_sensitive (GTK_WIDGET (self->show_password_check), !always_ask);

	if (!self->idle_func_id)
		self->idle_func_id = g_idle_add ((GSourceFunc) stuff_changed, self);
}

static void
eap_method_simple_dispose (GObject *object)
{
	EAPMethodSimple *self = EAP_METHOD_SIMPLE (object);

	g_clear_pointer (&self->name, g_free);

	g_signal_handlers_disconnect_by_data (self, self);
	g_signal_handlers_disconnect_by_data (self->password_entry, self);
	g_signal_handlers_disconnect_by_data (self->show_password_check, self);

	if (self->idle_func_id != 0) {
		g_source_remove (self->idle_func_id);
		self->idle_func_id = 0;
	}

	G_OBJECT_CLASS (eap_method_simple_parent_class)->dispose (object);
}

static void
changed_cb (EAPMethodSimple *self)
{
	eap_method_emit_changed (EAP_METHOD (self));
}

static void
eap_method_simple_init (EAPMethodSimple *self)
{
	gtk_widget_init_template (GTK_WIDGET (self));
}

static void
eap_method_simple_class_init (EAPMethodSimpleClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);
        GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

	object_class->dispose = eap_method_simple_dispose;

	gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/ControlCenter/network/eap-method-simple.ui");

	gtk_widget_class_bind_template_child (widget_class, EAPMethodSimple, password_label);
	gtk_widget_class_bind_template_child (widget_class, EAPMethodSimple, username_label);
	gtk_widget_class_bind_template_child (widget_class, EAPMethodSimple, password_entry);
	gtk_widget_class_bind_template_child (widget_class, EAPMethodSimple, show_password_check);
	gtk_widget_class_bind_template_child (widget_class, EAPMethodSimple, username_entry);
}

static void
eap_method_iface_init (EAPMethodInterface *iface)
{
	iface->validate = validate;
	iface->add_to_size_group = add_to_size_group;
	iface->fill_connection = fill_connection;
	iface->update_secrets = update_secrets;
	iface->get_default_field = get_default_field;
	iface->get_password_flags_name = get_password_flags_name;
	iface->get_phase2 = get_phase2;
	iface->get_username = get_username;
	iface->set_username = set_username;
	iface->get_password = get_password;
	iface->set_password = set_password;
	iface->get_show_password = get_show_password;
	iface->set_show_password = set_show_password;
}

EAPMethodSimple *
eap_method_simple_new (NMConnection *connection, const gchar *name, gboolean phase2, gboolean autheap_allowed)
{
	EAPMethodSimple *self;
	NMSetting8021x *s_8021x = NULL;

	self = g_object_new (eap_method_simple_get_type (), NULL);
	self->name = g_strdup (name);
	self->phase2 = phase2;
	self->autheap_allowed = autheap_allowed;

	g_signal_connect_swapped (self->username_entry, "changed", G_CALLBACK (changed_cb), self);

	g_signal_connect_swapped (self->password_entry, "changed", G_CALLBACK (changed_cb), self);

	/* Create password-storage popup menu for password entry under entry's secondary icon */
	if (connection)
		s_8021x = nm_connection_get_setting_802_1x (connection);
	nma_utils_setup_password_storage (GTK_WIDGET (self->password_entry), 0, (NMSetting *) s_8021x, NM_SETTING_802_1X_PASSWORD,
	                                  FALSE, FALSE);

	g_signal_connect_swapped (self->password_entry, "notify::secondary-icon-name", G_CALLBACK (password_storage_changed), self);

	g_signal_connect_swapped (self->show_password_check, "toggled", G_CALLBACK (show_toggled_cb), self);

	return self;
}

