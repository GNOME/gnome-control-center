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

#include "config.h"

#include "gsd-kerberos-identity-manager.h"
#include "gsd-identity-manager.h"
#include "gsd-identity-manager-private.h"
#include "gsd-kerberos-identity.h"

#include <fcntl.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <glib/gi18n.h>
#include <glib/gstdio.h>
#include <gio/gio.h>

#include <krb5.h>

struct _GsdKerberosIdentityManagerPrivate
{
        GHashTable      *identities;
        GHashTable      *expired_identities;
        GHashTable      *identities_by_realm;
        GAsyncQueue     *pending_operations;
        GCancellable    *scheduler_cancellable;

        krb5_context     kerberos_context;
        GFileMonitor    *credentials_cache_monitor;
        gulong           credentials_cache_changed_signal_id;
        char            *credentials_cache_type;

        GMutex           scheduler_job_lock;
        GCond            scheduler_job_unblocked;
        gboolean         is_blocking_scheduler_job;

        volatile int     pending_refresh_count;
};

typedef enum
{
        OPERATION_TYPE_REFRESH,
        OPERATION_TYPE_LIST,
        OPERATION_TYPE_RENEW,
        OPERATION_TYPE_SIGN_IN,
        OPERATION_TYPE_SIGN_OUT,
        OPERATION_TYPE_STOP_JOB
} OperationType;

typedef struct
{
        GCancellable               *cancellable;
        GsdKerberosIdentityManager *manager;
        OperationType               type;
        GSimpleAsyncResult         *result;
        GIOSchedulerJob            *job;
        union {
                GsdIdentity        *identity;
                struct {
                        const char             *identifier;
                        GsdIdentityInquiry     *inquiry;
                        GsdIdentityInquiryFunc  inquiry_func;
                        gpointer                inquiry_data;
                        GMutex                  inquiry_lock;
                        GCond                   inquiry_finished_condition;
                        volatile gboolean       is_inquiring;
                };
        };
} Operation;

typedef struct
{
        GsdKerberosIdentityManager *manager;
        GsdIdentity                *identity;
} IdentitySignalWork;

static void identity_manager_interface_init (GsdIdentityManagerInterface *interface);
static void initable_interface_init (GInitableIface *interface);

static void on_identity_expired (GsdIdentity                *identity,
                                 GsdKerberosIdentityManager *self);

void gsd_kerberos_identity_manager_start_test (GsdKerberosIdentityManager  *manager,
                                               GError                     **error);
void gsd_kerberos_identity_manager_stop_test (GsdKerberosIdentityManager  *manager,
                                              GError                     **error);

G_DEFINE_TYPE_WITH_CODE (GsdKerberosIdentityManager,
                         gsd_kerberos_identity_manager,
                         G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (GSD_TYPE_IDENTITY_MANAGER,
                                                identity_manager_interface_init)
                         G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE,
                                                initable_interface_init));

static Operation *
operation_new (GsdKerberosIdentityManager *self,
               GCancellable               *cancellable,
               OperationType               type,
               GSimpleAsyncResult         *result)
{
        Operation *operation;

        operation = g_slice_new (Operation);

        operation->manager = self;
        operation->type = type;

        if (cancellable == NULL) {
                cancellable = g_cancellable_new ();
        } else {
                g_object_ref (cancellable);
        }
        operation->cancellable = cancellable;

        if (result != NULL) {
                g_object_ref (result);
        }
        operation->result = result;

        operation->identity = NULL;

        return operation;
}

static void
operation_free (Operation *operation)
{
       g_clear_object (&operation->cancellable);

       if (operation->type != OPERATION_TYPE_SIGN_IN) {
               g_clear_object (&operation->identity);
       } else {
               g_clear_pointer (&operation->identifier, g_free);
       }

       g_clear_object (&operation->result);

       g_slice_free (Operation, operation);
}

static void
schedule_refresh (GsdKerberosIdentityManager *self)
{
        Operation *operation;

        g_atomic_int_inc (&self->priv->pending_refresh_count);

        operation = operation_new (self, NULL, OPERATION_TYPE_REFRESH, NULL);
        g_async_queue_push (self->priv->pending_operations, operation);
}

static IdentitySignalWork *
identity_signal_work_new (GsdKerberosIdentityManager *self,
                          GsdIdentity                *identity)
{
        IdentitySignalWork *work;

        work = g_slice_new (IdentitySignalWork);
        work->manager = self;
        work->identity = g_object_ref (identity);

        return work;
}

static void
identity_signal_work_free (IdentitySignalWork *work)
{
        g_object_unref (work->identity);
        g_slice_free (IdentitySignalWork, work);
}

static void
on_identity_expired (GsdIdentity                *identity,
                     GsdKerberosIdentityManager *self)
{
        _gsd_identity_manager_emit_identity_expired (GSD_IDENTITY_MANAGER (self), identity);
}

static void
on_identity_unexpired (GsdIdentity                *identity,
                       GsdKerberosIdentityManager *self)
{
        g_debug ("GsdKerberosIdentityManager: identity unexpired");
        /* If an identity is now unexpired, that means some sort of weird
         * clock skew happened and we should just do a full refresh, since it's
         * probably affected more than one identity
         */
        schedule_refresh (self);
}

static void
on_identity_expiring (GsdIdentity                *identity,
                      GsdKerberosIdentityManager *self)
{
        g_debug ("GsdKerberosIdentityManager: identity about to expire");
        _gsd_identity_manager_emit_identity_expiring (GSD_IDENTITY_MANAGER (self), identity);
}

static void
on_identity_needs_renewal (GsdIdentity                *identity,
                           GsdKerberosIdentityManager *self)
{
        g_debug ("GsdKerberosIdentityManager: identity needs renewal");
        _gsd_identity_manager_emit_identity_needs_renewal (GSD_IDENTITY_MANAGER (self), identity);
}

static void
on_identity_needs_refresh (GsdIdentity                *identity,
                           GsdKerberosIdentityManager *self)
{
        g_debug ("GsdKerberosIdentityManager: needs refresh");
        schedule_refresh (self);
}

static void
watch_for_identity_expiration (GsdKerberosIdentityManager *self,
                               GsdIdentity                *identity)
{
        g_signal_handlers_disconnect_by_func (G_OBJECT (identity),
                                              G_CALLBACK (on_identity_expired),
                                              self);
        g_signal_connect (G_OBJECT (identity),
                          "expired",
                          G_CALLBACK (on_identity_expired),
                          self);

        g_signal_handlers_disconnect_by_func (G_OBJECT (identity),
                                              G_CALLBACK (on_identity_unexpired),
                                              self);
        g_signal_connect (G_OBJECT (identity),
                          "unexpired",
                          G_CALLBACK (on_identity_unexpired),
                          self);

        g_signal_handlers_disconnect_by_func (G_OBJECT (identity),
                                              G_CALLBACK (on_identity_expiring),
                                              self);
        g_signal_connect (G_OBJECT (identity),
                          "expiring",
                          G_CALLBACK (on_identity_expiring),
                          self);

        g_signal_handlers_disconnect_by_func (G_OBJECT (identity),
                                              G_CALLBACK (on_identity_needs_renewal),
                                              self);
        g_signal_connect (G_OBJECT (identity),
                          "needs-renewal",
                          G_CALLBACK (on_identity_needs_renewal),
                          self);

        g_signal_handlers_disconnect_by_func (G_OBJECT (identity),
                                              G_CALLBACK (on_identity_needs_refresh),
                                              self);
        g_signal_connect (G_OBJECT (identity),
                          "needs-refresh",
                          G_CALLBACK (on_identity_needs_refresh),
                          self);
}

static void
do_identity_signal_added_work (IdentitySignalWork *work)
{
        GsdKerberosIdentityManager *self = work->manager;
        GsdIdentity *identity = work->identity;

        watch_for_identity_expiration (self, identity);
        _gsd_identity_manager_emit_identity_added (GSD_IDENTITY_MANAGER (self), identity);
}

static void
do_identity_signal_removed_work (IdentitySignalWork *work)
{
        GsdKerberosIdentityManager *self = work->manager;
        GsdIdentity *identity = work->identity;

        _gsd_identity_manager_emit_identity_removed (GSD_IDENTITY_MANAGER (self), identity);
}

static void
do_identity_signal_renamed_work (IdentitySignalWork *work)
{
        GsdKerberosIdentityManager *self = work->manager;
        GsdIdentity *identity = work->identity;

        _gsd_identity_manager_emit_identity_renamed (GSD_IDENTITY_MANAGER (self), identity);
}

static void
do_identity_signal_refreshed_work (IdentitySignalWork *work)
{
        GsdKerberosIdentityManager *self = work->manager;
        GsdIdentity *identity = work->identity;

        watch_for_identity_expiration (self, identity);
        _gsd_identity_manager_emit_identity_refreshed (GSD_IDENTITY_MANAGER (self), identity);
}

static void
remove_identity (GsdKerberosIdentityManager *self,
                 Operation                  *operation,
                 GsdIdentity                *identity)
{

        IdentitySignalWork *work;
        const char *identifier;
        char *name;
        GList *other_identities = NULL;

        identifier = gsd_identity_get_identifier (identity);
        name = gsd_kerberos_identity_get_realm_name (GSD_KERBEROS_IDENTITY (identity));

        if (name != NULL) {
                other_identities = g_hash_table_lookup (self->priv->identities_by_realm,
                                                        name);
                g_hash_table_remove (self->priv->identities_by_realm, name);

                other_identities = g_list_remove (other_identities, identity);
        }


        if (other_identities != NULL) {
                g_hash_table_replace (self->priv->identities_by_realm,
                                      g_strdup (name),
                                      other_identities);
        }
        g_free (name);

        work = identity_signal_work_new (self, identity);
        g_hash_table_remove (self->priv->expired_identities,
                             identifier);
        g_hash_table_remove (self->priv->identities,
                             identifier);

        g_io_scheduler_job_send_to_mainloop (operation->job,
                                             (GSourceFunc)
                                             do_identity_signal_removed_work,
                                             work,
                                             (GDestroyNotify)
                                             identity_signal_work_free);
        /* If there's only one identity for this realm now, then we can
         * rename that identity to just the realm name
         */
        if (other_identities != NULL && other_identities->next == NULL) {
                GsdIdentity *other_identity = other_identities->data;

                work = identity_signal_work_new (self, other_identity);

                g_io_scheduler_job_send_to_mainloop (operation->job,
                                                     (GSourceFunc)
                                                     do_identity_signal_renamed_work,
                                                     work,
                                                     (GDestroyNotify)
                                                     identity_signal_work_free);
        }
}

static void
drop_stale_identities (GsdKerberosIdentityManager *self,
                       Operation                  *operation,
                       GHashTable                 *known_identities)
{
        GList *stale_identity_ids;
        GList *node;

        stale_identity_ids = g_hash_table_get_keys (self->priv->identities);

        node = stale_identity_ids;
        while (node != NULL) {
                GsdIdentity *identity;
                const char *identifier = node->data;

                identity = g_hash_table_lookup (known_identities, identifier);
                if (identity == NULL) {
                        identity = g_hash_table_lookup (self->priv->identities,
                                                        identifier);

                        if (identity != NULL) {
                                remove_identity (self, operation, identity);
                        }
                }
                node = node->next;
        }
        g_list_free (stale_identity_ids);
}

static void
update_identity (GsdKerberosIdentityManager *self,
                 Operation                  *operation,
                 GsdIdentity                *identity,
                 GsdIdentity                *new_identity)
{

        gsd_kerberos_identity_update (GSD_KERBEROS_IDENTITY (identity),
                                      GSD_KERBEROS_IDENTITY (new_identity));

        if (gsd_identity_is_signed_in (identity)) {
                IdentitySignalWork *work;

                /* if it's not expired, send out a refresh signal */
                g_debug ("GsdKerberosIdentityManager: identity '%s' refreshed",
                         gsd_identity_get_identifier (identity));

                work = identity_signal_work_new (self, identity);
                g_io_scheduler_job_send_to_mainloop (operation->job,
                                                     (GSourceFunc)
                                                     do_identity_signal_refreshed_work,
                                                     work,
                                                     (GDestroyNotify)
                                                     identity_signal_work_free);
        }
}

static void
add_identity (GsdKerberosIdentityManager *self,
              Operation                  *operation,
              GsdIdentity                *identity,
              const char                 *identifier)
{
        IdentitySignalWork *work;

        g_hash_table_replace (self->priv->identities,
                              g_strdup (identifier),
                              g_object_ref (identity));

        if (!gsd_identity_is_signed_in (identity)) {
                g_hash_table_replace (self->priv->expired_identities,
                                      g_strdup (identifier),
                                      identity);
        }

        work = identity_signal_work_new (self, identity);
        g_io_scheduler_job_send_to_mainloop (operation->job,
                                             (GSourceFunc)
                                             do_identity_signal_added_work,
                                             work,
                                             (GDestroyNotify)
                                             identity_signal_work_free);
}

static void
refresh_identity (GsdKerberosIdentityManager *self,
                  Operation                  *operation,
                  GHashTable                 *refreshed_identities,
                  GsdIdentity                *identity)
{
        const char *identifier;
        GsdIdentity *old_identity;

        identifier = gsd_identity_get_identifier (identity);

        if (identifier == NULL) {
                return;
        }
        old_identity = g_hash_table_lookup (self->priv->identities, identifier);

        if (old_identity != NULL) {
                g_debug ("GsdKerberosIdentityManager: refreshing identity '%s'", identifier);
                update_identity (self, operation, old_identity, identity);

                /* Reuse the old identity, so any object data set up on it doesn't
                 * disappear spurriously
                 */
                identifier = gsd_identity_get_identifier (old_identity);
                identity = old_identity;
        } else {
                g_debug ("GsdKerberosIdentityManager: adding new identity '%s'", identifier);
                add_identity (self, operation, identity, identifier);
        }

        /* Track refreshed identities so we can emit removals when we're done fully
         * enumerating the collection of credential caches
         */
        g_hash_table_replace (refreshed_identities,
                              g_strdup (identifier),
                              g_object_ref (identity));
}

static gboolean
refresh_identities (GsdKerberosIdentityManager *self,
                    Operation                  *operation)
{
        krb5_error_code error_code;
        krb5_ccache cache;
        krb5_cccol_cursor cursor;
        const char *error_message;
        GHashTable *refreshed_identities;

        /* If we have more refreshes queued up, don't bother doing this one
         */
        if (!g_atomic_int_dec_and_test (&self->priv->pending_refresh_count)) {
                return FALSE;
        }

        g_debug ("GsdKerberosIdentityManager: Refreshing identities");
        refreshed_identities = g_hash_table_new_full (g_str_hash,
                                                      g_str_equal,
                                                      (GDestroyNotify)
                                                      g_free,
                                                      (GDestroyNotify)
                                                      g_object_unref);
        error_code = krb5_cccol_cursor_new (self->priv->kerberos_context, &cursor);

        if (error_code != 0) {
                error_message = krb5_get_error_message (self->priv->kerberos_context, error_code);
                g_debug ("GsdKerberosIdentityManager:         Error looking up available credential caches: %s", error_message);
                krb5_free_error_message (self->priv->kerberos_context, error_message);
                goto done;
        }

        error_code = krb5_cccol_cursor_next (self->priv->kerberos_context,
                                             cursor,
                                             &cache);

        while (error_code == 0 && cache != NULL) {
                GsdIdentity *identity;

                identity = gsd_kerberos_identity_new (self->priv->kerberos_context,
                                                      cache);

                if (identity != NULL) {
                        refresh_identity (self, operation, refreshed_identities, identity);
                }

                krb5_cc_close (self->priv->kerberos_context, cache);
                error_code = krb5_cccol_cursor_next (self->priv->kerberos_context,
                                                     cursor,
                                                     &cache);
        }

        if (error_code != 0) {
                error_message = krb5_get_error_message (self->priv->kerberos_context, error_code);
                g_debug ("GsdKerberosIdentityManager:         Error iterating over available credential caches: %s", error_message);
                krb5_free_error_message (self->priv->kerberos_context, error_message);
        }

        krb5_cccol_cursor_free (self->priv->kerberos_context, &cursor);
done:
        drop_stale_identities (self, operation, refreshed_identities);
        g_hash_table_unref (refreshed_identities);

        return TRUE;
}

static int
identity_sort_func (GsdIdentity *a,
                    GsdIdentity *b)
{
        return g_strcmp0 (gsd_identity_get_identifier (a),
                          gsd_identity_get_identifier (b));
}

static void
free_identity_list (GList *list)
{
        g_list_foreach (list, (GFunc) g_object_unref, NULL);
        g_list_free (list);
}

static void
list_identities (GsdKerberosIdentityManager *self,
                 Operation                  *operation)
{
        GList *identities;

        g_debug ("GsdKerberosIdentityManager: Listing identities");
        identities = g_hash_table_get_values (self->priv->identities);

        identities = g_list_sort (identities,
                                  (GCompareFunc)
                                  identity_sort_func);

        g_list_foreach (identities, (GFunc) g_object_ref, NULL);
        g_simple_async_result_set_op_res_gpointer (operation->result,
                                                   identities,
                                                   (GDestroyNotify)
                                                   free_identity_list);
}

static void
renew_identity (GsdKerberosIdentityManager *self,
                Operation                  *operation)
{
        GError *error;
        gboolean was_renewed;
        char *identity_name;

        identity_name = gsd_kerberos_identity_get_principal_name (GSD_KERBEROS_IDENTITY (operation->identity));
        g_debug ("GsdKerberosIdentityManager: renewing identity %s", identity_name);
        g_free (identity_name);

        error = NULL;
        was_renewed = gsd_kerberos_identity_renew (GSD_KERBEROS_IDENTITY (operation->identity),
                                                   &error);

        if (!was_renewed) {
                g_debug ("GsdKerberosIdentityManager: could not renew identity: %s",
                         error->message);

                g_simple_async_result_set_from_error (operation->result,
                                                      error);
        }

        g_simple_async_result_set_op_res_gboolean (operation->result,
                                                   was_renewed);
}

static void
do_identity_inquiry (Operation *operation)
{
        if (operation->inquiry_func == NULL) {
                return;
        }

        operation->inquiry_func (operation->inquiry,
                                 operation->cancellable,
                                 operation->inquiry_data);
}

static void
stop_waiting_on_inquiry (Operation *operation)
{
        g_mutex_lock (&operation->inquiry_lock);
        if (operation->is_inquiring) {
                operation->is_inquiring = FALSE;
                g_cond_signal (&operation->inquiry_finished_condition);
        }
        g_mutex_unlock (&operation->inquiry_lock);
}

static void
on_kerberos_identity_inquiry_complete (GsdIdentityInquiry *inquiry,
                                       Operation          *operation)
{
        stop_waiting_on_inquiry (operation);
}

static void
start_inquiry (Operation          *operation,
               GsdIdentityInquiry *inquiry)
{
        operation->is_inquiring = TRUE;

        g_signal_connect (G_OBJECT (inquiry),
                          "complete",
                          G_CALLBACK (on_kerberos_identity_inquiry_complete),
                          operation);

        operation->inquiry = inquiry;
        g_io_scheduler_job_send_to_mainloop (operation->job,
                                             (GSourceFunc)
                                             do_identity_inquiry,
                                             operation,
                                             (GDestroyNotify)
                                             NULL);
}

static void
wait_for_inquiry_to_complete (Operation                  *operation,
                              GsdKerberosIdentityInquiry *inquiry)
{
        g_mutex_lock (&operation->inquiry_lock);
        while (operation->is_inquiring) {
                g_cond_wait (&operation->inquiry_finished_condition,
                             &operation->inquiry_lock);
        }
        g_mutex_unlock (&operation->inquiry_lock);
}

static void
on_sign_in_operation_cancelled (GCancellable *cancellable,
                                Operation    *operation)
{
        stop_waiting_on_inquiry (operation);
}

static void
on_kerberos_identity_inquiry (GsdKerberosIdentityInquiry *inquiry,
                              GCancellable               *cancellable,
                              Operation                  *operation)
{
        gulong handler_id;

        start_inquiry (operation, GSD_IDENTITY_INQUIRY (inquiry));

        handler_id = g_cancellable_connect (cancellable,
                                            G_CALLBACK (on_sign_in_operation_cancelled),
                                            operation,
                                            NULL);

        if ((operation->cancellable == NULL) ||
            !g_cancellable_is_cancelled (operation->cancellable)) {
                wait_for_inquiry_to_complete (operation, inquiry);
        }

        g_cancellable_disconnect (cancellable, handler_id);
}

static void
sign_in_identity (GsdKerberosIdentityManager *self,
                  Operation                  *operation)
{
        GsdIdentity     *identity;
        GError          *error;
        krb5_error_code  error_code;

        g_debug ("GsdKerberosIdentityManager: signing in identity %s", operation->identifier);
        identity = g_hash_table_lookup (self->priv->identities,
                                        operation->identifier);
        if (identity == NULL) {
                krb5_ccache credentials_cache;
                error_code = krb5_cc_new_unique (self->priv->kerberos_context,
                                                 self->priv->credentials_cache_type,
                                                 NULL,
                                                 &credentials_cache);

                if (error_code != 0) {
                        const char *error_message;

                        error_message = krb5_get_error_message (self->priv->kerberos_context, error_code);
                        g_debug ("GsdKerberosIdentityManager:         Error creating new cache for identity credentials: %s", error_message);
                        krb5_free_error_message (self->priv->kerberos_context, error_message);
                } else {
                        identity = gsd_kerberos_identity_new (self->priv->kerberos_context,
                                                              credentials_cache);
                        if (identity == NULL) {
                                krb5_cc_close (self->priv->kerberos_context, credentials_cache);
                        }
                }
        }

        if (identity == NULL) {
                g_simple_async_result_set_error (operation->result,
                                                 GSD_IDENTITY_MANAGER_ERROR,
                                                 GSD_IDENTITY_MANAGER_ERROR_SIGNING_IN,
                                                 _("Could not create identity"));
                g_simple_async_result_set_op_res_gpointer (operation->result,
                                                           NULL, NULL);

                return;
        }

        g_hash_table_replace (self->priv->identities,
                              g_strdup (operation->identifier),
                              g_object_ref (identity));

        error = NULL;
        if (!gsd_kerberos_identity_sign_in (GSD_KERBEROS_IDENTITY (identity),
                                            operation->identifier,
                                            (GsdIdentityInquiryFunc)
                                            on_kerberos_identity_inquiry,
                                            operation,
                                            NULL,
                                            operation->cancellable,
                                            &error)) {
                g_simple_async_result_set_from_error (operation->result,
                                                      error);
                g_simple_async_result_set_op_res_gpointer (operation->result,
                                                           NULL, NULL);

        } else {
                g_simple_async_result_set_op_res_gpointer (operation->result,
                                                           g_object_ref (identity),
                                                           (GDestroyNotify)
                                                           g_object_unref);
        }
}

static void
sign_out_identity (GsdKerberosIdentityManager *self,
                   Operation                  *operation)
{
        GError *error;
        gboolean was_signed_out;
        char *identity_name;

        identity_name = gsd_kerberos_identity_get_principal_name (GSD_KERBEROS_IDENTITY (operation->identity));
        g_debug ("GsdKerberosIdentityManager: signing out identity %s", identity_name);
        g_free (identity_name);

        error = NULL;
        was_signed_out = gsd_kerberos_identity_erase (GSD_KERBEROS_IDENTITY (operation->identity),
                                                      &error);

        if (!was_signed_out) {
                g_debug ("GsdKerberosIdentityManager: could not sign out identity: %s",
                         error->message);
                g_error_free (error);
        }
}

static void
block_scheduler_job (GsdKerberosIdentityManager *self)
{
        g_mutex_lock (&self->priv->scheduler_job_lock);
        while (self->priv->is_blocking_scheduler_job) {
                g_cond_wait (&self->priv->scheduler_job_unblocked, &self->priv->scheduler_job_lock);
        }
        self->priv->is_blocking_scheduler_job = TRUE;
        g_mutex_unlock (&self->priv->scheduler_job_lock);
}

static void
stop_blocking_scheduler_job (GsdKerberosIdentityManager *self)
{
        g_mutex_lock (&self->priv->scheduler_job_lock);
        self->priv->is_blocking_scheduler_job = FALSE;
        g_cond_signal (&self->priv->scheduler_job_unblocked);
        g_mutex_unlock (&self->priv->scheduler_job_lock);
}

static void
wait_for_scheduler_job_to_become_unblocked (GsdKerberosIdentityManager *self)
{
        g_mutex_lock (&self->priv->scheduler_job_lock);
        while (self->priv->is_blocking_scheduler_job) {
                g_cond_wait (&self->priv->scheduler_job_unblocked, &self->priv->scheduler_job_lock);
        }
        g_mutex_unlock (&self->priv->scheduler_job_lock);
}

static void
on_job_cancelled (GCancellable               *cancellable,
                  GsdKerberosIdentityManager *self)
{
        Operation *operation;
        operation = operation_new (self,
                                   cancellable,
                                   OPERATION_TYPE_STOP_JOB,
                                   NULL);
        g_async_queue_push (self->priv->pending_operations, operation);

        stop_blocking_scheduler_job (self);
}

static gboolean
on_job_scheduled (GIOSchedulerJob            *job,
                  GCancellable               *cancellable,
                  GsdKerberosIdentityManager *self)
{
        GAsyncQueue *pending_operations;

        g_assert (cancellable != NULL);

        g_cancellable_connect (cancellable,
                               G_CALLBACK (on_job_cancelled),
                               self,
                               NULL);

        /* Take ownership of queue, since we may out live the identity manager */
        pending_operations = g_async_queue_ref (self->priv->pending_operations);
        while (!g_cancellable_is_cancelled (cancellable)) {
                Operation *operation;
                gboolean   processed_operation;
                GError    *error = NULL;

                operation = g_async_queue_pop (pending_operations);

                if (operation->result != NULL &&
                    g_cancellable_set_error_if_cancelled (operation->cancellable, &error)) {
                        g_simple_async_result_take_error (operation->result,
                                                          error);
                        g_simple_async_result_complete_in_idle (operation->result);
                        g_object_unref (operation->result);
                        operation->result = NULL;
                        continue;
                }

                operation->job = job;

                switch (operation->type) {
                        case OPERATION_TYPE_STOP_JOB:
                                /* do nothing, loop will exit next iteration since cancellable
                                 * is cancelled
                                 */
                                g_assert (g_cancellable_is_cancelled (cancellable));
                                operation_free (operation);
                                continue;
                        case OPERATION_TYPE_REFRESH:
                                processed_operation = refresh_identities (operation->manager, operation);
                                break;
                        case OPERATION_TYPE_LIST:
                                list_identities (operation->manager, operation);
                                processed_operation = TRUE;

                                /* We want to block refreshes (and their associated "added"
                                 * and "removed" signals) until the caller has had
                                 * a chance to look at the batch of
                                 * results we already processed
                                 */
                                g_assert (operation->result != NULL);

                                g_debug ("GsdKerberosIdentityManager:         Blocking until identities list processed");
                                block_scheduler_job (self);
                                g_object_weak_ref (G_OBJECT (operation->result),
                                                   (GWeakNotify)
                                                   stop_blocking_scheduler_job,
                                                   self);
                                g_debug ("GsdKerberosIdentityManager:         Continuing");
                                break;
                        case OPERATION_TYPE_SIGN_IN:
                                sign_in_identity (operation->manager, operation);
                                processed_operation = TRUE;
                                break;
                        case OPERATION_TYPE_SIGN_OUT:
                                sign_out_identity (operation->manager, operation);
                                processed_operation = TRUE;
                                break;
                        case OPERATION_TYPE_RENEW:
                                renew_identity (operation->manager, operation);
                                processed_operation = TRUE;
                                break;
                }

                operation->job = NULL;

                if (operation->result != NULL) {
                        g_simple_async_result_complete_in_idle (operation->result);
                        g_object_unref (operation->result);
                        operation->result = NULL;
                }
                operation_free (operation);

                wait_for_scheduler_job_to_become_unblocked (self);

                /* Don't bother saying "Waiting for next operation" if this operation
                 * was a no-op, since the debug spew probably already says the message
                 */
                if (processed_operation) {
                        g_debug ("GsdKerberosIdentityManager: Waiting for next operation");
                }
        }

        g_async_queue_unref (pending_operations);

        return FALSE;
}

static void
gsd_kerberos_identity_manager_list_identities (GsdIdentityManager          *manager,
                                               GCancellable               *cancellable,
                                               GAsyncReadyCallback         callback,
                                               gpointer                    user_data)
{
        GsdKerberosIdentityManager *self = GSD_KERBEROS_IDENTITY_MANAGER (manager);
        GSimpleAsyncResult *result;
        Operation *operation;

        result = g_simple_async_result_new (G_OBJECT (self),
                                            callback,
                                            user_data,
                                            gsd_kerberos_identity_manager_list_identities);

        operation = operation_new (self,
                                   cancellable,
                                   OPERATION_TYPE_LIST,
                                   result);
        g_object_unref (result);

        g_async_queue_push (self->priv->pending_operations, operation);
}

static GList *
gsd_kerberos_identity_manager_list_identities_finish (GsdIdentityManager  *manager,
                                                      GAsyncResult        *result,
                                                      GError             **error)
{
        GList *identities;

        if (g_simple_async_result_propagate_error (G_SIMPLE_ASYNC_RESULT (result),
                                                   error)) {
                return NULL;
        }

        identities = g_simple_async_result_get_op_res_gpointer (G_SIMPLE_ASYNC_RESULT (result));

        return identities;

}

static void
gsd_kerberos_identity_manager_renew_identity (GsdIdentityManager         *manager,
                                              GsdIdentity                *identity,
                                              GCancellable               *cancellable,
                                              GAsyncReadyCallback         callback,
                                              gpointer                    user_data)
{
        GsdKerberosIdentityManager *self = GSD_KERBEROS_IDENTITY_MANAGER (manager);
        GSimpleAsyncResult *result;
        Operation *operation;

        result = g_simple_async_result_new (G_OBJECT (self),
                                            callback,
                                            user_data,
                                            gsd_kerberos_identity_manager_renew_identity);
        operation = operation_new (self,
                                   cancellable,
                                   OPERATION_TYPE_RENEW,
                                   result);
        g_object_unref (result);

        operation->identity = g_object_ref (identity);

        g_async_queue_push (self->priv->pending_operations, operation);
}

static void
gsd_kerberos_identity_manager_renew_identity_finish (GsdIdentityManager  *self,
                                                     GAsyncResult        *result,
                                                     GError             **error)
{
        if (g_simple_async_result_propagate_error (G_SIMPLE_ASYNC_RESULT (result),
                                                   error)) {
                return;
        }

        return;
}

static void
gsd_kerberos_identity_manager_sign_identity_in (GsdIdentityManager     *manager,
                                                const char             *identifier,
                                                GsdIdentityInquiryFunc  inquiry_func,
                                                gpointer                inquiry_data,
                                                GCancellable           *cancellable,
                                                GAsyncReadyCallback     callback,
                                                gpointer                user_data)
{
        GsdKerberosIdentityManager *self = GSD_KERBEROS_IDENTITY_MANAGER (manager);
        GSimpleAsyncResult *result;
        Operation *operation;

        result = g_simple_async_result_new (G_OBJECT (self),
                                            callback,
                                            user_data,
                                            gsd_kerberos_identity_manager_sign_identity_in);
        operation = operation_new (self,
                                   cancellable,
                                   OPERATION_TYPE_SIGN_IN,
                                   result);
        g_object_unref (result);

        operation->identifier = g_strdup (identifier);
        operation->inquiry_func = inquiry_func;
        operation->inquiry_data = inquiry_data;
        g_mutex_init (&operation->inquiry_lock);
        g_cond_init (&operation->inquiry_finished_condition);
        operation->is_inquiring = FALSE;

        g_async_queue_push (self->priv->pending_operations, operation);
}

static GsdIdentity *
gsd_kerberos_identity_manager_sign_identity_in_finish (GsdIdentityManager  *self,
                                                       GAsyncResult        *result,
                                                       GError             **error)
{
        GsdIdentity *identity;

        if (g_simple_async_result_propagate_error (G_SIMPLE_ASYNC_RESULT (result),
                                                   error)) {
                return NULL;
        }

        identity = g_simple_async_result_get_op_res_gpointer (G_SIMPLE_ASYNC_RESULT (result));

        return identity;
}

static void
gsd_kerberos_identity_manager_sign_identity_out (GsdIdentityManager   *manager,
                                                 GsdIdentity          *identity,
                                                 GCancellable         *cancellable,
                                                 GAsyncReadyCallback   callback,
                                                 gpointer              user_data)
{
        GsdKerberosIdentityManager *self = GSD_KERBEROS_IDENTITY_MANAGER (manager);
        GSimpleAsyncResult *result;
        Operation *operation;

        result = g_simple_async_result_new (G_OBJECT (self),
                                            callback,
                                            user_data,
                                            gsd_kerberos_identity_manager_sign_identity_out);
        operation = operation_new (self,
                                   cancellable,
                                   OPERATION_TYPE_SIGN_OUT,
                                   result);
        g_object_unref (result);

        operation->identity = g_object_ref (identity);

        g_async_queue_push (self->priv->pending_operations, operation);
}

static void
gsd_kerberos_identity_manager_sign_identity_out_finish (GsdIdentityManager  *self,
                                                        GAsyncResult        *result,
                                                        GError             **error)
{
        if (g_simple_async_result_propagate_error (G_SIMPLE_ASYNC_RESULT (result),
                                                   error)) {
                return;
        }

        return;
}

static char *
gsd_kerberos_identity_manager_name_identity (GsdIdentityManager *manager,
                                             GsdIdentity        *identity)
{
        GsdKerberosIdentityManager *self = GSD_KERBEROS_IDENTITY_MANAGER (manager);
        char *name;
        GList *other_identities;
        gboolean other_identity_needs_rename;

        name = gsd_kerberos_identity_get_realm_name (GSD_KERBEROS_IDENTITY (identity));

        if (name == NULL) {
                return NULL;
        }

        other_identities = g_hash_table_lookup (self->priv->identities_by_realm,
                                                name);

        /* If there was already exactly one identity for this realm before,
         * then it was going by just the realm name, so we need to rename it
         * to use the full principle name
         */
        if (other_identities != NULL &&
            other_identities->next == NULL &&
            other_identities->data != identity) {
                other_identity_needs_rename = TRUE;
        }

        other_identities = g_list_remove (other_identities, identity);
        other_identities = g_list_prepend (other_identities, identity);

        g_hash_table_replace (self->priv->identities_by_realm,
                              g_strdup (name),
                              other_identities);

        if (other_identities->next != NULL) {
                g_free (name);
                name = gsd_kerberos_identity_get_principal_name (GSD_KERBEROS_IDENTITY (identity));
                if (other_identity_needs_rename) {
                        GsdIdentity *other_identity = other_identities->next->data;

                        _gsd_identity_manager_emit_identity_renamed (GSD_IDENTITY_MANAGER (self),
                                                                    other_identity);
                }
        }

        return name;
}

static void
identity_manager_interface_init (GsdIdentityManagerInterface *interface)
{
        interface->sign_identity_in = gsd_kerberos_identity_manager_sign_identity_in;
        interface->sign_identity_in_finish = gsd_kerberos_identity_manager_sign_identity_in_finish;
        interface->sign_identity_out = gsd_kerberos_identity_manager_sign_identity_out;
        interface->sign_identity_out_finish = gsd_kerberos_identity_manager_sign_identity_out_finish;
        interface->renew_identity = gsd_kerberos_identity_manager_renew_identity;
        interface->renew_identity_finish = gsd_kerberos_identity_manager_renew_identity_finish;
        interface->list_identities = gsd_kerberos_identity_manager_list_identities;
        interface->list_identities_finish = gsd_kerberos_identity_manager_list_identities_finish;
        interface->name_identity = gsd_kerberos_identity_manager_name_identity;
}

static void
on_credentials_cache_changed (GFileMonitor               *monitor,
                              GFile                      *file,
                              GFile                      *other_file,
                              GFileMonitorEvent          *event_type,
                              GsdKerberosIdentityManager *self)
{
        schedule_refresh (self);
}

static gboolean
monitor_credentials_cache (GsdKerberosIdentityManager  *self,
                           GError                    **error)
{
        krb5_ccache default_cache;
        const char *cache_type;
        const char *cache_path;
        GFile *file;
        GFileMonitor *monitor;
        krb5_error_code error_code;
        GError *monitoring_error;

        error_code = krb5_cc_default (self->priv->kerberos_context,
                                      &default_cache);

        if (error_code != 0) {
                const char *error_message;
                error_message = krb5_get_error_message (self->priv->kerberos_context, error_code);

                g_set_error_literal (error,
                                     GSD_IDENTITY_MANAGER_ERROR,
                                     GSD_IDENTITY_MANAGER_ERROR_MONITORING,
                                     error_message);
                krb5_free_error_message (self->priv->kerberos_context, error_message);

                return FALSE;
        }

        cache_type = krb5_cc_get_type (self->priv->kerberos_context,
                                       default_cache);
        g_assert (cache_type != NULL);

        if (strcmp (cache_type, "FILE") != 0 &&
            strcmp (cache_type, "DIR") != 0) {
                g_set_error (error,
                             GSD_IDENTITY_MANAGER_ERROR,
                             GSD_IDENTITY_MANAGER_ERROR_MONITORING,
                             "Only 'FILE' and 'DIR' credential cache types are really supported, not '%s'",
                             cache_type);
                return FALSE;
        }

        g_free (self->priv->credentials_cache_type);
        self->priv->credentials_cache_type = g_strdup (cache_type);

        /* If we're using a FILE type credential cache, then the
         * default cache file is the only cache we care about,
         * and its path is what we want to monitor.
         *
         * If we're using a DIR type credential cache, then the default
         * cache file is one of many possible cache files, all in the
         * same directory.  We want to monitor that directory.
         */
        cache_path = krb5_cc_get_name (self->priv->kerberos_context,
                                       default_cache);

        /* The cache name might have a : in front of it.
         * FIXME: figure out if that behavior is by design, or some
         * odd bug.
         */
        if (cache_path[0] == ':') {
                cache_path++;
        }

        file = g_file_new_for_path (cache_path);

        monitoring_error = NULL;
        if (strcmp (cache_type, "FILE") == 0) {
                monitor = g_file_monitor_file (file,
                                               G_FILE_MONITOR_NONE,
                                               NULL,
                                               &monitoring_error);
        } else if (strcmp (cache_type, "DIR") == 0) {
                GFile *directory;

                directory = g_file_get_parent (file);
                monitor = g_file_monitor_directory (directory,
                                                    G_FILE_MONITOR_NONE,
                                                    NULL,
                                                    &monitoring_error);
                g_object_unref (directory);

        } else {
                g_assert_not_reached ();
        }
        g_object_unref (file);

        if (monitor == NULL) {
                g_propagate_error (error, monitoring_error);
                return FALSE;
        }

        self->priv->credentials_cache_changed_signal_id = g_signal_connect (G_OBJECT (monitor),
                                                                            "changed",
                                                                            G_CALLBACK (on_credentials_cache_changed),
                                                                            self);
        self->priv->credentials_cache_monitor = monitor;

        return TRUE;
}

static void
stop_watching_credentials_cache (GsdKerberosIdentityManager *self)
{
        if (!g_file_monitor_is_cancelled (self->priv->credentials_cache_monitor)) {
                g_file_monitor_cancel (self->priv->credentials_cache_monitor);
        }
        g_object_unref (self->priv->credentials_cache_monitor);
        self->priv->credentials_cache_monitor = NULL;
}

static gboolean
gsd_kerberos_identity_manager_initable_init (GInitable     *initable,
                                             GCancellable  *cancellable,
                                             GError       **error)
{
        GsdKerberosIdentityManager *self = GSD_KERBEROS_IDENTITY_MANAGER (initable);
        krb5_error_code error_code;
        GError *monitoring_error;

        if (g_cancellable_set_error_if_cancelled (cancellable, error)) {
                return FALSE;
        }

        error_code = krb5_init_context (&self->priv->kerberos_context);

        if (error_code != 0) {
                const char *error_message;
                error_message = krb5_get_error_message (self->priv->kerberos_context, error_code);

                g_set_error_literal (error,
                                     GSD_IDENTITY_MANAGER_ERROR,
                                     GSD_IDENTITY_MANAGER_ERROR_INITIALIZING,
                                     error_message);
                krb5_free_error_message (self->priv->kerberos_context, error_message);

                return FALSE;
        }

        monitoring_error = NULL;
        if (!monitor_credentials_cache (self, &monitoring_error)) {
                g_warning ("GsdKerberosIdentityManager: Could not monitor credentials: %s",
                           monitoring_error->message);
                g_error_free (monitoring_error);
        }

        schedule_refresh (self);

        return TRUE;
}

static void
initable_interface_init (GInitableIface *interface)
{
        interface->init = gsd_kerberos_identity_manager_initable_init;
}

static void
gsd_kerberos_identity_manager_init (GsdKerberosIdentityManager *self)
{
        self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
                                                  GSD_TYPE_KERBEROS_IDENTITY_MANAGER,
                                                  GsdKerberosIdentityManagerPrivate);
        self->priv->identities = g_hash_table_new_full (g_str_hash,
                                                        g_str_equal,
                                                        (GDestroyNotify)
                                                        g_free,
                                                        (GDestroyNotify)
                                                        g_object_unref);
        self->priv->expired_identities = g_hash_table_new_full (g_str_hash,
                                                                g_str_equal,
                                                                (GDestroyNotify)
                                                                g_free,
                                                                NULL);

        self->priv->identities_by_realm = g_hash_table_new_full (g_str_hash,
                                                                 g_str_equal,
                                                                (GDestroyNotify)
                                                                 g_free,
                                                                 NULL);
        self->priv->pending_operations = g_async_queue_new ();

        g_mutex_init (&self->priv->scheduler_job_lock);
        g_cond_init (&self->priv->scheduler_job_unblocked);

        self->priv->scheduler_cancellable = g_cancellable_new ();
        g_io_scheduler_push_job ((GIOSchedulerJobFunc)
                                 on_job_scheduled,
                                 self,
                                 NULL,
                                 G_PRIORITY_DEFAULT,
                                 self->priv->scheduler_cancellable);

}

static void
cancel_pending_operations (GsdKerberosIdentityManager *self)
{
        Operation *operation;

        operation = g_async_queue_try_pop (self->priv->pending_operations);
        while (operation != NULL) {
                if (!g_cancellable_is_cancelled (operation->cancellable)) {
                        g_cancellable_cancel (operation->cancellable);
                }
                operation_free (operation);
                operation = g_async_queue_try_pop (self->priv->pending_operations);
        }
}

static void
gsd_kerberos_identity_manager_dispose (GObject *object)
{
        GsdKerberosIdentityManager *self = GSD_KERBEROS_IDENTITY_MANAGER (object);

        if (self->priv->identities_by_realm != NULL) {
                g_hash_table_unref (self->priv->identities_by_realm);
                self->priv->identities_by_realm = NULL;
        }

        if (self->priv->expired_identities != NULL) {
                g_hash_table_unref (self->priv->expired_identities);
                self->priv->expired_identities = NULL;
        }

        if (self->priv->identities != NULL) {
                g_hash_table_unref (self->priv->identities);
                self->priv->identities = NULL;
        }

        if (self->priv->credentials_cache_monitor != NULL) {
                stop_watching_credentials_cache (self);
        }

        if (self->priv->pending_operations != NULL) {
                cancel_pending_operations (self);
        }

        if (self->priv->scheduler_cancellable != NULL) {
                if (!g_cancellable_is_cancelled (self->priv->scheduler_cancellable)) {
                        g_cancellable_cancel (self->priv->scheduler_cancellable);
                }

                g_clear_object (&self->priv->scheduler_cancellable);
        }

        /* Note, other thread may still be holding a local reference to queue
         * while it shuts down from cancelled scheduler_cancellable above
         */
        if (self->priv->pending_operations != NULL) {
                g_async_queue_unref (self->priv->pending_operations);
                self->priv->pending_operations = NULL;
        }

        G_OBJECT_CLASS (gsd_kerberos_identity_manager_parent_class)->dispose (object);
}

static void
gsd_kerberos_identity_manager_finalize (GObject *object)
{
        GsdKerberosIdentityManager *self = GSD_KERBEROS_IDENTITY_MANAGER (object);

        g_free (self->priv->credentials_cache_type);

        g_cond_clear (&self->priv->scheduler_job_unblocked);
        krb5_free_context (self->priv->kerberos_context);

        G_OBJECT_CLASS (gsd_kerberos_identity_manager_parent_class)->finalize (object);
}

static void
gsd_kerberos_identity_manager_class_init (GsdKerberosIdentityManagerClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);

        object_class->dispose = gsd_kerberos_identity_manager_dispose;
        object_class->finalize = gsd_kerberos_identity_manager_finalize;

        g_type_class_add_private (klass, sizeof (GsdKerberosIdentityManagerPrivate));
}

GsdIdentityManager *
gsd_kerberos_identity_manager_new (void)
{
        GObject *object;
        GError *error;
        object = g_object_new (GSD_TYPE_KERBEROS_IDENTITY_MANAGER, NULL);

        error = NULL;
        if (!g_initable_init (G_INITABLE (object), NULL, &error)) {
                g_warning ("Could not create kerberos identity manager: %s",
                           error->message);
                g_error_free (error);
                g_object_unref (object);
                return NULL;
        }

        return GSD_IDENTITY_MANAGER (object);
}

static void
test_on_identity_renewed (GsdIdentityManager  *manager,
                          GAsyncResult        *result)
{
        GError *error;

        error = NULL;
        gsd_identity_manager_renew_identity_finish (manager,
                                                    result,
                                                    &error);

        if (error != NULL) {
                g_warning ("Could not renew identity: %s",
                         error->message);
                g_error_free (error);
                return;
        }

        g_message ("identity renewed");
}

static void
test_on_identity_needs_renewal (GsdIdentityManager *identity_manager,
                                GsdIdentity        *identity)
{
        g_message ("identity needs renewal");
        gsd_identity_manager_renew_identity (identity_manager,
                                             identity,
                                             NULL,
                                             (GAsyncReadyCallback)
                                             test_on_identity_renewed,
                                             NULL);
}

static void
test_on_identity_inquiry (GsdIdentityInquiry *inquiry,
                          GCancellable       *cancellable)
{
        GsdIdentityInquiryIter iter;
        GsdIdentityQuery *query;
        char *name, *banner;
        int fd;

        name = gsd_identity_inquiry_get_name (inquiry);
        g_message ("name: %s", name);
        g_free (name);

        banner = gsd_identity_inquiry_get_banner (inquiry);
        g_message ("banner: %s", banner);
        g_free (banner);

        fd =  g_open ("/dev/tty", O_RDWR);
        gsd_identity_inquiry_iter_init (&iter, inquiry);
        while ((query = gsd_identity_inquiry_iter_next (&iter, inquiry)) != NULL) {
                char *prompt;
                char  answer[256] = "";
                ssize_t bytes_read;

                prompt = gsd_identity_query_get_prompt (inquiry, query);
                g_message ("prompt: %s", prompt);
                g_free (prompt);

                bytes_read = read (fd, answer, sizeof (answer) - 1);

                if (bytes_read > 0) {
                        /* Trim off \n */
                        answer[bytes_read - 1] = '\0';
                        g_message ("using password '%s'", answer);
                        gsd_identity_inquiry_answer_query (inquiry, query, answer);
                }
        }
        close (fd);
}

static void
test_on_identity_signed_in (GsdIdentityManager  *manager,
                            GAsyncResult        *result,
                            const char          *principal_name)
{
        GError *error;

        error = NULL;
        gsd_identity_manager_sign_identity_in_finish (manager,
                                                      result,
                                                      &error);

        if (error != NULL) {
                g_warning ("Could not sign-in identity %s: %s",
                           principal_name, error->message);
                g_error_free (error);
                return;
        }

        g_message ("identity %s signed in", principal_name);
}

static void
test_on_identity_expiring (GsdIdentityManager *identity_manager,
                           GsdIdentity        *identity)
{
        const char *identifier;

        g_message ("identity about to expire");

        identifier = gsd_identity_get_identifier (identity);

        gsd_kerberos_identity_manager_sign_identity_in (GSD_IDENTITY_MANAGER (identity_manager),
                                                        identifier,
                                                        (GsdIdentityInquiryFunc)
                                                        test_on_identity_inquiry,
                                                        NULL,
                                                        NULL,
                                                        (GAsyncReadyCallback)
                                                        test_on_identity_signed_in,
                                                        (gpointer)
                                                        identifier);
}

static void
test_on_identity_expired (GsdIdentityManager *identity_manager,
                          GsdIdentity        *identity)
{
        const char *identifier;

        g_message ("identity expired");

        identifier = gsd_identity_get_identifier (identity);

        gsd_kerberos_identity_manager_sign_identity_in (GSD_IDENTITY_MANAGER (identity_manager),
                                                        identifier,
                                                        (GsdIdentityInquiryFunc)
                                                        test_on_identity_inquiry,
                                                        NULL,
                                                        NULL,
                                                        (GAsyncReadyCallback)
                                                        test_on_identity_signed_in,
                                                        (gpointer)
                                                        identifier);
}

void
gsd_kerberos_identity_manager_start_test (GsdKerberosIdentityManager  *manager,
                                          GError                     **error)
{
        const char *principal_name;

        g_signal_connect (G_OBJECT (manager),
                          "identity-needs-renewal",
                          G_CALLBACK (test_on_identity_needs_renewal),
                          NULL);
        g_signal_connect (G_OBJECT (manager),
                          "identity-expiring",
                          G_CALLBACK (test_on_identity_expiring),
                          NULL);
        g_signal_connect (G_OBJECT (manager),
                          "identity-expired",
                          G_CALLBACK (test_on_identity_expired),
                          NULL);

        principal_name = g_getenv ("GSD_KERBEROS_IDENTITY_MANAGER_TEST_PRINCIPAL");
        if (principal_name != NULL) {
                GCancellable *cancellable;

                cancellable = g_cancellable_new ();

                gsd_kerberos_identity_manager_sign_identity_in (GSD_IDENTITY_MANAGER (manager),
                                                                principal_name,
                                                                (GsdIdentityInquiryFunc)
                                                                test_on_identity_inquiry,
                                                                NULL,
                                                                cancellable,
                                                                (GAsyncReadyCallback)
                                                                test_on_identity_signed_in,
                                                                (gpointer)
                                                                principal_name);
                g_object_set_data_full (G_OBJECT (manager),
                                        "test-cancellable",
                                        cancellable,
                                        (GDestroyNotify)
                                        g_object_unref);
        }
}

void
gsd_kerberos_identity_manager_stop_test (GsdKerberosIdentityManager  *manager,
                                         GError                     **error)
{
        GCancellable *cancellable;

        g_signal_handlers_disconnect_by_func (G_OBJECT (manager),
                                              G_CALLBACK (test_on_identity_needs_renewal),
                                              NULL);
        g_signal_handlers_disconnect_by_func (G_OBJECT (manager),
                                              G_CALLBACK (test_on_identity_expired),
                                              NULL);

        cancellable = g_object_get_data (G_OBJECT (manager), "test-cancellable");

        if (cancellable != NULL) {
                if (!g_cancellable_is_cancelled (cancellable)) {
                        g_cancellable_cancel (cancellable);
                }

                g_object_set_data (G_OBJECT (manager), "test-cancellable", NULL);
        }
}
