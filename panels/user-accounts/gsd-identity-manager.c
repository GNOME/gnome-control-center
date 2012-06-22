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

#include "gsd-identity-manager.h"
#include "gsd-identity-manager-private.h"

enum {
        IDENTITY_ADDED,
        IDENTITY_REMOVED,
        IDENTITY_RENAMED,
        IDENTITY_REFRESHED,
        IDENTITY_NEEDS_RENEWAL,
        IDENTITY_EXPIRING,
        IDENTITY_EXPIRED,
        NUMBER_OF_SIGNALS,
};

static guint signals[NUMBER_OF_SIGNALS] = { 0 };

G_DEFINE_INTERFACE (GsdIdentityManager, gsd_identity_manager, G_TYPE_OBJECT);

static void
gsd_identity_manager_default_init (GsdIdentityManagerInterface *interface)
{
      signals[IDENTITY_ADDED] = g_signal_new ("identity-added",
                                              G_TYPE_FROM_INTERFACE (interface),
                                              G_SIGNAL_RUN_LAST,
                                              G_STRUCT_OFFSET (GsdIdentityManagerInterface, identity_added),
                                              NULL, NULL, NULL,
                                              G_TYPE_NONE, 1, GSD_TYPE_IDENTITY);
      signals[IDENTITY_REMOVED] = g_signal_new ("identity-removed",
                                                G_TYPE_FROM_INTERFACE (interface),
                                                G_SIGNAL_RUN_LAST,
                                                G_STRUCT_OFFSET (GsdIdentityManagerInterface, identity_removed),
                                                NULL, NULL, NULL,
                                                G_TYPE_NONE, 1, GSD_TYPE_IDENTITY);
      signals[IDENTITY_REFRESHED] = g_signal_new ("identity-refreshed",
                                                  G_TYPE_FROM_INTERFACE (interface),
                                                  G_SIGNAL_RUN_LAST,
                                                  G_STRUCT_OFFSET (GsdIdentityManagerInterface, identity_refreshed),
                                                  NULL, NULL, NULL,
                                                  G_TYPE_NONE, 1, GSD_TYPE_IDENTITY);
      signals[IDENTITY_RENAMED] = g_signal_new ("identity-renamed",
                                                G_TYPE_FROM_INTERFACE (interface),
                                                G_SIGNAL_RUN_LAST,
                                                G_STRUCT_OFFSET (GsdIdentityManagerInterface, identity_renamed),
                                                NULL, NULL, NULL,
                                                G_TYPE_NONE, 1, GSD_TYPE_IDENTITY);
      signals[IDENTITY_NEEDS_RENEWAL] = g_signal_new ("identity-needs-renewal",
                                                      G_TYPE_FROM_INTERFACE (interface),
                                                      G_SIGNAL_RUN_LAST,
                                                      G_STRUCT_OFFSET (GsdIdentityManagerInterface, identity_needs_renewal),
                                                      NULL, NULL, NULL,
                                                      G_TYPE_NONE, 1, GSD_TYPE_IDENTITY);
      signals[IDENTITY_EXPIRING] = g_signal_new ("identity-expiring",
                                                 G_TYPE_FROM_INTERFACE (interface),
                                                 G_SIGNAL_RUN_LAST,
                                                 G_STRUCT_OFFSET (GsdIdentityManagerInterface, identity_expiring),
                                                 NULL, NULL, NULL,
                                                 G_TYPE_NONE, 1, GSD_TYPE_IDENTITY);
      signals[IDENTITY_EXPIRED] = g_signal_new ("identity-expired",
                                                G_TYPE_FROM_INTERFACE (interface),
                                                G_SIGNAL_RUN_LAST,
                                                G_STRUCT_OFFSET (GsdIdentityManagerInterface, identity_expired),
                                                NULL, NULL, NULL,
                                                G_TYPE_NONE, 1, GSD_TYPE_IDENTITY);
}

GQuark
gsd_identity_manager_error_quark (void)
{
        static GQuark error_quark = 0;

        if (error_quark == 0) {
                error_quark = g_quark_from_static_string ("gsd-identity-manager-error");
        }

        return error_quark;
}

void
gsd_identity_manager_list_identities (GsdIdentityManager   *self,
                                      GCancellable         *cancellable,
                                      GAsyncReadyCallback   callback,
                                      gpointer              user_data)
{
        GSD_IDENTITY_MANAGER_GET_IFACE (self)->list_identities (self,
                                                               cancellable,
                                                               callback,
                                                               user_data);
}

GList *
gsd_identity_manager_list_identities_finish (GsdIdentityManager  *self,
                                             GAsyncResult        *result,
                                             GError             **error)
{
        return GSD_IDENTITY_MANAGER_GET_IFACE (self)->list_identities_finish (self,
                                                                             result,
                                                                             error);
}

void
gsd_identity_manager_renew_identity (GsdIdentityManager   *self,
                                     GsdIdentity          *identity,
                                     GCancellable         *cancellable,
                                     GAsyncReadyCallback   callback,
                                     gpointer              user_data)
{
        GSD_IDENTITY_MANAGER_GET_IFACE (self)->renew_identity (self, identity, cancellable, callback, user_data);
}

void
gsd_identity_manager_renew_identity_finish (GsdIdentityManager  *self,
                                            GAsyncResult        *result,
                                            GError             **error)
{
        GSD_IDENTITY_MANAGER_GET_IFACE (self)->renew_identity_finish (self, result, error);
}

void
gsd_identity_manager_sign_identity_in (GsdIdentityManager     *self,
                                       const char             *identifier,
                                       GsdIdentityInquiryFunc  inquiry_func,
                                       gpointer                inquiry_data,
                                       GCancellable           *cancellable,
                                       GAsyncReadyCallback     callback,
                                       gpointer                user_data)
{
        GSD_IDENTITY_MANAGER_GET_IFACE (self)->sign_identity_in (self, identifier, inquiry_func, inquiry_data, cancellable, callback, user_data);
}

GsdIdentity *
gsd_identity_manager_sign_identity_in_finish (GsdIdentityManager  *self,
                                              GAsyncResult        *result,
                                              GError             **error)
{
        return GSD_IDENTITY_MANAGER_GET_IFACE (self)->sign_identity_in_finish (self, result, error);
}

void
gsd_identity_manager_sign_identity_out (GsdIdentityManager   *self,
                                        GsdIdentity          *identity,
                                        GCancellable         *cancellable,
                                        GAsyncReadyCallback   callback,
                                        gpointer              user_data)
{
        GSD_IDENTITY_MANAGER_GET_IFACE (self)->sign_identity_out (self, identity, cancellable, callback, user_data);
}

void
gsd_identity_manager_sign_identity_out_finish (GsdIdentityManager  *self,
                                               GAsyncResult        *result,
                                               GError             **error)
{
        GSD_IDENTITY_MANAGER_GET_IFACE (self)->sign_identity_out_finish (self, result, error);
}

char *
gsd_identity_manager_name_identity (GsdIdentityManager *self,
                                    GsdIdentity        *identity)
{
        return GSD_IDENTITY_MANAGER_GET_IFACE (self)->name_identity (self,
                                                                     identity);
}

void
_gsd_identity_manager_emit_identity_added (GsdIdentityManager *self,
                                           GsdIdentity        *identity)
{
        g_signal_emit (G_OBJECT (self), signals[IDENTITY_ADDED], 0, identity);
}

void
_gsd_identity_manager_emit_identity_removed (GsdIdentityManager *self,
                                             GsdIdentity        *identity)
{
        g_signal_emit (G_OBJECT (self), signals[IDENTITY_REMOVED], 0, identity);
}

void
_gsd_identity_manager_emit_identity_renamed (GsdIdentityManager *self,
                                             GsdIdentity        *identity)
{
        g_signal_emit (G_OBJECT (self), signals[IDENTITY_RENAMED], 0, identity);
}

void
_gsd_identity_manager_emit_identity_refreshed (GsdIdentityManager *self,
                                               GsdIdentity        *identity)
{
        g_signal_emit (G_OBJECT (self), signals[IDENTITY_REFRESHED], 0, identity);
}

void
_gsd_identity_manager_emit_identity_needs_renewal (GsdIdentityManager *self,
                                                   GsdIdentity        *identity)
{
        g_signal_emit (G_OBJECT (self), signals[IDENTITY_NEEDS_RENEWAL], 0, identity);
}

void
_gsd_identity_manager_emit_identity_expiring (GsdIdentityManager *self,
                                              GsdIdentity        *identity)
{
        g_signal_emit (G_OBJECT (self), signals[IDENTITY_EXPIRING], 0, identity);
}

void
_gsd_identity_manager_emit_identity_expired (GsdIdentityManager *self,
                                             GsdIdentity        *identity)
{
        g_signal_emit (G_OBJECT (self), signals[IDENTITY_EXPIRED], 0, identity);
}

