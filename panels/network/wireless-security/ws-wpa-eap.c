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

#include "ws-wpa-eap.h"
#include "wireless-security.h"
#include "eap-method.h"
#include "eap-method-fast.h"
#include "eap-method-leap.h"
#include "eap-method-peap.h"
#include "eap-method-simple.h"
#include "eap-method-tls.h"
#include "eap-method-ttls.h"

struct _WirelessSecurityWPAEAP {
	GtkGrid parent;

	GtkComboBox  *auth_combo;
	GtkLabel     *auth_label;
	GtkListStore *auth_model;
	GtkBox       *method_box;

	EAPMethodSimple *em_md5;
	EAPMethodTLS    *em_tls;
	EAPMethodLEAP   *em_leap;
	EAPMethodSimple *em_pwd;
	EAPMethodFAST   *em_fast;
	EAPMethodTTLS   *em_ttls;
	EAPMethodPEAP   *em_peap;
};

static void wireless_security_iface_init (WirelessSecurityInterface *);

G_DEFINE_TYPE_WITH_CODE (WirelessSecurityWPAEAP, ws_wpa_eap, GTK_TYPE_GRID,
                         G_IMPLEMENT_INTERFACE (wireless_security_get_type (), wireless_security_iface_init));

#define AUTH_NAME_COLUMN    0
#define AUTH_ID_COLUMN      1

static EAPMethod *
get_eap (WirelessSecurityWPAEAP *self)
{
	GtkTreeIter iter;
	g_autofree gchar *id = NULL;

	if (!gtk_combo_box_get_active_iter (self->auth_combo, &iter))
		return NULL;
	gtk_tree_model_get (GTK_TREE_MODEL (self->auth_model), &iter, AUTH_ID_COLUMN, &id, -1);

	if (strcmp (id, "md5") == 0)
		return EAP_METHOD (self->em_md5);
	if (strcmp (id, "tls") == 0)
		return EAP_METHOD (self->em_tls);
	if (strcmp (id, "leap") == 0)
		return EAP_METHOD (self->em_leap);
	if (strcmp (id, "pwd") == 0)
		return EAP_METHOD (self->em_pwd);
	if (strcmp (id, "fast") == 0)
		return EAP_METHOD (self->em_fast);
	if (strcmp (id, "ttls") == 0)
		return EAP_METHOD (self->em_ttls);
	if (strcmp (id, "peap") == 0)
		return EAP_METHOD (self->em_peap);

	return NULL;
}

static gboolean
validate (WirelessSecurity *security, GError **error)
{
	WirelessSecurityWPAEAP *self = WS_WPA_EAP (security);
	return eap_method_validate (get_eap (self), error);
}

static void
add_to_size_group (WirelessSecurity *security, GtkSizeGroup *group)
{
	WirelessSecurityWPAEAP *self = WS_WPA_EAP (security);

	gtk_size_group_add_widget (group, GTK_WIDGET (self->auth_label));
	eap_method_add_to_size_group (EAP_METHOD (self->em_md5), group);
	eap_method_add_to_size_group (EAP_METHOD (self->em_tls), group);
	eap_method_add_to_size_group (EAP_METHOD (self->em_leap), group);
	eap_method_add_to_size_group (EAP_METHOD (self->em_pwd), group);
	eap_method_add_to_size_group (EAP_METHOD (self->em_fast), group);
	eap_method_add_to_size_group (EAP_METHOD (self->em_ttls), group);
	eap_method_add_to_size_group (EAP_METHOD (self->em_peap), group);
}

static void
ws_802_1x_fill_connection (WirelessSecurityWPAEAP *self, NMConnection *connection)
{
	NMSettingWirelessSecurity *s_wireless_sec;
	NMSetting8021x *s_8021x;
	NMSettingSecretFlags secret_flags = NM_SETTING_SECRET_FLAG_NONE;
	EAPMethod *eap;

	/* Get the EAPMethod object */
	eap = get_eap (self);

	/* Get previous pasword flags, if any. Otherwise default to agent-owned secrets */
	s_8021x = nm_connection_get_setting_802_1x (connection);
	if (s_8021x)
		nm_setting_get_secret_flags (NM_SETTING (s_8021x), eap_method_get_password_flags_name (eap), &secret_flags, NULL);
	else
		secret_flags = NM_SETTING_SECRET_FLAG_AGENT_OWNED;

	/* Blow away the old wireless security setting by adding a clear one */
	s_wireless_sec = (NMSettingWirelessSecurity *) nm_setting_wireless_security_new ();
	nm_connection_add_setting (connection, (NMSetting *) s_wireless_sec);

	/* Blow away the old 802.1x setting by adding a clear one */
	s_8021x = (NMSetting8021x *) nm_setting_802_1x_new ();
	nm_connection_add_setting (connection, (NMSetting *) s_8021x);

	eap_method_fill_connection (eap, connection, secret_flags);
}

static void
fill_connection (WirelessSecurity *security, NMConnection *connection)
{
	WirelessSecurityWPAEAP *self = WS_WPA_EAP (security);
	NMSettingWirelessSecurity *s_wireless_sec;

	ws_802_1x_fill_connection (self, connection);

	s_wireless_sec = nm_connection_get_setting_wireless_security (connection);
	g_assert (s_wireless_sec);

	g_object_set (s_wireless_sec, NM_SETTING_WIRELESS_SECURITY_KEY_MGMT, "wpa-eap", NULL);
}

static gboolean
adhoc_compatible (WirelessSecurity *security)
{
	return FALSE;
}

static void
auth_combo_changed_cb (WirelessSecurityWPAEAP *self)
{
	EAPMethod *eap;
	GList *children;
	GtkWidget *eap_default_field;

	eap = get_eap (self);

	/* Remove the previous method and migrate username/password across */
	children = gtk_container_get_children (GTK_CONTAINER (self->method_box));
	if (children != NULL) {
		EAPMethod *old_eap = g_list_nth_data (children, 0);
		eap_method_set_username (eap, eap_method_get_username (old_eap));
		eap_method_set_password (eap, eap_method_get_password (old_eap));
		eap_method_set_show_password (eap, eap_method_get_show_password (old_eap));
		gtk_container_remove (GTK_CONTAINER (self->method_box), GTK_WIDGET (old_eap));
	}

	gtk_container_add (GTK_CONTAINER (self->method_box), g_object_ref (GTK_WIDGET (eap)));
	eap_default_field = eap_method_get_default_field (eap);
	if (eap_default_field)
		gtk_widget_grab_focus (eap_default_field);

	wireless_security_notify_changed (WIRELESS_SECURITY (self));
}

void
ws_wpa_eap_init (WirelessSecurityWPAEAP *self)
{
	gtk_widget_init_template (GTK_WIDGET (self));
}

void
ws_wpa_eap_class_init (WirelessSecurityWPAEAPClass *klass)
{
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

	gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/ControlCenter/network/ws-wpa-eap.ui");

	gtk_widget_class_bind_template_child (widget_class, WirelessSecurityWPAEAP, auth_combo);
	gtk_widget_class_bind_template_child (widget_class, WirelessSecurityWPAEAP, auth_label);
	gtk_widget_class_bind_template_child (widget_class, WirelessSecurityWPAEAP, auth_model);
	gtk_widget_class_bind_template_child (widget_class, WirelessSecurityWPAEAP, method_box);
}

static void
wireless_security_iface_init (WirelessSecurityInterface *iface)
{
	iface->validate = validate;
	iface->add_to_size_group = add_to_size_group;
	iface->fill_connection = fill_connection;
	iface->adhoc_compatible = adhoc_compatible;
}

WirelessSecurityWPAEAP *
ws_wpa_eap_new (NMConnection *connection)
{
	WirelessSecurityWPAEAP *self;
	const gchar *remove_method, *default_method = NULL;
	gboolean wired = FALSE;
	GtkTreeIter iter;

	self = g_object_new (ws_wpa_eap_get_type (), NULL);

	/* Grab the default EAP method out of the security object */
	if (connection) {
		NMSettingConnection *s_con;
		NMSetting8021x *s_8021x;
		const char *ctype = NULL;

		s_con = nm_connection_get_setting_connection (connection);
		if (s_con)
			ctype = nm_setting_connection_get_connection_type (s_con);
		if ((g_strcmp0 (ctype, NM_SETTING_WIRED_SETTING_NAME) == 0)
		    || nm_connection_get_setting_wired (connection))
			wired = TRUE;

		s_8021x = nm_connection_get_setting_802_1x (connection);
		if (s_8021x && nm_setting_802_1x_get_num_eap_methods (s_8021x))
			default_method = nm_setting_802_1x_get_eap_method (s_8021x, 0);
	}
	if (wired)
		remove_method = "leap";
	else
		remove_method = "md5";
	if (default_method == NULL) {
		if (wired)
			default_method = "md5";
		else
			default_method = "tls";
	}

	self->em_md5 = eap_method_simple_new (connection, "md5", FALSE, FALSE);
	gtk_widget_show (GTK_WIDGET (self->em_md5));
	g_signal_connect_object (self->em_md5, "changed", G_CALLBACK (wireless_security_notify_changed), self, G_CONNECT_SWAPPED);
	self->em_tls = eap_method_tls_new (connection);
	gtk_widget_show (GTK_WIDGET (self->em_tls));
	g_signal_connect_object (self->em_tls, "changed", G_CALLBACK (wireless_security_notify_changed), self, G_CONNECT_SWAPPED);
	self->em_leap = eap_method_leap_new (connection);
	gtk_widget_show (GTK_WIDGET (self->em_leap));
	g_signal_connect_object (self->em_leap, "changed", G_CALLBACK (wireless_security_notify_changed), self, G_CONNECT_SWAPPED);
	self->em_pwd = eap_method_simple_new (connection, "pwd", FALSE, FALSE);
	gtk_widget_show (GTK_WIDGET (self->em_pwd));
	g_signal_connect_object (self->em_pwd, "changed", G_CALLBACK (wireless_security_notify_changed), self, G_CONNECT_SWAPPED);
	self->em_fast = eap_method_fast_new (connection);
	gtk_widget_show (GTK_WIDGET (self->em_fast));
	g_signal_connect_object (self->em_fast, "changed", G_CALLBACK (wireless_security_notify_changed), self, G_CONNECT_SWAPPED);
	self->em_ttls = eap_method_ttls_new (connection);
	gtk_widget_show (GTK_WIDGET (self->em_ttls));
	g_signal_connect_object (self->em_ttls, "changed", G_CALLBACK (wireless_security_notify_changed), self, G_CONNECT_SWAPPED);
	self->em_peap = eap_method_peap_new (connection);
	gtk_widget_show (GTK_WIDGET (self->em_peap));
	g_signal_connect_object (self->em_peap, "changed", G_CALLBACK (wireless_security_notify_changed), self, G_CONNECT_SWAPPED);

	if (gtk_tree_model_get_iter_first (GTK_TREE_MODEL (self->auth_model), &iter)) {
		do {
			g_autofree gchar *id = NULL;
			gtk_tree_model_get (GTK_TREE_MODEL (self->auth_model), &iter, AUTH_ID_COLUMN, &id, -1);
			if (strcmp (id, remove_method) == 0) {
				gtk_list_store_remove (self->auth_model, &iter);
				break;
			}
		} while (gtk_tree_model_iter_next (GTK_TREE_MODEL (self->auth_model), &iter));
	}  
	if (gtk_tree_model_get_iter_first (GTK_TREE_MODEL (self->auth_model), &iter)) {
		do {
			g_autofree gchar *id = NULL;
			gtk_tree_model_get (GTK_TREE_MODEL (self->auth_model), &iter, AUTH_ID_COLUMN, &id, -1);
			if (strcmp (id, default_method) == 0)
				gtk_combo_box_set_active_iter (self->auth_combo, &iter);
		} while (gtk_tree_model_iter_next (GTK_TREE_MODEL (self->auth_model), &iter));
	}

	if (connection) {
		NMSetting8021x *setting;

		setting = nm_connection_get_setting_802_1x (connection);
		if (setting) {
			eap_method_set_username (get_eap (self), nm_setting_802_1x_get_identity (setting));
			eap_method_set_password (get_eap (self), nm_setting_802_1x_get_password (setting));
		}
	}

	g_signal_connect_object (G_OBJECT (self->auth_combo), "changed", G_CALLBACK (auth_combo_changed_cb), self, G_CONNECT_SWAPPED);
	auth_combo_changed_cb (self);

	return self;
}

void
ws_wpa_eap_fill_connection (WirelessSecurityWPAEAP *self, NMConnection *connection)
{
        ws_802_1x_fill_connection (self, connection);
}
