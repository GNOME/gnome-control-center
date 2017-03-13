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

#include "nm-default.h"

#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "eap-method.h"
#include "nm-utils.h"
#include "utils.h"
#include "helpers.h"

G_DEFINE_BOXED_TYPE (EAPMethod, eap_method, eap_method_ref, eap_method_unref)

GtkWidget *
eap_method_get_widget (EAPMethod *method)
{
	g_return_val_if_fail (method != NULL, NULL);

	return method->ui_widget;
}

gboolean
eap_method_validate (EAPMethod *method, GError **error)
{
	gboolean result;

	g_return_val_if_fail (method != NULL, FALSE);

	g_assert (method->validate);
	result = (*(method->validate)) (method, error);
	if (!result && error && !*error)
		g_set_error_literal (error, NMA_ERROR, NMA_ERROR_GENERIC, _("undefined error in 802.1X security (wpa-eap)"));
	return result;
}

void
eap_method_add_to_size_group (EAPMethod *method, GtkSizeGroup *group)
{
	g_return_if_fail (method != NULL);
	g_return_if_fail (group != NULL);

	g_assert (method->add_to_size_group);
	return (*(method->add_to_size_group)) (method, group);
}

void
eap_method_fill_connection (EAPMethod *method,
                            NMConnection *connection,
                            NMSettingSecretFlags flags)
{
	g_return_if_fail (method != NULL);
	g_return_if_fail (connection != NULL);

	g_assert (method->fill_connection);
	return (*(method->fill_connection)) (method, connection, flags);
}

void
eap_method_update_secrets (EAPMethod *method, NMConnection *connection)
{
	g_return_if_fail (method != NULL);
	g_return_if_fail (connection != NULL);

	if (method->update_secrets)
		method->update_secrets (method, connection);
}

void
eap_method_phase2_update_secrets_helper (EAPMethod *method,
                                         NMConnection *connection,
                                         const char *combo_name,
                                         guint32 column)
{
	GtkWidget *combo;
	GtkTreeIter iter;
	GtkTreeModel *model;

	g_return_if_fail (method != NULL);
	g_return_if_fail (connection != NULL);
	g_return_if_fail (combo_name != NULL);

	combo = GTK_WIDGET (gtk_builder_get_object (method->builder, combo_name));
	g_assert (combo);

	/* Let each EAP phase2 method try to update its secrets */
	model = gtk_combo_box_get_model (GTK_COMBO_BOX (combo));
	if (gtk_tree_model_get_iter_first (model, &iter)) {
		do {
			EAPMethod *eap = NULL;

			gtk_tree_model_get (model, &iter, column, &eap, -1);
			if (eap) {
				eap_method_update_secrets (eap, connection);
				eap_method_unref (eap);
			}
		} while (gtk_tree_model_iter_next (model, &iter));
	}
}

EAPMethod *
eap_method_init (gsize obj_size,
                 EMValidateFunc validate,
                 EMAddToSizeGroupFunc add_to_size_group,
                 EMFillConnectionFunc fill_connection,
                 EMUpdateSecretsFunc update_secrets,
                 EMDestroyFunc destroy,
                 const char *ui_resource,
                 const char *ui_widget_name,
                 const char *default_field,
                 gboolean phase2)
{
	EAPMethod *method;
	GError *error = NULL;

	g_return_val_if_fail (obj_size > 0, NULL);
	g_return_val_if_fail (ui_resource != NULL, NULL);
	g_return_val_if_fail (ui_widget_name != NULL, NULL);

	method = g_slice_alloc0 (obj_size);
	g_assert (method);

	method->refcount = 1;
	method->obj_size = obj_size;
	method->validate = validate;
	method->add_to_size_group = add_to_size_group;
	method->fill_connection = fill_connection;
	method->update_secrets = update_secrets;
	method->default_field = default_field;
	method->phase2 = phase2;

	method->builder = gtk_builder_new ();
	if (!gtk_builder_add_from_resource (method->builder, ui_resource, &error)) {
		g_warning ("Couldn't load UI builder resource %s: %s",
		           ui_resource, error->message);
		eap_method_unref (method);
		return NULL;
	}

	method->ui_widget = GTK_WIDGET (gtk_builder_get_object (method->builder, ui_widget_name));
	if (!method->ui_widget) {
		g_warning ("Couldn't load UI widget '%s' from UI file %s",
		           ui_widget_name, ui_resource);
		eap_method_unref (method);
		return NULL;
	}
	g_object_ref_sink (method->ui_widget);

	method->destroy = destroy;

	return method;
}


EAPMethod *
eap_method_ref (EAPMethod *method)
{
	g_return_val_if_fail (method != NULL, NULL);
	g_return_val_if_fail (method->refcount > 0, NULL);

	method->refcount++;
	return method;
}

void
eap_method_unref (EAPMethod *method)
{
	g_return_if_fail (method != NULL);
	g_return_if_fail (method->refcount > 0);

	method->refcount--;
	if (method->refcount == 0) {
		if (method->destroy)
			method->destroy (method);

		if (method->builder)
			g_object_unref (method->builder);
		if (method->ui_widget)
			g_object_unref (method->ui_widget);

		g_slice_free1 (method->obj_size, method);
	}
}

gboolean
eap_method_validate_filepicker (GtkBuilder *builder,
                                const char *name,
                                guint32 item_type,
                                const char *password,
                                NMSetting8021xCKFormat *out_format,
                                GError **error)
{
	GtkWidget *widget;
	char *filename;
	NMSetting8021x *setting;
	gboolean success = TRUE;

	if (item_type == TYPE_PRIVATE_KEY) {
		if (!password || *password == '\0')
			success = FALSE;
	}

	widget = GTK_WIDGET (gtk_builder_get_object (builder, name));
	g_assert (widget);
	filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (widget));
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

	g_object_unref (setting);

out:
	g_free (filename);

	if (!success && error && !*error)
		g_set_error_literal (error, NMA_ERROR, NMA_ERROR_GENERIC, _("unspecified error validating eap-method file"));

	if (success)
		widget_unset_error (widget);
	else
		widget_set_error (widget);
	return success;
}

static gboolean
file_has_extension (const char *filename, const char *extensions[])
{
	char *p, *ext;
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
	g_free (ext);

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

/* Some methods (PEAP, TLS, TTLS) require a CA certificate. The user can choose
 * not to provide such a certificate. This method whether the checkbox
 * id_ca_cert_not_required_checkbutton is checked or id_ca_cert_chooser has a certificate
 * selected.
 */
gboolean
eap_method_ca_cert_required (GtkBuilder *builder, const char *id_ca_cert_not_required_checkbutton, const char *id_ca_cert_chooser)
{
	char *filename;
	GtkWidget *widget;

	g_assert (builder && id_ca_cert_not_required_checkbutton && id_ca_cert_chooser);

	widget = GTK_WIDGET (gtk_builder_get_object (builder, id_ca_cert_not_required_checkbutton));
	g_assert (widget && GTK_IS_TOGGLE_BUTTON (widget));

	if (!gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget))) {
		widget = GTK_WIDGET (gtk_builder_get_object (builder, id_ca_cert_chooser));
		g_assert (widget && GTK_IS_FILE_CHOOSER (widget));

		filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (widget));
		if (!filename)
			return TRUE;
		g_free (filename);
	}
	return FALSE;
}


void
eap_method_ca_cert_not_required_toggled (GtkBuilder *builder, const char *id_ca_cert_not_required_checkbutton, const char *id_ca_cert_chooser)
{
	char *filename, *filename_old;
	gboolean is_not_required;
	GtkWidget *widget;

	g_assert (builder && id_ca_cert_not_required_checkbutton && id_ca_cert_chooser);

	widget = GTK_WIDGET (gtk_builder_get_object (builder, id_ca_cert_not_required_checkbutton));
	g_assert (widget && GTK_IS_TOGGLE_BUTTON (widget));
	is_not_required = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));

	widget = GTK_WIDGET (gtk_builder_get_object (builder, id_ca_cert_chooser));
	g_assert (widget && GTK_IS_FILE_CHOOSER (widget));

	filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (widget));
	filename_old = g_object_steal_data (G_OBJECT (widget), "filename-old");
	if (is_not_required) {
		g_free (filename_old);
		filename_old = filename;
		filename = NULL;
	} else {
		g_free (filename);
		filename = filename_old;
		filename_old = NULL;
	}
	gtk_widget_set_sensitive (widget, !is_not_required);
	if (filename)
		gtk_file_chooser_set_filename (GTK_FILE_CHOOSER (widget), filename);
	else
		gtk_file_chooser_unselect_all (GTK_FILE_CHOOSER (widget));
	g_free (filename);
	g_object_set_data_full (G_OBJECT (widget), "filename-old", filename_old, g_free);
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
eap_method_ca_cert_ignore_set (EAPMethod *method,
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
		                   method->phase2 ? IGNORE_PHASE2_CA_CERT_TAG : IGNORE_CA_CERT_TAG,
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
eap_method_ca_cert_ignore_get (EAPMethod *method, NMConnection *connection)
{
	NMSetting8021x *s_8021x;

	s_8021x = nm_connection_get_setting_802_1x (connection);
	if (s_8021x) {
		return !!g_object_get_data (G_OBJECT (s_8021x),
		                            method->phase2 ? IGNORE_PHASE2_CA_CERT_TAG : IGNORE_CA_CERT_TAG);
	}
	return FALSE;
}

static GSettings *
_get_ca_ignore_settings (NMConnection *connection)
{
	GSettings *settings;
	char *path = NULL;
	const char *uuid;

	g_return_val_if_fail (connection, NULL);

	uuid = nm_connection_get_uuid (connection);
	g_return_val_if_fail (uuid && *uuid, NULL);

	path = g_strdup_printf ("/org/gnome/nm-applet/eap/%s/", uuid);
	settings = g_settings_new_with_path ("org.gnome.nm-applet.eap", path);
	g_free (path);

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
	GSettings *settings;
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
	g_object_unref (settings);
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
	GSettings *settings;
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
	g_object_unref (settings);
}

