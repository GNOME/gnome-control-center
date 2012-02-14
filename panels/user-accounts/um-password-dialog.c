/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright 2009-2010  Red Hat, Inc,
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
 */

#include "config.h"

#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <glib.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include "um-password-dialog.h"
#include "um-user-manager.h"
#include "cc-strength-bar.h"
#include "um-utils.h"
#include "run-passwd.h"

#define MIN_PASSWORD_LEN 6

struct _UmPasswordDialog {
        GtkWidget *dialog;
        GtkWidget *user_icon;
        GtkWidget *user_name;
        GtkWidget *action_label;
        GtkWidget *action_combo;
        GtkWidget *password_entry;
        GtkWidget *verify_entry;
        GtkWidget *strength_indicator;
        GtkWidget *strength_indicator_label;
        GtkWidget *normal_hint_entry;
        GtkWidget *normal_hint_label;
        GtkWidget *generate_button;
        GtkWidget *generate_menu;
        GtkWidget *show_password_button;
        GtkWidget *ok_button;

        UmUser *user;

        GtkWidget *old_password_label;
        GtkWidget *old_password_entry;
        gboolean   old_password_ok;

        PasswdHandler *passwd_handler;
};

static void
generate_clicked (GtkButton        *button,
                  UmPasswordDialog *um)
{
        gtk_menu_popup (GTK_MENU (um->generate_menu),
                        NULL, NULL,
                        (GtkMenuPositionFunc) popup_menu_below_button, um->generate_button,
                        0, gtk_get_current_event_time ());

        gtk_widget_set_has_tooltip (um->generate_button, FALSE);
}

static void
generate_draw (GtkWidget        *widget,
               cairo_t          *cr,
               UmPasswordDialog *um)
{
        if (!gtk_widget_is_sensitive (widget))
                return;

        down_arrow (gtk_widget_get_style_context (widget),
                    cr,
                    gtk_widget_get_allocated_width (widget) - 12,
                    gtk_widget_get_allocated_height (widget) - 12,
                    12, 12);
}

static void
activate_password_item (GtkMenuItem      *item,
                        UmPasswordDialog *um)
{
        const char *password;

        password = gtk_menu_item_get_label (item);

        gtk_entry_set_text (GTK_ENTRY (um->password_entry), password);
        gtk_entry_set_text (GTK_ENTRY (um->verify_entry), "");
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (um->show_password_button), TRUE);
        gtk_widget_grab_focus (um->verify_entry);
}

static void generate_passwords (UmPasswordDialog *um);

static void
activate_generate_item (GtkMenuItem      *item,
                        UmPasswordDialog *um)
{
        generate_passwords (um);
        generate_clicked (GTK_BUTTON (um->generate_button), um);
}

static void
on_generate_menu_unmap (GtkWidget        *menu,
                        UmPasswordDialog *um)
{
        gtk_widget_set_has_tooltip (um->generate_button, TRUE);
}

static void
generate_passwords (UmPasswordDialog *um)
{
        gint min_len, max_len;
        gchar *output, *err, *cmdline;
        gint status;
        GError *error;
        gint i;
        GtkWidget *item;

        min_len = 6;
        max_len = 12;

        if (um->generate_menu) {
                gtk_widget_destroy (um->generate_menu);
        }

        um->generate_menu = gtk_menu_new ();
        g_signal_connect (um->generate_menu, "unmap",
                          G_CALLBACK (on_generate_menu_unmap), um);

        cmdline = g_strdup_printf ("apg -n 6 -M SNC -m %d -x %d", min_len, max_len);
        error = NULL;
        output = NULL;
        err = NULL;
        if (!g_spawn_command_line_sync (cmdline, &output, &err, &status, &error)) {
                g_warning ("Failed to run apg: %s", error->message);
                g_error_free (error);
        } else if (WEXITSTATUS (status) == 0) {
                char **lines;
                lines = g_strsplit (output, "\n", 0);
                for (i = 0; lines[i]; i++) {
                        if (lines[i][0] == 0)
                                continue;

                        item = gtk_menu_item_new_with_label (lines[i]);
                        g_signal_connect (item, "activate",
                                          G_CALLBACK (activate_password_item), um);
                        gtk_widget_show (item);
                        gtk_menu_shell_append (GTK_MENU_SHELL (um->generate_menu), item);
                }
                g_strfreev (lines);
        } else {
                g_warning ("agp returned an error: %s", err);
        }

        g_free (cmdline);
        g_free (output);
        g_free (err);

        item = gtk_separator_menu_item_new ();
        gtk_widget_show (item);
        gtk_menu_shell_append (GTK_MENU_SHELL (um->generate_menu), item);

        item = gtk_menu_item_new_with_label (_("More choices..."));
        g_signal_connect (item, "activate",
                          G_CALLBACK (activate_generate_item), um);
        gtk_widget_show (item);
        gtk_menu_shell_append (GTK_MENU_SHELL (um->generate_menu), item);
}

/* This code is based on the Master Password dialog in Firefox
 * (pref-masterpass.js)
 * Original code triple-licensed under the MPL, GPL, and LGPL
 * so is license-compatible with this file
 */
static gdouble
compute_password_strength (const gchar *password)
{
        gint length;
        gint upper, lower, digit, misc;
        gint i;
        gdouble strength;

        length = strlen (password);
        upper = 0;
        lower = 0;
        digit = 0;
        misc = 0;

        for (i = 0; i < length ; i++) {
                if (g_ascii_isdigit (password[i]))
                        digit++;
                else if (g_ascii_islower (password[i]))
                        lower++;
                else if (g_ascii_isupper (password[i]))
                        upper++;
                else
                        misc++;
        }

        if (length > 5)
                length = 5;

        if (digit > 3)
                digit = 3;

        if (upper > 3)
                upper = 3;

        if (misc > 3)
                misc = 3;

        strength = ((length * 0.1) - 0.2) +
                    (digit * 0.1) +
                    (misc * 0.15) +
                    (upper * 0.1);

        strength = CLAMP (strength, 0.0, 1.0);

        return strength;
}

static void
finish_password_change (UmPasswordDialog *um)
{
        gtk_widget_hide (um->dialog);

        gtk_entry_set_text (GTK_ENTRY (um->password_entry), " ");
        gtk_entry_set_text (GTK_ENTRY (um->verify_entry), "");
        gtk_entry_set_text (GTK_ENTRY (um->normal_hint_entry), "");
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
        GtkTreeModel *model;
        GtkTreeIter iter;
        gint mode;
        const gchar *hint;
        const gchar *password;

        model = gtk_combo_box_get_model (GTK_COMBO_BOX (um->action_combo));
        gtk_combo_box_get_active_iter (GTK_COMBO_BOX (um->action_combo), &iter);
        gtk_tree_model_get (model, &iter, 1, &mode, -1);

        password = gtk_entry_get_text (GTK_ENTRY (um->password_entry));
        hint = gtk_entry_get_text (GTK_ENTRY (um->normal_hint_entry));

        if (mode == 0 && um_user_get_uid (um->user) == getuid ()) {
                GdkDisplay *display;
                GdkCursor *cursor;

                /* When setting a password for the current user,
                 * use passwd directly, to preserve the audit trail
                 * and to e.g. update the keyring password.
                 */
                passwd_change_password (um->passwd_handler, password, (PasswdCallback) password_changed_cb, um);
                gtk_widget_set_sensitive (um->dialog, FALSE);
                display = gtk_widget_get_display (um->dialog);
                cursor = gdk_cursor_new_for_display (display, GDK_WATCH);
                gdk_window_set_cursor (gtk_widget_get_window (um->dialog), cursor);
                gdk_display_flush (display);
                g_object_unref (cursor);
        }
        else {
                um_user_set_password (um->user, mode, password, hint);
                finish_password_change (um);
        }
}

static void
update_sensitivity (UmPasswordDialog *um)
{
        const gchar *password, *verify;
        const gchar *old_password;
        const gchar *tooltip;
        gboolean can_change;

        password = gtk_entry_get_text (GTK_ENTRY (um->password_entry));
        verify = gtk_entry_get_text (GTK_ENTRY (um->verify_entry));
        old_password = gtk_entry_get_text (GTK_ENTRY (um->old_password_entry));

        /* TODO: configurable policies for acceptable passwords */
        if (strlen (password) < MIN_PASSWORD_LEN) {
                can_change = FALSE;
                if (password[0] == '\0') {
                        tooltip = _("You need to enter a new password");
                }
                else {
                        tooltip = _("The new password is too short");
                }
        }
        else if (strcmp (password, verify) != 0) {
                can_change = FALSE;
                if (verify[0] == '\0') {
                        tooltip = _("You need to confirm the password");
                }
                else {
                        tooltip = _("The passwords do not match");
                }
        }
        else if (!um->old_password_ok) {
                can_change = FALSE;
                if (old_password[0] == '\0') {
                        tooltip = _("You need to enter your current password");
                }
                else {
                        tooltip = _("The current password is not correct");
                }
        }
        else {
                can_change = TRUE;
                tooltip = NULL;
        }

        gtk_widget_set_sensitive (um->ok_button, can_change);
        gtk_widget_set_tooltip_text (um->ok_button, tooltip);
}

static void
action_changed (GtkComboBox      *combo,
                UmPasswordDialog *um)
{
        gint active;

        active = gtk_combo_box_get_active (combo);
        if (active == 0) {
                gtk_widget_set_sensitive (um->password_entry, TRUE);
                gtk_widget_set_sensitive (um->generate_button, TRUE);
                gtk_widget_set_has_tooltip (um->generate_button, TRUE);
                gtk_widget_set_sensitive (um->verify_entry, TRUE);
                gtk_widget_set_sensitive (um->old_password_entry, TRUE);
                gtk_widget_set_sensitive (um->normal_hint_entry, TRUE);
                gtk_widget_set_sensitive (um->normal_hint_label, TRUE);
                gtk_widget_set_sensitive (um->strength_indicator_label, TRUE);
                gtk_widget_set_sensitive (um->show_password_button, TRUE);

                update_sensitivity (um);
        }
        else {
                gtk_widget_set_sensitive (um->password_entry, FALSE);
                gtk_widget_set_sensitive (um->generate_button, FALSE);
                gtk_widget_set_has_tooltip (um->generate_button, FALSE);
                gtk_widget_set_sensitive (um->verify_entry, FALSE);
                gtk_widget_set_sensitive (um->old_password_entry, FALSE);
                gtk_widget_set_sensitive (um->normal_hint_entry, FALSE);
                gtk_widget_set_sensitive (um->normal_hint_label, FALSE);
                gtk_widget_set_sensitive (um->strength_indicator_label, FALSE);
                gtk_widget_set_sensitive (um->show_password_button, FALSE);
                gtk_widget_set_sensitive (um->ok_button, TRUE);
        }
}

static void
show_password_toggled (GtkToggleButton  *button,
                       UmPasswordDialog *um)
{
        gboolean active;

        active = gtk_toggle_button_get_active (button);
        gtk_entry_set_visibility (GTK_ENTRY (um->password_entry), active);
        gtk_entry_set_visibility (GTK_ENTRY (um->verify_entry), active);
}

static void
update_password_strength (UmPasswordDialog *um)
{
        const gchar *password;
        gdouble strength;
        const gchar *hint;

        password = gtk_entry_get_text (GTK_ENTRY (um->password_entry));

        strength = compute_password_strength (password);

        if (strlen (password) < MIN_PASSWORD_LEN) {
                strength = 0.0;
                if (password[0] == '\0')
                        hint = "";
                else
                        hint = C_("Password strength", "Too short");
        }
        else if (strength < 0.50)
                hint = C_("Password strength", "Weak");
        else if (strength < 0.75)
                hint = C_("Password strength", "Fair");
        else if (strength < 0.90)
                hint = C_("Password strength", "Good");
        else
                hint = C_("Password strength", "Strong");

        cc_strength_bar_set_fraction (CC_STRENGTH_BAR (um->strength_indicator), strength);
        gtk_label_set_label (GTK_LABEL (um->strength_indicator_label), hint);
}

static void
password_entry_changed (GtkEntry         *entry,
                        GParamSpec       *pspec,
                        UmPasswordDialog *um)
{
        update_password_strength (um);
        update_sensitivity (um);
}

static void
verify_entry_changed (GtkEntry         *entry,
                      GParamSpec       *pspec,
                      UmPasswordDialog *um)
{
        clear_entry_validation_error (GTK_ENTRY (entry));
        update_password_strength (um);
        update_sensitivity (um);
}

static gboolean
verify_entry_focus_out (GtkWidget        *entry,
                        GdkEventFocus    *event,
                        UmPasswordDialog *um)
{
        const char *password;
        const char *verify;

        password = gtk_entry_get_text (GTK_ENTRY (um->password_entry));
        verify = gtk_entry_get_text (GTK_ENTRY (um->verify_entry));

        if (strlen (password) > 0 && strlen (verify) > 0) {
                if (strcmp (password, verify) != 0) {
                        set_entry_validation_error (GTK_ENTRY (um->verify_entry),
                                                    _("Passwords do not match"));
                }
                else {
                        clear_entry_validation_error (GTK_ENTRY (um->verify_entry));
                }
        }

        return FALSE;
}

static void
entry_size_changed (GtkWidget     *entry,
                    GtkAllocation *allocation,
                    GtkWidget     *label)
{
        gtk_widget_set_size_request (label, allocation->width, -1);
}

static void
auth_cb (PasswdHandler    *handler,
         GError           *error,
         UmPasswordDialog *um)
{
        if (error) {
                um->old_password_ok = FALSE;
                set_entry_validation_error (GTK_ENTRY (um->old_password_entry),
                                            _("Wrong password"));
        }
        else {
                um->old_password_ok = TRUE;
                clear_entry_validation_error (GTK_ENTRY (um->old_password_entry));
        }

        update_sensitivity (um);
}

static gboolean
old_password_entry_focus_out (GtkWidget        *entry,
                              GdkEventFocus    *event,
                              UmPasswordDialog *um)
{
        const char *text;

        text = gtk_entry_get_text (GTK_ENTRY (entry));
        if (strlen (text) > 0) {
                passwd_authenticate (um->passwd_handler, text,
                                     (PasswdCallback)auth_cb, um);
        }

        return FALSE;
}

static void
old_password_entry_activate (GtkWidget        *entry,
                             UmPasswordDialog *um)
{
        const char *text;

        text = gtk_entry_get_text (GTK_ENTRY (entry));
        if (strlen (text) > 0) {
                passwd_authenticate (um->passwd_handler, text,
                                     (PasswdCallback)auth_cb, um);
        }
}


static void
old_password_entry_changed (GtkEntry         *entry,
                            GParamSpec       *pspec,
                            UmPasswordDialog *um)
{
        clear_entry_validation_error (GTK_ENTRY (entry));
        um->old_password_ok = FALSE;
        update_sensitivity (um);
}

void
um_password_dialog_set_privileged (UmPasswordDialog *um,
                                   gboolean          privileged)
{
        if (privileged) {
                gtk_widget_set_visible (um->action_label, TRUE);
                gtk_widget_set_visible (um->action_combo, TRUE);
        }
        else {
                gtk_combo_box_set_active (GTK_COMBO_BOX (um->action_combo), 0);
                gtk_widget_set_visible (um->action_label, FALSE);
                gtk_widget_set_visible (um->action_combo, FALSE);
        }
}

UmPasswordDialog *
um_password_dialog_new (void)
{
        GtkBuilder *builder;
        GError *error;
        const gchar *filename;
        UmPasswordDialog *um;
        GtkWidget *widget;
        const char *old_label;
        char *label;
        gint len;

        builder = gtk_builder_new ();

        error = NULL;
        filename = UIDIR "/password-dialog.ui";
        if (!g_file_test (filename, G_FILE_TEST_EXISTS))
                filename = "data/password-dialog.ui";
        if (!gtk_builder_add_from_file (builder, filename, &error)) {
                g_error ("%s", error->message);
                g_error_free (error);
                return NULL;
        }

        um = g_new0 (UmPasswordDialog, 1);

        um->action_label = (GtkWidget *) gtk_builder_get_object (builder, "action-label");
        widget = (GtkWidget *) gtk_builder_get_object (builder, "action-combo");
        g_signal_connect (widget, "changed",
                          G_CALLBACK (action_changed), um);
        um->action_combo = widget;

        widget = (GtkWidget *) gtk_builder_get_object (builder, "dialog");
        g_signal_connect (widget, "delete-event",
                          G_CALLBACK (gtk_widget_hide_on_delete), NULL);
        um->dialog = widget;

        um->user_icon = (GtkWidget *) gtk_builder_get_object (builder, "user-icon");
        um->user_name = (GtkWidget *) gtk_builder_get_object (builder, "user-name");

        widget = (GtkWidget *) gtk_builder_get_object (builder, "cancel-button");
        g_signal_connect (widget, "clicked",
                          G_CALLBACK (cancel_password_dialog), um);

        widget = (GtkWidget *) gtk_builder_get_object (builder, "ok-button");
        g_signal_connect (widget, "clicked",
                          G_CALLBACK (accept_password_dialog), um);
        gtk_widget_grab_default (widget);
        um->ok_button = widget;

        widget = (GtkWidget *) gtk_builder_get_object (builder, "password-normal-strength-hints-label");
        old_label = gtk_label_get_label (GTK_LABEL (widget));
        label = g_strdup_printf ("<a href=\"%s\">%s</a>",
                                 "help:gnome-help/user-goodpassword",
                                 old_label);
        gtk_label_set_markup (GTK_LABEL (widget), label);
        g_free (label);

        widget = (GtkWidget *) gtk_builder_get_object (builder, "show-password-checkbutton");
        g_signal_connect (widget, "toggled",
                          G_CALLBACK (show_password_toggled), um);
        um->show_password_button = widget;

        widget = (GtkWidget *) gtk_builder_get_object (builder, "password-entry");
        g_signal_connect (widget, "notify::text",
                          G_CALLBACK (password_entry_changed), um);
        gtk_entry_set_visibility (GTK_ENTRY (widget), FALSE);

        um->password_entry = widget;

        widget = (GtkWidget *) gtk_builder_get_object (builder, "old-password-entry");
        g_signal_connect_after (widget, "focus-out-event",
                                G_CALLBACK (old_password_entry_focus_out), um);
        g_signal_connect (widget, "notify::text",
                          G_CALLBACK (old_password_entry_changed), um);
        g_signal_connect (widget, "activate",
                          G_CALLBACK (old_password_entry_activate), um);
        um->old_password_entry = widget;
        um->old_password_label = (GtkWidget *) gtk_builder_get_object (builder, "old-password-label");

        widget = (GtkWidget *) gtk_builder_get_object (builder, "verify-entry");
        g_signal_connect (widget, "notify::text",
                          G_CALLBACK (verify_entry_changed), um);
        g_signal_connect_after (widget, "focus-out-event",
                                G_CALLBACK (verify_entry_focus_out), um);
        um->verify_entry = widget;

        len = 0;
        len = MAX (len, strlen (C_("Password strength", "Too short")));
        len = MAX (len, strlen (C_("Password strength", "Weak")));
        len = MAX (len, strlen (C_("Password strength", "Fair")));
        len = MAX (len, strlen (C_("Password strength", "Good")));
        len = MAX (len, strlen (C_("Password strength", "Strong")));
        len += 2;

        widget = (GtkWidget *) gtk_builder_get_object (builder, "strength-indicator-label");
        gtk_label_set_width_chars (GTK_LABEL (widget), len);


        widget = (GtkWidget *) gtk_builder_get_object (builder, "generate-again-button");
        g_signal_connect (widget, "clicked",
                          G_CALLBACK (generate_clicked), um);
#if 0
        g_signal_connect (widget, "state-changed",
                          G_CALLBACK (generate_state_changed), um);
#endif
        um->generate_button = widget;
        g_signal_connect_after (widget, "draw",
                                G_CALLBACK (generate_draw), um);

        um->normal_hint_entry = (GtkWidget *) gtk_builder_get_object (builder, "normal-hint-entry");

        /* Label size hack.
         * This only sort-of works because the dialog is non-resizable.
         */
        widget = (GtkWidget *)gtk_builder_get_object (builder, "password-normal-hint-description-label");
        g_signal_connect (um->normal_hint_entry, "size-allocate",
                          G_CALLBACK (entry_size_changed), widget);
        um->normal_hint_label = widget;

        um->strength_indicator = (GtkWidget *) gtk_builder_get_object (builder, "strength-indicator");

        um->strength_indicator_label = (GtkWidget *) gtk_builder_get_object (builder, "strength-indicator-label");

        g_object_unref (builder);

        generate_passwords (um);

        return um;
}

void
um_password_dialog_free (UmPasswordDialog *um)
{
        gtk_widget_destroy (um->dialog);

        if (um->user)
                g_object_unref (um->user);

        if (um->passwd_handler)
                passwd_destroy (um->passwd_handler);

        g_free (um);
}

static gboolean
visible_func (GtkTreeModel     *model,
              GtkTreeIter      *iter,
              UmPasswordDialog *um)
{
        if (um->user) {
                gint mode; 
                gboolean locked = um_user_get_locked (um->user);

                gtk_tree_model_get (model, iter, 1, &mode, -1);

                if (mode == 3 && locked)
                        return FALSE;

                if (mode == 4 && !locked)
                        return FALSE;

                return TRUE;
        }

        return TRUE;
}

void
um_password_dialog_set_user (UmPasswordDialog *um,
                             UmUser           *user)
{
        GdkPixbuf *pixbuf;
        GtkTreeModel *model;

        if (um->user) {
                g_object_unref (um->user);
                um->user = NULL;
        }
        if (user) {
                um->user = g_object_ref (user);

                pixbuf = um_user_render_icon (user, FALSE, 48);
                gtk_image_set_from_pixbuf (GTK_IMAGE (um->user_icon), pixbuf);
                g_object_unref (pixbuf);

                gtk_label_set_label (GTK_LABEL (um->user_name),
                                     um_user_get_real_name (user));

                gtk_entry_set_text (GTK_ENTRY (um->password_entry), "");
                gtk_entry_set_text (GTK_ENTRY (um->verify_entry), "");
                gtk_entry_set_text (GTK_ENTRY (um->normal_hint_entry), "");
                gtk_entry_set_text (GTK_ENTRY (um->old_password_entry), "");
                gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (um->show_password_button), FALSE);
                if (um_user_get_uid (um->user) == getuid()) {
                        gtk_widget_show (um->old_password_label);
                        gtk_widget_show (um->old_password_entry);
                        if (um->passwd_handler != NULL)
                                passwd_destroy (um->passwd_handler);
                        um->passwd_handler = passwd_init ();
                        um->old_password_ok = FALSE;
                }
                else {
                        gtk_widget_hide (um->old_password_label);
                        gtk_widget_hide (um->old_password_entry);
                        um->old_password_ok = TRUE;
                }
        }

        model = gtk_combo_box_get_model (GTK_COMBO_BOX (um->action_combo));
        if (!GTK_IS_TREE_MODEL_FILTER (model)) {
                model = gtk_tree_model_filter_new (model, NULL);
                gtk_combo_box_set_model (GTK_COMBO_BOX (um->action_combo), model);
                gtk_tree_model_filter_set_visible_func (GTK_TREE_MODEL_FILTER (model),
                                                        (GtkTreeModelFilterVisibleFunc) visible_func,
                                                        um, NULL);
        }

        gtk_tree_model_filter_refilter (GTK_TREE_MODEL_FILTER (model));
        gtk_combo_box_set_active (GTK_COMBO_BOX (um->action_combo), 0);
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

