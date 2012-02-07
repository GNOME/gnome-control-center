/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2009-2010 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 */

#ifndef __UM_USER_MANAGER__
#define __UM_USER_MANAGER__

#include <glib-object.h>
#include <gio/gio.h>

#include "um-user.h"

G_BEGIN_DECLS

#define UM_TYPE_USER_MANAGER         (um_user_manager_get_type ())
#define UM_USER_MANAGER(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), UM_TYPE_USER_MANAGER, UmUserManager))
#define UM_USER_MANAGER_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), UM_TYPE_USER_MANAGER, UmUserManagerClass))
#define UM_IS_USER_MANAGER(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), UM_TYPE_USER_MANAGER))
#define UM_IS_USER_MANAGER_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), UM_TYPE_USER_MANAGER))
#define UM_USER_MANAGER_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), UM_TYPE_USER_MANAGER, UmUserManagerClass))

typedef struct
{
        GObject parent;

        GDBusProxy *proxy;

        GHashTable *user_by_object_path;
        GHashTable *user_by_name;

        gboolean no_service;
} UmUserManager;

typedef struct
{
        GObjectClass   parent_class;

        void          (* users_loaded)              (UmUserManager *user_managaer);
        void          (* user_added)                (UmUserManager *user_manager,
                                                     UmUser        *user);
        void          (* user_removed)              (UmUserManager *user_manager,
                                                     UmUser        *user);
        void          (* user_changed)              (UmUserManager *user_manager,
                                                     UmUser        *user);
} UmUserManagerClass;


typedef enum {
        UM_USER_MANAGER_ERROR_FAILED,
        UM_USER_MANAGER_ERROR_USER_EXISTS,
        UM_USER_MANAGER_ERROR_USER_DOES_NOT_EXIST,
        UM_USER_MANAGER_ERROR_PERMISSION_DENIED
} UmUserManagerError;

#define UM_USER_MANAGER_ERROR um_user_manager_error_quark ()

GQuark             um_user_manager_error_quark (void);

GType              um_user_manager_get_type              (void);

UmUserManager *    um_user_manager_ref_default           (void);

gboolean           um_user_manager_no_service            (UmUserManager *manager);

GSList *           um_user_manager_list_users            (UmUserManager *manager);
UmUser *           um_user_manager_get_user              (UmUserManager *manager,
                                                          const char    *user_name);
UmUser *           um_user_manager_get_user_by_id        (UmUserManager *manager,
                                                          uid_t          uid);

void               um_user_manager_create_user           (UmUserManager       *manager,
                                                          const char          *user_name,
                                                          const char          *real_name,
                                                          gint                 account_type,
                                                          GAsyncReadyCallback  done,
                                                          gpointer             user_data,
                                                          GDestroyNotify       destroy);
gboolean           um_user_manager_create_user_finish    (UmUserManager       *manager,
                                                          GAsyncResult        *result,
                                                          UmUser             **user,
                                                          GError             **error);
void               um_user_manager_delete_user           (UmUserManager       *manager,
                                                          UmUser              *user,
                                                          gboolean             remove_files,
                                                          GAsyncReadyCallback  done,
                                                          gpointer             user_data,
                                                          GDestroyNotify       destroy);
gboolean           um_user_manager_delete_user_finish    (UmUserManager       *manager,
                                                          GAsyncResult        *result,
                                                          GError             **error);

G_END_DECLS

#endif /* __UM_USER_MANAGER__ */
