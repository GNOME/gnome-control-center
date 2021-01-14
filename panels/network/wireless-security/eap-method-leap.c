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
#include "eap-method-leap.h"
#include "helpers.h"
#include "nma-ui-utils.h"
#include "ui-helpers.h"

struct _EAPMethodLEAP {
	GtkGrid parent;

	GtkEntry       *password_entry;
	GtkLabel       *password_label;
	GtkCheckButton *show_password_check;
	GtkEntry       *username_entry;
	GtkLabel       *username_label;
};

static void eap_method_iface_init (EAPMethodInterface *);

G_DEFINE_TYPE_WITH_CODE (EAPMethodLEAP, eap_method_leap, GTK_TYPE_GRID,
                         G_IMPLEMENT_INTERFACE (eap_method_get_type (), eap_method_iface_init))

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
	NMSettingSecretFlags secret_flags;
	const char *text;
	gboolean ret = TRUE;

	text = gtk_entry_get_text (self->username_entry);
	if (!text || !strlen (text)) {
		widget_set_error (GTK_WIDGET (self->username_entry));
		g_set_error_literal (error, NMA_ERROR, NMA_ERROR_GENERIC, _("missing EAP-LEAP username"));
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
	nm_setting_set_secret_flags (NM_SETTING (s_8021x), NM_SETTING_802_1X_PASSWORD,
	                             secret_flags, NULL);

	/* Update secret flags and popup when editing the connection */
	nma_utils_update_password_storage (GTK_WIDGET (self->password_entry), secret_flags,
	                                   NM_SETTING (s_8021x), NM_SETTING_802_1X_PASSWORD);
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

static GtkWidget *
get_default_field (EAPMethod *parent)
{
	EAPMethodLEAP *self = (EAPMethodLEAP *) parent;
	return GTK_WIDGET (self->username_entry);
}

static const gchar *
get_password_flags_name (EAPMethod *parent)
{
	return NM_SETTING_802_1X_PASSWORD;
}

static const gchar *
get_username (EAPMethod *method)
{
	EAPMethodLEAP *self = EAP_METHOD_LEAP (method);
	return gtk_entry_get_text (self->username_entry);
}

static void
set_username (EAPMethod *method, const gchar *username)
{
	EAPMethodLEAP *self = EAP_METHOD_LEAP (method);
	gtk_entry_set_text (self->username_entry, username);
}

static const gchar *
get_password (EAPMethod *method)
{
	EAPMethodLEAP *self = EAP_METHOD_LEAP (method);
	return gtk_entry_get_text (self->password_entry);
}

static void
set_password (EAPMethod *method, const gchar *password)
{
	EAPMethodLEAP *self = EAP_METHOD_LEAP (method);
	gtk_entry_set_text (self->password_entry, password);
}

static gboolean
get_show_password (EAPMethod *method)
{
	EAPMethodLEAP *self = EAP_METHOD_LEAP (method);
	return gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (self->show_password_check));
}

static void
set_show_password (EAPMethod *method, gboolean show_password)
{
	EAPMethodLEAP *self = EAP_METHOD_LEAP (method);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (self->show_password_check), show_password);
}

static void
eap_method_leap_dispose (GObject *object)
{
	EAPMethodLEAP *self = EAP_METHOD_LEAP (object);

	g_signal_handlers_disconnect_by_data (self, self);
	g_signal_handlers_disconnect_by_data (self->show_password_check, self);

	G_OBJECT_CLASS (eap_method_leap_parent_class)->dispose (object);
}

static void
changed_cb (EAPMethodLEAP *self)
{
	eap_method_emit_changed (EAP_METHOD (self));
}

static void
eap_method_leap_init (EAPMethodLEAP *self)
{
	gtk_widget_init_template (GTK_WIDGET (self));
}

static void
eap_method_leap_class_init (EAPMethodLEAPClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);
        GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

	object_class->dispose = eap_method_leap_dispose;

	gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/ControlCenter/network/eap-method-leap.ui");

	gtk_widget_class_bind_template_child (widget_class, EAPMethodLEAP, password_entry);
	gtk_widget_class_bind_template_child (widget_class, EAPMethodLEAP, password_label);
	gtk_widget_class_bind_template_child (widget_class, EAPMethodLEAP, show_password_check);
	gtk_widget_class_bind_template_child (widget_class, EAPMethodLEAP, username_entry);
	gtk_widget_class_bind_template_child (widget_class, EAPMethodLEAP, username_label);
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
	iface->get_username = get_username;
	iface->set_username = set_username;
	iface->get_password = get_password;
	iface->set_password = set_password;
	iface->get_show_password = get_show_password;
	iface->set_show_password = set_show_password;
}

EAPMethodLEAP *
eap_method_leap_new (NMConnection *connection)
{
	EAPMethodLEAP *self;
	NMSetting8021x *s_8021x = NULL;

	self = g_object_new (eap_method_leap_get_type (), NULL);

	g_signal_connect_swapped (self->username_entry, "changed", G_CALLBACK (changed_cb), self);

	g_signal_connect_swapped (self->password_entry, "changed", G_CALLBACK (changed_cb), self);

	/* Create password-storage popup menu for password entry under entry's secondary icon */
	if (connection)
		s_8021x = nm_connection_get_setting_802_1x (connection);
	nma_utils_setup_password_storage (GTK_WIDGET (self->password_entry), 0, (NMSetting *) s_8021x, NM_SETTING_802_1X_PASSWORD,
	                                  FALSE, FALSE);

	g_signal_connect_swapped (self->show_password_check, "toggled", G_CALLBACK (show_toggled_cb), self);

	return self;
}

