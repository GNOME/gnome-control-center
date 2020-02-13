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

#pragma once

#include <gtk/gtk.h>
#include <NetworkManager.h>

G_BEGIN_DECLS

G_DECLARE_INTERFACE (EAPMethod, eap_method, EAP, METHOD, GObject)

struct _EAPMethodInterface {
	GTypeInterface g_iface;

	void         (*add_to_size_group)       (EAPMethod *method, GtkSizeGroup *group);
	void         (*fill_connection)         (EAPMethod *method, NMConnection *connection, NMSettingSecretFlags flags);
	void         (*update_secrets)          (EAPMethod *method, NMConnection *connection);
	gboolean     (*validate)                (EAPMethod *method, GError **error);
	GtkWidget*   (*get_default_field)       (EAPMethod *method);
	const gchar* (*get_password_flags_name) (EAPMethod *method);
	gboolean     (*get_phase2)              (EAPMethod *method);
	const gchar* (*get_username)            (EAPMethod *method);
	void         (*set_username)            (EAPMethod *method, const gchar *username);
	const gchar* (*get_password)            (EAPMethod *method);
	void         (*set_password)            (EAPMethod *method, const gchar *password);
	gboolean     (*get_show_password)       (EAPMethod *method);
	void         (*set_show_password)       (EAPMethod *method, gboolean show_password);
};

GtkWidget *eap_method_get_default_field (EAPMethod *method);

const gchar *eap_method_get_password_flags_name (EAPMethod *method);

gboolean eap_method_get_phase2 (EAPMethod *method);

void eap_method_update_secrets (EAPMethod *method, NMConnection *connection);

gboolean eap_method_validate (EAPMethod *method, GError **error);

void eap_method_add_to_size_group (EAPMethod *method, GtkSizeGroup *group);

void eap_method_fill_connection (EAPMethod *method,
                                 NMConnection *connection,
                                 NMSettingSecretFlags flags);

void eap_method_emit_changed (EAPMethod *method);

const gchar *eap_method_get_username (EAPMethod *method);

void eap_method_set_username (EAPMethod *method, const gchar *username);

const gchar *eap_method_get_password (EAPMethod *method);

void eap_method_set_password (EAPMethod *method, const gchar *password);

gboolean eap_method_get_show_password (EAPMethod *method);

void eap_method_set_show_password (EAPMethod *method, gboolean show_password);

/* Below for internal use only */

GtkFileFilter * eap_method_default_file_chooser_filter_new (gboolean privkey);

gboolean eap_method_is_encrypted_private_key (const char *path);

#define TYPE_CLIENT_CERT 0
#define TYPE_CA_CERT     1
#define TYPE_PRIVATE_KEY 2

gboolean eap_method_validate_filepicker (GtkFileChooser *chooser,
                                         guint32 item_type,
                                         const char *password,
                                         NMSetting8021xCKFormat *out_format,
                                         GError **error);

void eap_method_ca_cert_not_required_toggled (GtkToggleButton *id_ca_cert_is_not_required_checkbox,
                                              GtkFileChooser *id_ca_cert_chooser);

void eap_method_ca_cert_ignore_set (EAPMethod *method,
                                    NMConnection *connection,
                                    const char *filename,
                                    gboolean ca_cert_error);
gboolean eap_method_ca_cert_ignore_get (EAPMethod *method, NMConnection *connection);

void eap_method_ca_cert_ignore_save (NMConnection *connection);
void eap_method_ca_cert_ignore_load (NMConnection *connection);

G_END_DECLS
