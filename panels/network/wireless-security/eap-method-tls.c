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
#include "eap-method-tls.h"
#include "helpers.h"
#include "nma-ui-utils.h"
#include "ui-helpers.h"

struct _EAPMethodTLS {
	GtkGrid parent;

	GtkFileChooserButton *ca_cert_button;
	GtkLabel             *ca_cert_label;
	GtkCheckButton       *ca_cert_not_required_check;
	GtkEntry             *identity_entry;
	GtkLabel             *identity_label;
	GtkFileChooserButton *private_key_button;
	GtkLabel             *private_key_label;
	GtkEntry             *private_key_password_entry;
	GtkLabel             *private_key_password_label;
	GtkCheckButton       *show_password_check;
	GtkFileChooserButton *user_cert_button;
	GtkLabel             *user_cert_label;

	gchar *username;
	gchar *password;
	gboolean show_password;
};

static void eap_method_iface_init (EAPMethodInterface *);

G_DEFINE_TYPE_WITH_CODE (EAPMethodTLS, eap_method_tls, GTK_TYPE_GRID,
                         G_IMPLEMENT_INTERFACE (eap_method_get_type (), eap_method_iface_init))

static void
eap_method_tls_dispose (GObject *object)
{
	EAPMethodTLS *self = EAP_METHOD_TLS (object);

	g_clear_pointer (&self->username, g_free);
	g_clear_pointer (&self->password, g_free);

	G_OBJECT_CLASS (eap_method_tls_parent_class)->dispose (object);
}

static void
show_toggled_cb (EAPMethodTLS *self)
{
	gboolean visible;

	visible = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (self->show_password_check));
	gtk_entry_set_visibility (self->private_key_password_entry, visible);
}

static gboolean
validate (EAPMethod *method, GError **error)
{
	EAPMethodTLS *self = EAP_METHOD_TLS (method);
	NMSetting8021xCKFormat format = NM_SETTING_802_1X_CK_FORMAT_UNKNOWN;
	NMSettingSecretFlags secret_flags;
	const char *password, *identity;
	g_autoptr(GError) ca_cert_error = NULL;
	g_autoptr(GError) private_key_error = NULL;
	g_autoptr(GError) user_cert_error = NULL;
	gboolean ret = TRUE;

	identity = gtk_entry_get_text (self->identity_entry);
	if (!identity || !strlen (identity)) {
		widget_set_error (GTK_WIDGET (self->identity_entry));
		g_set_error_literal (error, NMA_ERROR, NMA_ERROR_GENERIC, _("missing EAP-TLS identity"));
		ret = FALSE;
	} else {
		widget_unset_error (GTK_WIDGET (self->identity_entry));
	}

	if (!eap_method_validate_filepicker (GTK_FILE_CHOOSER (self->ca_cert_button),
	                                     TYPE_CA_CERT, NULL, NULL, &ca_cert_error)) {
		widget_set_error (GTK_WIDGET (self->ca_cert_button));
		if (ret) {
			g_set_error (error, NMA_ERROR, NMA_ERROR_GENERIC, _("invalid EAP-TLS CA certificate: %s"), ca_cert_error->message);
			ret = FALSE;
		}
	} else if (!gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (self->ca_cert_not_required_check))) {
		g_autofree gchar *filename = NULL;

		filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (self->ca_cert_button));
		if (filename == NULL) {
			widget_set_error (GTK_WIDGET (self->ca_cert_button));
			if (ret) {
				g_set_error_literal (error, NMA_ERROR, NMA_ERROR_GENERIC, _("invalid EAP-TLS CA certificate: no certificate specified"));
				ret = FALSE;
			}
		}
	}

	password = gtk_entry_get_text (self->private_key_password_entry);
	secret_flags = nma_utils_menu_to_secret_flags (GTK_WIDGET (self->private_key_password_entry));
	if (secret_flags & NM_SETTING_SECRET_FLAG_NOT_SAVED)
		password = NULL;

	if (!eap_method_validate_filepicker (GTK_FILE_CHOOSER (self->private_key_button),
	                                     TYPE_PRIVATE_KEY,
	                                     password,
	                                     &format,
	                                     &private_key_error)) {
		if (ret) {
			g_set_error (error, NMA_ERROR, NMA_ERROR_GENERIC, _("invalid EAP-TLS private-key: %s"), private_key_error->message);
			ret = FALSE;
		}
		widget_set_error (GTK_WIDGET (self->private_key_button));
	}

	if (format != NM_SETTING_802_1X_CK_FORMAT_PKCS12) {
		if (!eap_method_validate_filepicker (GTK_FILE_CHOOSER (self->user_cert_button),
		                                     TYPE_CLIENT_CERT, NULL, NULL, &user_cert_error)) {
			if (ret) {
				g_set_error (error, NMA_ERROR, NMA_ERROR_GENERIC, _("invalid EAP-TLS user-certificate: %s"), user_cert_error->message);
				ret = FALSE;
			}
			widget_set_error (GTK_WIDGET (self->user_cert_button));
		}
	}

	return ret;
}

static void
ca_cert_not_required_toggled (EAPMethodTLS *self)
{
	eap_method_ca_cert_not_required_toggled (GTK_TOGGLE_BUTTON (self->ca_cert_not_required_check),
	                                         GTK_FILE_CHOOSER (self->ca_cert_button));
	eap_method_emit_changed (EAP_METHOD (self));
}

static void
add_to_size_group (EAPMethod *method, GtkSizeGroup *group)
{
	EAPMethodTLS *self = EAP_METHOD_TLS (method);

	gtk_size_group_add_widget (group, GTK_WIDGET (self->ca_cert_not_required_check));
	gtk_size_group_add_widget (group, GTK_WIDGET (self->identity_label));
	gtk_size_group_add_widget (group, GTK_WIDGET (self->user_cert_label));
	gtk_size_group_add_widget (group, GTK_WIDGET (self->ca_cert_label));
	gtk_size_group_add_widget (group, GTK_WIDGET (self->private_key_label));
	gtk_size_group_add_widget (group, GTK_WIDGET (self->private_key_password_label));
}

static void
fill_connection (EAPMethod *method, NMConnection *connection, NMSettingSecretFlags flags)
{
	EAPMethodTLS *self = EAP_METHOD_TLS (method);
	NMSetting8021xCKFormat format = NM_SETTING_802_1X_CK_FORMAT_UNKNOWN;
	NMSetting8021x *s_8021x;
	NMSettingSecretFlags secret_flags;
	g_autofree gchar *ca_filename = NULL;
	g_autofree gchar *pk_filename = NULL;
	const char *password = NULL;
	gboolean ca_cert_error = FALSE;
	g_autoptr(GError) error = NULL;
	g_autoptr(GError) error2 = NULL;

	s_8021x = nm_connection_get_setting_802_1x (connection);
	g_assert (s_8021x);

	nm_setting_802_1x_add_eap_method (s_8021x, "tls");

	g_object_set (s_8021x, NM_SETTING_802_1X_IDENTITY, gtk_entry_get_text (self->identity_entry), NULL);

	/* TLS private key */
	password = gtk_entry_get_text (self->private_key_password_entry);
	secret_flags = nma_utils_menu_to_secret_flags (GTK_WIDGET (self->private_key_password_entry));
	if (secret_flags & NM_SETTING_SECRET_FLAG_NOT_SAVED)
		password = NULL;

	pk_filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (self->private_key_button));
	g_assert (pk_filename);

	if (!nm_setting_802_1x_set_private_key (s_8021x, pk_filename, password, NM_SETTING_802_1X_CK_SCHEME_PATH, &format, &error))
		g_warning ("Couldn't read private key '%s': %s", pk_filename, error ? error->message : "(unknown)");

	/* Save 802.1X password flags to the connection */
	nm_setting_set_secret_flags (NM_SETTING (s_8021x), NM_SETTING_802_1X_PRIVATE_KEY_PASSWORD,
	                             secret_flags, NULL);

	/* Update secret flags and popup when editing the connection */
	nma_utils_update_password_storage (GTK_WIDGET (self->private_key_password_entry), secret_flags,
	                                   NM_SETTING (s_8021x), NM_SETTING_802_1X_PRIVATE_KEY_PASSWORD);

	/* TLS client certificate */
	if (format != NM_SETTING_802_1X_CK_FORMAT_PKCS12) {
		g_autofree gchar *cc_filename = NULL;
		g_autoptr(GError) error = NULL;

		/* If the key is pkcs#12 nm_setting_802_1x_set_private_key() already
		 * set the client certificate for us.
		 */
		cc_filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (self->user_cert_button));
		g_assert (cc_filename);

		format = NM_SETTING_802_1X_CK_FORMAT_UNKNOWN;
		if (!nm_setting_802_1x_set_client_cert (s_8021x, cc_filename, NM_SETTING_802_1X_CK_SCHEME_PATH, &format, &error))
			g_warning ("Couldn't read client certificate '%s': %s", cc_filename, error ? error->message : "(unknown)");
	}

	/* TLS CA certificate */
	ca_filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (self->ca_cert_button));

	format = NM_SETTING_802_1X_CK_FORMAT_UNKNOWN;
	if (!nm_setting_802_1x_set_ca_cert (s_8021x, ca_filename, NM_SETTING_802_1X_CK_SCHEME_PATH, &format, &error2)) {
		g_warning ("Couldn't read CA certificate '%s': %s", ca_filename, error2 ? error2->message : "(unknown)");
		ca_cert_error = TRUE;
	}
	eap_method_ca_cert_ignore_set (method, connection, ca_filename, ca_cert_error);
}

static void
private_key_picker_helper (EAPMethodTLS *self, const char *filename, gboolean changed)
{
	g_autoptr(NMSetting8021x) setting = NULL;
	NMSetting8021xCKFormat cert_format = NM_SETTING_802_1X_CK_FORMAT_UNKNOWN;
	const char *password;

	password = gtk_entry_get_text (self->private_key_password_entry);

	setting = (NMSetting8021x *) nm_setting_802_1x_new ();
	nm_setting_802_1x_set_private_key (setting, filename, password, NM_SETTING_802_1X_CK_SCHEME_PATH, &cert_format, NULL);

	/* With PKCS#12, the client cert must be the same as the private key */
	if (cert_format == NM_SETTING_802_1X_CK_FORMAT_PKCS12) {
		gtk_file_chooser_unselect_all (GTK_FILE_CHOOSER (self->user_cert_button));
		gtk_widget_set_sensitive (GTK_WIDGET (self->user_cert_button), FALSE);
	} else if (changed)
		gtk_widget_set_sensitive (GTK_WIDGET (self->user_cert_button), TRUE);

	/* Warn the user if the private key is unencrypted */
	if (!eap_method_is_encrypted_private_key (filename)) {
		GtkWidget *dialog;
		GtkWidget *toplevel;
		GtkWindow *parent_window = NULL;

		toplevel = gtk_widget_get_toplevel (GTK_WIDGET (self));
		if (gtk_widget_is_toplevel (toplevel))
			parent_window = GTK_WINDOW (toplevel);

		dialog = gtk_message_dialog_new (parent_window,
		                                 GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
		                                 GTK_MESSAGE_WARNING,
		                                 GTK_BUTTONS_OK,
		                                 "%s",
		                                 _("Unencrypted private keys are insecure"));
		gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
		                                          "%s",
		                                          _("The selected private key does not appear to be protected by a password. This could allow your security credentials to be compromised. Please select a password-protected private key.\n\n(You can password-protect your private key with openssl)"));
		gtk_dialog_run (GTK_DIALOG (dialog));
		gtk_widget_destroy (dialog);
	}
}

static void
private_key_picker_file_set_cb (GtkWidget *chooser, gpointer user_data)
{
	EAPMethodTLS *self = user_data;
	g_autofree gchar *filename = NULL;

	filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (chooser));
	if (filename)
		private_key_picker_helper (self, filename, TRUE);
}

static void reset_filter (GtkWidget *widget, GParamSpec *spec, gpointer user_data)
{
	if (!gtk_file_chooser_get_filter (GTK_FILE_CHOOSER (widget))) {
		g_signal_handlers_block_by_func (widget, reset_filter, user_data);
		gtk_file_chooser_set_filter (GTK_FILE_CHOOSER (widget), GTK_FILE_FILTER (user_data));
		g_signal_handlers_unblock_by_func (widget, reset_filter, user_data);
	}
}

typedef const char * (*PathFunc) (NMSetting8021x *setting);
typedef NMSetting8021xCKScheme (*SchemeFunc)  (NMSetting8021x *setting);

static void
changed_cb (EAPMethodTLS *self)
{
	eap_method_emit_changed (EAP_METHOD (self));
}

static void
setup_filepicker (EAPMethodTLS *self,
                  GtkFileChooserButton *button,
                  const char *title,
                  NMSetting8021x *s_8021x,
                  SchemeFunc scheme_func,
                  PathFunc path_func,
                  gboolean privkey,
                  gboolean client_cert)
{
	GtkFileFilter *filter;
	const char *filename = NULL;

	gtk_file_chooser_set_local_only (GTK_FILE_CHOOSER (button), TRUE);
	gtk_file_chooser_button_set_title (button, title);

	if (s_8021x && path_func && scheme_func) {
		if (scheme_func (s_8021x) == NM_SETTING_802_1X_CK_SCHEME_PATH) {
			filename = path_func (s_8021x);
			if (filename)
				gtk_file_chooser_set_filename (GTK_FILE_CHOOSER (button), filename);
		}
	}

	/* Connect a special handler for private keys to intercept PKCS#12 key types
	 * and desensitize the user cert button.
	 */
	if (privkey) {
		g_signal_connect (button, "selection-changed",
		                  (GCallback) private_key_picker_file_set_cb,
		                  self);
		if (filename)
			private_key_picker_helper (self, filename, FALSE);
	}

	g_signal_connect_swapped (button, "selection-changed", G_CALLBACK (changed_cb), self);

	filter = eap_method_default_file_chooser_filter_new (privkey);
	gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (button), filter);

	/* For some reason, GTK+ calls set_current_filter (..., NULL) from 
	 * gtkfilechooserdefault.c::show_and_select_files_finished_loading() on our
	 * dialog; so force-reset the filter to what we want it to be whenever
	 * it gets cleared.
	 */
	if (client_cert)
		g_signal_connect (button, "notify::filter", (GCallback) reset_filter, filter);
}

static void
update_secrets (EAPMethod *method, NMConnection *connection)
{
	EAPMethodTLS *self = EAP_METHOD_TLS (method);
	NMSetting8021x *s_8021x;
	const char *filename;

	helper_fill_secret_entry (connection,
	                          self->private_key_password_entry,
	                          NM_TYPE_SETTING_802_1X,
	                          (HelperSecretFunc) nm_setting_802_1x_get_private_key_password);

	/* Set the private key filepicker button path if we have a private key */
	s_8021x = nm_connection_get_setting_802_1x (connection);
	if (s_8021x && (nm_setting_802_1x_get_private_key_scheme (s_8021x) == NM_SETTING_802_1X_CK_SCHEME_PATH)) {
		filename = nm_setting_802_1x_get_private_key_path (s_8021x);
		if (filename)
			gtk_file_chooser_set_filename (GTK_FILE_CHOOSER (self->private_key_button), filename);
	}
}

static GtkWidget *
get_default_field (EAPMethod *method)
{
	EAPMethodTLS *self = EAP_METHOD_TLS (method);
	return GTK_WIDGET (self->identity_entry);
}

static const gchar *
get_password_flags_name (EAPMethod *method)
{
	return NM_SETTING_802_1X_PRIVATE_KEY_PASSWORD;
}

static const gchar *
get_username (EAPMethod *method)
{
	EAPMethodTLS *self = EAP_METHOD_TLS (method);
	return self->username;
}

static void
set_username (EAPMethod *method, const gchar *username)
{
	EAPMethodTLS *self = EAP_METHOD_TLS (method);
	g_free (self->username);
	self->username = g_strdup (username);
}

static const gchar *
get_password (EAPMethod *method)
{
	EAPMethodTLS *self = EAP_METHOD_TLS (method);
	return self->password;
}

static void
set_password (EAPMethod *method, const gchar *password)
{
	EAPMethodTLS *self = EAP_METHOD_TLS (method);
	g_free (self->password);
	self->password = g_strdup (password);
}

static const gboolean
get_show_password (EAPMethod *method)
{
	EAPMethodTLS *self = EAP_METHOD_TLS (method);
	return self->show_password;
}

static void
set_show_password (EAPMethod *method, gboolean show_password)
{
	EAPMethodTLS *self = EAP_METHOD_TLS (method);
	self->show_password = show_password;
}

static void
eap_method_tls_init (EAPMethodTLS *self)
{
	gtk_widget_init_template (GTK_WIDGET (self));
	self->username = g_strdup ("");
	self->password = g_strdup ("");
}

static void
eap_method_tls_class_init (EAPMethodTLSClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
        GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

	object_class->dispose = eap_method_tls_dispose;

	gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/ControlCenter/network/eap-method-tls.ui");

	gtk_widget_class_bind_template_child (widget_class, EAPMethodTLS, ca_cert_button);
	gtk_widget_class_bind_template_child (widget_class, EAPMethodTLS, ca_cert_label);
	gtk_widget_class_bind_template_child (widget_class, EAPMethodTLS, ca_cert_not_required_check);
	gtk_widget_class_bind_template_child (widget_class, EAPMethodTLS, identity_entry);
	gtk_widget_class_bind_template_child (widget_class, EAPMethodTLS, identity_label);
	gtk_widget_class_bind_template_child (widget_class, EAPMethodTLS, private_key_button);
	gtk_widget_class_bind_template_child (widget_class, EAPMethodTLS, private_key_label);
	gtk_widget_class_bind_template_child (widget_class, EAPMethodTLS, private_key_password_entry);
	gtk_widget_class_bind_template_child (widget_class, EAPMethodTLS, private_key_password_label);
	gtk_widget_class_bind_template_child (widget_class, EAPMethodTLS, show_password_check);
	gtk_widget_class_bind_template_child (widget_class, EAPMethodTLS, user_cert_button);
	gtk_widget_class_bind_template_child (widget_class, EAPMethodTLS, user_cert_label);
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

EAPMethodTLS *
eap_method_tls_new (NMConnection *connection)
{
	EAPMethodTLS *self;
	NMSetting8021x *s_8021x = NULL;
	gboolean ca_not_required = FALSE;

	self = g_object_new (eap_method_tls_get_type (), NULL);

	if (connection)
		s_8021x = nm_connection_get_setting_802_1x (connection);

	g_signal_connect_swapped (self->ca_cert_not_required_check, "toggled", G_CALLBACK (ca_cert_not_required_toggled), self);

	g_signal_connect_swapped (self->identity_entry, "changed", G_CALLBACK (changed_cb), self);
	if (s_8021x && nm_setting_802_1x_get_identity (s_8021x))
		gtk_entry_set_text (self->identity_entry, nm_setting_802_1x_get_identity (s_8021x));

	setup_filepicker (self,
	                  self->user_cert_button,
	                  _("Choose your personal certificate"),
	                  s_8021x,
	                  nm_setting_802_1x_get_client_cert_scheme,
	                  nm_setting_802_1x_get_client_cert_path,
	                  FALSE, TRUE);
	setup_filepicker (self,
	                  self->ca_cert_button,
	                  _("Choose a Certificate Authority certificate"),
	                  s_8021x,
	                  nm_setting_802_1x_get_ca_cert_scheme,
	                  nm_setting_802_1x_get_ca_cert_path,
	                  FALSE, FALSE);
	setup_filepicker (self,
	                  self->private_key_button,
	                  _("Choose your private key"),
	                  s_8021x,
	                  nm_setting_802_1x_get_private_key_scheme,
	                  nm_setting_802_1x_get_private_key_path,
	                  TRUE, FALSE);

	if (connection && eap_method_ca_cert_ignore_get (EAP_METHOD (self), connection))
		ca_not_required = !gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (self->ca_cert_button));
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (self->ca_cert_not_required_check), ca_not_required);

	/* Fill secrets, if any */
	if (connection)
		update_secrets (EAP_METHOD (self), connection);

	g_signal_connect_swapped (self->private_key_password_entry, "changed", G_CALLBACK (changed_cb), self);

	/* Create password-storage popup menu for password entry under entry's secondary icon */
	nma_utils_setup_password_storage (GTK_WIDGET (self->private_key_password_entry), 0, (NMSetting *) s_8021x, NM_SETTING_802_1X_PRIVATE_KEY_PASSWORD,
	                                  FALSE, FALSE);

	g_signal_connect_swapped (self->show_password_check, "toggled", G_CALLBACK (show_toggled_cb), self);

	return self;
}

