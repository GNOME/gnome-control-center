/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2012 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

#include "config.h"

#include <glib-object.h>
#include <glib/gi18n.h>
#include <gio/gio.h>

#include "um-identity-manager.h"
#include "um-identity-manager-private.h"

enum {
        IDENTITY_ADDED,
        IDENTITY_REMOVED,
        IDENTITY_EXPIRED,
        IDENTITY_RENEWED,
        IDENTITY_RENAMED,
        NUMBER_OF_SIGNALS,
};

static guint signals[NUMBER_OF_SIGNALS] = { 0 };

G_DEFINE_INTERFACE (UmIdentityManager, um_identity_manager, G_TYPE_OBJECT);

static void
um_identity_manager_default_init (UmIdentityManagerInterface *interface)
{
      signals[IDENTITY_ADDED] = g_signal_new ("identity-added",
                                              G_TYPE_FROM_INTERFACE (interface),
                                              G_SIGNAL_RUN_LAST,
                                              G_STRUCT_OFFSET (UmIdentityManagerInterface, identity_added),
                                              NULL, NULL, NULL,
                                              G_TYPE_NONE, 1, UM_TYPE_IDENTITY);
      signals[IDENTITY_REMOVED] = g_signal_new ("identity-removed",
                                                G_TYPE_FROM_INTERFACE (interface),
                                                G_SIGNAL_RUN_LAST,
                                                G_STRUCT_OFFSET (UmIdentityManagerInterface, identity_removed),
                                                NULL, NULL, NULL,
                                                G_TYPE_NONE, 1, UM_TYPE_IDENTITY);
      signals[IDENTITY_EXPIRED] = g_signal_new ("identity-expired",
                                                G_TYPE_FROM_INTERFACE (interface),
                                                G_SIGNAL_RUN_LAST,
                                                G_STRUCT_OFFSET (UmIdentityManagerInterface, identity_expired),
                                                NULL, NULL, NULL,
                                                G_TYPE_NONE, 1, UM_TYPE_IDENTITY);
      signals[IDENTITY_RENEWED] = g_signal_new ("identity-renewed",
                                                G_TYPE_FROM_INTERFACE (interface),
                                                G_SIGNAL_RUN_LAST,
                                                G_STRUCT_OFFSET (UmIdentityManagerInterface, identity_renewed),
                                                NULL, NULL, NULL,
                                                G_TYPE_NONE, 1, UM_TYPE_IDENTITY);
      signals[IDENTITY_RENAMED] = g_signal_new ("identity-renamed",
                                                G_TYPE_FROM_INTERFACE (interface),
                                                G_SIGNAL_RUN_LAST,
                                                G_STRUCT_OFFSET (UmIdentityManagerInterface, identity_renamed),
                                                NULL, NULL, NULL,
                                                G_TYPE_NONE, 1, UM_TYPE_IDENTITY);
}

GQuark
um_identity_manager_error_quark (void)
{
        static GQuark error_quark = 0;

        if (error_quark == 0) {
                error_quark = g_quark_from_static_string ("um-identity-manager-error");
        }

        return error_quark;
}

void
um_identity_manager_list_identities (UmIdentityManager   *self,
                                     GCancellable        *cancellable,
                                     GAsyncReadyCallback  callback,
                                     gpointer             user_data)
{
        g_return_if_fail (UM_IDENTITY_MANAGER_GET_IFACE (self)->list_identities);
        UM_IDENTITY_MANAGER_GET_IFACE (self)->list_identities (self,
                                                               cancellable,
                                                               callback,
                                                               user_data);
}

GList *
um_identity_manager_list_identities_finish (UmIdentityManager  *self,
                                            GAsyncResult       *result,
                                            GError            **error)
{
        g_return_val_if_fail (UM_IDENTITY_MANAGER_GET_IFACE (self)->list_identities_finish, NULL);
        return UM_IDENTITY_MANAGER_GET_IFACE (self)->list_identities_finish (self,
                                                                             result,
                                                                             error);
}

void
um_identity_manager_sign_identity_out (UmIdentityManager   *self,
                                       UmIdentity          *identity,
                                       GCancellable        *cancellable,
                                       GAsyncReadyCallback  callback,
                                       gpointer             user_data)
{
        g_return_if_fail (UM_IDENTITY_MANAGER_GET_IFACE (self)->sign_identity_out);
        UM_IDENTITY_MANAGER_GET_IFACE (self)->sign_identity_out (self, identity, cancellable, callback, user_data);
}

void
um_identity_manager_sign_identity_out_finish (UmIdentityManager  *self,
                                              GAsyncResult       *result,
                                              GError            **error)
{
        g_return_if_fail (UM_IDENTITY_MANAGER_GET_IFACE (self)->sign_identity_out_finish);
        UM_IDENTITY_MANAGER_GET_IFACE (self)->sign_identity_out_finish (self, result, error);
}

char *
um_identity_manager_name_identity (UmIdentityManager *self,
                                   UmIdentity        *identity)
{
        g_return_val_if_fail (UM_IDENTITY_MANAGER_GET_IFACE (self)->name_identity, NULL);
        return UM_IDENTITY_MANAGER_GET_IFACE (self)->name_identity (self,
                                                                    identity);
}

void
_um_identity_manager_emit_identity_added (UmIdentityManager *self,
                                          UmIdentity        *identity)
{
        g_signal_emit (G_OBJECT (self), signals[IDENTITY_ADDED], 0, identity);
}

void
_um_identity_manager_emit_identity_removed (UmIdentityManager *self,
                                            UmIdentity        *identity)
{
        g_signal_emit (G_OBJECT (self), signals[IDENTITY_REMOVED], 0, identity);
}

void
_um_identity_manager_emit_identity_expired (UmIdentityManager *self,
                                            UmIdentity        *identity)
{
        g_signal_emit (G_OBJECT (self), signals[IDENTITY_EXPIRED], 0, identity);
}

void
_um_identity_manager_emit_identity_renewed (UmIdentityManager *self,
                                            UmIdentity        *identity)
{
        g_signal_emit (G_OBJECT (self), signals[IDENTITY_RENEWED], 0, identity);
}

void
_um_identity_manager_emit_identity_renamed (UmIdentityManager *self,
                                            UmIdentity        *identity)
{
        g_signal_emit (G_OBJECT (self), signals[IDENTITY_RENAMED], 0, identity);
}
