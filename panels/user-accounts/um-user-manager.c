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
 * Written by: Matthias Clasen <mclasen@redhat.com>
 */

#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <pwd.h>

#ifdef HAVE_PATHS_H
#include <paths.h>
#endif /* HAVE_PATHS_H */

#include <glib.h>
#include <glib/gi18n.h>
#include <glib/gstdio.h>
#include <glib-object.h>
#include <gio/gio.h>

#include "um-user-manager.h"

enum {
        USERS_LOADED,
        USER_ADDED,
        USER_REMOVED,
        USER_CHANGED,
        LAST_SIGNAL
};

static guint signals [LAST_SIGNAL] = { 0, };

static void     um_user_manager_class_init (UmUserManagerClass *klass);
static void     um_user_manager_init       (UmUserManager      *user_manager);
static void     um_user_manager_finalize   (GObject            *object);

static gpointer user_manager_object = NULL;

G_DEFINE_TYPE (UmUserManager, um_user_manager, G_TYPE_OBJECT)

static void
um_user_manager_class_init (UmUserManagerClass *klass)
{
        GObjectClass   *object_class = G_OBJECT_CLASS (klass);

        object_class->finalize = um_user_manager_finalize;

       signals [USERS_LOADED] =
                g_signal_new ("users-loaded",
                              G_TYPE_FROM_CLASS (klass),
                              G_SIGNAL_RUN_LAST,
                              G_STRUCT_OFFSET (UmUserManagerClass, users_loaded),
                              NULL, NULL,
                              g_cclosure_marshal_VOID__VOID,
                              G_TYPE_NONE, 0);

        signals [USER_ADDED] =
                g_signal_new ("user-added",
                              G_TYPE_FROM_CLASS (klass),
                              G_SIGNAL_RUN_LAST,
                              G_STRUCT_OFFSET (UmUserManagerClass, user_added),
                              NULL, NULL,
                              g_cclosure_marshal_VOID__OBJECT,
                              G_TYPE_NONE, 1, UM_TYPE_USER);
        signals [USER_REMOVED] =
                g_signal_new ("user-removed",
                              G_TYPE_FROM_CLASS (klass),
                              G_SIGNAL_RUN_LAST,
                              G_STRUCT_OFFSET (UmUserManagerClass, user_removed),
                              NULL, NULL,
                              g_cclosure_marshal_VOID__OBJECT,
                              G_TYPE_NONE, 1, UM_TYPE_USER);
        signals [USER_CHANGED] =
                g_signal_new ("user-changed",
                              G_TYPE_FROM_CLASS (klass),
                              G_SIGNAL_RUN_LAST,
                              G_STRUCT_OFFSET (UmUserManagerClass, user_changed),
                              NULL, NULL,
                              g_cclosure_marshal_VOID__OBJECT,
                              G_TYPE_NONE, 1, UM_TYPE_USER);
}


/* We maintain a ring for each group of users with the same real name.
 * We need this to pick the right display names.
 */
static void
remove_user_from_dupe_ring (UmUserManager *manager,
                            UmUser        *user)
{
        GList *dupes;
        UmUser *dup;

        um_user_show_short_display_name (user);

        dupes = g_object_get_data (G_OBJECT (user), "dupes");

        if (dupes == NULL) {
                return;
        }

        if (dupes->next == dupes->prev) {
                dup = dupes->next->data;
                um_user_show_short_display_name (dup);
                g_signal_emit (manager, signals[USER_CHANGED], 0, dup);

                g_list_free_1 (dupes->next);
                g_object_set_data (G_OBJECT (dup), "dupes", NULL);
        }
        else {
                dupes->next->prev = dupes->prev;
                dupes->prev->next = dupes->next;
        }

        g_list_free_1 (dupes);
        g_object_set_data (G_OBJECT (user), "dupes", NULL);
}

static gboolean
match_real_name_hrfunc (gpointer key,
                        gpointer value,
                        gpointer user)
{
        return (value != user && g_strcmp0 (um_user_get_real_name (user), um_user_get_real_name (value)) == 0);
}

static void
add_user_to_dupe_ring (UmUserManager *manager,
                       UmUser        *user)
{
        UmUser *dup;
        GList *dupes;
        GList *l;

        dup = g_hash_table_find (manager->user_by_object_path,
                                 match_real_name_hrfunc, user);

        if (!dup) {
                return;
        }

        um_user_show_full_display_name (user);

        dupes = g_object_get_data (G_OBJECT (dup), "dupes");
        if (!dupes) {
                um_user_show_full_display_name (dup);
                g_signal_emit (manager, signals[USER_CHANGED], 0, dup);
                dupes = g_list_append (NULL, dup);
                g_object_set_data (G_OBJECT (dup), "dupes", dupes);
                dupes->next = dupes->prev = dupes;
        }

        l = g_list_append (NULL, user);
        g_object_set_data (G_OBJECT (user), "dupes", l);
        l->prev = dupes->prev;
        dupes->prev->next = l;
        l->next = dupes;
        dupes->prev = l;
}

static void
user_changed_handler (UmUser        *user,
                      UmUserManager *manager)
{
        remove_user_from_dupe_ring (manager, user);
        add_user_to_dupe_ring (manager, user);
        g_signal_emit (manager, signals[USER_CHANGED], 0, user);
}

static void
user_added_handler (DBusGProxy *proxy,
                    const char *object_path,
                    gpointer    user_data)
{
        UmUserManager *manager = UM_USER_MANAGER (user_data);
        UmUser *user;
 
        if (g_hash_table_lookup (manager->user_by_object_path, object_path))
                return;

        user = um_user_new_from_object_path (object_path);
        if (!user)
                return;

        add_user_to_dupe_ring (manager, user);

        g_signal_connect (user, "changed",
                          G_CALLBACK (user_changed_handler), manager);
        g_hash_table_insert (manager->user_by_object_path, g_strdup (um_user_get_object_path (user)), g_object_ref (user));
        g_hash_table_insert (manager->user_by_name, g_strdup (um_user_get_user_name (user)), g_object_ref (user));

        g_signal_emit (manager, signals[USER_ADDED], 0, user);
        g_object_unref (user);
}

static void
user_deleted_handler (DBusGProxy *proxy,
                      const char *object_path,
                      gpointer    user_data)
{
        UmUserManager *manager = UM_USER_MANAGER (user_data);
        UmUser *user;

        user = g_hash_table_lookup (manager->user_by_object_path, object_path);
        g_object_ref (user);
        g_signal_handlers_disconnect_by_func (user, user_changed_handler, manager);

        remove_user_from_dupe_ring (manager, user);

        g_hash_table_remove (manager->user_by_object_path, um_user_get_object_path (user));
        g_hash_table_remove (manager->user_by_name, um_user_get_user_name (user));
        g_signal_emit (manager, signals[USER_REMOVED], 0, user);
        g_object_unref (user);
}

static void
add_user (const gchar   *object_path,
          UmUserManager *manager)
{
        user_added_handler (NULL, object_path, manager);
}

static void
got_users (DBusGProxy     *proxy,
           DBusGProxyCall *call_id,
           gpointer        data)
{
        UmUserManager *manager = data;
        GError *error = NULL;
        GPtrArray *paths;

        if (!dbus_g_proxy_end_call (proxy,
                                    call_id,
                                    &error,
                                    dbus_g_type_get_collection ("GPtrArray", DBUS_TYPE_G_OBJECT_PATH), &paths,
                                    G_TYPE_INVALID)) {
                manager->no_service = TRUE;
                g_error_free (error);
                goto done;
        }

        g_ptr_array_foreach (paths, (GFunc)add_user, manager);

        g_ptr_array_foreach (paths, (GFunc)g_free, NULL);
        g_ptr_array_free (paths, TRUE);

 done:
        g_signal_emit (G_OBJECT (manager), signals[USERS_LOADED], 0);
}

static void
get_users (UmUserManager *manager)
{
        g_debug ("calling 'ListCachedUsers'");
        dbus_g_proxy_begin_call (manager->proxy,
                                 "ListCachedUsers",
                                 got_users,
                                 manager,
                                 NULL,
                                 G_TYPE_INVALID);
}

static void
um_user_manager_init (UmUserManager *manager)
{
        GError *error = NULL;

        manager->user_by_object_path = g_hash_table_new_full (g_str_hash,
                                                              g_str_equal,
                                                              g_free,
                                                              g_object_unref);
        manager->user_by_name = g_hash_table_new_full (g_str_hash,
                                                       g_str_equal,
                                                       g_free,
                                                       g_object_unref);

        manager->bus = dbus_g_bus_get (DBUS_BUS_SYSTEM, &error);
        if (manager->bus == NULL) {
                g_warning ("Couldn't connect to system bus: %s", error->message);
                g_error_free (error);
                goto error;
        }

        manager->proxy = dbus_g_proxy_new_for_name (manager->bus,
                                                    "org.freedesktop.Accounts",
                                                    "/org/freedesktop/Accounts",
                                                    "org.freedesktop.Accounts");

        dbus_g_proxy_add_signal (manager->proxy, "UserAdded", DBUS_TYPE_G_OBJECT_PATH, G_TYPE_INVALID);
        dbus_g_proxy_add_signal (manager->proxy, "UserDeleted", DBUS_TYPE_G_OBJECT_PATH, G_TYPE_INVALID);

        dbus_g_proxy_connect_signal (manager->proxy, "UserAdded",
                                     G_CALLBACK (user_added_handler), manager, NULL);
        dbus_g_proxy_connect_signal (manager->proxy, "UserDeleted",
                                     G_CALLBACK (user_deleted_handler), manager, NULL);

        get_users (manager);

 error: ;
}

static void
clear_dup (gpointer key,
           gpointer value,
           gpointer data)
{
        GList *dupes;

        /* don't bother maintaining the ring, we're destroying the
         * entire hash table anyway
         */
        dupes = g_object_get_data (G_OBJECT (value), "dupes");

        if (dupes) {
                g_list_free_1 (dupes);
                g_object_set_data (G_OBJECT (value), "dupes", NULL);
        }
}

static void
um_user_manager_finalize (GObject *object)
{
        UmUserManager *manager;

        manager = UM_USER_MANAGER (object);

        g_hash_table_foreach (manager->user_by_object_path, clear_dup, NULL);
        g_hash_table_destroy (manager->user_by_object_path);
        g_hash_table_destroy (manager->user_by_name);

        G_OBJECT_CLASS (um_user_manager_parent_class)->finalize (object);
}

UmUserManager *
um_user_manager_ref_default (void)
{
        if (user_manager_object != NULL) {
                g_object_ref (user_manager_object);
        } else {
                user_manager_object = g_object_new (UM_TYPE_USER_MANAGER, NULL);
                g_object_add_weak_pointer (user_manager_object,
                                           (gpointer *) &user_manager_object);
        }

        return UM_USER_MANAGER (user_manager_object);
}

typedef struct {
        UmUserManager       *manager;
        gchar               *user_name;
        GAsyncReadyCallback  callback;
        gpointer             data;
        GDestroyNotify       destroy;
}  AsyncUserOpData;

static void
async_user_op_data_free (gpointer d)
{
        AsyncUserOpData *data = d;

        g_object_unref (data->manager);

        g_free (data->user_name);

        if (data->destroy)
                data->destroy (data->data);

        g_free (data);
}

static void
create_user_done (DBusGProxy     *proxy,
                  DBusGProxyCall *call_id,
                  gpointer        user_data)
{
        AsyncUserOpData *data = user_data;
        gchar *path;
        GError *error;
        GSimpleAsyncResult *res;

        res = g_simple_async_result_new (G_OBJECT (data->manager),
                                         data->callback,
                                         data->data,
                                         um_user_manager_create_user);
        error = NULL;
        if (!dbus_g_proxy_end_call (proxy,
                                    call_id,
                                    &error,
                                    DBUS_TYPE_G_OBJECT_PATH, &path,
                                    G_TYPE_INVALID)) {
                /* dbus-glib fail:
                 * We have to translate the errors manually here, since
                 * calling dbus_g_error_has_name on the error returned in
                 * um_user_manager_create_user_finish doesn't work.
                 */
                if (dbus_g_error_has_name (error, "org.freedesktop.Accounts.Error.PermissionDenied")) {
                        g_simple_async_result_set_error (res,
                                                         UM_USER_MANAGER_ERROR,
                                                         UM_USER_MANAGER_ERROR_PERMISSION_DENIED,
                                                         "Not authorized");
                }
                else if (dbus_g_error_has_name (error, "org.freedesktop.Accounts.Error.UserExists")) {
                        g_simple_async_result_set_error (res,
                                                         UM_USER_MANAGER_ERROR,
                                                         UM_USER_MANAGER_ERROR_USER_EXISTS,
                                                         _("A user with name '%s' already exists."),
                                                         data->user_name);
                }
                else {
                        g_simple_async_result_set_from_error (res, error);
                }
                g_error_free (error);
        }
        else {
                g_simple_async_result_set_op_res_gpointer (res, path, g_free);
        }

        data->callback (G_OBJECT (data->manager), G_ASYNC_RESULT (res), data->data);
}

gboolean
um_user_manager_create_user_finish (UmUserManager  *manager,
                                    GAsyncResult   *result,
                                    UmUser        **user,
                                    GError        **error)
{
        gchar *path;
        GSimpleAsyncResult *res;

        res = G_SIMPLE_ASYNC_RESULT (result);

        *user = NULL;

        if (g_simple_async_result_propagate_error (res, error)) {
                return FALSE;
        }

        path = g_simple_async_result_get_op_res_gpointer (res);
        *user = g_hash_table_lookup (manager->user_by_object_path, path);

        return TRUE;
}

void
um_user_manager_create_user (UmUserManager       *manager,
                             const char          *user_name,
                             const char          *real_name,
                             gint                 account_type,
                             GAsyncReadyCallback  done,
                             gpointer             done_data,
                             GDestroyNotify       destroy)
{
        AsyncUserOpData *data;

        data = g_new0 (AsyncUserOpData, 1);
        data->manager = g_object_ref (manager);
        data->user_name = g_strdup (user_name);
        data->callback = done;
        data->data = done_data;
        data->destroy = destroy;

        dbus_g_proxy_begin_call (manager->proxy,
                                 "CreateUser",
                                 create_user_done,
                                 data,
                                 async_user_op_data_free,
                                 G_TYPE_STRING, user_name,
                                 G_TYPE_STRING, real_name,
                                 G_TYPE_INT, account_type,
                                 G_TYPE_INVALID);
}

static void
delete_user_done (DBusGProxy     *proxy,
                  DBusGProxyCall *call_id,
                  gpointer        user_data)
{
        AsyncUserOpData *data = user_data;
        GError *error;
        GSimpleAsyncResult *res;

        res = g_simple_async_result_new (G_OBJECT (data->manager),
                                         data->callback,
                                         data->data,
                                         um_user_manager_delete_user);
        error = NULL;
        if (!dbus_g_proxy_end_call (proxy,
                                    call_id,
                                    &error,
                                    G_TYPE_INVALID)) {
                if (dbus_g_error_has_name (error, "org.freedesktop.Accounts.Error.PermissionDenied")) {
                        g_simple_async_result_set_error (res,
                                                         UM_USER_MANAGER_ERROR,
                                                         UM_USER_MANAGER_ERROR_PERMISSION_DENIED,
                                                         "Not authorized");
                }
                else if (dbus_g_error_has_name (error, "org.freedesktop.Accounts.Error.UserDoesntExists")) {
                        g_simple_async_result_set_error (res,
                                                         UM_USER_MANAGER_ERROR,
                                                         UM_USER_MANAGER_ERROR_USER_DOES_NOT_EXIST,
                                                         _("This user does not exist."));
                }
                else {
                        g_simple_async_result_set_from_error (res, error);
                        g_error_free (error);
                }
        }

        data->callback (G_OBJECT (data->manager), G_ASYNC_RESULT (res), data->data);
}

gboolean
um_user_manager_delete_user_finish (UmUserManager  *manager,
                                    GAsyncResult   *result,
                                    GError        **error)
{
        GSimpleAsyncResult *res;

        res = G_SIMPLE_ASYNC_RESULT (result);

        if (g_simple_async_result_propagate_error (res, error)) {
                return FALSE;
        }

        return TRUE;
}

void
um_user_manager_delete_user (UmUserManager       *manager,
                             UmUser              *user,
                             gboolean             remove_files,
                             GAsyncReadyCallback  done,
                             gpointer             done_data,
                             GDestroyNotify       destroy)
{
        AsyncUserOpData *data;

        data = g_new0 (AsyncUserOpData, 1);
        data->manager = g_object_ref (manager);
        data->callback = done;
        data->data = done_data;
        data->destroy = destroy;

        dbus_g_proxy_begin_call (manager->proxy,
                                 "DeleteUser",
                                 delete_user_done,
                                 data,
                                 async_user_op_data_free,
                                 G_TYPE_INT64, um_user_get_uid (user),
                                 G_TYPE_BOOLEAN, remove_files,
                                 G_TYPE_INVALID);
}

GSList *
um_user_manager_list_users (UmUserManager *manager)
{
        GSList *list = NULL;
        GHashTableIter iter;
        gpointer value;

        g_hash_table_iter_init (&iter, manager->user_by_name);
        while (g_hash_table_iter_next (&iter, NULL, &value)) {
                list = g_slist_prepend (list, value);
        }

        return g_slist_sort (list, (GCompareFunc) um_user_collate);
}

UmUser *
um_user_manager_get_user (UmUserManager *manager,
                          const gchar   *name)
{
        return g_hash_table_lookup (manager->user_by_name, name);
}

UmUser *
um_user_manager_get_user_by_id (UmUserManager *manager,
                                uid_t          uid)
{
        struct passwd *pwent;

        pwent = getpwuid (uid);
        if (!pwent) {
                return NULL;
        }

        return um_user_manager_get_user (manager, pwent->pw_name);
}

gboolean
um_user_manager_no_service (UmUserManager *manager)
{
        return manager->no_service;
}

GQuark
um_user_manager_error_quark (void)
{
        return g_quark_from_static_string ("um-user-manager-error-quark");
}
