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

#include "um-user-panel.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <locale.h>

#include <glib.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <polkit/polkit.h>
#include <act/act.h>
#include <cairo-gobject.h>

#define GNOME_DESKTOP_USE_UNSTABLE_API
#include <libgnome-desktop/gnome-languages.h>

#include "um-user-image.h"

#include "um-account-dialog.h"
#include "cc-language-chooser.h"
#include "um-password-dialog.h"
#include "um-carousel.h"
#include "um-photo-dialog.h"
#include "um-fingerprint-dialog.h"
#include "um-utils.h"
#include "um-resources.h"
#include "um-history-dialog.h"

#include "cc-common-language.h"
#include "cc-util.h"

#include "um-realm-manager.h"

#define USER_ACCOUNTS_PERMISSION "org.gnome.controlcenter.user-accounts.administration"

struct _CcUserPanel {
        CcPanel parent_instance;

        ActUserManager *um;
        GCancellable  *cancellable;
        GtkBuilder *builder;
        GtkWidget *notification;
        GSettings *login_screen_settings;

        GtkWidget *headerbar_buttons;
        GtkWidget *stack;
        GtkWidget *main_box;
        UmCarousel *carousel;
        ActUser *selected_user;
        GPermission *permission;
        CcLanguageChooser *language_chooser;

        UmPasswordDialog *password_dialog;
        UmPhotoDialog *photo_dialog;
        UmHistoryDialog *history_dialog;

        gint other_accounts;

        UmAccountDialog *account_dialog;
};

CC_PANEL_REGISTER (CcUserPanel, cc_user_panel)

static GtkWidget *
get_widget (CcUserPanel *self, const char *name)
{
        return (GtkWidget *)gtk_builder_get_object (self->builder, name);
}

/* Headerbar button states. */
#define PAGE_LOCK "_lock"
#define PAGE_ADDUSER "_adduser"

/* Panel states */
#define PAGE_NO_USERS "_empty_state"
#define PAGE_USERS "_users"

static void show_restart_notification (CcUserPanel *self, const gchar *locale);
static gint user_compare (gconstpointer i, gconstpointer u);

typedef struct {
        CcUserPanel *self;
        GCancellable *cancellable;
        gchar *login;
} AsyncDeleteData;

static void
async_delete_data_free (AsyncDeleteData *data)
{
        g_object_unref (data->self);
        g_object_unref (data->cancellable);
        g_free (data->login);
        g_slice_free (AsyncDeleteData, data);
}

static void
show_error_dialog (CcUserPanel *self,
                   const gchar *message,
                   GError *error)
{
        GtkWidget *dialog;

        dialog = gtk_message_dialog_new (GTK_WINDOW (gtk_widget_get_toplevel (self->main_box)),
                                         GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_USE_HEADER_BAR,
                                         GTK_MESSAGE_ERROR,
                                         GTK_BUTTONS_CLOSE,
                                         "%s", message);

        if (error != NULL) {
                g_dbus_error_strip_remote_error (error);
                gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
                                                          "%s", error->message);
        }

        g_signal_connect (dialog, "response", G_CALLBACK (gtk_widget_destroy), NULL);
        gtk_window_present (GTK_WINDOW (dialog));
}

static ActUser *
get_selected_user (CcUserPanel *self)
{
        return self->selected_user;
}

static const gchar *
get_real_or_user_name (ActUser *user)
{
  const gchar *name;

  name = act_user_get_real_name (user);
  if (name == NULL)
    name = act_user_get_user_name (user);

  return name;
}

static void show_user (ActUser *user, CcUserPanel *self);

static void
set_selected_user (UmCarousel *carousel, UmCarouselItem *item, CcUserPanel *self)
{
        uid_t uid;

        g_clear_object (&self->selected_user);

        uid = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (item), "uid"));
        self->selected_user = act_user_manager_get_user_by_id (self->um, uid);

        if (self->selected_user != NULL) {
                show_user (self->selected_user, self);
        }
}

static GtkWidget *
create_carousel_entry (CcUserPanel *self, ActUser *user)
{
        GtkWidget *box, *widget;
        gchar *label;

        box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);

        widget = um_user_image_new ();
        um_user_image_set_user (UM_USER_IMAGE (widget), user);
        gtk_box_pack_start (GTK_BOX (box), widget, FALSE, FALSE, 0);

        label = g_strdup_printf ("<b>%s</b>",
                                 get_real_or_user_name (user));
        widget = gtk_label_new (label);
        gtk_label_set_use_markup (GTK_LABEL (widget), TRUE);
        gtk_widget_set_margin_top (widget, 5);
        gtk_box_pack_start (GTK_BOX (box), widget, FALSE, TRUE, 0);
        g_free (label);

        if (act_user_get_uid (user) == getuid ())
                label = g_strdup_printf ("<small>%s</small>", _("Your account"));
        else
                label = g_strdup (" ");

        widget = gtk_label_new (label);
        gtk_label_set_use_markup (GTK_LABEL (widget), TRUE);
        g_free (label);

        gtk_box_pack_start (GTK_BOX (box), widget, FALSE, TRUE, 0);
        gtk_style_context_add_class (gtk_widget_get_style_context (widget),
                                     "dim-label");

        return box;
}

static void
user_added (ActUserManager *um, ActUser *user, CcUserPanel *self)
{
        GtkWidget *item, *widget;
        gboolean show_carousel;

        if (act_user_is_system_account (user)) {
                return;
        }

        g_debug ("user added: %d %s\n", act_user_get_uid (user), get_real_or_user_name (user));

        widget = create_carousel_entry (self, user);
        item = um_carousel_item_new ();
        gtk_container_add (GTK_CONTAINER (item), widget);

        g_object_set_data (G_OBJECT (item), "uid", GINT_TO_POINTER (act_user_get_uid (user)));
        gtk_container_add (GTK_CONTAINER (self->carousel), item);

        if (act_user_get_uid (user) != getuid ()) {
                self->other_accounts++;
        }

        /* Show heading for other accounts if new one have been added. */
        show_carousel = (self->other_accounts > 0);
        gtk_revealer_set_reveal_child (GTK_REVEALER (self->carousel), show_carousel);

        gtk_stack_set_visible_child_name (GTK_STACK (self->stack), PAGE_USERS);
}

static gint
sort_users (gconstpointer a, gconstpointer b)
{
        ActUser *ua, *ub;
        gchar *name1, *name2;
        gint result;

        ua = ACT_USER (a);
        ub = ACT_USER (b);

        /* Make sure the current user is shown first */
        if (act_user_get_uid (ua) == getuid ()) {
                result = -G_MAXINT32;
        }
        else if (act_user_get_uid (ub) == getuid ()) {
                result = G_MAXINT32;
        }
        else {
                name1 = g_utf8_collate_key (get_real_or_user_name (ua), -1);
                name2 = g_utf8_collate_key (get_real_or_user_name (ub), -1);

                result = strcmp (name1, name2);

                g_free (name1);
                g_free (name2);
        }

        return result;
}

static void
reload_users (CcUserPanel *self, ActUser *selected_user)
{
        ActUser *user;
        GSList *list, *l;
        UmCarouselItem *item = NULL;
        GtkSettings *settings;
        gboolean animations;

        settings = gtk_settings_get_default ();

        g_object_get (settings, "gtk-enable-animations", &animations, NULL);
        g_object_set (settings, "gtk-enable-animations", FALSE, NULL);

        um_carousel_purge_items (self->carousel);
        self->other_accounts = 0;

        list = act_user_manager_list_users (self->um);
        g_debug ("Got %d users\n", g_slist_length (list));

        list = g_slist_sort (list, (GCompareFunc) sort_users);
        for (l = list; l; l = l->next) {
                user = l->data;
                g_debug ("adding user %s\n", get_real_or_user_name (user));
                user_added (self->um, user, self);
        }
        g_slist_free (list);

        if (um_carousel_get_item_count (self->carousel) == 0)
                gtk_stack_set_visible_child_name (GTK_STACK (self->stack), PAGE_NO_USERS);
        if (self->other_accounts == 0)
                gtk_revealer_set_reveal_child (GTK_REVEALER (self->carousel), FALSE);

        if (selected_user)
                item = um_carousel_find_item (self->carousel, selected_user, user_compare);
        um_carousel_select_item (self->carousel, item);

        g_object_set (settings, "gtk-enable-animations", animations, NULL);
}

static gint
user_compare (gconstpointer i,
              gconstpointer u)
{
        UmCarouselItem *item;
        ActUser *user;
        gint uid_a, uid_b;
        gint result;

        item = (UmCarouselItem *) i;
        user = ACT_USER (u);

        uid_a = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (item), "uid"));
        uid_b = act_user_get_uid (user);

        result = uid_a - uid_b;

        return result;
}

static void
user_changed (ActUserManager *um, ActUser *user, CcUserPanel *self)
{
        reload_users (self, self->selected_user);
}

static void
select_created_user (GObject *object,
                     GAsyncResult *result,
                     gpointer user_data)
{
        CcUserPanel *self = user_data;
        UmAccountDialog *dialog;
        ActUser *user;

        dialog = UM_ACCOUNT_DIALOG (object);
        user = um_account_dialog_finish (dialog, result);
        gtk_widget_destroy (GTK_WIDGET (dialog));
        self->account_dialog = NULL;

        if (user == NULL)
                return;

        reload_users (self, user);
}

static void
add_user (GtkButton *button, CcUserPanel *self)
{
        self->account_dialog = um_account_dialog_new ();
        um_account_dialog_show (self->account_dialog, GTK_WINDOW (gtk_widget_get_toplevel (self->main_box)),
                                self->permission, select_created_user, self);
}

static void
delete_user_done (ActUserManager *manager,
                  GAsyncResult   *res,
                  CcUserPanel    *self)
{
        GError *error;

        error = NULL;
        if (!act_user_manager_delete_user_finish (manager, res, &error)) {
                if (!g_error_matches (error, ACT_USER_MANAGER_ERROR,
                                      ACT_USER_MANAGER_ERROR_PERMISSION_DENIED))
                        show_error_dialog (self, _("Failed to delete user"), error);

                g_error_free (error);
        }
}

static void
delete_user_response (GtkWidget   *dialog,
                      gint         response_id,
                      CcUserPanel *self)
{
        ActUser *user;
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

        user = get_selected_user (self);

        /* remove autologin */
        if (act_user_get_automatic_login (user)) {
                act_user_set_automatic_login (user, FALSE);
        }

        act_user_manager_delete_user_async (self->um,
                                            user,
                                            remove_files,
                                            NULL,
                                            (GAsyncReadyCallback)delete_user_done,
                                            self);
}

static void
enterprise_user_revoked (GObject *source,
                         GAsyncResult *result,
                         gpointer user_data)
{
        AsyncDeleteData *data = user_data;
        CcUserPanel *self = data->self;
        UmRealmCommon *common = UM_REALM_COMMON (source);
        GError *error = NULL;

        if (g_cancellable_is_cancelled (data->cancellable)) {
                async_delete_data_free (data);
                return;
        }

        um_realm_common_call_change_login_policy_finish (common, result, &error);
        if (error != NULL) {
                show_error_dialog (self, _("Failed to revoke remotely managed user"), error);
                g_error_free (error);
        }

        async_delete_data_free (data);
}

static UmRealmCommon *
find_matching_realm (UmRealmManager *realm_manager, const gchar *login)
{
        UmRealmCommon *common = NULL;
        GList *realms, *l;

        realms = um_realm_manager_get_realms (realm_manager);
        for (l = realms; l != NULL; l = g_list_next (l)) {
                const gchar * const *permitted_logins;
                gint i;

                common = um_realm_object_get_common (l->data);
                if (common == NULL)
                        continue;

                permitted_logins = um_realm_common_get_permitted_logins (common);
                for (i = 0; permitted_logins[i] != NULL; i++) {
                        if (g_strcmp0 (permitted_logins[i], login) == 0)
                                break;
                }

                if (permitted_logins[i] != NULL)
                        break;

                g_clear_object (&common);
        }
        g_list_free_full (realms, g_object_unref);

        return common;
}

static void
realm_manager_found (GObject *source,
                     GAsyncResult *result,
                     gpointer user_data)
{
        AsyncDeleteData *data = user_data;
        CcUserPanel *self = data->self;
        UmRealmCommon *common;
        UmRealmManager *realm_manager;
        const gchar *add[1];
        const gchar *remove[2];
        GVariant *options;
        GError *error = NULL;

        if (g_cancellable_is_cancelled (data->cancellable)) {
                async_delete_data_free (data);
                return;
        }

        realm_manager = um_realm_manager_new_finish (result, &error);
        if (error != NULL) {
                show_error_dialog (self, _("Failed to revoke remotely managed user"), error);
                g_error_free (error);
                async_delete_data_free (data);
                return;
        }

        /* Find matching realm */
        common = find_matching_realm (realm_manager, data->login);
        if (common == NULL) {
                /* The realm was probably left */
                async_delete_data_free (data);
                return;
        }

        /* Remove the user from permitted logins */
        g_debug ("Denying future login for: %s", data->login);

        add[0] = NULL;
        remove[0] = data->login;
        remove[1] = NULL;

        options = g_variant_new_array (G_VARIANT_TYPE ("{sv}"), NULL, 0);
        um_realm_common_call_change_login_policy (common, "",
                                                  add, remove, options,
                                                  data->cancellable,
                                                  enterprise_user_revoked,
                                                  data);

        g_object_unref (common);
}

static void
enterprise_user_uncached (GObject           *source,
                          GAsyncResult      *res,
                          gpointer           user_data)
{
        AsyncDeleteData *data = user_data;
        CcUserPanel *self = data->self;
        ActUserManager *manager = ACT_USER_MANAGER (source);
        GError *error = NULL;

        if (g_cancellable_is_cancelled (data->cancellable)) {
                async_delete_data_free (data);
                return;
        }

        act_user_manager_uncache_user_finish (manager, res, &error);
        if (error == NULL) {
                /* Find realm manager */
                um_realm_manager_new (self->cancellable, realm_manager_found, data);
        }
        else {
                show_error_dialog (self, _("Failed to revoke remotely managed user"), error);
                g_error_free (error);
                async_delete_data_free (data);
        }
}

static void
delete_enterprise_user_response (GtkWidget          *dialog,
                                 gint                response_id,
                                 gpointer            user_data)
{
        CcUserPanel *self = UM_USER_PANEL (user_data);
        AsyncDeleteData *data;
        ActUser *user;

        gtk_widget_destroy (dialog);

        if (response_id != GTK_RESPONSE_ACCEPT) {
                return;
        }

        user = get_selected_user (self);

        data = g_slice_new (AsyncDeleteData);
        data->self = g_object_ref (self);
        data->cancellable = g_object_ref (self->cancellable);
        data->login = g_strdup (act_user_get_user_name (user));

        /* Uncache the user account from the accountsservice */
        g_debug ("Uncaching remote user: %s", data->login);

        act_user_manager_uncache_user_async (self->um, data->login,
                                             data->cancellable,
                                             enterprise_user_uncached,
                                             data);
}

static void
delete_user (GtkButton *button, CcUserPanel *self)
{
        ActUser *user;
        GtkWidget *dialog;

        user = get_selected_user (self);
        if (user == NULL) {
                return;
        }
        else if (act_user_get_uid (user) == getuid ()) {
                dialog = gtk_message_dialog_new (GTK_WINDOW (gtk_widget_get_toplevel (self->main_box)),
                                                 0,
                                                 GTK_MESSAGE_INFO,
                                                 GTK_BUTTONS_CLOSE,
                                                 _("You cannot delete your own account."));
                g_signal_connect (dialog, "response",
                                  G_CALLBACK (gtk_widget_destroy), NULL);
        }
        else if (act_user_is_logged_in_anywhere (user)) {
                dialog = gtk_message_dialog_new (GTK_WINDOW (gtk_widget_get_toplevel (self->main_box)),
                                                 0,
                                                 GTK_MESSAGE_INFO,
                                                 GTK_BUTTONS_CLOSE,
                                                 _("%s is still logged in"),
                                                get_real_or_user_name (user));

                gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
                                                          _("Deleting a user while they are logged in can leave the system in an inconsistent state."));
                g_signal_connect (dialog, "response",
                                  G_CALLBACK (gtk_widget_destroy), NULL);
        }
        else if (act_user_is_local_account (user)) {
                dialog = gtk_message_dialog_new (GTK_WINDOW (gtk_widget_get_toplevel (self->main_box)),
                                                 0,
                                                 GTK_MESSAGE_QUESTION,
                                                 GTK_BUTTONS_NONE,
                                                 _("Do you want to keep %s’s files?"),
                                                get_real_or_user_name (user));

                gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
                                                          _("It is possible to keep the home directory, mail spool and temporary files around when deleting a user account."));

                gtk_dialog_add_buttons (GTK_DIALOG (dialog),
                                        _("_Delete Files"), GTK_RESPONSE_NO,
                                        _("_Keep Files"), GTK_RESPONSE_YES,
                                        _("_Cancel"), GTK_RESPONSE_CANCEL,
                                        NULL);

                gtk_window_set_icon_name (GTK_WINDOW (dialog), "system-users");

                g_signal_connect (dialog, "response",
                                  G_CALLBACK (delete_user_response), self);
        }
        else {
                dialog = gtk_message_dialog_new (GTK_WINDOW (gtk_widget_get_toplevel (self->main_box)),
                                                 0,
                                                 GTK_MESSAGE_QUESTION,
                                                 GTK_BUTTONS_NONE,
                                                 _("Are you sure you want to revoke remotely managed %s’s account?"),
                                                 get_real_or_user_name (user));

                gtk_dialog_add_buttons (GTK_DIALOG (dialog),
                                        _("_Delete"), GTK_RESPONSE_ACCEPT,
                                        _("_Cancel"), GTK_RESPONSE_CANCEL,
                                        NULL);

                gtk_window_set_icon_name (GTK_WINDOW (dialog), "system-users");

                g_signal_connect (dialog, "response",
                                  G_CALLBACK (delete_enterprise_user_response), self);
        }

        g_signal_connect (dialog, "close",
                          G_CALLBACK (gtk_widget_destroy), NULL);

        gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);

        gtk_window_present (GTK_WINDOW (dialog));
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
get_password_mode_text (ActUser *user)
{
        const gchar *text;

        if (act_user_get_locked (user)) {
                text = C_("Password mode", "Account disabled");
        }
        else {
                switch (act_user_get_password_mode (user)) {
                case ACT_USER_PASSWORD_MODE_REGULAR:
                        text = get_invisible_text ();
                        break;
                case ACT_USER_PASSWORD_MODE_SET_AT_LOGIN:
                        text = C_("Password mode", "To be set at next login");
                        break;
                case ACT_USER_PASSWORD_MODE_NONE:
                        text = C_("Password mode", "None");
                        break;
                default:
                        g_assert_not_reached ();
                }
        }

        return text;
}

static void
autologin_changed (GObject     *object,
                   GParamSpec  *pspec,
                   CcUserPanel *self)
{
        gboolean active;
        ActUser *user;

        active = gtk_switch_get_active (GTK_SWITCH (object));
        user = get_selected_user (self);

        if (active != act_user_get_automatic_login (user)) {
                act_user_set_automatic_login (user, active);
                if (act_user_get_automatic_login (user)) {
                        GSList *list;
                        GSList *l;
                        list = act_user_manager_list_users (self->um);
                        for (l = list; l != NULL; l = l->next) {
                                ActUser *u = l->data;
                                if (act_user_get_uid (u) != act_user_get_uid (user)) {
                                        act_user_set_automatic_login (user, FALSE);
                                }
                        }
                        g_slist_free (list);
                }
        }
}

static gchar *
get_login_time_text (ActUser *user)
{
        gchar *text, *date_str, *time_str;
        GDateTime *date_time;
        gint64 time;

        time = act_user_get_login_time (user);
        if (act_user_is_logged_in (user)) {
                text = g_strdup (_("Logged in"));
        }
        else if (time > 0) {
                date_time = g_date_time_new_from_unix_local (time);
                date_str = cc_util_get_smart_date (date_time);
                /* Translators: This is a time format string in the style of "22:58".
                   It indicates a login time which follows a date. */
                time_str = g_date_time_format (date_time, C_("login date-time", "%k:%M"));

                /* Translators: This indicates a login date-time.
                   The first %s is a date, and the second %s a time. */
                text = g_strdup_printf(C_("login date-time", "%s, %s"), date_str, time_str);

                g_date_time_unref (date_time);
                g_free (date_str);
                g_free (time_str);
        }
        else {
                text = g_strdup ("—");
        }

        return text;
}

static gboolean
get_autologin_possible (ActUser *user)
{
        gboolean locked;
        gboolean set_password_at_login;

        locked = act_user_get_locked (user);
        set_password_at_login = (act_user_get_password_mode (user) == ACT_USER_PASSWORD_MODE_SET_AT_LOGIN);

        return !(locked || set_password_at_login);
}

static void on_permission_changed (GPermission *permission, GParamSpec *pspec, gpointer data);

static void
show_user (ActUser *user, CcUserPanel *self)
{
        GtkWidget *image;
        GtkWidget *label;
        gchar *lang, *text, *name;
        GtkWidget *widget;
        gboolean show, enable;
        ActUser *current;

        self->selected_user = user;

        image = get_widget (self, "user-icon-image");
        um_user_image_set_user (UM_USER_IMAGE (image), user);
        image = get_widget (self, "user-icon-image2");
        um_user_image_set_user (UM_USER_IMAGE (image), user);

        um_photo_dialog_set_user (self->photo_dialog, user);

        widget = get_widget (self, "full-name-entry");
        gtk_entry_set_text (GTK_ENTRY (widget), act_user_get_real_name (user));
        gtk_widget_set_tooltip_text (widget, act_user_get_user_name (user));

        widget = get_widget (self, act_user_get_account_type (user) ? "account-type-admin" : "account-type-standard");
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), TRUE);

        /* Do not show the "Account Type" option when there's a single user account. */
        show = (self->other_accounts != 0);
        gtk_widget_set_visible (get_widget (self, "account-type-label"), show);
        gtk_widget_set_visible (get_widget (self, "account-type-box"), show);

        widget = get_widget (self, "account-password-button-label");
        gtk_label_set_label (GTK_LABEL (widget), get_password_mode_text (user));
        enable = act_user_is_local_account (user);
        gtk_widget_set_sensitive (widget, enable);

        widget = get_widget (self, "autologin-switch");
        g_signal_handlers_block_by_func (widget, autologin_changed, self);
        gtk_switch_set_active (GTK_SWITCH (widget), act_user_get_automatic_login (user));
        g_signal_handlers_unblock_by_func (widget, autologin_changed, self);
        gtk_widget_set_sensitive (widget, get_autologin_possible (user));

        widget = get_widget (self, "account-language-button-label");

        name = NULL;
        lang = g_strdup (act_user_get_language (user));
        if ((!lang || *lang == '\0') && act_user_get_uid (user) == getuid ()) {
                lang = cc_common_language_get_current_language ();
                act_user_set_language (user, lang);
        }

        if (lang && *lang != '\0') {
                name = gnome_get_language_from_locale (lang, NULL);
        } else {
                name = g_strdup ("—");
        }

        gtk_label_set_label (GTK_LABEL (widget), name);
        g_free (lang);
        g_free (name);

        /* Fingerprint: show when self, local, enabled, and possible */
        widget = get_widget (self, "account-fingerprint-button");
        label = get_widget (self, "account-fingerprint-label");
        show = (act_user_get_uid (user) == getuid() &&
                act_user_is_local_account (user) &&
                (self->login_screen_settings &&
                 g_settings_get_boolean (self->login_screen_settings, "enable-fingerprint-authentication")) &&
                set_fingerprint_label (widget));
        gtk_widget_set_visible (label, show);
        gtk_widget_set_visible (widget, show);

        /* Autologin: show when local account */
        widget = get_widget (self, "autologin-box");
        label = get_widget (self, "autologin-label");
        show = act_user_is_local_account (user);
        gtk_widget_set_visible (widget, show);
        gtk_widget_set_visible (label, show);

        /* Language: do not show for current user */
        widget = get_widget (self, "account-language-button");
        label = get_widget (self, "language-label");
        show = act_user_get_uid (user) != getuid();
        gtk_widget_set_visible (widget, show);
        gtk_widget_set_visible (label, show);

        /* Last login: show when administrator or current user */
        widget = get_widget (self, "last-login-button");
        label = get_widget (self, "last-login-button-label");

        current = act_user_manager_get_user_by_id (self->um, getuid ());
        show = act_user_get_uid (user) == getuid () ||
               act_user_get_account_type (current) == ACT_USER_ACCOUNT_TYPE_ADMINISTRATOR;
        if (show) {
                text = get_login_time_text (user);
                gtk_label_set_label (GTK_LABEL (label), text);
                g_free (text);
        }
        label = get_widget (self, "last-login-label");
        gtk_widget_set_visible (widget, show);
        gtk_widget_set_visible (label, show);

        enable = act_user_get_login_history (user) != NULL;
        gtk_widget_set_sensitive (widget, enable);

        if (self->permission != NULL)
                on_permission_changed (self->permission, NULL, self);
}

static void
change_name_done (GtkWidget   *entry,
                  CcUserPanel *self)
{
        const gchar *text;
        ActUser *user;

        user = get_selected_user (self);

        text = gtk_entry_get_text (GTK_ENTRY (entry));
        if (g_strcmp0 (text, act_user_get_real_name (user)) != 0 &&
            is_valid_name (text)) {
                act_user_set_real_name (user, text);
        }
}

static void
change_name_focus_out (GtkWidget   *entry,
                       GdkEvent    *event,
                       CcUserPanel *self)
{
        change_name_done (entry, self);
}

static void
account_type_changed (GtkToggleButton    *button,
                      CcUserPanel *self)
{
        ActUser *user;
        gint account_type;
        gboolean self_selected;

        user = get_selected_user (self);
        self_selected = act_user_get_uid (user) == geteuid ();

        account_type = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button)) ?  ACT_USER_ACCOUNT_TYPE_STANDARD : ACT_USER_ACCOUNT_TYPE_ADMINISTRATOR;

        if (account_type != act_user_get_account_type (user)) {
                act_user_set_account_type (user, account_type);

                if (self_selected)
                        show_restart_notification (self, NULL);
        }
}

static void
dismiss_notification (CcUserPanel *self)
{
        gtk_revealer_set_reveal_child (GTK_REVEALER (self->notification), FALSE);
}

static void
restart_now (CcUserPanel *self)
{
        GDBusConnection *bus;

        gtk_revealer_set_reveal_child (GTK_REVEALER (self->notification), FALSE);

        bus = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, NULL);
        g_dbus_connection_call (bus,
                                "org.gnome.SessionManager",
                                "/org/gnome/SessionManager",
                                "org.gnome.SessionManager",
                                "Logout",
                                g_variant_new ("(u)", 0),
                                NULL, 0, G_MAXINT,
                                NULL, NULL, NULL);
        g_object_unref (bus);
}

static void
show_restart_notification (CcUserPanel *self, const gchar *locale)
{
        gchar *current_locale;

        if (locale) {
                current_locale = g_strdup (setlocale (LC_MESSAGES, NULL));
                setlocale (LC_MESSAGES, locale);
        }

        gtk_revealer_set_reveal_child (GTK_REVEALER (self->notification), TRUE);

        if (locale) {
                setlocale (LC_MESSAGES, current_locale);
                g_free (current_locale);
        }
}

static void
language_response (GtkDialog   *dialog,
                   gint         response_id,
                   CcUserPanel *self)
{
        GtkWidget *button;
        ActUser *user;
        const gchar *lang, *account_language;
        gchar *name = NULL;

        if (response_id != GTK_RESPONSE_OK) {
                gtk_widget_hide (GTK_WIDGET (dialog));
                return;
        }

        user = get_selected_user (self);
        account_language = act_user_get_language (user);

        lang = cc_language_chooser_get_language (CC_LANGUAGE_CHOOSER (dialog));
        if (lang) {
                if (g_strcmp0 (lang, account_language) != 0) {
                        act_user_set_language (user, lang);
                }

                button = get_widget (self, "account-language-button-label");
                name = gnome_get_language_from_locale (lang, NULL);
                gtk_label_set_label (GTK_LABEL (button), name);
                g_free (name);
        }

        gtk_widget_hide (GTK_WIDGET (dialog));
}

static void
change_language (GtkButton   *button,
                 CcUserPanel *self)
{
        const gchar *current_language;
        ActUser *user;

        user = get_selected_user (self);
        current_language = act_user_get_language (user);

        if (self->language_chooser) {
		cc_language_chooser_clear_filter (self->language_chooser);
                cc_language_chooser_set_language (self->language_chooser, NULL);
        }
        else {
                self->language_chooser = cc_language_chooser_new ();
                gtk_window_set_transient_for (GTK_WINDOW (self->language_chooser),
                                              GTK_WINDOW (gtk_widget_get_toplevel (self->main_box)));

                g_signal_connect (self->language_chooser, "response",
                                  G_CALLBACK (language_response), self);
                g_signal_connect (self->language_chooser, "delete-event",
                                  G_CALLBACK (gtk_widget_hide_on_delete), NULL);

                gdk_window_set_cursor (gtk_widget_get_window (gtk_widget_get_toplevel (self->main_box)), NULL);
        }

        if (current_language && *current_language != '\0')
                cc_language_chooser_set_language (self->language_chooser, current_language);
        gtk_window_present (GTK_WINDOW (self->language_chooser));
}

static void
change_password (GtkButton *button, CcUserPanel *self)
{
        ActUser *user;

        user = get_selected_user (self);

        um_password_dialog_set_user (self->password_dialog, user);
        um_password_dialog_show (self->password_dialog,
                                  GTK_WINDOW (gtk_widget_get_toplevel (self->main_box)));
}

static void
change_fingerprint (GtkButton *button, CcUserPanel *self)
{
        GtkWidget *widget;
        ActUser *user;

        user = get_selected_user (self);

        g_assert (g_strcmp0 (g_get_user_name (), act_user_get_user_name (user)) == 0);

        widget = get_widget (self, "account-fingerprint-button");
        fingerprint_button_clicked (GTK_WINDOW (gtk_widget_get_toplevel (self->main_box)), widget, user);
}

static void
show_history (GtkButton *button, CcUserPanel *self)
{
        ActUser *user;

        user = get_selected_user (self);

        um_history_dialog_set_user (self->history_dialog, user);
        um_history_dialog_show (self->history_dialog, GTK_WINDOW (gtk_widget_get_toplevel (self->main_box)));
}

static void
users_loaded (ActUserManager *manager,
              GParamSpec     *pspec,
              CcUserPanel    *self)
{
        GtkWidget *dialog;

        if (act_user_manager_no_service (self->um)) {
                dialog = gtk_message_dialog_new (GTK_WINDOW (gtk_widget_get_toplevel (self->main_box)),
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

                gtk_widget_set_sensitive (self->main_box, FALSE);
        }

        g_signal_connect (self->um, "user-changed", G_CALLBACK (user_changed), self);
        g_signal_connect (self->um, "user-is-logged-in-changed", G_CALLBACK (user_changed), self);
        g_signal_connect (self->um, "user-added", G_CALLBACK (user_added), self);
        g_signal_connect (self->um, "user-removed", G_CALLBACK (user_changed), self);

        reload_users (self, NULL);
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
        setup_tooltip_with_embedded_icon (button,
                                          /* Translator comments:
                                           * We split the line in 2 here to "make it look good", as there's
                                           * no good way to do this in GTK+ for tooltips. See:
                                           * https://bugzilla.gnome.org/show_bug.cgi?id=657168 */
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
        CcUserPanel *self = data;
        gboolean is_authorized;
        gboolean self_selected;
        ActUser *user;
        GtkWidget *widget;

        user = get_selected_user (self);
        if (!user) {
                return;
        }

        is_authorized = g_permission_get_allowed (G_PERMISSION (self->permission));
        self_selected = act_user_get_uid (user) == geteuid ();

        gtk_stack_set_visible_child_name (GTK_STACK (self->headerbar_buttons), is_authorized ? PAGE_ADDUSER : PAGE_LOCK);

        widget = get_widget (self, "add-user-toolbutton");
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

        widget = get_widget (self, "remove-user-toolbutton");
        gtk_widget_set_sensitive (widget, is_authorized && !self_selected
                                  && !would_demote_only_admin (user));
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

        if (!act_user_is_local_account (user)) {
                gtk_widget_set_sensitive (get_widget (self, "account-type-box"), FALSE);
                remove_unlock_tooltip (get_widget (self, "account-type-box"));
                gtk_widget_set_sensitive (GTK_WIDGET (get_widget (self, "autologin-switch")), FALSE);
                remove_unlock_tooltip (get_widget (self, "autologin-switch"));

        } else if (is_authorized && act_user_is_local_account (user)) {
                if (would_demote_only_admin (user)) {
                        gtk_widget_set_sensitive (get_widget (self, "account-type-box"), FALSE);
                } else {
                        gtk_widget_set_sensitive (get_widget (self, "account-type-box"), TRUE);
                }
                remove_unlock_tooltip (get_widget (self, "account-type-box"));

                gtk_widget_set_sensitive (GTK_WIDGET (get_widget (self, "autologin-switch")), get_autologin_possible (user));
                remove_unlock_tooltip (get_widget (self, "autologin-switch"));
        }
        else {
                gtk_widget_set_sensitive (get_widget (self, "account-type-box"), FALSE);
                if (would_demote_only_admin (user)) {
                        remove_unlock_tooltip (get_widget (self, "account-type-box"));
                } else {
                        add_unlock_tooltip (get_widget (self, "account-type-box"));
                }
                gtk_widget_set_sensitive (GTK_WIDGET (get_widget (self, "autologin-switch")), FALSE);
                add_unlock_tooltip (get_widget (self, "autologin-switch"));
        }

        /* The full name entry: insensitive if remote or not authorized and not self */
        widget = get_widget (self, "full-name-entry");
        if (!act_user_is_local_account (user)) {
                gtk_widget_set_sensitive (widget, FALSE);
                remove_unlock_tooltip (widget);

        } else if (is_authorized || self_selected) {
                gtk_widget_set_sensitive (widget, TRUE);
                remove_unlock_tooltip (widget);

        } else {
                gtk_widget_set_sensitive (widget, FALSE);
                add_unlock_tooltip (widget);
        }

        if (is_authorized || self_selected) {
                gtk_stack_set_visible_child (GTK_STACK (get_widget (self, "user-icon")),
                                             get_widget (self, "user-icon-button"));

                gtk_widget_set_sensitive (get_widget (self, "account-language-button"), TRUE);
                remove_unlock_tooltip (get_widget (self, "account-language-button"));

                gtk_widget_set_sensitive (get_widget (self, "account-password-button"), TRUE);
                remove_unlock_tooltip (get_widget (self, "account-password-button"));

                gtk_widget_set_sensitive (get_widget (self, "account-fingerprint-button"), TRUE);
                remove_unlock_tooltip (get_widget (self, "account-fingerprint-button"));

                gtk_widget_set_sensitive (get_widget (self, "last-login-button"), TRUE);
                remove_unlock_tooltip (get_widget (self, "last-login-button"));
        }
        else {
                gtk_stack_set_visible_child (GTK_STACK (get_widget (self, "user-icon")),
                                             get_widget (self, "user-icon-image"));

                gtk_widget_set_sensitive (get_widget (self, "account-language-button"), FALSE);
                add_unlock_tooltip (get_widget (self, "account-language-button"));

                gtk_widget_set_sensitive (get_widget (self, "account-password-button"), FALSE);
                add_unlock_tooltip (get_widget (self, "account-password-button"));

                gtk_widget_set_sensitive (get_widget (self, "account-fingerprint-button"), FALSE);
                add_unlock_tooltip (get_widget (self, "account-fingerprint-button"));

                gtk_widget_set_sensitive (get_widget (self, "last-login-button"), FALSE);
                add_unlock_tooltip (get_widget (self, "last-login-button"));
        }

        um_password_dialog_set_user (self->password_dialog, user);
}

static void
setup_main_window (CcUserPanel *self)
{
        GtkWidget *button;
        GIcon *icon;
        GError *error = NULL;
        gchar *names[3];
        gboolean loaded;

        self->notification = get_widget (self, "notification");

        button = get_widget (self, "restart-button");
        g_signal_connect_swapped (button, "clicked", G_CALLBACK (restart_now), self);

        button = get_widget (self, "dismiss-button");
        g_signal_connect_swapped (button, "clicked", G_CALLBACK (dismiss_notification), self);

        self->other_accounts = 0;

        self->carousel = UM_CAROUSEL (get_widget (self, "carousel"));
        g_signal_connect (self->carousel, "item-activated", G_CALLBACK (set_selected_user), self);

        button = get_widget (self, "add-user-toolbutton");
        g_signal_connect (button, "clicked", G_CALLBACK (add_user), self);

        button = get_widget (self, "remove-user-toolbutton");
        g_signal_connect (button, "clicked", G_CALLBACK (delete_user), self);

        button = get_widget (self, "user-icon-image");
        add_unlock_tooltip (button);

        button = get_widget (self, "full-name-entry");
        g_signal_connect (button, "activate", G_CALLBACK (change_name_done), self);
        g_signal_connect (button, "focus-out-event", G_CALLBACK (change_name_focus_out), self);

        button = get_widget (self, "account-type-standard");
        g_signal_connect (button, "toggled", G_CALLBACK (account_type_changed), self);

        button = get_widget (self, "account-password-button");
        g_signal_connect (button, "clicked", G_CALLBACK (change_password), self);

        button = get_widget (self, "account-language-button");
        g_signal_connect (button, "clicked", G_CALLBACK (change_language), self);

        button = get_widget (self, "autologin-switch");
        g_signal_connect (button, "notify::active", G_CALLBACK (autologin_changed), self);

        button = get_widget (self, "account-fingerprint-button");
        g_signal_connect (button, "clicked",
                          G_CALLBACK (change_fingerprint), self);

        button = get_widget (self, "last-login-button");
        g_signal_connect (button, "clicked",
                          G_CALLBACK (show_history), self);

        self->permission = (GPermission *)polkit_permission_new_sync (USER_ACCOUNTS_PERMISSION, NULL, NULL, &error);
        if (self->permission != NULL) {
                g_signal_connect (self->permission, "notify",
                                  G_CALLBACK (on_permission_changed), self);
                on_permission_changed (self->permission, NULL, self);
        } else {
                g_warning ("Cannot create '%s' permission: %s", USER_ACCOUNTS_PERMISSION, error->message);
                g_error_free (error);
        }

        button = get_widget (self, "add-user-toolbutton");
        names[0] = "changes-allow-symbolic";
        names[1] = "changes-allow";
        names[2] = NULL;
        icon = (GIcon *)g_themed_icon_new_from_names (names, -1);
        setup_tooltip_with_embedded_icon (button,
                                          _("To create a user account,\nclick the * icon first"),
                                          "*",
                                          icon);
        button = get_widget (self, "remove-user-toolbutton");
        setup_tooltip_with_embedded_icon (button,
                                          _("To delete the selected user account,\nclick the * icon first"),
                                          "*",
                                          icon);
        g_object_unref (icon);

        g_object_get (self->um, "is-loaded", &loaded, NULL);
        if (loaded)
                users_loaded (self->um, NULL, self);
        else
                g_signal_connect (self->um, "notify::is-loaded", G_CALLBACK (users_loaded), self);
}

static GSettings *
settings_or_null (const gchar *schema)
{
        GSettingsSchemaSource *source = NULL;
        gchar **non_relocatable = NULL;
        gchar **relocatable = NULL;
        GSettings *settings = NULL;

        source = g_settings_schema_source_get_default ();
        if (!source)
                return NULL;

        g_settings_schema_source_list_schemas (source, TRUE, &non_relocatable, &relocatable);

        if (g_strv_contains ((const gchar * const *)non_relocatable, schema) ||
            g_strv_contains ((const gchar * const *)relocatable, schema))
                settings = g_settings_new (schema);

        g_strfreev (non_relocatable);
        g_strfreev (relocatable);
        return settings;
}

static void
cc_user_panel_constructed (GObject *object)
{
        CcUserPanel *self = UM_USER_PANEL (object);
        GtkWidget *button;
        CcShell *shell;

        G_OBJECT_CLASS (cc_user_panel_parent_class)->constructed (object);

        shell = cc_panel_get_shell (CC_PANEL (self));
        cc_shell_embed_widget_in_header (shell, self->headerbar_buttons);

        button = get_widget (self, "lock-button");
        gtk_lock_button_set_permission (GTK_LOCK_BUTTON (button), self->permission);
}

static void
cc_user_panel_init (CcUserPanel *self)
{
        GError *error;
        volatile GType type G_GNUC_UNUSED;
        GtkWidget *button;
        GtkCssProvider *provider;

        g_resources_register (um_get_resource ());

        /* register types that the builder might need */
        type = um_user_image_get_type ();
        type = um_carousel_get_type ();

        self->builder = gtk_builder_new ();
        self->um = act_user_manager_get_default ();
        self->cancellable = g_cancellable_new ();

        error = NULL;
        if (!gtk_builder_add_from_resource (self->builder,
                                            "/org/gnome/control-center/user-accounts/user-accounts-dialog.ui",
                                            &error)) {
                g_error ("%s", error->message);
                g_error_free (error);
                return;
        }

        provider = gtk_css_provider_new ();
        gtk_css_provider_load_from_resource (provider, "/org/gnome/control-center/user-accounts/user-accounts-dialog.css");
        gtk_style_context_add_provider_for_screen (gdk_screen_get_default (),
                                                   GTK_STYLE_PROVIDER (provider),
                                                   GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
        g_object_unref (provider);

        self->headerbar_buttons = get_widget (self, "headerbar-buttons");
        self->stack = get_widget (self, "stack");
        self->login_screen_settings = settings_or_null ("org.gnome.login-screen");

        self->password_dialog = um_password_dialog_new ();
        button = get_widget (self, "user-icon-button");
        self->photo_dialog = um_photo_dialog_new (button);
        self->main_box = get_widget (self, "accounts-vbox");
        gtk_container_add (GTK_CONTAINER (self), self->stack);
        self->history_dialog = um_history_dialog_new ();
        setup_main_window (self);
}

static void
cc_user_panel_dispose (GObject *object)
{
        CcUserPanel *self = UM_USER_PANEL (object);

        g_cancellable_cancel (self->cancellable);
        g_clear_object (&self->cancellable);

        g_clear_object (&self->login_screen_settings);

        if (self->um) {
                g_signal_handlers_disconnect_by_data (self->um, self);
                self->um = NULL;
        }
        g_clear_object (&self->builder);
        g_clear_pointer (&self->password_dialog, um_password_dialog_free);
        g_clear_pointer (&self->history_dialog, um_history_dialog_free);
        if (self->account_dialog) {
                gtk_dialog_response (GTK_DIALOG (self->account_dialog), GTK_RESPONSE_DELETE_EVENT);
                self->account_dialog = NULL;
        }
        g_clear_pointer (&self->language_chooser, gtk_widget_destroy);
        g_clear_object (&self->permission);
        G_OBJECT_CLASS (cc_user_panel_parent_class)->dispose (object);
}

static const char *
cc_user_panel_get_help_uri (CcPanel *panel)
{
	return "help:gnome-help/user-accounts";
}

static void
cc_user_panel_class_init (CcUserPanelClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);
        CcPanelClass *panel_class = CC_PANEL_CLASS (klass);

        object_class->dispose = cc_user_panel_dispose;
        object_class->constructed = cc_user_panel_constructed;

        panel_class->get_help_uri = cc_user_panel_get_help_uri;
}
