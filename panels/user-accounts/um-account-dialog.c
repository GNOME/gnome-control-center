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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Written by: Matthias Clasen <mclasen@redhat.com>
 */

#include "config.h"

#include <glib.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <act/act.h>

#include "um-account-dialog.h"
#include "um-realm-manager.h"
#include "um-utils.h"
#include "pw-utils.h"

#define PASSWORD_CHECK_TIMEOUT 600
#define DOMAIN_DEFAULT_HINT _("Should match the web address of your login provider.")

typedef enum {
        UM_LOCAL,
        UM_ENTERPRISE,
        UM_OFFLINE
} UmAccountMode;

static const char * const mode_pages[] = {
  "_local",
  "_enterprise",
  "_offline"
};

static void   mode_change          (UmAccountDialog *self,
                                    UmAccountMode mode);

static void   dialog_validate      (UmAccountDialog *self);

static void   on_join_login        (GObject *source,
                                    GAsyncResult *result,
                                    gpointer user_data);

static void   on_realm_joined      (GObject *source,
                                    GAsyncResult *result,
                                    gpointer user_data);

static void   um_account_dialog_response  (GtkDialog *dialog,
                                           gint response_id);

#define UM_ACCOUNT_DIALOG_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), UM_TYPE_ACCOUNT_DIALOG, \
                                                                    UmAccountDialogClass))
#define UM_IS_ACCOUNT_DIALOG_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), UM_TYPE_ACCOUNT_DIALOG))
#define UM_ACCOUNT_DIALOG_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), UM_TYPE_ACCOUNT_DIALOG, \
                                                                      UmAccountDialogClass))

struct _UmAccountDialog {
        GtkDialog parent;
        GtkWidget *stack;
        GTask *task;
        GCancellable *cancellable;
        GPermission *permission;
        GtkSpinner *spinner;
        UmAccountMode mode;

        /* Local user account widgets */
        GtkWidget *local_username;
        GtkWidget *local_username_entry;
        gboolean   has_custom_username;
        GtkWidget *local_name;
        gint       local_name_timeout_id;
        GtkWidget *local_username_hint;
        gint       local_username_timeout_id;
        GtkWidget *account_type_standard;
        ActUserPasswordMode local_password_mode;
        GtkWidget *local_password_radio;
        GtkWidget *local_password;
        GtkWidget *local_verify;
        gint       local_password_timeout_id;
        GtkWidget *local_strength_indicator;
        GtkWidget *local_hint;
        GtkWidget *local_verify_hint;

        /* Enterprise widgets */
        guint realmd_watch;
        GtkWidget *enterprise_button;
        GtkListStore *enterprise_realms;
        GtkComboBox *enterprise_domain;
        GtkEntry *enterprise_domain_entry;
        GtkEntry *enterprise_login;
        GtkEntry *enterprise_password;
        UmRealmManager *realm_manager;
        UmRealmObject *selected_realm;
        gboolean enterprise_check_credentials;
        GtkWidget *enterprise_hint;
        GtkWidget *enterprise_domain_hint;
        gint       enterprise_domain_timeout_id;
        gboolean   enterprise_domain_chosen;

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
                                         GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_USE_HEADER_BAR,
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

        if (self->enterprise_check_credentials) {
                gtk_widget_set_sensitive (self->stack, FALSE);
        }
        gtk_widget_set_sensitive (self->enterprise_button, FALSE);
        gtk_dialog_set_response_sensitive (GTK_DIALOG (self), GTK_RESPONSE_OK, FALSE);

        gtk_widget_show (GTK_WIDGET (self->spinner));
        gtk_spinner_start (self->spinner);
}

static void
finish_action (UmAccountDialog *self)
{
        g_debug ("Completed domain action");

        if (self->enterprise_check_credentials) {
                gtk_widget_set_sensitive (self->stack, TRUE);
        }
        gtk_widget_set_sensitive (self->enterprise_button, TRUE);
        gtk_dialog_set_response_sensitive (GTK_DIALOG (self), GTK_RESPONSE_OK, TRUE);

        gtk_widget_hide (GTK_WIDGET (self->spinner));
        gtk_spinner_stop (self->spinner);
}

static void
complete_dialog (UmAccountDialog *self,
                 ActUser         *user)
{
        gtk_widget_hide (GTK_WIDGET (self));

        if (user != NULL)
                g_object_ref (user);

        g_task_return_pointer (self->task, user, g_object_unref);
        g_clear_object (&self->task);
}

static void
user_loaded_cb (ActUser         *user,
                GParamSpec      *pspec,
                UmAccountDialog *self)
{
  const gchar *password;

  finish_action (self);

  /* Set a password for the user */
  password = gtk_entry_get_text (GTK_ENTRY (self->local_password));
  act_user_set_password_mode (user, self->local_password_mode);
  if (self->local_password_mode == ACT_USER_PASSWORD_MODE_REGULAR)
        act_user_set_password (user, password, "");

  complete_dialog (self, user);
}

static void
create_user_done (ActUserManager  *manager,
                  GAsyncResult    *res,
                  UmAccountDialog *self)
{
        ActUser *user;
        GError *error;

        /* Note that user is returned without an extra reference */

        error = NULL;
        user = act_user_manager_create_user_finish (manager, res, &error);

        if (user == NULL) {
                finish_action (self);
                g_debug ("Failed to create user: %s", error->message);
                if (!g_error_matches (error, ACT_USER_MANAGER_ERROR, ACT_USER_MANAGER_ERROR_PERMISSION_DENIED))
                       show_error_dialog (self, _("Failed to add account"), error);
                g_error_free (error);
                gtk_widget_grab_focus (self->local_name);
        } else {
                g_debug ("Created user: %s", act_user_get_user_name (user));

                /* Check if the returned object is fully loaded before returning it */
                if (act_user_is_loaded (user))
                        user_loaded_cb (user, NULL, self);
                else
                        g_signal_connect (user, "notify::is-loaded", G_CALLBACK (user_loaded_cb), self);
        }
}

static void
local_create_user (UmAccountDialog *self)
{
        ActUserManager *manager;
        const gchar *username;
        const gchar *name;
        gint account_type;

        begin_action (self);

        name = gtk_entry_get_text (GTK_ENTRY (self->local_name));
        username = gtk_combo_box_text_get_active_text (GTK_COMBO_BOX_TEXT (self->local_username));
        account_type = (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (self->account_type_standard)) ? ACT_USER_ACCOUNT_TYPE_STANDARD : ACT_USER_ACCOUNT_TYPE_ADMINISTRATOR);

        g_debug ("Creating local user: %s", username);

        manager = act_user_manager_get_default ();
        act_user_manager_create_user_async (manager,
                                            username,
                                            name,
                                            account_type,
                                            self->cancellable,
                                            (GAsyncReadyCallback)create_user_done,
                                            self);
}

static gint
update_password_strength (UmAccountDialog *self)
{
        const gchar *password;
        const gchar *username;
        const gchar *hint;
        gint strength_level;

        password = gtk_entry_get_text (GTK_ENTRY (self->local_password));
        username = gtk_combo_box_text_get_active_text (GTK_COMBO_BOX_TEXT (self->local_username));

        pw_strength (password, NULL, username, &hint, &strength_level);

        gtk_label_set_label (GTK_LABEL (self->local_hint), hint);
        gtk_level_bar_set_value (GTK_LEVEL_BAR (self->local_strength_indicator), strength_level);

        if (strength_level > 1) {
                set_entry_validation_checkmark (GTK_ENTRY (self->local_password));
        } else if (strlen (password) == 0) {
                set_entry_generation_icon (GTK_ENTRY (self->local_password));
        } else {
                clear_entry_validation_error (GTK_ENTRY (self->local_password));
        }

        return strength_level;
}

static gboolean
local_validate (UmAccountDialog *self)
{
        gboolean valid_login;
        gboolean valid_name;
        gboolean valid_password;
        GtkWidget *entry;
        const gchar *name;
        const gchar *password;
        const gchar *verify;
        gchar *tip;
        gint strength;

        name = gtk_combo_box_text_get_active_text (GTK_COMBO_BOX_TEXT (self->local_username));
        valid_login = is_valid_username (name, &tip);

        entry = gtk_bin_get_child (GTK_BIN (self->local_username));
        gtk_label_set_label (GTK_LABEL (self->local_username_hint), tip);
        g_free (tip);

        if (valid_login) {
                set_entry_validation_checkmark (GTK_ENTRY (entry));
        }

        name = gtk_entry_get_text (GTK_ENTRY (self->local_name));
        valid_name = is_valid_name (name);
        if (valid_name) {
                set_entry_validation_checkmark (GTK_ENTRY (self->local_name));
        }

        password = gtk_entry_get_text (GTK_ENTRY (self->local_password));
        verify = gtk_entry_get_text (GTK_ENTRY (self->local_verify));
        if (self->local_password_mode == ACT_USER_PASSWORD_MODE_REGULAR) {
                strength = update_password_strength (self);
                valid_password = strength > 1 && strcmp (password, verify) == 0;
        } else {
                valid_password = TRUE;
        }

        return valid_name && valid_login && valid_password;
}

static gboolean
local_username_timeout (UmAccountDialog *self)
{
        self->local_username_timeout_id = 0;

        dialog_validate (self);

        return FALSE;
}

static gboolean
on_username_focus_out (GtkEntry *entry,
                       GParamSpec *pspec,
                       gpointer user_data)
{
        UmAccountDialog *self = UM_ACCOUNT_DIALOG (user_data);

        if (self->local_username_timeout_id != 0) {
                g_source_remove (self->local_username_timeout_id);
                self->local_username_timeout_id = 0;
        }

        local_username_timeout (self);

        return FALSE;
}

static void
on_username_changed (GtkComboBoxText *combo,
                     gpointer         user_data)
{
        UmAccountDialog *self = UM_ACCOUNT_DIALOG (user_data);
        GtkWidget *entry;
        const gchar *username;

        entry = gtk_bin_get_child (GTK_BIN (self->local_username));
        username = gtk_entry_get_text (GTK_ENTRY (entry));
        if (*username == '\0')
                self->has_custom_username = FALSE;
        else if (gtk_widget_has_focus (entry) ||
                 gtk_combo_box_get_active (GTK_COMBO_BOX (self->local_username)) > 0)
                self->has_custom_username = TRUE;

        if (self->local_username_timeout_id != 0) {
                g_source_remove (self->local_username_timeout_id);
                self->local_username_timeout_id = 0;
        }

        clear_entry_validation_error (GTK_ENTRY (entry));
        gtk_dialog_set_response_sensitive (GTK_DIALOG (self), GTK_RESPONSE_OK, FALSE);

        self->local_username_timeout_id = g_timeout_add (PASSWORD_CHECK_TIMEOUT, (GSourceFunc) local_username_timeout, self);
}

static gboolean
local_name_timeout (UmAccountDialog *self)
{
        self->local_name_timeout_id = 0;

        dialog_validate (self);

        return FALSE;
}

static gboolean
on_name_focus_out (GtkEntry *entry,
                       GParamSpec *pspec,
                       gpointer user_data)
{
        UmAccountDialog *self = UM_ACCOUNT_DIALOG (user_data);

        if (self->local_name_timeout_id != 0) {
                g_source_remove (self->local_name_timeout_id);
                self->local_name_timeout_id = 0;
        }

        local_name_timeout (self);

        return FALSE;
}

static void
on_name_changed (GtkEditable *editable,
                 gpointer user_data)
{
        UmAccountDialog *self = UM_ACCOUNT_DIALOG (user_data);
        GtkTreeModel *model;
        const char *name;
        GtkWidget *entry;

        model = gtk_combo_box_get_model (GTK_COMBO_BOX (self->local_username));
        gtk_list_store_clear (GTK_LIST_STORE (model));

        name = gtk_entry_get_text (GTK_ENTRY (editable));
        if ((name == NULL || strlen (name) == 0) && !self->has_custom_username) {
                entry = gtk_bin_get_child (GTK_BIN (self->local_username));
                gtk_entry_set_text (GTK_ENTRY (entry), "");
        } else if (name != NULL && strlen (name) != 0) {
                generate_username_choices (name, GTK_LIST_STORE (model));
                if (!self->has_custom_username)
                        gtk_combo_box_set_active (GTK_COMBO_BOX (self->local_username), 0);
        }

        if (self->local_name_timeout_id != 0) {
                g_source_remove (self->local_name_timeout_id);
                self->local_name_timeout_id = 0;
        }

        clear_entry_validation_error (GTK_ENTRY (editable));
        gtk_dialog_set_response_sensitive (GTK_DIALOG (self), GTK_RESPONSE_OK, FALSE);

        self->local_name_timeout_id = g_timeout_add (PASSWORD_CHECK_TIMEOUT, (GSourceFunc) local_name_timeout, self);
}

static void
update_password_match (UmAccountDialog *self)
{
        const gchar *password;
        const gchar *verify;

        password = gtk_entry_get_text (GTK_ENTRY (self->local_password));
        verify = gtk_entry_get_text (GTK_ENTRY (self->local_verify));
        if (strlen (verify) != 0) {
                if (strcmp (password, verify) != 0) {
                        gtk_label_set_label (GTK_LABEL (self->local_verify_hint), _("Passwords do not match."));
                } else {
                        gtk_label_set_label (GTK_LABEL (self->local_verify_hint), "");
                        set_entry_validation_checkmark (GTK_ENTRY (self->local_verify));
                }
        }
}

static void
on_generate (GtkEntry             *entry,
             GtkEntryIconPosition  pos,
             GdkEventButton       *event,
             UmAccountDialog      *self)
{
        gchar *pwd;

        pwd = pw_generate ();

        gtk_entry_set_text (GTK_ENTRY (self->local_password), pwd);
        gtk_entry_set_text (GTK_ENTRY (self->local_verify), pwd);
        gtk_entry_set_visibility (GTK_ENTRY (self->local_password), TRUE);

        g_free (pwd);
}

static gboolean
local_password_timeout (UmAccountDialog *self)
{
        self->local_password_timeout_id = 0;

        dialog_validate (self);
        update_password_match (self);

        return FALSE;
}

static gboolean
on_password_focus_out (GtkEntry *entry,
                     GParamSpec *pspec,
                     gpointer user_data)
{
        UmAccountDialog *self = UM_ACCOUNT_DIALOG (user_data);

        if (self->local_password_timeout_id != 0) {
                g_source_remove (self->local_password_timeout_id);
                self->local_password_timeout_id = 0;
        }

        local_password_timeout (self);

        return FALSE;
}

static void
on_password_changed (GtkEntry *entry,
                   GParamSpec *pspec,
                   gpointer user_data)
{
        UmAccountDialog *self = UM_ACCOUNT_DIALOG (user_data);
        const char *password;

        if (self->local_password_timeout_id != 0) {
                g_source_remove (self->local_password_timeout_id);
                self->local_password_timeout_id = 0;
        }

        clear_entry_validation_error (GTK_ENTRY (entry));
        clear_entry_validation_error (GTK_ENTRY (self->local_verify));
        gtk_dialog_set_response_sensitive (GTK_DIALOG (self), GTK_RESPONSE_OK, FALSE);

        password = gtk_entry_get_text (GTK_ENTRY (self->local_password));
        if (strlen (password) == 0) {
                gtk_entry_set_visibility (GTK_ENTRY (self->local_password), FALSE);
        }

        self->local_password_timeout_id = g_timeout_add (PASSWORD_CHECK_TIMEOUT, (GSourceFunc) local_password_timeout, self);
}

static void
on_password_radio_changed (GtkRadioButton *radio,
                           gpointer user_data)
{
        UmAccountDialog *self = UM_ACCOUNT_DIALOG (user_data);
        gboolean active;

        active = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (radio));
        self->local_password_mode = active ? ACT_USER_PASSWORD_MODE_REGULAR : ACT_USER_PASSWORD_MODE_SET_AT_LOGIN;

        gtk_widget_set_sensitive (self->local_password, active);
        gtk_widget_set_sensitive (self->local_verify, active);
        gtk_widget_set_sensitive (self->local_strength_indicator, active);
        gtk_widget_set_sensitive (self->local_hint, active);

        dialog_validate (self);
}

static void
local_init (UmAccountDialog *self)
{
        g_signal_connect (self->local_username, "changed",
                          G_CALLBACK (on_username_changed), self);
        g_signal_connect_after (self->local_username, "focus-out-event", G_CALLBACK (on_username_focus_out), self);

        g_signal_connect_swapped (self->local_username_entry, "activate", G_CALLBACK (dialog_validate), self);

        g_signal_connect (self->local_name, "changed", G_CALLBACK (on_name_changed), self);
        g_signal_connect_after (self->local_name, "focus-out-event", G_CALLBACK (on_name_focus_out), self);
        g_signal_connect_swapped (self->local_name, "activate", G_CALLBACK (dialog_validate), self);

        g_signal_connect (self->local_password_radio, "toggled", G_CALLBACK (on_password_radio_changed), self);

        self->local_password_mode = ACT_USER_PASSWORD_MODE_SET_AT_LOGIN;

        gtk_widget_set_sensitive (self->local_password, FALSE);
        g_signal_connect (self->local_password, "notify::text", G_CALLBACK (on_password_changed), self);
        g_signal_connect_after (self->local_password, "focus-out-event", G_CALLBACK (on_password_focus_out), self);
        g_signal_connect_swapped (self->local_password, "activate", G_CALLBACK (dialog_validate), self);
        g_signal_connect (self->local_password, "icon-press", G_CALLBACK (on_generate), self);

        gtk_widget_set_sensitive (self->local_verify, FALSE);
        g_signal_connect (self->local_verify, "notify::text", G_CALLBACK (on_password_changed), self);
        g_signal_connect_after (self->local_verify, "focus-out-event", G_CALLBACK (on_password_focus_out), self);
        g_signal_connect_swapped (self->local_verify, "activate", G_CALLBACK (dialog_validate), self);

        gtk_widget_set_sensitive (self->local_strength_indicator, FALSE);

        gtk_widget_set_sensitive (self->local_hint, FALSE);

        dialog_validate (self);
        update_password_strength (self);
}

static void
local_prepare (UmAccountDialog *self)
{
        GtkTreeModel *model;

        gtk_entry_set_text (GTK_ENTRY (self->local_name), "");
        gtk_entry_set_text (GTK_ENTRY (gtk_bin_get_child (GTK_BIN (self->local_username))), "");
        model = gtk_combo_box_get_model (GTK_COMBO_BOX (self->local_username));
        gtk_list_store_clear (GTK_LIST_STORE (model));
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (self->account_type_standard), TRUE);
        self->has_custom_username = FALSE;
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

        valid_domain = is_valid_name (name) && self->selected_realm != NULL;
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
        g_return_if_fail (common != NULL);

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
                        g_object_unref (common);
                        return;
                }
                ret = gtk_tree_model_iter_next (model, &iter);
        }

        gtk_list_store_append (self->enterprise_realms, &iter);
        gtk_list_store_set (self->enterprise_realms, &iter,
                            0, realm_name,
                            1, realm,
                            -1);

        /* Prefill domain entry by the existing one */
        if (!self->enterprise_domain_chosen && um_realm_is_configured (realm)) {
                gtk_entry_set_text (self->enterprise_domain_entry, realm_name);
        }

        g_debug ("added realm to drop down: %s %s", realm_name,
                 g_dbus_object_get_object_path (G_DBUS_OBJECT (realm)));

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
        ActUser *user;

        if (g_cancellable_is_cancelled (self->cancellable)) {
                g_object_unref (self);
                return;
        }

        user = act_user_manager_cache_user_finish (ACT_USER_MANAGER (source), result, &error);

        /* This is where we're finally done */
        if (user != NULL) {
                g_debug ("Successfully cached remote user: %s", act_user_get_user_name (user));
                finish_action (self);
                complete_dialog (self, user);

        } else {
                show_error_dialog (self, _("Failed to register account"), error);
                g_message ("Couldn't cache user account: %s", error->message);
                finish_action (self);
                g_error_free (error);
        }

        g_object_unref (self);
}

static void
on_permit_user_login (GObject *source,
                      GAsyncResult *result,
                      gpointer user_data)
{
        UmAccountDialog *self = UM_ACCOUNT_DIALOG (user_data);
        UmRealmCommon *common;
        ActUserManager *manager;
        GError *error = NULL;
        gchar *login;

        if (g_cancellable_is_cancelled (self->cancellable)) {
                g_object_unref (self);
                return;
        }

        common = UM_REALM_COMMON (source);
        um_realm_common_call_change_login_policy_finish (common, result, &error);
        if (error == NULL) {

                /*
                 * Now tell the account service about this user. The account service
                 * should also lookup information about this via the realm and make
                 * sure all that is functional.
                 */
                manager = act_user_manager_get_default ();
                login = um_realm_calculate_login (common, gtk_entry_get_text (self->enterprise_login));
                g_return_if_fail (login != NULL);

                g_debug ("Caching remote user: %s", login);

                act_user_manager_cache_user_async (manager, login, self->cancellable,
                                                   on_register_user, g_object_ref (self));

                g_free (login);

        } else {
                show_error_dialog (self, _("Failed to register account"), error);
                g_message ("Couldn't permit logins on account: %s", error->message);
                finish_action (self);
                g_error_free (error);
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
        if (common == NULL) {
                g_debug ("Failed to register account: failed to get d-bus interface");
                show_error_dialog (self, _("Failed to register account"), NULL);
                finish_action (self);
                return;
        }

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

        if (g_cancellable_is_cancelled (self->cancellable)) {
                g_object_unref (self);
                return;
        }

        creds = um_realm_login_finish (result, &error);

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
join_init (UmAccountDialog *self)
{
        GtkBuilder *builder;
        GError *error = NULL;

        builder = gtk_builder_new ();

        if (!gtk_builder_add_from_resource (builder,
                                            "/org/gnome/control-center/user-accounts/join-dialog.ui",
                                            &error)) {
                g_error ("%s", error->message);
                g_error_free (error);
                return;
        }

        self->join_dialog = GTK_DIALOG (gtk_builder_get_object (builder, "join-dialog"));
        self->join_domain = GTK_LABEL (gtk_builder_get_object (builder, "join-domain"));
        self->join_name = GTK_ENTRY (gtk_builder_get_object (builder, "join-name"));
        self->join_password = GTK_ENTRY (gtk_builder_get_object (builder, "join-password"));

        g_signal_connect (self->join_dialog, "response",
                          G_CALLBACK (on_join_response), self);

        g_object_unref (builder);
}

static void
on_realm_joined (GObject *source,
                 GAsyncResult *result,
                 gpointer user_data)
{
        UmAccountDialog *self = UM_ACCOUNT_DIALOG (user_data);
        GError *error = NULL;

        if (g_cancellable_is_cancelled (self->cancellable)) {
                g_object_unref (self);
                return;
        }

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
        }

        g_clear_error (&error);
        g_object_unref (self);
}

static void
on_realm_login (GObject *source,
                GAsyncResult *result,
                gpointer user_data)
{
        UmAccountDialog *self = UM_ACCOUNT_DIALOG (user_data);
        GError *error = NULL;
        GBytes *creds = NULL;
        const gchar *message;

        if (g_cancellable_is_cancelled (self->cancellable)) {
                g_object_unref (self);
                return;
        }

        creds = um_realm_login_finish (result, &error);

        /*
         * User login is valid, but cannot authenticate right now (eg: user needs
         * to change password at next login etc.)
         */
        if (g_error_matches (error, UM_REALM_ERROR, UM_REALM_ERROR_CANNOT_AUTH)) {
                g_clear_error (&error);
                creds = NULL;
        }

        if (error == NULL) {

                /* Already joined to the domain, just register this user */
                if (um_realm_is_configured (self->selected_realm)) {
                        g_debug ("Already joined to this realm");
                        enterprise_permit_user_login (self);

                /* Join the domain, try using the user's creds */
                } else if (creds == NULL ||
                           !um_realm_join_as_user (self->selected_realm,
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
                message = _("That login name didn’t work.\nPlease try again.");
                gtk_label_set_text (GTK_LABEL (self->enterprise_hint), message);
                finish_action (self);
                gtk_widget_grab_focus (GTK_WIDGET (self->enterprise_login));

        } else if (g_error_matches (error, UM_REALM_ERROR, UM_REALM_ERROR_BAD_PASSWORD)) {
                g_debug ("Problem with the user's password: %s", error->message);
                message = _("That login password didn’t work.\nPlease try again.");
                gtk_label_set_text (GTK_LABEL (self->enterprise_hint), message);
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
        gchar *message;

        if (g_cancellable_is_cancelled (self->cancellable)) {
                g_object_unref (self);
                return;
        }

        realms = um_realm_manager_discover_finish (self->realm_manager,
                                                   result, &error);

        /* Found a realm, log user into domain */
        if (error == NULL) {
                g_assert (realms != NULL);
                self->selected_realm = g_object_ref (realms->data);

                if (self->enterprise_check_credentials) {
                        enterprise_check_login (self);
                }
                set_entry_validation_checkmark (GTK_ENTRY (self->enterprise_domain_entry));
                gtk_label_set_text (GTK_LABEL (self->enterprise_domain_hint), DOMAIN_DEFAULT_HINT);
                g_list_free_full (realms, g_object_unref);

        /* The domain is likely invalid*/
        } else {
                g_message ("Couldn't discover domain: %s", error->message);
                g_dbus_error_strip_remote_error (error);

                if (g_error_matches (error, UM_REALM_ERROR, UM_REALM_ERROR_GENERIC)) {
                        message = g_strdup (_("Unable to find the domain. Maybe you misspelled it?"));
                } else {
                        message = g_strdup_printf ("%s.", error->message);
                }
                gtk_label_set_text (GTK_LABEL (self->enterprise_domain_hint), message);

                g_free (message);
                g_error_free (error);

                if (self->enterprise_check_credentials) {
                        finish_action (self);
                        self->enterprise_check_credentials = FALSE;
                }
        }

        if (!self->enterprise_check_credentials) {
                finish_action (self);
                dialog_validate (self);
        }

        g_object_unref (self);
}

static void
enterprise_check_domain (UmAccountDialog *self)
{
        const gchar *domain;

        domain = gtk_entry_get_text (self->enterprise_domain_entry);
        if (strlen (domain) == 0) {
                gtk_label_set_text (GTK_LABEL (self->enterprise_domain_hint), DOMAIN_DEFAULT_HINT);
                return;
        }

        begin_action (self);

        self->join_prompted = FALSE;
        um_realm_manager_discover (self->realm_manager,
                                   domain,
                                   self->cancellable,
                                   on_realm_discover_input,
                                   g_object_ref (self));
}

static void
enterprise_add_user (UmAccountDialog *self)
{
        self->join_prompted = FALSE;
        self->enterprise_check_credentials = TRUE;
        begin_action (self);
        enterprise_check_login (self);

}

static void
clear_realm_manager (UmAccountDialog *self)
{
        if (self->realm_manager) {
                g_signal_handlers_disconnect_by_func (self->realm_manager,
                                                      on_manager_realm_added,
                                                      self);
                g_object_unref (self->realm_manager);
                self->realm_manager = NULL;
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

        clear_realm_manager (self);

        self->realm_manager = um_realm_manager_new_finish (result, &error);
        if (error != NULL) {
                g_warning ("Couldn't contact realmd service: %s", error->message);
                g_object_unref (self);
                g_error_free (error);
                return;
        }

        if (g_cancellable_is_cancelled (self->cancellable)) {
                g_object_unref (self);
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
        g_object_unref (self);
}

static void
on_realmd_appeared (GDBusConnection *connection,
                    const gchar *name,
                    const gchar *name_owner,
                    gpointer user_data)
{
        UmAccountDialog *self = UM_ACCOUNT_DIALOG (user_data);
        um_realm_manager_new (self->cancellable, on_realm_manager_created,
                              g_object_ref (self));
}

static void
on_realmd_disappeared (GDBusConnection *unused1,
                       const gchar *unused2,
                       gpointer user_data)
{
        UmAccountDialog *self = UM_ACCOUNT_DIALOG (user_data);

        clear_realm_manager (self);
        gtk_list_store_clear (self->enterprise_realms);
        gtk_widget_hide (self->enterprise_button);
        mode_change (self, UM_LOCAL);
}

static void
on_network_changed (GNetworkMonitor *monitor,
                    gboolean available,
                    gpointer user_data)
{
        UmAccountDialog *self = UM_ACCOUNT_DIALOG (user_data);

        if (self->mode != UM_LOCAL)
                mode_change (self, UM_ENTERPRISE);
}

static gboolean
enterprise_domain_timeout (UmAccountDialog *self)
{
        GtkTreeIter iter;

        self->enterprise_domain_timeout_id = 0;

        if (gtk_combo_box_get_active_iter (self->enterprise_domain, &iter)) {
                gtk_tree_model_get (gtk_combo_box_get_model (self->enterprise_domain), &iter, 1, &self->selected_realm, -1);
                set_entry_validation_checkmark (GTK_ENTRY (self->enterprise_domain_entry));
                gtk_label_set_text (GTK_LABEL (self->enterprise_domain_hint), DOMAIN_DEFAULT_HINT);
        }
        else {
                enterprise_check_domain (self);
        }

        return FALSE;
}

static void
on_domain_changed (GtkComboBox *widget,
                   gpointer user_data)
{
        UmAccountDialog *self = UM_ACCOUNT_DIALOG (user_data);

        if (self->enterprise_domain_timeout_id != 0) {
                g_source_remove (self->enterprise_domain_timeout_id);
                self->enterprise_domain_timeout_id = 0;
        }

        g_clear_object (&self->selected_realm);
        clear_entry_validation_error (GTK_ENTRY (self->enterprise_domain_entry));
        self->enterprise_domain_timeout_id = g_timeout_add (PASSWORD_CHECK_TIMEOUT, (GSourceFunc) enterprise_domain_timeout, self);
        gtk_dialog_set_response_sensitive (GTK_DIALOG (self), GTK_RESPONSE_OK, FALSE);

        self->enterprise_domain_chosen = TRUE;
        dialog_validate (self);
}

static gboolean
on_enterprise_domain_focus_out (GtkEntry *entry,
                                GParamSpec *pspec,
                                gpointer user_data)
{
        UmAccountDialog *self = UM_ACCOUNT_DIALOG (user_data);

        if (self->enterprise_domain_timeout_id != 0) {
                g_source_remove (self->enterprise_domain_timeout_id);
                self->enterprise_domain_timeout_id = 0;
        }

        if (self->selected_realm == NULL) {
                enterprise_check_domain (self);
        }

        return FALSE;
}

static void
on_entry_changed (GtkEditable *editable,
                  gpointer user_data)
{
        UmAccountDialog *self = UM_ACCOUNT_DIALOG (user_data);

        dialog_validate (self);
        clear_entry_validation_error (GTK_ENTRY (editable));
        clear_entry_validation_error (GTK_ENTRY (self->enterprise_password));
}

static void
enterprise_init (UmAccountDialog *self)
{
        GNetworkMonitor *monitor;

        self->enterprise_realms = gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_OBJECT);
        self->enterprise_check_credentials = FALSE;

        g_signal_connect (self->enterprise_domain, "changed", G_CALLBACK (on_domain_changed), self);
        g_signal_connect_after (self->enterprise_domain, "focus-out-event", G_CALLBACK (on_enterprise_domain_focus_out), self);
        gtk_combo_box_set_model (GTK_COMBO_BOX (self->enterprise_domain),
                                 GTK_TREE_MODEL (self->enterprise_realms));
        self->enterprise_domain_entry = GTK_ENTRY (gtk_bin_get_child (GTK_BIN (self->enterprise_domain)));
        enterprise_check_domain (self);

        g_signal_connect (self->enterprise_login, "changed", G_CALLBACK (on_entry_changed), self);

        g_signal_connect (self->enterprise_password, "changed", G_CALLBACK (on_entry_changed), self);

        /* Initially we hide the 'Enterprise Login' stuff */
        gtk_widget_hide (self->enterprise_button);

        self->realmd_watch = g_bus_watch_name (G_BUS_TYPE_SYSTEM, "org.freedesktop.realmd",
                                               G_BUS_NAME_WATCHER_FLAGS_AUTO_START,
                                               on_realmd_appeared, on_realmd_disappeared,
                                               self, NULL);

        monitor = g_network_monitor_get_default ();
        g_signal_connect_object (monitor, "network-changed", G_CALLBACK (on_network_changed), self, 0);
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
mode_change (UmAccountDialog *self,
             UmAccountMode mode)
{
        gboolean active, available;
        GNetworkMonitor *monitor;

        if (mode != UM_LOCAL) {
                monitor = g_network_monitor_get_default ();
                available = g_network_monitor_get_network_available (monitor);
                mode = available ? UM_ENTERPRISE : UM_OFFLINE;
        }

        gtk_stack_set_visible_child_name (GTK_STACK (self->stack), mode_pages[mode]);

        /* The enterprise toggle state */
        active = (mode != UM_LOCAL);
        if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (self->enterprise_button)) != active)
                gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (self->enterprise_button), active);

        self->mode = mode;
        dialog_validate (self);
}

static void
on_enterprise_toggle (GtkToggleButton *toggle,
                      gpointer user_data)
{
        UmAccountDialog *self = UM_ACCOUNT_DIALOG (user_data);
        UmAccountMode mode;

        mode = gtk_toggle_button_get_active (toggle) ? UM_ENTERPRISE : UM_LOCAL;
        mode_change (self, mode);
}

static void
mode_init (UmAccountDialog *self)
{
        g_signal_connect (self->enterprise_button, "toggled", G_CALLBACK (on_enterprise_toggle), self);
}

static void
um_account_dialog_init (UmAccountDialog *self)
{
        gtk_widget_init_template (GTK_WIDGET (self));

        local_init (self);
        enterprise_init (self);
        join_init (self);
        mode_init (self);
}

static void
on_permission_acquired (GObject *source_object,
                        GAsyncResult *res,
                        gpointer user_data)
{
	UmAccountDialog *self = UM_ACCOUNT_DIALOG (user_data);
	GError *error = NULL;

	/* Paired with begin_action in um_account_dialog_response () */
	finish_action (self);

	if (g_permission_acquire_finish (self->permission, res, &error)) {
		g_return_if_fail (g_permission_get_allowed (self->permission));
		um_account_dialog_response (GTK_DIALOG (self), GTK_RESPONSE_OK);
	} else if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
		g_warning ("Failed to acquire permission: %s", error->message);
	}

	g_clear_error (&error);
	g_object_unref (self);
}

static void
um_account_dialog_response (GtkDialog *dialog,
                            gint response_id)
{
        UmAccountDialog *self = UM_ACCOUNT_DIALOG (dialog);

        switch (response_id) {
        case GTK_RESPONSE_OK:
                /* We don't (or no longer) have necessary permissions */
                if (self->permission && !g_permission_get_allowed (self->permission)) {
                        begin_action (self);
                        g_permission_acquire_async (self->permission, self->cancellable,
                                                    on_permission_acquired, g_object_ref (self));
                        return;
                }

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

        if (self->local_password_timeout_id != 0) {
                g_source_remove (self->local_password_timeout_id);
                self->local_password_timeout_id = 0;
        }

        if (self->local_name_timeout_id != 0) {
                g_source_remove (self->local_name_timeout_id);
                self->local_name_timeout_id = 0;
        }

        if (self->local_username_timeout_id != 0) {
                g_source_remove (self->local_username_timeout_id);
                self->local_username_timeout_id = 0;
        }

        if (self->enterprise_domain_timeout_id != 0) {
                g_source_remove (self->enterprise_domain_timeout_id);
                self->enterprise_domain_timeout_id = 0;
        }

        g_clear_pointer (&self->join_dialog, gtk_widget_destroy);

        G_OBJECT_CLASS (um_account_dialog_parent_class)->dispose (obj);
}

static void
um_account_dialog_finalize (GObject *obj)
{
        UmAccountDialog *self = UM_ACCOUNT_DIALOG (obj);

        if (self->cancellable)
                g_object_unref (self->cancellable);
        g_clear_object (&self->permission);
        g_object_unref (self->enterprise_realms);

        G_OBJECT_CLASS (um_account_dialog_parent_class)->finalize (obj);
}

static void
um_account_dialog_class_init (UmAccountDialogClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);
        GtkDialogClass *dialog_class = GTK_DIALOG_CLASS (klass);
        GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

        object_class->dispose = um_account_dialog_dispose;
        object_class->finalize = um_account_dialog_finalize;

        dialog_class->response = um_account_dialog_response;

        gtk_widget_class_set_template_from_resource (GTK_WIDGET_CLASS (dialog_class),
                                                     "/org/gnome/control-center/user-accounts/account-dialog.ui");

        gtk_widget_class_bind_template_child (widget_class, UmAccountDialog, stack);
        gtk_widget_class_bind_template_child (widget_class, UmAccountDialog, local_username);
        gtk_widget_class_bind_template_child (widget_class, UmAccountDialog, local_username_entry);
        gtk_widget_class_bind_template_child (widget_class, UmAccountDialog, local_name);
        gtk_widget_class_bind_template_child (widget_class, UmAccountDialog, local_username_hint);
        gtk_widget_class_bind_template_child (widget_class, UmAccountDialog, account_type_standard);
        gtk_widget_class_bind_template_child (widget_class, UmAccountDialog, local_password_radio);
        gtk_widget_class_bind_template_child (widget_class, UmAccountDialog, local_password);
        gtk_widget_class_bind_template_child (widget_class, UmAccountDialog, local_verify);
        gtk_widget_class_bind_template_child (widget_class, UmAccountDialog, local_strength_indicator);
        gtk_widget_class_bind_template_child (widget_class, UmAccountDialog, local_hint);
        gtk_widget_class_bind_template_child (widget_class, UmAccountDialog, local_verify_hint);
        gtk_widget_class_bind_template_child (widget_class, UmAccountDialog, enterprise_button);
        gtk_widget_class_bind_template_child (widget_class, UmAccountDialog, spinner);
        gtk_widget_class_bind_template_child (widget_class, UmAccountDialog, enterprise_domain);
        gtk_widget_class_bind_template_child (widget_class, UmAccountDialog, enterprise_login);
        gtk_widget_class_bind_template_child (widget_class, UmAccountDialog, enterprise_password);
        gtk_widget_class_bind_template_child (widget_class, UmAccountDialog, enterprise_domain_hint);
}

UmAccountDialog *
um_account_dialog_new (void)
{
        return g_object_new (UM_TYPE_ACCOUNT_DIALOG, "use-header-bar", TRUE, NULL);
}

void
um_account_dialog_show (UmAccountDialog     *self,
                        GtkWindow           *parent,
                        GPermission         *permission,
                        GAsyncReadyCallback  callback,
                        gpointer             user_data)
{
        g_return_if_fail (UM_IS_ACCOUNT_DIALOG (self));

        /* Make sure not already doing an operation */
        g_return_if_fail (self->task == NULL);

        if (self->cancellable)
                g_object_unref (self->cancellable);
        self->cancellable = g_cancellable_new ();

        self->task = g_task_new (G_OBJECT (self), self->cancellable, callback, user_data);
        g_task_set_source_tag (self->task, um_account_dialog_show);

        g_clear_object (&self->permission);
        self->permission = permission ? g_object_ref (permission) : NULL;

        local_prepare (self);
        enterprise_prepare (self);
        mode_change (self, UM_LOCAL);
        dialog_validate (self);

        gtk_window_set_modal (GTK_WINDOW (self), parent != NULL);
        gtk_window_set_transient_for (GTK_WINDOW (self), parent);
        gtk_window_present (GTK_WINDOW (self));
        gtk_widget_grab_focus (self->local_name);
}

ActUser *
um_account_dialog_finish (UmAccountDialog     *self,
                          GAsyncResult        *result)
{
        g_return_val_if_fail (UM_IS_ACCOUNT_DIALOG (self), NULL);
        g_return_val_if_fail (g_task_is_valid (result, G_OBJECT (self)), NULL);
        g_return_val_if_fail (g_async_result_is_tagged (result, um_account_dialog_show), NULL);
        g_return_val_if_fail (result == G_ASYNC_RESULT (self->task), NULL);

        return g_task_propagate_pointer (self->task, NULL);
}
