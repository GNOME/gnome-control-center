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
 * (C) Copyright 2007 - 2010 Red Hat, Inc.
 */

#include "config.h"

#include <glib/gi18n.h>
#include <ctype.h>
#include <string.h>

#include <nm-setting-connection.h>
#include <nm-setting-8021x.h>

#include "eap-method.h"
#include "wireless-security.h"
#include "helpers.h"

struct _EAPMethodTLS {
	EAPMethod parent;

	gboolean new_connection;
};


static void
show_toggled_cb (GtkCheckButton *button, EAPMethod *method)
{
	GtkWidget *widget;
	gboolean visible;

	widget = GTK_WIDGET (gtk_builder_get_object (method->builder, "eap_tls_private_key_password_entry"));
	g_assert (widget);

	visible = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button));
	gtk_entry_set_visibility (GTK_ENTRY (widget), visible);
}

static gboolean
validate (EAPMethod *parent)
{
	NMSetting8021xCKFormat format = NM_SETTING_802_1X_CK_FORMAT_UNKNOWN;
	GtkWidget *widget;
	const char *password, *identity;

	widget = GTK_WIDGET (gtk_builder_get_object (parent->builder, "eap_tls_identity_entry"));
	g_assert (widget);
	identity = gtk_entry_get_text (GTK_ENTRY (widget));
	if (!identity || !strlen (identity))
		return FALSE;

	if (!eap_method_validate_filepicker (parent->builder, "eap_tls_ca_cert_button", TYPE_CA_CERT, NULL, NULL))
		return FALSE;

	widget = GTK_WIDGET (gtk_builder_get_object (parent->builder, "eap_tls_private_key_password_entry"));
	g_assert (widget);
	password = gtk_entry_get_text (GTK_ENTRY (widget));
	if (!password || !strlen (password))
		return FALSE;

	if (!eap_method_validate_filepicker (parent->builder,
	                                     "eap_tls_private_key_button",
	                                     TYPE_PRIVATE_KEY,
	                                     password,
	                                     &format))
		return FALSE;

	if (format != NM_SETTING_802_1X_CK_FORMAT_PKCS12) {
		if (!eap_method_validate_filepicker (parent->builder, "eap_tls_user_cert_button", TYPE_CLIENT_CERT, NULL, NULL))
			return FALSE;
	}

	return TRUE;
}

static void
add_to_size_group (EAPMethod *parent, GtkSizeGroup *group)
{
	GtkWidget *widget;

	widget = GTK_WIDGET (gtk_builder_get_object (parent->builder, "eap_tls_identity_label"));
	g_assert (widget);
	gtk_size_group_add_widget (group, widget);

	widget = GTK_WIDGET (gtk_builder_get_object (parent->builder, "eap_tls_user_cert_label"));
	g_assert (widget);
	gtk_size_group_add_widget (group, widget);

	widget = GTK_WIDGET (gtk_builder_get_object (parent->builder, "eap_tls_ca_cert_label"));
	g_assert (widget);
	gtk_size_group_add_widget (group, widget);

	widget = GTK_WIDGET (gtk_builder_get_object (parent->builder, "eap_tls_private_key_label"));
	g_assert (widget);
	gtk_size_group_add_widget (group, widget);

	widget = GTK_WIDGET (gtk_builder_get_object (parent->builder, "eap_tls_private_key_password_label"));
	g_assert (widget);
	gtk_size_group_add_widget (group, widget);
}

static void
fill_connection (EAPMethod *parent, NMConnection *connection)
{
	EAPMethodTLS *method = (EAPMethodTLS *) parent;
	NMSetting8021xCKFormat format = NM_SETTING_802_1X_CK_FORMAT_UNKNOWN;
	NMSetting8021x *s_8021x;
	GtkWidget *widget;
	char *ca_filename, *pk_filename, *cc_filename;
	const char *password = NULL;
	GError *error = NULL;
	const char *secret_flag_prop = NULL;

	s_8021x = nm_connection_get_setting_802_1x (connection);
	g_assert (s_8021x);

	if (parent->phase2)
		g_object_set (s_8021x, NM_SETTING_802_1X_PHASE2_AUTH, "tls", NULL);
	else
		nm_setting_802_1x_add_eap_method (s_8021x, "tls");

	widget = GTK_WIDGET (gtk_builder_get_object (parent->builder, "eap_tls_identity_entry"));
	g_assert (widget);
	g_object_set (s_8021x, NM_SETTING_802_1X_IDENTITY, gtk_entry_get_text (GTK_ENTRY (widget)), NULL);

	/* TLS private key */
	widget = GTK_WIDGET (gtk_builder_get_object (parent->builder, "eap_tls_private_key_password_entry"));
	g_assert (widget);
	password = gtk_entry_get_text (GTK_ENTRY (widget));
	g_assert (password);

	widget = GTK_WIDGET (gtk_builder_get_object (parent->builder, "eap_tls_private_key_button"));
	g_assert (widget);
	pk_filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (widget));
	g_assert (pk_filename);

	if (parent->phase2) {
		if (!nm_setting_802_1x_set_phase2_private_key (s_8021x, pk_filename, password, NM_SETTING_802_1X_CK_SCHEME_PATH, &format, &error)) {
			g_warning ("Couldn't read phase2 private key '%s': %s", pk_filename, error ? error->message : "(unknown)");
			g_clear_error (&error);
		}
		secret_flag_prop = NM_SETTING_802_1X_PHASE2_PRIVATE_KEY_PASSWORD_FLAGS;
	} else {
		if (!nm_setting_802_1x_set_private_key (s_8021x, pk_filename, password, NM_SETTING_802_1X_CK_SCHEME_PATH, &format, &error)) {
			g_warning ("Couldn't read private key '%s': %s", pk_filename, error ? error->message : "(unknown)");
			g_clear_error (&error);
		}
		secret_flag_prop = NM_SETTING_802_1X_PRIVATE_KEY_PASSWORD_FLAGS;
	}
	g_free (pk_filename);

	/* Default to agent-owned secrets for new connections */
	if (method->new_connection) {
		g_object_set (s_8021x,
		              secret_flag_prop, NM_SETTING_SECRET_FLAG_AGENT_OWNED,
		              NM_SETTING_802_1X_SYSTEM_CA_CERTS, TRUE,
		              NULL);
	}

	/* TLS client certificate */
	if (format != NM_SETTING_802_1X_CK_FORMAT_PKCS12) {
		/* If the key is pkcs#12 nm_setting_802_1x_set_private_key() already
		 * set the client certificate for us.
		 */
		widget = GTK_WIDGET (gtk_builder_get_object (parent->builder, "eap_tls_user_cert_button"));
		g_assert (widget);
		cc_filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (widget));
		g_assert (cc_filename);

		format = NM_SETTING_802_1X_CK_FORMAT_UNKNOWN;
		if (parent->phase2) {
			if (!nm_setting_802_1x_set_phase2_client_cert (s_8021x, cc_filename, NM_SETTING_802_1X_CK_SCHEME_PATH, &format, &error)) {
				g_warning ("Couldn't read phase2 client certificate '%s': %s", cc_filename, error ? error->message : "(unknown)");
				g_clear_error (&error);
			}
		} else {
			if (!nm_setting_802_1x_set_client_cert (s_8021x, cc_filename, NM_SETTING_802_1X_CK_SCHEME_PATH, &format, &error)) {
				g_warning ("Couldn't read client certificate '%s': %s", cc_filename, error ? error->message : "(unknown)");
				g_clear_error (&error);
			}
		}
		g_free (cc_filename);
	}

	/* TLS CA certificate */
	widget = GTK_WIDGET (gtk_builder_get_object (parent->builder, "eap_tls_ca_cert_button"));
	g_assert (widget);
	ca_filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (widget));

	format = NM_SETTING_802_1X_CK_FORMAT_UNKNOWN;
	if (parent->phase2) {
		if (!nm_setting_802_1x_set_phase2_ca_cert (s_8021x, ca_filename, NM_SETTING_802_1X_CK_SCHEME_PATH, &format, &error)) {
			g_warning ("Couldn't read phase2 CA certificate '%s': %s", ca_filename, error ? error->message : "(unknown)");
			g_clear_error (&error);
		}
	} else {
		if (!nm_setting_802_1x_set_ca_cert (s_8021x, ca_filename, NM_SETTING_802_1X_CK_SCHEME_PATH, &format, &error)) {
			g_warning ("Couldn't read CA certificate '%s': %s", ca_filename, error ? error->message : "(unknown)");
			g_clear_error (&error);
		}
	}
}

static void
private_key_picker_helper (EAPMethod *parent, const char *filename, gboolean changed)
{
	NMSetting8021x *setting;
	NMSetting8021xCKFormat cert_format = NM_SETTING_802_1X_CK_FORMAT_UNKNOWN;
	const char *password;
	GtkWidget *widget;

	widget = GTK_WIDGET (gtk_builder_get_object (parent->builder, "eap_tls_private_key_password_entry"));
	g_assert (widget);
	password = gtk_entry_get_text (GTK_ENTRY (widget));

	setting = (NMSetting8021x *) nm_setting_802_1x_new ();
	nm_setting_802_1x_set_private_key (setting, filename, password, NM_SETTING_802_1X_CK_SCHEME_PATH, &cert_format, NULL);
	g_object_unref (setting);

	/* With PKCS#12, the client cert must be the same as the private key */
	widget = GTK_WIDGET (gtk_builder_get_object (parent->builder, "eap_tls_user_cert_button"));
	if (cert_format == NM_SETTING_802_1X_CK_FORMAT_PKCS12) {
		gtk_file_chooser_unselect_all (GTK_FILE_CHOOSER (widget));
		gtk_widget_set_sensitive (widget, FALSE);
	} else if (changed)
		gtk_widget_set_sensitive (widget, TRUE);

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
		                                          _("The selected private key does not appear to be protected by a password.  This could allow your security credentials to be compromised.  Please select a password-protected private key.\n\n(You can password-protect your private key with openssl)"));
		gtk_dialog_run (GTK_DIALOG (dialog));
		gtk_widget_destroy (dialog);
	}
}

static void
private_key_picker_file_set_cb (GtkWidget *chooser, gpointer user_data)
{
	EAPMethod *parent = (EAPMethod *) user_data;
	char *filename;

	filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (chooser));
	if (filename)
		private_key_picker_helper (parent, filename, TRUE);
	g_free (filename);
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
setup_filepicker (GtkBuilder *builder,
                  const char *name,
                  const char *title,
                  WirelessSecurity *ws_parent,
                  EAPMethod *parent,
                  NMSetting8021x *s_8021x,
                  SchemeFunc scheme_func,
                  PathFunc path_func,
                  gboolean privkey,
                  gboolean client_cert)
{
	GtkWidget *widget;
	GtkFileFilter *filter;
	const char *filename = NULL;

	widget = GTK_WIDGET (gtk_builder_get_object (builder, name));
	g_assert (widget);
	gtk_file_chooser_set_local_only (GTK_FILE_CHOOSER (widget), TRUE);
	gtk_file_chooser_button_set_title (GTK_FILE_CHOOSER_BUTTON (widget), title);

	if (s_8021x && path_func && scheme_func) {
		if (scheme_func (s_8021x) == NM_SETTING_802_1X_CK_SCHEME_PATH) {
			filename = path_func (s_8021x);
			if (filename)
				gtk_file_chooser_set_filename (GTK_FILE_CHOOSER (widget), filename);
		}
	}

	/* Connect a special handler for private keys to intercept PKCS#12 key types
	 * and desensitize the user cert button.
	 */
	if (privkey) {
		g_signal_connect (G_OBJECT (widget), "selection-changed",
		                  (GCallback) private_key_picker_file_set_cb,
		                  parent);
		if (filename)
			private_key_picker_helper (parent, filename, FALSE);
	}

	g_signal_connect (G_OBJECT (widget), "selection-changed",
	                  (GCallback) wireless_security_changed_cb,
	                  ws_parent);

	filter = eap_method_default_file_chooser_filter_new (privkey);
	gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (widget), filter);

	/* For some reason, GTK+ calls set_current_filter (..., NULL) from 
	 * gtkfilechooserdefault.c::show_and_select_files_finished_loading() on our
	 * dialog; so force-reset the filter to what we want it to be whenever
	 * it gets cleared.
	 */
	if (client_cert)
		g_signal_connect (G_OBJECT (widget), "notify::filter", (GCallback) reset_filter, filter);
}

static void
update_secrets (EAPMethod *parent, NMConnection *connection)
{
	NMSetting8021x *s_8021x;
	HelperSecretFunc password_func;
	SchemeFunc scheme_func;
	PathFunc path_func;
	const char *filename;
	GtkWidget *widget;

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
	                          parent->builder,
	                          "eap_tls_private_key_password_entry",
	                          NM_TYPE_SETTING_802_1X,
	                          password_func);

	/* Set the private key filepicker button path if we have a private key */
	s_8021x = nm_connection_get_setting_802_1x (connection);
	if (s_8021x && (scheme_func (s_8021x) == NM_SETTING_802_1X_CK_SCHEME_PATH)) {
		filename = path_func (s_8021x);
		if (filename) {
			widget = GTK_WIDGET (gtk_builder_get_object (parent->builder, "eap_tls_private_key_button"));
			g_assert (widget);
			gtk_file_chooser_set_filename (GTK_FILE_CHOOSER (widget), filename);
		}
	}
}

EAPMethodTLS *
eap_method_tls_new (WirelessSecurity *ws_parent,
                    NMConnection *connection,
                    gboolean phase2,
                    gboolean secrets_only)
{
	EAPMethodTLS *method;
	EAPMethod *parent;
	GtkWidget *widget;
	NMSetting8021x *s_8021x = NULL;

	parent = eap_method_init (sizeof (EAPMethodTLS),
	                          validate,
	                          add_to_size_group,
	                          fill_connection,
	                          update_secrets,
	                          NULL,
	                          UIDIR "/eap-method-tls.ui",
	                          "eap_tls_notebook",
	                          "eap_tls_identity_entry",
	                          phase2);
	if (!parent)
		return NULL;

	method = (EAPMethodTLS *) parent;
	method->new_connection = secrets_only ? FALSE : TRUE;

	eap_method_nag_init (parent, "eap_tls_ca_cert_button", connection);

	if (connection)
		s_8021x = nm_connection_get_setting_802_1x (connection);

	widget = GTK_WIDGET (gtk_builder_get_object (parent->builder, "eap_tls_identity_entry"));
	g_assert (widget);
	g_signal_connect (G_OBJECT (widget), "changed",
	                  (GCallback) wireless_security_changed_cb,
	                  ws_parent);
	if (s_8021x && nm_setting_802_1x_get_identity (s_8021x))
		gtk_entry_set_text (GTK_ENTRY (widget), nm_setting_802_1x_get_identity (s_8021x));

	setup_filepicker (parent->builder, "eap_tls_user_cert_button",
	                  _("Choose your personal certificate..."),
	                  ws_parent, parent, s_8021x,
	                  phase2 ? nm_setting_802_1x_get_phase2_client_cert_scheme : nm_setting_802_1x_get_client_cert_scheme,
	                  phase2 ? nm_setting_802_1x_get_phase2_client_cert_path : nm_setting_802_1x_get_client_cert_path,
	                  FALSE, TRUE);
	setup_filepicker (parent->builder, "eap_tls_ca_cert_button",
	                  _("Choose a Certificate Authority certificate..."),
	                  ws_parent, parent, s_8021x,
	                  phase2 ? nm_setting_802_1x_get_phase2_ca_cert_scheme : nm_setting_802_1x_get_ca_cert_scheme,
	                  phase2 ? nm_setting_802_1x_get_phase2_ca_cert_path : nm_setting_802_1x_get_ca_cert_path,
	                  FALSE, FALSE);
	setup_filepicker (parent->builder, "eap_tls_private_key_button",
	                  _("Choose your private key..."),
	                  ws_parent, parent, s_8021x,
	                  phase2 ? nm_setting_802_1x_get_phase2_private_key_scheme : nm_setting_802_1x_get_private_key_scheme,
	                  phase2 ? nm_setting_802_1x_get_phase2_private_key_path : nm_setting_802_1x_get_private_key_path,
	                  TRUE, FALSE);

	/* Fill secrets, if any */
	if (connection)
		update_secrets (parent, connection);

	widget = GTK_WIDGET (gtk_builder_get_object (parent->builder, "eap_tls_private_key_password_entry"));
	g_assert (widget);
	g_signal_connect (G_OBJECT (widget), "changed",
	                  (GCallback) wireless_security_changed_cb,
	                  ws_parent);

	widget = GTK_WIDGET (gtk_builder_get_object (parent->builder, "show_checkbutton_eaptls"));
	g_assert (widget);
	g_signal_connect (G_OBJECT (widget), "toggled",
	                  (GCallback) show_toggled_cb,
	                  parent);

	if (secrets_only) {
		widget = GTK_WIDGET (gtk_builder_get_object (parent->builder, "eap_tls_identity_entry"));
		gtk_widget_set_sensitive (widget, FALSE);
		widget = GTK_WIDGET (gtk_builder_get_object (parent->builder, "eap_tls_user_cert_label"));
		gtk_widget_hide (widget);
		widget = GTK_WIDGET (gtk_builder_get_object (parent->builder, "eap_tls_user_cert_button"));
		gtk_widget_hide (widget);
		widget = GTK_WIDGET (gtk_builder_get_object (parent->builder, "eap_tls_private_key_label"));
		gtk_widget_hide (widget);
		widget = GTK_WIDGET (gtk_builder_get_object (parent->builder, "eap_tls_private_key_button"));
		gtk_widget_hide (widget);
		widget = GTK_WIDGET (gtk_builder_get_object (parent->builder, "eap_tls_ca_cert_label"));
		gtk_widget_hide (widget);
		widget = GTK_WIDGET (gtk_builder_get_object (parent->builder, "eap_tls_ca_cert_button"));
		gtk_widget_hide (widget);
	}

	return method;
}

