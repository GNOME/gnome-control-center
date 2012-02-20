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

#ifndef __UM_IDENTITY_MANAGER_H__
#define __UM_IDENTITY_MANAGER_H__

#include <glib.h>
#include <glib-object.h>
#include <gio/gio.h>

#include "um-identity.h"

G_BEGIN_DECLS

#define UM_TYPE_IDENTITY_MANAGER             (um_identity_manager_get_type ())
#define UM_IDENTITY_MANAGER(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), UM_TYPE_IDENTITY_MANAGER, UmIdentityManager))
#define UM_IDENTITY_MANAGER_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), UM_TYPE_IDENTITY_MANAGER, UmIdentityManagerInterface))
#define UM_IS_IDENTITY_MANAGER(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), UM_TYPE_IDENTITY_MANAGER))
#define UM_IDENTITY_MANAGER_GET_IFACE(obj)   (G_TYPE_INSTANCE_GET_INTERFACE((obj), UM_TYPE_IDENTITY_MANAGER, UmIdentityManagerInterface))
#define UM_IDENTITY_MANAGER_ERROR            (um_identity_manager_error_quark ())

typedef struct _UmIdentityManager          UmIdentityManager;
typedef struct _UmIdentityManagerInterface UmIdentityManagerInterface;
typedef enum   _UmIdentityManagerError     UmIdentityManagerError;

struct _UmIdentityManagerInterface
{
        GTypeInterface base_interface;

        /* Signals */
        void      (* identity_added)    (UmIdentityManager *identity_manager,
                                         UmIdentity        *identity);

        void      (* identity_removed)  (UmIdentityManager *identity_manager,
                                         UmIdentity        *identity);
        void      (* identity_expired)  (UmIdentityManager *identity_manager,
                                         UmIdentity        *identity);
        void      (* identity_renewed)  (UmIdentityManager *identity_manager,
                                         UmIdentity        *identity);
        void      (* identity_renamed)  (UmIdentityManager *identity_manager,
                                         UmIdentity        *identity);

        /* Virtual Functions */
        void      (* list_identities)        (UmIdentityManager   *identity_manager,
                                              GCancellable        *cancellable,
                                              GAsyncReadyCallback  callback,
                                              gpointer             user_data);
        GList *   (* list_identities_finish) (UmIdentityManager  *identity_manager,
                                              GAsyncResult       *result,
                                              GError            **error);

        void      (* sign_identity_out)  (UmIdentityManager   *identity_manager,
                                          UmIdentity          *identity,
                                          GCancellable        *cancellable,
                                          GAsyncReadyCallback  callback,
                                          gpointer             user_data);
        void      (* sign_identity_out_finish)  (UmIdentityManager  *identity_manager,
                                                 GAsyncResult       *result,
                                                 GError            **error);

        char  *   (* name_identity)      (UmIdentityManager *identity_manager,
                                          UmIdentity        *identity);
};

enum _UmIdentityManagerError {
        UM_IDENTITY_MANAGER_ERROR_INITIALIZING,
        UM_IDENTITY_MANAGER_ERROR_MONITORING,
        UM_IDENTITY_MANAGER_ERROR_SIGNING_OUT
};

GType      um_identity_manager_get_type         (void);
GQuark     um_identity_manager_error_quark      (void);

void       um_identity_manager_list_identities  (UmIdentityManager   *identity_manager,
                                                 GCancellable        *cancellable,
                                                 GAsyncReadyCallback  callback,
                                                 gpointer             user_data);
GList *    um_identity_manager_list_identities_finish  (UmIdentityManager  *identity_manager,
                                                        GAsyncResult       *result,
                                                        GError            **error);
void       um_identity_manager_sign_identity_out    (UmIdentityManager   *identity_manager,
                                                     UmIdentity          *identity,
                                                     GCancellable        *cancellable,
                                                     GAsyncReadyCallback  callback,
                                                     gpointer             user_data);
void       um_identity_manager_sign_identity_out_finish (UmIdentityManager *identity_manager,
                                                         GAsyncResult       *result,
                                                         GError            **error);
char      *um_identity_manager_name_identity    (UmIdentityManager *identity_manager,
                                                 UmIdentity        *identity);

G_END_DECLS

#endif /* __UM_IDENTITY_MANAGER_H__ */
