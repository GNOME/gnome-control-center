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
#include "eap-method-ttls.h"
#include "helpers.h"

#define I_NAME_COLUMN 0
#define I_ID_COLUMN   1

struct _EAPMethodTTLS {
	GtkGrid parent;

	GtkEntry             *anon_identity_entry;
	GtkLabel             *anon_identity_label;
	GtkFileChooserButton *ca_cert_button;
	GtkLabel             *ca_cert_label;
	GtkCheckButton       *ca_cert_not_required_check;
	GtkEntry             *domain_match_entry;
	GtkLabel             *domain_match_label;
	GtkComboBox          *inner_auth_combo;
	GtkLabel             *inner_auth_label;
	GtkListStore         *inner_auth_model;
	GtkBox               *inner_auth_box;

	EAPMethodSimple      *em_chap;
	EAPMethodSimple      *em_gtc;
	EAPMethodSimple      *em_md5;
	EAPMethodSimple      *em_mschap;
	EAPMethodSimple      *em_mschap_v2;
	EAPMethodSimple      *em_pap;
	EAPMethodSimple      *em_plain_mschap_v2;
};

static void eap_method_iface_init (EAPMethodInterface *);

G_DEFINE_TYPE_WITH_CODE (EAPMethodTTLS, eap_method_ttls, GTK_TYPE_GRID,
                         G_IMPLEMENT_INTERFACE (eap_method_get_type (), eap_method_iface_init))

static EAPMethod *
get_inner_method (EAPMethodTTLS *self)
{
	GtkTreeIter iter;
	g_autofree gchar *id = NULL;

	if (!gtk_combo_box_get_active_iter (self->inner_auth_combo, &iter))
		return NULL;
	gtk_tree_model_get (GTK_TREE_MODEL (self->inner_auth_model), &iter, I_ID_COLUMN, &id, -1);

	if (strcmp (id, "chap") == 0)
		return EAP_METHOD (self->em_chap);
	if (strcmp (id, "gtc") == 0)
		return EAP_METHOD (self->em_gtc);
	if (strcmp (id, "md5") == 0)
		return EAP_METHOD (self->em_md5);
	if (strcmp (id, "mschap") == 0)
		return EAP_METHOD (self->em_mschap);
	if (strcmp (id, "mschapv2") == 0)
		return EAP_METHOD (self->em_mschap_v2);
	if (strcmp (id, "pap") == 0)
		return EAP_METHOD (self->em_pap);
	if (strcmp (id, "plain_mschapv2") == 0)
		return EAP_METHOD (self->em_plain_mschap_v2);

	return NULL;
}

static gboolean
validate (EAPMethod *method, GError **error)
{
	EAPMethodTTLS *self = EAP_METHOD_TTLS (method);
	g_autoptr(GError) local_error = NULL;

	if (!eap_method_validate_filepicker (GTK_FILE_CHOOSER (self->ca_cert_button),
	                                     TYPE_CA_CERT, NULL, NULL, &local_error)) {
		g_set_error (error, NMA_ERROR, NMA_ERROR_GENERIC, _("invalid EAP-TTLS CA certificate: %s"), local_error->message);
		return FALSE;
	}
	if (!gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (self->ca_cert_not_required_check))) {
		g_autofree gchar *filename = NULL;

		filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (self->ca_cert_button));
		if (filename == NULL) {
			g_set_error_literal (error, NMA_ERROR, NMA_ERROR_GENERIC, _("invalid EAP-TTLS CA certificate: no certificate specified"));
			return FALSE;
		}
	}

	return eap_method_validate (get_inner_method (self), error);
}

static void
ca_cert_not_required_toggled (EAPMethodTTLS *self)
{
	eap_method_ca_cert_not_required_toggled (GTK_TOGGLE_BUTTON (self->ca_cert_not_required_check),
	                                         GTK_FILE_CHOOSER (self->ca_cert_button));
	eap_method_emit_changed (EAP_METHOD (self));
}

static void
add_to_size_group (EAPMethod *method, GtkSizeGroup *group)
{
	EAPMethodTTLS *self = EAP_METHOD_TTLS (method);

	gtk_size_group_add_widget (group, GTK_WIDGET (self->ca_cert_not_required_check));
	gtk_size_group_add_widget (group, GTK_WIDGET (self->anon_identity_label));
	gtk_size_group_add_widget (group, GTK_WIDGET (self->domain_match_label));
	gtk_size_group_add_widget (group, GTK_WIDGET (self->ca_cert_label));
	gtk_size_group_add_widget (group, GTK_WIDGET (self->inner_auth_label));

	eap_method_add_to_size_group (EAP_METHOD (self->em_chap), group);
	eap_method_add_to_size_group (EAP_METHOD (self->em_gtc), group);
	eap_method_add_to_size_group (EAP_METHOD (self->em_md5), group);
	eap_method_add_to_size_group (EAP_METHOD (self->em_mschap), group);
	eap_method_add_to_size_group (EAP_METHOD (self->em_mschap_v2), group);
	eap_method_add_to_size_group (EAP_METHOD (self->em_pap), group);
	eap_method_add_to_size_group (EAP_METHOD (self->em_plain_mschap_v2), group);
}

static void
fill_connection (EAPMethod *method, NMConnection *connection, NMSettingSecretFlags flags)
{
	EAPMethodTTLS *self = EAP_METHOD_TTLS (method);
	NMSetting8021x *s_8021x;
	NMSetting8021xCKFormat format = NM_SETTING_802_1X_CK_FORMAT_UNKNOWN;
	const char *text;
	g_autofree gchar *filename = NULL;
	g_autoptr(GError) error = NULL;
	gboolean ca_cert_error = FALSE;

	s_8021x = nm_connection_get_setting_802_1x (connection);
	g_assert (s_8021x);

	nm_setting_802_1x_add_eap_method (s_8021x, "ttls");

	text = gtk_entry_get_text (self->anon_identity_entry);
	if (text && strlen (text))
		g_object_set (s_8021x, NM_SETTING_802_1X_ANONYMOUS_IDENTITY, text, NULL);

	text = gtk_entry_get_text (self->domain_match_entry);
	if (text && strlen (text))
		g_object_set (s_8021x, NM_SETTING_802_1X_DOMAIN_SUFFIX_MATCH, text, NULL);

	filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (self->ca_cert_button));
	if (!nm_setting_802_1x_set_ca_cert (s_8021x, filename, NM_SETTING_802_1X_CK_SCHEME_PATH, &format, &error)) {
		g_warning ("Couldn't read CA certificate '%s': %s", filename, error ? error->message : "(unknown)");
		ca_cert_error = TRUE;
	}
	eap_method_ca_cert_ignore_set (method, connection, filename, ca_cert_error);

	eap_method_fill_connection (get_inner_method (self), connection, flags);
}

static void
inner_auth_combo_changed_cb (EAPMethodTTLS *self)
{
	EAPMethod *inner_method;
	GList *children;

	inner_method = get_inner_method (self);

	/* Remove the previous method and migrate username/password across */
	children = gtk_container_get_children (GTK_CONTAINER (self->inner_auth_box));
	if (children != NULL) {
		EAPMethod *old_eap = g_list_nth_data (children, 0);
		eap_method_set_username (inner_method, eap_method_get_username (old_eap));
		eap_method_set_password (inner_method, eap_method_get_password (old_eap));
		eap_method_set_show_password (inner_method, eap_method_get_show_password (old_eap));
		gtk_container_remove (GTK_CONTAINER (self->inner_auth_box), GTK_WIDGET (old_eap));
	}

	gtk_container_add (GTK_CONTAINER (self->inner_auth_box), g_object_ref (GTK_WIDGET (inner_method)));

	eap_method_emit_changed (EAP_METHOD (self));
}

static void
update_secrets (EAPMethod *method, NMConnection *connection)
{
	EAPMethodTTLS *self = EAP_METHOD_TTLS (method);

	eap_method_update_secrets (EAP_METHOD (self->em_chap), connection);
	eap_method_update_secrets (EAP_METHOD (self->em_gtc), connection);
	eap_method_update_secrets (EAP_METHOD (self->em_md5), connection);
	eap_method_update_secrets (EAP_METHOD (self->em_mschap), connection);
	eap_method_update_secrets (EAP_METHOD (self->em_mschap_v2), connection);
	eap_method_update_secrets (EAP_METHOD (self->em_pap), connection);
	eap_method_update_secrets (EAP_METHOD (self->em_plain_mschap_v2), connection);
}

static GtkWidget *
get_default_field (EAPMethod *method)
{
	EAPMethodTTLS *self = EAP_METHOD_TTLS (method);
	return GTK_WIDGET (self->anon_identity_entry);
}

static const gchar *
get_password_flags_name (EAPMethod *method)
{
	return NM_SETTING_802_1X_PASSWORD;
}

static const gchar *
get_username (EAPMethod *method)
{
	EAPMethodTTLS *self = EAP_METHOD_TTLS (method);
	return eap_method_get_username (get_inner_method (self));
}

static void
set_username (EAPMethod *method, const gchar *username)
{
	EAPMethodTTLS *self = EAP_METHOD_TTLS (method);
	return eap_method_set_username (get_inner_method (self), username);
}

static const gchar *
get_password (EAPMethod *method)
{
	EAPMethodTTLS *self = EAP_METHOD_TTLS (method);
	return eap_method_get_password (get_inner_method (self));
}

static void
set_password (EAPMethod *method, const gchar *password)
{
	EAPMethodTTLS *self = EAP_METHOD_TTLS (method);
	return eap_method_set_password (get_inner_method (self), password);
}

static gboolean
get_show_password (EAPMethod *method)
{
	EAPMethodTTLS *self = EAP_METHOD_TTLS (method);
	return eap_method_get_show_password (get_inner_method (self));
}

static void
set_show_password (EAPMethod *method, gboolean show_password)
{
	EAPMethodTTLS *self = EAP_METHOD_TTLS (method);
	return eap_method_set_show_password (get_inner_method (self), show_password);
}

static void
changed_cb (EAPMethodTTLS *self)
{
	eap_method_emit_changed (EAP_METHOD (self));
}

static void
eap_method_ttls_init (EAPMethodTTLS *self)
{
	gtk_widget_init_template (GTK_WIDGET (self));
}

static void
eap_method_ttls_class_init (EAPMethodTTLSClass *klass)
{
        GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

	gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/ControlCenter/network/eap-method-ttls.ui");

	gtk_widget_class_bind_template_child (widget_class, EAPMethodTTLS, anon_identity_entry);
	gtk_widget_class_bind_template_child (widget_class, EAPMethodTTLS, anon_identity_label);
	gtk_widget_class_bind_template_child (widget_class, EAPMethodTTLS, ca_cert_button);
	gtk_widget_class_bind_template_child (widget_class, EAPMethodTTLS, ca_cert_label);
	gtk_widget_class_bind_template_child (widget_class, EAPMethodTTLS, ca_cert_not_required_check);
	gtk_widget_class_bind_template_child (widget_class, EAPMethodTTLS, domain_match_entry);
	gtk_widget_class_bind_template_child (widget_class, EAPMethodTTLS, domain_match_label);
	gtk_widget_class_bind_template_child (widget_class, EAPMethodTTLS, inner_auth_combo);
	gtk_widget_class_bind_template_child (widget_class, EAPMethodTTLS, inner_auth_label);
	gtk_widget_class_bind_template_child (widget_class, EAPMethodTTLS, inner_auth_model);
	gtk_widget_class_bind_template_child (widget_class, EAPMethodTTLS, inner_auth_box);
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

EAPMethodTTLS *
eap_method_ttls_new (NMConnection *connection)
{
	EAPMethodTTLS *self;
	GtkFileFilter *filter;
	NMSetting8021x *s_8021x = NULL;
	const char *filename;
	const char *phase2_auth = NULL;
	GtkTreeIter iter;

	self = g_object_new (eap_method_ttls_get_type (), NULL);

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
		                              !filename && eap_method_ca_cert_ignore_get (EAP_METHOD (self), connection));
	}

	if (s_8021x && nm_setting_802_1x_get_anonymous_identity (s_8021x))
		gtk_entry_set_text (self->anon_identity_entry, nm_setting_802_1x_get_anonymous_identity (s_8021x));
	g_signal_connect_swapped (self->anon_identity_entry, "changed", G_CALLBACK (changed_cb), self);
	if (s_8021x && nm_setting_802_1x_get_domain_suffix_match (s_8021x))
		gtk_entry_set_text (self->domain_match_entry, nm_setting_802_1x_get_domain_suffix_match (s_8021x));
	g_signal_connect_swapped (self->domain_match_entry, "changed", G_CALLBACK (changed_cb), self);

	self->em_pap = eap_method_simple_new (connection, "pap", TRUE, FALSE);
	gtk_widget_show (GTK_WIDGET (self->em_pap));
	g_signal_connect_object (self->em_pap, "changed", G_CALLBACK (eap_method_emit_changed), self, G_CONNECT_SWAPPED);

	self->em_mschap = eap_method_simple_new (connection, "mschap", TRUE, FALSE);
	gtk_widget_show (GTK_WIDGET (self->em_mschap));
	g_signal_connect_object (self->em_mschap, "changed", G_CALLBACK (eap_method_emit_changed), self, G_CONNECT_SWAPPED);

	self->em_mschap_v2 = eap_method_simple_new (connection, "mschapv2", TRUE, TRUE);
	gtk_widget_show (GTK_WIDGET (self->em_mschap_v2));
	g_signal_connect_object (self->em_mschap_v2, "changed", G_CALLBACK (eap_method_emit_changed), self, G_CONNECT_SWAPPED);

	self->em_plain_mschap_v2 = eap_method_simple_new (connection, "mschapv2", TRUE, FALSE);
	gtk_widget_show (GTK_WIDGET (self->em_plain_mschap_v2));
	g_signal_connect_object (self->em_plain_mschap_v2, "changed", G_CALLBACK (eap_method_emit_changed), self, G_CONNECT_SWAPPED);

	self->em_chap = eap_method_simple_new (connection, "chap", TRUE, FALSE);
	gtk_widget_show (GTK_WIDGET (self->em_chap));
	g_signal_connect_object (self->em_chap, "changed", G_CALLBACK (eap_method_emit_changed), self, G_CONNECT_SWAPPED);

	self->em_md5 = eap_method_simple_new (connection, "md5", TRUE, TRUE);
	gtk_widget_show (GTK_WIDGET (self->em_md5));
	g_signal_connect_object (self->em_md5, "changed", G_CALLBACK (eap_method_emit_changed), self, G_CONNECT_SWAPPED);

	self->em_gtc = eap_method_simple_new (connection, "gtc", TRUE, TRUE);
	gtk_widget_show (GTK_WIDGET (self->em_gtc));
	g_signal_connect_object (self->em_gtc, "changed", G_CALLBACK (eap_method_emit_changed), self, G_CONNECT_SWAPPED);

	if (s_8021x) {
		if (nm_setting_802_1x_get_phase2_auth (s_8021x))
			phase2_auth = nm_setting_802_1x_get_phase2_auth (s_8021x);
		else if (nm_setting_802_1x_get_phase2_autheap (s_8021x))
			phase2_auth = nm_setting_802_1x_get_phase2_autheap (s_8021x);
	}
	if (phase2_auth == NULL)
		phase2_auth = "pap";

	if (strcmp (phase2_auth, "mschapv2") == 0 && nm_setting_802_1x_get_phase2_auth (s_8021x) != NULL)
		phase2_auth = "plain_mschapv2";

	if (gtk_tree_model_get_iter_first (GTK_TREE_MODEL (self->inner_auth_model), &iter)) {
		do {
			g_autofree gchar *id = NULL;
			gtk_tree_model_get (GTK_TREE_MODEL (self->inner_auth_model), &iter, I_ID_COLUMN, &id, -1);
			if (strcmp (id, phase2_auth) == 0)
				gtk_combo_box_set_active_iter (self->inner_auth_combo, &iter);
		} while (gtk_tree_model_iter_next (GTK_TREE_MODEL (self->inner_auth_model), &iter));
	}

	g_signal_connect_swapped (self->inner_auth_combo, "changed", G_CALLBACK (inner_auth_combo_changed_cb), self);
	inner_auth_combo_changed_cb (self);

	return self;
}

