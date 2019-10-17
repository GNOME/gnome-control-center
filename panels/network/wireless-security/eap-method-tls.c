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
#include "helpers.h"
#include "nma-ui-utils.h"
#include "utils.h"

struct _EAPMethodTLS {
	EAPMethod parent;

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

	WirelessSecurity *sec_parent;
	gboolean editing_connection;
};


static void
show_toggled_cb (EAPMethodTLS *self)
{
	gboolean visible;

	visible = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (self->show_password_check));
	gtk_entry_set_visibility (self->private_key_password_entry, visible);
}

static gboolean
validate (EAPMethod *parent, GError **error)
{
	EAPMethodTLS *self = (EAPMethodTLS *) parent;
	NMSetting8021xCKFormat format = NM_SETTING_802_1X_CK_FORMAT_UNKNOWN;
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
	} else if (eap_method_ca_cert_required (GTK_TOGGLE_BUTTON (self->ca_cert_not_required_check),
	                                        GTK_FILE_CHOOSER (self->ca_cert_button))) {
		widget_set_error (GTK_WIDGET (self->ca_cert_button));
		if (ret) {
			g_set_error_literal (error, NMA_ERROR, NMA_ERROR_GENERIC, _("invalid EAP-TLS CA certificate: no certificate specified"));
			ret = FALSE;
		}
	}

	password = gtk_entry_get_text (self->private_key_password_entry);

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
	wireless_security_notify_changed (self->sec_parent);
}

static void
add_to_size_group (EAPMethod *parent, GtkSizeGroup *group)
{
	EAPMethodTLS *self = (EAPMethodTLS *) parent;

	gtk_size_group_add_widget (group, GTK_WIDGET (self->ca_cert_not_required_check));
	gtk_size_group_add_widget (group, GTK_WIDGET (self->identity_label));
	gtk_size_group_add_widget (group, GTK_WIDGET (self->user_cert_label));
	gtk_size_group_add_widget (group, GTK_WIDGET (self->ca_cert_label));
	gtk_size_group_add_widget (group, GTK_WIDGET (self->private_key_label));
	gtk_size_group_add_widget (group, GTK_WIDGET (self->private_key_password_label));
}

static void
fill_connection (EAPMethod *parent, NMConnection *connection, NMSettingSecretFlags flags)
{
	EAPMethodTLS *self = (EAPMethodTLS *) parent;
	NMSetting8021xCKFormat format = NM_SETTING_802_1X_CK_FORMAT_UNKNOWN;
	NMSetting8021x *s_8021x;
	NMSettingSecretFlags secret_flags;
	g_autofree gchar *ca_filename = NULL;
	g_autofree gchar *pk_filename = NULL;
	const char *password = NULL;
	gboolean ca_cert_error = FALSE;

	s_8021x = nm_connection_get_setting_802_1x (connection);
	g_assert (s_8021x);

	if (parent->phase2)
		g_object_set (s_8021x, NM_SETTING_802_1X_PHASE2_AUTH, "tls", NULL);
	else
		nm_setting_802_1x_add_eap_method (s_8021x, "tls");

	g_object_set (s_8021x, NM_SETTING_802_1X_IDENTITY, gtk_entry_get_text (self->identity_entry), NULL);

	/* TLS private key */
	password = gtk_entry_get_text (self->private_key_password_entry);

	pk_filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (self->private_key_button));
	g_assert (pk_filename);

	if (parent->phase2) {
		g_autoptr(GError) error = NULL;
		if (!nm_setting_802_1x_set_phase2_private_key (s_8021x, pk_filename, password, NM_SETTING_802_1X_CK_SCHEME_PATH, &format, &error))
			g_warning ("Couldn't read phase2 private key '%s': %s", pk_filename, error ? error->message : "(unknown)");
	} else {
		g_autoptr(GError) error = NULL;
		if (!nm_setting_802_1x_set_private_key (s_8021x, pk_filename, password, NM_SETTING_802_1X_CK_SCHEME_PATH, &format, &error))
			g_warning ("Couldn't read private key '%s': %s", pk_filename, error ? error->message : "(unknown)");
	}

	/* Save 802.1X password flags to the connection */
	secret_flags = nma_utils_menu_to_secret_flags (GTK_WIDGET (self->private_key_password_entry));
	nm_setting_set_secret_flags (NM_SETTING (s_8021x), parent->password_flags_name,
	                             secret_flags, NULL);

	/* Update secret flags and popup when editing the connection */
	if (self->editing_connection) {
		nma_utils_update_password_storage (GTK_WIDGET (self->private_key_password_entry), secret_flags,
		                                   NM_SETTING (s_8021x), parent->password_flags_name);
	}

	/* TLS client certificate */
	if (format != NM_SETTING_802_1X_CK_FORMAT_PKCS12) {
		g_autofree gchar *cc_filename = NULL;

		/* If the key is pkcs#12 nm_setting_802_1x_set_private_key() already
		 * set the client certificate for us.
		 */
		cc_filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (self->user_cert_button));
		g_assert (cc_filename);

		format = NM_SETTING_802_1X_CK_FORMAT_UNKNOWN;
		if (parent->phase2) {
			g_autoptr(GError) error = NULL;
			if (!nm_setting_802_1x_set_phase2_client_cert (s_8021x, cc_filename, NM_SETTING_802_1X_CK_SCHEME_PATH, &format, &error))
				g_warning ("Couldn't read phase2 client certificate '%s': %s", cc_filename, error ? error->message : "(unknown)");
		} else {
			g_autoptr(GError) error = NULL;
			if (!nm_setting_802_1x_set_client_cert (s_8021x, cc_filename, NM_SETTING_802_1X_CK_SCHEME_PATH, &format, &error))
				g_warning ("Couldn't read client certificate '%s': %s", cc_filename, error ? error->message : "(unknown)");
		}
	}

	/* TLS CA certificate */
	ca_filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (self->ca_cert_button));

	format = NM_SETTING_802_1X_CK_FORMAT_UNKNOWN;
	if (parent->phase2) {
		g_autoptr(GError) error = NULL;
		if (!nm_setting_802_1x_set_phase2_ca_cert (s_8021x, ca_filename, NM_SETTING_802_1X_CK_SCHEME_PATH, &format, &error)) {
			g_warning ("Couldn't read phase2 CA certificate '%s': %s", ca_filename, error ? error->message : "(unknown)");
			ca_cert_error = TRUE;
		}
	} else {
		g_autoptr(GError) error = NULL;
		if (!nm_setting_802_1x_set_ca_cert (s_8021x, ca_filename, NM_SETTING_802_1X_CK_SCHEME_PATH, &format, &error)) {
			g_warning ("Couldn't read CA certificate '%s': %s", ca_filename, error ? error->message : "(unknown)");
			ca_cert_error = TRUE;
		}
	}
	eap_method_ca_cert_ignore_set (parent, connection, ca_filename, ca_cert_error);
}

static void
private_key_picker_helper (EAPMethod *parent, const char *filename, gboolean changed)
{
	EAPMethodTLS *self = (EAPMethodTLS *) parent;
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

		toplevel = gtk_widget_get_toplevel (parent->ui_widget);
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
	EAPMethod *parent = (EAPMethod *) user_data;
	g_autofree gchar *filename = NULL;

	filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (chooser));
	if (filename)
		private_key_picker_helper (parent, filename, TRUE);
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
	wireless_security_notify_changed (self->sec_parent);
}

static void
setup_filepicker (GtkFileChooserButton *button,
                  const char *title,
                  WirelessSecurity *ws_parent,
                  EAPMethod *parent,
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
		                  parent);
		if (filename)
			private_key_picker_helper (parent, filename, FALSE);
	}

	g_signal_connect_swapped (button, "selection-changed", G_CALLBACK (changed_cb), parent);

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
update_secrets (EAPMethod *parent, NMConnection *connection)
{
	EAPMethodTLS *self = (EAPMethodTLS *) parent;
	NMSetting8021x *s_8021x;
	HelperSecretFunc password_func;
	SchemeFunc scheme_func;
	PathFunc path_func;
	const char *filename;

	if (parent->phase2) {
		password_func = (HelperSecretFunc) nm_setting_802_1x_get_phase2_private_key_password;
		scheme_func = nm_setting_802_1x_get_phase2_private_key_scheme;
		path_func = nm_setting_802_1x_get_phase2_private_key_path;
	} else {
		password_func = (HelperSecretFunc) nm_setting_802_1x_get_private_key_password;
		scheme_func = nm_setting_802_1x_get_private_key_scheme;
		path_func = nm_setting_802_1x_get_private_key_path;
	}

	helper_fill_secret_entry (connection,
	                          self->private_key_password_entry,
	                          NM_TYPE_SETTING_802_1X,
	                          password_func);

	/* Set the private key filepicker button path if we have a private key */
	s_8021x = nm_connection_get_setting_802_1x (connection);
	if (s_8021x && (scheme_func (s_8021x) == NM_SETTING_802_1X_CK_SCHEME_PATH)) {
		filename = path_func (s_8021x);
		if (filename)
			gtk_file_chooser_set_filename (GTK_FILE_CHOOSER (self->private_key_button), filename);
	}
}

EAPMethodTLS *
eap_method_tls_new (WirelessSecurity *ws_parent,
                    NMConnection *connection,
                    gboolean phase2,
                    gboolean secrets_only)
{
	EAPMethodTLS *self;
	EAPMethod *parent;
	NMSetting8021x *s_8021x = NULL;
	gboolean ca_not_required = FALSE;

	parent = eap_method_init (sizeof (EAPMethodTLS),
	                          validate,
	                          add_to_size_group,
	                          fill_connection,
	                          update_secrets,
	                          NULL,
	                          "/org/gnome/ControlCenter/network/eap-method-tls.ui",
	                          "grid",
	                          "identity_entry",
	                          phase2);
	if (!parent)
		return NULL;

	parent->password_flags_name = phase2 ?
	                                NM_SETTING_802_1X_PHASE2_PRIVATE_KEY_PASSWORD :
	                                NM_SETTING_802_1X_PRIVATE_KEY_PASSWORD;
	self = (EAPMethodTLS *) parent;
	self->sec_parent = ws_parent;
	self->editing_connection = secrets_only ? FALSE : TRUE;

	self->ca_cert_button = GTK_FILE_CHOOSER_BUTTON (gtk_builder_get_object (parent->builder, "ca_cert_button"));
	self->ca_cert_label = GTK_LABEL (gtk_builder_get_object (parent->builder, "ca_cert_label"));
	self->ca_cert_not_required_check = GTK_CHECK_BUTTON (gtk_builder_get_object (parent->builder, "ca_cert_not_required_check"));
	self->identity_entry = GTK_ENTRY (gtk_builder_get_object (parent->builder, "identity_entry"));
	self->identity_label = GTK_LABEL (gtk_builder_get_object (parent->builder, "identity_label"));
	self->private_key_button = GTK_FILE_CHOOSER_BUTTON (gtk_builder_get_object (parent->builder, "private_key_button"));
	self->private_key_label = GTK_LABEL (gtk_builder_get_object (parent->builder, "private_key_label"));
	self->private_key_password_entry = GTK_ENTRY (gtk_builder_get_object (parent->builder, "private_key_password_entry"));
	self->private_key_password_label = GTK_LABEL (gtk_builder_get_object (parent->builder, "private_key_password_label"));
	self->show_password_check = GTK_CHECK_BUTTON (gtk_builder_get_object (parent->builder, "show_password_check"));
	self->user_cert_button = GTK_FILE_CHOOSER_BUTTON (gtk_builder_get_object (parent->builder, "user_cert_button"));
	self->user_cert_label = GTK_LABEL (gtk_builder_get_object (parent->builder, "user_cert_label"));

	if (connection)
		s_8021x = nm_connection_get_setting_802_1x (connection);

	g_signal_connect_swapped (self->ca_cert_not_required_check, "toggled", G_CALLBACK (ca_cert_not_required_toggled), self);

	g_signal_connect_swapped (self->identity_entry, "changed", G_CALLBACK (changed_cb), self);
	if (s_8021x && nm_setting_802_1x_get_identity (s_8021x))
		gtk_entry_set_text (self->identity_entry, nm_setting_802_1x_get_identity (s_8021x));

	setup_filepicker (self->user_cert_button,
	                  _("Choose your personal certificate"),
	                  ws_parent, parent, s_8021x,
	                  phase2 ? nm_setting_802_1x_get_phase2_client_cert_scheme : nm_setting_802_1x_get_client_cert_scheme,
	                  phase2 ? nm_setting_802_1x_get_phase2_client_cert_path : nm_setting_802_1x_get_client_cert_path,
	                  FALSE, TRUE);
	setup_filepicker (self->ca_cert_button,
	                  _("Choose a Certificate Authority certificate"),
	                  ws_parent, parent, s_8021x,
	                  phase2 ? nm_setting_802_1x_get_phase2_ca_cert_scheme : nm_setting_802_1x_get_ca_cert_scheme,
	                  phase2 ? nm_setting_802_1x_get_phase2_ca_cert_path : nm_setting_802_1x_get_ca_cert_path,
	                  FALSE, FALSE);
	setup_filepicker (self->private_key_button,
	                  _("Choose your private key"),
	                  ws_parent, parent, s_8021x,
	                  phase2 ? nm_setting_802_1x_get_phase2_private_key_scheme : nm_setting_802_1x_get_private_key_scheme,
	                  phase2 ? nm_setting_802_1x_get_phase2_private_key_path : nm_setting_802_1x_get_private_key_path,
	                  TRUE, FALSE);

	if (connection && eap_method_ca_cert_ignore_get (parent, connection))
		ca_not_required = !gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (self->ca_cert_button));
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (self->ca_cert_not_required_check), ca_not_required);

	/* Fill secrets, if any */
	if (connection)
		update_secrets (parent, connection);

	g_signal_connect_swapped (self->private_key_password_entry, "changed", G_CALLBACK (changed_cb), self);

	/* Create password-storage popup menu for password entry under entry's secondary icon */
	nma_utils_setup_password_storage (GTK_WIDGET (self->private_key_password_entry), 0, (NMSetting *) s_8021x, parent->password_flags_name,
	                                  FALSE, secrets_only);

	g_signal_connect_swapped (self->show_password_check, "toggled", G_CALLBACK (show_toggled_cb), self);

	if (secrets_only) {
		gtk_widget_set_sensitive (GTK_WIDGET (self->identity_entry), FALSE);
		gtk_widget_hide (GTK_WIDGET (self->user_cert_label));
		gtk_widget_hide (GTK_WIDGET (self->user_cert_button));
		gtk_widget_hide (GTK_WIDGET (self->private_key_label));
		gtk_widget_hide (GTK_WIDGET (self->private_key_button));
		gtk_widget_hide (GTK_WIDGET (self->ca_cert_label));
		gtk_widget_hide (GTK_WIDGET (self->ca_cert_button));
		gtk_widget_hide (GTK_WIDGET (self->ca_cert_not_required_check));
	}

	return self;
}

