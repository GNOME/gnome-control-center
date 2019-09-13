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
#include "utils.h"

#define I_NAME_COLUMN   0
#define I_METHOD_COLUMN 1

struct _EAPMethodPEAP {
	EAPMethod parent;

	GtkSizeGroup *size_group;
	WirelessSecurity *sec_parent;
	gboolean is_editor;
};

static void
destroy (EAPMethod *parent)
{
	EAPMethodPEAP *method = (EAPMethodPEAP *) parent;

	if (method->size_group)
		g_object_unref (method->size_group);
}

static gboolean
validate (EAPMethod *parent, GError **error)
{
	GtkWidget *widget;
	GtkTreeModel *model;
	GtkTreeIter iter;
	g_autoptr(EAPMethod) eap = NULL;
	gboolean valid = FALSE;
	g_autoptr(GError) local_error = NULL;

	if (!eap_method_validate_filepicker (parent->builder, "eap_peap_ca_cert_button", TYPE_CA_CERT, NULL, NULL, &local_error)) {
		g_set_error (error, NMA_ERROR, NMA_ERROR_GENERIC, _("invalid EAP-PEAP CA certificate: %s"), local_error->message);
		return FALSE;
	}
	if (eap_method_ca_cert_required (parent->builder, "eap_peap_ca_cert_not_required_checkbox", "eap_peap_ca_cert_button")) {
		g_set_error_literal (error, NMA_ERROR, NMA_ERROR_GENERIC, _("invalid EAP-PEAP CA certificate: no certificate specified"));
		return FALSE;
	}

	widget = GTK_WIDGET (gtk_builder_get_object (parent->builder, "eap_peap_inner_auth_combo"));
	g_assert (widget);

	model = gtk_combo_box_get_model (GTK_COMBO_BOX (widget));
	gtk_combo_box_get_active_iter (GTK_COMBO_BOX (widget), &iter);
	gtk_tree_model_get (model, &iter, I_METHOD_COLUMN, &eap, -1);
	g_assert (eap);
	valid = eap_method_validate (eap, error);
	return valid;
}

static void
ca_cert_not_required_toggled (GtkWidget *ignored, gpointer user_data)
{
	EAPMethod *parent = user_data;

	eap_method_ca_cert_not_required_toggled (parent->builder, "eap_peap_ca_cert_not_required_checkbox", "eap_peap_ca_cert_button");
}

static void
add_to_size_group (EAPMethod *parent, GtkSizeGroup *group)
{
	EAPMethodPEAP *method = (EAPMethodPEAP *) parent;
	GtkWidget *widget;
	GtkTreeModel *model;
	GtkTreeIter iter;
	g_autoptr(EAPMethod) eap = NULL;

	if (method->size_group)
		g_object_unref (method->size_group);
	method->size_group = g_object_ref (group);

	widget = GTK_WIDGET (gtk_builder_get_object (parent->builder, "eap_peap_ca_cert_not_required_checkbox"));
	g_assert (widget);
	gtk_size_group_add_widget (group, widget);

	widget = GTK_WIDGET (gtk_builder_get_object (parent->builder, "eap_peap_anon_identity_label"));
	g_assert (widget);
	gtk_size_group_add_widget (group, widget);

	widget = GTK_WIDGET (gtk_builder_get_object (parent->builder, "eap_peap_ca_cert_label"));
	g_assert (widget);
	gtk_size_group_add_widget (group, widget);

	widget = GTK_WIDGET (gtk_builder_get_object (parent->builder, "eap_peap_version_label"));
	g_assert (widget);
	gtk_size_group_add_widget (group, widget);

	widget = GTK_WIDGET (gtk_builder_get_object (parent->builder, "eap_peap_inner_auth_label"));
	g_assert (widget);
	gtk_size_group_add_widget (group, widget);

	widget = GTK_WIDGET (gtk_builder_get_object (parent->builder, "eap_peap_inner_auth_combo"));
	g_assert (widget);

	model = gtk_combo_box_get_model (GTK_COMBO_BOX (widget));
	gtk_combo_box_get_active_iter (GTK_COMBO_BOX (widget), &iter);
	gtk_tree_model_get (model, &iter, I_METHOD_COLUMN, &eap, -1);
	g_assert (eap);
	eap_method_add_to_size_group (eap, group);
}

static void
fill_connection (EAPMethod *parent, NMConnection *connection, NMSettingSecretFlags flags)
{
	NMSetting8021x *s_8021x;
	NMSetting8021xCKFormat format = NM_SETTING_802_1X_CK_FORMAT_UNKNOWN;
	GtkWidget *widget;
	const char *text;
	g_autofree gchar *filename = NULL;
	g_autoptr(EAPMethod) eap = NULL;
	GtkTreeModel *model;
	GtkTreeIter iter;
	int peapver_active = 0;
	g_autoptr(GError) error = NULL;
	gboolean ca_cert_error = FALSE;

	s_8021x = nm_connection_get_setting_802_1x (connection);
	g_assert (s_8021x);

	nm_setting_802_1x_add_eap_method (s_8021x, "peap");

	widget = GTK_WIDGET (gtk_builder_get_object (parent->builder, "eap_peap_anon_identity_entry"));
	g_assert (widget);
	text = gtk_entry_get_text (GTK_ENTRY (widget));
	if (text && strlen (text))
		g_object_set (s_8021x, NM_SETTING_802_1X_ANONYMOUS_IDENTITY, text, NULL);

	widget = GTK_WIDGET (gtk_builder_get_object (parent->builder, "eap_peap_ca_cert_button"));
	g_assert (widget);
	filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (widget));
	if (!nm_setting_802_1x_set_ca_cert (s_8021x, filename, NM_SETTING_802_1X_CK_SCHEME_PATH, &format, &error)) {
		g_warning ("Couldn't read CA certificate '%s': %s", filename, error ? error->message : "(unknown)");
		ca_cert_error = TRUE;
	}
	eap_method_ca_cert_ignore_set (parent, connection, filename, ca_cert_error);

	widget = GTK_WIDGET (gtk_builder_get_object (parent->builder, "eap_peap_version_combo"));
	peapver_active = gtk_combo_box_get_active (GTK_COMBO_BOX (widget));
	switch (peapver_active) {
	case 1:  /* PEAP v0 */
		g_object_set (G_OBJECT (s_8021x), NM_SETTING_802_1X_PHASE1_PEAPVER, "0", NULL);
		break;
	case 2:  /* PEAP v1 */
		g_object_set (G_OBJECT (s_8021x), NM_SETTING_802_1X_PHASE1_PEAPVER, "1", NULL);
		break;
	default: /* Automatic */
		g_object_set (G_OBJECT (s_8021x), NM_SETTING_802_1X_PHASE1_PEAPVER, NULL, NULL);
		break;
	}

	widget = GTK_WIDGET (gtk_builder_get_object (parent->builder, "eap_peap_inner_auth_combo"));
	model = gtk_combo_box_get_model (GTK_COMBO_BOX (widget));
	gtk_combo_box_get_active_iter (GTK_COMBO_BOX (widget), &iter);
	gtk_tree_model_get (model, &iter, I_METHOD_COLUMN, &eap, -1);
	g_assert (eap);

	eap_method_fill_connection (eap, connection, flags);
}
static void
inner_auth_combo_changed_cb (GtkWidget *combo, gpointer user_data)
{
	EAPMethod *parent = (EAPMethod *) user_data;
	EAPMethodPEAP *method = (EAPMethodPEAP *) parent;
	GtkWidget *vbox;
	g_autoptr(EAPMethod) eap = NULL;
	GList *elt, *children;
	GtkTreeModel *model;
	GtkTreeIter iter;
	GtkWidget *eap_widget;

	vbox = GTK_WIDGET (gtk_builder_get_object (parent->builder, "eap_peap_inner_auth_vbox"));
	g_assert (vbox);

	/* Remove any previous wireless security widgets */
	children = gtk_container_get_children (GTK_CONTAINER (vbox));
	for (elt = children; elt; elt = g_list_next (elt))
		gtk_container_remove (GTK_CONTAINER (vbox), GTK_WIDGET (elt->data));

	model = gtk_combo_box_get_model (GTK_COMBO_BOX (combo));
	gtk_combo_box_get_active_iter (GTK_COMBO_BOX (combo), &iter);
	gtk_tree_model_get (model, &iter, I_METHOD_COLUMN, &eap, -1);
	g_assert (eap);

	eap_widget = eap_method_get_widget (eap);
	g_assert (eap_widget);
	gtk_widget_unparent (eap_widget);

	if (method->size_group)
		eap_method_add_to_size_group (eap, method->size_group);
	gtk_container_add (GTK_CONTAINER (vbox), eap_widget);

	wireless_security_changed_cb (combo, method->sec_parent);
}

static GtkWidget *
inner_auth_combo_init (EAPMethodPEAP *method,
                       NMConnection *connection,
                       NMSetting8021x *s_8021x,
                       gboolean secrets_only)
{
	EAPMethod *parent = (EAPMethod *) method;
	GtkWidget *combo;
	g_autoptr(GtkListStore) auth_model = NULL;
	GtkTreeIter iter;
	g_autoptr(EAPMethodSimple) em_mschap_v2 = NULL;
	g_autoptr(EAPMethodSimple) em_md5 = NULL;
	g_autoptr(EAPMethodSimple) em_gtc = NULL;
	guint32 active = 0;
	const char *phase2_auth = NULL;
	EAPMethodSimpleFlags simple_flags;

	auth_model = gtk_list_store_new (2, G_TYPE_STRING, eap_method_get_type ());

	if (s_8021x) {
		if (nm_setting_802_1x_get_phase2_auth (s_8021x))
			phase2_auth = nm_setting_802_1x_get_phase2_auth (s_8021x);
		else if (nm_setting_802_1x_get_phase2_autheap (s_8021x))
			phase2_auth = nm_setting_802_1x_get_phase2_autheap (s_8021x);
	}

	simple_flags = EAP_METHOD_SIMPLE_FLAG_PHASE2;
	if (method->is_editor)
		simple_flags |= EAP_METHOD_SIMPLE_FLAG_IS_EDITOR;
	if (secrets_only)
		simple_flags |= EAP_METHOD_SIMPLE_FLAG_SECRETS_ONLY;

	em_mschap_v2 = eap_method_simple_new (method->sec_parent,
	                                      connection,
	                                      EAP_METHOD_SIMPLE_TYPE_MSCHAP_V2,
	                                      simple_flags);
	gtk_list_store_append (auth_model, &iter);
	gtk_list_store_set (auth_model, &iter,
	                    I_NAME_COLUMN, _("MSCHAPv2"),
	                    I_METHOD_COLUMN, em_mschap_v2,
	                    -1);

	/* Check for defaulting to MSCHAPv2 */
	if (phase2_auth && !strcasecmp (phase2_auth, "mschapv2"))
		active = 0;

	em_md5 = eap_method_simple_new (method->sec_parent,
	                                connection,
	                                EAP_METHOD_SIMPLE_TYPE_MD5,
	                                simple_flags);
	gtk_list_store_append (auth_model, &iter);
	gtk_list_store_set (auth_model, &iter,
	                    I_NAME_COLUMN, _("MD5"),
	                    I_METHOD_COLUMN, em_md5,
	                    -1);

	/* Check for defaulting to MD5 */
	if (phase2_auth && !strcasecmp (phase2_auth, "md5"))
		active = 1;

	em_gtc = eap_method_simple_new (method->sec_parent,
	                                connection,
	                                EAP_METHOD_SIMPLE_TYPE_GTC,
	                                simple_flags);
	gtk_list_store_append (auth_model, &iter);
	gtk_list_store_set (auth_model, &iter,
	                    I_NAME_COLUMN, _("GTC"),
	                    I_METHOD_COLUMN, em_gtc,
	                    -1);

	/* Check for defaulting to GTC */
	if (phase2_auth && !strcasecmp (phase2_auth, "gtc"))
		active = 2;

	combo = GTK_WIDGET (gtk_builder_get_object (parent->builder, "eap_peap_inner_auth_combo"));
	g_assert (combo);

	gtk_combo_box_set_model (GTK_COMBO_BOX (combo), GTK_TREE_MODEL (auth_model));
	gtk_combo_box_set_active (GTK_COMBO_BOX (combo), active);

	g_signal_connect (G_OBJECT (combo), "changed",
	                  (GCallback) inner_auth_combo_changed_cb,
	                  method);
	return combo;
}

static void
update_secrets (EAPMethod *parent, NMConnection *connection)
{
	eap_method_phase2_update_secrets_helper (parent,
	                                         connection,
	                                         "eap_peap_inner_auth_combo",
	                                         I_METHOD_COLUMN);
}

EAPMethodPEAP *
eap_method_peap_new (WirelessSecurity *ws_parent,
                     NMConnection *connection,
                     gboolean is_editor,
                     gboolean secrets_only)
{
	EAPMethod *parent;
	EAPMethodPEAP *method;
	GtkWidget *widget, *widget_ca_not_required_checkbox;
	GtkFileFilter *filter;
	NMSetting8021x *s_8021x = NULL;
	const char *filename;

	parent = eap_method_init (sizeof (EAPMethodPEAP),
	                          validate,
	                          add_to_size_group,
	                          fill_connection,
	                          update_secrets,
	                          destroy,
	                          "/org/gnome/ControlCenter/network/eap-method-peap.ui",
	                          "eap_peap_notebook",
	                          "eap_peap_anon_identity_entry",
	                          FALSE);
	if (!parent)
		return NULL;

	parent->password_flags_name = NM_SETTING_802_1X_PASSWORD;
	method = (EAPMethodPEAP *) parent;
	method->sec_parent = ws_parent;
	method->is_editor = is_editor;

	if (connection)
		s_8021x = nm_connection_get_setting_802_1x (connection);

	widget = GTK_WIDGET (gtk_builder_get_object (parent->builder, "eap_peap_ca_cert_not_required_checkbox"));
	g_assert (widget);
	g_signal_connect (G_OBJECT (widget), "toggled",
	                  (GCallback) ca_cert_not_required_toggled,
	                  parent);
	g_signal_connect (G_OBJECT (widget), "toggled",
	                  (GCallback) wireless_security_changed_cb,
	                  ws_parent);
	widget_ca_not_required_checkbox = widget;

	widget = GTK_WIDGET (gtk_builder_get_object (parent->builder, "eap_peap_ca_cert_button"));
	g_assert (widget);
	gtk_file_chooser_set_local_only (GTK_FILE_CHOOSER (widget), TRUE);
	gtk_file_chooser_button_set_title (GTK_FILE_CHOOSER_BUTTON (widget),
	                                   _("Choose a Certificate Authority certificate"));
	g_signal_connect (G_OBJECT (widget), "selection-changed",
	                  (GCallback) wireless_security_changed_cb,
	                  ws_parent);
	filter = eap_method_default_file_chooser_filter_new (FALSE);
	gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (widget), filter);
	if (connection && s_8021x) {
		filename = NULL;
		if (nm_setting_802_1x_get_ca_cert_scheme (s_8021x) == NM_SETTING_802_1X_CK_SCHEME_PATH) {
			filename = nm_setting_802_1x_get_ca_cert_path (s_8021x);
			if (filename)
				gtk_file_chooser_set_filename (GTK_FILE_CHOOSER (widget), filename);
		}
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget_ca_not_required_checkbox),
		                              !filename && eap_method_ca_cert_ignore_get (parent, connection));
	}

	widget = inner_auth_combo_init (method, connection, s_8021x, secrets_only);
	inner_auth_combo_changed_cb (widget, (gpointer) method);

	widget = GTK_WIDGET (gtk_builder_get_object (parent->builder, "eap_peap_version_combo"));
	g_assert (widget);
	gtk_combo_box_set_active (GTK_COMBO_BOX (widget), 0);
	if (s_8021x) {
		const char *peapver;

		peapver = nm_setting_802_1x_get_phase1_peapver (s_8021x);
		if (peapver) {
			/* Index 0 is "Automatic" */
			if (!strcmp (peapver, "0"))
				gtk_combo_box_set_active (GTK_COMBO_BOX (widget), 1);
			else if (!strcmp (peapver, "1"))
				gtk_combo_box_set_active (GTK_COMBO_BOX (widget), 2);
		}
	}
	g_signal_connect (G_OBJECT (widget), "changed",
	                  (GCallback) wireless_security_changed_cb,
	                  ws_parent);

	widget = GTK_WIDGET (gtk_builder_get_object (parent->builder, "eap_peap_anon_identity_entry"));
	if (s_8021x && nm_setting_802_1x_get_anonymous_identity (s_8021x))
		gtk_entry_set_text (GTK_ENTRY (widget), nm_setting_802_1x_get_anonymous_identity (s_8021x));
	g_signal_connect (G_OBJECT (widget), "changed",
	                  (GCallback) wireless_security_changed_cb,
	                  ws_parent);

	if (secrets_only) {
		widget = GTK_WIDGET (gtk_builder_get_object (parent->builder, "eap_peap_anon_identity_label"));
		gtk_widget_hide (widget);
		widget = GTK_WIDGET (gtk_builder_get_object (parent->builder, "eap_peap_anon_identity_entry"));
		gtk_widget_hide (widget);
		widget = GTK_WIDGET (gtk_builder_get_object (parent->builder, "eap_peap_ca_cert_label"));
		gtk_widget_hide (widget);
		widget = GTK_WIDGET (gtk_builder_get_object (parent->builder, "eap_peap_ca_cert_button"));
		gtk_widget_hide (widget);
		widget = GTK_WIDGET (gtk_builder_get_object (parent->builder, "eap_peap_ca_cert_not_required_checkbox"));
		gtk_widget_hide (widget);
		widget = GTK_WIDGET (gtk_builder_get_object (parent->builder, "eap_peap_inner_auth_label"));
		gtk_widget_hide (widget);
		widget = GTK_WIDGET (gtk_builder_get_object (parent->builder, "eap_peap_inner_auth_combo"));
		gtk_widget_hide (widget);
		widget = GTK_WIDGET (gtk_builder_get_object (parent->builder, "eap_peap_version_label"));
		gtk_widget_hide (widget);
		widget = GTK_WIDGET (gtk_builder_get_object (parent->builder, "eap_peap_version_combo"));
		gtk_widget_hide (widget);
	}

	return method;
}

