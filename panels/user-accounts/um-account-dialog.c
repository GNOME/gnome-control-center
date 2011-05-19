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

#include <glib.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include "um-account-dialog.h"
#include "um-user-manager.h"
#include "um-utils.h"

#define MAXNAMELEN  (UT_NAMESIZE - 1)

struct _UmAccountDialog {
        GtkWidget *dialog;
        GtkWidget *username_combo;
        GtkWidget *name_entry;
        GtkWidget *account_type_combo;
        GtkWidget *ok_button;

        gboolean valid_name;
        gboolean valid_username;

        UserCreatedCallback user_created_callback;
        gpointer            user_created_data;
};

static void
cancel_account_dialog (GtkButton       *button,
                       UmAccountDialog *um)
{
        gtk_widget_hide (um->dialog);
}

static void
create_user_done (UmUserManager   *manager,
                  GAsyncResult    *res,
                  UmAccountDialog *um)
{
        UmUser *user;
        GError *error;

        error = NULL;
        if (!um_user_manager_create_user_finish (manager, res, &user, &error)) {

                if (!g_error_matches (error, UM_USER_MANAGER_ERROR, UM_USER_MANAGER_ERROR_PERMISSION_DENIED)) {
                        GtkWidget *dialog;

                        dialog = gtk_message_dialog_new (gtk_window_get_transient_for (GTK_WINDOW (um->dialog)),
                                                         GTK_DIALOG_DESTROY_WITH_PARENT,
                                                         GTK_MESSAGE_ERROR,
                                                         GTK_BUTTONS_CLOSE,
                                                         _("Failed to create user"));

                        gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
                                                                  "%s", error->message);

                        g_signal_connect (G_OBJECT (dialog), "response",
                                          G_CALLBACK (gtk_widget_destroy), NULL);
                        gtk_window_present (GTK_WINDOW (dialog));
                }
                g_error_free (error);
        }
        else {
                um->user_created_callback (user, um->user_created_data);
        }
}

static void
accept_account_dialog (GtkButton       *button,
                       UmAccountDialog *um)
{
        UmUserManager *manager;
        const gchar *username;
        const gchar *name;
        gint account_type;
        GtkTreeModel *model;
        GtkTreeIter iter;

        name = gtk_entry_get_text (GTK_ENTRY (um->name_entry));
        username = gtk_combo_box_text_get_active_text (GTK_COMBO_BOX_TEXT (um->username_combo));
        model = gtk_combo_box_get_model (GTK_COMBO_BOX (um->account_type_combo));
        gtk_combo_box_get_active_iter (GTK_COMBO_BOX (um->account_type_combo), &iter);
        gtk_tree_model_get (model, &iter, 1, &account_type, -1);

        manager = um_user_manager_ref_default ();
        um_user_manager_create_user (manager,
                                     username,
                                     name,
                                     account_type,
                                     (GAsyncReadyCallback)create_user_done,
                                     um,
                                     NULL);
        g_object_unref (manager);

        gtk_widget_hide (um->dialog);
}

static void
username_changed (GtkComboBoxText *combo,
                  UmAccountDialog *um)
{
        const gchar *username;
        gchar *tip;
        GtkWidget *entry;

        username = gtk_combo_box_text_get_active_text (combo);

        um->valid_username = is_valid_username (username, &tip);

        gtk_widget_set_sensitive (um->ok_button, um->valid_name && um->valid_username);
        entry = gtk_bin_get_child (GTK_BIN (combo));

        if (tip) {
                set_entry_validation_error (GTK_ENTRY (entry), tip);
                g_free (tip);
        }
        else {
                clear_entry_validation_error (GTK_ENTRY (entry));
        }
}

static void
name_changed (GtkEntry        *name_entry,
              GParamSpec      *pspec,
              UmAccountDialog *um)
{
        GtkWidget *entry;
        GtkTreeModel *model;
        const char *name;

        entry = gtk_bin_get_child (GTK_BIN (um->username_combo));

        model = gtk_combo_box_get_model (GTK_COMBO_BOX (um->username_combo));
        gtk_list_store_clear (GTK_LIST_STORE (model));

        name = gtk_entry_get_text (GTK_ENTRY (name_entry));

        um->valid_name = is_valid_name (name);
        gtk_widget_set_sensitive (um->ok_button, um->valid_name && um->valid_username);

        if (!um->valid_name) {
                gtk_entry_set_text (GTK_ENTRY (entry), "");
                return;
        }

        generate_username_choices (name, GTK_LIST_STORE (model));

        gtk_combo_box_set_active (GTK_COMBO_BOX (um->username_combo), 0);
}

UmAccountDialog *
um_account_dialog_new (void)
{
        GtkBuilder *builder;
        GtkWidget *widget;
        UmAccountDialog *um;
        const gchar *filename;
        GError *error = NULL;

        builder = gtk_builder_new ();

        filename = UIDIR "/account-dialog.ui";
        if (!g_file_test (filename, G_FILE_TEST_EXISTS))
                filename = "data/account-dialog.ui";
        if (!gtk_builder_add_from_file (builder, filename, &error)) {
                g_error ("%s", error->message);
                g_error_free (error);
                return NULL;
        }

        um = g_new0 (UmAccountDialog, 1);

        widget = (GtkWidget *) gtk_builder_get_object (builder, "dialog");
        g_signal_connect (widget, "delete-event",
                          G_CALLBACK (gtk_widget_hide_on_delete), NULL);
        um->dialog = widget;

        widget = (GtkWidget *) gtk_builder_get_object (builder, "cancel-button");
        g_signal_connect (widget, "clicked",
                          G_CALLBACK (cancel_account_dialog), um);

        widget = (GtkWidget *) gtk_builder_get_object (builder, "ok-button");
        g_signal_connect (widget, "clicked",
                          G_CALLBACK (accept_account_dialog), um);
        gtk_widget_grab_default (widget);

        widget = (GtkWidget *) gtk_builder_get_object (builder, "username-combo");
        g_signal_connect (widget, "changed",
                          G_CALLBACK (username_changed), um);
        um->username_combo = widget;

        widget = (GtkWidget *) gtk_builder_get_object (builder, "name-entry");
        g_signal_connect (widget, "notify::text",
                          G_CALLBACK (name_changed), um);
        um->name_entry = widget;

        um->ok_button = (GtkWidget *) gtk_builder_get_object (builder, "ok-button");

        widget = (GtkWidget *) gtk_builder_get_object (builder, "account-type-combo");
        um->account_type_combo = widget;

        return um;
}

void
um_account_dialog_free (UmAccountDialog *um)
{
        gtk_widget_destroy (um->dialog);
        g_free (um);
}

void
um_account_dialog_show (UmAccountDialog     *um,
                        GtkWindow           *parent,
                        UserCreatedCallback  user_created_callback,
                        gpointer             user_created_data)
{
        GtkTreeModel *model;

        gtk_entry_set_text (GTK_ENTRY (um->name_entry), "");
        gtk_entry_set_text (GTK_ENTRY (gtk_bin_get_child (GTK_BIN (um->username_combo))), "");
        model = gtk_combo_box_get_model (GTK_COMBO_BOX (um->username_combo));
        gtk_list_store_clear (GTK_LIST_STORE (model));
        gtk_combo_box_set_active (GTK_COMBO_BOX (um->account_type_combo), 0);

        gtk_window_set_transient_for (GTK_WINDOW (um->dialog), parent);
        gtk_window_present (GTK_WINDOW (um->dialog));
        gtk_widget_grab_focus (um->name_entry);

        um->valid_name = um->valid_username = TRUE;

        um->user_created_callback = user_created_callback;
        um->user_created_data = user_created_data;
}


