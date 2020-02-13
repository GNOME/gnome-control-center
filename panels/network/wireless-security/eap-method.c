/* -*- Mode: C; tab-width: 5; indent-tabs-mode: t; c-basic-offset: 5 -*- */

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

#include <fcntl.h>
#include <glib/gi18n.h>

#include "eap-method.h"
#include "helpers.h"
#include "ui-helpers.h"

G_DEFINE_INTERFACE (EAPMethod, eap_method, G_TYPE_OBJECT)

enum {
        CHANGED,
        LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

static void
eap_method_default_init (EAPMethodInterface *iface)
{
        signals[CHANGED] =
                g_signal_new ("changed",
                              G_TYPE_FROM_INTERFACE (iface),
                              G_SIGNAL_RUN_FIRST,
                              0,
                              NULL, NULL,
                              g_cclosure_marshal_VOID__VOID,
                              G_TYPE_NONE, 0);
}

GtkWidget *
eap_method_get_default_field (EAPMethod *self)
{
	g_return_val_if_fail (self != NULL, NULL);

	return EAP_METHOD_GET_IFACE (self)->get_default_field (self);
}

const gchar *
eap_method_get_password_flags_name (EAPMethod *self)
{
	g_return_val_if_fail (self != NULL, NULL);

	if (EAP_METHOD_GET_IFACE (self)->get_password_flags_name)
		return EAP_METHOD_GET_IFACE (self)->get_password_flags_name (self);
	else
		return NULL;
}

gboolean
eap_method_get_phase2 (EAPMethod *self)
{
	g_return_val_if_fail (self != NULL, FALSE);

	if (EAP_METHOD_GET_IFACE (self)->get_phase2)
		return EAP_METHOD_GET_IFACE (self)->get_phase2 (self);
	else
		return FALSE;
}

gboolean
eap_method_validate (EAPMethod *self, GError **error)
{
	gboolean result;

	g_return_val_if_fail (self != NULL, FALSE);

	result = (*(EAP_METHOD_GET_IFACE (self)->validate)) (self, error);
	if (!result && error && !*error)
		g_set_error_literal (error, NMA_ERROR, NMA_ERROR_GENERIC, _("undefined error in 802.1X security (wpa-eap)"));
	return result;
}

void
eap_method_update_secrets (EAPMethod *self, NMConnection *connection)
{
	g_return_if_fail (self != NULL);

	if (EAP_METHOD_GET_IFACE (self)->update_secrets)
		EAP_METHOD_GET_IFACE (self)->update_secrets (self, connection);
}

void
eap_method_add_to_size_group (EAPMethod *self, GtkSizeGroup *group)
{
	g_return_if_fail (self != NULL);
	g_return_if_fail (group != NULL);

	return (*(EAP_METHOD_GET_IFACE (self)->add_to_size_group)) (self, group);
}

void
eap_method_fill_connection (EAPMethod *self,
                            NMConnection *connection,
                            NMSettingSecretFlags flags)
{
	g_return_if_fail (self != NULL);
	g_return_if_fail (connection != NULL);

	return (*(EAP_METHOD_GET_IFACE (self)->fill_connection)) (self, connection, flags);
}

void
eap_method_emit_changed (EAPMethod *self)
{
        g_return_if_fail (EAP_IS_METHOD (self));

        g_signal_emit (self, signals[CHANGED], 0);
}

const gchar *
eap_method_get_username (EAPMethod *self)
{
	g_return_val_if_fail (EAP_IS_METHOD (self), NULL);
	return EAP_METHOD_GET_IFACE (self)->get_username (self);
}

void
eap_method_set_username (EAPMethod *self, const gchar *username)
{
	g_return_if_fail (EAP_IS_METHOD (self));
	EAP_METHOD_GET_IFACE (self)->set_username (self, username);
}

const gchar *
eap_method_get_password (EAPMethod *self)
{
	g_return_val_if_fail (EAP_IS_METHOD (self), NULL);
	return EAP_METHOD_GET_IFACE (self)->get_password (self);
}

void
eap_method_set_password (EAPMethod *self, const gchar *password)
{
	g_return_if_fail (EAP_IS_METHOD (self));
	EAP_METHOD_GET_IFACE (self)->set_password (self, password);
}

gboolean
eap_method_get_show_password (EAPMethod *self)
{
	g_return_val_if_fail (EAP_IS_METHOD (self), FALSE);
	return EAP_METHOD_GET_IFACE (self)->get_show_password (self);
}

void
eap_method_set_show_password (EAPMethod *self, gboolean show_password)
{
	g_return_if_fail (EAP_IS_METHOD (self));
	EAP_METHOD_GET_IFACE (self)->set_show_password (self, show_password);
}

gboolean
eap_method_validate_filepicker (GtkFileChooser *chooser,
                                guint32 item_type,
                                const char *password,
                                NMSetting8021xCKFormat *out_format,
                                GError **error)
{
	g_autofree gchar *filename = NULL;
	g_autoptr(NMSetting8021x) setting = NULL;
	gboolean success = TRUE;

	if (item_type == TYPE_PRIVATE_KEY) {
		if (!password || *password == '\0')
			success = FALSE;
	}

	filename = gtk_file_chooser_get_filename (chooser);
	if (!filename) {
		if (item_type != TYPE_CA_CERT) {
			success = FALSE;
			g_set_error_literal (error, NMA_ERROR, NMA_ERROR_GENERIC, _("no file selected"));
		}
		goto out;
	}

	if (!g_file_test (filename, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_REGULAR)) {
		success = FALSE;
		goto out;
	}

	setting = (NMSetting8021x *) nm_setting_802_1x_new ();

	success = FALSE;
	if (item_type == TYPE_PRIVATE_KEY) {
		if (nm_setting_802_1x_set_private_key (setting, filename, password, NM_SETTING_802_1X_CK_SCHEME_PATH, out_format, error))
			success = TRUE;
	} else if (item_type == TYPE_CLIENT_CERT) {
		if (nm_setting_802_1x_set_client_cert (setting, filename, NM_SETTING_802_1X_CK_SCHEME_PATH, out_format, error))
			success = TRUE;
	} else if (item_type == TYPE_CA_CERT) {
		if (nm_setting_802_1x_set_ca_cert (setting, filename, NM_SETTING_802_1X_CK_SCHEME_PATH, out_format, error))
			success = TRUE;
	} else
		g_warning ("%s: invalid item type %d.", __func__, item_type);

out:
	if (!success && error && !*error)
		g_set_error_literal (error, NMA_ERROR, NMA_ERROR_GENERIC, _("unspecified error validating eap-method file"));

	if (success)
		widget_unset_error (GTK_WIDGET (chooser));
	else
		widget_set_error (GTK_WIDGET (chooser));
	return success;
}

static gboolean
file_has_extension (const char *filename, const char *extensions[])
{
	char *p;
	g_autofree gchar *ext = NULL;
	int i = 0;
	gboolean found = FALSE;

	p = strrchr (filename, '.');
	if (!p)
		return FALSE;

	ext = g_ascii_strdown (p, -1);
	if (ext) {
		while (extensions[i]) {
			if (!strcmp (ext, extensions[i++])) {
				found = TRUE;
				break;
			}
		}
	}

	return found;
}

#if !LIBNM_BUILD
static const char *
find_tag (const char *tag, const char *buf, gsize len)
{
	gsize i, taglen;

	taglen = strlen (tag);
	if (len < taglen)
		return NULL;

	for (i = 0; i < len - taglen + 1; i++) {
		if (memcmp (buf + i, tag, taglen) == 0)
			return buf + i;
	}
	return NULL;
}

static const char *pem_rsa_key_begin = "-----BEGIN RSA PRIVATE KEY-----";
static const char *pem_dsa_key_begin = "-----BEGIN DSA PRIVATE KEY-----";
static const char *pem_pkcs8_enc_key_begin = "-----BEGIN ENCRYPTED PRIVATE KEY-----";
static const char *pem_pkcs8_dec_key_begin = "-----BEGIN PRIVATE KEY-----";
static const char *pem_cert_begin = "-----BEGIN CERTIFICATE-----";
static const char *proc_type_tag = "Proc-Type: 4,ENCRYPTED";
static const char *dek_info_tag = "DEK-Info:";

static gboolean
pem_file_is_encrypted (const char *buffer, gsize bytes_read)
{
	/* Check if the private key is encrypted or not by looking for the
	 * old OpenSSL-style proc-type and dec-info tags.
	 */
	if (find_tag (proc_type_tag, (const char *) buffer, bytes_read)) {
		if (find_tag (dek_info_tag, (const char *) buffer, bytes_read))
			return TRUE;
	}
	return FALSE;
}

static gboolean
file_is_der_or_pem (const char *filename,
                    gboolean privkey,
                    gboolean *out_privkey_encrypted)
{
	int fd;
	unsigned char buffer[8192];
	ssize_t bytes_read;
	gboolean success = FALSE;

	fd = open (filename, O_RDONLY);
	if (fd < 0)
		return FALSE;

	bytes_read = read (fd, buffer, sizeof (buffer) - 1);
	if (bytes_read < 400)  /* needs to be lower? */
		goto out;
	buffer[bytes_read] = '\0';

	/* Check for DER signature */
	if (bytes_read > 2 && buffer[0] == 0x30 && buffer[1] == 0x82) {
		success = TRUE;
		goto out;
	}

	/* Check for PEM signatures */
	if (privkey) {
		if (find_tag (pem_rsa_key_begin, (const char *) buffer, bytes_read)) {
			success = TRUE;
			if (out_privkey_encrypted)
				*out_privkey_encrypted = pem_file_is_encrypted ((const char *) buffer, bytes_read);
			goto out;
		}

		if (find_tag (pem_dsa_key_begin, (const char *) buffer, bytes_read)) {
			success = TRUE;
			if (out_privkey_encrypted)
				*out_privkey_encrypted = pem_file_is_encrypted ((const char *) buffer, bytes_read);
			goto out;
		}

		if (find_tag (pem_pkcs8_enc_key_begin, (const char *) buffer, bytes_read)) {
			success = TRUE;
			if (out_privkey_encrypted)
				*out_privkey_encrypted = TRUE;
			goto out;
		}

		if (find_tag (pem_pkcs8_dec_key_begin, (const char *) buffer, bytes_read)) {
			success = TRUE;
			if (out_privkey_encrypted)
				*out_privkey_encrypted = FALSE;
			goto out;
		}
	} else {
		if (find_tag (pem_cert_begin, (const char *) buffer, bytes_read)) {
			success = TRUE;
			goto out;
		}
	}

out:
	close (fd);
	return success;
}
#endif

static gboolean
default_filter_privkey (const GtkFileFilterInfo *filter_info, gpointer user_data)
{
	const char *extensions[] = { ".der", ".pem", ".p12", ".key", NULL };

	if (!filter_info->filename)
		return FALSE;

	if (!file_has_extension (filter_info->filename, extensions))
		return FALSE;

	return TRUE;
}

static gboolean
default_filter_cert (const GtkFileFilterInfo *filter_info, gpointer user_data)
{
	const char *extensions[] = { ".der", ".pem", ".crt", ".cer", NULL };

	if (!filter_info->filename)
		return FALSE;

	if (!file_has_extension (filter_info->filename, extensions))
		return FALSE;

	return TRUE;
}

GtkFileFilter *
eap_method_default_file_chooser_filter_new (gboolean privkey)
{
	GtkFileFilter *filter;

	filter = gtk_file_filter_new ();
	if (privkey) {
		gtk_file_filter_add_custom (filter, GTK_FILE_FILTER_FILENAME, default_filter_privkey, NULL, NULL);
		gtk_file_filter_set_name (filter, _("DER, PEM, or PKCS#12 private keys (*.der, *.pem, *.p12, *.key)"));
	} else {
		gtk_file_filter_add_custom (filter, GTK_FILE_FILTER_FILENAME, default_filter_cert, NULL, NULL);
		gtk_file_filter_set_name (filter, _("DER or PEM certificates (*.der, *.pem, *.crt, *.cer)"));
	}
	return filter;
}

gboolean
eap_method_is_encrypted_private_key (const char *path)
{
	GtkFileFilterInfo info = { .filename = path };
	gboolean is_encrypted;

	if (!default_filter_privkey (&info, NULL))
		return FALSE;

#if LIBNM_BUILD
	is_encrypted = FALSE;
	if (!nm_utils_file_is_private_key (path, &is_encrypted))
		return FALSE;
#else
	is_encrypted = TRUE;
	if (   !file_is_der_or_pem (path, TRUE, &is_encrypted)
	    && !nm_utils_file_is_pkcs12 (path))
		return FALSE;
#endif
	return is_encrypted;
}

void
eap_method_ca_cert_not_required_toggled (GtkToggleButton *id_ca_cert_not_required_checkbutton, GtkFileChooser *id_ca_cert_chooser)
{
	g_autofree gchar *filename = NULL;
	g_autofree gchar *filename_old = NULL;
	gboolean is_not_required;

	g_assert (id_ca_cert_not_required_checkbutton && id_ca_cert_chooser);

	is_not_required = gtk_toggle_button_get_active (id_ca_cert_not_required_checkbutton);

	filename = gtk_file_chooser_get_filename (id_ca_cert_chooser);
	filename_old = g_object_steal_data (G_OBJECT (id_ca_cert_chooser), "filename-old");
	if (is_not_required) {
		g_free (filename_old);
		filename_old = g_steal_pointer (&filename);
	} else {
		g_free (filename);
		filename = g_steal_pointer (&filename_old);
	}
	gtk_widget_set_sensitive (GTK_WIDGET (id_ca_cert_chooser), !is_not_required);
	if (filename)
		gtk_file_chooser_set_filename (id_ca_cert_chooser, filename);
	else
		gtk_file_chooser_unselect_all (id_ca_cert_chooser);
	g_object_set_data_full (G_OBJECT (id_ca_cert_chooser), "filename-old", g_steal_pointer (&filename_old), g_free);
}

/* Used as both GSettings keys and GObject data tags */
#define IGNORE_CA_CERT_TAG "ignore-ca-cert"
#define IGNORE_PHASE2_CA_CERT_TAG "ignore-phase2-ca-cert"

/**
 * eap_method_ca_cert_ignore_set:
 * @method: the #EAPMethod object
 * @connection: the #NMConnection
 * @filename: the certificate file, if any
 * @ca_cert_error: %TRUE if an error was encountered loading the given CA
 * certificate, %FALSE if not or if a CA certificate is not present
 *
 * Updates the connection's CA cert ignore value to %TRUE if the "CA certificate
 * not required" checkbox is checked.  If @ca_cert_error is %TRUE, then the
 * connection's CA cert ignore value will always be set to %FALSE, because it
 * means that the user selected an invalid certificate (thus he does not want to
 * ignore the CA cert)..
 */
void
eap_method_ca_cert_ignore_set (EAPMethod *self,
                               NMConnection *connection,
                               const char *filename,
                               gboolean ca_cert_error)
{
	NMSetting8021x *s_8021x;
	gboolean ignore;

	s_8021x = nm_connection_get_setting_802_1x (connection);
	if (s_8021x) {
		ignore = !ca_cert_error && filename == NULL;
		g_object_set_data (G_OBJECT (s_8021x),
		                   eap_method_get_phase2 (self) ? IGNORE_PHASE2_CA_CERT_TAG : IGNORE_CA_CERT_TAG,
		                   GUINT_TO_POINTER (ignore));
	}
}

/**
 * eap_method_ca_cert_ignore_get:
 * @method: the #EAPMethod object
 * @connection: the #NMConnection
 *
 * Returns: %TRUE if a missing CA certificate can be ignored, %FALSE if a CA
 * certificate should be required for the connection to be valid.
 */
gboolean
eap_method_ca_cert_ignore_get (EAPMethod *self, NMConnection *connection)
{
	NMSetting8021x *s_8021x;

	s_8021x = nm_connection_get_setting_802_1x (connection);
	if (s_8021x) {
		return !!g_object_get_data (G_OBJECT (s_8021x),
		                            eap_method_get_phase2 (self) ? IGNORE_PHASE2_CA_CERT_TAG : IGNORE_CA_CERT_TAG);
	}
	return FALSE;
}

static GSettings *
_get_ca_ignore_settings (NMConnection *connection)
{
	GSettings *settings;
	g_autofree gchar *path = NULL;
	const char *uuid;

	g_return_val_if_fail (connection, NULL);

	uuid = nm_connection_get_uuid (connection);
	g_return_val_if_fail (uuid && *uuid, NULL);

	path = g_strdup_printf ("/org/gnome/nm-applet/eap/%s/", uuid);
	settings = g_settings_new_with_path ("org.gnome.nm-applet.eap", path);

	return settings;
}

/**
 * eap_method_ca_cert_ignore_save:
 * @connection: the connection for which to save CA cert ignore values to GSettings
 *
 * Reads the CA cert ignore tags from the 802.1x setting GObject data and saves
 * then to GSettings if present, using the connection UUID as the index.
 */
void
eap_method_ca_cert_ignore_save (NMConnection *connection)
{
	NMSetting8021x *s_8021x;
	g_autoptr(GSettings) settings = NULL;
	gboolean ignore = FALSE, phase2_ignore = FALSE;

	g_return_if_fail (connection);

	s_8021x = nm_connection_get_setting_802_1x (connection);
	if (s_8021x) {
		ignore = !!g_object_get_data (G_OBJECT (s_8021x), IGNORE_CA_CERT_TAG);
		phase2_ignore = !!g_object_get_data (G_OBJECT (s_8021x), IGNORE_PHASE2_CA_CERT_TAG);
	}

	settings = _get_ca_ignore_settings (connection);
	if (!settings)
		return;

	g_settings_set_boolean (settings, IGNORE_CA_CERT_TAG, ignore);
	g_settings_set_boolean (settings, IGNORE_PHASE2_CA_CERT_TAG, phase2_ignore);
}

/**
 * eap_method_ca_cert_ignore_load:
 * @connection: the connection for which to load CA cert ignore values to GSettings
 *
 * Reads the CA cert ignore tags from the 802.1x setting GObject data and saves
 * then to GSettings if present, using the connection UUID as the index.
 */
void
eap_method_ca_cert_ignore_load (NMConnection *connection)
{
	g_autoptr(GSettings) settings = NULL;
	NMSetting8021x *s_8021x;
	gboolean ignore, phase2_ignore;

	g_return_if_fail (connection);

	s_8021x = nm_connection_get_setting_802_1x (connection);
	if (!s_8021x)
		return;

	settings = _get_ca_ignore_settings (connection);
	if (!settings)
		return;

	ignore = g_settings_get_boolean (settings, IGNORE_CA_CERT_TAG);
	phase2_ignore = g_settings_get_boolean (settings, IGNORE_PHASE2_CA_CERT_TAG);

	g_object_set_data (G_OBJECT (s_8021x),
	                   IGNORE_CA_CERT_TAG,
	                   GUINT_TO_POINTER (ignore));
	g_object_set_data (G_OBJECT (s_8021x),
	                   IGNORE_PHASE2_CA_CERT_TAG,
	                   GUINT_TO_POINTER (phase2_ignore));
}

