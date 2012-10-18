/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright 2009-2010  Red Hat, Inc,
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

#include <glib.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include "um-account-dialog.h"
#include "um-realm-manager.h"
#include "um-user-manager.h"
#include "um-utils.h"

typedef enum {
        UM_LOCAL,
        UM_ENTERPRISE,
        NUM_MODES
} UmAccountMode;

static void   mode_change          (UmAccountDialog *self,
                                    UmAccountMode mode);

static void   dialog_validate      (UmAccountDialog *self);

static void   on_join_login        (GObject *source,
                                    GAsyncResult *result,
                                    gpointer user_data);

static void   on_realm_joined      (GObject *source,
                                    GAsyncResult *result,
                                    gpointer user_data);

#define UM_ACCOUNT_DIALOG_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), UM_TYPE_ACCOUNT_DIALOG, \
                                                                    UmAccountDialogClass))
#define UM_IS_ACCOUNT_DIALOG_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), UM_TYPE_ACCOUNT_DIALOG))
#define UM_ACCOUNT_DIALOG_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), UM_TYPE_ACCOUNT_DIALOG, \
                                                                      UmAccountDialogClass))

struct _UmAccountDialog {
        GtkDialog parent;
        GtkWidget *container_widget;
        GSimpleAsyncResult *async;
        GCancellable *cancellable;
        GtkSpinner *spinner;

        /* Buttons to switch modes between local/enterprise */
        UmAccountMode mode;
        GtkWidget *mode_container;
        gboolean mode_updating;
        GtkWidget *mode_buttons[NUM_MODES];
        GtkWidget *mode_areas[NUM_MODES];

        /* Local user account widgets */
        GtkWidget *local_username;
        GtkWidget *local_name;
        GtkWidget *local_account_type;

        /* Enterprise widgets */
        guint realmd_watch;
        GtkWidget *enterprise_button;
        GtkListStore *enterprise_realms;
        GtkComboBox *enterprise_domain;
        GtkEntry *enterprise_domain_entry;
        gboolean enterprise_domain_chosen;
        GtkEntry *enterprise_login;
        GtkEntry *enterprise_password;
        UmRealmManager *realm_manager;
        UmRealmObject *selected_realm;

        /* Join credential dialog */
        GtkDialog *join_dialog;
        GtkLabel *join_domain;
        GtkEntry *join_name;
        GtkEntry *join_password;
        gboolean join_prompted;
};

struct _UmAccountDialogClass {
        GtkDialogClass parent_class;
};

G_DEFINE_TYPE (UmAccountDialog, um_account_dialog, GTK_TYPE_DIALOG);

static void
show_error_dialog (UmAccountDialog *self,
                   const gchar *message,
                   GError *error)
{
        GtkWidget *dialog;

        dialog = gtk_message_dialog_new (GTK_WINDOW (self),
                                         GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                         GTK_MESSAGE_ERROR,
                                         GTK_BUTTONS_CLOSE,
                                         "%s", message);

        if (error != NULL) {
                g_dbus_error_strip_remote_error (error);
                gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
                                                          "%s", error->message);
        }

        g_signal_connect (dialog, "response", G_CALLBACK (gtk_widget_destroy), NULL);
        gtk_window_present (GTK_WINDOW (dialog));
}

static void
begin_action (UmAccountDialog *self)
{
        g_debug ("Beginning action, disabling dialog controls");

        gtk_widget_set_sensitive (self->container_widget, FALSE);
        gtk_dialog_set_response_sensitive (GTK_DIALOG (self), GTK_RESPONSE_OK, FALSE);

        gtk_widget_show (GTK_WIDGET (self->spinner));
        gtk_spinner_start (self->spinner);
}

static void
finish_action (UmAccountDialog *self)
{
        g_debug ("Completed action, enabling dialog controls");

        gtk_widget_set_sensitive (self->container_widget, TRUE);
        gtk_dialog_set_response_sensitive (GTK_DIALOG (self), GTK_RESPONSE_OK, TRUE);

        gtk_widget_hide (GTK_WIDGET (self->spinner));
        gtk_spinner_stop (self->spinner);
}

static void
complete_dialog (UmAccountDialog *self,
                 UmUser *user)
{
        if (user != NULL) {
                g_simple_async_result_set_op_res_gpointer (self->async,
                                                           g_object_ref (user),
                                                           g_object_unref);
        }

        g_simple_async_result_complete_in_idle (self->async);
        gtk_widget_hide (GTK_WIDGET (self));
}

static void
create_user_done (UmUserManager   *manager,
                  GAsyncResult    *res,
                  UmAccountDialog *self)
{
        UmUser *user;
        GError *error;

        finish_action (self);

        /* Note that user is returned without an extra reference */

        error = NULL;
        if (!um_user_manager_create_user_finish (manager, res, &user, &error)) {
                g_debug ("Failed to create user: %s", error->message);
                if (!g_error_matches (error, UM_USER_MANAGER_ERROR, UM_USER_MANAGER_ERROR_PERMISSION_DENIED))
                       show_error_dialog (self, _("Failed to add account"), error);
                g_error_free (error);
                gtk_widget_grab_focus (self->local_name);
        } else {
                g_debug ("Created user: %s", um_user_get_user_name (user));
                complete_dialog (self, user);
        }
}

static void
local_create_user (UmAccountDialog *self)
{
        UmUserManager *manager;
        const gchar *username;
        const gchar *name;
        gint account_type;
        GtkTreeModel *model;
        GtkTreeIter iter;

        begin_action (self);

        name = gtk_entry_get_text (GTK_ENTRY (self->local_name));
        username = gtk_combo_box_text_get_active_text (GTK_COMBO_BOX_TEXT (self->local_username));
        model = gtk_combo_box_get_model (GTK_COMBO_BOX (self->local_account_type));
        gtk_combo_box_get_active_iter (GTK_COMBO_BOX (self->local_account_type), &iter);
        gtk_tree_model_get (model, &iter, 1, &account_type, -1);

        g_debug ("Creating local user: %s", username);

        manager = um_user_manager_ref_default ();
        um_user_manager_create_user (manager,
                                     username,
                                     name,
                                     account_type,
                                     self->cancellable,
                                     (GAsyncReadyCallback)create_user_done,
                                     self,
                                     NULL);
        g_object_unref (manager);
}

static gboolean
local_validate (UmAccountDialog *self)
{
        gboolean valid_login;
        gboolean valid_name;
        GtkWidget *entry;
        const gchar *name;
        gchar *tip;

        name = gtk_combo_box_text_get_active_text (GTK_COMBO_BOX_TEXT (self->local_username));
        valid_login = is_valid_username (name, &tip);

        entry = gtk_bin_get_child (GTK_BIN (self->local_username));
        if (tip) {
                set_entry_validation_error (GTK_ENTRY (entry), tip);
                g_free (tip);
        } else {
                clear_entry_validation_error (GTK_ENTRY (entry));
        }

        name = gtk_entry_get_text (GTK_ENTRY (self->local_name));
        valid_name = is_valid_name (name);

        return valid_name && valid_login;
}

static void
on_username_changed (GtkComboBoxText *combo,
                     gpointer         user_data)
{
        dialog_validate (UM_ACCOUNT_DIALOG (user_data));
}

static void
on_name_changed (GtkEditable *editable,
                 gpointer user_data)
{
        UmAccountDialog *self = UM_ACCOUNT_DIALOG (user_data);
        GtkTreeModel *model;
        const char *name;

        model = gtk_combo_box_get_model (GTK_COMBO_BOX (self->local_username));
        gtk_list_store_clear (GTK_LIST_STORE (model));

        name = gtk_entry_get_text (GTK_ENTRY (editable));
        generate_username_choices (name, GTK_LIST_STORE (model));
        gtk_combo_box_set_active (GTK_COMBO_BOX (self->local_username), 0);

        dialog_validate (self);
}

static void
local_init (UmAccountDialog *self,
            GtkBuilder *builder)
{
        GtkWidget *widget;

        widget = (GtkWidget *) gtk_builder_get_object (builder, "local-username");
        g_signal_connect (widget, "changed",
                          G_CALLBACK (on_username_changed), self);
        self->local_username = widget;

        widget = (GtkWidget *) gtk_builder_get_object (builder, "local-name");
        g_signal_connect (widget, "changed", G_CALLBACK (on_name_changed), self);
        self->local_name = widget;

        widget = (GtkWidget *) gtk_builder_get_object (builder, "local-account-type");
        self->local_account_type = widget;
}

static void
local_prepare (UmAccountDialog *self)
{
        GtkTreeModel *model;

        gtk_entry_set_text (GTK_ENTRY (self->local_name), "");
        gtk_entry_set_text (GTK_ENTRY (gtk_bin_get_child (GTK_BIN (self->local_username))), "");
        model = gtk_combo_box_get_model (GTK_COMBO_BOX (self->local_username));
        gtk_list_store_clear (GTK_LIST_STORE (model));
        gtk_combo_box_set_active (GTK_COMBO_BOX (self->local_account_type), 0);
}

static gboolean
enterprise_validate (UmAccountDialog *self)
{
        const gchar *name;
        gboolean valid_name;
        gboolean valid_domain;
        GtkTreeIter iter;

        name = gtk_entry_get_text (GTK_ENTRY (self->enterprise_login));
        valid_name = is_valid_name (name);

        if (gtk_combo_box_get_active_iter (self->enterprise_domain, &iter)) {
                gtk_tree_model_get (gtk_combo_box_get_model (self->enterprise_domain),
                                    &iter, 0, &name, -1);
        } else {
                name = gtk_entry_get_text (self->enterprise_domain_entry);
        }

        valid_domain = is_valid_name (name);
        return valid_name && valid_domain;
}

static void
enterprise_add_realm (UmAccountDialog *self,
                      UmRealmObject *realm)
{
        GtkTreeModel *model;
        GtkTreeIter iter;
        UmRealmCommon *common;
        const gchar *realm_name;
        gboolean match;
        gboolean ret;
        gchar *name;

        common = um_realm_object_get_common (realm);
        realm_name = um_realm_common_get_name (common);

        /*
         * Don't add a second realm if we already have one with this name.
         * Sometimes realmd returns to realms for the same name, if it has
         * different ways to use that realm. The first one that realmd
         * returns is the one it prefers.
         */

        model = GTK_TREE_MODEL (self->enterprise_realms);
        ret = gtk_tree_model_get_iter_first (model, &iter);
        while (ret) {
                gtk_tree_model_get (model, &iter, 0, &name, -1);
                match = (g_strcmp0 (name, realm_name) == 0);
                g_free (name);
                if (match) {
                        g_debug ("ignoring duplicate realm: %s", realm_name);
                        return;
                }
                ret = gtk_tree_model_iter_next (model, &iter);
        }

        gtk_list_store_append (self->enterprise_realms, &iter);
        gtk_list_store_set (self->enterprise_realms, &iter,
                            0, realm_name,
                            1, realm,
                            -1);

        g_debug ("added realm to drop down: %s %s", realm_name,
                 g_dbus_object_get_object_path (G_DBUS_OBJECT (realm)));

        if (!self->enterprise_domain_chosen && um_realm_is_configured (realm))
                gtk_combo_box_set_active_iter (self->enterprise_domain, &iter);

        g_object_unref (common);
}

static void
on_manager_realm_added (UmRealmManager  *manager,
                        UmRealmObject   *realm,
                        gpointer         user_data)
{
        UmAccountDialog *self = UM_ACCOUNT_DIALOG (user_data);
        enterprise_add_realm (self, realm);
}


static void
on_register_user (GObject *source,
                  GAsyncResult *result,
                  gpointer user_data)
{
        UmAccountDialog *self = UM_ACCOUNT_DIALOG (user_data);
        GError *error = NULL;
        UmUser *user = NULL;

        um_user_manager_cache_user_finish (UM_USER_MANAGER (source),
                                           result, &user, &error);

        /* This is where we're finally done */
        if (error == NULL) {
                g_debug ("Successfully cached remote user: %s", um_user_get_user_name (user));
                finish_action (self);
                complete_dialog (self, user);

        } else {
                show_error_dialog (self, _("Failed to register account"), error);
                g_message ("Couldn't cache user account: %s", error->message);
                finish_action (self);
                g_error_free (error);
        }
}

static void
on_permit_user_login (GObject *source,
                      GAsyncResult *result,
                      gpointer user_data)
{
        UmAccountDialog *self = UM_ACCOUNT_DIALOG (user_data);
        UmRealmCommon *common;
        UmUserManager *manager;
        GError *error = NULL;
        gchar *login;

        common = UM_REALM_COMMON (source);
        um_realm_common_call_change_login_policy_finish (common, result, &error);
        if (error == NULL) {

                /*
                 * Now tell the account service about this user. The account service
                 * should also lookup information about this via the realm and make
                 * sure all that is functional.
                 */
                manager = um_user_manager_ref_default ();
                login = um_realm_calculate_login (common, gtk_entry_get_text (self->enterprise_login));
                g_return_if_fail (login != NULL);

                g_debug ("Caching remote user: %s", login);

                um_user_manager_cache_user (manager, login, self->cancellable,
                                            on_register_user, g_object_ref (self),
                                            g_object_unref);

                g_free (login);
                g_object_unref (manager);

        } else {
                show_error_dialog (self, _("Failed to register account"), error);
                g_message ("Couldn't permit logins on account: %s", error->message);
                finish_action (self);
        }

        g_object_unref (self);
}

static void
enterprise_permit_user_login (UmAccountDialog *self)
{
        UmRealmCommon *common;
        gchar *login;
        const gchar *add[2];
        const gchar *remove[1];
        GVariant *options;

        common = um_realm_object_get_common (self->selected_realm);

        login = um_realm_calculate_login (common, gtk_entry_get_text (self->enterprise_login));
        g_return_if_fail (login != NULL);

        add[0] = login;
        add[1] = NULL;
        remove[0] = NULL;

        g_debug ("Permitting login for: %s", login);
        options = g_variant_new_array (G_VARIANT_TYPE ("{sv}"), NULL, 0);

        um_realm_common_call_change_login_policy (common, "",
                                                  add, remove, options,
                                                  self->cancellable,
                                                  on_permit_user_login,
                                                  g_object_ref (self));

        g_object_unref (common);
        g_free (login);
}

static void
on_join_response (GtkDialog *dialog,
                  gint response,
                  gpointer user_data)
{
        UmAccountDialog *self = UM_ACCOUNT_DIALOG (user_data);

        gtk_widget_hide (GTK_WIDGET (dialog));
        if (response != GTK_RESPONSE_OK) {
                finish_action (self);
                return;
        }

        g_debug ("Logging in as admin user: %s", gtk_entry_get_text (self->join_name));

        /* Prompted for some admin credentials, try to use them to log in */
        um_realm_login (self->selected_realm,
                        gtk_entry_get_text (self->join_name),
                        gtk_entry_get_text (self->join_password),
                        self->cancellable,
                        on_join_login,
                        g_object_ref (self));
}

static void
join_show_prompt (UmAccountDialog *self,
                  GError *error)
{
        UmRealmKerberosMembership *membership;
        UmRealmKerberos *kerberos;
        const gchar *name;

        gtk_entry_set_text (self->join_password, "");
        gtk_widget_grab_focus (GTK_WIDGET (self->join_password));

        kerberos = um_realm_object_get_kerberos (self->selected_realm);
        membership = um_realm_object_get_kerberos_membership (self->selected_realm);

        gtk_label_set_text (self->join_domain,
                            um_realm_kerberos_get_domain_name (kerberos));

        clear_entry_validation_error (self->join_name);
        clear_entry_validation_error (self->join_password);

        if (!self->join_prompted) {
                name = um_realm_kerberos_membership_get_suggested_administrator (membership);
                if (name && !g_str_equal (name, "")) {
                        g_debug ("Suggesting admin user: %s", name);
                        gtk_entry_set_text (self->join_name, name);
                } else {
                        gtk_widget_grab_focus (GTK_WIDGET (self->join_name));
                }

        } else if (g_error_matches (error, UM_REALM_ERROR, UM_REALM_ERROR_BAD_PASSWORD)) {
                g_debug ("Bad admin password: %s", error->message);
                set_entry_validation_error (self->join_password, error->message);

        } else {
                g_debug ("Admin login failure: %s", error->message);
                g_dbus_error_strip_remote_error (error);
                set_entry_validation_error (self->join_name, error->message);
        }

        g_debug ("Showing admin password dialog");
        gtk_window_set_transient_for (GTK_WINDOW (self->join_dialog), GTK_WINDOW (self));
        gtk_window_set_modal (GTK_WINDOW (self->join_dialog), TRUE);
        gtk_window_present (GTK_WINDOW (self->join_dialog));

        self->join_prompted = TRUE;
        g_object_unref (kerberos);
        g_object_unref (membership);

        /* And now we wait for on_join_response() */
}

static void
on_join_login (GObject *source,
               GAsyncResult *result,
               gpointer user_data)
{
        UmAccountDialog *self = UM_ACCOUNT_DIALOG (user_data);
        GError *error = NULL;
        GBytes *creds;

        um_realm_login_finish (result, &creds, &error);

        /* Logged in as admin successfully, use creds to join domain */
        if (error == NULL) {
                if (!um_realm_join_as_admin (self->selected_realm,
                                             gtk_entry_get_text (self->join_name),
                                             gtk_entry_get_text (self->join_password),
                                             creds, self->cancellable, on_realm_joined,
                                             g_object_ref (self))) {
                        show_error_dialog (self, _("No supported way to authenticate with this domain"), NULL);
                        g_message ("Authenticating as admin is not supported by the realm");
                        finish_action (self);
                }

                g_bytes_unref (creds);

        /* Couldn't login as admin, show prompt again */
        } else {
                join_show_prompt (self, error);
                g_message ("Couldn't log in as admin to join domain: %s", error->message);
                g_error_free (error);
        }

        g_object_unref (self);
}

static void
join_init (UmAccountDialog *self,
           GtkBuilder *builder)
{
        self->join_dialog = GTK_DIALOG (gtk_builder_get_object (builder, "join-dialog"));
        self->join_domain = GTK_LABEL (gtk_builder_get_object (builder, "join-domain"));
        self->join_name = GTK_ENTRY (gtk_builder_get_object (builder, "join-name"));
        self->join_password = GTK_ENTRY (gtk_builder_get_object (builder, "join-password"));

        g_signal_connect (self->join_dialog, "response",
                          G_CALLBACK (on_join_response), self);
}

static void
on_realm_joined (GObject *source,
                 GAsyncResult *result,
                 gpointer user_data)
{
        UmAccountDialog *self = UM_ACCOUNT_DIALOG (user_data);
        GError *error = NULL;

        um_realm_join_finish (self->selected_realm,
                              result, &error);

        /* Yay, joined the domain, register the user locally */
        if (error == NULL) {
                g_debug ("Joining realm completed successfully");
                enterprise_permit_user_login (self);

        /* Credential failure while joining domain, prompt for admin creds */
        } else if (g_error_matches (error, UM_REALM_ERROR, UM_REALM_ERROR_BAD_LOGIN) ||
                   g_error_matches (error, UM_REALM_ERROR, UM_REALM_ERROR_BAD_PASSWORD)) {
                g_debug ("Joining realm failed due to credentials");
                join_show_prompt (self, error);

        /* Other failure */
        } else {
                show_error_dialog (self, _("Failed to join domain"), error);
                g_message ("Failed to join the domain: %s", error->message);
                finish_action (self);
                g_error_free (error);
        }

        g_object_unref (self);
}

static void
on_realm_login (GObject *source,
                GAsyncResult *result,
                gpointer user_data)
{
        UmAccountDialog *self = UM_ACCOUNT_DIALOG (user_data);
        GError *error = NULL;
        GBytes *creds;

        um_realm_login_finish (result, &creds, &error);
        if (error == NULL) {

                /* Already joined to the domain, just register this user */
                if (um_realm_is_configured (self->selected_realm)) {
                        g_debug ("Already joined to this realm");
                        enterprise_permit_user_login (self);

                /* Join the domain, try using the user's creds */
                } else if (!um_realm_join_as_user (self->selected_realm,
                                                   gtk_entry_get_text (self->enterprise_login),
                                                   gtk_entry_get_text (self->enterprise_password),
                                                   creds, self->cancellable,
                                                   on_realm_joined,
                                                   g_object_ref (self))) {

                        /* If we can't do user auth, try to authenticate as admin */
                        g_debug ("Cannot join with user credentials");
                        join_show_prompt (self, NULL);
                }

                g_bytes_unref (creds);

        /* A problem with the user's login name or password */
        } else if (g_error_matches (error, UM_REALM_ERROR, UM_REALM_ERROR_BAD_LOGIN)) {
                g_debug ("Problem with the user's login: %s", error->message);
                set_entry_validation_error (self->enterprise_login, error->message);
                finish_action (self);
                gtk_widget_grab_focus (GTK_WIDGET (self->enterprise_login));

        } else if (g_error_matches (error, UM_REALM_ERROR, UM_REALM_ERROR_BAD_PASSWORD)) {
                g_debug ("Problem with the user's password: %s", error->message);
                set_entry_validation_error (self->enterprise_password, error->message);
                finish_action (self);
                gtk_widget_grab_focus (GTK_WIDGET (self->enterprise_password));

        /* Other login failure */
        } else {
                g_dbus_error_strip_remote_error (error);
                show_error_dialog (self, _("Failed to log into domain"), error);
                g_message ("Couldn't log in as user: %s", error->message);
                finish_action (self);
        }

        g_clear_error (&error);
        g_object_unref (self);
}

static void
enterprise_check_login (UmAccountDialog *self)
{
        g_assert (self->selected_realm);

        um_realm_login (self->selected_realm,
                        gtk_entry_get_text (self->enterprise_login),
                        gtk_entry_get_text (self->enterprise_password),
                        self->cancellable,
                        on_realm_login,
                        g_object_ref (self));
}

static void
on_realm_discover_input (GObject *source,
                         GAsyncResult *result,
                         gpointer user_data)
{
        UmAccountDialog *self = UM_ACCOUNT_DIALOG (user_data);
        GError *error = NULL;
        GList *realms;

        realms = um_realm_manager_discover_finish (self->realm_manager,
                                                   result, &error);

        /* Found a realm, log user into domain */
        if (error == NULL) {
                g_assert (realms != NULL);
                self->selected_realm = g_object_ref (realms->data);
                enterprise_check_login (self);
                g_list_free_full (realms, g_object_unref);

        /* The domain is likely invalid*/
        } else {
                finish_action (self);
                g_message ("Couldn't discover domain: %s", error->message);
                gtk_widget_grab_focus (GTK_WIDGET (self->enterprise_domain_entry));
                g_dbus_error_strip_remote_error (error);
                set_entry_validation_error (self->enterprise_domain_entry,
                                            error->message);
                g_error_free (error);
        }

        g_object_unref (self);
}

static void
enterprise_add_user (UmAccountDialog *self)
{
        GtkTreeIter iter;

        begin_action (self);

        g_clear_object (&self->selected_realm);
        self->join_prompted = FALSE;

        /* Already know about this realm, try to login as user */
        if (gtk_combo_box_get_active_iter (self->enterprise_domain, &iter)) {
                gtk_tree_model_get (gtk_combo_box_get_model (self->enterprise_domain),
                                    &iter, 1, &self->selected_realm, -1);
                enterprise_check_login (self);

        /* Something the user typed, we need to discover realm */
        } else {
                um_realm_manager_discover (self->realm_manager,
                                           gtk_entry_get_text (self->enterprise_domain_entry),
                                           self->cancellable,
                                           on_realm_discover_input,
                                           g_object_ref (self));
        }
}

static void
on_realm_manager_created (GObject *source,
                           GAsyncResult *result,
                           gpointer user_data)
{
        UmAccountDialog *self = UM_ACCOUNT_DIALOG (user_data);
        GError *error = NULL;
        GList *realms, *l;

        g_clear_object (&self->realm_manager);

        self->realm_manager = um_realm_manager_new_finish (result, &error);
        if (error != NULL) {
                g_warning ("Couldn't contact realmd service: %s", error->message);
                g_error_free (error);
                return;
        }

        /* Lookup all the realm objects */
        realms = um_realm_manager_get_realms (self->realm_manager);
        for (l = realms; l != NULL; l = g_list_next (l))
                enterprise_add_realm (self, l->data);
        g_list_free (realms);
        g_signal_connect (self->realm_manager, "realm-added",
                          G_CALLBACK (on_manager_realm_added), self);

        /* When no realms try to discover a sensible default, triggers realm-added signal */
        um_realm_manager_discover (self->realm_manager, "", self->cancellable,
                                   NULL, NULL);

        /* Show the 'Enterprise Login' stuff, and update mode */
        gtk_widget_show (self->enterprise_button);
        mode_change (self, self->mode);
}

static void
on_realmd_appeared (GDBusConnection *connection,
                    const gchar *name,
                    const gchar *name_owner,
                    gpointer user_data)
{
        UmAccountDialog *self = UM_ACCOUNT_DIALOG (user_data);
        um_realm_manager_new (self->cancellable, on_realm_manager_created, self);
}

static void
on_realmd_disappeared (GDBusConnection *unused1,
                       const gchar *unused2,
                       gpointer user_data)
{
        UmAccountDialog *self = UM_ACCOUNT_DIALOG (user_data);

        if (self->realm_manager) {
                g_signal_handlers_disconnect_by_func (self->realm_manager,
                                                      on_manager_realm_added,
                                                      self);
                g_object_unref (self->realm_manager);
                self->realm_manager = NULL;
        }

        gtk_list_store_clear (self->enterprise_realms);
        gtk_widget_hide (self->enterprise_button);
        mode_change (self, UM_LOCAL);
}

static void
on_domain_changed (GtkComboBox *widget,
                   gpointer user_data)
{
        UmAccountDialog *self = UM_ACCOUNT_DIALOG (user_data);

        dialog_validate (self);
        self->enterprise_domain_chosen = TRUE;
        clear_entry_validation_error (self->enterprise_domain_entry);
}

static void
on_entry_changed (GtkEditable *editable,
                  gpointer user_data)
{
        UmAccountDialog *self = UM_ACCOUNT_DIALOG (user_data);
        dialog_validate (self);
        clear_entry_validation_error (GTK_ENTRY (editable));
}

static void
enterprise_init (UmAccountDialog *self,
                 GtkBuilder *builder)
{
        GtkWidget *widget;

        self->enterprise_realms = gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_OBJECT);

        widget = (GtkWidget *) gtk_builder_get_object (builder, "enterprise-domain");
        g_signal_connect (widget, "changed", G_CALLBACK (on_domain_changed), self);
        self->enterprise_domain = GTK_COMBO_BOX (widget);
        gtk_combo_box_set_model (self->enterprise_domain,
                                 GTK_TREE_MODEL (self->enterprise_realms));
        gtk_combo_box_set_entry_text_column (self->enterprise_domain, 0);
        self->enterprise_domain_entry = GTK_ENTRY (gtk_bin_get_child (GTK_BIN (widget)));

        widget = (GtkWidget *) gtk_builder_get_object (builder, "enterprise-login");
        g_signal_connect (widget, "changed", G_CALLBACK (on_entry_changed), self);
        self->enterprise_login = GTK_ENTRY (widget);

        widget = (GtkWidget *) gtk_builder_get_object (builder, "enterprise-password");
        g_signal_connect (widget, "changed", G_CALLBACK (on_entry_changed), self);
        self->enterprise_password = GTK_ENTRY (widget);

        /* Initially we hide the 'Enterprise Login' stuff */
        widget = (GtkWidget *) gtk_builder_get_object (builder, "enterprise-button");
        self->enterprise_button = widget;
        gtk_widget_hide (widget);

        self->realmd_watch = g_bus_watch_name (G_BUS_TYPE_SYSTEM, "org.freedesktop.realmd",
                                               G_BUS_NAME_WATCHER_FLAGS_AUTO_START,
                                               on_realmd_appeared, on_realmd_disappeared,
                                               self, NULL);
}

static void
enterprise_prepare (UmAccountDialog *self)
{
        gtk_entry_set_text (GTK_ENTRY (self->enterprise_login), "");
        gtk_entry_set_text (GTK_ENTRY (self->enterprise_password), "");
}

static void
dialog_validate (UmAccountDialog *self)
{
        gboolean valid = FALSE;

        switch (self->mode) {
        case UM_LOCAL:
                valid = local_validate (self);
                break;
        case UM_ENTERPRISE:
                valid = enterprise_validate (self);
                break;
        default:
                valid = FALSE;
                break;
        }

        gtk_dialog_set_response_sensitive (GTK_DIALOG (self), GTK_RESPONSE_OK, valid);
}

static void
label_set_bold (GtkLabel *label,
                gboolean  bold)
{
        PangoAttrList *attrs;
        PangoAttribute *attr;

        attrs = pango_attr_list_new ();
        attr = pango_attr_weight_new (bold ? PANGO_WEIGHT_BOLD : PANGO_WEIGHT_NORMAL);
        pango_attr_list_insert (attrs, attr);
        gtk_label_set_attributes (label, attrs);
        pango_attr_list_unref (attrs);
}


static void
mode_change (UmAccountDialog *self,
             UmAccountMode mode)
{
        GtkWidget *button;
        gint visible_count = 0;
        gboolean active;
        gint i;

        g_assert (!self->mode_updating);
        self->mode_updating = TRUE;

        for (i = 0; i < NUM_MODES; i++) {
                button = self->mode_buttons[i];
                active = (i == (gint)mode);

                /* The toggle state */
                if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button)) != active)
                        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), active);

                /* Make toggled buttons bold */
                label_set_bold (GTK_LABEL (gtk_bin_get_child (GTK_BIN (button))), active);

                /* Show the correct area */
                gtk_widget_set_visible (GTK_WIDGET (self->mode_areas[i]), active);

                if (gtk_widget_get_visible (button))
                        visible_count++;
        }

        /* Show mode container if more than one visible */
        gtk_widget_set_visible (GTK_WIDGET (self->mode_container), visible_count > 1);

        self->mode = mode;
        self->mode_updating = FALSE;
        dialog_validate (self);
}

static void
mode_toggled (UmAccountDialog *self,
              GtkToggleButton *toggle,
              UmAccountMode mode)
{
        if (self->mode_updating)
                return;

        /* Undo the toggle if already pressed */
        if (!gtk_toggle_button_get_active (toggle))
                gtk_toggle_button_set_active (toggle, TRUE);

        /* Otherwise update mode */
        else
                mode_change (self, mode);
}

static void
on_local_toggle (GtkToggleButton *toggle,
                 gpointer user_data)
{
        mode_toggled (UM_ACCOUNT_DIALOG (user_data), toggle, UM_LOCAL);
}

static void
on_enterprise_toggle (GtkToggleButton *toggle,
                      gpointer user_data)
{
        mode_toggled (UM_ACCOUNT_DIALOG (user_data), toggle, UM_ENTERPRISE);
}

static void
mode_init (UmAccountDialog *self,
           GtkBuilder *builder)
{
        GtkWidget *widget;

        self->mode_container = (GtkWidget *) gtk_builder_get_object (builder, "account-mode");

        widget = (GtkWidget *) gtk_builder_get_object (builder, "local-area");
        self->mode_areas[UM_LOCAL] = widget;
        widget = (GtkWidget *) gtk_builder_get_object (builder, "enterprise-area");
        self->mode_areas[UM_ENTERPRISE] = widget;

        widget = (GtkWidget *) gtk_builder_get_object (builder, "local-button");
        g_signal_connect (widget, "toggled", G_CALLBACK (on_local_toggle), self);
        self->mode_buttons[UM_LOCAL] = widget;
        widget = (GtkWidget *) gtk_builder_get_object (builder, "enterprise-button");
        g_signal_connect (widget, "toggled", G_CALLBACK (on_enterprise_toggle), self);
        self->mode_buttons[UM_ENTERPRISE] = widget;
}

static void
um_account_dialog_init (UmAccountDialog *self)
{
        GtkBuilder *builder;
        GtkWidget *widget;
        const gchar *filename;
        GError *error = NULL;
        GtkDialog *dialog;
        GtkWidget *content;
        GtkWidget *actions;
        GtkWidget *box;

        builder = gtk_builder_new ();

        filename = UIDIR "/account-dialog.ui";
        if (!g_file_test (filename, G_FILE_TEST_EXISTS))
                filename = "data/account-dialog.ui";
        if (!gtk_builder_add_from_file (builder, filename, &error)) {
                g_error ("%s", error->message);
                g_error_free (error);
                return;
        }

        dialog = GTK_DIALOG (self);
        actions = gtk_dialog_get_action_area (dialog);
        content = gtk_dialog_get_content_area (dialog);
        gtk_container_set_border_width (GTK_CONTAINER (dialog), 5);
        gtk_window_set_resizable (GTK_WINDOW (dialog), FALSE);
        gtk_window_set_title (GTK_WINDOW (dialog), " ");
        gtk_window_set_icon_name (GTK_WINDOW (dialog), "system-users");

        /* Rearrange the bottom of dialog, so we can have spinner on left */
        g_object_ref (actions);
        box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 10);
        gtk_container_remove (GTK_CONTAINER (content), actions);
        gtk_box_pack_end (GTK_BOX (box), actions, FALSE, TRUE, 0);
        gtk_box_pack_end (GTK_BOX (content), box, TRUE, TRUE, 0);
        gtk_widget_show (box);
        g_object_unref (actions);

        /* Create the spinner, but don't show it yet */
        self->spinner = GTK_SPINNER (gtk_spinner_new ());
        widget = gtk_alignment_new (0.5, 0.5, 1.0, 1.0);
        gtk_alignment_set_padding (GTK_ALIGNMENT (widget), 0, 0, 12, 6);
        gtk_box_pack_start (GTK_BOX (box), widget, FALSE, FALSE, 0);
        gtk_container_add (GTK_CONTAINER (widget), GTK_WIDGET (self->spinner));
        gtk_widget_show (widget);

        gtk_dialog_add_button (dialog, _("Cancel"), GTK_RESPONSE_CANCEL);
        widget = gtk_dialog_add_button (dialog, _("_Add"), GTK_RESPONSE_OK);
        gtk_widget_grab_default (widget);

        widget = (GtkWidget *) gtk_builder_get_object (builder, "account-dialog");
        gtk_container_add (GTK_CONTAINER (content), widget);
        self->container_widget = widget;

        local_init (self, builder);
        enterprise_init (self, builder);
        join_init (self, builder);
        mode_init (self, builder);

        g_object_unref (builder);
}

static void
um_account_dialog_response (GtkDialog *dialog,
                            gint response_id)
{
        UmAccountDialog *self = UM_ACCOUNT_DIALOG (dialog);

        switch (response_id) {
        case GTK_RESPONSE_OK:
                switch (self->mode) {
                case UM_LOCAL:
                        local_create_user (self);
                        break;
                case UM_ENTERPRISE:
                        enterprise_add_user (self);
                        break;
                default:
                        g_assert_not_reached ();
                }
                break;
        case GTK_RESPONSE_CANCEL:
        case GTK_RESPONSE_DELETE_EVENT:
                g_cancellable_cancel (self->cancellable);
                complete_dialog (self, NULL);
                break;
        }
}

static void
um_account_dialog_dispose (GObject *obj)
{
        UmAccountDialog *self = UM_ACCOUNT_DIALOG (obj);

        if (self->cancellable)
                g_cancellable_cancel (self->cancellable);

        if (self->realmd_watch)
                g_bus_unwatch_name (self->realmd_watch);
        self->realmd_watch = 0;

        if (self->realm_manager) {
                g_signal_handlers_disconnect_by_func (self->realm_manager,
                                                      on_manager_realm_added,
                                                      self);
                g_object_unref (self->realm_manager);
                self->realm_manager = NULL;
        }

        G_OBJECT_CLASS (um_account_dialog_parent_class)->dispose (obj);
}

static void
um_account_dialog_finalize (GObject *obj)
{
        UmAccountDialog *self = UM_ACCOUNT_DIALOG (obj);

        if (self->cancellable)
                g_object_unref (self->cancellable);
        g_object_unref (self->enterprise_realms);

        G_OBJECT_CLASS (um_account_dialog_parent_class)->finalize (obj);
}

static void
um_account_dialog_class_init (UmAccountDialogClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);
        GtkDialogClass *dialog_class = GTK_DIALOG_CLASS (klass);

        object_class->dispose = um_account_dialog_dispose;
        object_class->finalize = um_account_dialog_finalize;

        dialog_class->response = um_account_dialog_response;
}

UmAccountDialog *
um_account_dialog_new (void)
{
        return g_object_new (UM_TYPE_ACCOUNT_DIALOG, NULL);
}

void
um_account_dialog_show (UmAccountDialog     *self,
                        GtkWindow           *parent,
                        GAsyncReadyCallback  callback,
                        gpointer             user_data)
{
        g_return_if_fail (UM_IS_ACCOUNT_DIALOG (self));

        /* Make sure not already doing an operation */
        g_return_if_fail (self->async == NULL);

        self->async = g_simple_async_result_new (G_OBJECT (self), callback, user_data,
                                                 um_account_dialog_show);

        if (self->cancellable)
                g_object_unref (self->cancellable);
        self->cancellable = g_cancellable_new ();

        local_prepare (self);
        enterprise_prepare (self);
        mode_change (self, UM_LOCAL);
        dialog_validate (self);

        gtk_window_set_modal (GTK_WINDOW (self), parent != NULL);
        gtk_window_set_transient_for (GTK_WINDOW (self), parent);
        gtk_window_present (GTK_WINDOW (self));
        gtk_widget_grab_focus (self->local_name);
}

UmUser *
um_account_dialog_finish (UmAccountDialog     *self,
                          GAsyncResult        *result)
{
        UmUser *user;

        g_return_val_if_fail (UM_IS_ACCOUNT_DIALOG (self), NULL);
        g_return_val_if_fail (g_simple_async_result_is_valid (result, G_OBJECT (self),
                              um_account_dialog_show), NULL);
        g_return_val_if_fail (result == G_ASYNC_RESULT (self->async), NULL);

        user = g_simple_async_result_get_op_res_gpointer (self->async);
        if (user != NULL)
                g_object_ref (user);

        g_clear_object (&self->async);
        return user;
}
