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

#include <string.h>

#include "wireless-security.h"
#include "wireless-security-resources.h"
#include "eap-method.h"
#include "utils.h"

struct _WirelessSecurityPrivate {
	guint32 refcount;
	gsize obj_size;
	WSChangedFunc changed_notify;
	gpointer changed_notify_data;
	gboolean adhoc_compatible;
	gboolean hotspot_compatible;

	char *username, *password;
	gboolean always_ask, show_password;

	WSAddToSizeGroupFunc add_to_size_group;
	WSFillConnectionFunc fill_connection;
	WSGetWidgetFunc get_widget;
	WSValidateFunc validate;
	WSDestroyFunc destroy;
};

GType
wireless_security_get_type (void)
{
	static GType type_id = 0;

	if (!type_id) {
		g_resources_register (wireless_security_get_resource ());

		type_id = g_boxed_type_register_static ("CcWirelessSecurity",
							(GBoxedCopyFunc) wireless_security_ref,
							(GBoxedFreeFunc) wireless_security_unref);
	}

	return type_id;
}

GtkWidget *
wireless_security_get_widget (WirelessSecurity *self)
{
	WirelessSecurityPrivate *priv = self->priv;
	g_return_val_if_fail (self != NULL, NULL);

	g_assert (priv->get_widget);
	return (*(priv->get_widget)) (self);
}

void
wireless_security_set_changed_notify (WirelessSecurity *self,
                                      WSChangedFunc func,
                                      gpointer user_data)
{
	WirelessSecurityPrivate *priv = self->priv;
	g_return_if_fail (self != NULL);

	priv->changed_notify = func;
	priv->changed_notify_data = user_data;
}

void
wireless_security_notify_changed (WirelessSecurity *self)
{
	WirelessSecurityPrivate *priv = self->priv;

	if (priv->changed_notify)
		(*(priv->changed_notify)) (self, priv->changed_notify_data);
}

gboolean
wireless_security_validate (WirelessSecurity *self, GError **error)
{
	WirelessSecurityPrivate *priv = self->priv;
	gboolean result;

	g_return_val_if_fail (self != NULL, FALSE);
	g_return_val_if_fail (!error || !*error, FALSE);

	g_assert (priv->validate);
	result = (*(priv->validate)) (self, error);
	if (!result && error && !*error)
		g_set_error_literal (error, NMA_ERROR, NMA_ERROR_GENERIC, _("Unknown error validating 802.1X security"));
	return result;
}

void
wireless_security_add_to_size_group (WirelessSecurity *self, GtkSizeGroup *group)
{
	WirelessSecurityPrivate *priv = self->priv;

	g_return_if_fail (self != NULL);
	g_return_if_fail (group != NULL);

	g_assert (priv->add_to_size_group);
	return (*(priv->add_to_size_group)) (self, group);
}

void
wireless_security_fill_connection (WirelessSecurity *self,
                                   NMConnection *connection)
{
	WirelessSecurityPrivate *priv = self->priv;

	g_return_if_fail (self != NULL);
	g_return_if_fail (connection != NULL);

	g_assert (priv->fill_connection);
	return (*(priv->fill_connection)) (self, connection);
}

WirelessSecurity *
wireless_security_ref (WirelessSecurity *self)
{
	WirelessSecurityPrivate *priv = self->priv;

	g_return_val_if_fail (self != NULL, NULL);
	g_return_val_if_fail (priv->refcount > 0, NULL);

	priv->refcount++;
	return self;
}

void
wireless_security_unref (WirelessSecurity *self)
{
	WirelessSecurityPrivate *priv = self->priv;

	g_return_if_fail (self != NULL);
	g_return_if_fail (priv->refcount > 0);

	priv->refcount--;
	if (priv->refcount == 0) {
		if (priv->destroy)
			priv->destroy (self);

		if (priv->password)
			memset (priv->password, 0, strlen (priv->password));

		g_clear_pointer (&priv->username, g_free);
		g_clear_pointer (&priv->password, g_free);

		g_clear_object (&self->builder);
		g_slice_free1 (priv->obj_size, self);
		g_free (priv);
	}
}

WirelessSecurity *
wireless_security_init (gsize obj_size,
                        WSGetWidgetFunc get_widget,
                        WSValidateFunc validate,
                        WSAddToSizeGroupFunc add_to_size_group,
                        WSFillConnectionFunc fill_connection,
                        WSDestroyFunc destroy,
                        const char *ui_resource)
{
	g_autoptr(WirelessSecurity) self = NULL;
	WirelessSecurityPrivate *priv;
	g_autoptr(GError) error = NULL;

	g_return_val_if_fail (obj_size > 0, NULL);
	g_return_val_if_fail (ui_resource != NULL, NULL);

	g_type_ensure (WIRELESS_TYPE_SECURITY);

	self = g_slice_alloc0 (obj_size);
	g_assert (self);
	self->priv = priv = g_new0 (WirelessSecurityPrivate, 1);

	priv->refcount = 1;
	priv->obj_size = obj_size;

	priv->get_widget = get_widget;
	priv->validate = validate;
	priv->add_to_size_group = add_to_size_group;
	priv->fill_connection = fill_connection;

	self->builder = gtk_builder_new ();
	if (!gtk_builder_add_from_resource (self->builder, ui_resource, &error)) {
		g_warning ("Couldn't load UI builder resource %s: %s",
		           ui_resource, error->message);
		return NULL;
	}

	priv->destroy = destroy;
	priv->adhoc_compatible = TRUE;
	priv->hotspot_compatible = TRUE;

	return g_steal_pointer (&self);
}

void
wireless_security_set_adhoc_compatible (WirelessSecurity *self, gboolean adhoc_compatible)
{
	WirelessSecurityPrivate *priv = self->priv;

	g_return_if_fail (self != NULL);

	priv->adhoc_compatible = adhoc_compatible;
}

gboolean
wireless_security_adhoc_compatible (WirelessSecurity *self)
{
	WirelessSecurityPrivate *priv = self->priv;

	g_return_val_if_fail (self != NULL, FALSE);

	return priv->adhoc_compatible;
}

void
wireless_security_set_hotspot_compatible (WirelessSecurity *self, gboolean hotspot_compatible)
{
	WirelessSecurityPrivate *priv = self->priv;

	g_return_if_fail (self != NULL);

	priv->hotspot_compatible = hotspot_compatible;
}

gboolean
wireless_security_hotspot_compatible (WirelessSecurity *self)
{
	WirelessSecurityPrivate *priv = self->priv;

	g_return_val_if_fail (self != NULL, FALSE);

	return priv->hotspot_compatible;
}

const gchar *
wireless_security_get_username (WirelessSecurity *self)
{
	WirelessSecurityPrivate *priv = self->priv;

	g_return_val_if_fail (self != NULL, NULL);

	return priv->username;
}

const gchar *
wireless_security_get_password (WirelessSecurity *self)
{
	WirelessSecurityPrivate *priv = self->priv;

	g_return_val_if_fail (self != NULL, NULL);

	return priv->password;
}

gboolean
wireless_security_get_always_ask (WirelessSecurity *self)
{
	WirelessSecurityPrivate *priv = self->priv;

	g_return_val_if_fail (self != NULL, FALSE);

	return priv->always_ask;
}

gboolean
wireless_security_get_show_password (WirelessSecurity *self)
{
	WirelessSecurityPrivate *priv = self->priv;

	g_return_val_if_fail (self != NULL, FALSE);

	return priv->show_password;
}

void
wireless_security_set_userpass (WirelessSecurity *self,
                                const char *user,
                                const char *password,
                                gboolean always_ask,
                                gboolean show_password)
{
	WirelessSecurityPrivate *priv = self->priv;

	g_clear_pointer (&priv->username, g_free);
	priv->username = g_strdup (user);

	if (priv->password)
		memset (priv->password, 0, strlen (priv->password));

	g_clear_pointer (&priv->password, g_free);
	priv->password = g_strdup (password);

	if (always_ask != (gboolean) -1)
		priv->always_ask = always_ask;
	priv->show_password = show_password;
}

void
wireless_security_set_userpass_802_1x (WirelessSecurity *self,
                                       NMConnection *connection)
{
	const char *user = NULL, *password = NULL;
	gboolean always_ask = FALSE, show_password = FALSE;
	NMSetting8021x  *setting;
	NMSettingSecretFlags flags;

	if (!connection)
		goto set;

	setting = nm_connection_get_setting_802_1x (connection);
	if (!setting)
		goto set;

	user = nm_setting_802_1x_get_identity (setting);
	password = nm_setting_802_1x_get_password (setting);

	if (nm_setting_get_secret_flags (NM_SETTING (setting), NM_SETTING_802_1X_PASSWORD, &flags, NULL))
		always_ask = !!(flags & NM_SETTING_SECRET_FLAG_NOT_SAVED);

set:
	wireless_security_set_userpass (self, user, password, always_ask, show_password);
}

void
wireless_security_clear_ciphers (NMConnection *connection)
{
	NMSettingWirelessSecurity *s_wireless_sec;

	g_return_if_fail (connection != NULL);

	s_wireless_sec = nm_connection_get_setting_wireless_security (connection);
	g_assert (s_wireless_sec);

	nm_setting_wireless_security_clear_protos (s_wireless_sec);
	nm_setting_wireless_security_clear_pairwise (s_wireless_sec);
	nm_setting_wireless_security_clear_groups (s_wireless_sec);
}

void
ws_802_1x_add_to_size_group (GtkSizeGroup *size_group,
                             GtkLabel *label,
                             GtkComboBox *combo)
{
	GtkTreeModel *model;
	GtkTreeIter iter;
	g_autoptr(EAPMethod) eap = NULL;

	gtk_size_group_add_widget (size_group, GTK_WIDGET (label));

	model = gtk_combo_box_get_model (combo);
	gtk_combo_box_get_active_iter (combo, &iter);
	gtk_tree_model_get (model, &iter, AUTH_METHOD_COLUMN, &eap, -1);
	g_assert (eap);
	eap_method_add_to_size_group (eap, size_group);
}

gboolean
ws_802_1x_validate (GtkComboBox *combo, GError **error)
{
	GtkTreeModel *model;
	GtkTreeIter iter;
	g_autoptr(EAPMethod) eap = NULL;
	gboolean valid = FALSE;

	model = gtk_combo_box_get_model (combo);
	gtk_combo_box_get_active_iter (combo, &iter);
	gtk_tree_model_get (model, &iter, AUTH_METHOD_COLUMN, &eap, -1);
	g_assert (eap);
	valid = eap_method_validate (eap, error);
	return valid;
}

void
ws_802_1x_auth_combo_changed (GtkWidget *combo,
                              WirelessSecurity *self,
                              GtkBox *vbox,
                              GtkSizeGroup *size_group)
{
	g_autoptr(EAPMethod) eap = NULL;
	GList *elt, *children;
	GtkTreeModel *model;
	GtkTreeIter iter;
	GtkWidget *eap_widget;
	GtkWidget *eap_default_widget = NULL;

	/* Remove any previous wireless security widgets */
	children = gtk_container_get_children (GTK_CONTAINER (vbox));
	for (elt = children; elt; elt = g_list_next (elt))
		gtk_container_remove (GTK_CONTAINER (vbox), GTK_WIDGET (elt->data));

	model = gtk_combo_box_get_model (GTK_COMBO_BOX (combo));
	gtk_combo_box_get_active_iter (GTK_COMBO_BOX (combo), &iter);
	gtk_tree_model_get (model, &iter, AUTH_METHOD_COLUMN, &eap, -1);
	g_assert (eap);

	eap_widget = eap_method_get_widget (eap);
	g_assert (eap_widget);
	gtk_widget_unparent (eap_widget);

	if (size_group)
		eap_method_add_to_size_group (eap, size_group);
	gtk_container_add (GTK_CONTAINER (vbox), eap_widget);

	/* Refocus the EAP method's default widget */
	if (eap->default_field) {
		eap_default_widget = GTK_WIDGET (gtk_builder_get_object (eap->builder, eap->default_field));
		if (eap_default_widget)
			gtk_widget_grab_focus (eap_default_widget);
	}

	wireless_security_notify_changed (WIRELESS_SECURITY (self));
}

void
ws_802_1x_auth_combo_init (WirelessSecurity *self,
                           GtkComboBox *combo,
                           GtkLabel *label,
                           GCallback auth_combo_changed_cb,
                           NMConnection *connection,
                           gboolean is_editor,
                           gboolean secrets_only)
{
	g_autoptr(GtkListStore) auth_model = NULL;
	GtkTreeIter iter;
	g_autoptr(EAPMethodTLS) em_tls = NULL;
	g_autoptr(EAPMethodSimple) em_pwd = NULL;
	g_autoptr(EAPMethodFAST) em_fast = NULL;
	g_autoptr(EAPMethodTTLS) em_ttls = NULL;
	g_autoptr(EAPMethodPEAP) em_peap = NULL;
	const char *default_method = NULL, *ctype = NULL;
	int active = -1, item = 0;
	gboolean wired = FALSE;
	EAPMethodSimpleFlags simple_flags = EAP_METHOD_SIMPLE_FLAG_NONE;

	/* Grab the default EAP method out of the security object */
	if (connection) {
		NMSettingConnection *s_con;
		NMSetting8021x *s_8021x;

		s_con = nm_connection_get_setting_connection (connection);
		if (s_con)
			ctype = nm_setting_connection_get_connection_type (s_con);
		if (   (g_strcmp0 (ctype, NM_SETTING_WIRED_SETTING_NAME) == 0)
		    || nm_connection_get_setting_wired (connection))
			wired = TRUE;

		s_8021x = nm_connection_get_setting_802_1x (connection);
		if (s_8021x && nm_setting_802_1x_get_num_eap_methods (s_8021x))
			default_method = nm_setting_802_1x_get_eap_method (s_8021x, 0);
	}

	/* initialize WirelessSecurity userpass from connection (clear if no connection) */
	wireless_security_set_userpass_802_1x (self, connection);

	auth_model = gtk_list_store_new (2, G_TYPE_STRING, eap_method_get_type ());

	if (is_editor)
		simple_flags |= EAP_METHOD_SIMPLE_FLAG_IS_EDITOR;
	if (secrets_only)
		simple_flags |= EAP_METHOD_SIMPLE_FLAG_SECRETS_ONLY;

	if (wired) {
		g_autoptr(EAPMethodSimple) em_md5 = NULL;

		em_md5 = eap_method_simple_new (self, connection, EAP_METHOD_SIMPLE_TYPE_MD5, simple_flags);
		gtk_list_store_append (auth_model, &iter);
		gtk_list_store_set (auth_model, &iter,
			                AUTH_NAME_COLUMN, _("MD5"),
			                AUTH_METHOD_COLUMN, em_md5,
			                -1);
		if (default_method && (active < 0) && !strcmp (default_method, "md5"))
			active = item;
		item++;
	}

	em_tls = eap_method_tls_new (self, connection, FALSE, secrets_only);
	gtk_list_store_append (auth_model, &iter);
	gtk_list_store_set (auth_model, &iter,
	                    AUTH_NAME_COLUMN, _("TLS"),
	                    AUTH_METHOD_COLUMN, em_tls,
	                    -1);
	if (default_method && (active < 0) && !strcmp (default_method, "tls"))
		active = item;
	item++;

	if (!wired) {
		g_autoptr(EAPMethodLEAP) em_leap = NULL;

		em_leap = eap_method_leap_new (self, connection, secrets_only);
		gtk_list_store_append (auth_model, &iter);
		gtk_list_store_set (auth_model, &iter,
		                    AUTH_NAME_COLUMN, _("LEAP"),
		                    AUTH_METHOD_COLUMN, em_leap,
		                    -1);
		if (default_method && (active < 0) && !strcmp (default_method, "leap"))
			active = item;
		item++;
	}

	em_pwd = eap_method_simple_new (self, connection, EAP_METHOD_SIMPLE_TYPE_PWD, simple_flags);
	gtk_list_store_append (auth_model, &iter);
	gtk_list_store_set (auth_model, &iter,
	                    AUTH_NAME_COLUMN, _("PWD"),
	                    AUTH_METHOD_COLUMN, em_pwd,
	                    -1);
	if (default_method && (active < 0) && !strcmp (default_method, "pwd"))
		active = item;
	item++;

	em_fast = eap_method_fast_new (self, connection, is_editor, secrets_only);
	gtk_list_store_append (auth_model, &iter);
	gtk_list_store_set (auth_model, &iter,
	                    AUTH_NAME_COLUMN, _("FAST"),
	                    AUTH_METHOD_COLUMN, em_fast,
	                    -1);
	if (default_method && (active < 0) && !strcmp (default_method, "fast"))
		active = item;
	item++;

	em_ttls = eap_method_ttls_new (self, connection, is_editor, secrets_only);
	gtk_list_store_append (auth_model, &iter);
	gtk_list_store_set (auth_model, &iter,
	                    AUTH_NAME_COLUMN, _("Tunneled TLS"),
	                    AUTH_METHOD_COLUMN, em_ttls,
	                    -1);
	if (default_method && (active < 0) && !strcmp (default_method, "ttls"))
		active = item;
	item++;

	em_peap = eap_method_peap_new (self, connection, is_editor, secrets_only);
	gtk_list_store_append (auth_model, &iter);
	gtk_list_store_set (auth_model, &iter,
	                    AUTH_NAME_COLUMN, _("Protected EAP (PEAP)"),
	                    AUTH_METHOD_COLUMN, em_peap,
	                    -1);
	if (default_method && (active < 0) && !strcmp (default_method, "peap"))
		active = item;
	item++;

	gtk_combo_box_set_model (combo, GTK_TREE_MODEL (auth_model));
	gtk_combo_box_set_active (combo, active < 0 ? 0 : (guint32) active);

	g_signal_connect (G_OBJECT (combo), "changed", auth_combo_changed_cb, self);

	if (secrets_only) {
		gtk_widget_hide (GTK_WIDGET (combo));
		gtk_widget_hide (GTK_WIDGET (label));
	}
}

void
ws_802_1x_fill_connection (GtkComboBox *combo,
                           NMConnection *connection)
{
	NMSettingWirelessSecurity *s_wireless_sec;
	NMSetting8021x *s_8021x;
	NMSettingSecretFlags secret_flags = NM_SETTING_SECRET_FLAG_NONE;
	g_autoptr(EAPMethod) eap = NULL;
	GtkTreeModel *model;
	GtkTreeIter iter;

	/* Get the EAPMethod object */
	model = gtk_combo_box_get_model (combo);
	gtk_combo_box_get_active_iter (combo, &iter);
	gtk_tree_model_get (model, &iter, AUTH_METHOD_COLUMN, &eap, -1);
	g_assert (eap);

	/* Get previous pasword flags, if any. Otherwise default to agent-owned secrets */
	s_8021x = nm_connection_get_setting_802_1x (connection);
	if (s_8021x)
		nm_setting_get_secret_flags (NM_SETTING (s_8021x), eap->password_flags_name, &secret_flags, NULL);
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

void
ws_802_1x_update_secrets (GtkComboBox *combo,
                          NMConnection *connection)
{
	GtkTreeModel *model;
	GtkTreeIter iter;

	g_return_if_fail (combo != NULL);
	g_return_if_fail (connection != NULL);

	model = gtk_combo_box_get_model (combo);

	/* Let each EAP method try to update its secrets */
	if (gtk_tree_model_get_iter_first (model, &iter)) {
		do {
			g_autoptr(EAPMethod) eap = NULL;

			gtk_tree_model_get (model, &iter, AUTH_METHOD_COLUMN, &eap, -1);
			if (eap)
				eap_method_update_secrets (eap, connection);
		} while (gtk_tree_model_iter_next (model, &iter));
	}
}

