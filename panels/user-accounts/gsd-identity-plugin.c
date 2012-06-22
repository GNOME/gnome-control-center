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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 */

#include "config.h"

#include <glib/gi18n-lib.h>
#include <gmodule.h>

#include <glib.h>
#include <glib-object.h>
#include <gio/gio.h>

#include <libnotify/notify.h>
#include <gcr/gcr.h>

#include "gnome-settings-plugin.h"
#include "gsd-identity-plugin.h"
#include "gsd-identity-manager.h"
#include "gsd-kerberos-identity-manager.h"
#include "gsd-kerberos-identity.h"

struct GsdIdentityPluginPrivate {
        GsdIdentityManager *identity_manager;
};

#define GSD_IDENTITY_PLUGIN_GET_PRIVATE(object) (G_TYPE_INSTANCE_GET_PRIVATE ((object), GSD_TYPE_IDENTITY_PLUGIN, GsdIdentityPluginPrivate))

GNOME_SETTINGS_PLUGIN_REGISTER (GsdIdentityPlugin, gsd_identity_plugin);

static void
gsd_identity_plugin_init (GsdIdentityPlugin *self)
{
        self->priv = GSD_IDENTITY_PLUGIN_GET_PRIVATE (self);

        g_debug ("GsdIdentityPlugin initializing");
}

static void
gsd_identity_plugin_finalize (GObject *object)
{
        GsdIdentityPlugin *self;

        g_return_if_fail (object != NULL);
        g_return_if_fail (GSD_IS_IDENTITY_PLUGIN (object));

        g_debug ("GsdIdentityPlugin: finalizing");

        self = GSD_IDENTITY_PLUGIN (object);

        g_return_if_fail (self->priv != NULL);

        g_clear_object (&self->priv->identity_manager);

        G_OBJECT_CLASS (gsd_identity_plugin_parent_class)->finalize (object);
}

static void
on_identity_renewed (GsdIdentityManager  *manager,
                     GAsyncResult        *result,
                     GnomeSettingsPlugin *self)
{
        GError *error;

        error = NULL;
        gsd_identity_manager_renew_identity_finish (manager,
                                                    result,
                                                    &error);

        if (error != NULL) {
                g_debug ("GsdIdentityPlugin: could not renew identity: %s",
                         error->message);
                g_error_free (error);
                return;
        }

        g_debug ("GsdIdentityPlugin: identity renewed");
}

static void
on_identity_needs_renewal (GsdIdentityManager *identity_manager,
                           GsdIdentity        *identity,
                           GsdIdentityPlugin  *self)
{
        g_debug ("GsdIdentityPlugin: identity needs renewal");
        gsd_identity_manager_renew_identity (GSD_IDENTITY_MANAGER (self->priv->identity_manager),
                                             identity,
                                             NULL,
                                             (GAsyncReadyCallback)
                                             on_identity_renewed,
                                             self);
}

static void
on_identity_signed_in (GsdIdentityManager  *manager,
                       GAsyncResult        *result,
                       GnomeSettingsPlugin *self)
{
        GError *error;

        error = NULL;
        gsd_identity_manager_sign_identity_in_finish (manager,
                                                      result,
                                                      &error);

        if (error != NULL) {
                g_debug ("GsdIdentityPlugin: could not sign in identity: %s",
                         error->message);
                g_error_free (error);
                return;
        }

        g_debug ("GsdIdentityPlugin: identity signed in");
}

typedef struct {
        GsdIdentityPlugin  *plugin;
        GsdIdentity        *identity;
        NotifyNotification *notification;
        GCancellable       *cancellable;
        gulong              refreshed_signal_id;
} SignInRequest;

static SignInRequest *
sign_in_request_new (GsdIdentityPlugin  *plugin,
                     GsdIdentity        *identity,
                     NotifyNotification *notification,
                     GCancellable       *cancellable)
{
    SignInRequest *request;

    request = g_slice_new0 (SignInRequest);

    request->plugin = plugin;
    request->identity = g_object_ref (identity);
    request->notification = notification;
    request->cancellable = g_object_ref (cancellable);

    return request;
}

static void
sign_in_request_free (SignInRequest *data)
{
        GsdIdentityPlugin *plugin = data->plugin;

        g_signal_handler_disconnect (plugin->priv->identity_manager,
                                     data->refreshed_signal_id);
        g_object_set_data (G_OBJECT (data->identity), "sign-in-request", NULL);
        g_clear_object (&data->identity);
        g_clear_object (&data->cancellable);
        g_slice_free (SignInRequest, data);
}

typedef struct {
        GsdIdentityPlugin   *plugin;
        GsdIdentity         *identity;
        GsdIdentityInquiry  *inquiry;
        GsdIdentityQuery    *query;
        GcrSystemPrompt     *prompt;
        GCancellable        *cancellable;
} SystemPromptRequest;

static SystemPromptRequest *
system_prompt_request_new (GsdIdentityPlugin  *plugin,
                           GcrSystemPrompt    *prompt,
                           GsdIdentity        *identity,
                           GsdIdentityInquiry *inquiry,
                           GsdIdentityQuery   *query,
                           GCancellable       *cancellable)
{
    SystemPromptRequest *data;

    data = g_slice_new0 (SystemPromptRequest);

    data->plugin = plugin;
    data->prompt = prompt;
    data->identity = g_object_ref (identity);
    data->inquiry = g_object_ref (inquiry);
    data->query = query;
    data->cancellable = g_object_ref (cancellable);

    return data;
}

static void
system_prompt_request_free (SystemPromptRequest *data)
{
        g_clear_object (&data->identity);
        g_clear_object (&data->inquiry);
        g_clear_object (&data->cancellable);
        g_slice_free (SystemPromptRequest, data);
}

static void
close_system_prompt (GsdIdentityManager *manager,
                     GsdIdentity        *identity,
                     SystemPromptRequest          *data)
{
        GError *error;

        /* Only close the prompt if the identity we're
         * waiting on got refreshed
         */
        if (data->identity != identity) {
                return;
        }

        g_signal_handlers_disconnect_by_func (G_OBJECT (manager),
                                              G_CALLBACK (close_system_prompt),
                                              data);
        error = NULL;
        if (!gcr_system_prompt_close (data->prompt,
                                      NULL,
                                      &error)) {
                if (error != NULL) {
                        g_debug ("GsdIdentityPlugin: could not close system prompt: %s",
                                 error->message);
                        g_error_free (error);
                }
        }
}

static void
on_password_system_prompt_answered (GcrPrompt           *prompt,
                                    GAsyncResult        *result,
                                    SystemPromptRequest *request)
{
        GsdIdentityPlugin  *self = request->plugin;
        GsdIdentityInquiry *inquiry = request->inquiry;
        GsdIdentity        *identity = request->identity;
        GsdIdentityQuery   *query = request->query;
        GCancellable       *cancellable = request->cancellable;
        GError             *error;
        const char         *password;

        error = NULL;
        password = gcr_prompt_password_finish (prompt, result, &error);

        if (password == NULL) {
                if (error != NULL) {
                        g_debug ("GsdIdentityPlugin: could not get password from user: %s",
                                 error->message);
                        g_error_free (error);
                } else {
                        g_cancellable_cancel (cancellable);
                }
        } else if (!g_cancellable_is_cancelled (cancellable)) {
                gsd_identity_inquiry_answer_query (inquiry,
                                                   query,
                                                   password);
        }

        close_system_prompt (self->priv->identity_manager, identity, request);
        system_prompt_request_free (request);
}

static void
query_user (GsdIdentityPlugin   *self,
            GsdIdentity         *identity,
            GsdIdentityInquiry  *inquiry,
            GsdIdentityQuery    *query,
            GcrPrompt           *prompt,
            GCancellable        *cancellable)
{
        SystemPromptRequest  *request;
        char                 *prompt_text;
        GsdIdentityQueryMode  query_mode;
        char                 *description;
        char                 *name;

        g_assert (GSD_IS_KERBEROS_IDENTITY (identity));

        gcr_prompt_set_title (prompt, _("Sign In to Realm"));

        name = gsd_identity_manager_name_identity (self->priv->identity_manager,
                                                   identity);

        description = g_strdup_printf (_("The network realm %s needs some information to sign you in."), name);
        g_free (name);

        gcr_prompt_set_description (prompt, description);
        g_free (description);

        prompt_text = gsd_identity_query_get_prompt (inquiry, query);
        gcr_prompt_set_message (prompt, prompt_text);
        g_free (prompt_text);

        request = system_prompt_request_new (self,
                                             GCR_SYSTEM_PROMPT (prompt),
                                             identity,
                                             inquiry,
                                             query,
                                             cancellable);

        g_signal_connect (G_OBJECT (self->priv->identity_manager),
                          "identity-refreshed",
                          G_CALLBACK (close_system_prompt),
                          request);

        query_mode = gsd_identity_query_get_mode (inquiry, query);

        switch (query_mode) {
                case GSD_IDENTITY_QUERY_MODE_INVISIBLE:
                    gcr_prompt_password_async (prompt,
                                               cancellable,
                                               (GAsyncReadyCallback)
                                               on_password_system_prompt_answered,
                                               request);
                    break;
                case GSD_IDENTITY_QUERY_MODE_VISIBLE:
                    gcr_prompt_password_async (prompt,
                                               cancellable,
                                               (GAsyncReadyCallback)
                                               on_password_system_prompt_answered,
                                               request);
                    break;
        }
}

typedef struct {
        GsdIdentityPlugin   *plugin;
        GsdIdentityInquiry  *inquiry;
        GCancellable        *cancellable;
} SystemPromptOpenRequest;

static SystemPromptOpenRequest *
system_prompt_open_request_new (GsdIdentityPlugin  *plugin,
                  GsdIdentityInquiry *inquiry,
                  GCancellable       *cancellable)
{
    SystemPromptOpenRequest *data;

    data = g_slice_new0 (SystemPromptOpenRequest);

    data->plugin = plugin;
    data->inquiry = g_object_ref (inquiry);
    data->cancellable = g_object_ref (cancellable);

    return data;
}

static void
system_prompt_open_request_free (SystemPromptOpenRequest *data)
{
        g_clear_object (&data->inquiry);
        g_clear_object (&data->cancellable);
        g_slice_free (SystemPromptOpenRequest, data);
}

static void
on_system_prompt_open (GcrSystemPrompt         *system_prompt,
                       GAsyncResult            *result,
                       SystemPromptOpenRequest *request)
{
        GsdIdentityPlugin       *self = request->plugin;
        GsdIdentityInquiry      *inquiry = request->inquiry;
        GCancellable            *cancellable = request->cancellable;
        GsdIdentity             *identity;
        GsdIdentityQuery        *query;
        GcrPrompt               *prompt;
        GError                  *error;
        GsdIdentityInquiryIter   iter;

        error = NULL;
        prompt = gcr_system_prompt_open_finish (result, &error);

        if (prompt == NULL) {
                if (error != NULL) {
                        g_debug ("GsdIdentityPlugin: could not open system prompt: %s",
                                 error->message);
                        g_error_free (error);
                }
                return;
        }

        identity = gsd_identity_inquiry_get_identity (inquiry);
        gsd_identity_inquiry_iter_init (&iter, inquiry);
        while ((query = gsd_identity_inquiry_iter_next (&iter, inquiry)) != NULL) {
                query_user (self, identity, inquiry, query, prompt, cancellable);
        }

        system_prompt_open_request_free (request);
}

static void
on_identity_inquiry (GsdIdentityInquiry *inquiry,
                     GCancellable       *cancellable,
                     GsdIdentityPlugin  *self)
{
        SystemPromptOpenRequest *request;

        request = system_prompt_open_request_new (self, inquiry, cancellable);
        gcr_system_prompt_open_async (-1,
                                      cancellable,
                                      (GAsyncReadyCallback)
                                      on_system_prompt_open,
                                      request);
}

static void
on_sign_in_clicked (NotifyNotification *notification,
                    const char         *acition_id,
                    SignInRequest      *request)
{
        GsdIdentityPlugin *self = request->plugin;
        GsdIdentity       *identity = request->identity;
        const char        *identifier;

        identifier = gsd_identity_get_identifier (identity);
        gsd_identity_manager_sign_identity_in (self->priv->identity_manager,
                                               identifier,
                                               (GsdIdentityInquiryFunc)
                                               on_identity_inquiry,
                                               self,
                                               request->cancellable,
                                               (GAsyncReadyCallback)
                                               on_identity_signed_in,
                                               self);
}

static void
close_notification (GCancellable       *cancellable,
                    NotifyNotification *notification)
{
        notify_notification_close (notification, NULL);
}

static void
cancel_sign_in (GsdIdentityManager *identity_manager,
                GsdIdentity        *identity,
                SignInRequest         *data)
{
        if (data->cancellable == NULL) {
                return;
        }

        if (!g_cancellable_is_cancelled (data->cancellable)) {
                g_cancellable_cancel (data->cancellable);
        }

        g_clear_object (&data->cancellable);
}

static void
ask_to_sign_in (GsdIdentityPlugin  *self,
                GsdIdentity        *identity)
{
        NotifyNotification *notification;
        char               *name;
        char               *description;
        SignInRequest      *request;
        GCancellable       *cancellable;

        request = g_object_get_data (G_OBJECT (identity), "sign-in-request");

        if (request != NULL) {
                if (!g_cancellable_is_cancelled (request->cancellable)) {
                        g_cancellable_cancel (request->cancellable);
                }
        }

        g_debug ("GsdIdentityPlugin: asking to sign back in");

        name = gsd_identity_manager_name_identity (self->priv->identity_manager,
                                                   identity);
        if (gsd_identity_is_signed_in (identity)) {
                description = g_strdup_printf (_("The network realm %s will soon be inaccessible."),
                                               name);
        } else {
                description = g_strdup_printf (_("The network realm %s is now inaccessible."),
                                               name);
        }
        g_free (name);

        notification = notify_notification_new (_("Realm Access"),
                                                description,
                                                "dialog-password-symbolic");
        g_free (description);
        notify_notification_set_app_name (notification, _("Network Realm"));

        cancellable = g_cancellable_new ();

        request = sign_in_request_new (self, identity, notification, cancellable);

        g_object_set_data (G_OBJECT (identity), "sign-in-request", request);

        g_cancellable_connect (cancellable,
                               G_CALLBACK (close_notification),
                               notification,
                               NULL);
        g_signal_connect_swapped (G_OBJECT (notification),
                                  "closed",
                                  G_CALLBACK (sign_in_request_free),
                                  request);

        request->refreshed_signal_id = g_signal_connect (G_OBJECT (self->priv->identity_manager),
                                                         "identity-refreshed",
                                                         G_CALLBACK (cancel_sign_in),
                                                         request);

        notify_notification_add_action (notification,
                                        "sign-in",
                                        _("Sign In"),
                                        (NotifyActionCallback)
                                        on_sign_in_clicked,
                                        request,
                                        NULL);

        notify_notification_show (notification, NULL);
}

static void
on_identity_expiring (GsdIdentityManager *identity_manager,
                      GsdIdentity        *identity,
                      GsdIdentityPlugin  *self)
{
        g_debug ("GsdIdentityPlugin: identity expiring");
        ask_to_sign_in (self, identity);
}

static void
on_identity_expired (GsdIdentityManager *identity_manager,
                     GsdIdentity        *identity,
                     GsdIdentityPlugin  *self)
{
        g_debug ("GsdIdentityPlugin: identity expired");
        ask_to_sign_in (self, identity);
}

static void
impl_activate (GnomeSettingsPlugin *plugin)
{
        GsdIdentityPlugin *self = GSD_IDENTITY_PLUGIN (plugin);

        if (self->priv->identity_manager != NULL) {
                g_debug ("GsdIdentityPlugin: Not activating identity plugin, because it's "
                         "already active");
                return;
        }

        g_debug ("GsdIdentityPlugin: Activating identity plugin");
        self->priv->identity_manager = gsd_kerberos_identity_manager_new ();
        g_signal_connect (G_OBJECT (self->priv->identity_manager),
                          "identity-needs-renewal",
                          G_CALLBACK (on_identity_needs_renewal),
                          self);
        g_signal_connect (G_OBJECT (self->priv->identity_manager),
                          "identity-expiring",
                          G_CALLBACK (on_identity_expiring),
                          self);
        g_signal_connect (G_OBJECT (self->priv->identity_manager),
                          "identity-expired",
                          G_CALLBACK (on_identity_expired),
                          self);
}

static void
impl_deactivate (GnomeSettingsPlugin *plugin)
{
        GsdIdentityPlugin *self = GSD_IDENTITY_PLUGIN (plugin);

        if (self->priv->identity_manager == NULL) {
                g_debug ("GsdIdentityPlugin: Not deactivating identity plugin, "
                         "because it's already inactive");
                return;
        }

        g_debug ("GsdIdentityPlugin: Deactivating identity plugin");
        g_signal_handlers_disconnect_by_func (self, on_identity_needs_renewal, self);
        g_clear_object (&self->priv->identity_manager);
}

static void
gsd_identity_plugin_class_init (GsdIdentityPluginClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);
        GnomeSettingsPluginClass *plugin_class = GNOME_SETTINGS_PLUGIN_CLASS (klass);

        object_class->finalize = gsd_identity_plugin_finalize;

        plugin_class->activate = impl_activate;
        plugin_class->deactivate = impl_deactivate;

        g_type_class_add_private (klass, sizeof (GsdIdentityPluginPrivate));
}

static void
gsd_identity_plugin_class_finalize (GsdIdentityPluginClass *klass)
{
}
