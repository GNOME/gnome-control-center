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

#include <adwaita.h>
#include <glib.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <act/act.h>

#include "cc-add-user-dialog.h"
#include "cc-entry-feedback.h"
#include "cc-list-row.h"
#include "user-utils.h"
#include "pw-utils.h"

#define PASSWORD_CHECK_TIMEOUT 600

static void   dialog_validate      (CcAddUserDialog *self);

static void   add_button_clicked_cb (CcAddUserDialog *self);

struct _CcAddUserDialog {
        AdwDialog parent_instance;

        GtkButton          *add_button;
        GtkSwitch          *account_type_switch;
        AdwPreferencesPage *page;
        AdwPasswordEntryRow *password_row;
        GtkLevelBar        *strength_indicator;
        CcEntryFeedback    *password_hint;
        GtkCheckButton     *password_radio;
        AdwEntryRow        *name_row;
        AdwActionRow       *username_row;
        CcEntryFeedback    *verify_password_hint;
        AdwPasswordEntryRow *verify_password_row;
        CcEntryFeedback    *name_feedback;
        AdwNavigationView  *navigation;
        AdwPreferencesPage *password_page;
        GtkWidget          *password_page_add_button;
        AdwSpinner         *spinner;

        GCancellable       *cancellable;
        GPermission        *permission;
        ActUser            *user;

        gboolean            has_custom_username;
        ActUserPasswordMode password_mode;
        gint                password_timeout_id;
        gchar              *password_hint_text;
};

G_DEFINE_TYPE (CcAddUserDialog, cc_add_user_dialog, ADW_TYPE_DIALOG);

static void
show_error_dialog (CcAddUserDialog *self,
                   const gchar     *message,
                   GError          *error)
{
        AdwDialog *dialog;

        dialog = adw_alert_dialog_new (message, NULL);

        if (error != NULL) {
                g_dbus_error_strip_remote_error (error);
                adw_alert_dialog_set_body (ADW_ALERT_DIALOG (dialog), error->message);
        }

        adw_alert_dialog_add_responses (ADW_ALERT_DIALOG (dialog),
                                        "ok",  _("_OK"),
                                        NULL);

        adw_alert_dialog_set_default_response (ADW_ALERT_DIALOG (dialog),
                                               "ok");

        adw_dialog_present (dialog, GTK_WIDGET (self));
}

static void
begin_action (CcAddUserDialog *self)
{
        g_debug ("Beginning action, disabling dialog controls");

        gtk_widget_set_sensitive (GTK_WIDGET (self->add_button), FALSE);

        gtk_widget_set_visible (GTK_WIDGET (self->spinner), TRUE);
}

static void
finish_action (CcAddUserDialog *self)
{
        g_debug ("Completed domain action");

        gtk_widget_set_sensitive (GTK_WIDGET (self->add_button), TRUE);

        gtk_widget_set_visible (GTK_WIDGET (self->spinner), FALSE);
}

static void
user_loaded_cb (CcAddUserDialog *self,
                GParamSpec      *pspec,
                ActUser         *user)
{
  const gchar *password;

  finish_action (self);

  /* Set a password for the user */
  password = gtk_editable_get_text (GTK_EDITABLE (self->password_row));
  act_user_set_password_mode (user, self->password_mode);
  if (self->password_mode == ACT_USER_PASSWORD_MODE_REGULAR)
        act_user_set_password (user, password, "");

  self->user = g_object_ref (user);
  adw_dialog_close (ADW_DIALOG (self));
}

static void
create_user_done (ActUserManager  *manager,
                  GAsyncResult    *res,
                  CcAddUserDialog *self)
{
        ActUser *user;
        g_autoptr(GError) error = NULL;

        /* Note that user is returned without an extra reference */

        user = act_user_manager_create_user_finish (manager, res, &error);

        if (user == NULL) {
                finish_action (self);
                g_debug ("Failed to create user: %s", error->message);
                if (!g_error_matches (error, ACT_USER_MANAGER_ERROR, ACT_USER_MANAGER_ERROR_PERMISSION_DENIED))
                       show_error_dialog (self, _("Failed to add account"), error);
                gtk_widget_grab_focus (GTK_WIDGET (self->name_row));
        } else {
                g_debug ("Created user: %s", act_user_get_user_name (user));

                /* Check if the returned object is fully loaded before returning it */
                if (act_user_is_loaded (user))
                        user_loaded_cb (self, NULL, user);
                else
                        g_signal_connect_object (user, "notify::is-loaded", G_CALLBACK (user_loaded_cb), self, G_CONNECT_SWAPPED);
        }
}

static void
create_user (CcAddUserDialog *self)
{
        ActUserManager *manager;
        const gchar *username;
        const gchar *name;
        gint account_type;

        begin_action (self);

        name = gtk_editable_get_text (GTK_EDITABLE (self->name_row));
        username = gtk_editable_get_text (GTK_EDITABLE (self->username_row));
        account_type = gtk_switch_get_active (self->account_type_switch) ? ACT_USER_ACCOUNT_TYPE_ADMINISTRATOR : ACT_USER_ACCOUNT_TYPE_STANDARD;

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

static gboolean
update_password_strength (CcAddUserDialog *self)
{
        const gchar *password;
        const gchar *username;
        const gchar *hint;
        const gchar *verify;
        gint strength_level;
        gboolean valid, enforcing, accept;

        password = gtk_editable_get_text (GTK_EDITABLE (self->password_row));
        username = gtk_editable_get_text (GTK_EDITABLE (self->username_row));

        if (strlen (password) == 0) {
                cc_entry_feedback_reset (self->password_hint);
                gtk_level_bar_set_value (self->strength_indicator, 0);

                return FALSE;
        }

        pw_strength (password, NULL, username, &hint, &strength_level, &enforcing);
        valid = strength_level > 1;
        accept = valid || !enforcing;

        gtk_level_bar_set_value (self->strength_indicator, strength_level);

        /* Don't re-announce the password hint if it didn't change.
         * In this case we announce the verify-password hint i(if it exists) instead. */
        if (enforcing && g_strcmp0 (hint, self->password_hint_text) != 0) {
                cc_entry_feedback_update (self->password_hint,
                                          valid ? "check-outlined-symbolic" : "dialog-error-symbolic",
                                          hint);
                self->password_hint_text = g_strdup (hint);
        }

        verify = gtk_editable_get_text (GTK_EDITABLE (self->verify_password_row));
        if (strlen (verify) == 0) {
                gtk_widget_set_sensitive (GTK_WIDGET (self->verify_password_row), accept);
        }

        return accept;
}

static gboolean
validate_password (CcAddUserDialog *self)
{
        const gchar *password;
        const gchar *verify;
        gboolean accept;

        password = gtk_editable_get_text (GTK_EDITABLE (self->password_row));
        verify = gtk_editable_get_text (GTK_EDITABLE (self->verify_password_row));
        accept = update_password_strength (self);

        return accept && strcmp (password, verify) == 0;
}

static void dialog_validate_cb (GObject *source_object,
                                GAsyncResult *result,
                                gpointer user_data)
{
        g_autoptr(CcAddUserDialog) self = CC_ADD_USER_DIALOG (user_data);
        g_autoptr(GError) error = NULL;
        g_autofree gchar *tip = NULL;
        g_autofree gchar *username = NULL;
        const gchar *name = NULL;
        gboolean username_valid = FALSE;

        username_valid = is_valid_username_finish (result, &tip, &username, &error);
        if (error != NULL) {
                g_warning ("Could not check username by usermod: %s", error->message);
                username_valid = TRUE;
        }

        name = gtk_editable_get_text (GTK_EDITABLE (self->name_row));
        gtk_widget_set_sensitive (GTK_WIDGET (self->add_button), username_valid && is_valid_name (name));

        if (tip && strlen (tip) > 0) {
                cc_entry_feedback_update (self->name_feedback, "dialog-error-symbolic", tip);
        } else {
                cc_entry_feedback_reset (self->name_feedback);
        }
}

static void
update_password_match (CcAddUserDialog *self)
{
        const gchar *password;
        const gchar *verify;
        gboolean match;

        password = gtk_editable_get_text (GTK_EDITABLE (self->password_row));
        verify = gtk_editable_get_text (GTK_EDITABLE (self->verify_password_row));
        match = g_str_equal (password, verify);

        gtk_widget_set_visible (GTK_WIDGET (self->verify_password_hint),
                                strlen (verify) != 0);
        cc_entry_feedback_update (self->verify_password_hint,
                                  match ? "check-outlined-symbolic" : "dialog-error-symbolic",
                                  match ? _("Passwords match") : _("Passwords do not match"));
}

static void
generate_password (CcAddUserDialog *self)
{
        g_autofree gchar *pwd = NULL;

        pwd = pw_generate ();
        if (pwd == NULL)
                return;

        gtk_editable_set_text (GTK_EDITABLE (self->password_row), pwd);
        gtk_editable_set_text (GTK_EDITABLE (self->verify_password_row), pwd);
        gtk_widget_set_sensitive (GTK_WIDGET (self->verify_password_row), TRUE);
}

static gboolean
password_timeout (CcAddUserDialog *self)
{
        self->password_timeout_id = 0;

        gtk_widget_set_sensitive (self->password_page_add_button, validate_password (self));
        update_password_match (self);

        return FALSE;
}

static gboolean
password_focus_out_event_cb (CcAddUserDialog *self)
{
        g_clear_handle_id (&self->password_timeout_id, g_source_remove);

        password_timeout (self);

        return FALSE;
}

static gboolean
password_entry_key_press_event_cb (CcAddUserDialog       *self,
                                         guint                  keyval,
                                         guint                  keycode,
                                         GdkModifierType        state,
                                         GtkEventControllerKey *controller)
{
        if (keyval == GDK_KEY_Tab)
               password_timeout (self);

        return FALSE;
}

static void
recheck_password_match (CcAddUserDialog *self)
{
        g_clear_handle_id (&self->password_timeout_id, g_source_remove);

        gtk_widget_set_sensitive (GTK_WIDGET (self->password_page_add_button), FALSE);

        self->password_timeout_id = g_timeout_add (PASSWORD_CHECK_TIMEOUT, (GSourceFunc) password_timeout, self);
}

static void
password_entry_changed_cb (CcAddUserDialog *self)
{
        recheck_password_match (self);
}

static void
verify_entry_changed_cb (CcAddUserDialog *self)
{
        recheck_password_match (self);
}

static void
password_radio_changed_cb (CcAddUserDialog *self)
{
        gboolean active;

        active = gtk_check_button_get_active (GTK_CHECK_BUTTON (self->password_radio));
        self->password_mode = active ? ACT_USER_PASSWORD_MODE_REGULAR : ACT_USER_PASSWORD_MODE_SET_AT_LOGIN;
        gtk_button_set_label (self->add_button, active ? _("_Next") : _("_Add"));

        dialog_validate (self);
}

static void
dialog_validate (CcAddUserDialog *self)
{
        const gchar *username;

        username = gtk_editable_get_text (GTK_EDITABLE (self->username_row));
        is_valid_username_async (username, NULL, dialog_validate_cb, g_object_ref (self));
}

static void
password_page_validate (CcAddUserDialog *self)
{
        gboolean valid = FALSE;

        valid = validate_password (self);

        gtk_widget_set_sensitive (GTK_WIDGET (self->add_button), valid);
}

static void
cc_add_user_dialog_init (CcAddUserDialog *self)
{
        gtk_widget_init_template (GTK_WIDGET (self));

        self->cancellable = g_cancellable_new ();

        self->password_mode = ACT_USER_PASSWORD_MODE_SET_AT_LOGIN;
        dialog_validate (self);
        update_password_strength (self);
}

static void
on_permission_acquired (GObject *source_object,
                        GAsyncResult *res,
                        gpointer user_data)
{
        g_autoptr(CcAddUserDialog) self = CC_ADD_USER_DIALOG (user_data);
        g_autoptr(GError) error = NULL;

        /* Paired with begin_action in cc_add_user_dialog_response () */
        finish_action (self);

        if (g_permission_acquire_finish (self->permission, res, &error)) {
                g_return_if_fail (g_permission_get_allowed (self->permission));
                add_button_clicked_cb (self);
        } else if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
                g_warning ("Failed to acquire permission: %s", error->message);
        }
}

static void
add_button_clicked_cb (CcAddUserDialog *self)
{
        /* We don't (or no longer) have necessary permissions */
        if (self->permission && !g_permission_get_allowed (self->permission)) {
                begin_action (self);
                g_permission_acquire_async (self->permission, self->cancellable,
                                            on_permission_acquired, g_object_ref (self));
                return;
        }

        if (self->password_mode == ACT_USER_PASSWORD_MODE_REGULAR) {
                adw_navigation_view_push_by_tag (self->navigation, "password-page");

                return;
        }

        create_user (self);
}

static void
cc_add_user_dialog_dispose (GObject *obj)
{
        CcAddUserDialog *self = CC_ADD_USER_DIALOG (obj);

        if (self->cancellable)
                g_cancellable_cancel (self->cancellable);

        g_clear_object (&self->user);

        g_clear_handle_id (&self->password_timeout_id, g_source_remove);

        G_OBJECT_CLASS (cc_add_user_dialog_parent_class)->dispose (obj);
}

static void
cc_add_user_dialog_finalize (GObject *obj)
{
        CcAddUserDialog *self = CC_ADD_USER_DIALOG (obj);

        g_clear_object (&self->cancellable);
        g_clear_object (&self->permission);
        g_clear_pointer (&self->password_hint_text, g_free);

        G_OBJECT_CLASS (cc_add_user_dialog_parent_class)->finalize (obj);
}

static void
cc_add_user_dialog_class_init (CcAddUserDialogClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);
        GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

        object_class->dispose = cc_add_user_dialog_dispose;
        object_class->finalize = cc_add_user_dialog_finalize;

        gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/system/users/cc-add-user-dialog.ui");

        gtk_widget_class_bind_template_child (widget_class, CcAddUserDialog, add_button);
        gtk_widget_class_bind_template_child (widget_class, CcAddUserDialog, account_type_switch);
        gtk_widget_class_bind_template_child (widget_class, CcAddUserDialog, page);
        gtk_widget_class_bind_template_child (widget_class, CcAddUserDialog, password_hint);
        gtk_widget_class_bind_template_child (widget_class, CcAddUserDialog, password_row);
        gtk_widget_class_bind_template_child (widget_class, CcAddUserDialog, password_radio);
        gtk_widget_class_bind_template_child (widget_class, CcAddUserDialog, name_row);
        gtk_widget_class_bind_template_child (widget_class, CcAddUserDialog, username_row);
        gtk_widget_class_bind_template_child (widget_class, CcAddUserDialog, strength_indicator);
        gtk_widget_class_bind_template_child (widget_class, CcAddUserDialog, verify_password_row);
        gtk_widget_class_bind_template_child (widget_class, CcAddUserDialog, verify_password_hint);
        gtk_widget_class_bind_template_child (widget_class, CcAddUserDialog, name_feedback);
        gtk_widget_class_bind_template_child (widget_class, CcAddUserDialog, navigation);
        gtk_widget_class_bind_template_child (widget_class, CcAddUserDialog, password_page);
        gtk_widget_class_bind_template_child (widget_class, CcAddUserDialog, password_page_add_button);
        gtk_widget_class_bind_template_child (widget_class, CcAddUserDialog, spinner);

        gtk_widget_class_bind_template_callback (widget_class, add_button_clicked_cb);
        gtk_widget_class_bind_template_callback (widget_class, dialog_validate);
        gtk_widget_class_bind_template_callback (widget_class, generate_password);
        gtk_widget_class_bind_template_callback (widget_class, create_user);
        gtk_widget_class_bind_template_callback (widget_class, password_entry_changed_cb);
        gtk_widget_class_bind_template_callback (widget_class, password_entry_key_press_event_cb);
        gtk_widget_class_bind_template_callback (widget_class, password_radio_changed_cb);
        gtk_widget_class_bind_template_callback (widget_class, verify_entry_changed_cb);
        gtk_widget_class_bind_template_callback (widget_class, password_focus_out_event_cb);
        gtk_widget_class_bind_template_callback (widget_class, password_page_validate);
}

CcAddUserDialog *
cc_add_user_dialog_new (GPermission *permission)
{
        CcAddUserDialog *self;

        self = g_object_new (CC_TYPE_ADD_USER_DIALOG, NULL);

        if (permission != NULL)
                self->permission = g_object_ref (permission);

        return self;
}

ActUser *
cc_add_user_dialog_get_user (CcAddUserDialog *self)
{
        g_return_val_if_fail (CC_IS_ADD_USER_DIALOG (self), NULL);
        return self->user;
}
