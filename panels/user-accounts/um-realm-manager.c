/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright 2009-2012  Red Hat, Inc.
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
 *             Stef Walter <stefw@gnome.org>
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
        UmRealmProviderProxy parent;
        guint diagnostics_sig;
        GHashTable *realms;
};

typedef struct {
        UmRealmProviderProxyClass parent_class;
} UmRealmManagerClass;

enum {
        REALM_ADDED,
        NUM_SIGNALS,
};

static gint signals[NUM_SIGNALS] = { 0, };

G_DEFINE_TYPE (UmRealmManager, um_realm_manager, UM_REALM_TYPE_PROVIDER_PROXY);

GQuark
um_realm_error_get_quark (void)
{
        static GQuark quark = 0;
        if (quark == 0)
                quark = g_quark_from_static_string ("um-realm-error");
        return quark;
}

typedef struct {
        UmRealmManager *manager;
        GList *realms;
        gint outstanding;
} LoadClosure;

static void
load_closure_free (gpointer data)
{
        LoadClosure *load = data;
        g_list_free_full (load->realms, g_object_unref);
        g_object_unref (load->manager);
        g_slice_free (LoadClosure, load);
}

static void
on_realm_proxy_created (GObject *source,
                        GAsyncResult *result,
                        gpointer user_data)
{
        GSimpleAsyncResult *async = G_SIMPLE_ASYNC_RESULT (user_data);
        LoadClosure *load = g_simple_async_result_get_op_res_gpointer (async);
        UmRealmManager *self = load->manager;
        UmRealmKerberos *realm;
        UmRealmKerberos *have;
        GError *error = NULL;
        GDBusProxy *proxy;
        GVariant *info;

        realm = um_realm_kerberos_proxy_new_finish (result, &error);
        if (error == NULL) {
                proxy = G_DBUS_PROXY (realm);
                info = g_variant_new ("(os)",
                                      g_dbus_proxy_get_object_path (proxy),
                                      g_dbus_proxy_get_interface_name (proxy));

                /* Add it to the manager, unless race */
                have = g_hash_table_lookup (self->realms, info);
                if (have == NULL) {
                        g_dbus_proxy_set_default_timeout (proxy, G_MAXINT);
                        g_hash_table_insert (self->realms,
                                             g_variant_ref_sink (info), realm);
                        g_signal_emit (self, signals[REALM_ADDED], 0, realm);

                } else {
                        g_object_unref (realm);
                        g_variant_unref (info);
                        realm = have;
                }

                load->realms = g_list_prepend (load->realms, g_object_ref (realm));

        } else {
                g_simple_async_result_take_error (async, error);
        }

        if (load->outstanding-- == 1)
                g_simple_async_result_complete (async);

        g_object_unref (async);
}

static void
um_realm_manager_load (UmRealmManager *self,
                       GVariant *realms,
                       GCancellable *cancellable,
                       GAsyncReadyCallback callback,
                       gpointer user_data)
{
        GSimpleAsyncResult *async;
        GDBusConnection *connection;
        LoadClosure *load;
        GVariantIter iter;
        GVariant *info;
        UmRealmKerberos *realm;
        const gchar *path;
        const gchar *iface;

        g_return_if_fail (realms != NULL);

        async = g_simple_async_result_new (G_OBJECT (self), callback, user_data,
                                           um_realm_manager_load);
        load = g_slice_new0 (LoadClosure);
        load->manager = g_object_ref (self);
        g_simple_async_result_set_op_res_gpointer (async, load, load_closure_free);

        connection = g_dbus_proxy_get_connection (G_DBUS_PROXY (self));

        g_variant_iter_init (&iter, realms);
        while (1) {
                info = g_variant_iter_next_value (&iter);
                if (info == NULL)
                        break;
                realm = g_hash_table_lookup (self->realms, info);
                if (realm == NULL) {
                        g_variant_get (info, "(&o&s)", &path, &iface);
                        if (g_str_equal (iface, "org.freedesktop.realmd.Kerberos")) {
                                um_realm_kerberos_proxy_new (connection,
                                                             G_DBUS_PROXY_FLAGS_NONE,
                                                             "org.freedesktop.realmd",
                                                             path, cancellable,
                                                             on_realm_proxy_created,
                                                             g_object_ref (async));
                                load->outstanding++;
                        }
                } else {
                        load->realms = g_list_prepend (load->realms, g_object_ref (realm));
                }
                g_variant_unref (info);
        }

        if (load->outstanding == 0)
                g_simple_async_result_complete_in_idle (async);

        g_object_unref (async);
}

static GList *
um_realm_manager_load_finish (UmRealmManager *self,
                              GAsyncResult *result,
                              GError **error)
{
        GSimpleAsyncResult *async = G_SIMPLE_ASYNC_RESULT (result);
        LoadClosure *load;
        GList *realms;

        if (g_simple_async_result_propagate_error (async, error))
                return NULL;

        load = g_simple_async_result_get_op_res_gpointer (async);
        realms = g_list_reverse (load->realms);
        load->realms = NULL;
        return realms;
}

static guint
hash_realm_info (gconstpointer value)
{
        const gchar *path, *iface;
        g_variant_get ((GVariant *)value, "(&o&s)", &path, &iface);
        return g_str_hash (path) ^ g_str_hash (iface);
}

static void
um_realm_manager_init (UmRealmManager *self)
{
        self->realms = g_hash_table_new_full (hash_realm_info, g_variant_equal,
                                              (GDestroyNotify)g_variant_unref,
                                              g_object_unref);
}

static void
um_realm_manager_notify (GObject *obj,
                         GParamSpec *spec)
{
        UmRealmManager *self = UM_REALM_MANAGER (obj);
        GVariant *realms;

        if (G_OBJECT_CLASS (um_realm_manager_parent_class)->notify)
                G_OBJECT_CLASS (um_realm_manager_parent_class)->notify (obj, spec);

        if (g_str_equal (spec->name, "realms")) {
                realms = um_realm_provider_get_realms (UM_REALM_PROVIDER (self));
                if (realms != NULL)
                        um_realm_manager_load (self, realms, NULL, NULL, NULL);
        }
}

static void
um_realm_manager_dispose (GObject *obj)
{
        UmRealmManager *self = UM_REALM_MANAGER (obj);
        GDBusConnection *connection;

        if (self->diagnostics_sig) {
                connection = g_dbus_proxy_get_connection (G_DBUS_PROXY (self));
                if (connection != NULL)
                        g_dbus_connection_signal_unsubscribe (connection, self->diagnostics_sig);
                self->diagnostics_sig = 0;
        }

        G_OBJECT_CLASS (um_realm_manager_parent_class)->dispose (obj);
}

static void
um_realm_manager_finalize (GObject *obj)
{
        UmRealmManager *self = UM_REALM_MANAGER (obj);

        g_hash_table_destroy (self->realms);

        G_OBJECT_CLASS (um_realm_manager_parent_class)->finalize (obj);
}

static void
um_realm_manager_class_init (UmRealmManagerClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);

        object_class->notify = um_realm_manager_notify;
        object_class->dispose = um_realm_manager_dispose;
        object_class->finalize = um_realm_manager_finalize;

        signals[REALM_ADDED] = g_signal_new ("realm-added", UM_TYPE_REALM_MANAGER,
                                             G_SIGNAL_RUN_FIRST, 0, NULL, NULL,
                                             g_cclosure_marshal_generic,
                                             G_TYPE_NONE, 1, UM_REALM_TYPE_KERBEROS);
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

static gboolean
number_at_least (const gchar *number,
                 guint minimum)
{
        gchar *end;

        if (strtol (number, &end, 10) < (long)minimum)
                return FALSE;
        if (!end || *end != '\0')
                return FALSE;
        return TRUE;
}

static gboolean
version_compare (const char *version,
                 guint req_major,
                 guint req_minor)
{
        gboolean match = FALSE;
        gchar **parts;

        parts = g_strsplit (version, ".", 2);

        if (parts[0] && parts[1]) {
                match = number_at_least (parts[0], req_major) &&
                        number_at_least (parts[1], req_minor);
        }

        g_strfreev (parts);
        return match;
}

static void
on_realm_manager_async_init (GObject *source,
                             GAsyncResult *result,
                             gpointer user_data)
{
        GSimpleAsyncResult *async = G_SIMPLE_ASYNC_RESULT (user_data);
        UmRealmManager *self;
        GError *error = NULL;
        const gchar *version;
        GDBusProxy *proxy;
        GObject *object;
        guint sig;

        object = g_async_initable_new_finish (G_ASYNC_INITABLE (source), result, &error);

        if (error != NULL)
                g_simple_async_result_take_error (async, error);

        /* This is temporary until the dbus interface stabilizes */
        if (object) {
                version = um_realm_provider_get_version (UM_REALM_PROVIDER (object));
                if (!version_compare (version, 0, 6)) {
                        /* No need to bother translators with this temporary message */
                        g_simple_async_result_set_error (async, UM_REALM_ERROR,
                                                         UM_REALM_ERROR_GENERIC,
                                                         "Unsupported version of realmd: %s", version);
                        g_object_unref (object);
                        object = NULL;
                }
        }

        if (object != NULL) {
                proxy = G_DBUS_PROXY (object);
                sig = g_dbus_connection_signal_subscribe (g_dbus_proxy_get_connection (proxy),
                                                          "org.freedesktop.realmd",
                                                          "org.freedesktop.realmd.Service",
                                                          "Diagnostics",
                                                          NULL,
                                                          NULL,
                                                          G_DBUS_SIGNAL_FLAGS_NONE,
                                                          on_realm_diagnostics,
                                                          NULL,
                                                          NULL);

                self = UM_REALM_MANAGER (object);
                self->diagnostics_sig = sig;
                g_simple_async_result_set_op_res_gpointer (async, self, g_object_unref);
        }

        g_simple_async_result_complete (async);
        g_object_unref (async);
}

void
um_realm_manager_new (GCancellable *cancellable,
                      GAsyncReadyCallback callback,
                      gpointer user_data)
{
        GSimpleAsyncResult *async;

        async = g_simple_async_result_new (NULL, callback, user_data,
                                           um_realm_manager_new);

        g_async_initable_new_async (UM_TYPE_REALM_MANAGER,
                                    G_PRIORITY_DEFAULT, cancellable,
                                    on_realm_manager_async_init, g_object_ref (async),
                                    "g-name", "org.freedesktop.realmd",
                                    "g-bus-type", G_BUS_TYPE_SYSTEM,
                                    "g-object-path", "/org/freedesktop/realmd",
                                    "g-interface-name", "org.freedesktop.realmd.Provider",
                                    "g-default-timeout", G_MAXINT,
                                    NULL);

        g_object_unref (async);
}

UmRealmManager *
um_realm_manager_new_finish (GAsyncResult *result,
                             GError **error)
{
        UmRealmManager *self;
        GSimpleAsyncResult *async;

        g_return_val_if_fail (g_simple_async_result_is_valid (result, NULL,
                              um_realm_manager_new), NULL);
        g_return_val_if_fail (error == NULL || *error == NULL, NULL);

        async = G_SIMPLE_ASYNC_RESULT (result);
        if (g_simple_async_result_propagate_error (async, error))
                return NULL;

        self = g_simple_async_result_get_op_res_gpointer (async);
        if (self != NULL)
                g_object_ref (self);
        return self;
}

typedef struct {
        GCancellable *cancellable;
        GList *realms;
} DiscoverClosure;

static void
discover_closure_free (gpointer data)
{
        DiscoverClosure *discover = data;
        g_clear_object (&discover->cancellable);
        g_list_free_full (discover->realms, g_object_unref);
        g_slice_free (DiscoverClosure, discover);
}

static void
on_manager_load (GObject *source,
                 GAsyncResult *result,
                 gpointer user_data)
{
        GSimpleAsyncResult *async = G_SIMPLE_ASYNC_RESULT (user_data);
        DiscoverClosure *discover = g_simple_async_result_get_op_res_gpointer (async);
        GError *error = NULL;

        discover->realms = um_realm_manager_load_finish (UM_REALM_MANAGER (source),
                                                         result, &error);
        if (error != NULL)
                g_simple_async_result_take_error (async, error);
        g_simple_async_result_complete (async);

        g_object_unref (async);
}

static void
on_provider_discover (GObject *source,
                      GAsyncResult *result,
                      gpointer user_data)
{
        GSimpleAsyncResult *async = G_SIMPLE_ASYNC_RESULT (user_data);
        DiscoverClosure *discover = g_simple_async_result_get_op_res_gpointer (async);
        UmRealmManager *self = UM_REALM_MANAGER (source);
        GError *error = NULL;
        GVariant *realms;
        gint relevance;

        um_realm_provider_call_discover_finish (UM_REALM_PROVIDER (self),
                                                &relevance, &realms, result, &error);
        if (error == NULL) {
                um_realm_manager_load (self, realms, discover->cancellable,
                                       on_manager_load, g_object_ref (async));

        } else {
                g_simple_async_result_take_error (async, error);
                g_simple_async_result_complete (async);
        }

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

        res = g_simple_async_result_new (G_OBJECT (self), callback, user_data,
                                         um_realm_manager_discover);
        discover = g_slice_new0 (DiscoverClosure);
        discover->cancellable = cancellable ? g_object_ref (cancellable) : NULL;
        g_simple_async_result_set_op_res_gpointer (res, discover, discover_closure_free);

	options = g_variant_new_array (G_VARIANT_TYPE ("{sv}"), NULL, 0);

        um_realm_provider_call_discover (UM_REALM_PROVIDER (self), input, options, cancellable,
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

        realms = discover->realms;
        discover->realms = NULL;
        return realms;
}

GList *
um_realm_manager_get_realms (UmRealmManager *self)
{
        g_return_val_if_fail (UM_IS_REALM_MANAGER (self), NULL);
        return g_hash_table_get_values (self->realms);
}

static const gchar *
find_supported_credentials (UmRealmKerberos *realm,
                            const gchar *owner)
{
        const gchar *cred_owner;
        const gchar *cred_type;
        GVariant *supported;
        GVariantIter iter;

        supported = um_realm_kerberos_get_supported_enroll_credentials (realm);
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
	g_simple_async_result_set_op_res_gpointer (async, g_object_ref (result), g_object_unref);
	g_simple_async_result_complete_in_idle (async);
	g_object_unref (async);
}

static gboolean
realm_join_as_owner (UmRealmKerberos *realm,
                     const gchar *owner,
                     const gchar *login,
                     const gchar *password,
                     GBytes *credentials,
                     GCancellable *cancellable,
                     GAsyncReadyCallback callback,
                     gpointer user_data)
{
        GSimpleAsyncResult *async;
        GVariant *contents;
        GVariant *options;
        GVariant *creds;
        const gchar *type;

        type = find_supported_credentials (realm, owner);
        if (type == NULL)
                return FALSE;

        async = g_simple_async_result_new (G_OBJECT (realm), callback, user_data,
                                           realm_join_as_owner);

        if (g_str_equal (type, "ccache")) {
                contents = g_variant_new_from_data (G_VARIANT_TYPE ("ay"),
                                                    g_bytes_get_data (credentials, NULL),
                                                    g_bytes_get_size (credentials),
                                                    TRUE, (GDestroyNotify)g_bytes_unref, credentials);

        } else if (g_str_equal (type, "password")) {
                contents = g_variant_new ("(ss)", login, password);

        } else {
                g_assert_not_reached ();
        }

        creds = g_variant_new ("(ssv)", type, owner, contents);
        options = g_variant_new_array (G_VARIANT_TYPE ("{sv}"), NULL, 0);

        um_realm_kerberos_call_enroll (realm, creds, options,
                                       cancellable, on_realm_join_complete, g_object_ref (async));

        g_object_unref (async);

        return TRUE;
}

gboolean
um_realm_join_as_user (UmRealmKerberos *realm,
                       const gchar *login,
                       const gchar *password,
                       GBytes *credentials,
                       GCancellable *cancellable,
                       GAsyncReadyCallback callback,
                       gpointer user_data)
{
        g_return_val_if_fail (UM_REALM_IS_KERBEROS (realm), FALSE);
        g_return_val_if_fail (credentials != NULL, FALSE);
        g_return_val_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable), FALSE);
        g_return_val_if_fail (login != NULL, FALSE);
        g_return_val_if_fail (password != NULL, FALSE);
        g_return_val_if_fail (credentials != NULL, FALSE);

        return realm_join_as_owner (realm, "user", login, password, credentials,
                                    cancellable, callback, user_data);
}

gboolean
um_realm_join_as_admin (UmRealmKerberos *realm,
                        const gchar *login,
                        const gchar *password,
                        GBytes *credentials,
                        GCancellable *cancellable,
                        GAsyncReadyCallback callback,
                        gpointer user_data)
{
        g_return_val_if_fail (UM_REALM_IS_KERBEROS (realm), FALSE);
        g_return_val_if_fail (credentials != NULL, FALSE);
        g_return_val_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable), FALSE);
        g_return_val_if_fail (login != NULL, FALSE);
        g_return_val_if_fail (password != NULL, FALSE);
        g_return_val_if_fail (credentials != NULL, FALSE);

        return realm_join_as_owner (realm, "administrator", login, password, credentials,
                                    cancellable, callback, user_data);
}

gboolean
um_realm_join_finish (UmRealmKerberos *self,
                      GAsyncResult *result,
                      GError **error)
{
        GError *call_error = NULL;
        gchar *dbus_error;
        GAsyncResult *async;

        g_return_val_if_fail (UM_REALM_IS_KERBEROS (self), FALSE);
        g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

        async = g_simple_async_result_get_op_res_gpointer (G_SIMPLE_ASYNC_RESULT (result));
        um_realm_kerberos_call_enroll_finish (self, async, &call_error);
        if (call_error == NULL)
                return TRUE;

        dbus_error = g_dbus_error_get_remote_error (call_error);
        if (dbus_error == NULL) {
                g_propagate_error (error, call_error);
                return FALSE;
        }

        g_dbus_error_strip_remote_error (call_error);

        if (g_str_equal (dbus_error, "org.freedesktop.realmd.Error.AuthFailed")) {
                g_set_error (error, UM_REALM_ERROR, UM_REALM_ERROR_BAD_LOGIN,
                             "%s", call_error->message);
                g_error_free (call_error);
        } else {
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
        g_free (name);

        if (code != 0)
                return code;

        if (filename == NULL)
                code = krb5_cc_default (k5, &ccache);
        else
                code = krb5_cc_resolve (k5, filename, &ccache);

        if (code != 0) {
                krb5_free_principal (k5, principal);
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

        if (code == 0)
                krb5_free_cred_contents (k5, &creds);

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
                        } else {
                                g_warning ("Couldn't read credential cache: %s", error->message);
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
                g_free (filename);
        }

        if (k5)
                krb5_free_context (k5);
}

void
um_realm_login (const gchar *realm,
                const gchar *domain,
                const gchar *user,
                const gchar *password,
                GCancellable *cancellable,
                GAsyncReadyCallback callback,
                gpointer user_data)
{
        GSimpleAsyncResult *async;
        LoginClosure *login;

        g_return_if_fail (realm != NULL);
        g_return_if_fail (user != NULL);
        g_return_if_fail (password != NULL);
        g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

        async = g_simple_async_result_new (NULL, callback, user_data,
                                           um_realm_login);
        login = g_slice_new0 (LoginClosure);
        login->domain = g_strdup (domain ? domain : realm);
        login->realm = g_strdup (realm);
        login->user = g_strdup (user);
        login->password = g_strdup (password);
        g_simple_async_result_set_op_res_gpointer (async, login, login_closure_free);

        g_simple_async_result_set_handle_cancellation (async, TRUE);
        g_simple_async_result_run_in_thread (async, kinit_thread_func,
                                             G_PRIORITY_DEFAULT, cancellable);

        g_object_unref (async);
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
