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

static void   dialog_validate     (UmAccountDialog *self);

#define UM_ACCOUNT_DIALOG_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), UM_TYPE_ACCOUNT_DIALOG, \
                                                                    UmAccountDialogClass))
#define UM_IS_ACCOUNT_DIALOG_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), UM_TYPE_ACCOUNT_DIALOG))
#define UM_ACCOUNT_DIALOG_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), UM_TYPE_ACCOUNT_DIALOG, \
                                                                      UmAccountDialogClass))

typedef enum {
        UM_LOCAL,
        UM_ENTERPRISE,
        NUM_MODES
} UmAccountMode;

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
                                         message);

        gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
                                                  "%s", error->message);

        g_signal_connect (dialog, "response", G_CALLBACK (gtk_widget_destroy), NULL);
        gtk_window_present (GTK_WINDOW (dialog));
}

static void
begin_action (UmAccountDialog *self)
{
        gtk_widget_set_sensitive (self->container_widget, FALSE);
        gtk_dialog_set_response_sensitive (GTK_DIALOG (self), GTK_RESPONSE_OK, FALSE);

        gtk_widget_show (GTK_WIDGET (self->spinner));
        gtk_spinner_start (self->spinner);
}

static void
finish_action (UmAccountDialog *self)
{
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
                if (!g_error_matches (error, UM_USER_MANAGER_ERROR, UM_USER_MANAGER_ERROR_PERMISSION_DENIED))
                       show_error_dialog (self, _("Failed to add account"), error);
                g_error_free (error);
                gtk_widget_grab_focus (self->local_name);
        } else {
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
local_area_init (UmAccountDialog *self,
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

static void
dialog_validate (UmAccountDialog *self)
{
        gboolean valid = FALSE;

        switch (self->mode) {
        case UM_LOCAL:
                valid = local_validate (self);
                break;
        case UM_ENTERPRISE:
                /* TODO: Implement */
                valid = FALSE;
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

        local_area_init (self, builder);
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
                        /* TODO: */
                        g_assert_not_reached ();
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

        G_OBJECT_CLASS (um_account_dialog_parent_class)->dispose (obj);
}

static void
um_account_dialog_finalize (GObject *obj)
{
        UmAccountDialog *self = UM_ACCOUNT_DIALOG (obj);

        if (self->cancellable)
                g_object_unref (self->cancellable);

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
