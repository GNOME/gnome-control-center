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

#define UM_ACCOUNT_DIALOG_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), UM_TYPE_ACCOUNT_DIALOG, \
                                                                    UmAccountDialogClass))
#define UM_IS_ACCOUNT_DIALOG_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), UM_TYPE_ACCOUNT_DIALOG))
#define UM_ACCOUNT_DIALOG_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), UM_TYPE_ACCOUNT_DIALOG, \
                                                                      UmAccountDialogClass))

struct _UmAccountDialog {
        GtkDialog parent;
        GtkWidget *username_combo;
        GtkWidget *name_entry;
        GtkWidget *account_type_combo;

        gboolean valid_name;
        gboolean valid_username;

        UserCreatedCallback user_created_callback;
        gpointer            user_created_data;
};

struct _UmAccountDialogClass {
        GtkDialogClass parent_class;
};

G_DEFINE_TYPE (UmAccountDialog, um_account_dialog, GTK_TYPE_DIALOG);

static void
cancel_account_dialog (UmAccountDialog *um)
{
        gtk_widget_hide (GTK_WIDGET (um));
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

                        dialog = gtk_message_dialog_new (gtk_window_get_transient_for (GTK_WINDOW (um)),
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
accept_account_dialog (UmAccountDialog *um)
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

        gtk_widget_hide (GTK_WIDGET (um));
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

        gtk_dialog_set_response_sensitive (GTK_DIALOG (um), GTK_RESPONSE_OK,
                                           um->valid_name && um->valid_username);
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
        gtk_dialog_set_response_sensitive (GTK_DIALOG (um), GTK_RESPONSE_OK,
                                           um->valid_name && um->valid_username);

        if (!um->valid_name) {
                gtk_entry_set_text (GTK_ENTRY (entry), "");
                return;
        }

        generate_username_choices (name, GTK_LIST_STORE (model));

        gtk_combo_box_set_active (GTK_COMBO_BOX (um->username_combo), 0);
}

static void
um_account_dialog_init (UmAccountDialog *um)
{
        GtkBuilder *builder;
        GtkWidget *widget;
        const gchar *filename;
        GError *error = NULL;
        GtkDialog *dialog;
        GtkWidget *content;

        builder = gtk_builder_new ();

        filename = UIDIR "/account-dialog.ui";
        if (!g_file_test (filename, G_FILE_TEST_EXISTS))
                filename = "data/account-dialog.ui";
        if (!gtk_builder_add_from_file (builder, filename, &error)) {
                g_error ("%s", error->message);
                g_error_free (error);
                return;
        }

        dialog = GTK_DIALOG (um);
        content = gtk_dialog_get_content_area (dialog);
        gtk_container_set_border_width (GTK_CONTAINER (dialog), 5);
        gtk_window_set_resizable (GTK_WINDOW (dialog), FALSE);
        gtk_window_set_title (GTK_WINDOW (dialog), " ");
        gtk_window_set_icon_name (GTK_WINDOW (dialog), "system-users");

        gtk_dialog_add_button (dialog, _("Cancel"), GTK_RESPONSE_CANCEL);
        widget = gtk_dialog_add_button (dialog, _("Create"), GTK_RESPONSE_OK);
        gtk_widget_grab_default (widget);

        widget = (GtkWidget *) gtk_builder_get_object (builder, "account-dialog");
        gtk_container_add (GTK_CONTAINER (content), widget);

        widget = (GtkWidget *) gtk_builder_get_object (builder, "username-combo");
        g_signal_connect (widget, "changed",
                          G_CALLBACK (username_changed), um);
        um->username_combo = widget;

        widget = (GtkWidget *) gtk_builder_get_object (builder, "name-entry");
        g_signal_connect (widget, "notify::text",
                          G_CALLBACK (name_changed), um);
        um->name_entry = widget;

        widget = (GtkWidget *) gtk_builder_get_object (builder, "account-type-combo");
        um->account_type_combo = widget;
}

static void
um_account_dialog_response (GtkDialog *dialog,
                            gint response_id)
{
        UmAccountDialog *self = UM_ACCOUNT_DIALOG (dialog);

        switch (response_id) {
        case GTK_RESPONSE_OK:
                accept_account_dialog (self);
                break;
        case GTK_RESPONSE_CANCEL:
        case GTK_RESPONSE_DELETE_EVENT:
                cancel_account_dialog (self);
                break;
        }
}

static void
um_account_dialog_class_init (UmAccountDialogClass *klass)
{
        GtkDialogClass *dialog_class = GTK_DIALOG_CLASS (klass);

        dialog_class->response = um_account_dialog_response;
}

UmAccountDialog *
um_account_dialog_new (void)
{
        return g_object_new (UM_TYPE_ACCOUNT_DIALOG, NULL);
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

        gtk_window_set_modal (GTK_WINDOW (um), parent != NULL);
        gtk_window_set_transient_for (GTK_WINDOW (um), parent);
        gtk_window_present (GTK_WINDOW (um));
        gtk_widget_grab_focus (um->name_entry);

        um->valid_name = um->valid_username = TRUE;

        um->user_created_callback = user_created_callback;
        um->user_created_data = user_created_data;
}
