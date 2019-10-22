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

	GtkEntry             *anon_identity_entry;
	GtkLabel             *anon_identity_label;
	GtkFileChooserButton *ca_cert_button;
	GtkLabel             *ca_cert_label;
	GtkCheckButton       *ca_cert_not_required_check;
	GtkBox               *inner_auth_box;
	GtkComboBox          *inner_auth_combo;
	GtkLabel             *inner_auth_label;
	GtkComboBox          *version_combo;
	GtkLabel             *version_label;

	GtkSizeGroup *size_group;
	WirelessSecurity *sec_parent;
	gboolean is_editor;
};

static void
destroy (EAPMethod *parent)
{
	EAPMethodPEAP *self = (EAPMethodPEAP *) parent;

	g_clear_object (&self->size_group);
}

static gboolean
validate (EAPMethod *parent, GError **error)
{
	EAPMethodPEAP *self = (EAPMethodPEAP *) parent;
	GtkTreeModel *model;
	GtkTreeIter iter;
	g_autoptr(EAPMethod) eap = NULL;
	gboolean valid = FALSE;
	g_autoptr(GError) local_error = NULL;

	if (!eap_method_validate_filepicker (GTK_FILE_CHOOSER (self->ca_cert_button),
	                                     TYPE_CA_CERT, NULL, NULL, &local_error)) {
		g_set_error (error, NMA_ERROR, NMA_ERROR_GENERIC, _("invalid EAP-PEAP CA certificate: %s"), local_error->message);
		return FALSE;
	}
	if (eap_method_ca_cert_required (GTK_TOGGLE_BUTTON (self->ca_cert_not_required_check),
	                                 GTK_FILE_CHOOSER (self->ca_cert_button))) {
		g_set_error_literal (error, NMA_ERROR, NMA_ERROR_GENERIC, _("invalid EAP-PEAP CA certificate: no certificate specified"));
		return FALSE;
	}

	model = gtk_combo_box_get_model (self->inner_auth_combo);
	gtk_combo_box_get_active_iter (self->inner_auth_combo, &iter);
	gtk_tree_model_get (model, &iter, I_METHOD_COLUMN, &eap, -1);
	g_assert (eap);
	valid = eap_method_validate (eap, error);
	return valid;
}

static void
ca_cert_not_required_toggled (EAPMethodPEAP *self)
{
	eap_method_ca_cert_not_required_toggled (GTK_TOGGLE_BUTTON (self->ca_cert_not_required_check),
	                                         GTK_FILE_CHOOSER (self->ca_cert_button));
	wireless_security_notify_changed (self->sec_parent);
}

static void
add_to_size_group (EAPMethod *parent, GtkSizeGroup *group)
{
	EAPMethodPEAP *self = (EAPMethodPEAP *) parent;
	GtkTreeModel *model;
	GtkTreeIter iter;
	g_autoptr(EAPMethod) eap = NULL;

	g_clear_object (&self->size_group);
	self->size_group = g_object_ref (group);

	gtk_size_group_add_widget (group, GTK_WIDGET (self->ca_cert_not_required_check));
	gtk_size_group_add_widget (group, GTK_WIDGET (self->anon_identity_label));
	gtk_size_group_add_widget (group, GTK_WIDGET (self->ca_cert_label));
	gtk_size_group_add_widget (group, GTK_WIDGET (self->version_label));
	gtk_size_group_add_widget (group, GTK_WIDGET (self->inner_auth_label));

	model = gtk_combo_box_get_model (self->inner_auth_combo);
	gtk_combo_box_get_active_iter (self->inner_auth_combo, &iter);
	gtk_tree_model_get (model, &iter, I_METHOD_COLUMN, &eap, -1);
	g_assert (eap);
	eap_method_add_to_size_group (eap, group);
}

static void
fill_connection (EAPMethod *parent, NMConnection *connection, NMSettingSecretFlags flags)
{
	EAPMethodPEAP *self = (EAPMethodPEAP *) parent;
	NMSetting8021x *s_8021x;
	NMSetting8021xCKFormat format = NM_SETTING_802_1X_CK_FORMAT_UNKNOWN;
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

	text = gtk_entry_get_text (self->anon_identity_entry);
	if (text && strlen (text))
		g_object_set (s_8021x, NM_SETTING_802_1X_ANONYMOUS_IDENTITY, text, NULL);

	filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (self->ca_cert_button));
	if (!nm_setting_802_1x_set_ca_cert (s_8021x, filename, NM_SETTING_802_1X_CK_SCHEME_PATH, &format, &error)) {
		g_warning ("Couldn't read CA certificate '%s': %s", filename, error ? error->message : "(unknown)");
		ca_cert_error = TRUE;
	}
	eap_method_ca_cert_ignore_set (parent, connection, filename, ca_cert_error);

	peapver_active = gtk_combo_box_get_active (self->version_combo);
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

	model = gtk_combo_box_get_model (self->inner_auth_combo);
	gtk_combo_box_get_active_iter (self->inner_auth_combo, &iter);
	gtk_tree_model_get (model, &iter, I_METHOD_COLUMN, &eap, -1);
	g_assert (eap);

	eap_method_fill_connection (eap, connection, flags);
}

static void
inner_auth_combo_changed_cb (EAPMethodPEAP *self)
{
	g_autoptr(EAPMethod) eap = NULL;
	GList *elt, *children;
	GtkTreeModel *model;
	GtkTreeIter iter;
	GtkWidget *eap_widget;

	/* Remove any previous wireless security widgets */
	children = gtk_container_get_children (GTK_CONTAINER (self->inner_auth_box));
	for (elt = children; elt; elt = g_list_next (elt))
		gtk_container_remove (GTK_CONTAINER (self->inner_auth_box), GTK_WIDGET (elt->data));

	model = gtk_combo_box_get_model (self->inner_auth_combo);
	gtk_combo_box_get_active_iter (self->inner_auth_combo, &iter);
	gtk_tree_model_get (model, &iter, I_METHOD_COLUMN, &eap, -1);
	g_assert (eap);

	eap_widget = eap_method_get_widget (eap);
	g_assert (eap_widget);
	gtk_widget_unparent (eap_widget);

	if (self->size_group)
		eap_method_add_to_size_group (eap, self->size_group);
	gtk_container_add (GTK_CONTAINER (self->inner_auth_box), eap_widget);

	wireless_security_notify_changed (self->sec_parent);
}

static void
inner_auth_combo_init (EAPMethodPEAP *self,
                       NMConnection *connection,
                       NMSetting8021x *s_8021x,
                       gboolean secrets_only)
{
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
	if (self->is_editor)
		simple_flags |= EAP_METHOD_SIMPLE_FLAG_IS_EDITOR;
	if (secrets_only)
		simple_flags |= EAP_METHOD_SIMPLE_FLAG_SECRETS_ONLY;

	em_mschap_v2 = eap_method_simple_new (self->sec_parent,
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

	em_md5 = eap_method_simple_new (self->sec_parent,
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

	em_gtc = eap_method_simple_new (self->sec_parent,
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

	gtk_combo_box_set_model (self->inner_auth_combo, GTK_TREE_MODEL (auth_model));
	gtk_combo_box_set_active (self->inner_auth_combo, active);

	g_signal_connect_swapped (self->inner_auth_combo, "changed", G_CALLBACK (inner_auth_combo_changed_cb), self);
}

static void
update_secrets (EAPMethod *parent, NMConnection *connection)
{
	EAPMethodPEAP *self = (EAPMethodPEAP *) parent;
	eap_method_phase2_update_secrets_helper (parent,
	                                         connection,
	                                         self->inner_auth_combo,
	                                         I_METHOD_COLUMN);
}

static void
changed_cb (EAPMethodPEAP *self)
{
	wireless_security_notify_changed (self->sec_parent);
}

EAPMethodPEAP *
eap_method_peap_new (WirelessSecurity *ws_parent,
                     NMConnection *connection,
                     gboolean is_editor,
                     gboolean secrets_only)
{
	EAPMethod *parent;
	EAPMethodPEAP *self;
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
	                          "grid",
	                          "anon_identity_entry",
	                          FALSE);
	if (!parent)
		return NULL;

	parent->password_flags_name = NM_SETTING_802_1X_PASSWORD;
	self = (EAPMethodPEAP *) parent;
	self->sec_parent = ws_parent;
	self->is_editor = is_editor;

	self->anon_identity_entry = GTK_ENTRY (gtk_builder_get_object (parent->builder, "anon_identity_entry"));
	self->anon_identity_label = GTK_LABEL (gtk_builder_get_object (parent->builder, "anon_identity_label"));
	self->ca_cert_button = GTK_FILE_CHOOSER_BUTTON (gtk_builder_get_object (parent->builder, "ca_cert_button"));
	self->ca_cert_label = GTK_LABEL (gtk_builder_get_object (parent->builder, "ca_cert_label"));
	self->ca_cert_not_required_check = GTK_CHECK_BUTTON (gtk_builder_get_object (parent->builder, "ca_cert_not_required_check"));
	self->inner_auth_combo = GTK_COMBO_BOX (gtk_builder_get_object (parent->builder, "inner_auth_combo"));
	self->inner_auth_label = GTK_LABEL (gtk_builder_get_object (parent->builder, "inner_auth_label"));
	self->inner_auth_box = GTK_BOX (gtk_builder_get_object (parent->builder, "inner_auth_box"));
	self->version_combo = GTK_COMBO_BOX (gtk_builder_get_object (parent->builder, "version_combo"));
	self->version_label = GTK_LABEL (gtk_builder_get_object (parent->builder, "version_label"));

	if (connection)
		s_8021x = nm_connection_get_setting_802_1x (connection);

	g_signal_connect_swapped (self->ca_cert_not_required_check, "toggled", G_CALLBACK (ca_cert_not_required_toggled), self);

	gtk_file_chooser_set_local_only (GTK_FILE_CHOOSER (self->ca_cert_button), TRUE);
	gtk_file_chooser_button_set_title (self->ca_cert_button,
	                                   _("Choose a Certificate Authority certificate"));
	g_signal_connect_swapped (self->ca_cert_button, "selection-changed", G_CALLBACK (changed_cb), self);
	filter = eap_method_default_file_chooser_filter_new (FALSE);
	gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (self->ca_cert_button), filter);
	if (connection && s_8021x) {
		filename = NULL;
		if (nm_setting_802_1x_get_ca_cert_scheme (s_8021x) == NM_SETTING_802_1X_CK_SCHEME_PATH) {
			filename = nm_setting_802_1x_get_ca_cert_path (s_8021x);
			if (filename)
				gtk_file_chooser_set_filename (GTK_FILE_CHOOSER (self->ca_cert_button), filename);
		}
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (self->ca_cert_not_required_check),
		                              !filename && eap_method_ca_cert_ignore_get (parent, connection));
	}

	inner_auth_combo_init (self, connection, s_8021x, secrets_only);
	inner_auth_combo_changed_cb (self);

	gtk_combo_box_set_active (self->version_combo, 0);
	if (s_8021x) {
		const char *peapver;

		peapver = nm_setting_802_1x_get_phase1_peapver (s_8021x);
		if (peapver) {
			/* Index 0 is "Automatic" */
			if (!strcmp (peapver, "0"))
				gtk_combo_box_set_active (self->version_combo, 1);
			else if (!strcmp (peapver, "1"))
				gtk_combo_box_set_active (self->version_combo, 2);
		}
	}
	g_signal_connect_swapped (self->version_combo, "changed", G_CALLBACK (changed_cb), self);

	if (s_8021x && nm_setting_802_1x_get_anonymous_identity (s_8021x))
		gtk_entry_set_text (self->anon_identity_entry, nm_setting_802_1x_get_anonymous_identity (s_8021x));
	g_signal_connect_swapped (self->anon_identity_entry, "changed", G_CALLBACK (changed_cb), self);

	if (secrets_only) {
		gtk_widget_hide (GTK_WIDGET (self->anon_identity_label));
		gtk_widget_hide (GTK_WIDGET (self->anon_identity_entry));
		gtk_widget_hide (GTK_WIDGET (self->ca_cert_label));
		gtk_widget_hide (GTK_WIDGET (self->ca_cert_button));
		gtk_widget_hide (GTK_WIDGET (self->ca_cert_not_required_check));
		gtk_widget_hide (GTK_WIDGET (self->inner_auth_label));
		gtk_widget_hide (GTK_WIDGET (self->inner_auth_combo));
		gtk_widget_hide (GTK_WIDGET (self->version_label));
		gtk_widget_hide (GTK_WIDGET (self->version_combo));
	}

	return self;
}

