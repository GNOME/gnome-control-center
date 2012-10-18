/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright 2009-2012  Red Hat, Inc.
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
 * Written by: Stef Walter <stefw@gnome.org>
 */

#include "config.h"

#include "um-realm-manager.h"

#include <krb5/krb5.h>

#include <glib.h>
#include <glib/gi18n.h>
#include <glib/gstdio.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>


struct _UmRealmManager {
        UmRealmObjectManagerClient parent;
        UmRealmProvider *provider;
        guint diagnostics_sig;
};

typedef struct {
        UmRealmProviderProxyClass parent_class;
} UmRealmManagerClass;

enum {
        REALM_ADDED,
        NUM_SIGNALS,
};

static gint signals[NUM_SIGNALS] = { 0, };

G_DEFINE_TYPE (UmRealmManager, um_realm_manager, UM_REALM_TYPE_OBJECT_MANAGER_CLIENT);

GQuark
um_realm_error_get_quark (void)
{
        static GQuark quark = 0;
        if (quark == 0)
                quark = g_quark_from_static_string ("um-realm-error");
        return quark;
}

static gboolean
is_realm_with_kerberos_and_membership (gpointer object)
{
        GDBusInterface *interface;

        if (!G_IS_DBUS_OBJECT (object))
                return FALSE;

        interface = g_dbus_object_get_interface (object, "org.freedesktop.realmd.Kerberos");
        if (interface == NULL)
                return FALSE;
        g_object_unref (interface);

        interface = g_dbus_object_get_interface (object, "org.freedesktop.realmd.KerberosMembership");
        if (interface == NULL)
                return FALSE;
        g_object_unref (interface);

        return TRUE;
}

static void
on_interface_added (GDBusObjectManager *manager,
                    GDBusObject *object,
                    GDBusInterface *interface)
{
        g_dbus_proxy_set_default_timeout (G_DBUS_PROXY (interface), G_MAXINT);
}

static void
on_object_added (GDBusObjectManager *manager,
                 GDBusObject *object,
                 gpointer user_data)
{
        GList *interfaces, *l;

        interfaces = g_dbus_object_get_interfaces (object);
        for (l = interfaces; l != NULL; l = g_list_next (l))
                on_interface_added (manager, object, l->data);
        g_list_free_full (interfaces, g_object_unref);

        if (is_realm_with_kerberos_and_membership (object)) {
                g_debug ("Saw realm: %s", g_dbus_object_get_object_path (object));
                g_signal_emit (user_data, signals[REALM_ADDED], 0, object);
        }
}

static void
um_realm_manager_init (UmRealmManager *self)
{
        g_signal_connect (self, "object-added", G_CALLBACK (on_object_added), self);
        g_signal_connect (self, "interface-added", G_CALLBACK (on_interface_added), self);
}

static void
um_realm_manager_dispose (GObject *obj)
{
        UmRealmManager *self = UM_REALM_MANAGER (obj);
        GDBusConnection *connection;

        g_clear_object (&self->provider);

        if (self->diagnostics_sig) {
                connection = g_dbus_object_manager_client_get_connection (G_DBUS_OBJECT_MANAGER_CLIENT (self));
                if (connection != NULL)
                        g_dbus_connection_signal_unsubscribe (connection, self->diagnostics_sig);
                self->diagnostics_sig = 0;
        }

        G_OBJECT_CLASS (um_realm_manager_parent_class)->dispose (obj);
}

static void
um_realm_manager_class_init (UmRealmManagerClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);

        object_class->dispose = um_realm_manager_dispose;

        signals[REALM_ADDED] = g_signal_new ("realm-added", UM_TYPE_REALM_MANAGER,
                                             G_SIGNAL_RUN_FIRST, 0, NULL, NULL,
                                             g_cclosure_marshal_generic,
                                             G_TYPE_NONE, 1, UM_REALM_TYPE_OBJECT);
}

static void
on_realm_diagnostics (GDBusConnection *connection,
                      const gchar *sender_name,
                      const gchar *object_path,
                      const gchar *interface_name,
                      const gchar *signal_name,
                      GVariant *parameters,
                      gpointer user_data)
{
        const gchar *message;
        const gchar *unused;

        if (g_variant_is_of_type (parameters, G_VARIANT_TYPE ("(ss)"))) {
                /* Data is already formatted appropriately for stderr */
                g_variant_get (parameters, "(&s&s)", &message, &unused);
                g_printerr ("%s", message);
        }
}

typedef struct {
        GCancellable *cancellable;
        UmRealmManager *manager;
} NewClosure;

static void
new_closure_free (gpointer data)
{
        NewClosure *closure = data;
        g_clear_object (&closure->cancellable);
        g_clear_object (&closure->manager);
        g_slice_free (NewClosure, closure);
}

static void
on_provider_new (GObject *source,
                 GAsyncResult *result,
                 gpointer user_data)
{
        GSimpleAsyncResult *async = G_SIMPLE_ASYNC_RESULT (user_data);
        NewClosure *closure = g_simple_async_result_get_op_res_gpointer (async);
        GError *error = NULL;
        UmRealmProvider *provider;

        provider = um_realm_provider_proxy_new_finish (result, &error);
        closure->manager->provider = provider;

        if (error == NULL) {
                g_dbus_proxy_set_default_timeout (G_DBUS_PROXY (closure->manager->provider), -1);
                g_debug ("Created realm manager");
        } else {
                g_simple_async_result_take_error (async, error);
        }
        g_simple_async_result_complete (async);

        g_object_unref (async);
}

static void
on_manager_new (GObject *source,
                GAsyncResult *result,
                gpointer user_data)
{
        GSimpleAsyncResult *async = G_SIMPLE_ASYNC_RESULT (user_data);
        NewClosure *closure = g_simple_async_result_get_op_res_gpointer (async);
        GDBusConnection *connection;
        GError *error = NULL;
        GObject *object;
        guint sig;

        object = g_async_initable_new_finish (G_ASYNC_INITABLE (source), result, &error);
        if (error == NULL) {
                closure->manager = UM_REALM_MANAGER (object);
                connection = g_dbus_object_manager_client_get_connection (G_DBUS_OBJECT_MANAGER_CLIENT (object));

                g_debug ("Connected to realmd");

                sig = g_dbus_connection_signal_subscribe (connection,
                                                          "org.freedesktop.realmd",
                                                          "org.freedesktop.realmd.Service",
                                                          "Diagnostics",
                                                          NULL,
                                                          NULL,
                                                          G_DBUS_SIGNAL_FLAGS_NONE,
                                                          on_realm_diagnostics,
                                                          NULL,
                                                          NULL);
                closure->manager->diagnostics_sig = sig;

                um_realm_provider_proxy_new (connection,
                                             G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES,
                                             "org.freedesktop.realmd",
                                             "/org/freedesktop/realmd",
                                             closure->cancellable,
                                             on_provider_new, g_object_ref (async));
        } else {
                g_simple_async_result_take_error (async, error);
                g_simple_async_result_complete (async);
        }

        g_object_unref (async);
}

void
um_realm_manager_new (GCancellable *cancellable,
                      GAsyncReadyCallback callback,
                      gpointer user_data)
{
        GSimpleAsyncResult *async;
        NewClosure *closure;

        g_debug ("Connecting to realmd...");

        async = g_simple_async_result_new (NULL, callback, user_data,
                                           um_realm_manager_new);
        closure = g_slice_new (NewClosure);
        closure->cancellable = cancellable ? g_object_ref (cancellable) : NULL;
        g_simple_async_result_set_op_res_gpointer (async, closure, new_closure_free);

        g_async_initable_new_async (UM_TYPE_REALM_MANAGER, G_PRIORITY_DEFAULT,
                                    cancellable, on_manager_new, g_object_ref (async),
                                    "flags", G_DBUS_OBJECT_MANAGER_CLIENT_FLAGS_NONE,
                                    "name", "org.freedesktop.realmd",
                                    "bus-type", G_BUS_TYPE_SYSTEM,
                                    "object-path", "/org/freedesktop/realmd",
                                    "get-proxy-type-func", um_realm_object_manager_client_get_proxy_type,
                                    NULL);

        g_object_unref (async);
}

UmRealmManager *
um_realm_manager_new_finish (GAsyncResult *result,
                             GError **error)
{
        GSimpleAsyncResult *async;
        NewClosure *closure;

        g_return_val_if_fail (g_simple_async_result_is_valid (result, NULL,
                                                              um_realm_manager_new), NULL);

        async = G_SIMPLE_ASYNC_RESULT (result);
        if (g_simple_async_result_propagate_error (async, error))
                return NULL;

        closure = g_simple_async_result_get_op_res_gpointer (async);
        return g_object_ref (closure->manager);
}

typedef struct {
        GDBusObjectManager *manager;
        GCancellable *cancellable;
        GList *realms;
} DiscoverClosure;

static void
discover_closure_free (gpointer data)
{
        DiscoverClosure *discover = data;
        g_object_unref (discover->manager);
        g_clear_object (&discover->cancellable);
        g_list_free_full (discover->realms, g_object_unref);
        g_slice_free (DiscoverClosure, discover);
}

static void
on_provider_discover (GObject *source,
                      GAsyncResult *result,
                      gpointer user_data)
{
        GSimpleAsyncResult *async = G_SIMPLE_ASYNC_RESULT (user_data);
        DiscoverClosure *discover = g_simple_async_result_get_op_res_gpointer (async);
        GDBusObject *object;
        GError *error = NULL;
        gchar **realms;
        gint relevance;
        gint i;

        um_realm_provider_call_discover_finish (UM_REALM_PROVIDER (source), &relevance,
                                                &realms, result, &error);
        if (error == NULL) {
                for (i = 0; realms[i]; i++) {
                        object = g_dbus_object_manager_get_object (discover->manager, realms[i]);
                        if (object == NULL) {
                                g_warning ("Realm is not in object manager: %s", realms[i]);
                        } else {
                                if (is_realm_with_kerberos_and_membership (object)) {
                                        g_debug ("Discovered realm: %s", realms[i]);
                                        discover->realms = g_list_prepend (discover->realms, object);
                                } else {
                                        g_debug ("Realm does not support kerberos membership: %s", realms[i]);
                                        g_object_unref (object);
                                }
                        }
                }
                g_strfreev (realms);

        } else {
                g_simple_async_result_take_error (async, error);
        }

        g_simple_async_result_complete (async);
        g_object_unref (async);
}

void
um_realm_manager_discover (UmRealmManager *self,
                           const gchar *input,
                           GCancellable *cancellable,
                           GAsyncReadyCallback callback,
                           gpointer user_data)
{
        GSimpleAsyncResult *res;
        DiscoverClosure *discover;
        GVariant *options;

        g_return_if_fail (UM_IS_REALM_MANAGER (self));
        g_return_if_fail (input != NULL);
        g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

        g_debug ("Discovering realms for: %s", input);

        res = g_simple_async_result_new (G_OBJECT (self), callback, user_data,
                                         um_realm_manager_discover);
        discover = g_slice_new0 (DiscoverClosure);
        discover->manager = g_object_ref (self);
        discover->cancellable = cancellable ? g_object_ref (cancellable) : NULL;
        g_simple_async_result_set_op_res_gpointer (res, discover, discover_closure_free);

	options = g_variant_new_array (G_VARIANT_TYPE ("{sv}"), NULL, 0);

        um_realm_provider_call_discover (self->provider, input, options, cancellable,
                                         on_provider_discover, g_object_ref (res));

        g_object_unref (res);
}

GList *
um_realm_manager_discover_finish (UmRealmManager *self,
                                  GAsyncResult *result,
                                  GError **error)
{
        GSimpleAsyncResult *async;
        DiscoverClosure *discover;
        GList *realms;

        g_return_val_if_fail (UM_IS_REALM_MANAGER (self), NULL);
        g_return_val_if_fail (g_simple_async_result_is_valid (result, G_OBJECT (self),
                              um_realm_manager_discover), NULL);
        g_return_val_if_fail (error == NULL || *error == NULL, NULL);

        async = G_SIMPLE_ASYNC_RESULT (result);
        if (g_simple_async_result_propagate_error (async, error))
                return NULL;

        discover = g_simple_async_result_get_op_res_gpointer (async);
        if (!discover->realms) {
                g_set_error (error, UM_REALM_ERROR, UM_REALM_ERROR_GENERIC,
                             _("No such domain or realm found"));
                return NULL;
        }

        realms = g_list_reverse (discover->realms);
        discover->realms = NULL;
        return realms;
}

GList *
um_realm_manager_get_realms (UmRealmManager *self)
{
        GList *objects;
        GList *realms = NULL;
        GList *l;

        g_return_val_if_fail (UM_IS_REALM_MANAGER (self), NULL);

        objects = g_dbus_object_manager_get_objects (G_DBUS_OBJECT_MANAGER (self));
        for (l = objects; l != NULL; l = g_list_next (l)) {
                if (is_realm_with_kerberos_and_membership (l->data))
                        realms = g_list_prepend (realms, g_object_ref (l->data));
        }

        g_list_free_full (objects, g_object_unref);
        return realms;
}

static void
string_replace (GString *string,
                const gchar *find,
                const gchar *replace)
{
        const gchar *at;
        gssize pos;

        at = strstr (string->str, find);
        if (at != NULL) {
                pos = at - string->str;
                g_string_erase (string, pos, strlen (find));
                g_string_insert (string, pos, replace);
        }
}

gchar *
um_realm_calculate_login (UmRealmCommon *realm,
                          const gchar *username)
{
        GString *string;
        const gchar *const *formats;
        gchar *login = NULL;

        formats = um_realm_common_get_login_formats (realm);
        if (formats[0] != NULL) {
                string = g_string_new (formats[0]);
                string_replace (string, "%U", username);
                string_replace (string, "%D", um_realm_common_get_name (realm));
                login = g_string_free (string, FALSE);
        }

        return login;

}

gboolean
um_realm_is_configured (UmRealmObject *realm)
{
        UmRealmCommon *common;
        const gchar *configured;
        gboolean is;

        common = um_realm_object_get_common (realm);
        configured = um_realm_common_get_configured (common);
        is = configured != NULL && !g_str_equal (configured, "");
        g_object_unref (common);

        return is;
}

static const gchar *
find_supported_credentials (UmRealmKerberosMembership *membership,
                            const gchar *owner)
{
        const gchar *cred_owner;
        const gchar *cred_type;
        GVariant *supported;
        GVariantIter iter;

        supported = um_realm_kerberos_membership_get_supported_join_credentials (membership);
        g_return_val_if_fail (supported != NULL, NULL);

        g_variant_iter_init (&iter, supported);
        while (g_variant_iter_loop (&iter, "(&s&s)", &cred_type, &cred_owner)) {
                if (g_str_equal (owner, cred_owner)) {
                        if (g_str_equal (cred_type, "ccache") ||
                            g_str_equal (cred_type, "password")) {
                                return g_intern_string (cred_type);
                        }
                }
        }

        return NULL;
}

static void
on_realm_join_complete (GObject *source,
                        GAsyncResult *result,
                        gpointer user_data)
{
	GSimpleAsyncResult *async = G_SIMPLE_ASYNC_RESULT (user_data);

	g_debug ("Completed Join() method call");

	g_simple_async_result_set_op_res_gpointer (async, g_object_ref (result), g_object_unref);
	g_simple_async_result_complete_in_idle (async);
	g_object_unref (async);
}

static gboolean
realm_join_as_owner (UmRealmObject *realm,
                     const gchar *owner,
                     const gchar *login,
                     const gchar *password,
                     GBytes *credentials,
                     GCancellable *cancellable,
                     GAsyncReadyCallback callback,
                     gpointer user_data)
{
        UmRealmKerberosMembership *membership;
        GSimpleAsyncResult *async;
        GVariant *contents;
        GVariant *options;
        GVariant *creds;
        const gchar *type;

        membership = um_realm_object_get_kerberos_membership (realm);
        g_return_val_if_fail (membership != NULL, FALSE);

        type = find_supported_credentials (membership, owner);
        if (type == NULL) {
                g_debug ("Couldn't find supported credential type for owner: %s", owner);
                g_object_unref (membership);
                return FALSE;
        }

        async = g_simple_async_result_new (G_OBJECT (realm), callback, user_data,
                                           realm_join_as_owner);

        if (g_str_equal (type, "ccache")) {
                g_debug ("Using a kerberos credential cache to join the realm");
                contents = g_variant_new_from_data (G_VARIANT_TYPE ("ay"),
                                                    g_bytes_get_data (credentials, NULL),
                                                    g_bytes_get_size (credentials),
                                                    TRUE, (GDestroyNotify)g_bytes_unref, credentials);

        } else if (g_str_equal (type, "password")) {
                g_debug ("Using a user/password to join the realm");
                contents = g_variant_new ("(ss)", login, password);

        } else {
                g_assert_not_reached ();
        }

        creds = g_variant_new ("(ssv)", type, owner, contents);
        options = g_variant_new_array (G_VARIANT_TYPE ("{sv}"), NULL, 0);

        g_debug ("Calling the Join() method with %s credentials", owner);

        um_realm_kerberos_membership_call_join (membership, creds, options,
                                                cancellable, on_realm_join_complete,
                                                g_object_ref (async));

        g_object_unref (async);
        g_object_unref (membership);

        return TRUE;
}

gboolean
um_realm_join_as_user (UmRealmObject *realm,
                       const gchar *login,
                       const gchar *password,
                       GBytes *credentials,
                       GCancellable *cancellable,
                       GAsyncReadyCallback callback,
                       gpointer user_data)
{
        g_return_val_if_fail (UM_REALM_IS_OBJECT (realm), FALSE);
        g_return_val_if_fail (credentials != NULL, FALSE);
        g_return_val_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable), FALSE);
        g_return_val_if_fail (login != NULL, FALSE);
        g_return_val_if_fail (password != NULL, FALSE);
        g_return_val_if_fail (credentials != NULL, FALSE);

        return realm_join_as_owner (realm, "user", login, password,
                                    credentials, cancellable, callback, user_data);
}

gboolean
um_realm_join_as_admin (UmRealmObject *realm,
                        const gchar *login,
                        const gchar *password,
                        GBytes *credentials,
                        GCancellable *cancellable,
                        GAsyncReadyCallback callback,
                        gpointer user_data)
{
        g_return_val_if_fail (UM_REALM_IS_OBJECT (realm), FALSE);
        g_return_val_if_fail (credentials != NULL, FALSE);
        g_return_val_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable), FALSE);
        g_return_val_if_fail (login != NULL, FALSE);
        g_return_val_if_fail (password != NULL, FALSE);
        g_return_val_if_fail (credentials != NULL, FALSE);

        return realm_join_as_owner (realm, "administrator", login, password, credentials,
                                    cancellable, callback, user_data);
}

gboolean
um_realm_join_finish (UmRealmObject *realm,
                      GAsyncResult *result,
                      GError **error)
{
        UmRealmKerberosMembership *membership;
        GError *call_error = NULL;
        gchar *dbus_error;
        GAsyncResult *async;

        g_return_val_if_fail (UM_REALM_IS_OBJECT (realm), FALSE);
        g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

        membership = um_realm_object_get_kerberos_membership (realm);
        g_return_val_if_fail (membership != NULL, FALSE);

        async = g_simple_async_result_get_op_res_gpointer (G_SIMPLE_ASYNC_RESULT (result));
        um_realm_kerberos_membership_call_join_finish (membership, async, &call_error);
        g_object_unref (membership);

        if (call_error == NULL)
                return TRUE;

        dbus_error = g_dbus_error_get_remote_error (call_error);
        if (dbus_error == NULL) {
                g_debug ("Join() failed because of %s", call_error->message);
                g_propagate_error (error, call_error);
                return FALSE;
        }

        g_dbus_error_strip_remote_error (call_error);

        if (g_str_equal (dbus_error, "org.freedesktop.realmd.Error.AuthenticationFailed")) {
                g_debug ("Join() failed because of invalid/insufficient credentials");
                g_set_error (error, UM_REALM_ERROR, UM_REALM_ERROR_BAD_LOGIN,
                             "%s", call_error->message);
                g_error_free (call_error);
        } else {
                g_debug ("Join() failed because of %s", call_error->message);
                g_propagate_error (error, call_error);
        }

        g_free (dbus_error);
        return FALSE;
}

typedef struct {
        gchar *domain;
        gchar *realm;
        gchar *user;
        gchar *password;
        GBytes *credentials;
} LoginClosure;

static void
login_closure_free (gpointer data)
{
        LoginClosure *login = data;
        g_free (login->domain);
        g_free (login->realm);
        g_free (login->user);
        g_free (login->password);
        g_bytes_unref (login->credentials);
        g_slice_free (LoginClosure, login);
}

static krb5_error_code
login_perform_kinit (krb5_context k5,
                     const gchar *realm,
                     const gchar *login,
                     const gchar *password,
                     const gchar *filename)
{
        krb5_get_init_creds_opt *opts;
        krb5_error_code code;
        krb5_principal principal;
        krb5_ccache ccache;
        krb5_creds creds;
        gchar *name;

        name = g_strdup_printf ("%s@%s", login, realm);
        code = krb5_parse_name (k5, name, &principal);

        if (code != 0) {
                g_debug ("Couldn't parse principal name: %s: %s",
                         name, krb5_get_error_message (k5, code));
                g_free (name);
                return code;
        }

        g_debug ("Using principal name to kinit: %s", name);
        g_free (name);

        if (filename == NULL)
                code = krb5_cc_default (k5, &ccache);
        else
                code = krb5_cc_resolve (k5, filename, &ccache);

        if (code != 0) {
                krb5_free_principal (k5, principal);
                g_debug ("Couldn't open credential cache: %s: %s",
                         filename ? filename : "<default>",
                         krb5_get_error_message (k5, code));
                return code;
        }

        code = krb5_get_init_creds_opt_alloc (k5, &opts);
        g_return_val_if_fail (code == 0, code);

        code = krb5_get_init_creds_opt_set_out_ccache (k5, opts, ccache);
        g_return_val_if_fail (code == 0, code);

        code = krb5_get_init_creds_password (k5, &creds, principal,
                                             (char *)password,
                                             NULL, 0, 0, NULL, opts);

        krb5_get_init_creds_opt_free (k5, opts);
        krb5_cc_close (k5, ccache);
        krb5_free_principal (k5, principal);

        if (code == 0) {
                g_debug ("kinit succeeded");
                krb5_free_cred_contents (k5, &creds);
        } else {
                g_debug ("kinit failed: %s", krb5_get_error_message (k5, code));
        }

        return code;
}

static void
kinit_thread_func (GSimpleAsyncResult *async,
                   GObject *object,
                   GCancellable *cancellable)
{
        LoginClosure *login = g_simple_async_result_get_op_res_gpointer (async);
        krb5_context k5 = NULL;
        krb5_error_code code;
        GError *error = NULL;
        gchar *filename = NULL;
        gchar *contents;
        gsize length;
        gint temp_fd;

        filename = g_build_filename (g_get_user_runtime_dir (),
                                     "um-krb5-creds.XXXXXX", NULL);
        temp_fd = g_mkstemp_full (filename, O_RDWR, S_IRUSR | S_IWUSR);
        if (temp_fd == -1) {
                g_warning ("Couldn't create credential cache file: %s: %s",
                           filename, g_strerror (errno));
                g_free (filename);
                filename = NULL;
        } else {
                close (temp_fd);
        }

        code = krb5_init_context (&k5);
        if (code == 0) {
                code = login_perform_kinit (k5, login->realm, login->user,
                                            login->password, filename);
        }

        switch (code) {
        case 0:
                if (filename != NULL) {
                        g_file_get_contents (filename, &contents, &length, &error);
                        if (error == NULL) {
                                login->credentials = g_bytes_new_take (contents, length);
                                g_debug ("Read in credential cache: %s", filename);
                        } else {
                                g_warning ("Couldn't read credential cache: %s: %s",
                                           filename, error->message);
                                g_error_free (error);
                        }
                }
                break;

        case KRB5KDC_ERR_C_PRINCIPAL_UNKNOWN:
        case KRB5KDC_ERR_CLIENT_REVOKED:
        case KRB5KDC_ERR_KEY_EXP:
        case KRB5KDC_ERR_POLICY:
        case KRB5KDC_ERR_ETYPE_NOSUPP:
                g_simple_async_result_set_error (async, UM_REALM_ERROR, UM_REALM_ERROR_BAD_LOGIN,
                                                 _("Cannot log in as %s at the %s domain"),
                                                 login->user, login->domain);
                break;
        case KRB5KDC_ERR_PREAUTH_FAILED:
                g_simple_async_result_set_error (async, UM_REALM_ERROR, UM_REALM_ERROR_BAD_PASSWORD,
                                                 _("Invalid password, please try again"));
                break;
        default:
                g_simple_async_result_set_error (async, UM_REALM_ERROR, UM_REALM_ERROR_GENERIC,
                                                 _("Couldn't connect to the %s domain: %s"),
                                                 login->domain, krb5_get_error_message (k5, code));
                break;
        }

        if (filename) {
                g_unlink (filename);
                g_debug ("Deleted credential cache: %s", filename);
                g_free (filename);
        }

        if (k5)
                krb5_free_context (k5);
}

void
um_realm_login (UmRealmObject *realm,
                const gchar *user,
                const gchar *password,
                GCancellable *cancellable,
                GAsyncReadyCallback callback,
                gpointer user_data)
{
        GSimpleAsyncResult *async;
        LoginClosure *login;
        UmRealmKerberos *kerberos;

        g_return_if_fail (UM_REALM_IS_OBJECT (realm));
        g_return_if_fail (user != NULL);
        g_return_if_fail (password != NULL);
        g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

        kerberos = um_realm_object_get_kerberos (realm);
        g_return_if_fail (kerberos != NULL);

        async = g_simple_async_result_new (NULL, callback, user_data,
                                           um_realm_login);
        login = g_slice_new0 (LoginClosure);
        login->domain = g_strdup (um_realm_kerberos_get_domain_name (kerberos));
        login->realm = g_strdup (um_realm_kerberos_get_realm_name (kerberos));
        login->user = g_strdup (user);
        login->password = g_strdup (password);
        g_simple_async_result_set_op_res_gpointer (async, login, login_closure_free);

        g_simple_async_result_set_handle_cancellation (async, TRUE);
        g_simple_async_result_run_in_thread (async, kinit_thread_func,
                                             G_PRIORITY_DEFAULT, cancellable);

        g_object_unref (async);
        g_object_unref (kerberos);
}

gboolean
um_realm_login_finish (GAsyncResult *result,
                       GBytes **credentials,
                       GError **error)
{
        GSimpleAsyncResult *async;
        LoginClosure *login;

        g_return_val_if_fail (g_simple_async_result_is_valid (result, NULL,
                              um_realm_login), FALSE);
        g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

        async = G_SIMPLE_ASYNC_RESULT (result);
        if (g_simple_async_result_propagate_error (async, error))
                return FALSE;

        login = g_simple_async_result_get_op_res_gpointer (async);
        if (credentials) {
                if (login->credentials)
                        *credentials = g_bytes_ref (login->credentials);
                else
                        *credentials = NULL;
        }

        return TRUE;
}
