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
wireless_security_get_widget (WirelessSecurity *sec)
{
	g_return_val_if_fail (sec != NULL, NULL);

	return sec->ui_widget;
}

void
wireless_security_set_changed_notify (WirelessSecurity *sec,
                                      WSChangedFunc func,
                                      gpointer user_data)
{
	g_return_if_fail (sec != NULL);

	sec->changed_notify = func;
	sec->changed_notify_data = user_data;
}

void
wireless_security_changed_cb (GtkWidget *ignored, gpointer user_data)
{
	WirelessSecurity *sec = WIRELESS_SECURITY (user_data);

	if (sec->changed_notify)
		(*(sec->changed_notify)) (sec, sec->changed_notify_data);
}

gboolean
wireless_security_validate (WirelessSecurity *sec, GError **error)
{
	gboolean result;

	g_return_val_if_fail (sec != NULL, FALSE);
	g_return_val_if_fail (!error || !*error, FALSE);

	g_assert (sec->validate);
	result = (*(sec->validate)) (sec, error);
	if (!result && error && !*error)
		g_set_error_literal (error, NMA_ERROR, NMA_ERROR_GENERIC, _("Unknown error validating 802.1X security"));
	return result;
}

void
wireless_security_add_to_size_group (WirelessSecurity *sec, GtkSizeGroup *group)
{
	g_return_if_fail (sec != NULL);
	g_return_if_fail (group != NULL);

	g_assert (sec->add_to_size_group);
	return (*(sec->add_to_size_group)) (sec, group);
}

void
wireless_security_fill_connection (WirelessSecurity *sec,
                                   NMConnection *connection)
{
	g_return_if_fail (sec != NULL);
	g_return_if_fail (connection != NULL);

	g_assert (sec->fill_connection);
	return (*(sec->fill_connection)) (sec, connection);
}

void
wireless_security_update_secrets (WirelessSecurity *sec, NMConnection *connection)
{
	g_return_if_fail (sec != NULL);
	g_return_if_fail (connection != NULL);

	if (sec->update_secrets)
		sec->update_secrets (sec, connection);
}

WirelessSecurity *
wireless_security_ref (WirelessSecurity *sec)
{
	g_return_val_if_fail (sec != NULL, NULL);
	g_return_val_if_fail (sec->refcount > 0, NULL);

	sec->refcount++;
	return sec;
}

void
wireless_security_unref (WirelessSecurity *sec)
{
	g_return_if_fail (sec != NULL);
	g_return_if_fail (sec->refcount > 0);

	sec->refcount--;
	if (sec->refcount == 0) {
		if (sec->destroy)
			sec->destroy (sec);

		g_free (sec->username);
		if (sec->password) {
			memset (sec->password, 0, strlen (sec->password));
			g_free (sec->password);
		}

		if (sec->builder)
			g_object_unref (sec->builder);
		if (sec->ui_widget)
			g_object_unref (sec->ui_widget);
		g_slice_free1 (sec->obj_size, sec);
	}
}

WirelessSecurity *
wireless_security_init (gsize obj_size,
                        WSValidateFunc validate,
                        WSAddToSizeGroupFunc add_to_size_group,
                        WSFillConnectionFunc fill_connection,
                        WSUpdateSecretsFunc update_secrets,
                        WSDestroyFunc destroy,
                        const char *ui_resource,
                        const char *ui_widget_name,
                        const char *default_field)
{
	WirelessSecurity *sec;
	g_autoptr(GError) error = NULL;

	g_return_val_if_fail (obj_size > 0, NULL);
	g_return_val_if_fail (ui_resource != NULL, NULL);
	g_return_val_if_fail (ui_widget_name != NULL, NULL);

	g_type_ensure (WIRELESS_TYPE_SECURITY);

	sec = g_slice_alloc0 (obj_size);
	g_assert (sec);

	sec->refcount = 1;
	sec->obj_size = obj_size;

	sec->validate = validate;
	sec->add_to_size_group = add_to_size_group;
	sec->fill_connection = fill_connection;
	sec->update_secrets = update_secrets;
	sec->default_field = default_field;

	sec->builder = gtk_builder_new ();
	if (!gtk_builder_add_from_resource (sec->builder, ui_resource, &error)) {
		g_warning ("Couldn't load UI builder resource %s: %s",
		           ui_resource, error->message);
		wireless_security_unref (sec);
		return NULL;
	}

	sec->ui_widget = GTK_WIDGET (gtk_builder_get_object (sec->builder, ui_widget_name));
	if (!sec->ui_widget) {
		g_warning ("Couldn't load UI widget '%s' from UI file %s",
		           ui_widget_name, ui_resource);
		wireless_security_unref (sec);
		return NULL;
	}
	g_object_ref_sink (sec->ui_widget);

	sec->destroy = destroy;
	sec->adhoc_compatible = TRUE;
	sec->hotspot_compatible = TRUE;

	return sec;
}

gboolean
wireless_security_adhoc_compatible (WirelessSecurity *sec)
{
	g_return_val_if_fail (sec != NULL, FALSE);

	return sec->adhoc_compatible;
}

gboolean
wireless_security_hotspot_compatible (WirelessSecurity *sec)
{
	g_return_val_if_fail (sec != NULL, FALSE);

	return sec->hotspot_compatible;
}

void
wireless_security_set_userpass (WirelessSecurity *sec,
                                const char *user,
                                const char *password,
                                gboolean always_ask,
                                gboolean show_password)
{
	g_free (sec->username);
	sec->username = g_strdup (user);

	if (sec->password) {
		memset (sec->password, 0, strlen (sec->password));
		g_free (sec->password);
	}
	sec->password = g_strdup (password);

	if (always_ask != (gboolean) -1)
		sec->always_ask = always_ask;
	sec->show_password = show_password;
}

void
wireless_security_set_userpass_802_1x (WirelessSecurity *sec,
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
	wireless_security_set_userpass (sec, user, password, always_ask, show_password);
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
ws_802_1x_add_to_size_group (WirelessSecurity *sec,
                             GtkSizeGroup *size_group,
                             const char *label_name,
                             const char *combo_name)
{
	GtkWidget *widget;
	GtkTreeModel *model;
	GtkTreeIter iter;
	EAPMethod *eap;

	widget = GTK_WIDGET (gtk_builder_get_object (sec->builder, label_name));
	g_assert (widget);
	gtk_size_group_add_widget (size_group, widget);

	widget = GTK_WIDGET (gtk_builder_get_object (sec->builder, combo_name));
	g_assert (widget);

	model = gtk_combo_box_get_model (GTK_COMBO_BOX (widget));
	gtk_combo_box_get_active_iter (GTK_COMBO_BOX (widget), &iter);
	gtk_tree_model_get (model, &iter, AUTH_METHOD_COLUMN, &eap, -1);
	g_assert (eap);
	eap_method_add_to_size_group (eap, size_group);
	eap_method_unref (eap);
}

gboolean
ws_802_1x_validate (WirelessSecurity *sec, const char *combo_name, GError **error)
{
	GtkWidget *widget;
	GtkTreeModel *model;
	GtkTreeIter iter;
	EAPMethod *eap = NULL;
	gboolean valid = FALSE;

	widget = GTK_WIDGET (gtk_builder_get_object (sec->builder, combo_name));
	g_assert (widget);

	model = gtk_combo_box_get_model (GTK_COMBO_BOX (widget));
	gtk_combo_box_get_active_iter (GTK_COMBO_BOX (widget), &iter);
	gtk_tree_model_get (model, &iter, AUTH_METHOD_COLUMN, &eap, -1);
	g_assert (eap);
	valid = eap_method_validate (eap, error);
	eap_method_unref (eap);
	return valid;
}

void
ws_802_1x_auth_combo_changed (GtkWidget *combo,
                              WirelessSecurity *sec,
                              const char *vbox_name,
                              GtkSizeGroup *size_group)
{
	GtkWidget *vbox;
	EAPMethod *eap = NULL;
	GList *elt, *children;
	GtkTreeModel *model;
	GtkTreeIter iter;
	GtkWidget *eap_widget;
	GtkWidget *eap_default_widget = NULL;

	vbox = GTK_WIDGET (gtk_builder_get_object (sec->builder, vbox_name));
	g_assert (vbox);

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

	eap_method_unref (eap);

	wireless_security_changed_cb (combo, WIRELESS_SECURITY (sec));
}

GtkWidget *
ws_802_1x_auth_combo_init (WirelessSecurity *sec,
                           const char *combo_name,
                           const char *combo_label,
                           GCallback auth_combo_changed_cb,
                           NMConnection *connection,
                           gboolean is_editor,
                           gboolean secrets_only)
{
	GtkWidget *combo, *widget;
	GtkListStore *auth_model;
	GtkTreeIter iter;
	EAPMethodSimple *em_md5;
	EAPMethodTLS *em_tls;
	EAPMethodLEAP *em_leap;
	EAPMethodSimple *em_pwd;
	EAPMethodFAST *em_fast;
	EAPMethodTTLS *em_ttls;
	EAPMethodPEAP *em_peap;
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
	wireless_security_set_userpass_802_1x (sec, connection);

	auth_model = gtk_list_store_new (2, G_TYPE_STRING, eap_method_get_type ());

	if (is_editor)
		simple_flags |= EAP_METHOD_SIMPLE_FLAG_IS_EDITOR;
	if (secrets_only)
		simple_flags |= EAP_METHOD_SIMPLE_FLAG_SECRETS_ONLY;

	if (wired) {
		em_md5 = eap_method_simple_new (sec, connection, EAP_METHOD_SIMPLE_TYPE_MD5, simple_flags);
		gtk_list_store_append (auth_model, &iter);
		gtk_list_store_set (auth_model, &iter,
			                AUTH_NAME_COLUMN, _("MD5"),
			                AUTH_METHOD_COLUMN, em_md5,
			                -1);
		eap_method_unref (EAP_METHOD (em_md5));
		if (default_method && (active < 0) && !strcmp (default_method, "md5"))
			active = item;
		item++;
	}

	em_tls = eap_method_tls_new (sec, connection, FALSE, secrets_only);
	gtk_list_store_append (auth_model, &iter);
	gtk_list_store_set (auth_model, &iter,
	                    AUTH_NAME_COLUMN, _("TLS"),
	                    AUTH_METHOD_COLUMN, em_tls,
	                    -1);
	eap_method_unref (EAP_METHOD (em_tls));
	if (default_method && (active < 0) && !strcmp (default_method, "tls"))
		active = item;
	item++;

	if (!wired) {
		em_leap = eap_method_leap_new (sec, connection, secrets_only);
		gtk_list_store_append (auth_model, &iter);
		gtk_list_store_set (auth_model, &iter,
		                    AUTH_NAME_COLUMN, _("LEAP"),
		                    AUTH_METHOD_COLUMN, em_leap,
		                    -1);
		eap_method_unref (EAP_METHOD (em_leap));
		if (default_method && (active < 0) && !strcmp (default_method, "leap"))
			active = item;
		item++;
	}

	em_pwd = eap_method_simple_new (sec, connection, EAP_METHOD_SIMPLE_TYPE_PWD, simple_flags);
	gtk_list_store_append (auth_model, &iter);
	gtk_list_store_set (auth_model, &iter,
	                    AUTH_NAME_COLUMN, _("PWD"),
	                    AUTH_METHOD_COLUMN, em_pwd,
	                    -1);
	eap_method_unref (EAP_METHOD (em_pwd));
	if (default_method && (active < 0) && !strcmp (default_method, "pwd"))
		active = item;
	item++;

	em_fast = eap_method_fast_new (sec, connection, is_editor, secrets_only);
	gtk_list_store_append (auth_model, &iter);
	gtk_list_store_set (auth_model, &iter,
	                    AUTH_NAME_COLUMN, _("FAST"),
	                    AUTH_METHOD_COLUMN, em_fast,
	                    -1);
	eap_method_unref (EAP_METHOD (em_fast));
	if (default_method && (active < 0) && !strcmp (default_method, "fast"))
		active = item;
	item++;

	em_ttls = eap_method_ttls_new (sec, connection, is_editor, secrets_only);
	gtk_list_store_append (auth_model, &iter);
	gtk_list_store_set (auth_model, &iter,
	                    AUTH_NAME_COLUMN, _("Tunneled TLS"),
	                    AUTH_METHOD_COLUMN, em_ttls,
	                    -1);
	eap_method_unref (EAP_METHOD (em_ttls));
	if (default_method && (active < 0) && !strcmp (default_method, "ttls"))
		active = item;
	item++;

	em_peap = eap_method_peap_new (sec, connection, is_editor, secrets_only);
	gtk_list_store_append (auth_model, &iter);
	gtk_list_store_set (auth_model, &iter,
	                    AUTH_NAME_COLUMN, _("Protected EAP (PEAP)"),
	                    AUTH_METHOD_COLUMN, em_peap,
	                    -1);
	eap_method_unref (EAP_METHOD (em_peap));
	if (default_method && (active < 0) && !strcmp (default_method, "peap"))
		active = item;
	item++;

	combo = GTK_WIDGET (gtk_builder_get_object (sec->builder, combo_name));
	g_assert (combo);

	gtk_combo_box_set_model (GTK_COMBO_BOX (combo), GTK_TREE_MODEL (auth_model));
	g_object_unref (G_OBJECT (auth_model));
	gtk_combo_box_set_active (GTK_COMBO_BOX (combo), active < 0 ? 0 : (guint32) active);

	g_signal_connect (G_OBJECT (combo), "changed", auth_combo_changed_cb, sec);

	if (secrets_only) {
		gtk_widget_hide (combo);
		widget = GTK_WIDGET (gtk_builder_get_object (sec->builder, combo_label));
		gtk_widget_hide (widget);
	}

	return combo;
}

void
ws_802_1x_fill_connection (WirelessSecurity *sec,
                           const char *combo_name,
                           NMConnection *connection)
{
	GtkWidget *widget;
	NMSettingWirelessSecurity *s_wireless_sec;
	NMSetting8021x *s_8021x;
	NMSettingSecretFlags secret_flags = NM_SETTING_SECRET_FLAG_NONE;
	EAPMethod *eap = NULL;
	GtkTreeModel *model;
	GtkTreeIter iter;

	/* Get the EAPMethod object */
	widget = GTK_WIDGET (gtk_builder_get_object (sec->builder, combo_name));
	model = gtk_combo_box_get_model (GTK_COMBO_BOX (widget));
	gtk_combo_box_get_active_iter (GTK_COMBO_BOX (widget), &iter);
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
	eap_method_unref (eap);
}

void
ws_802_1x_update_secrets (WirelessSecurity *sec,
                          const char *combo_name,
                          NMConnection *connection)
{
	GtkWidget *widget;
	EAPMethod *eap = NULL;
	GtkTreeModel *model;
	GtkTreeIter iter;

	g_return_if_fail (sec != NULL);
	g_return_if_fail (combo_name != NULL);
	g_return_if_fail (connection != NULL);

	widget = GTK_WIDGET (gtk_builder_get_object (sec->builder, combo_name));
	g_return_if_fail (widget != NULL);
	model = gtk_combo_box_get_model (GTK_COMBO_BOX (widget));

	/* Let each EAP method try to update its secrets */
	if (gtk_tree_model_get_iter_first (model, &iter)) {
		do {
			gtk_tree_model_get (model, &iter, AUTH_METHOD_COLUMN, &eap, -1);
			if (eap) {
				eap_method_update_secrets (eap, connection);
				eap_method_unref (eap);
			}
		} while (gtk_tree_model_iter_next (model, &iter));
	}
}

