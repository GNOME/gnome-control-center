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

#include "um-user-panel.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>

#include <glib.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <polkit/polkit.h>

#ifdef HAVE_CHEESE
#include <gst/gst.h>
#endif /* HAVE_CHEESE */

#include "shell/cc-editable-entry.h"

#include "um-user.h"
#include "um-user-manager.h"

#include "cc-strength-bar.h"
#include "um-editable-button.h"
#include "um-editable-combo.h"

#include "um-account-dialog.h"
#include "cc-language-chooser.h"
#include "um-password-dialog.h"
#include "um-photo-dialog.h"
#include "um-fingerprint-dialog.h"
#include "um-utils.h"

#include "cc-common-language.h"

G_DEFINE_DYNAMIC_TYPE (UmUserPanel, um_user_panel, CC_TYPE_PANEL)

#define UM_USER_PANEL_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), UM_TYPE_USER_PANEL, UmUserPanelPrivate))

struct _UmUserPanelPrivate {
        UmUserManager *um;
        GtkBuilder *builder;

        GtkWidget *main_box;
        GPermission *permission;
        GtkWidget *language_chooser;

        UmAccountDialog *account_dialog;
        UmPasswordDialog *password_dialog;
        UmPhotoDialog *photo_dialog;
};

static GtkWidget *
get_widget (UmUserPanelPrivate *d, const char *name)
{
        return (GtkWidget *)gtk_builder_get_object (d->builder, name);
}

enum {
        USER_COL,
        FACE_COL,
        NAME_COL,
        USER_ROW_COL,
        TITLE_COL,
        HEADING_ROW_COL,
        SORT_KEY_COL,
        AUTOLOGIN_COL,
        NUM_USER_LIST_COLS
};

static UmUser *
get_selected_user (UmUserPanelPrivate *d)
{
        GtkTreeView *tv;
        GtkTreeIter iter;
        GtkTreeSelection *selection;
        GtkTreeModel *model;
        UmUser *user;

        tv = (GtkTreeView *)get_widget (d, "list-treeview");
        selection = gtk_tree_view_get_selection (tv);

        if (gtk_tree_selection_get_selected (selection, &model, &iter)) {
                gtk_tree_model_get (model, &iter, USER_COL, &user, -1);
                return user;
        }

        return NULL;
}

static char *
get_name_col_str (UmUser *user)
{
        return g_markup_printf_escaped ("<b>%s</b>\n<i>%s</i>",
                                        um_user_get_display_name (user),
                                        um_account_type_get_name (um_user_get_account_type (user)));
}

static void
user_added (UmUserManager *um, UmUser *user, UmUserPanelPrivate *d)
{
        GtkWidget *widget;
        GtkTreeModel *model;
        GtkListStore *store;
        GtkTreeIter iter;
        GtkTreeIter dummy;
        GdkPixbuf *pixbuf;
        gchar *text;
        GtkTreeSelection *selection;
        gint sort_key;
        gboolean is_autologin;

        g_debug ("user added: %d %s\n", um_user_get_uid (user), um_user_get_real_name (user));
        widget = get_widget (d, "list-treeview");
        model = gtk_tree_view_get_model (GTK_TREE_VIEW (widget));
        store = GTK_LIST_STORE (model);
        selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (widget));

        pixbuf = um_user_render_icon (user, TRUE, 48);
        text = get_name_col_str (user);

        is_autologin = um_user_get_automatic_login (user);

        if (um_user_get_uid (user) == getuid ()) {
                sort_key = 1;
        }
        else {
                sort_key = 3;
        }
        gtk_list_store_append (store, &iter);

        gtk_list_store_set (store, &iter,
                            USER_COL, user,
                            FACE_COL, pixbuf,
                            NAME_COL, text,
                            USER_ROW_COL, TRUE,
                            TITLE_COL, NULL,
                            HEADING_ROW_COL, FALSE,
                            SORT_KEY_COL, sort_key,
                            AUTOLOGIN_COL, is_autologin,
                            -1);
        g_object_unref (pixbuf);
        g_free (text);

        if (sort_key == 1 &&
            !gtk_tree_selection_get_selected (selection, &model, &dummy)) {
                gtk_tree_selection_select_iter (selection, &iter);
        }
}

static void
get_previous_user_row (GtkTreeModel *model,
                       GtkTreeIter  *iter,
                       GtkTreeIter  *prev)
{
        GtkTreePath *path;
        UmUser *user;

        path = gtk_tree_model_get_path (model, iter);
        while (gtk_tree_path_prev (path)) {
                gtk_tree_model_get_iter (model, prev, path);
                gtk_tree_model_get (model, prev, USER_COL, &user, -1);
                if (user) {
                        g_object_unref (user);
                        break;
                }
        }
        gtk_tree_path_free (path);
}

static gboolean
get_next_user_row (GtkTreeModel *model,
                   GtkTreeIter  *iter,
                   GtkTreeIter  *next)
{
        UmUser *user;

        *next = *iter;
        while (gtk_tree_model_iter_next (model, next)) {
                gtk_tree_model_get (model, next, USER_COL, &user, -1);
                if (user) {
                        g_object_unref (user);
                        return TRUE;
                }
        }

        return FALSE;
}

static void
user_removed (UmUserManager *um, UmUser *user, UmUserPanelPrivate *d)
{
        GtkTreeView *tv;
        GtkTreeModel *model;
        GtkTreeSelection *selection;
        GtkListStore *store;
        GtkTreeIter iter, next;
        UmUser *u;

        g_debug ("user removed: %s\n", um_user_get_user_name (user));
        tv = (GtkTreeView *)get_widget (d, "list-treeview");
        selection = gtk_tree_view_get_selection (tv);
        model = gtk_tree_view_get_model (tv);
        store = GTK_LIST_STORE (model);
        if (gtk_tree_model_get_iter_first (model, &iter)) {
                do {
                        gtk_tree_model_get (model, &iter, USER_COL, &u, -1);

                        if (u != NULL) {
                                if (um_user_get_uid (user) == um_user_get_uid (u)) {
                                        if (!get_next_user_row (model, &iter, &next))
                                                get_previous_user_row (model, &iter, &next);
                                        gtk_list_store_remove (store, &iter);
                                        gtk_tree_selection_select_iter (selection, &next);
                                        g_object_unref (u);
                                        break;
                                }
                                g_object_unref (u);
                        }
                } while (gtk_tree_model_iter_next (model, &iter));
        }
}

static void show_user (UmUser *user, UmUserPanelPrivate *d);

static void
user_changed (UmUserManager *um, UmUser *user, UmUserPanelPrivate *d)
{
        GtkTreeView *tv;
        GtkTreeSelection *selection;
        GtkTreeModel *model;
        GtkTreeIter iter;
        UmUser *current;
        GdkPixbuf *pixbuf;
        char *text;
        gboolean is_autologin;

        tv = (GtkTreeView *)get_widget (d, "list-treeview");
        model = gtk_tree_view_get_model (tv);
        selection = gtk_tree_view_get_selection (tv);

        gtk_tree_model_get_iter_first (model, &iter);
        do {
                gtk_tree_model_get (model, &iter, USER_COL, &current, -1);
                if (current == user) {
                        pixbuf = um_user_render_icon (user, TRUE, 48);
                        text = get_name_col_str (user);
                        is_autologin = um_user_get_automatic_login (user);

                        gtk_list_store_set (GTK_LIST_STORE (model), &iter,
                                            USER_COL, user,
                                            FACE_COL, pixbuf,
                                            NAME_COL, text,
                                            AUTOLOGIN_COL, is_autologin,
                                            -1);
                        g_object_unref (pixbuf);
                        g_free (text);
                        g_object_unref (current);

                        break;
                }
                if (current)
                        g_object_unref (current);

        } while (gtk_tree_model_iter_next (model, &iter));

        if (gtk_tree_selection_get_selected (selection, &model, &iter)) {
                gtk_tree_model_get (model, &iter, USER_COL, &current, -1);

                if (current == user) {
                        show_user (user, d);
                }
                if (current)
                        g_object_unref (current);
        }
}

static void
select_created_user (UmUser *user, UmUserPanelPrivate *d)
{
        GtkTreeView *tv;
        GtkTreeModel *model;
        GtkTreeSelection *selection;
        GtkTreeIter iter;
        UmUser *current;
        GtkTreePath *path;

        tv = (GtkTreeView *)get_widget (d, "list-treeview");
        model = gtk_tree_view_get_model (tv);
        selection = gtk_tree_view_get_selection (tv);

        gtk_tree_model_get_iter_first (model, &iter);
        do {
                gtk_tree_model_get (model, &iter, USER_COL, &current, -1);
                if (user == current) {
                        path = gtk_tree_model_get_path (model, &iter);
                        gtk_tree_view_scroll_to_cell (tv, path, NULL, FALSE, 0.0, 0.0);
                        gtk_tree_selection_select_path (selection, path);
                        gtk_tree_path_free (path);
                        g_object_unref (current);
                        break;
                }
                if (current)
                        g_object_unref (current);
        } while (gtk_tree_model_iter_next (model, &iter));
}

static void
add_user (GtkButton *button, UmUserPanelPrivate *d)
{
        um_account_dialog_show (d->account_dialog,
                                GTK_WINDOW (gtk_widget_get_toplevel (d->main_box)),
                                (UserCreatedCallback)select_created_user, d);
}

static void
delete_user_done (UmUserManager     *manager,
                  GAsyncResult      *res,
                  UmUserPanelPrivate *d)
{
        GError *error;

        error = NULL;
        if (!um_user_manager_delete_user_finish (manager, res, &error)) {
                if (!g_error_matches (error, UM_USER_MANAGER_ERROR, UM_USER_MANAGER_ERROR_PERMISSION_DENIED)) {
                        GtkWidget *dialog;

                        dialog = gtk_message_dialog_new (GTK_WINDOW (gtk_widget_get_toplevel (d->main_box)),
                                                         GTK_DIALOG_DESTROY_WITH_PARENT,
                                                         GTK_MESSAGE_ERROR,
                                                         GTK_BUTTONS_CLOSE,
                                                         _("Failed to delete user"));

                        gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
                                                                  "%s", error->message);

                        g_signal_connect (G_OBJECT (dialog), "response",
                                          G_CALLBACK (gtk_widget_destroy), NULL);
                        gtk_window_present (GTK_WINDOW (dialog));
                }
                g_error_free (error);
        }
}

static void
delete_user_response (GtkWidget         *dialog,
                      gint               response_id,
                      UmUserPanelPrivate *d)
{
        UmUser *user;
        gboolean remove_files;

        gtk_widget_destroy (dialog);

        if (response_id == GTK_RESPONSE_CANCEL) {
                return;
        }
        else if (response_id == GTK_RESPONSE_NO) {
                remove_files = TRUE;
        }
        else {
                remove_files = FALSE;
        }

        user = get_selected_user (d);

        um_user_manager_delete_user (d->um,
                                     user,
                                     remove_files,
                                     (GAsyncReadyCallback)delete_user_done,
                                     d,
                                     NULL);

        g_object_unref (user);
}

static void
delete_user (GtkButton *button, UmUserPanelPrivate *d)
{
        UmUser *user;
        GtkWidget *dialog;

        user = get_selected_user (d);
        if (user == NULL) {
                return;
        }
        else if (um_user_get_uid (user) == getuid ()) {
                dialog = gtk_message_dialog_new (GTK_WINDOW (gtk_widget_get_toplevel (d->main_box)),
                                                 0,
                                                 GTK_MESSAGE_INFO,
                                                 GTK_BUTTONS_CLOSE,
                                                 _("You cannot delete your own account."));
                g_signal_connect (dialog, "response",
                                  G_CALLBACK (gtk_widget_destroy), NULL);
        }
        else if (um_user_is_logged_in (user)) {
                dialog = gtk_message_dialog_new (GTK_WINDOW (gtk_widget_get_toplevel (d->main_box)),
                                                 0,
                                                 GTK_MESSAGE_INFO,
                                                 GTK_BUTTONS_CLOSE,
                                                 _("%s is still logged in"),
                                                um_user_get_real_name (user));

                gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
                                                          _("Deleting a user while they are logged in can leave the system in an inconsistent state."));
                g_signal_connect (dialog, "response",
                                  G_CALLBACK (gtk_widget_destroy), NULL);
        }
        else {
                dialog = gtk_message_dialog_new (GTK_WINDOW (gtk_widget_get_toplevel (d->main_box)),
                                                 0,
                                                 GTK_MESSAGE_QUESTION,
                                                 GTK_BUTTONS_NONE,
                                                 _("Do you want to keep %s's files?"),
                                                um_user_get_real_name (user));

                gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
                                                          _("It is possible to keep the home directory, mail spool and temporary files around when deleting a user account."));

                gtk_dialog_add_buttons (GTK_DIALOG (dialog),
                                        _("_Delete Files"), GTK_RESPONSE_NO,
                                        _("_Keep Files"), GTK_RESPONSE_YES,
                                        _("_Cancel"), GTK_RESPONSE_CANCEL,
                                        NULL);

                gtk_window_set_icon_name (GTK_WINDOW (dialog), "system-users");

                g_signal_connect (dialog, "response",
                                  G_CALLBACK (delete_user_response), d);
        }

        g_signal_connect (dialog, "close",
                          G_CALLBACK (gtk_widget_destroy), NULL);

        gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);

        gtk_window_present (GTK_WINDOW (dialog));

        g_object_unref (user);
}

static const gchar *
get_invisible_text (void)
{
   GtkWidget *entry;
   gunichar invisible_char;
   static gchar invisible_text[40];
   gchar *p;
   gint i;

   entry = gtk_entry_new ();
   invisible_char = gtk_entry_get_invisible_char (GTK_ENTRY (entry));
   if (invisible_char == 0)
     invisible_char = 0x2022;

   g_object_ref_sink (entry);
   g_object_unref (entry);

   /* five bullets */
   p = invisible_text;
   for (i = 0; i < 5; i++)
     p += g_unichar_to_utf8 (invisible_char, p);
   *p = 0;

   return invisible_text;
}

static const gchar *
get_password_mode_text (UmUser *user)
{
        const gchar *text;

        if (um_user_get_locked (user)) {
                text = C_("Password mode", "Account disabled");
        }
        else {
                switch (um_user_get_password_mode (user)) {
                case UM_PASSWORD_MODE_REGULAR:
                        text = get_invisible_text ();
                        break;
                case UM_PASSWORD_MODE_SET_AT_LOGIN:
                        text = C_("Password mode", "To be set at next login");
                        break;
                case UM_PASSWORD_MODE_NONE:
                        text = C_("Password mode", "None");
                        break;
                default:
                        g_assert_not_reached ();
                }
        }

        return text;
}

static void
autologin_changed (GObject            *object,
                   GParamSpec         *pspec,
                   UmUserPanelPrivate *d)
{
        gboolean active;
        UmUser *user;

        active = gtk_switch_get_active (GTK_SWITCH (object));
        user = get_selected_user (d);

        if (active != um_user_get_automatic_login (user)) {
                um_user_set_automatic_login (user, active);
                if (um_user_get_automatic_login (user)) {
                        GSList *list;
                        GSList *l;
                        list = um_user_manager_list_users (d->um);
                        for (l = list; l != NULL; l = l->next) {
                                UmUser *u = l->data;
                                if (um_user_get_uid (u) != um_user_get_uid (user)) {
                                        um_user_set_automatic_login (user, FALSE);
                                }
                        }
                        g_slist_free (list);
                }
        }

        g_object_unref (user);
}

static void
show_user (UmUser *user, UmUserPanelPrivate *d)
{
        GtkWidget *image;
        GtkWidget *label;
        GtkWidget *label2;
        GtkWidget *label3;
        GdkPixbuf *pixbuf;
        gchar *lang;
        GtkWidget *widget;
        GtkTreeModel *model;
        GtkTreeIter iter;

        pixbuf = um_user_render_icon (user, FALSE, 48);
        image = get_widget (d, "user-icon-image");
        gtk_image_set_from_pixbuf (GTK_IMAGE (image), pixbuf);
        image = get_widget (d, "user-icon-image2");
        gtk_image_set_from_pixbuf (GTK_IMAGE (image), pixbuf);
        g_object_unref (pixbuf);

        um_photo_dialog_set_user (d->photo_dialog, user);

        widget = get_widget (d, "full-name-entry");
        cc_editable_entry_set_text (CC_EDITABLE_ENTRY (widget), um_user_get_real_name (user));
        gtk_widget_set_tooltip_text (widget, um_user_get_user_name (user));

        widget = get_widget (d, "account-type-combo");
        um_editable_combo_set_active (UM_EDITABLE_COMBO (widget), um_user_get_account_type (user));

        widget = get_widget (d, "account-password-button");
        um_editable_button_set_text (UM_EDITABLE_BUTTON (widget), get_password_mode_text (user));

        widget = get_widget (d, "autologin-switch");
        g_signal_handlers_block_by_func (widget, autologin_changed, d);
        gtk_switch_set_active (GTK_SWITCH (widget), um_user_get_automatic_login (user));
        g_signal_handlers_unblock_by_func (widget, autologin_changed, d);

        gtk_widget_set_sensitive (widget, !um_user_get_locked (user));

        widget = get_widget (d, "account-language-combo");
        model = um_editable_combo_get_model (UM_EDITABLE_COMBO (widget));
        cc_add_user_languages (model);

        lang = g_strdup (um_user_get_language (user));
        if (!lang)
                lang = cc_common_language_get_current_language ();
        cc_common_language_get_iter_for_language (model, lang, &iter);
        um_editable_combo_set_active_iter (UM_EDITABLE_COMBO (widget), &iter);
        g_free (lang);

        widget = get_widget (d, "account-fingerprint-notebook");
        label = get_widget (d, "account-fingerprint-label");
        label2 = get_widget (d, "account-fingerprint-value-label");
        label3 = get_widget (d, "account-fingerprint-button-label");
        if (um_user_get_uid (user) != getuid() ||
            !set_fingerprint_label (label2, label3)) {
                gtk_widget_hide (label);
                gtk_widget_hide (widget);
        } else {
                gtk_widget_show (label);
                gtk_widget_show (widget);
        }
}

static void on_permission_changed (GPermission *permission, GParamSpec *pspec, gpointer data);

static void
selected_user_changed (GtkTreeSelection *selection, UmUserPanelPrivate *d)
{
        GtkTreeModel *model;
        GtkTreeIter iter;
        UmUser *user;

        if (gtk_tree_selection_get_selected (selection, &model, &iter)) {
                gtk_tree_model_get (model, &iter, USER_COL, &user, -1);
                show_user (user, d);
                on_permission_changed (d->permission, NULL, d);
                gtk_widget_set_sensitive (get_widget (d, "main-user-vbox"), TRUE);
                g_object_unref (user);
        } else {                
                gtk_widget_set_sensitive (get_widget (d, "main-user-vbox"), FALSE);
        }
}

static void
change_name_done (GtkWidget          *entry,
                  UmUserPanelPrivate *d)
{
        const gchar *text;
        UmUser *user;

        user = get_selected_user (d);

        text = cc_editable_entry_get_text (CC_EDITABLE_ENTRY (entry));
        if (g_strcmp0 (text, um_user_get_real_name (user)) != 0) {
                um_user_set_real_name (user, text);
        }

        g_object_unref (user);
}

static void
account_type_changed (UmEditableCombo    *combo,
                      UmUserPanelPrivate *d)
{
        UmUser *user;
        GtkTreeModel *model;
        GtkTreeIter iter;
        gint account_type;

        user = get_selected_user (d);

        model = um_editable_combo_get_model (combo);
        um_editable_combo_get_active_iter (combo, &iter);
        gtk_tree_model_get (model, &iter, 1, &account_type, -1);

        if (account_type != um_user_get_account_type (user)) {
                um_user_set_account_type (user, account_type);
        }

        g_object_unref (user);
}

static void
language_response (GtkDialog         *dialog,
                   gint               response_id,
                   UmUserPanelPrivate *d)
{
        GtkWidget *combo;
        UmUser *user;
        gchar *lang;
        GtkTreeModel *model;
        GtkTreeIter iter;

        user = get_selected_user (d);

        combo = get_widget (d, "account-language-combo");
        model = um_editable_combo_get_model (UM_EDITABLE_COMBO (combo));

        if (response_id == GTK_RESPONSE_OK) {
                lang = cc_language_chooser_get_language (GTK_WIDGET (dialog));
                um_user_set_language (user, lang);
        }
        else {
                lang = g_strdup (um_user_get_language (user));
                if (!lang)
                        lang = cc_common_language_get_current_language ();
        }
        cc_common_language_get_iter_for_language (model, lang, &iter);
        um_editable_combo_set_active_iter (UM_EDITABLE_COMBO (combo), &iter);
        g_free (lang);

        gtk_widget_hide (GTK_WIDGET (dialog));
        gtk_widget_set_sensitive (combo, TRUE);

        g_object_unref (user);
}

static void
language_changed (UmEditableCombo    *combo,
                  UmUserPanelPrivate *d)
{
        GtkTreeModel *model;
        GtkTreeIter iter;
        gchar *lang;
        UmUser *user;

        if (!um_editable_combo_get_active_iter (combo, &iter))
                 return;

        user = get_selected_user (d);

        model = um_editable_combo_get_model (combo);

        gtk_tree_model_get (model, &iter, 0, &lang, -1);
        if (lang) {
                if (g_strcmp0 (lang, um_user_get_language (user)) != 0) {
                        um_user_set_language (user, lang);
                }
                g_free (lang);
                goto out;
        }

        if (d->language_chooser) {
		cc_language_chooser_clear_filter (d->language_chooser);
                gtk_window_present (GTK_WINDOW (d->language_chooser));
                gtk_widget_set_sensitive (GTK_WIDGET (combo), FALSE);
                goto out;
        }

        d->language_chooser = cc_language_chooser_new (gtk_widget_get_toplevel (d->main_box), FALSE);

        g_signal_connect (d->language_chooser, "response",
                          G_CALLBACK (language_response), d);
        g_signal_connect (d->language_chooser, "delete-event",
                          G_CALLBACK (gtk_widget_hide_on_delete), NULL);

        gdk_window_set_cursor (gtk_widget_get_window (gtk_widget_get_toplevel (d->main_box)), NULL);
        gtk_window_present (GTK_WINDOW (d->language_chooser));
        gtk_widget_set_sensitive (GTK_WIDGET (combo), FALSE);

out:
        g_object_unref (user);
}

static void
change_password (GtkButton *button, UmUserPanelPrivate *d)
{
        UmUser *user;

        user = get_selected_user (d);

        um_password_dialog_set_user (d->password_dialog, user);
        um_password_dialog_show (d->password_dialog,
                                  GTK_WINDOW (gtk_widget_get_toplevel (d->main_box)));

        g_object_unref (user);
}

static void
change_fingerprint (GtkButton *button, UmUserPanelPrivate *d)
{
        GtkWidget *label, *label2;
        UmUser *user;

        user = get_selected_user (d);

        g_assert (g_strcmp0 (g_get_user_name (), um_user_get_user_name (user)) == 0);

        label = get_widget (d, "account-fingerprint-value-label");
        label2 = get_widget (d, "account-fingerprint-button-label");
        fingerprint_button_clicked (GTK_WINDOW (gtk_widget_get_toplevel (d->main_box)), label, label2, user);

        g_object_unref (user);
}

static gint
sort_users (GtkTreeModel *model,
            GtkTreeIter  *a,
            GtkTreeIter  *b,
            gpointer      data)
{
        UmUser *ua, *ub;
        gint sa, sb;
        gint result;

        gtk_tree_model_get (model, a, USER_COL, &ua, SORT_KEY_COL, &sa, -1);
        gtk_tree_model_get (model, b, USER_COL, &ub, SORT_KEY_COL, &sb, -1);

        if (sa < sb) {
                result = -1;
        }
        else if (sa > sb) {
                result = 1;
        }
        else {
                result = um_user_collate (ua, ub);
        }

        if (ua) {
                g_object_unref (ua);
        }
        if (ub) {
                g_object_unref (ub);
        }

        return result;
}

static gboolean
dont_select_headings (GtkTreeSelection *selection,
                      GtkTreeModel     *model,
                      GtkTreePath      *path,
                      gboolean          selected,
                      gpointer          data)
{
        GtkTreeIter iter;
        gboolean is_user;

        gtk_tree_model_get_iter (model, &iter, path);
        gtk_tree_model_get (model, &iter, USER_ROW_COL, &is_user, -1);

        return is_user;
}

static void
users_loaded (UmUserManager     *manager,
              UmUserPanelPrivate *d)
{
        GSList *list, *l;
        UmUser *user;
        GtkWidget *dialog;

        if (um_user_manager_no_service (d->um)) {
                dialog = gtk_message_dialog_new (GTK_WINDOW (gtk_widget_get_toplevel (d->main_box)),
                                                 GTK_DIALOG_MODAL,
                                                 GTK_MESSAGE_OTHER,
                                                 GTK_BUTTONS_CLOSE,
                                                 _("Failed to contact the accounts service"));
                gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
                                                          _("Please make sure that the AccountService is installed and enabled."));
                g_signal_connect_swapped (dialog, "response",
                                          G_CALLBACK (gtk_widget_destroy),
                                          dialog);
                gtk_widget_show (dialog);

                gtk_widget_set_sensitive (d->main_box, FALSE);
        }

        list = um_user_manager_list_users (d->um);
        g_debug ("Got %d users\n", g_slist_length (list));

        g_signal_connect (d->um, "user-changed", G_CALLBACK (user_changed), d);

        for (l = list; l; l = l->next) {
                user = l->data;
                g_debug ("adding user %s\n", um_user_get_real_name (user));
                user_added (d->um, user, d);
        }
        g_slist_free (list);

        g_signal_connect (d->um, "user-added", G_CALLBACK (user_added), d);
        g_signal_connect (d->um, "user-removed", G_CALLBACK (user_removed), d);
}

static void
add_unlock_tooltip (GtkWidget *button)
{
        gchar *names[3];
        GIcon *icon;

        names[0] = "changes-allow-symbolic";
        names[1] = "changes-allow";
        names[2] = NULL;
        icon = (GIcon *)g_themed_icon_new_from_names (names, -1);
        /* Translator comments:
         * We split the line in 2 here to "make it look good", as there's
         * no good way to do this in GTK+ for tooltips. See:
         * https://bugzilla.gnome.org/show_bug.cgi?id=657168 */
        setup_tooltip_with_embedded_icon (button,
                                          _("To make changes,\nclick the * icon first"),
                                          "*",
                                          icon);
        g_object_unref (icon);
        g_signal_connect (button, "button-release-event",
                           G_CALLBACK (show_tooltip_now), NULL);
}

static void
remove_unlock_tooltip (GtkWidget *button)
{
        setup_tooltip_with_embedded_icon (button, NULL, NULL, NULL);
        g_signal_handlers_disconnect_by_func (button,
                                              G_CALLBACK (show_tooltip_now), NULL);
}

static void
on_permission_changed (GPermission *permission,
                       GParamSpec  *pspec,
                       gpointer     data)
{
        UmUserPanelPrivate *d = data;
        gboolean is_authorized;
        gboolean self_selected;
        UmUser *user;
        GtkWidget *widget;

        user = get_selected_user (d);
        if (!user) {
                return;
        }

        is_authorized = g_permission_get_allowed (G_PERMISSION (d->permission));
        self_selected = um_user_get_uid (user) == geteuid ();

        widget = get_widget (d, "add-user-toolbutton");
        gtk_widget_set_sensitive (widget, is_authorized);
        if (is_authorized) {
                setup_tooltip_with_embedded_icon (widget, _("Create a user account"), NULL, NULL);
        }
        else {
                gchar *names[3];
                GIcon *icon;

                names[0] = "changes-allow-symbolic";
                names[1] = "changes-allow";
                names[2] = NULL;
                icon = (GIcon *)g_themed_icon_new_from_names (names, -1);
                setup_tooltip_with_embedded_icon (widget,
                                                  _("To create a user account,\nclick the * icon first"),
                                                  "*",
                                                  icon);
                g_object_unref (icon);
        }

        widget = get_widget (d, "remove-user-toolbutton");
        gtk_widget_set_sensitive (widget, is_authorized && !self_selected);
        if (is_authorized) {
                setup_tooltip_with_embedded_icon (widget, _("Delete the selected user account"), NULL, NULL);
        }
        else {
                gchar *names[3];
                GIcon *icon;

                names[0] = "changes-allow-symbolic";
                names[1] = "changes-allow";
                names[2] = NULL;
                icon = (GIcon *)g_themed_icon_new_from_names (names, -1);

                setup_tooltip_with_embedded_icon (widget,
                                                  _("To delete the selected user account,\nclick the * icon first"),
                                                  "*",
                                                  icon);
                g_object_unref (icon);
        }

        if (is_authorized) {
                um_editable_combo_set_editable (UM_EDITABLE_COMBO (get_widget (d, "account-type-combo")), TRUE);
                remove_unlock_tooltip (get_widget (d, "account-type-combo"));
                gtk_widget_set_sensitive (GTK_WIDGET (get_widget (d, "autologin-switch")), TRUE);
                remove_unlock_tooltip (get_widget (d, "autologin-switch"));
        }
        else {
                um_editable_combo_set_editable (UM_EDITABLE_COMBO (get_widget (d, "account-type-combo")), FALSE);
                add_unlock_tooltip (get_widget (d, "account-type-combo"));
                gtk_widget_set_sensitive (GTK_WIDGET (get_widget (d, "autologin-switch")), FALSE);
                add_unlock_tooltip (get_widget (d, "autologin-switch"));
        }

        if (is_authorized || self_selected) {
                gtk_widget_show (get_widget (d, "user-icon-button"));
                gtk_widget_hide (get_widget (d, "user-icon-nonbutton"));

                cc_editable_entry_set_editable (CC_EDITABLE_ENTRY (get_widget (d, "full-name-entry")), TRUE);
                remove_unlock_tooltip (get_widget (d, "full-name-entry"));

                um_editable_combo_set_editable (UM_EDITABLE_COMBO (get_widget (d, "account-language-combo")), TRUE);
                remove_unlock_tooltip (get_widget (d, "account-language-combo"));

                um_editable_button_set_editable (UM_EDITABLE_BUTTON (get_widget (d, "account-password-button")), TRUE);
                remove_unlock_tooltip (get_widget (d, "account-password-button"));

                gtk_notebook_set_current_page (GTK_NOTEBOOK (get_widget (d, "account-fingerprint-notebook")), 1);
        }
        else {
                gtk_widget_hide (get_widget (d, "user-icon-button"));
                gtk_widget_show (get_widget (d, "user-icon-nonbutton"));

                cc_editable_entry_set_editable (CC_EDITABLE_ENTRY (get_widget (d, "full-name-entry")), FALSE);
                add_unlock_tooltip (get_widget (d, "full-name-entry"));

                um_editable_combo_set_editable (UM_EDITABLE_COMBO (get_widget (d, "account-language-combo")), FALSE);
                add_unlock_tooltip (get_widget (d, "account-language-combo"));

                um_editable_button_set_editable (UM_EDITABLE_BUTTON (get_widget (d, "account-password-button")), FALSE);
                add_unlock_tooltip (get_widget (d, "account-password-button"));

                gtk_notebook_set_current_page (GTK_NOTEBOOK (get_widget (d, "account-fingerprint-notebook")), 0);
        }

        um_password_dialog_set_privileged (d->password_dialog, is_authorized);

        g_object_unref (user);
}

static gboolean
match_user (GtkTreeModel *model,
            gint          column,
            const gchar  *key,
            GtkTreeIter  *iter,
            gpointer      search_data)
{
        UmUser *user;
        const gchar *name;
        gchar *normalized_key = NULL;
        gchar *normalized_name = NULL;
        gchar *case_normalized_key = NULL;
        gchar *case_normalized_name = NULL;
        gchar *p;
        gboolean result = TRUE;
        gint i;

        gtk_tree_model_get (model, iter, USER_COL, &user, -1);

        if (!user) {
                goto out;
        }

        normalized_key = g_utf8_normalize (key, -1, G_NORMALIZE_ALL);
        if (!normalized_key) {
                goto out;
        }

        case_normalized_key = g_utf8_casefold (normalized_key, -1);

        for (i = 0; i < 2; i++) {
                if (i == 0) {
                        name = um_user_get_real_name (user);
                }
                else {
                        name = um_user_get_user_name (user);
                }
                g_free (normalized_name);
                normalized_name = g_utf8_normalize (name, -1, G_NORMALIZE_ALL);
                if (normalized_name) {
                        g_free (case_normalized_name);
                        case_normalized_name = g_utf8_casefold (normalized_name, -1);
                        p = strstr (case_normalized_name, case_normalized_key);

                        /* poor man's \b */
                        if (p == case_normalized_name || (p && p[-1] == ' ')) {
                                result = FALSE;
                                break;
                        }
                }
        }

 out:
        if (user) {
                g_object_unref (user);
        }
        g_free (normalized_key);
        g_free (case_normalized_key);
        g_free (normalized_name);
        g_free (case_normalized_name);

        return result;
}

static void
autologin_cell_data_func (GtkTreeViewColumn    *tree_column,
                          GtkCellRenderer      *cell,
                          GtkTreeModel         *model,
                          GtkTreeIter          *iter,
                          UmUserPanelPrivate   *d)
{
        gboolean is_autologin;

        gtk_tree_model_get (model, iter, AUTOLOGIN_COL, &is_autologin, -1);

        if (is_autologin) {
                g_object_set (cell, "icon-name", "emblem-default-symbolic", NULL);
        } else {
                g_object_set (cell, "icon-name", NULL, NULL);
        }
}

static void
setup_main_window (UmUserPanelPrivate *d)
{
        GtkWidget *userlist;
        GtkTreeModel *model;
        GtkListStore *store;
        GtkTreeViewColumn *column;
        GtkCellRenderer *cell;
        GtkTreeSelection *selection;
        GtkWidget *button;
        GtkTreeIter iter;
        gint expander_size;
        gchar *title;
        GIcon *icon;
        gchar *names[3];

        userlist = get_widget (d, "list-treeview");
        store = gtk_list_store_new (NUM_USER_LIST_COLS,
                                    UM_TYPE_USER,
                                    GDK_TYPE_PIXBUF,
                                    G_TYPE_STRING,
                                    G_TYPE_BOOLEAN,
                                    G_TYPE_STRING,
                                    G_TYPE_BOOLEAN,
                                    G_TYPE_INT,
                                    G_TYPE_BOOLEAN);
        model = (GtkTreeModel *)store;
        gtk_tree_sortable_set_default_sort_func (GTK_TREE_SORTABLE (model), sort_users, NULL, NULL);
        gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (model), GTK_TREE_SORTABLE_DEFAULT_SORT_COLUMN_ID, GTK_SORT_ASCENDING);
        gtk_tree_view_set_model (GTK_TREE_VIEW (userlist), model);
        gtk_tree_view_set_search_column (GTK_TREE_VIEW (userlist), USER_COL);
        gtk_tree_view_set_search_equal_func (GTK_TREE_VIEW (userlist),
                                             match_user, NULL, NULL);
        g_object_unref (model);

        g_signal_connect (d->um, "users-loaded", G_CALLBACK (users_loaded), d);

        gtk_widget_style_get (userlist, "expander-size", &expander_size, NULL);
        gtk_tree_view_set_level_indentation (GTK_TREE_VIEW (userlist), - (expander_size + 6));

        title = g_strdup_printf ("<small><span foreground=\"#555555\">%s</span></small>", _("My Account"));
        gtk_list_store_append (store, &iter);
        gtk_list_store_set (store, &iter,
                            TITLE_COL, title,
                            HEADING_ROW_COL, TRUE,
                            SORT_KEY_COL, 0,
                            AUTOLOGIN_COL, FALSE,
                            -1);
        g_free (title);

        title = g_strdup_printf ("<small><span foreground=\"#555555\">%s</span></small>", _("Other Accounts"));
        gtk_list_store_append (store, &iter);
        gtk_list_store_set (store, &iter,
                            TITLE_COL, title,
                            HEADING_ROW_COL, TRUE,
                            SORT_KEY_COL, 2,
                            AUTOLOGIN_COL, FALSE,
                            -1);
        g_free (title);

        column = gtk_tree_view_column_new ();
        cell = gtk_cell_renderer_pixbuf_new ();
        gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (column), cell, FALSE);
        gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (column), cell, "pixbuf", FACE_COL);
        gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (column), cell, "visible", USER_ROW_COL);
        cell = gtk_cell_renderer_text_new ();
        g_object_set (cell, "ellipsize", PANGO_ELLIPSIZE_END, NULL);
        gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (column), cell, TRUE);
        gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (column), cell, "markup", NAME_COL);
        gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (column), cell, "visible", USER_ROW_COL);
        cell = gtk_cell_renderer_text_new ();
        gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (column), cell, TRUE);
        gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (column), cell, "markup", TITLE_COL);
        gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (column), cell, "visible", HEADING_ROW_COL);
        cell = gtk_cell_renderer_pixbuf_new ();
        gtk_tree_view_column_pack_start (column, cell, FALSE);
        gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (column), cell, "visible", USER_ROW_COL);
        gtk_tree_view_column_set_cell_data_func (column,
                                                 cell,
                                                 (GtkTreeCellDataFunc) autologin_cell_data_func,
                                                 d,
                                                 NULL);

        gtk_tree_view_append_column (GTK_TREE_VIEW (userlist), column);

        selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (userlist));
        gtk_tree_selection_set_mode (selection, GTK_SELECTION_BROWSE);
        g_signal_connect (selection, "changed", G_CALLBACK (selected_user_changed), d);
        gtk_tree_selection_set_select_function (selection, dont_select_headings, NULL, NULL);

        gtk_scrolled_window_set_min_content_width (GTK_SCROLLED_WINDOW (get_widget (d, "list-scrolledwindow")), 300);
        gtk_widget_set_size_request (get_widget (d, "list-scrolledwindow"), 200, -1);

        button = get_widget (d, "add-user-toolbutton");
        g_signal_connect (button, "clicked", G_CALLBACK (add_user), d);

        button = get_widget (d, "remove-user-toolbutton");
        g_signal_connect (button, "clicked", G_CALLBACK (delete_user), d);

        button = get_widget (d, "user-icon-nonbutton");
        add_unlock_tooltip (button);

        button = get_widget (d, "full-name-entry");
        g_signal_connect (button, "editing-done", G_CALLBACK (change_name_done), d);

        button = get_widget (d, "account-type-combo");
        g_signal_connect (button, "editing-done", G_CALLBACK (account_type_changed), d);

        button = get_widget (d, "account-password-button");
        g_signal_connect (button, "start-editing", G_CALLBACK (change_password), d);

        button = get_widget (d, "account-language-combo");
        g_signal_connect (button, "editing-done", G_CALLBACK (language_changed), d);

        button = get_widget (d, "autologin-switch");
        g_signal_connect (button, "notify::active", G_CALLBACK (autologin_changed), d);

        button = get_widget (d, "account-fingerprint-button");
        g_signal_connect (button, "clicked",
                          G_CALLBACK (change_fingerprint), d);

        d->permission = (GPermission *)polkit_permission_new_sync ("org.freedesktop.accounts.user-administration", NULL, NULL, NULL);
        g_signal_connect (d->permission, "notify",
                          G_CALLBACK (on_permission_changed), d);
        on_permission_changed (d->permission, NULL, d);

        button = get_widget (d, "add-user-toolbutton");
        names[0] = "changes-allow-symbolic";
        names[1] = "changes-allow";
        names[2] = NULL;
        icon = (GIcon *)g_themed_icon_new_from_names (names, -1);
        setup_tooltip_with_embedded_icon (button,
                                          _("To create a user account,\nclick the * icon first"),
                                          "*",
                                          icon);
        button = get_widget (d, "remove-user-toolbutton");
        setup_tooltip_with_embedded_icon (button,
                                          _("To delete the selected user account,\nclick the * icon first"),
                                          "*",
                                          icon);
        g_object_unref (icon);
}

static void
um_user_panel_init (UmUserPanel *self)
{
        UmUserPanelPrivate *d;
        GError *error;
        volatile GType type G_GNUC_UNUSED;
        const gchar *filename;
        GtkWidget *button;
        GtkStyleContext *context;

        d = self->priv = UM_USER_PANEL_PRIVATE (self);

        /* register types that the builder might need */
        type = cc_strength_bar_get_type ();
        type = um_editable_button_get_type ();
        type = cc_editable_entry_get_type ();
        type = um_editable_combo_get_type ();

        gtk_widget_set_size_request (GTK_WIDGET (self), -1, 350);

        d->builder = gtk_builder_new ();
        d->um = um_user_manager_ref_default ();

        filename = UIDIR "/user-accounts-dialog.ui";
        if (!g_file_test (filename, G_FILE_TEST_EXISTS)) {
                filename = "data/user-accounts-dialog.ui";
        }
        error = NULL;
        if (!gtk_builder_add_from_file (d->builder, filename, &error)) {
                g_error ("%s", error->message);
                g_error_free (error);
                return;
        }

        setup_main_window (d);
        d->account_dialog = um_account_dialog_new ();
        d->password_dialog = um_password_dialog_new ();
        button = get_widget (d, "user-icon-button");
        d->photo_dialog = um_photo_dialog_new (button);
        d->main_box = get_widget (d, "accounts-vbox");
        gtk_widget_reparent (d->main_box, GTK_WIDGET (self));

        context = gtk_widget_get_style_context (get_widget (d, "list-scrolledwindow"));
        gtk_style_context_set_junction_sides (context, GTK_JUNCTION_BOTTOM);
        context = gtk_widget_get_style_context (get_widget (d, "add-remove-toolbar"));
        gtk_style_context_set_junction_sides (context, GTK_JUNCTION_TOP);
}

static void
um_user_panel_dispose (GObject *object)
{
        UmUserPanelPrivate *priv = UM_USER_PANEL (object)->priv;

        if (priv->um) {
                g_object_unref (priv->um);
                priv->um = NULL;
        }
        if (priv->builder) {
                g_object_unref (priv->builder);
                priv->builder = NULL;
        }
        if (priv->account_dialog) {
                um_account_dialog_free (priv->account_dialog);
                priv->account_dialog = NULL;
        }
        if (priv->password_dialog) {
                um_password_dialog_free (priv->password_dialog);
                priv->password_dialog = NULL;
        }
        if (priv->photo_dialog) {
                um_photo_dialog_free (priv->photo_dialog);
                priv->photo_dialog = NULL;
        }
        if (priv->language_chooser) {
                gtk_widget_destroy (priv->language_chooser);
                priv->language_chooser = NULL;
        }
        if (priv->permission) {
                g_object_unref (priv->permission);
                priv->permission = NULL;
        }
        G_OBJECT_CLASS (um_user_panel_parent_class)->dispose (object);
}

static GPermission *
um_user_panel_get_permission (CcPanel *panel)
{
        UmUserPanelPrivate *priv = UM_USER_PANEL (panel)->priv;

        return priv->permission;
}

static void
um_user_panel_class_init (UmUserPanelClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);
        CcPanelClass *panel_class = CC_PANEL_CLASS (klass);

        object_class->dispose = um_user_panel_dispose;

        panel_class->get_permission = um_user_panel_get_permission;

        g_type_class_add_private (klass, sizeof (UmUserPanelPrivate));
}

static void
um_user_panel_class_finalize (UmUserPanelClass *klass)
{
}

void
um_user_panel_register (GIOModule *module)
{
        um_user_panel_register_type (G_TYPE_MODULE (module));
        g_io_extension_point_implement (CC_SHELL_PANEL_EXTENSION_POINT,
                                        UM_TYPE_USER_PANEL, "user-accounts", 0);
}
