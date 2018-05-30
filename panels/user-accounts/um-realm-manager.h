/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright 2012  Red Hat, Inc.
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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Written by: Stef Walter <stefw@gnome.org>
 */

#pragma once

#include "um-realm-generated.h"

G_BEGIN_DECLS

typedef enum {
	UM_REALM_ERROR_BAD_LOGIN,
	UM_REALM_ERROR_BAD_PASSWORD,
	UM_REALM_ERROR_CANNOT_AUTH,
	UM_REALM_ERROR_GENERIC,
} UmRealmErrors;

#define UM_REALM_ERROR             (um_realm_error_get_quark ())

GQuark           um_realm_error_get_quark         (void) G_GNUC_CONST;

#define UM_TYPE_REALM_MANAGER (um_realm_manager_get_type ())
G_DECLARE_FINAL_TYPE (UmRealmManager, um_realm_manager, UM, REALM_MANAGER, UmRealmObjectManagerClient)

void             um_realm_manager_new             (GCancellable *cancellable,
                                                   GAsyncReadyCallback callback,
                                                   gpointer user_data);

UmRealmManager * um_realm_manager_new_finish      (GAsyncResult *result,
                                                   GError **error);

void             um_realm_manager_discover        (UmRealmManager *self,
                                                   const gchar *input,
                                                   GCancellable *cancellable,
                                                   GAsyncReadyCallback callback,
                                                   gpointer user_data);

GList *          um_realm_manager_discover_finish (UmRealmManager *self,
                                                   GAsyncResult *result,
                                                   GError **error);

GList *          um_realm_manager_get_realms      (UmRealmManager *self);

void             um_realm_login                   (UmRealmObject *realm,
                                                   const gchar *login,
                                                   const gchar *password,
                                                   GCancellable *cancellable,
                                                   GAsyncReadyCallback callback,
                                                   gpointer user_data);

GBytes *         um_realm_login_finish            (GAsyncResult *result,
                                                   GError **error);

gboolean         um_realm_join_as_user            (UmRealmObject *realm,
                                                   const gchar *login,
                                                   const gchar *password,
                                                   GBytes *credentials,
                                                   GCancellable *cancellable,
                                                   GAsyncReadyCallback callback,
                                                   gpointer user_data)
                                                   G_GNUC_WARN_UNUSED_RESULT;

gboolean         um_realm_join_as_admin           (UmRealmObject *realm,
                                                   const gchar *login,
                                                   const gchar *password,
                                                   GBytes *credentials,
                                                   GCancellable *cancellable,
                                                   GAsyncReadyCallback callback,
                                                   gpointer user_data)
                                                   G_GNUC_WARN_UNUSED_RESULT;

gboolean         um_realm_join_finish             (UmRealmObject *realm,
                                                   GAsyncResult *result,
                                                   GError **error);

gboolean         um_realm_is_configured           (UmRealmObject *realm);

gchar *          um_realm_calculate_login         (UmRealmCommon *realm,
                                                   const gchar *username);

G_END_DECLS
