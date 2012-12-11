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

#ifndef WIRELESS_SECURITY_H
#define WIRELESS_SECURITY_H

#include <glib.h>
#include <gtk/gtk.h>

#include <nm-connection.h>

typedef struct _WirelessSecurity WirelessSecurity;

typedef void (*WSChangedFunc) (WirelessSecurity *sec, gpointer user_data);

typedef void (*WSAddToSizeGroupFunc) (WirelessSecurity *sec, GtkSizeGroup *group);
typedef void (*WSFillConnectionFunc) (WirelessSecurity *sec, NMConnection *connection);
typedef void (*WSUpdateSecretsFunc)  (WirelessSecurity *sec, NMConnection *connection);
typedef void (*WSDestroyFunc)        (WirelessSecurity *sec);
typedef gboolean (*WSValidateFunc)   (WirelessSecurity *sec, const GByteArray *ssid);
typedef GtkWidget * (*WSNagUserFunc) (WirelessSecurity *sec);

struct _WirelessSecurity {
	guint32 refcount;
	gsize obj_size;
	GtkBuilder *builder;
	GtkWidget *ui_widget;
	WSChangedFunc changed_notify;
	gpointer changed_notify_data;
	const char *default_field;
	gboolean adhoc_compatible;

	WSAddToSizeGroupFunc add_to_size_group;
	WSFillConnectionFunc fill_connection;
	WSUpdateSecretsFunc update_secrets;
	WSValidateFunc validate;
	WSNagUserFunc nag_user;
	WSDestroyFunc destroy;
};

#define WIRELESS_SECURITY(x) ((WirelessSecurity *) x)


GtkWidget *wireless_security_get_widget (WirelessSecurity *sec);

void wireless_security_set_changed_notify (WirelessSecurity *sec,
                                           WSChangedFunc func,
                                           gpointer user_data);

gboolean wireless_security_validate (WirelessSecurity *sec, const GByteArray *ssid);

void wireless_security_add_to_size_group (WirelessSecurity *sec,
                                          GtkSizeGroup *group);

void wireless_security_fill_connection (WirelessSecurity *sec,
                                        NMConnection *connection);

void wireless_security_update_secrets (WirelessSecurity *sec,
                                       NMConnection *connection);

GtkWidget * wireless_security_nag_user (WirelessSecurity *sec);

gboolean wireless_security_adhoc_compatible (WirelessSecurity *sec);

WirelessSecurity *wireless_security_ref (WirelessSecurity *sec);

void wireless_security_unref (WirelessSecurity *sec);

GType wireless_security_get_g_type (void);

/* Below for internal use only */

#include "ws-wep-key.h"
#include "ws-wpa-psk.h"
#include "ws-leap.h"
#include "ws-wpa-eap.h"
#include "ws-dynamic-wep.h"

WirelessSecurity *wireless_security_init (gsize obj_size,
                                          WSValidateFunc validate,
                                          WSAddToSizeGroupFunc add_to_size_group,
                                          WSFillConnectionFunc fill_connection,
                                          WSUpdateSecretsFunc update_secrets,
                                          WSDestroyFunc destroy,
                                          const char *ui_file,
                                          const char *ui_widget_name,
                                          const char *default_field);

void wireless_security_changed_cb (GtkWidget *entry, gpointer user_data);

void wireless_security_clear_ciphers (NMConnection *connection);

#define AUTH_NAME_COLUMN   0
#define AUTH_METHOD_COLUMN 1

GtkWidget *ws_802_1x_auth_combo_init (WirelessSecurity *sec,
                                      const char *combo_name,
                                      const char *combo_label,
                                      GCallback auth_combo_changed_cb,
                                      NMConnection *connection,
                                      gboolean is_editor,
                                      gboolean secrets_only);

void ws_802_1x_auth_combo_changed (GtkWidget *combo,
                                   WirelessSecurity *sec,
                                   const char *vbox_name,
                                   GtkSizeGroup *size_group);

gboolean ws_802_1x_validate (WirelessSecurity *sec, const char *combo_name);

void ws_802_1x_add_to_size_group (WirelessSecurity *sec,
                                  GtkSizeGroup *size_group,
                                  const char *label_name,
                                  const char *combo_name);

void ws_802_1x_fill_connection (WirelessSecurity *sec,
                                const char *combo_name,
                                NMConnection *connection);

void ws_802_1x_update_secrets (WirelessSecurity *sec,
                               const char *combo_name,
                               NMConnection *connection);

GtkWidget * ws_802_1x_nag_user (WirelessSecurity *sec,
                                const char *combo_name);

#endif /* WIRELESS_SECURITY_H */

