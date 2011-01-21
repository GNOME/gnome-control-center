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

#include <stdlib.h>
#include <sys/types.h>
#include <pwd.h>
#include <utmp.h>

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

static gboolean
is_username_used (const gchar *username)
{
        struct passwd *pwent;

        if (username == NULL || username[0] == '\0') {
                return FALSE;
        }

        pwent = getpwnam (username);

        return pwent != NULL;
}

static void
username_changed (GtkComboBoxText *combo,
                  UmAccountDialog *um)
{
        gboolean in_use;
        gboolean empty;
        gboolean valid;
        gboolean toolong;
        const gchar *username;
        const gchar *c;
        gchar *tip;
        GtkWidget *entry;

        username = gtk_combo_box_text_get_active_text (combo);

        if (username == NULL || username[0] == '\0') {
                empty = TRUE;
                in_use = FALSE;
                toolong = FALSE;
        } else {
                empty = FALSE;
                in_use = is_username_used (username);
                toolong = strlen (username) > MAXNAMELEN;
        }
        valid = TRUE;

        if (!in_use && !empty && !toolong) {
                /* First char must be a letter, and it must only composed
                 * of ASCII letters, digits, and a '.', '-', '_'
                 */
                for (c = username; *c; c++) {
                        if (! ((*c >= 'a' && *c <= 'z') ||
                               (*c >= 'A' && *c <= 'Z') ||
                               (*c >= '0' && *c <= '9') ||
                               (*c == '_') || (*c == '.') ||
                               (*c == '-' && c != username)))
                           valid = FALSE;
                }
        }

        um->valid_username = !empty && !in_use && !toolong && valid;
        gtk_widget_set_sensitive (um->ok_button, um->valid_name && um->valid_username);

        entry = gtk_bin_get_child (GTK_BIN (combo));

        if (!empty && (in_use || toolong || !valid)) {
                if (in_use) {
                        tip = g_strdup_printf (_("A user with the username '%s' already exists"),
                                               username);
                }
                else if (toolong) {
                        tip = g_strdup_printf (_("The username is too long"));
                }
                else if (username[0] == '-') {
                        tip = g_strdup (_("The username cannot start with a '-'"));
                }
                else {
                        tip = g_strdup (_("The username must consist of:\n"
                                          " \xe2\x9e\xa3 letters from the English alphabet\n"
                                          " \xe2\x9e\xa3 digits\n"
                                          " \xe2\x9e\xa3 any of the characters '.', '-' and '_'"));
                }

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
        gboolean in_use;
        const char *name;
        char *lc_name, *ascii_name, *stripped_name;
        char **words1;
        char **words2 = NULL;
        char **w1, **w2;
        char *c;
        char *unicode_fallback = "?";
        GString *first_word, *last_word;
        GString *item0, *item1, *item2, *item3, *item4;
        int len;
        int nwords1, nwords2, i;
        GHashTable *items;

        entry = gtk_bin_get_child (GTK_BIN (um->username_combo));
        model = gtk_combo_box_get_model (GTK_COMBO_BOX (um->username_combo));
        gtk_list_store_clear (GTK_LIST_STORE (model));

        name = gtk_entry_get_text (GTK_ENTRY (name_entry));

        um->valid_name = (strlen (name) > 0);
        gtk_widget_set_sensitive (um->ok_button, um->valid_name && um->valid_username);

        if (!um->valid_name) {
                gtk_entry_set_text (GTK_ENTRY (entry), "");
                return;
        }

        ascii_name = g_convert_with_fallback (name, -1, "ASCII//TRANSLIT", "UTF-8",
                                              unicode_fallback, NULL, NULL, NULL);

        lc_name = g_ascii_strdown (ascii_name, -1);

        /* remove all non ASCII alphanumeric chars from the name,
         * apart from the few allowed symbols
         */
        stripped_name = g_strnfill (strlen (lc_name) + 1, '\0');
        i = 0;
        for (c = lc_name; *c; c++) {
                if (!(g_ascii_isdigit (*c) || g_ascii_islower (*c) ||
                    *c == ' ' || *c == '-' || *c == '.' || *c == '_' ||
                    /* used to track invalid words, removed below */
                    *c == '?') )
                        continue;

                    stripped_name[i] = *c;
                    i++;
        }

        if (strlen (stripped_name) == 0) {
                g_free (ascii_name);
                g_free (lc_name);
                g_free (stripped_name);
                return;
        }

        /* we split name on spaces, and then on dashes, so that we can treat
         * words linked with dashes the same way, i.e. both fully shown, or
         * both abbreviated
         */
        words1 = g_strsplit_set (stripped_name, " ", -1);
        len = g_strv_length (words1);

        /* The default item is a concatenation of all words without ? */
        item0 = g_string_sized_new (strlen (stripped_name));

        g_free (ascii_name);
        g_free (lc_name);
        g_free (stripped_name);

        /* Concatenate the whole first word with the first letter of each
         * word (item1), and the last word with the first letter of each
         * word (item2). item3 and item4 are symmetrical respectively to
         * item1 and item2.
         *
         * Constant 5 is the max reasonable number of words we may get when
         * splitting on dashes, since we can't guess it at this point,
         * and reallocating would be too bad.
         */
        item1 = g_string_sized_new (strlen (words1[0]) + len - 1 + 5);
        item3 = g_string_sized_new (strlen (words1[0]) + len - 1 + 5);

        item2 = g_string_sized_new (strlen (words1[len - 1]) + len - 1 + 5);
        item4 = g_string_sized_new (strlen (words1[len - 1]) + len - 1 + 5);

        /* again, guess at the max size of names */
        first_word = g_string_sized_new (20);
        last_word = g_string_sized_new (20);

        nwords1 = 0;
        nwords2 = 0;
        for (w1 = words1; *w1; w1++) {
                if (strlen (*w1) == 0)
                        continue;

                /* skip words with string '?', most likely resulting
                 * from failed transliteration to ASCII
                 */
                if (strstr (*w1, unicode_fallback) != NULL)
                        continue;

                nwords1++; /* count real words, excluding empty string */

                item0 = g_string_append (item0, *w1);

                words2 = g_strsplit_set (*w1, "-", -1);
                /* reset last word if a new non-empty word has been found */
                if (strlen (*words2) > 0)
                        last_word = g_string_set_size (last_word, 0);

                for (w2 = words2; *w2; w2++) {
                        if (strlen (*w2) == 0)
                                continue;

                        nwords2++;

                        /* part of the first "toplevel" real word */
                        if (nwords1 == 1) {
                                item1 = g_string_append (item1, *w2);
                                first_word = g_string_append (first_word, *w2);
                        }
                        else {
                                item1 = g_string_append_unichar (item1,
                                                                 g_utf8_get_char (*w2));
                                item3 = g_string_append_unichar (item3,
                                                                 g_utf8_get_char (*w2));
                        }

                        /* not part of the last "toplevel" word */
                        if (w1 != words1 + len - 1) {
                                item2 = g_string_append_unichar (item2,
                                                                 g_utf8_get_char (*w2));
                                item4 = g_string_append_unichar (item4,
                                                                 g_utf8_get_char (*w2));
                        }

                        /* always save current word so that we have it if last one reveals empty */
                        last_word = g_string_append (last_word, *w2);
                }

                g_strfreev (words2);
        }
        item2 = g_string_append (item2, last_word->str);
        item3 = g_string_append (item3, first_word->str);
        item4 = g_string_prepend (item4, last_word->str);

        items = g_hash_table_new (g_str_hash, g_str_equal);

        in_use = is_username_used (item0->str);
        if (!in_use && !g_ascii_isdigit (item0->str[0])) {
                gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (um->username_combo), item0->str);
                g_hash_table_insert (items, item0->str, item0->str);
        }

        in_use = is_username_used (item1->str);
        if (nwords2 > 0 && !in_use && !g_ascii_isdigit (item1->str[0])) {
                gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (um->username_combo), item1->str);
                g_hash_table_insert (items, item1->str, item1->str);
        }

        /* if there's only one word, would be the same as item1 */
        if (nwords2 > 1) {
                /* add other items */
                in_use = is_username_used (item2->str);
                if (!in_use && !g_ascii_isdigit (item2->str[0]) &&
                    !g_hash_table_lookup (items, item2->str)) {
                        gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (um->username_combo), item2->str);
                        g_hash_table_insert (items, item2->str, item2->str);
                }

                in_use = is_username_used (item3->str);
                if (!in_use && !g_ascii_isdigit (item3->str[0]) &&
                    !g_hash_table_lookup (items, item3->str)) {
                        gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (um->username_combo), item3->str);
                        g_hash_table_insert (items, item3->str, item3->str);
                }

                in_use = is_username_used (item4->str);
                if (!in_use && !g_ascii_isdigit (item4->str[0]) &&
                    !g_hash_table_lookup (items, item4->str)) {
                        gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (um->username_combo), item4->str);
                        g_hash_table_insert (items, item4->str, item4->str);
                }

                /* add the last word */
                in_use = is_username_used (last_word->str);
                if (!in_use && !g_ascii_isdigit (last_word->str[0]) &&
                    !g_hash_table_lookup (items, last_word->str)) {
                        gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (um->username_combo), last_word->str);
                        g_hash_table_insert (items, last_word->str, last_word->str);
                }

                /* ...and the first one */
                in_use = is_username_used (first_word->str);
                if (!in_use && !g_ascii_isdigit (first_word->str[0]) &&
                    !g_hash_table_lookup (items, first_word->str)) {
                        gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (um->username_combo), first_word->str);
                        g_hash_table_insert (items, first_word->str, first_word->str);
                }
        }

        gtk_combo_box_set_active (GTK_COMBO_BOX (um->username_combo), 0);
        g_hash_table_destroy (items);
        g_strfreev (words1);
        g_string_free (first_word, TRUE);
        g_string_free (last_word, TRUE);
        g_string_free (item0, TRUE);
        g_string_free (item1, TRUE);
        g_string_free (item2, TRUE);
        g_string_free (item3, TRUE);
        g_string_free (item4, TRUE);
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
                filename = "../data/account-dialog.ui";
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


