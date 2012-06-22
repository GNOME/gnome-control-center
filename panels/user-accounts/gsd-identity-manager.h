/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2012 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 *
 * Authors: Ray Strode
 */

#ifndef __GSD_IDENTITY_MANAGER_H__
#define __GSD_IDENTITY_MANAGER_H__

#include <glib.h>
#include <glib-object.h>
#include <gio/gio.h>

#include "gsd-identity.h"
#include "gsd-identity-inquiry.h"

G_BEGIN_DECLS

#define GSD_TYPE_IDENTITY_MANAGER             (gsd_identity_manager_get_type ())
#define GSD_IDENTITY_MANAGER(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), GSD_TYPE_IDENTITY_MANAGER, GsdIdentityManager))
#define GSD_IDENTITY_MANAGER_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), GSD_TYPE_IDENTITY_MANAGER, GsdIdentityManagerInterface))
#define GSD_IS_IDENTITY_MANAGER(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GSD_TYPE_IDENTITY_MANAGER))
#define GSD_IDENTITY_MANAGER_GET_IFACE(obj)   (G_TYPE_INSTANCE_GET_INTERFACE((obj), GSD_TYPE_IDENTITY_MANAGER, GsdIdentityManagerInterface))
#define GSD_IDENTITY_MANAGER_ERROR            (gsd_identity_manager_error_quark ())

typedef struct _GsdIdentityManager          GsdIdentityManager;
typedef struct _GsdIdentityManagerInterface GsdIdentityManagerInterface;
typedef enum   _GsdIdentityManagerError     GsdIdentityManagerError;

struct _GsdIdentityManagerInterface
{
        GTypeInterface base_interface;

        /* Signals */
        void      (* identity_added)    (GsdIdentityManager *identity_manager,
                                         GsdIdentity        *identity);

        void      (* identity_removed)  (GsdIdentityManager *identity_manager,
                                         GsdIdentity        *identity);
        void      (* identity_renamed)  (GsdIdentityManager *identity_manager,
                                         GsdIdentity        *identity);
        void      (* identity_refreshed)  (GsdIdentityManager *identity_manager,
                                           GsdIdentity        *identity);
        void      (* identity_needs_renewal)  (GsdIdentityManager *identity_manager,
                                               GsdIdentity        *identity);
        void      (* identity_expiring) (GsdIdentityManager *identity_manager,
                                         GsdIdentity        *identity);
        void      (* identity_expired)  (GsdIdentityManager *identity_manager,
                                         GsdIdentity        *identity);

        /* Virtual Functions */
        void      (* list_identities)        (GsdIdentityManager   *identity_manager,
                                              GCancellable        *cancellable,
                                              GAsyncReadyCallback  callback,
                                              gpointer             user_data);
        GList *   (* list_identities_finish) (GsdIdentityManager  *identity_manager,
                                              GAsyncResult       *result,
                                              GError            **error);

        void      (* sign_identity_in)  (GsdIdentityManager     *identity_manager,
                                         const char             *identifier,
                                         GsdIdentityInquiryFunc  inquiry_func,
                                         gpointer                inquiry_data,
                                         GCancellable           *cancellable,
                                         GAsyncReadyCallback     callback,
                                         gpointer                user_data);
        GsdIdentity * (* sign_identity_in_finish)  (GsdIdentityManager  *identity_manager,
                                                    GAsyncResult       *result,
                                                    GError            **error);

        void      (* sign_identity_out)  (GsdIdentityManager   *identity_manager,
                                          GsdIdentity          *identity,
                                          GCancellable        *cancellable,
                                          GAsyncReadyCallback  callback,
                                          gpointer             user_data);
        void      (* sign_identity_out_finish)  (GsdIdentityManager  *identity_manager,
                                                 GAsyncResult       *result,
                                                 GError            **error);

        void      (* renew_identity)     (GsdIdentityManager   *identity_manager,
                                          GsdIdentity          *identity,
                                          GCancellable        *cancellable,
                                          GAsyncReadyCallback  callback,
                                          gpointer             user_data);
        void      (* renew_identity_finish)  (GsdIdentityManager  *identity_manager,
                                              GAsyncResult       *result,
                                              GError            **error);

        char  *   (* name_identity)      (GsdIdentityManager *identity_manager,
                                          GsdIdentity        *identity);
};

enum _GsdIdentityManagerError
{
        GSD_IDENTITY_MANAGER_ERROR_INITIALIZING,
        GSD_IDENTITY_MANAGER_ERROR_MONITORING,
        GSD_IDENTITY_MANAGER_ERROR_SIGNING_IN,
        GSD_IDENTITY_MANAGER_ERROR_SIGNING_OUT
};

GType      gsd_identity_manager_get_type         (void);
GQuark     gsd_identity_manager_error_quark      (void);

void       gsd_identity_manager_list_identities  (GsdIdentityManager   *identity_manager,
                                                  GCancellable         *cancellable,
                                                  GAsyncReadyCallback   callback,
                                                  gpointer              user_data);
GList *    gsd_identity_manager_list_identities_finish  (GsdIdentityManager  *identity_manager,
                                                         GAsyncResult        *result,
                                                         GError             **error);

void       gsd_identity_manager_sign_identity_in    (GsdIdentityManager     *identity_manager,
                                                     const char             *identifier,
                                                     GsdIdentityInquiryFunc  inquiry_func,
                                                     gpointer                inquiry_data,
                                                     GCancellable           *cancellable,
                                                     GAsyncReadyCallback     callback,
                                                     gpointer                user_data);
GsdIdentity * gsd_identity_manager_sign_identity_in_finish (GsdIdentityManager *identity_manager,
                                                            GAsyncResult       *result,
                                                            GError            **error);

void       gsd_identity_manager_sign_identity_out    (GsdIdentityManager  *identity_manager,
                                                      GsdIdentity         *identity,
                                                      GCancellable        *cancellable,
                                                      GAsyncReadyCallback  callback,
                                                      gpointer             user_data);
void       gsd_identity_manager_sign_identity_out_finish (GsdIdentityManager *identity_manager,
                                                          GAsyncResult       *result,
                                                          GError            **error);

void       gsd_identity_manager_renew_identity (GsdIdentityManager   *identity_manager,
                                                GsdIdentity          *identity,
                                                GCancellable         *cancellable,
                                                GAsyncReadyCallback   callback,
                                                gpointer              user_data);
void       gsd_identity_manager_renew_identity_finish (GsdIdentityManager *identity_manager,
                                                       GAsyncResult       *result,
                                                       GError            **error);

char      *gsd_identity_manager_name_identity (GsdIdentityManager *identity_manager,
                                               GsdIdentity        *identity);

G_END_DECLS

#endif /* __GSD_IDENTITY_MANAGER_H__ */
