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
#include "um-utils.h"
#include "run-passwd.h"
#include "pw-utils.h"

#define PASSWORD_CHECK_TIMEOUT 600

struct _UmPasswordDialog {
        GtkWidget *dialog;
        GtkWidget *action_radio_box;
        GtkWidget *action_now_radio;
        GtkWidget *action_login_radio;
        GtkWidget *password_entry;
        GtkWidget *verify_entry;
        gint       password_entry_timeout_id;
        GtkWidget *strength_indicator;
        GtkWidget *ok_button;
        GtkWidget *password_hint;
        GtkWidget *verify_hint;

        ActUser *user;
        ActUserPasswordMode password_mode;

        GtkWidget *old_password_label;
        GtkWidget *old_password_entry;
        gboolean   old_password_ok;
        gint       old_password_entry_timeout_id;

        PasswdHandler *passwd_handler;
};

static gint
update_password_strength (UmPasswordDialog *um)
{
        const gchar *password;
        const gchar *old_password;
        const gchar *username;
        gint strength_level;
        const gchar *hint;

        if (um->user == NULL) {
                return 0;
        }

        password = gtk_entry_get_text (GTK_ENTRY (um->password_entry));
        old_password = gtk_entry_get_text (GTK_ENTRY (um->old_password_entry));
        username = act_user_get_user_name (um->user);

        pw_strength (password, old_password, username,
                     &hint, &strength_level);

        gtk_level_bar_set_value (GTK_LEVEL_BAR (um->strength_indicator), strength_level);
        gtk_label_set_label (GTK_LABEL (um->password_hint), hint);

        if (strength_level > 1) {
                set_entry_validation_checkmark (GTK_ENTRY (um->password_entry));
        } else if (strlen (password) == 0) {
                set_entry_generation_icon (GTK_ENTRY (um->password_entry));
        } else {
                clear_entry_validation_error (GTK_ENTRY (um->password_entry));
        }

        return strength_level;
}

static void
finish_password_change (UmPasswordDialog *um)
{
        gtk_widget_hide (um->dialog);

        gtk_entry_set_text (GTK_ENTRY (um->password_entry), " ");
        gtk_entry_set_text (GTK_ENTRY (um->verify_entry), "");
        gtk_entry_set_text (GTK_ENTRY (um->old_password_entry), "");

        um_password_dialog_set_user (um, NULL);
}

static void
cancel_password_dialog (GtkButton        *button,
                        UmPasswordDialog *um)
{
        finish_password_change (um);
}

static void
dialog_closed (GtkWidget        *dialog,
               gint              response_id,
               UmPasswordDialog *um)
{
        gtk_widget_destroy (dialog);
}

static void
password_changed_cb (PasswdHandler    *handler,
                     GError           *error,
                     UmPasswordDialog *um)
{
        GtkWidget *dialog;
        const gchar *primary_text;
        const gchar *secondary_text;

        gtk_widget_set_sensitive (um->dialog, TRUE);
        gdk_window_set_cursor (gtk_widget_get_window (um->dialog), NULL);

        if (!error) {
                finish_password_change (um);
                return;
        }

        if (error->code == PASSWD_ERROR_REJECTED) {
                primary_text = error->message;
                secondary_text = _("Please choose another password.");

                gtk_entry_set_text (GTK_ENTRY (um->password_entry), "");
                gtk_widget_grab_focus (um->password_entry);

                gtk_entry_set_text (GTK_ENTRY (um->verify_entry), "");
        }
        else if (error->code == PASSWD_ERROR_AUTH_FAILED) {
                primary_text = error->message;
                secondary_text = _("Please type your current password again.");

                gtk_entry_set_text (GTK_ENTRY (um->old_password_entry), "");
                gtk_widget_grab_focus (um->old_password_entry);
        }
        else {
                primary_text = _("Password could not be changed");
                secondary_text = error->message;
        }

        dialog = gtk_message_dialog_new (GTK_WINDOW (um->dialog),
                                         GTK_DIALOG_MODAL,
                                         GTK_MESSAGE_ERROR,
                                         GTK_BUTTONS_CLOSE,
                                         "%s", primary_text);
        gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
                                                  "%s", secondary_text);
        g_signal_connect (dialog, "response",
                          G_CALLBACK (dialog_closed), um);
        gtk_window_present (GTK_WINDOW (dialog));

}

static void
accept_password_dialog (GtkButton        *button,
                        UmPasswordDialog *um)
{
        const gchar *password;

        password = gtk_entry_get_text (GTK_ENTRY (um->password_entry));

        switch (um->password_mode) {
                case ACT_USER_PASSWORD_MODE_REGULAR:
                        if (act_user_get_uid (um->user) == getuid ()) {
                                GdkDisplay *display;
                                GdkCursor *cursor;

                                /* When setting a password for the current user,
                                 * use passwd directly, to preserve the audit trail
                                 * and to e.g. update the keyring password.
                                 */
                                passwd_change_password (um->passwd_handler, password,
                                                        (PasswdCallback) password_changed_cb, um);
                                gtk_widget_set_sensitive (um->dialog, FALSE);
                                display = gtk_widget_get_display (um->dialog);
                                cursor = gdk_cursor_new_for_display (display, GDK_WATCH);
                                gdk_window_set_cursor (gtk_widget_get_window (um->dialog), cursor);
                                gdk_display_flush (display);
                                g_object_unref (cursor);
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

        finish_password_change (um);
}

static void
update_sensitivity (UmPasswordDialog *um)
{
        const gchar *password, *verify;
        gboolean can_change;
        int strength;

        password = gtk_entry_get_text (GTK_ENTRY (um->password_entry));
        verify = gtk_entry_get_text (GTK_ENTRY (um->verify_entry));

        if (um->password_mode == ACT_USER_PASSWORD_MODE_REGULAR) {
                strength = update_password_strength (um);
                can_change = strength > 1 && strcmp (password, verify) == 0 &&
                             (um->old_password_ok || !gtk_widget_get_visible (um->old_password_entry));
        }
        else {
                can_change = TRUE;
        }

        gtk_widget_set_sensitive (um->ok_button, can_change);
}

static void
mode_change (UmPasswordDialog *um,
             ActUserPasswordMode mode)
{
        gboolean active;

        active = (mode == ACT_USER_PASSWORD_MODE_REGULAR);
        gtk_widget_set_sensitive (um->password_entry, active);
        gtk_widget_set_sensitive (um->verify_entry, active);
        gtk_widget_set_sensitive (um->old_password_entry, active);
        gtk_widget_set_sensitive (um->password_hint, active);
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (um->action_now_radio), active);
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (um->action_login_radio), !active);

        um->password_mode = mode;
        update_sensitivity (um);
}

static void
action_changed (GtkRadioButton   *radio,
                UmPasswordDialog *um)
{
        gint active;
        ActUserPasswordMode mode;

        active = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (radio));
        mode = active ? ACT_USER_PASSWORD_MODE_REGULAR : ACT_USER_PASSWORD_MODE_SET_AT_LOGIN;
        mode_change (um, mode);
}

static void
update_password_match (UmPasswordDialog *um)
{
        const char *password;
        const char *verify;

        password = gtk_entry_get_text (GTK_ENTRY (um->password_entry));
        verify = gtk_entry_get_text (GTK_ENTRY (um->verify_entry));

        if (strlen (password) > 0 && strlen (verify) > 0) {
                if (strcmp (password, verify) != 0) {
                        gtk_label_set_label (GTK_LABEL (um->verify_hint),
                                             _("The passwords do not match."));
                }
                else {
                        gtk_label_set_label (GTK_LABEL (um->verify_hint), "");
                        set_entry_validation_checkmark (GTK_ENTRY (um->verify_entry));
                }
        }
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
password_entry_changed (GtkEntry         *entry,
                      GParamSpec       *pspec,
                      UmPasswordDialog *um)
{
        const char *password;

        if (um->password_entry_timeout_id != 0) {
                g_source_remove (um->password_entry_timeout_id);
                um->password_entry_timeout_id = 0;
        }

        clear_entry_validation_error (GTK_ENTRY (entry));
        clear_entry_validation_error (GTK_ENTRY (um->verify_entry));
        gtk_widget_set_sensitive (um->ok_button, FALSE);

        password = gtk_entry_get_text (GTK_ENTRY (um->password_entry));
        if (strlen (password) == 0) {
                gtk_entry_set_visibility (GTK_ENTRY (um->password_entry), FALSE);
        }

        um->password_entry_timeout_id = g_timeout_add (PASSWORD_CHECK_TIMEOUT,
                                                     (GSourceFunc) password_entry_timeout,
                                                     um);
}

static gboolean
password_entry_focus_out (GtkWidget        *entry,
                        GdkEventFocus    *event,
                        UmPasswordDialog *um)
{
        if (um->password_entry_timeout_id != 0) {
                g_source_remove (um->password_entry_timeout_id);
                um->password_entry_timeout_id = 0;
        }

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
                set_entry_validation_checkmark (GTK_ENTRY (um->old_password_entry));
        }

        update_sensitivity (um);
}

static gboolean
old_password_entry_timeout (UmPasswordDialog *um)
{
        const char *text;

        update_sensitivity (um);

        text = gtk_entry_get_text (GTK_ENTRY (um->old_password_entry));
        if (!um->old_password_ok) {
                passwd_authenticate (um->passwd_handler, text, (PasswdCallback)auth_cb, um);
        }

        um->old_password_entry_timeout_id = 0;

        return FALSE;
}

static gboolean
old_password_entry_focus_out (GtkWidget        *entry,
                              GdkEventFocus    *event,
                              UmPasswordDialog *um)
{
        if (um->old_password_entry_timeout_id != 0) {
                g_source_remove (um->old_password_entry_timeout_id);
                um->old_password_entry_timeout_id = 0;
        }

        old_password_entry_timeout (um);

        return FALSE;
}

static void
old_password_entry_changed (GtkEntry         *entry,
                            GParamSpec       *pspec,
                            UmPasswordDialog *um)
{
        if (um->old_password_entry_timeout_id != 0) {
                g_source_remove (um->old_password_entry_timeout_id);
                um->old_password_entry_timeout_id = 0;
        }

        clear_entry_validation_error (GTK_ENTRY (entry));
        gtk_widget_set_sensitive (um->ok_button, FALSE);

        um->old_password_ok = FALSE;
        um->old_password_entry_timeout_id = g_timeout_add (PASSWORD_CHECK_TIMEOUT,
                                                           (GSourceFunc) old_password_entry_timeout,
                                                           um);
}

static void
on_generate (GtkEntry             *entry,
             GtkEntryIconPosition  pos,
             GdkEventButton       *event,
             UmPasswordDialog     *um)
{
        gchar *pwd;

        pwd = pw_generate ();

        gtk_entry_set_text (GTK_ENTRY (um->password_entry), pwd);
        gtk_entry_set_text (GTK_ENTRY (um->verify_entry), pwd);
        gtk_entry_set_visibility (GTK_ENTRY (um->password_entry), TRUE);

        g_free (pwd);
}

UmPasswordDialog *
um_password_dialog_new (void)
{
        GtkBuilder *builder;
        GError *error;
        UmPasswordDialog *um;
        GtkWidget *widget;

        builder = gtk_builder_new ();

        error = NULL;
        if (!gtk_builder_add_from_resource (builder,
                                            "/org/gnome/control-center/user-accounts/password-dialog.ui",
                                            &error)) {
                g_error ("%s", error->message);
                g_error_free (error);
                return NULL;
        }

        um = g_new0 (UmPasswordDialog, 1);

        um->action_radio_box = (GtkWidget *) gtk_builder_get_object (builder, "action-radio-box");
        widget = (GtkWidget *) gtk_builder_get_object (builder, "action-now-radio");
        g_signal_connect (widget, "toggled", G_CALLBACK (action_changed), um);
        um->action_now_radio = widget;
        um->action_login_radio = (GtkWidget *) gtk_builder_get_object (builder, "action-login-radio");

        widget = (GtkWidget *) gtk_builder_get_object (builder, "dialog");
        g_signal_connect (widget, "delete-event",
                          G_CALLBACK (gtk_widget_hide_on_delete), NULL);
        um->dialog = widget;

        widget = (GtkWidget *) gtk_builder_get_object (builder, "cancel-button");
        g_signal_connect (widget, "clicked",
                          G_CALLBACK (cancel_password_dialog), um);

        widget = (GtkWidget *) gtk_builder_get_object (builder, "ok-button");
        g_signal_connect (widget, "clicked",
                          G_CALLBACK (accept_password_dialog), um);
        gtk_widget_grab_default (widget);
        um->ok_button = widget;

        widget = (GtkWidget *) gtk_builder_get_object (builder, "password-entry");
        g_signal_connect (widget, "notify::text",
                          G_CALLBACK (password_entry_changed), um);
        g_signal_connect_after (widget, "focus-out-event",
                                G_CALLBACK (password_entry_focus_out), um);
        g_signal_connect_swapped (widget, "activate", G_CALLBACK (password_entry_timeout), um);
        gtk_entry_set_visibility (GTK_ENTRY (widget), FALSE);
        um->password_entry = widget;
        g_signal_connect (widget, "icon-press", G_CALLBACK (on_generate), um);

        widget = (GtkWidget *) gtk_builder_get_object (builder, "old-password-entry");
        g_signal_connect_after (widget, "focus-out-event",
                                G_CALLBACK (old_password_entry_focus_out), um);
        g_signal_connect (widget, "notify::text",
                          G_CALLBACK (old_password_entry_changed), um);
        g_signal_connect_swapped (widget, "activate", G_CALLBACK (password_entry_timeout), um);
        um->old_password_entry = widget;
        um->old_password_label = (GtkWidget *) gtk_builder_get_object (builder, "old-password-label");

        widget = (GtkWidget *) gtk_builder_get_object (builder, "verify-entry");
        g_signal_connect (widget, "notify::text",
                          G_CALLBACK (password_entry_changed), um);
        g_signal_connect_after (widget, "focus-out-event",
                                G_CALLBACK (password_entry_focus_out), um);
        g_signal_connect_swapped (widget, "activate", G_CALLBACK (password_entry_timeout), um);
        um->verify_entry = widget;

        um->strength_indicator = (GtkWidget *) gtk_builder_get_object (builder, "strength-indicator");

        widget = (GtkWidget *)gtk_builder_get_object (builder, "password-hint");
        um->password_hint = widget;

        widget = (GtkWidget *)gtk_builder_get_object (builder, "verify-hint");
        um->verify_hint = widget;

        g_object_unref (builder);

        return um;
}

void
um_password_dialog_free (UmPasswordDialog *um)
{
        gtk_widget_destroy (um->dialog);

        g_clear_object (&um->user);

        if (um->passwd_handler)
                passwd_destroy (um->passwd_handler);

        if (um->old_password_entry_timeout_id != 0) {
                g_source_remove (um->old_password_entry_timeout_id);
                um->old_password_entry_timeout_id = 0;
        }
 
        if (um->password_entry_timeout_id != 0) {
                g_source_remove (um->password_entry_timeout_id);
                um->password_entry_timeout_id = 0;
        }

        g_free (um);
}

void
um_password_dialog_set_user (UmPasswordDialog *um,
                             ActUser          *user)
{
        gboolean visible;

        if (um->user) {
                g_object_unref (um->user);
                um->user = NULL;
        }
        if (user) {
                um->user = g_object_ref (user);

                gtk_entry_set_text (GTK_ENTRY (um->password_entry), "");
                gtk_entry_set_text (GTK_ENTRY (um->verify_entry), "");
                gtk_entry_set_text (GTK_ENTRY (um->old_password_entry), "");

                gtk_entry_set_visibility (GTK_ENTRY (um->password_entry), FALSE);
                gtk_entry_set_visibility (GTK_ENTRY (um->verify_entry), FALSE);

                if (act_user_get_uid (um->user) == getuid ()) {
                        mode_change (um, ACT_USER_PASSWORD_MODE_REGULAR);
                        gtk_widget_hide (um->action_radio_box);

                        visible = (act_user_get_password_mode (user) != ACT_USER_PASSWORD_MODE_NONE);
                        gtk_widget_set_visible (um->old_password_label, visible);
                        gtk_widget_set_visible (um->old_password_entry, visible);
                        um->old_password_ok = !visible;
                }
                else {
                        mode_change (um, ACT_USER_PASSWORD_MODE_SET_AT_LOGIN);
                        gtk_widget_show (um->action_radio_box);

                        gtk_widget_hide (um->old_password_label);
                        gtk_widget_hide (um->old_password_entry);
                        um->old_password_ok = TRUE;
                }
                if (act_user_get_uid (um->user) == getuid()) {
                        if (um->passwd_handler != NULL)
                                passwd_destroy (um->passwd_handler);
                        um->passwd_handler = passwd_init ();
                }
        }
}

void
um_password_dialog_show (UmPasswordDialog *um,
                         GtkWindow        *parent)
{
        gtk_window_set_transient_for (GTK_WINDOW (um->dialog), parent);
        gtk_window_present (GTK_WINDOW (um->dialog));
        if (um->old_password_ok == FALSE)
                gtk_widget_grab_focus (um->old_password_entry);
        else
                gtk_widget_grab_focus (um->password_entry);
}

