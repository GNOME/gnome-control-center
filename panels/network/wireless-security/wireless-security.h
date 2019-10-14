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

#ifndef WIRELESS_SECURITY_H
#define WIRELESS_SECURITY_H

#include <gtk/gtk.h>

#define WIRELESS_TYPE_SECURITY (wireless_security_get_type ())

typedef struct _WirelessSecurity WirelessSecurity;
typedef struct _WirelessSecurityPrivate WirelessSecurityPrivate;

typedef void (*WSChangedFunc) (WirelessSecurity *sec, gpointer user_data);

typedef void (*WSAddToSizeGroupFunc)  (WirelessSecurity *sec, GtkSizeGroup *group);
typedef void (*WSFillConnectionFunc)  (WirelessSecurity *sec, NMConnection *connection);
typedef void (*WSDestroyFunc)         (WirelessSecurity *sec);
typedef gboolean (*WSValidateFunc)    (WirelessSecurity *sec, GError **error);
typedef GtkWidget* (*WSGetWidgetFunc) (WirelessSecurity *sec);

struct _WirelessSecurity {
	WirelessSecurityPrivate *priv;
	GtkBuilder *builder;
};

#define WIRELESS_SECURITY(x) ((WirelessSecurity *) x)


GtkWidget *wireless_security_get_widget (WirelessSecurity *sec);

void wireless_security_set_changed_notify (WirelessSecurity *sec,
                                           WSChangedFunc func,
                                           gpointer user_data);

gboolean wireless_security_validate (WirelessSecurity *sec, GError **error);

void wireless_security_add_to_size_group (WirelessSecurity *sec,
                                          GtkSizeGroup *group);

void wireless_security_fill_connection (WirelessSecurity *sec,
                                        NMConnection *connection);

void wireless_security_set_adhoc_compatible (WirelessSecurity *sec,
                                             gboolean adhoc_compatible);

gboolean wireless_security_adhoc_compatible (WirelessSecurity *sec);

void wireless_security_set_hotspot_compatible (WirelessSecurity *sec,
                                               gboolean hotspot_compatible);

gboolean wireless_security_hotspot_compatible (WirelessSecurity *sec);

const gchar *wireless_security_get_username (WirelessSecurity *sec);

const gchar *wireless_security_get_password (WirelessSecurity *sec);

gboolean wireless_security_get_always_ask (WirelessSecurity *sec);

gboolean wireless_security_get_show_password (WirelessSecurity *sec);

void wireless_security_set_userpass (WirelessSecurity *sec,
                                     const char *user,
                                     const char *password,
                                     gboolean always_ask,
                                     gboolean show_password);
void wireless_security_set_userpass_802_1x (WirelessSecurity *sec,
                                            NMConnection *connection);

WirelessSecurity *wireless_security_ref (WirelessSecurity *sec);

void wireless_security_unref (WirelessSecurity *sec);

GType wireless_security_get_type (void);

/* Below for internal use only */

WirelessSecurity *wireless_security_init (gsize obj_size,
                                          WSGetWidgetFunc get_widget,
                                          WSValidateFunc validate,
                                          WSAddToSizeGroupFunc add_to_size_group,
                                          WSFillConnectionFunc fill_connection,
                                          WSDestroyFunc destroy,
                                          const char *ui_resource);

void wireless_security_notify_changed (WirelessSecurity *sec);

void wireless_security_clear_ciphers (NMConnection *connection);

#define AUTH_NAME_COLUMN   0
#define AUTH_METHOD_COLUMN 1

void ws_802_1x_auth_combo_init (WirelessSecurity *sec,
                                GtkComboBox *combo,
                                GtkLabel *label,
                                GCallback auth_combo_changed_cb,
                                NMConnection *connection,
                                gboolean is_editor,
                                gboolean secrets_only);

void ws_802_1x_auth_combo_changed (GtkWidget *combo,
                                   WirelessSecurity *sec,
                                   GtkBox *vbox,
                                   GtkSizeGroup *size_group);

gboolean ws_802_1x_validate (GtkComboBox *combo, GError **error);

void ws_802_1x_add_to_size_group (GtkSizeGroup *size_group,
                                  GtkLabel *label,
                                  GtkComboBox *combo);

void ws_802_1x_fill_connection (GtkComboBox *combo,
                                NMConnection *connection);

void ws_802_1x_update_secrets (GtkComboBox *combo,
                               NMConnection *connection);

G_DEFINE_AUTOPTR_CLEANUP_FUNC (WirelessSecurity, wireless_security_unref)

#endif /* WIRELESS_SECURITY_H */

