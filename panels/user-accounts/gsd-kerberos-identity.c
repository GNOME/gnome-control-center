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
 *
 * Author: Ray Strode
 */

#include "config.h"

#include "gsd-identity.h"
#include "gsd-kerberos-identity.h"
#include "gsd-alarm.h"

#include <string.h>
#include <glib/gi18n.h>
#include <gio/gio.h>

typedef enum
{
        VERIFICATION_LEVEL_UNVERIFIED,
        VERIFICATION_LEVEL_ERROR,
        VERIFICATION_LEVEL_EXISTS,
        VERIFICATION_LEVEL_SIGNED_IN
} VerificationLevel;

struct _GsdKerberosIdentityPrivate
{
        krb5_context    kerberos_context;
        krb5_ccache     credentials_cache;

        char           *identifier;

        krb5_timestamp  expiration_time;

        GsdAlarm       *expiration_alarm;
        GCancellable   *expiration_alarm_cancellable;

        GsdAlarm       *expiring_alarm;
        GCancellable   *expiring_alarm_cancellable;

        GsdAlarm       *renewal_alarm;
        GCancellable   *renewal_alarm_cancellable;

        VerificationLevel cached_verification_level;
};

enum {
        EXPIRING,
        EXPIRED,
        UNEXPIRED,
        NEEDS_RENEWAL,
        NEEDS_REFRESH,
        NUMBER_OF_SIGNALS,
};

static guint signals[NUMBER_OF_SIGNALS] = { 0 };

static void identity_interface_init (GsdIdentityInterface *interface);
static void initable_interface_init (GInitableIface *interface);
static void reset_alarms (GsdKerberosIdentity *self);
static void clear_alarms (GsdKerberosIdentity *self);

G_DEFINE_TYPE_WITH_CODE (GsdKerberosIdentity,
                         gsd_kerberos_identity,
                         G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE,
                                                initable_interface_init)
                         G_IMPLEMENT_INTERFACE (GSD_TYPE_IDENTITY,
                                                identity_interface_init));

static void
gsd_kerberos_identity_dispose (GObject *object)
{
        GsdKerberosIdentity *self = GSD_KERBEROS_IDENTITY (object);

        clear_alarms (self);

        g_clear_object (&self->priv->renewal_alarm);
        g_clear_object (&self->priv->expiring_alarm);
        g_clear_object (&self->priv->expiration_alarm);
}

static void
gsd_kerberos_identity_finalize (GObject *object)
{
        GsdKerberosIdentity *self = GSD_KERBEROS_IDENTITY (object);

        g_free (self->priv->identifier);

        if (self->priv->credentials_cache != NULL) {
                krb5_cc_close (self->priv->kerberos_context,
                               self->priv->credentials_cache);
        }

        G_OBJECT_CLASS (gsd_kerberos_identity_parent_class)->finalize (object);
}

static void
gsd_kerberos_identity_class_init (GsdKerberosIdentityClass *klass)
{
        GObjectClass *object_class;

        object_class = G_OBJECT_CLASS (klass);

        object_class->dispose = gsd_kerberos_identity_dispose;
        object_class->finalize = gsd_kerberos_identity_finalize;

        g_type_class_add_private (klass, sizeof (GsdKerberosIdentityPrivate));

        signals[EXPIRING] = g_signal_new ("expiring",
                                          G_TYPE_FROM_CLASS (klass),
                                          G_SIGNAL_RUN_LAST,
                                          0,
                                          NULL, NULL, NULL,
                                          G_TYPE_NONE, 0);
        signals[EXPIRED] = g_signal_new ("expired",
                                         G_TYPE_FROM_CLASS (klass),
                                         G_SIGNAL_RUN_LAST,
                                         0,
                                         NULL, NULL, NULL,
                                         G_TYPE_NONE, 0);
        signals[UNEXPIRED] = g_signal_new ("unexpired",
                                           G_TYPE_FROM_CLASS (klass),
                                           G_SIGNAL_RUN_LAST,
                                           0,
                                           NULL, NULL, NULL,
                                           G_TYPE_NONE, 0);
        signals[NEEDS_RENEWAL] = g_signal_new ("needs-renewal",
                                               G_TYPE_FROM_CLASS (klass),
                                               G_SIGNAL_RUN_LAST,
                                               0,
                                               NULL, NULL, NULL,
                                               G_TYPE_NONE, 0);
        signals[NEEDS_REFRESH] = g_signal_new ("needs-refresh",
                                               G_TYPE_FROM_CLASS (klass),
                                               G_SIGNAL_RUN_LAST,
                                               0,
                                               NULL, NULL, NULL,
                                               G_TYPE_NONE, 0);
}

static char *
get_identifier (GsdKerberosIdentity *self)
{
        krb5_principal principal;
        krb5_error_code error_code;
        char *unparsed_name;
        char *identifier;

        if (self->priv->credentials_cache == NULL) {
                return NULL;
        }

        error_code = krb5_cc_get_principal (self->priv->kerberos_context,
                                            self->priv->credentials_cache,
                                            &principal);

        if (error_code != 0) {
                const char *error_message;
                error_message = krb5_get_error_message (self->priv->kerberos_context, error_code);
                g_debug ("GsdKerberosIdentity: Error looking up principal identity in credential cache: %s", error_message);
                krb5_free_error_message (self->priv->kerberos_context, error_message);
                return NULL;
        }

        error_code = krb5_unparse_name_flags (self->priv->kerberos_context,
                                              principal,
                                              0,
                                              &unparsed_name);

        if (error_code != 0) {
                const char *error_message;

                error_message = krb5_get_error_message (self->priv->kerberos_context, error_code);
                g_debug ("GsdKerberosIdentity: Error parsing principal identity name: %s", error_message);
                krb5_free_error_message (self->priv->kerberos_context, error_message);
                return NULL;
        }

        identifier = g_strdup (unparsed_name);
        krb5_free_unparsed_name (self->priv->kerberos_context, unparsed_name);

        return identifier;
}

static void
gsd_kerberos_identity_init (GsdKerberosIdentity *self)
{
        self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
                                                  GSD_TYPE_KERBEROS_IDENTITY,
                                                  GsdKerberosIdentityPrivate);
        self->priv->expiration_alarm = gsd_alarm_new ();
        self->priv->expiring_alarm = gsd_alarm_new ();
        self->priv->renewal_alarm = gsd_alarm_new ();
}

static void
set_error_from_krb5_error_code (GsdKerberosIdentity  *self,
                                GError              **error,
                                gint                  code,
                                krb5_error_code       error_code,
                                const char           *format,
                                ...)
{
        const char *error_message;
        char       *literal_message;
        char       *expanded_format;
        va_list     args;
        char      **chunks;

        error_message = krb5_get_error_message (self->priv->kerberos_context,
                                                error_code);
        chunks = g_strsplit (format, "%k", -1);
        expanded_format = g_strjoinv (error_message , chunks);
        g_strfreev (chunks);
        krb5_free_error_message (self->priv->kerberos_context, error_message);

        va_start (args, format);
        literal_message = g_strdup_vprintf (expanded_format, args);
        va_end (args);

        g_set_error_literal (error,
                             GSD_IDENTITY_ERROR,
                             code,
                             literal_message);
        g_free (literal_message);
}

char *
gsd_kerberos_identity_get_principal_name (GsdKerberosIdentity *self)
{
        krb5_principal principal;
        krb5_error_code error_code;
        char *unparsed_name;
        char *principal_name;
        int flags;

        if (self->priv->identifier == NULL) {
                return NULL;
        }

        error_code = krb5_parse_name (self->priv->kerberos_context,
                                      self->priv->identifier,
                                      &principal);

        if (error_code != 0) {
                const char *error_message;
                error_message = krb5_get_error_message (self->priv->kerberos_context, error_code);
                g_debug ("GsdKerberosIdentity: Error parsing identity %s into kerberos principal: %s",
                         self->priv->identifier,
                         error_message);
                krb5_free_error_message (self->priv->kerberos_context, error_message);
                return NULL;
        }

        flags = KRB5_PRINCIPAL_UNPARSE_DISPLAY;
        error_code = krb5_unparse_name_flags (self->priv->kerberos_context,
                                              principal,
                                              flags,
                                              &unparsed_name);

        if (error_code != 0) {
                const char *error_message;

                error_message = krb5_get_error_message (self->priv->kerberos_context, error_code);
                g_debug ("GsdKerberosIdentity: Error parsing principal identity name: %s", error_message);
                krb5_free_error_message (self->priv->kerberos_context, error_message);
                return NULL;
        }

        principal_name = g_strdup (unparsed_name);
        krb5_free_unparsed_name (self->priv->kerberos_context, unparsed_name);

        return principal_name;
}

char *
gsd_kerberos_identity_get_realm_name (GsdKerberosIdentity *self)
{
        krb5_principal principal;
        krb5_error_code error_code;
        krb5_data *realm;
        char *realm_name;

        if (self->priv->identifier == NULL) {
                return NULL;
        }

        error_code = krb5_parse_name (self->priv->kerberos_context,
                                      self->priv->identifier,
                                      &principal);

        if (error_code != 0) {
                const char *error_message;
                error_message = krb5_get_error_message (self->priv->kerberos_context, error_code);
                g_debug ("GsdKerberosIdentity: Error parsing identity %s into kerberos principal: %s",
                         self->priv->identifier,
                         error_message);
                krb5_free_error_message (self->priv->kerberos_context, error_message);
                return NULL;
        }

        realm = krb5_princ_realm (self->priv->kerberos_context,
                                  principal);
        realm_name = g_strndup (realm->data, realm->length);
        krb5_free_principal (self->priv->kerberos_context, principal);

        return realm_name;
}

static const char *
gsd_kerberos_identity_get_identifier (GsdIdentity *identity)
{
        GsdKerberosIdentity *self = GSD_KERBEROS_IDENTITY (identity);

        return self->priv->identifier;
}

static gboolean
credentials_validate_existence (GsdKerberosIdentity *self,
                                krb5_principal      principal,
                                krb5_creds         *credentials)
{
        /* Checks if default principal associated with the cache has a valid
         * ticket granting ticket in the passed in credentials
         */

        if (krb5_is_config_principal (self->priv->kerberos_context,
                                      credentials->server)) {
                return FALSE;
        }

        /* looking for the krbtgt / REALM pair, so it should be exactly 2 items */
        if (krb5_princ_size (self->priv->kerberos_context,
                             credentials->server) != 2) {
                return FALSE;
        }

        if (!krb5_realm_compare (self->priv->kerberos_context,
                                 credentials->server,
                                 principal)) {
                /* credentials are from some other realm */
                return FALSE;
        }

        if (strncmp (credentials->server->data[0].data,
                     KRB5_TGS_NAME,
                     credentials->server->data[0].length) != 0) {
                /* credentials aren't for ticket granting */
                return FALSE;
        }

        if (credentials->server->data[1].length != principal->realm.length ||
            memcmp (credentials->server->data[1].data,
                    principal->realm.data,
                    principal->realm.length) != 0) {
                /* credentials are for some other realm */
                return FALSE;
        }

        return TRUE;
}

static krb5_timestamp
get_current_time (GsdKerberosIdentity *self)
{
        krb5_timestamp  current_time;
        krb5_error_code error_code;

        error_code = krb5_timeofday (self->priv->kerberos_context,
                                     &current_time);

        if (error_code != 0) {
                const char *error_message;

                error_message = krb5_get_error_message (self->priv->kerberos_context, error_code);
                g_debug ("GsdKerberosIdentity: Error getting current time: %s", error_message);
                krb5_free_error_message (self->priv->kerberos_context, error_message);
                return 0;
        }

        return current_time;
}

static gboolean
credentials_are_expired (GsdKerberosIdentity *self,
                         krb5_creds          *credentials)
{
        krb5_timestamp  current_time;

        current_time = get_current_time (self);

        self->priv->expiration_time = MAX (credentials->times.endtime,
                                           self->priv->expiration_time);

        if (credentials->times.endtime <= current_time) {
                return TRUE;
        }

        return FALSE;
}

static VerificationLevel
verify_identity (GsdKerberosIdentity  *self,
                 GError              **error)
{
        krb5_principal principal;
        krb5_cc_cursor cursor;
        krb5_creds credentials;
        krb5_error_code error_code;
        VerificationLevel verification_level;

        self->priv->expiration_time = 0;

        if (self->priv->credentials_cache == NULL) {
                return VERIFICATION_LEVEL_UNVERIFIED;
        }

        error_code = krb5_cc_get_principal (self->priv->kerberos_context,
                                            self->priv->credentials_cache,
                                            &principal);

        if (error_code != 0) {
                if (error_code == KRB5_CC_END) {
                        return VERIFICATION_LEVEL_UNVERIFIED;
                }

                set_error_from_krb5_error_code (self,
                                                error,
                                                GSD_IDENTITY_ERROR_VERIFYING,
                                                error_code,
                                                _("Could not find identity in credential cache: %k"));
                return VERIFICATION_LEVEL_ERROR;
        }

        error_code = krb5_cc_start_seq_get (self->priv->kerberos_context,
                                            self->priv->credentials_cache,
                                            &cursor);
        if (error_code != 0) {
                set_error_from_krb5_error_code (self,
                                                error,
                                                GSD_IDENTITY_ERROR_VERIFYING,
                                                error_code,
                                                _("Could not find identity credentials in cache: %k"));

                verification_level = VERIFICATION_LEVEL_ERROR;
                goto out;
        }

        verification_level = VERIFICATION_LEVEL_UNVERIFIED;

        error_code = krb5_cc_next_cred (self->priv->kerberos_context,
                                        self->priv->credentials_cache,
                                        &cursor,
                                        &credentials);

        while (error_code == 0) {
                if (credentials_validate_existence (self, principal, &credentials)) {
                        if (!credentials_are_expired (self, &credentials)) {
                                verification_level = VERIFICATION_LEVEL_SIGNED_IN;
                        } else {
                                verification_level = VERIFICATION_LEVEL_EXISTS;
                        }
                }

                error_code = krb5_cc_next_cred (self->priv->kerberos_context,
                                                self->priv->credentials_cache,
                                                &cursor,
                                                &credentials);
        }

        if (error_code != KRB5_CC_END) {
                verification_level = VERIFICATION_LEVEL_ERROR;

                set_error_from_krb5_error_code (self,
                                                error,
                                                GSD_IDENTITY_ERROR_VERIFYING,
                                                error_code,
                                                _("Could not sift through identity credentials in cache: %k"));
                goto out;
        }

        error_code = krb5_cc_end_seq_get (self->priv->kerberos_context,
                                          self->priv->credentials_cache,
                                          &cursor);

        if (error_code != 0) {
                verification_level = VERIFICATION_LEVEL_ERROR;

                set_error_from_krb5_error_code (self,
                                                error,
                                                GSD_IDENTITY_ERROR_VERIFYING,
                                                error_code,
                                                _("Could not finish up sifting through identity credentials in cache: %k"));
                goto out;
        }
out:
        krb5_free_principal (self->priv->kerberos_context, principal);
        return verification_level;
}

static gboolean
gsd_kerberos_identity_is_signed_in (GsdIdentity *identity)
{
        GsdKerberosIdentity *self = GSD_KERBEROS_IDENTITY (identity);
        VerificationLevel verification_level;

        verification_level = verify_identity (self, NULL);

        return verification_level == VERIFICATION_LEVEL_SIGNED_IN;
}

static void
identity_interface_init (GsdIdentityInterface *interface)
{
        interface->get_identifier = gsd_kerberos_identity_get_identifier;
        interface->is_signed_in = gsd_kerberos_identity_is_signed_in;
}

static void
on_expiration_alarm_fired (GsdAlarm            *alarm,
                           GsdKerberosIdentity *self)
{
        g_return_if_fail (GSD_IS_ALARM (alarm));
        g_return_if_fail (GSD_IS_KERBEROS_IDENTITY (self));

        g_debug ("GsdKerberosIdentity: expiration alarm fired for identity %s",
                 gsd_identity_get_identifier (GSD_IDENTITY (self)));
        g_signal_emit (G_OBJECT (self), signals[NEEDS_REFRESH], 0);
}

static void
on_expiration_alarm_rearmed (GsdAlarm            *alarm,
                             GsdKerberosIdentity *self)
{
        g_return_if_fail (GSD_IS_ALARM (alarm));
        g_return_if_fail (GSD_IS_KERBEROS_IDENTITY (self));

        g_debug ("GsdKerberosIdentity: expiration alarm rearmed");
        g_signal_emit (G_OBJECT (self), signals[NEEDS_REFRESH], 0);
}

static void
on_renewal_alarm_rearmed (GsdAlarm            *alarm,
                          GsdKerberosIdentity *self)
{
        g_return_if_fail (GSD_IS_ALARM (alarm));
        g_return_if_fail (GSD_IS_KERBEROS_IDENTITY (self));

        g_debug ("GsdKerberosIdentity: renewal alarm rearmed");
}

static void
on_renewal_alarm_fired (GsdAlarm            *alarm,
                        GsdKerberosIdentity *self)
{
        g_return_if_fail (GSD_IS_ALARM (alarm));
        g_return_if_fail (GSD_IS_KERBEROS_IDENTITY (self));

        g_clear_object (&self->priv->renewal_alarm_cancellable);

        if (self->priv->cached_verification_level == VERIFICATION_LEVEL_SIGNED_IN) {
                g_debug ("GsdKerberosIdentity: renewal alarm fired for signed-in identity");
                g_signal_emit (G_OBJECT (self), signals[NEEDS_RENEWAL], 0);
        }
}

static void
on_expiring_alarm_rearmed (GsdAlarm            *alarm,
                           GsdKerberosIdentity *self)
{
        g_return_if_fail (GSD_IS_ALARM (alarm));
        g_return_if_fail (GSD_IS_KERBEROS_IDENTITY (self));

        g_debug ("GsdKerberosIdentity: expiring alarm rearmed");
}

static void
on_expiring_alarm_fired (GsdAlarm            *alarm,
                         GsdKerberosIdentity *self)
{
        g_return_if_fail (GSD_IS_ALARM (alarm));
        g_return_if_fail (GSD_IS_KERBEROS_IDENTITY (self));

        g_clear_object (&self->priv->expiring_alarm_cancellable);

        if (self->priv->cached_verification_level == VERIFICATION_LEVEL_SIGNED_IN) {
                g_debug ("GsdKerberosIdentity: expiring alarm fired for signed-in identity");
                g_signal_emit (G_OBJECT (self), signals[EXPIRING], 0);
        }
}

static void
set_alarm (GsdKerberosIdentity  *self,
           GsdAlarm             *alarm,
           GDateTime            *alarm_time,
           GCancellable        **cancellable)
{
        GDateTime *old_alarm_time;

        old_alarm_time = gsd_alarm_get_time (alarm);
        if (old_alarm_time == NULL ||
            !g_date_time_equal (alarm_time, old_alarm_time)) {
                GCancellable *new_cancellable;

                new_cancellable = g_cancellable_new ();
                gsd_alarm_set_time (alarm,
                                    alarm_time,
                                    new_cancellable);
                g_date_time_unref (alarm_time);

                g_clear_object (cancellable);
                *cancellable = new_cancellable;
        }

}

static void
disconnect_alarm_signals (GsdKerberosIdentity *self)
{
        g_signal_handlers_disconnect_by_func (G_OBJECT (self->priv->renewal_alarm),
                                              G_CALLBACK (on_renewal_alarm_fired),
                                              self);
        g_signal_handlers_disconnect_by_func (G_OBJECT (self->priv->renewal_alarm),
                                              G_CALLBACK (on_renewal_alarm_rearmed),
                                              self);
        g_signal_handlers_disconnect_by_func (G_OBJECT (self->priv->expiring_alarm),
                                              G_CALLBACK (on_expiring_alarm_fired),
                                              self);
        g_signal_handlers_disconnect_by_func (G_OBJECT (self->priv->expiration_alarm),
                                              G_CALLBACK (on_expiration_alarm_rearmed),
                                              self);
        g_signal_handlers_disconnect_by_func (G_OBJECT (self->priv->expiration_alarm),
                                              G_CALLBACK (on_expiration_alarm_fired),
                                              self);
        g_signal_handlers_disconnect_by_func (G_OBJECT (self->priv->expiring_alarm),
                                              G_CALLBACK (on_expiring_alarm_rearmed),
                                              self);
}

static void
connect_alarm_signals (GsdKerberosIdentity *self)
{
        g_signal_connect (G_OBJECT (self->priv->renewal_alarm),
                          "fired",
                          G_CALLBACK (on_renewal_alarm_fired),
                          self);
        g_signal_connect (G_OBJECT (self->priv->renewal_alarm),
                          "rearmed",
                          G_CALLBACK (on_renewal_alarm_rearmed),
                          self);
        g_signal_connect (G_OBJECT (self->priv->expiring_alarm),
                          "fired",
                          G_CALLBACK (on_expiring_alarm_fired),
                          self);
        g_signal_connect (G_OBJECT (self->priv->expiring_alarm),
                          "rearmed",
                          G_CALLBACK (on_expiring_alarm_rearmed),
                          self);
        g_signal_connect (G_OBJECT (self->priv->expiration_alarm),
                          "fired",
                          G_CALLBACK (on_expiration_alarm_fired),
                          self);
        g_signal_connect (G_OBJECT (self->priv->expiration_alarm),
                          "rearmed",
                          G_CALLBACK (on_expiration_alarm_rearmed),
                          self);
}

static void
reset_alarms (GsdKerberosIdentity *self)
{
        GDateTime    *now;
        GDateTime    *expiration_time;
        GDateTime    *expiring_time;
        GDateTime    *renewal_time;
        GTimeSpan     time_span_until_expiration;

        now = g_date_time_new_now_local ();
        expiration_time = g_date_time_new_from_unix_local (self->priv->expiration_time);
        time_span_until_expiration = g_date_time_difference (expiration_time, now);

        /* Let the user reauthenticate 10 min before expiration */
        expiring_time = g_date_time_add_minutes (expiration_time, -10);

        /* Try to quietly auto-renew halfway through so in ideal configurations
         * the ticket is never more than halfway to expired
         */
        renewal_time = g_date_time_add (expiration_time,
                                        - (time_span_until_expiration / 2));

        disconnect_alarm_signals (self);

        set_alarm (self,
                   self->priv->renewal_alarm,
                   renewal_time,
                   &self->priv->renewal_alarm_cancellable);
        set_alarm (self,
                   self->priv->expiring_alarm,
                   expiring_time,
                   &self->priv->expiring_alarm_cancellable);
        set_alarm (self,
                   self->priv->expiration_alarm,
                   expiration_time,
                   &self->priv->expiration_alarm_cancellable);

        connect_alarm_signals (self);
}

static void
cancel_and_clear_cancellable (GCancellable **cancellable)
{
        if (cancellable == NULL) {
                return;
        }

        if (!g_cancellable_is_cancelled (*cancellable)) {
            g_cancellable_cancel (*cancellable);
        }

        g_clear_object (cancellable);
}

static void
clear_alarms (GsdKerberosIdentity *self)
{
        cancel_and_clear_cancellable (&self->priv->renewal_alarm_cancellable);
        cancel_and_clear_cancellable (&self->priv->expiring_alarm_cancellable);
        cancel_and_clear_cancellable (&self->priv->expiration_alarm_cancellable);
}

static gboolean
gsd_kerberos_identity_initable_init (GInitable      *initable,
                                     GCancellable   *cancellable,
                                     GError        **error)
{
        GsdKerberosIdentity *self = GSD_KERBEROS_IDENTITY (initable);
        GError *verification_error;

        if (g_cancellable_set_error_if_cancelled (cancellable, error)) {
                return FALSE;
        }

        if (self->priv->identifier == NULL) {
                self->priv->identifier = get_identifier (self);
        }

        verification_error = NULL;
        self->priv->cached_verification_level = verify_identity (self, &verification_error);

        switch (self->priv->cached_verification_level) {
                case VERIFICATION_LEVEL_EXISTS:
                case VERIFICATION_LEVEL_SIGNED_IN:
                        reset_alarms (self);
                        return TRUE;

                case VERIFICATION_LEVEL_UNVERIFIED:
                        return TRUE;

                case VERIFICATION_LEVEL_ERROR:
                        g_propagate_error (error, verification_error);
                        return FALSE;
                default:
                        g_set_error (error,
                                     GSD_IDENTITY_ERROR,
                                     GSD_IDENTITY_ERROR_VERIFYING,
                                     _("No associated identification found"));
                    return FALSE;

        }
}

static void
initable_interface_init (GInitableIface *interface)
{
        interface->init = gsd_kerberos_identity_initable_init;
}

typedef struct
{
        GsdKerberosIdentity    *identity;
        GsdIdentityInquiryFunc  inquiry_func;
        gpointer                inquiry_data;
        GDestroyNotify          destroy_notify;
        GCancellable           *cancellable;
} SignInOperation;

static krb5_error_code
on_kerberos_inquiry (krb5_context     kerberos_context,
                     SignInOperation *operation,
                     const char      *name,
                     const char      *banner,
                     int              number_of_prompts,
                     krb5_prompt      prompts[])
{
        GsdIdentityInquiry *inquiry;
        krb5_error_code     error_code;

        inquiry = gsd_kerberos_identity_inquiry_new (operation->identity,
                                                     name,
                                                     banner,
                                                     prompts,
                                                     number_of_prompts);

        operation->inquiry_func (inquiry,
                                 operation->cancellable,
                                 operation->inquiry_data);

        if (g_cancellable_is_cancelled (operation->cancellable)) {
                error_code = KRB5_LIBOS_PWDINTR;
        } else if (!gsd_identity_inquiry_is_complete (inquiry)) {
                error_code = KRB5_LIBOS_PWDINTR;
        } else {
                error_code = 0;
        }

        g_object_unref (inquiry);

        return error_code;
}

static gboolean
gsd_kerberos_identity_update_credentials (GsdKerberosIdentity  *self,
                                          krb5_principal        principal,
                                          krb5_creds           *new_credentials,
                                          GError              **error)
{
        krb5_error_code error_code;

        error_code = krb5_cc_initialize (self->priv->kerberos_context,
                                         self->priv->credentials_cache,
                                         principal);
        if (error_code != 0) {
                set_error_from_krb5_error_code (self,
                                                error,
                                                GSD_IDENTITY_ERROR_SIGNING_IN,
                                                error_code,
                                                _("Could not initialize credentials cache: %k"));

                krb5_free_cred_contents (self->priv->kerberos_context, new_credentials);
                goto out;
        }

        error_code = krb5_cc_store_cred (self->priv->kerberos_context,
                                         self->priv->credentials_cache,
                                         new_credentials);

        if (error_code != 0) {
                set_error_from_krb5_error_code (self,
                                                error,
                                                GSD_IDENTITY_ERROR_RENEWING,
                                                error_code,
                                                _("Could not store new credentials in credentials cache: %k"));

                krb5_free_cred_contents (self->priv->kerberos_context, new_credentials);
                goto out;
        }
        krb5_free_cred_contents (self->priv->kerberos_context, new_credentials);

        return TRUE;
out:
        return FALSE;
}

static SignInOperation *
sign_in_operation_new (GsdKerberosIdentity    *identity,
                       GsdIdentityInquiryFunc  inquiry_func,
                       gpointer                inquiry_data,
                       GDestroyNotify          destroy_notify,
                       GCancellable           *cancellable)
{
        SignInOperation *operation;

        operation = g_slice_new0 (SignInOperation);
        operation->identity = g_object_ref (identity);
        operation->inquiry_func = inquiry_func;
        operation->inquiry_data = inquiry_data;
        operation->destroy_notify = destroy_notify;

        if (cancellable == NULL) {
                operation->cancellable = g_cancellable_new ();
        } else {
                operation->cancellable = g_object_ref (cancellable);
        }

        return operation;
}

static void
sign_in_operation_free (SignInOperation *operation)
{
        g_object_unref (operation->identity);
        g_object_unref (operation->cancellable);

        g_slice_free (SignInOperation, operation);
}

gboolean
gsd_kerberos_identity_sign_in (GsdKerberosIdentity     *self,
                               const char              *principal_name,
                               GsdIdentityInquiryFunc   inquiry_func,
                               gpointer                 inquiry_data,
                               GDestroyNotify           destroy_notify,
                               GCancellable            *cancellable,
                               GError                 **error)
{
        SignInOperation         *operation;
        krb5_principal           principal;
        krb5_error_code          error_code;
        krb5_creds               new_credentials;
        krb5_get_init_creds_opt *options;
        krb5_deltat              start_time;
        char                    *service_name;
        char                    *password;
        gboolean                 signed_in;

        if (g_cancellable_set_error_if_cancelled (cancellable, error)) {
                return FALSE;
        }

        error_code = krb5_get_init_creds_opt_alloc (self->priv->kerberos_context,
                                                    &options);
        if (error_code != 0) {
                set_error_from_krb5_error_code (self,
                                                error,
                                                GSD_IDENTITY_ERROR_SIGNING_IN,
                                                error_code,
                                                "%k");
                if (destroy_notify) {
                        destroy_notify (inquiry_data);
                }
                return FALSE;
        }

        signed_in = FALSE;

        operation = sign_in_operation_new (self,
                                           inquiry_func,
                                           inquiry_data,
                                           destroy_notify,
                                           cancellable);

        if (g_strcmp0 (self->priv->identifier, principal_name) != 0) {
                g_free (self->priv->identifier);
                self->priv->identifier = g_strdup (principal_name);
        }

        error_code = krb5_parse_name (self->priv->kerberos_context,
                                      principal_name,
                                      &principal);

        if (error_code != 0) {
                set_error_from_krb5_error_code (self,
                                                error,
                                                GSD_IDENTITY_ERROR_SIGNING_IN,
                                                error_code,
                                                "%k");
                if (destroy_notify) {
                        destroy_notify (inquiry_data);
                }
                return FALSE;
        }

        /* FIXME: get from keyring if so configured */
        password = NULL;

        krb5_get_init_creds_opt_set_forwardable (options, TRUE);
        krb5_get_init_creds_opt_set_proxiable (options, TRUE);
        krb5_get_init_creds_opt_set_renew_life (options, G_MAXINT);

        start_time = 0;
        service_name = NULL;
        error_code = krb5_get_init_creds_password (self->priv->kerberos_context,
                                                   &new_credentials,
                                                   principal,
                                                   password,
                                                   (krb5_prompter_fct)
                                                   on_kerberos_inquiry,
                                                   operation,
                                                   start_time,
                                                   service_name,
                                                   options);
        if (error_code != 0) {
                set_error_from_krb5_error_code (self,
                                                error,
                                                GSD_IDENTITY_ERROR_SIGNING_IN,
                                                error_code,
                                                "%k");
                if (destroy_notify) {
                        destroy_notify (inquiry_data);
                }
                sign_in_operation_free (operation);

                krb5_free_principal (self->priv->kerberos_context, principal);
                goto done;
        }

        if (destroy_notify) {
                destroy_notify (inquiry_data);
        }
        sign_in_operation_free (operation);

        if (!gsd_kerberos_identity_update_credentials (self,
                                                       principal,
                                                       &new_credentials,
                                                       error)) {
                krb5_free_principal (self->priv->kerberos_context, principal);
                goto done;
        }
        krb5_free_principal (self->priv->kerberos_context, principal);

        g_debug ("GsdKerberosIdentity: identity signed in");
        signed_in = TRUE;
done:

        return signed_in;
}

void
gsd_kerberos_identity_update (GsdKerberosIdentity *self,
                              GsdKerberosIdentity *new_identity)
{
        char *new_identifier;
        VerificationLevel verification_level;

        if (self->priv->credentials_cache != NULL) {
                krb5_cc_close (self->priv->kerberos_context, self->priv->credentials_cache);
        }

        krb5_cc_dup (new_identity->priv->kerberos_context,
                     new_identity->priv->credentials_cache,
                     &self->priv->credentials_cache);

        new_identifier = get_identifier (self);
        if (g_strcmp0 (self->priv->identifier, new_identifier) != 0) {
                g_free (self->priv->identifier);
                self->priv->identifier = new_identifier;
        } else {
                g_free (new_identifier);
        }

        verification_level = verify_identity (self, NULL);

        if (verification_level == VERIFICATION_LEVEL_SIGNED_IN) {
                reset_alarms (self);
        } else {
                clear_alarms (self);
        }

        if (verification_level != self->priv->cached_verification_level) {
                if (self->priv->cached_verification_level == VERIFICATION_LEVEL_SIGNED_IN &&
                    verification_level == VERIFICATION_LEVEL_EXISTS) {

                        self->priv->cached_verification_level = verification_level;
                        g_signal_emit (G_OBJECT (self), signals[EXPIRED], 0);
                } if (self->priv->cached_verification_level == VERIFICATION_LEVEL_EXISTS &&
                      verification_level == VERIFICATION_LEVEL_SIGNED_IN) {

                        self->priv->cached_verification_level = verification_level;
                        g_signal_emit (G_OBJECT (self), signals[UNEXPIRED], 0);
                }
        }
}

gboolean
gsd_kerberos_identity_renew (GsdKerberosIdentity  *self,
                             GError              **error)
{
        krb5_error_code error_code = 0;
        krb5_principal principal;
        krb5_creds new_credentials;
        gboolean renewed = FALSE;
        char *name = NULL;

        if (self->priv->credentials_cache == NULL) {
                g_set_error (error,
                             GSD_IDENTITY_ERROR,
                             GSD_IDENTITY_ERROR_RENEWING,
                             _("Could not renew identitys: Not signed in"));
                goto out;
        }

        error_code = krb5_cc_get_principal (self->priv->kerberos_context,
                                            self->priv->credentials_cache,
                                            &principal);

        if (error_code != 0) {
                set_error_from_krb5_error_code (self,
                                                error,
                                                GSD_IDENTITY_ERROR_RENEWING,
                                                error_code,
                                                _("Could not renew identity: %k"));
                goto out;
        }

        name = gsd_kerberos_identity_get_principal_name (self);

        error_code = krb5_get_renewed_creds (self->priv->kerberos_context,
                                             &new_credentials,
                                             principal,
                                             self->priv->credentials_cache,
                                             NULL);
        if (error_code != 0) {
                set_error_from_krb5_error_code (self,
                                                error,
                                                GSD_IDENTITY_ERROR_RENEWING,
                                                error_code,
                                                _("Could not get new credentials to renew identity %s: %k"),
                                                name);
                krb5_free_principal (self->priv->kerberos_context, principal);
                goto out;
        }

        if (!gsd_kerberos_identity_update_credentials (self,
                                                       principal,
                                                       &new_credentials,
                                                       error)) {
                krb5_free_principal (self->priv->kerberos_context, principal);
                goto out;
        }

        g_debug ("GsdKerberosIdentity: identity %s renewed", name);
        renewed = TRUE;
out:
        g_free (name);

        return renewed;
}

gboolean
gsd_kerberos_identity_erase  (GsdKerberosIdentity  *self,
                              GError              **error)
{
        krb5_error_code error_code = 0;

        if (self->priv->credentials_cache != NULL) {
                error_code = krb5_cc_destroy (self->priv->kerberos_context,
                                              self->priv->credentials_cache);
                self->priv->credentials_cache = NULL;
        }

        if (error_code != 0) {
                set_error_from_krb5_error_code (self,
                                                error,
                                                GSD_IDENTITY_ERROR_ERASING,
                                                error_code,
                                                _("Could not erase identity: %k"));
                return FALSE;
        }

        return TRUE;
}

GsdIdentity *
gsd_kerberos_identity_new (krb5_context context,
                           krb5_ccache  cache)
{
        GsdKerberosIdentity *self;
        GError *error;

        self = GSD_KERBEROS_IDENTITY (g_object_new (GSD_TYPE_KERBEROS_IDENTITY, NULL));

        krb5_cc_dup (context,
                     cache,
                     &self->priv->credentials_cache);
        self->priv->kerberos_context = context;

        error = NULL;
        if (!g_initable_init (G_INITABLE (self), NULL, &error)) {
                const char *name;

                name = krb5_cc_get_name (context,
                                         cache);
                g_debug ("Could not build identity%s%s: %s",
                         name != NULL? " from credentials cache " : "",
                         name != NULL? name : "",
                         error->message);
                g_error_free (error);
                g_object_unref (self);
                return NULL;
        }

        return GSD_IDENTITY (self);
}
