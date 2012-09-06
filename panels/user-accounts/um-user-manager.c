/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2009-2010 Red Hat, Inc.
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

        dup = NULL;
        dupes = g_object_get_data (G_OBJECT (user), "dupes");

        if (!dupes) {
                goto out;
        }

        if (dupes->next == dupes->prev) {
                dup = dupes->next->data;
                g_list_free_1 (dupes->next);
                g_object_set_data (G_OBJECT (dup), "dupes", NULL);
        }
        else {
                dupes->next->prev = dupes->prev;
                dupes->prev->next = dupes->next;
        }

        g_list_free_1 (dupes);
        g_object_set_data (G_OBJECT (user), "dupes", NULL);

out:
        if (dup) {
                um_user_show_short_display_name (dup);
                g_signal_emit (manager, signals[USER_CHANGED], 0, dup);
        }
        um_user_show_short_display_name (user);
        g_signal_emit (manager, signals[USER_CHANGED], 0, user);
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

        dupes = g_object_get_data (G_OBJECT (dup), "dupes");
        if (!dupes) {
                dupes = g_list_append (NULL, dup);
                g_object_set_data (G_OBJECT (dup), "dupes", dupes);
                dupes->next = dupes->prev = dupes;
        }
        else {
                dup = NULL;
        }

        l = g_list_append (NULL, user);
        g_object_set_data (G_OBJECT (user), "dupes", l);
        l->prev = dupes->prev;
        dupes->prev->next = l;
        l->next = dupes;
        dupes->prev = l;

        if (dup) {
                um_user_show_full_display_name (dup);
                g_signal_emit (manager, signals[USER_CHANGED], 0, dup);
        }
        um_user_show_full_display_name (user);
        g_signal_emit (manager, signals[USER_CHANGED], 0, user);
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
user_added_handler (UmUserManager *manager,
                    const char *object_path)
{
        UmUser *user;
 
        if (g_hash_table_lookup (manager->user_by_object_path, object_path))
                return;

        user = um_user_new_from_object_path (object_path);
        if (!user)
                return;

        if (um_user_is_system_account (user)) {
                g_object_unref (user);
                return;
        }

        add_user_to_dupe_ring (manager, user);

        g_signal_connect (user, "changed",
                          G_CALLBACK (user_changed_handler), manager);
        g_hash_table_insert (manager->user_by_object_path, g_strdup (um_user_get_object_path (user)), g_object_ref (user));
        g_hash_table_insert (manager->user_by_name, g_strdup (um_user_get_user_name (user)), g_object_ref (user));

        g_signal_emit (manager, signals[USER_ADDED], 0, user);
        g_object_unref (user);
}

static void
user_deleted_handler (UmUserManager *manager,
                      const char *object_path)
{
        UmUser *user;

        user = g_hash_table_lookup (manager->user_by_object_path, object_path);
        if (!user)
                return;
        g_object_ref (user);
        g_signal_handlers_disconnect_by_func (user, user_changed_handler, manager);

        remove_user_from_dupe_ring (manager, user);

        g_hash_table_remove (manager->user_by_object_path, um_user_get_object_path (user));
        g_hash_table_remove (manager->user_by_name, um_user_get_user_name (user));
        g_signal_emit (manager, signals[USER_REMOVED], 0, user);
        g_object_unref (user);
}

static void
manager_signal_cb (GDBusProxy *proxy, gchar *sender_name, gchar *signal_name, GVariant *parameters, UmUserManager *manager)
{
        if (strcmp (signal_name, "UserAdded") == 0) {
                if (g_variant_is_of_type (parameters, G_VARIANT_TYPE ("(o)"))) {
                        gchar *object_path;
                        g_variant_get (parameters, "(&o)", &object_path);
                        user_added_handler (manager, object_path);
                }
        }
        else if (strcmp (signal_name, "UserDeleted") == 0) {
                if (g_variant_is_of_type (parameters, G_VARIANT_TYPE ("(o)"))) {
                        gchar *object_path;
                        g_variant_get (parameters, "(&o)", &object_path);
                        user_deleted_handler (manager, object_path);
                }
        }
}

static void
got_users (GObject        *object,
           GAsyncResult   *res,
           gpointer        data)
{
        UmUserManager *manager = data;
        GVariant *result;
        GError *error = NULL;

        result = g_dbus_proxy_call_finish (G_DBUS_PROXY (object), res, &error);
        if (!result) {
                manager->no_service = TRUE;
                g_error_free (error);
                goto done;
        }

        if (g_variant_is_of_type (result, G_VARIANT_TYPE ("(ao)"))) {
                GVariantIter *iter;
                gchar *object_path;

                g_variant_get (result, "(ao)", &iter);
                while (g_variant_iter_loop (iter, "&o", &object_path))
                        user_added_handler (manager, object_path);
                g_variant_iter_free (iter);
        }

        g_variant_unref (result);

 done:
        g_signal_emit (G_OBJECT (manager), signals[USERS_LOADED], 0);
}

static void
get_users (UmUserManager *manager)
{
        g_debug ("calling 'ListCachedUsers'");
        g_dbus_proxy_call (manager->proxy,
                           "ListCachedUsers",
                           g_variant_new ("()"),
                           G_DBUS_CALL_FLAGS_NONE,
                           -1,
                           NULL,
                           got_users,
                           manager);
}

static void
um_user_manager_init (UmUserManager *manager)
{
        GError *error = NULL;
        GDBusConnection *bus;

        manager->user_by_object_path = g_hash_table_new_full (g_str_hash,
                                                              g_str_equal,
                                                              g_free,
                                                              g_object_unref);
        manager->user_by_name = g_hash_table_new_full (g_str_hash,
                                                       g_str_equal,
                                                       g_free,
                                                       g_object_unref);

        bus = g_bus_get_sync (G_BUS_TYPE_SYSTEM, NULL, &error);
        if (bus == NULL) {
                g_warning ("Couldn't connect to system bus: %s", error->message);
                g_error_free (error);
                return;
        }

        manager->proxy = g_dbus_proxy_new_sync (bus,
                                                G_DBUS_PROXY_FLAGS_NONE,
                                                NULL,
                                                "org.freedesktop.Accounts",
                                                "/org/freedesktop/Accounts",
                                                "org.freedesktop.Accounts",
                                                NULL,
                                                &error);
        if (manager->proxy == NULL) {
                g_warning ("Couldn't get accounts proxy: %s", error->message);
                g_error_free (error);
                return;     
        }

        g_signal_connect (manager->proxy, "g-signal", G_CALLBACK (manager_signal_cb), manager);

        get_users (manager);
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

        g_object_unref (manager->proxy);

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

/* Used for both create_user and cache_user */
static void
user_call_done (GObject        *proxy,
                GAsyncResult   *r,
                gpointer        user_data)
{
        AsyncUserOpData *data = user_data;
        GSimpleAsyncResult *res;
        GVariant *result;
        GError *error = NULL;
        gchar *remote;

        res = g_simple_async_result_new (G_OBJECT (data->manager),
                                         data->callback,
                                         data->data,
                                         um_user_manager_create_user);
        result = g_dbus_proxy_call_finish (G_DBUS_PROXY (proxy), r, &error);
        if (!result) {
                /* dbus-glib fail:
                 * We have to translate the errors manually here, since
                 * calling dbus_g_error_has_name on the error returned in
                 * um_user_manager_create_user_finish doesn't work.
                 */
                remote = g_dbus_error_get_remote_error (error);
                if (g_dbus_error_is_remote_error (error) &&
                    strcmp (remote, "org.freedesktop.Accounts.Error.PermissionDenied") == 0) {
                        g_simple_async_result_set_error (res,
                                                         UM_USER_MANAGER_ERROR,
                                                         UM_USER_MANAGER_ERROR_PERMISSION_DENIED,
                                                         "Not authorized");
                }
                if (g_dbus_error_is_remote_error (error) &&
                    strcmp (remote, "org.freedesktop.Accounts.Error.UserExists") == 0) {
                        g_simple_async_result_set_error (res,
                                                         UM_USER_MANAGER_ERROR,
                                                         UM_USER_MANAGER_ERROR_USER_EXISTS,
                                                         _("A user with name '%s' already exists."),
                                                         data->user_name);
                } else if (g_dbus_error_is_remote_error (error) &&
                    strcmp (remote, "org.freedesktop.Accounts.Error.UserDoesNotExist") == 0) {
                        g_simple_async_result_set_error (res,
                                                         UM_USER_MANAGER_ERROR,
                                                         UM_USER_MANAGER_ERROR_USER_DOES_NOT_EXIST,
                                                         _("No user with the name '%s' exists."),
                                                         data->user_name);
                }
                else {
                        g_simple_async_result_set_from_error (res, error);
                }
                g_error_free (error);
                g_free (remote);
        }
        else {
                if (g_variant_is_of_type (result, G_VARIANT_TYPE ("(o)"))) {
                        gchar *path;
                        g_variant_get (result, "(o)", &path);
                        g_simple_async_result_set_op_res_gpointer (res, path, g_free);
                }
                else
                        g_simple_async_result_set_error (res,
                                                         UM_USER_MANAGER_ERROR,
                                                         UM_USER_MANAGER_ERROR_FAILED,
                                                         "Got invalid response from AccountsService");
                g_variant_unref (result);
        }

        data->callback (G_OBJECT (data->manager), G_ASYNC_RESULT (res), data->data);
        async_user_op_data_free (data);
        g_object_unref (res);
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
                             GCancellable        *cancellable,
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

        g_dbus_proxy_call (manager->proxy,
                           "CreateUser",
                           g_variant_new ("(ssi)", user_name, real_name, account_type),
                           G_DBUS_CALL_FLAGS_NONE,
                           -1,
                           cancellable,
                           user_call_done,
                           data);
}

gboolean
um_user_manager_cache_user_finish (UmUserManager       *manager,
                                   GAsyncResult        *result,
                                   UmUser             **user,
                                   GError             **error)
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
um_user_manager_cache_user (UmUserManager       *manager,
                            const char          *user_name,
                            GCancellable        *cancellable,
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

        g_dbus_proxy_call (manager->proxy,
                           "CacheUser",
                           g_variant_new ("(s)", user_name),
                           G_DBUS_CALL_FLAGS_NONE,
                           -1,
                           cancellable,
                           user_call_done,
                           data);
}

static void
delete_user_done (GObject        *proxy,
                  GAsyncResult   *r,
                  gpointer        user_data)
{
        AsyncUserOpData *data = user_data;
        GSimpleAsyncResult *res;
        GVariant *result;
        GError *error = NULL;

        res = g_simple_async_result_new (G_OBJECT (data->manager),
                                         data->callback,
                                         data->data,
                                         um_user_manager_delete_user);
        result = g_dbus_proxy_call_finish (G_DBUS_PROXY (proxy), r, &error);
        if (!result) {
                if (g_dbus_error_is_remote_error (error) &&
                    strcmp (g_dbus_error_get_remote_error(error), "org.freedesktop.Accounts.Error.PermissionDenied") == 0) {
                        g_simple_async_result_set_error (res,
                                                         UM_USER_MANAGER_ERROR,
                                                         UM_USER_MANAGER_ERROR_PERMISSION_DENIED,
                                                         "Not authorized");
                }
                else if (g_dbus_error_is_remote_error (error) &&
                    strcmp (g_dbus_error_get_remote_error(error), "org.freedesktop.Accounts.Error.UserExists") == 0) {
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
        else
                g_variant_unref (result);

        data->callback (G_OBJECT (data->manager), G_ASYNC_RESULT (res), data->data);
        async_user_op_data_free (data);
        g_object_unref (res);
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

        g_dbus_proxy_call (manager->proxy,
                           "DeleteUser",
                           g_variant_new ("(xb)", (gint64) um_user_get_uid (user), remove_files),
                           G_DBUS_CALL_FLAGS_NONE,
                           -1,
                           NULL,
                           delete_user_done,
                           data);
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
