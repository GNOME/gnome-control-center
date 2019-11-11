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
#include "eap-method-fast.h"
#include "eap-method-leap.h"
#include "eap-method-peap.h"
#include "eap-method-simple.h"
#include "eap-method-tls.h"
#include "eap-method-ttls.h"
#include "wireless-security.h"
#include "ws-dynamic-wep.h"

struct _WirelessSecurityDynamicWEP {
	WirelessSecurity parent;

	GtkBuilder   *builder;
	GtkComboBox  *auth_combo;
	GtkLabel     *auth_label;
	GtkListStore *auth_model;
	GtkGrid      *grid;
	GtkBox       *method_box;

	EAPMethodSimple *em_md5;
	EAPMethodTLS    *em_tls;
	EAPMethodLEAP   *em_leap;
	EAPMethodSimple *em_pwd;
	EAPMethodFAST   *em_fast;
	EAPMethodTTLS   *em_ttls;
	EAPMethodPEAP   *em_peap;
};

G_DEFINE_TYPE (WirelessSecurityDynamicWEP, ws_dynamic_wep, wireless_security_get_type ())

#define AUTH_NAME_COLUMN    0
#define AUTH_ID_COLUMN      1

static EAPMethod *
get_eap (WirelessSecurityDynamicWEP *self)
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

static void
ws_dynamic_wep_dispose (GObject *object)
{
	WirelessSecurityDynamicWEP *self = WS_DYNAMIC_WEP (object);

	g_clear_object (&self->builder);

	G_OBJECT_CLASS (ws_dynamic_wep_parent_class)->dispose (object);
}

static GtkWidget *
get_widget (WirelessSecurity *security)
{
	WirelessSecurityDynamicWEP *self = WS_DYNAMIC_WEP (security);
	return GTK_WIDGET (self->grid);
}

static gboolean
validate (WirelessSecurity *security, GError **error)
{
	WirelessSecurityDynamicWEP *self = WS_DYNAMIC_WEP (security);
	return eap_method_validate (get_eap (self), error);
}

static void
add_to_size_group (WirelessSecurity *security, GtkSizeGroup *group)
{
	WirelessSecurityDynamicWEP *self = WS_DYNAMIC_WEP (security);

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
fill_connection (WirelessSecurity *security, NMConnection *connection)
{
	WirelessSecurityDynamicWEP *self = WS_DYNAMIC_WEP (security);
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

	g_object_set (s_wireless_sec, NM_SETTING_WIRELESS_SECURITY_KEY_MGMT, "ieee8021x", NULL);
}

static gboolean
adhoc_compatible (WirelessSecurity *security)
{
	return FALSE;
}

static void
auth_combo_changed_cb (WirelessSecurityDynamicWEP *self)
{
	EAPMethod *eap;
	GList *elt, *children;
	GtkWidget *eap_default_field;

	/* Remove any previous wireless security widgets */
	children = gtk_container_get_children (GTK_CONTAINER (self->method_box));
	for (elt = children; elt; elt = g_list_next (elt))
		gtk_container_remove (GTK_CONTAINER (self->method_box), GTK_WIDGET (elt->data));

	eap = get_eap (self);
	gtk_container_add (GTK_CONTAINER (self->method_box), g_object_ref (GTK_WIDGET (eap)));

	/* Refocus the EAP method's default widget */
	eap_default_field = eap_method_get_default_field (eap);
	if (eap_default_field)
		gtk_widget_grab_focus (eap_default_field);

	wireless_security_notify_changed (WIRELESS_SECURITY (self));
}

void
ws_dynamic_wep_init (WirelessSecurityDynamicWEP *self)
{
}

void
ws_dynamic_wep_class_init (WirelessSecurityDynamicWEPClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	WirelessSecurityClass *ws_class = WIRELESS_SECURITY_CLASS (klass);

	object_class->dispose = ws_dynamic_wep_dispose;
	ws_class->get_widget = get_widget;
	ws_class->validate = validate;
	ws_class->add_to_size_group = add_to_size_group;
	ws_class->fill_connection = fill_connection;
	ws_class->adhoc_compatible = adhoc_compatible;
}

WirelessSecurityDynamicWEP *
ws_dynamic_wep_new (NMConnection *connection,
                    gboolean is_editor,
                    gboolean secrets_only)
{
	WirelessSecurityDynamicWEP *self;
	const gchar *user = NULL, *password = NULL;
	gboolean always_ask = FALSE;
	const gchar *remove_method, *default_method = NULL;
	gboolean wired = FALSE;
	EAPMethodSimpleFlags simple_flags = EAP_METHOD_SIMPLE_FLAG_NONE;
	GtkTreeIter iter;
	g_autoptr(GError) error = NULL;

	self = g_object_new (ws_dynamic_wep_get_type (), NULL);

	self->builder = gtk_builder_new ();
	if (!gtk_builder_add_from_resource (self->builder, "/org/gnome/ControlCenter/network/ws-dynamic-wep.ui", &error)) {
		g_warning ("Couldn't load UI builder resource: %s", error->message);
		return NULL;
	}

	self->auth_combo = GTK_COMBO_BOX (gtk_builder_get_object (self->builder, "auth_combo"));
	self->auth_label = GTK_LABEL (gtk_builder_get_object (self->builder, "auth_label"));
	self->auth_model = GTK_LIST_STORE (gtk_builder_get_object (self->builder, "auth_model"));
	self->grid = GTK_GRID (gtk_builder_get_object (self->builder, "grid"));
	self->method_box = GTK_BOX (gtk_builder_get_object (self->builder, "method_box"));

	/* Grab the default EAP method out of the security object */
	if (connection) {
		NMSettingConnection *s_con;
		NMSetting8021x *s_8021x;
		const gchar *ctype = NULL;

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

	/* initialize WirelessSecurity userpass from connection (clear if no connection) */
	if (connection) {
		NMSetting8021x *setting;

		setting = nm_connection_get_setting_802_1x (connection);
		if (setting) {
			NMSettingSecretFlags flags;

			user = nm_setting_802_1x_get_identity (setting);
			password = nm_setting_802_1x_get_password (setting);

			if (nm_setting_get_secret_flags (NM_SETTING (setting), NM_SETTING_802_1X_PASSWORD, &flags, NULL))
				always_ask = !!(flags & NM_SETTING_SECRET_FLAG_NOT_SAVED);
		}
	}
	wireless_security_set_username (WIRELESS_SECURITY (self), user);
	wireless_security_set_password (WIRELESS_SECURITY (self), password);
	wireless_security_set_always_ask (WIRELESS_SECURITY (self), always_ask);
	wireless_security_set_show_password (WIRELESS_SECURITY (self), FALSE);

	if (is_editor)
		simple_flags |= EAP_METHOD_SIMPLE_FLAG_IS_EDITOR;
	if (secrets_only)
		simple_flags |= EAP_METHOD_SIMPLE_FLAG_SECRETS_ONLY;

	self->em_md5 = eap_method_simple_new (WIRELESS_SECURITY (self), connection, EAP_METHOD_SIMPLE_TYPE_MD5, simple_flags);
	gtk_widget_show (GTK_WIDGET (self->em_md5));
	self->em_tls = eap_method_tls_new (WIRELESS_SECURITY (self), connection, FALSE, secrets_only);
	gtk_widget_show (GTK_WIDGET (self->em_tls));
	self->em_leap = eap_method_leap_new (WIRELESS_SECURITY (self), connection, secrets_only);
	gtk_widget_show (GTK_WIDGET (self->em_leap));
	self->em_pwd = eap_method_simple_new (WIRELESS_SECURITY (self), connection, EAP_METHOD_SIMPLE_TYPE_PWD, simple_flags);
	gtk_widget_show (GTK_WIDGET (self->em_pwd));
	self->em_fast = eap_method_fast_new (WIRELESS_SECURITY (self), connection, is_editor, secrets_only);
	gtk_widget_show (GTK_WIDGET (self->em_fast));
	self->em_ttls = eap_method_ttls_new (WIRELESS_SECURITY (self), connection, is_editor, secrets_only);
	gtk_widget_show (GTK_WIDGET (self->em_ttls));
	self->em_peap = eap_method_peap_new (WIRELESS_SECURITY (self), connection, is_editor, secrets_only);
	gtk_widget_show (GTK_WIDGET (self->em_peap));

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

	if (secrets_only) {
		gtk_widget_hide (GTK_WIDGET (self->auth_combo));
		gtk_widget_hide (GTK_WIDGET (self->auth_label));
	}

	g_signal_connect_object (G_OBJECT (self->auth_combo), "changed", G_CALLBACK (auth_combo_changed_cb), self, G_CONNECT_SWAPPED);
	auth_combo_changed_cb (self);

	return self;
}

