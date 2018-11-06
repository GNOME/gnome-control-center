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

#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <glib.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <act/act.h>

#include "um-password-dialog.h"
#include "um-resources.h"
#include "um-utils.h"
#include "run-passwd.h"
#include "pw-utils.h"

#define PASSWORD_CHECK_TIMEOUT 600

struct _UmPasswordDialog
{
        GtkDialog           parent_instance;

        GtkBox             *action_radio_box;
        GtkRadioButton     *action_now_radio;
        GtkRadioButton     *action_login_radio;
        GtkButton          *ok_button;
        GtkLabel           *old_password_label;
        GtkEntry           *old_password_entry;
        GtkEntry           *password_entry;
        GtkLabel           *password_hint_label;
        GtkLevelBar        *strength_indicator;
        GtkEntry           *verify_entry;
        GtkLabel           *verify_hint_label;

        gint                password_entry_timeout_id;

        ActUser            *user;
        ActUserPasswordMode password_mode;

        gboolean            old_password_ok;
        gint                old_password_entry_timeout_id;

        PasswdHandler      *passwd_handler;
};

G_DEFINE_TYPE (UmPasswordDialog, um_password_dialog, GTK_TYPE_DIALOG)

static gint
update_password_strength (UmPasswordDialog *um)
{
        const gchar *password;
        const gchar *old_password;
        const gchar *username;
        gint strength_level;
        const gchar *hint;
        const gchar *verify;

        password = gtk_entry_get_text (um->password_entry);
        old_password = gtk_entry_get_text (um->old_password_entry);
        username = act_user_get_user_name (um->user);

        pw_strength (password, old_password, username,
                     &hint, &strength_level);

        gtk_level_bar_set_value (um->strength_indicator, strength_level);
        gtk_label_set_label (um->password_hint_label, hint);

        if (strength_level > 1) {
                set_entry_validation_checkmark (um->password_entry);
        } else if (strlen (password) == 0) {
                set_entry_generation_icon (um->password_entry);
        } else {
                clear_entry_validation_error (um->password_entry);
        }

        verify = gtk_entry_get_text (um->verify_entry);
        if (strlen (verify) == 0) {
                gtk_widget_set_sensitive (GTK_WIDGET (um->verify_entry), strength_level > 1);
        }

        return strength_level;
}

static void
password_changed_cb (PasswdHandler    *handler,
                     GError           *error,
                     UmPasswordDialog *um)
{
        GtkWidget *dialog;
        const gchar *primary_text;
        const gchar *secondary_text;

        gtk_widget_set_sensitive (GTK_WIDGET (um), TRUE);
        gdk_window_set_cursor (gtk_widget_get_window (GTK_WIDGET (um)), NULL);

        if (!error) {
                gtk_dialog_response (GTK_DIALOG (um), GTK_RESPONSE_ACCEPT);
                return;
        }

        if (error->code == PASSWD_ERROR_REJECTED) {
                primary_text = error->message;
                secondary_text = _("Please choose another password.");

                gtk_entry_set_text (um->password_entry, "");
                gtk_widget_grab_focus (GTK_WIDGET (um->password_entry));

                gtk_entry_set_text (um->verify_entry, "");
        }
        else if (error->code == PASSWD_ERROR_AUTH_FAILED) {
                primary_text = error->message;
                secondary_text = _("Please type your current password again.");

                gtk_entry_set_text (um->old_password_entry, "");
                gtk_widget_grab_focus (GTK_WIDGET (um->old_password_entry));
        }
        else {
                primary_text = _("Password could not be changed");
                secondary_text = error->message;
        }

        dialog = gtk_message_dialog_new (GTK_WINDOW (um),
                                         GTK_DIALOG_MODAL,
                                         GTK_MESSAGE_ERROR,
                                         GTK_BUTTONS_CLOSE,
                                         "%s", primary_text);
        gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
                                                  "%s", secondary_text);
        gtk_dialog_run (GTK_DIALOG (dialog));
        gtk_widget_destroy (dialog);
}

static void
ok_button_clicked_cb (UmPasswordDialog *um)
{
        const gchar *password;

        password = gtk_entry_get_text (um->password_entry);

        switch (um->password_mode) {
                case ACT_USER_PASSWORD_MODE_REGULAR:
                        if (act_user_get_uid (um->user) == getuid ()) {
                                GdkDisplay *display;
                                g_autoptr(GdkCursor) cursor = NULL;

                                /* When setting a password for the current user,
                                 * use passwd directly, to preserve the audit trail
                                 * and to e.g. update the keyring password.
                                 */
                                passwd_change_password (um->passwd_handler, password,
                                                        (PasswdCallback) password_changed_cb, um);
                                gtk_widget_set_sensitive (GTK_WIDGET (um), FALSE);
                                display = gtk_widget_get_display (GTK_WIDGET (um));
                                cursor = gdk_cursor_new_for_display (display, GDK_WATCH);
                                gdk_window_set_cursor (gtk_widget_get_window (GTK_WIDGET (um)), cursor);
                                gdk_display_flush (display);
                                return;
                        }

                        act_user_set_password_mode (um->user, ACT_USER_PASSWORD_MODE_REGULAR);
                        act_user_set_password (um->user, password, "");
                        break;

                case ACT_USER_PASSWORD_MODE_SET_AT_LOGIN:
                        act_user_set_password_mode (um->user,  um->password_mode);
                        act_user_set_automatic_login (um->user, FALSE);
                        break;

                default:
                        g_assert_not_reached ();
        }

        gtk_dialog_response (GTK_DIALOG (um), GTK_RESPONSE_ACCEPT);
}

static void
update_sensitivity (UmPasswordDialog *um)
{
        const gchar *password, *verify;
        gboolean can_change;
        int strength;

        password = gtk_entry_get_text (um->password_entry);
        verify = gtk_entry_get_text (um->verify_entry);

        if (um->password_mode == ACT_USER_PASSWORD_MODE_REGULAR) {
                strength = update_password_strength (um);
                can_change = strength > 1 && strcmp (password, verify) == 0 &&
                             (um->old_password_ok || !gtk_widget_get_visible (GTK_WIDGET (um->old_password_entry)));
        }
        else {
                can_change = TRUE;
        }

        gtk_widget_set_sensitive (GTK_WIDGET (um->ok_button), can_change);
}

static void
mode_change (UmPasswordDialog *um,
             ActUserPasswordMode mode)
{
        gboolean active;

        active = (mode == ACT_USER_PASSWORD_MODE_REGULAR);
        gtk_widget_set_sensitive (GTK_WIDGET (um->password_entry), active);
        gtk_widget_set_sensitive (GTK_WIDGET (um->verify_entry), active);
        gtk_widget_set_sensitive (GTK_WIDGET (um->old_password_entry), active);
        gtk_widget_set_sensitive (GTK_WIDGET (um->password_hint_label), active);
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (um->action_now_radio), active);
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (um->action_login_radio), !active);

        um->password_mode = mode;
        update_sensitivity (um);
}

static void
action_now_radio_toggled_cb (UmPasswordDialog *um)
{
        gint active;
        ActUserPasswordMode mode;

        active = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (um->action_now_radio));
        mode = active ? ACT_USER_PASSWORD_MODE_REGULAR : ACT_USER_PASSWORD_MODE_SET_AT_LOGIN;
        mode_change (um, mode);
}

static void
update_password_match (UmPasswordDialog *um)
{
        const gchar *password;
        const gchar *verify;
        const gchar *message = "";

        password = gtk_entry_get_text (um->password_entry);
        verify = gtk_entry_get_text (um->verify_entry);

        if (strlen (verify) > 0) {
                if (strcmp (password, verify) != 0) {
                        message = _("The passwords do not match.");
                }
                else {
                        set_entry_validation_checkmark (um->verify_entry);
                }
        }
        gtk_label_set_label (um->verify_hint_label, message);
}

static gboolean
password_entry_timeout (UmPasswordDialog *um)
{
        update_password_strength (um);
        update_sensitivity (um);
        update_password_match (um);

        um->password_entry_timeout_id = 0;

        return FALSE;
}

static void
recheck_password_match (UmPasswordDialog *um)
{
        const gchar *password;

        if (um->password_entry_timeout_id != 0) {
                g_source_remove (um->password_entry_timeout_id);
                um->password_entry_timeout_id = 0;
        }

        gtk_widget_set_sensitive (GTK_WIDGET (um->ok_button), FALSE);

        password = gtk_entry_get_text (um->password_entry);
        if (strlen (password) == 0) {
                gtk_entry_set_visibility (um->password_entry, FALSE);
        }

        um->password_entry_timeout_id = g_timeout_add (PASSWORD_CHECK_TIMEOUT,
                                                       (GSourceFunc) password_entry_timeout,
                                                       um);
}

static void
password_entry_changed (UmPasswordDialog *um)
{
        clear_entry_validation_error (um->password_entry);
        clear_entry_validation_error (um->verify_entry);
        recheck_password_match (um);
}

static void
verify_entry_changed (UmPasswordDialog *um)
{
        clear_entry_validation_error (um->verify_entry);
        recheck_password_match (um);
}

static gboolean
password_entry_focus_out_cb (UmPasswordDialog *um)
{
        if (um->password_entry_timeout_id != 0) {
                g_source_remove (um->password_entry_timeout_id);
                um->password_entry_timeout_id = 0;
        }

        password_entry_timeout (um);

        return FALSE;
}

static gboolean
password_entry_key_press_cb (UmPasswordDialog *um,
                             GdkEvent         *event)
{
        GdkEventKey *key = (GdkEventKey *)event;

        if (key->keyval == GDK_KEY_Tab)
               password_entry_timeout (um);

        return FALSE;
}

static void
auth_cb (PasswdHandler    *handler,
         GError           *error,
         UmPasswordDialog *um)
{
        if (error) {
                um->old_password_ok = FALSE;
        }
        else {
                um->old_password_ok = TRUE;
                set_entry_validation_checkmark (um->old_password_entry);
        }

        update_sensitivity (um);
}

static gboolean
old_password_entry_timeout (UmPasswordDialog *um)
{
        const gchar *text;

        update_sensitivity (um);

        text = gtk_entry_get_text (um->old_password_entry);
        if (!um->old_password_ok) {
                passwd_authenticate (um->passwd_handler, text, (PasswdCallback)auth_cb, um);
        }

        um->old_password_entry_timeout_id = 0;

        return FALSE;
}

static gboolean
old_password_entry_focus_out_cb (UmPasswordDialog *um)
{
        if (um->old_password_entry_timeout_id != 0) {
                g_source_remove (um->old_password_entry_timeout_id);
                um->old_password_entry_timeout_id = 0;
        }

        old_password_entry_timeout (um);

        return FALSE;
}

static void
old_password_entry_changed (UmPasswordDialog *um)
{
        if (um->old_password_entry_timeout_id != 0) {
                g_source_remove (um->old_password_entry_timeout_id);
                um->old_password_entry_timeout_id = 0;
        }

        clear_entry_validation_error (um->old_password_entry);
        gtk_widget_set_sensitive (GTK_WIDGET (um->ok_button), FALSE);

        um->old_password_ok = FALSE;
        um->old_password_entry_timeout_id = g_timeout_add (PASSWORD_CHECK_TIMEOUT,
                                                           (GSourceFunc) old_password_entry_timeout,
                                                           um);
}

static void
password_entry_icon_press_cb (UmPasswordDialog *um)
{
        g_autofree gchar *pwd = NULL;

        pwd = pw_generate ();
        if (pwd == NULL)
                return;

        gtk_entry_set_text (um->password_entry, pwd);
        gtk_entry_set_text (um->verify_entry, pwd);
        gtk_entry_set_visibility (um->password_entry, TRUE);
        gtk_widget_set_sensitive (GTK_WIDGET (um->verify_entry), TRUE);
}

static void
um_password_dialog_dispose (GObject *object)
{
        UmPasswordDialog *um = UM_PASSWORD_DIALOG (object);

        g_clear_object (&um->user);

        if (um->passwd_handler) {
                passwd_destroy (um->passwd_handler);
                um->passwd_handler = NULL;
        }

        if (um->old_password_entry_timeout_id != 0) {
                g_source_remove (um->old_password_entry_timeout_id);
                um->old_password_entry_timeout_id = 0;
        }
 
        if (um->password_entry_timeout_id != 0) {
                g_source_remove (um->password_entry_timeout_id);
                um->password_entry_timeout_id = 0;
        }

        G_OBJECT_CLASS (um_password_dialog_parent_class)->dispose (object);
}

static void
um_password_dialog_class_init (UmPasswordDialogClass *klass)
{
        GObjectClass   *object_class = G_OBJECT_CLASS (klass);
        GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

        object_class->dispose = um_password_dialog_dispose;

        gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/user-accounts/um-password-dialog.ui");

        gtk_widget_class_bind_template_child (widget_class, UmPasswordDialog, action_radio_box);
        gtk_widget_class_bind_template_child (widget_class, UmPasswordDialog, action_now_radio);
        gtk_widget_class_bind_template_child (widget_class, UmPasswordDialog, action_login_radio);
        gtk_widget_class_bind_template_child (widget_class, UmPasswordDialog, ok_button);
        gtk_widget_class_bind_template_child (widget_class, UmPasswordDialog, old_password_label);
        gtk_widget_class_bind_template_child (widget_class, UmPasswordDialog, old_password_entry);
        gtk_widget_class_bind_template_child (widget_class, UmPasswordDialog, password_entry);
        gtk_widget_class_bind_template_child (widget_class, UmPasswordDialog, password_hint_label);
        gtk_widget_class_bind_template_child (widget_class, UmPasswordDialog, strength_indicator);
        gtk_widget_class_bind_template_child (widget_class, UmPasswordDialog, verify_entry);
        gtk_widget_class_bind_template_child (widget_class, UmPasswordDialog, verify_hint_label);

        gtk_widget_class_bind_template_callback (widget_class, action_now_radio_toggled_cb);
        gtk_widget_class_bind_template_callback (widget_class, old_password_entry_changed);
        gtk_widget_class_bind_template_callback (widget_class, old_password_entry_focus_out_cb);
        gtk_widget_class_bind_template_callback (widget_class, ok_button_clicked_cb);
        gtk_widget_class_bind_template_callback (widget_class, password_entry_changed);
        gtk_widget_class_bind_template_callback (widget_class, password_entry_focus_out_cb);
        gtk_widget_class_bind_template_callback (widget_class, password_entry_icon_press_cb);
        gtk_widget_class_bind_template_callback (widget_class, password_entry_key_press_cb);
        gtk_widget_class_bind_template_callback (widget_class, password_entry_timeout);
        gtk_widget_class_bind_template_callback (widget_class, verify_entry_changed);
}

static void
um_password_dialog_init (UmPasswordDialog *um)
{
        g_resources_register (um_get_resource ());

        gtk_widget_init_template (GTK_WIDGET (um));
}

UmPasswordDialog *
um_password_dialog_new (ActUser *user)
{
        UmPasswordDialog *um;

        g_return_val_if_fail (ACT_IS_USER (user), NULL);

        um = g_object_new (UM_TYPE_PASSWORD_DIALOG,
                           "use-header-bar", 1,
                           NULL);

        um->user = g_object_ref (user);

        if (act_user_get_uid (um->user) == getuid ()) {
                gboolean visible;

                mode_change (um, ACT_USER_PASSWORD_MODE_REGULAR);
                gtk_widget_hide (GTK_WIDGET (um->action_radio_box));

                visible = (act_user_get_password_mode (user) != ACT_USER_PASSWORD_MODE_NONE);
                gtk_widget_set_visible (GTK_WIDGET (um->old_password_label), visible);
                gtk_widget_set_visible (GTK_WIDGET (um->old_password_entry), visible);
                um->old_password_ok = !visible;

                um->passwd_handler = passwd_init ();
        }
        else {
                mode_change (um, ACT_USER_PASSWORD_MODE_SET_AT_LOGIN);
                gtk_widget_show (GTK_WIDGET (um->action_radio_box));

                gtk_widget_hide (GTK_WIDGET (um->old_password_label));
                gtk_widget_hide (GTK_WIDGET (um->old_password_entry));
                um->old_password_ok = TRUE;
        }

        if (um->old_password_ok == FALSE)
                gtk_widget_grab_focus (GTK_WIDGET (um->old_password_entry));
        else
                gtk_widget_grab_focus (GTK_WIDGET (um->password_entry));

        gtk_widget_grab_default (GTK_WIDGET (um->ok_button));

        return um;
}
