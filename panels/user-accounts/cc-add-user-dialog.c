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
#include "cc-list-row.h"
#include "user-utils.h"
#include "pw-utils.h"

#define PASSWORD_CHECK_TIMEOUT 600
#define DOMAIN_DEFAULT_HINT _("Should match the web address of your login provider.")

static void   add_button_clicked_cb (CcAddUserDialog *self);

struct _CcAddUserDialog {
        AdwWindow           parent_instance;

        GtkButton          *add_button;
        GtkSwitch          *local_account_type_switch;
        AdwEntryRow        *local_name_row;
        AdwActionRow       *local_password_row;
        GtkLevelBar        *local_strength_indicator;
        GtkLabel           *local_password_hint;
        GtkCheckButton     *local_password_radio;
        AdwEntryRow        *local_username_row;
        AdwPasswordEntryRow *local_verify_password_row;
        GtkImage           *name_message_icon;
        GtkLabel           *name_message_label;
        GtkWidget          *name_message_box;
        GtkWidget          *password_page;
        GtkStack           *stack;

        GCancellable       *cancellable;
        GPermission        *permission;
        ActUser            *user;

        gboolean            has_custom_username;
        ActUserPasswordMode local_password_mode;
        gint                local_password_timeout_id;
        gboolean            local_valid_username;
};

G_DEFINE_TYPE (CcAddUserDialog, cc_add_user_dialog, ADW_TYPE_WINDOW);

static void
show_error_dialog (CcAddUserDialog *self,
                   const gchar     *message,
                   GError          *error)
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

        gtk_window_present (GTK_WINDOW (dialog));
}

static void
user_loaded_cb (CcAddUserDialog *self,
                GParamSpec      *pspec,
                ActUser         *user)
{
  const gchar *password;

  /* Set a password for the user */
  password = gtk_editable_get_text (GTK_EDITABLE (self->local_password_row));
  act_user_set_password_mode (user, self->local_password_mode);
  if (self->local_password_mode == ACT_USER_PASSWORD_MODE_REGULAR)
        act_user_set_password (user, password, "");

  self->user = g_object_ref (user);
  gtk_window_close (GTK_WINDOW (self));
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
                g_debug ("Failed to create user: %s", error->message);
                if (!g_error_matches (error, ACT_USER_MANAGER_ERROR, ACT_USER_MANAGER_ERROR_PERMISSION_DENIED))
                       show_error_dialog (self, _("Failed to add account"), error);
                gtk_widget_grab_focus (GTK_WIDGET (self->local_name_row));
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
local_create_user (CcAddUserDialog *self)
{
        ActUserManager *manager;
        const gchar *username;
        const gchar *name;
        gint account_type;

        name = gtk_editable_get_text (GTK_EDITABLE (self->local_name_row));
        username = gtk_editable_get_text (GTK_EDITABLE (self->local_username_row));
        account_type = gtk_switch_get_active (self->local_account_type_switch) ? ACT_USER_ACCOUNT_TYPE_ADMINISTRATOR : ACT_USER_ACCOUNT_TYPE_STANDARD;

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

static void
set_name_message (CcAddUserDialog *self,
                  const gchar     *tip)
{
        gboolean visible = tip != NULL || g_strcmp0 (tip, "") == 0;

        gtk_widget_set_visible (self->name_message_box, visible);
        if (!visible)
                return;

        gtk_image_set_from_icon_name (self->name_message_icon, "help-about-symbolic");
        gtk_label_set_label (self->name_message_label, tip);
}

static gboolean
validate_name (CcAddUserDialog *self)
{
        gboolean valid_name;
        const gchar *name;

        if (self->local_valid_username) {
                set_name_message (self, NULL);
        }

        name = gtk_editable_get_text (GTK_EDITABLE (self->local_name_row));
        valid_name = is_valid_name (name);
        if (valid_name) {
                set_name_message (self, NULL);
        }

        return valid_name && self->local_valid_username;
}

static void
dialog_validate (CcAddUserDialog *self)
{
        // FIXME this needs to account for the password page
        gboolean valid = validate_name (self);
        gtk_widget_set_sensitive (GTK_WIDGET (self->add_button), valid);
}

static void local_username_is_valid_cb (GObject *source_object,
                                        GAsyncResult *result,
                                        gpointer user_data)
{
        g_autoptr(CcAddUserDialog) self = CC_ADD_USER_DIALOG (user_data);
        g_autoptr(GError) error = NULL;
        g_autofree gchar *tip = NULL;
        g_autofree gchar *username = NULL;
        const gchar *name;
        gboolean valid;

        valid = is_valid_username_finish (result, &tip, &username, &error);
        if (error != NULL) {
                g_warning ("Could not check username by usermod: %s", error->message);
                valid = TRUE;
        }

        name = gtk_editable_get_text (GTK_EDITABLE (self->local_username_row));
        //if (g_strcmp0 (name, username) == 0) {
                self->local_valid_username = valid;

                set_name_message (self, tip);
                dialog_validate (self);
        //}
}

static void
local_username_validate (CcAddUserDialog *self)
{
        const gchar *name;

        name = gtk_editable_get_text (GTK_EDITABLE (self->local_username_row));
        is_valid_username_async (name, NULL, local_username_is_valid_cb, g_object_ref (self));
}

static void
local_name_entry_activated_cb (CcAddUserDialog *self)
{
        //FIXME this needs to account for the password page
        //gtk_widget_set_sensitive (GTK_WIDGET (self->add_button), FALSE);
}

static void
confirm_passwords_match (CcAddUserDialog *self)
{
        const gchar *password;
        const gchar *verify;

        password = gtk_editable_get_text (GTK_EDITABLE (self->local_password_row));
        verify = gtk_editable_get_text (GTK_EDITABLE (self->local_verify_password_row));

        if (g_strcmp0 (password, verify) == 0) {
                gtk_label_set_label (self->password_verify_status_label, _("Passwords match"));
                gtk_image_set_from_icon_name (self->password_verify_status_icon, "");
        } else {
                gtk_label_set_label (self->password_verify_status_label, _("Passwords do not match"));
                gtk_image_set_from_icon_name (self->password_verify_status_icon, "");
        }
}

static void
validate_password (CcAddUserDialog *self)
{
        const gchar *password;
        const gchar *message = "";
        gint strength;

        password = gtk_editable_get_text (GTK_EDITABLE (self->local_password_row));

        pw_strength (password, NULL, NULL, &message, &strength);

        gtk_level_bar_set_value (self->local_strength_indicator, strength);
        gtk_label_set_label (self->local_password_hint, message);
        gtk_widget_set_sensitive (GTK_WIDGET (self->add_button), strength > 1);
}

static void
generate_password (CcAddUserDialog *self)
{
        g_autofree gchar *pwd = NULL;

        pwd = pw_generate ();
        if (pwd == NULL)
                return;

        gtk_editable_set_text (GTK_EDITABLE (self->local_password_row), pwd);
        gtk_editable_set_text (GTK_EDITABLE (self->local_verify_password_row), pwd);
        gtk_widget_set_sensitive (GTK_WIDGET (self->local_verify_password_row), TRUE);
}

static void
local_password_radio_changed_cb (CcAddUserDialog *self)
{
        gboolean active;

        active = gtk_check_button_get_active (GTK_CHECK_BUTTON (self->local_password_radio));
        self->local_password_mode = active ? ACT_USER_PASSWORD_MODE_REGULAR : ACT_USER_PASSWORD_MODE_SET_AT_LOGIN;

        // Change button
        if (self->local_password_mode == ACT_USER_PASSWORD_MODE_REGULAR)
                gtk_button_set_label (self->add_button, _("Next"));
        else
                gtk_button_set_label (self->add_button, _("Add"));

        dialog_validate (self);
}

static void
cc_add_user_dialog_init (CcAddUserDialog *self)
{
        gtk_widget_init_template (GTK_WIDGET (self));

        self->cancellable = g_cancellable_new ();

        self->local_password_mode = ACT_USER_PASSWORD_MODE_SET_AT_LOGIN;
        dialog_validate (self);
}

static void
on_permission_acquired (GObject *source_object,
                        GAsyncResult *res,
                        gpointer user_data)
{
        g_autoptr(CcAddUserDialog) self = CC_ADD_USER_DIALOG (user_data);
        g_autoptr(GError) error = NULL;

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
                g_permission_acquire_async (self->permission, self->cancellable,
                                            on_permission_acquired, g_object_ref (self));
                return;
        }

        if (self->local_password_mode == ACT_USER_PASSWORD_MODE_REGULAR) {
                gtk_stack_set_visible_child (self->stack, self->password_page);
                gtk_button_set_label (self->add_button, _("Add"));
        } else {
                local_create_user (self);
        }
}

static void
cc_add_user_dialog_dispose (GObject *obj)
{
        CcAddUserDialog *self = CC_ADD_USER_DIALOG (obj);

        if (self->cancellable)
                g_cancellable_cancel (self->cancellable);

        g_clear_object (&self->user);

        if (self->local_password_timeout_id != 0) {
                g_source_remove (self->local_password_timeout_id);
                self->local_password_timeout_id = 0;
        }

        G_OBJECT_CLASS (cc_add_user_dialog_parent_class)->dispose (obj);
}

static void
cc_add_user_dialog_finalize (GObject *obj)
{
        CcAddUserDialog *self = CC_ADD_USER_DIALOG (obj);

        g_clear_object (&self->cancellable);
        g_clear_object (&self->permission);

        G_OBJECT_CLASS (cc_add_user_dialog_parent_class)->finalize (obj);
}

static void
cc_add_user_dialog_class_init (CcAddUserDialogClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);
        GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

        object_class->dispose = cc_add_user_dialog_dispose;
        object_class->finalize = cc_add_user_dialog_finalize;

        gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/user-accounts/cc-add-user-dialog.ui");

        gtk_widget_class_bind_template_child (widget_class, CcAddUserDialog, add_button);
        gtk_widget_class_bind_template_child (widget_class, CcAddUserDialog, local_account_type_switch);
        gtk_widget_class_bind_template_child (widget_class, CcAddUserDialog, local_password_hint);
        gtk_widget_class_bind_template_child (widget_class, CcAddUserDialog, local_password_row);
        gtk_widget_class_bind_template_child (widget_class, CcAddUserDialog, local_name_row);
        gtk_widget_class_bind_template_child (widget_class, CcAddUserDialog, local_password_radio);
        gtk_widget_class_bind_template_child (widget_class, CcAddUserDialog, local_username_row);
        gtk_widget_class_bind_template_child (widget_class, CcAddUserDialog, local_strength_indicator);
        gtk_widget_class_bind_template_child (widget_class, CcAddUserDialog, local_verify_password_row);
        gtk_widget_class_bind_template_child (widget_class, CcAddUserDialog, name_message_label);
        gtk_widget_class_bind_template_child (widget_class, CcAddUserDialog, name_message_icon);
        gtk_widget_class_bind_template_child (widget_class, CcAddUserDialog, name_message_box);
        gtk_widget_class_bind_template_child (widget_class, CcAddUserDialog, password_page);
        gtk_widget_class_bind_template_child (widget_class, CcAddUserDialog, stack);

        gtk_widget_class_bind_template_callback (widget_class, add_button_clicked_cb);
        gtk_widget_class_bind_template_callback (widget_class, dialog_validate);
        gtk_widget_class_bind_template_callback (widget_class, confirm_passwords_match);
        gtk_widget_class_bind_template_callback (widget_class, validate_password);
        gtk_widget_class_bind_template_callback (widget_class, generate_password);
        gtk_widget_class_bind_template_callback (widget_class, local_name_entry_activated_cb);
        gtk_widget_class_bind_template_callback (widget_class, local_username_validate);
        gtk_widget_class_bind_template_callback (widget_class, local_password_radio_changed_cb);
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
