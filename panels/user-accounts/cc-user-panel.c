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

#include "cc-user-panel.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <locale.h>
#include <errno.h>

#include <glib.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <polkit/polkit.h>
#include <act/act.h>
#include <cairo-gobject.h>

#define GNOME_DESKTOP_USE_UNSTABLE_API
#include <libgnome-desktop/gnome-languages.h>

#ifdef HAVE_MALCONTENT
#include <libmalcontent/malcontent.h>
#endif

#include "cc-add-user-dialog.h"
#include "cc-avatar-chooser.h"
#include "cc-language-chooser.h"
#include "cc-login-history-dialog.h"
#include "cc-password-dialog.h"
#include "cc-realm-manager.h"
#include "cc-user-accounts-resources.h"
#include "cc-fingerprint-manager.h"
#include "cc-fingerprint-dialog.h"
#include "user-utils.h"

#include "cc-common-language.h"
#include "cc-permission-infobar.h"
#include "cc-util.h"

#define USER_ACCOUNTS_PERMISSION "org.gnome.controlcenter.user-accounts.administration"

struct _CcUserPanel {
        CcPanel parent_instance;

        ActUserManager *um;
        GSettings *login_screen_settings;

        GtkBox          *account_settings_box;
        GtkListBoxRow   *account_type_row;
        GtkSwitch       *account_type_switch;
        GtkWidget       *add_user_button;
        GtkListBoxRow   *autologin_row;
        GtkSwitch       *autologin_switch;
        GtkButton       *back_button;
        GtkLabel        *fingerprint_state_label;
        GtkListBoxRow   *fingerprint_row;
        GtkStack        *full_name_stack;
        GtkLabel        *full_name_label;
        GtkToggleButton *full_name_edit_button;
        GtkEntry        *full_name_entry;
        GtkLabel        *language_button_label;
        GtkListBoxRow   *language_row;
        GtkLabel        *last_login_button_label;
        GtkListBoxRow   *last_login_row;
        GtkWidget       *no_users_box;
        GtkRevealer     *notification_revealer;
        AdwPreferencesGroup *other_users;
        GtkListBox      *other_users_listbox;
        AdwPreferencesRow *other_users_row;
        GtkLabel        *password_button_label;
#ifdef HAVE_MALCONTENT
        GtkLabel        *parental_controls_button_label;
        GtkListBoxRow   *parental_controls_row;
#endif
        GtkListBoxRow   *password_row;
        CcPermissionInfobar *permission_infobar;
        GtkButton       *remove_user_button;
        GtkStack        *stack;
        AdwAvatar       *user_avatar;
        GtkMenuButton   *user_avatar_edit_button;
        GtkOverlay      *users_overlay;

        ActUser *selected_user;
        GPermission *permission;
        CcLanguageChooser *language_chooser;
        GListStore *other_users_model;

        CcAvatarChooser *avatar_chooser;

        CcFingerprintManager *fingerprint_manager;
};

CC_PANEL_REGISTER (CcUserPanel, cc_user_panel)

static void show_restart_notification (CcUserPanel *self, const gchar *locale);

typedef struct {
        CcUserPanel *self;
        GCancellable *cancellable;
        gchar *login;
} AsyncDeleteData;

static void
async_delete_data_free (AsyncDeleteData *data)
{
        g_clear_object (&data->self);
        g_clear_object (&data->cancellable);
        g_clear_pointer (&data->login, g_free);
        g_slice_free (AsyncDeleteData, data);
}

G_DEFINE_AUTOPTR_CLEANUP_FUNC (AsyncDeleteData, async_delete_data_free)

static void
show_error_dialog (CcUserPanel *self,
                   const gchar *message,
                   GError *error)
{
        GtkWidget *dialog;

        dialog = gtk_message_dialog_new (GTK_WINDOW (gtk_widget_get_native (GTK_WIDGET (self))),
                                         GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_USE_HEADER_BAR,
                                         GTK_MESSAGE_ERROR,
                                         GTK_BUTTONS_CLOSE,
                                         "%s", message);

        if (error != NULL) {
                g_dbus_error_strip_remote_error (error);
                gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
                                                          "%s", error->message);
        }

        g_signal_connect (dialog, "response", G_CALLBACK (gtk_window_destroy), NULL);
        gtk_window_present (GTK_WINDOW (dialog));
}

static void show_user (ActUser *user, CcUserPanel *self);

static ActUser *
get_selected_user (CcUserPanel *self)
{
        return self->selected_user;
}

static void
set_selected_user (CcUserPanel  *self,
                   AdwActionRow *row)
{
        uid_t uid;
 
        uid = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (row), "uid"));
        g_set_object (&self->selected_user,
                      act_user_manager_get_user_by_id (self->um, uid));
        show_user (self->selected_user, self);
}

static void
show_current_user (CcUserPanel *self)
{
        ActUser *user;

        user = act_user_manager_get_user_by_id (self->um, getuid ()); 
        if (user != NULL)
            show_user (user, self);
}


static void
on_back_button_clicked_cb (CcUserPanel *self)
{

        if (act_user_get_uid (self->selected_user) == getuid ()) {
                gtk_widget_activate_action (GTK_WIDGET (self),
                                            "window.navigate",
                                            "i",
                                            ADW_NAVIGATION_DIRECTION_BACK);
        } else {
                show_current_user (self);
        }
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

static void
setup_avatar_for_user (AdwAvatar *avatar, ActUser *user)
{
        const gchar *avatar_file;

        adw_avatar_set_custom_image (avatar, NULL);
        adw_avatar_set_text (avatar, get_real_or_user_name (user));

        avatar_file = act_user_get_icon_file (user);
        if (avatar_file) {
                g_autoptr(GdkPixbuf) pixbuf = NULL;

                pixbuf = gdk_pixbuf_new_from_file_at_size (avatar_file,
                                                           adw_avatar_get_size (avatar),
                                                           adw_avatar_get_size (avatar),
                                                           NULL);
                if (pixbuf) {
                        adw_avatar_set_custom_image (avatar,
                                                     GDK_PAINTABLE (gdk_texture_new_for_pixbuf (pixbuf)));
                }
        }
}

static GtkWidget *
create_user_row (gpointer item,
                 gpointer user_data)
{
        ActUser *user = ACT_USER (item);
        GtkWidget *row, *user_image;

        row = adw_action_row_new ();
        g_object_set_data (G_OBJECT (row), "uid", GINT_TO_POINTER (act_user_get_uid (user)));
        gtk_list_box_row_set_activatable (GTK_LIST_BOX_ROW (row), TRUE); 
        adw_preferences_row_set_title (ADW_PREFERENCES_ROW (row),
                                       get_real_or_user_name (user));
        user_image = adw_avatar_new (48, NULL, TRUE);
        setup_avatar_for_user (ADW_AVATAR (user_image), user);
        adw_action_row_add_prefix (ADW_ACTION_ROW (row), user_image);

        return row;
}

static gint
sort_users (gconstpointer a, gconstpointer b, gpointer user_data)
{
        ActUser *ua, *ub;

        ua = ACT_USER (a);
        ub = ACT_USER (b);

        /* Make sure the current user is shown first */
        if (act_user_get_uid (ua) == getuid ()) {
                return -G_MAXINT32;
        }
        else if (act_user_get_uid (ub) == getuid ()) {
                return G_MAXINT32;
        }
        else {
                g_autofree gchar *name1 = NULL;
                g_autofree gchar *name2 = NULL;

                name1 = g_utf8_collate_key (get_real_or_user_name (ua), -1);
                name2 = g_utf8_collate_key (get_real_or_user_name (ub), -1);

                return strcmp (name1, name2);
        }
}

static void
user_changed (CcUserPanel *self, ActUser *user)
{
        GSList *user_list, *l;
        gboolean show;

        g_list_store_remove_all (self->other_users_model);
        user_list = act_user_manager_list_users (self->um);
        for (l = user_list; l; l = l->next) {
                ActUser *other_user = ACT_USER (l->data);

                if (act_user_is_system_account (other_user)) {
                        continue;
                }

                if (act_user_get_uid (other_user) == getuid ()) {
                        continue;
                }

                g_list_store_insert_sorted (self->other_users_model,
                                            other_user,
                                            sort_users,
                                            self);
        }

        if (self->selected_user == user)
                show_user (user, self);

        show = g_list_model_get_n_items (G_LIST_MODEL (self->other_users_model)) > 0;
        gtk_widget_set_visible (GTK_WIDGET (self->other_users_row), show);
}

static void
on_add_user_dialog_response (CcUserPanel     *self,
                             gint             response,
                             CcAddUserDialog *dialog)
{
        ActUser *user;

        user = cc_add_user_dialog_get_user (dialog);
        if (user != NULL) {
                set_default_avatar (user);
                show_user (user, self);
        }

        gtk_window_destroy (GTK_WINDOW (dialog));
}

static void
add_user (CcUserPanel *self)
{
        CcAddUserDialog *dialog;
        GtkWindow *toplevel;

        dialog = cc_add_user_dialog_new (self->permission);
        toplevel = GTK_WINDOW (gtk_widget_get_native (GTK_WIDGET (self)));
        gtk_window_set_transient_for (GTK_WINDOW (dialog), toplevel);

        gtk_window_present (GTK_WINDOW (dialog));
        g_signal_connect_object (dialog, "response", G_CALLBACK (on_add_user_dialog_response),
                                 self, G_CONNECT_SWAPPED);
}

static void
delete_user_done (ActUserManager *manager,
                  GAsyncResult   *res,
                  CcUserPanel    *self)
{
        g_autoptr(GError) error = NULL;

        if (!act_user_manager_delete_user_finish (manager, res, &error)) {
                if (!g_error_matches (error, ACT_USER_MANAGER_ERROR,
                                      ACT_USER_MANAGER_ERROR_PERMISSION_DENIED))
                        show_error_dialog (self, _("Failed to delete user"), error);
        }

        show_current_user (self);
}

static void
delete_user_response (CcUserPanel *self,
                      gint         response_id,
                      GtkWidget   *dialog)
{
        ActUser *user;
        gboolean remove_files;

        gtk_window_destroy (GTK_WINDOW (dialog));

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
        g_autoptr(AsyncDeleteData) data = user_data;
        CcUserPanel *self = data->self;
        CcRealmCommon *common = CC_REALM_COMMON (source);
        g_autoptr(GError) error = NULL;

        if (g_cancellable_is_cancelled (data->cancellable)) {
                return;
        }

        cc_realm_common_call_change_login_policy_finish (common, result, &error);
        if (error != NULL) {
                show_error_dialog (self, _("Failed to revoke remotely managed user"), error);
        }
}

static CcRealmCommon *
find_matching_realm (CcRealmManager *realm_manager, const gchar *login)
{
        CcRealmCommon *common = NULL;
        GList *realms;

        realms = cc_realm_manager_get_realms (realm_manager);
        for (GList *l = realms; l != NULL; l = g_list_next (l)) {
                const gchar * const *permitted_logins;
                gint i;

                common = cc_realm_object_get_common (l->data);
                if (common == NULL)
                        continue;

                permitted_logins = cc_realm_common_get_permitted_logins (common);
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
        g_autoptr(AsyncDeleteData) data = user_data;
        CcUserPanel *self = data->self;
        g_autoptr(CcRealmCommon) common = NULL;
        CcRealmManager *realm_manager;
        const gchar *add[1];
        const gchar *remove[2];
        GVariant *options;
        g_autoptr(GError) error = NULL;

        if (g_cancellable_is_cancelled (data->cancellable)) {
                return;
        }

        realm_manager = cc_realm_manager_new_finish (result, &error);
        if (error != NULL) {
                show_error_dialog (self, _("Failed to revoke remotely managed user"), error);
                return;
        }

        /* Find matching realm */
        common = find_matching_realm (realm_manager, data->login);
        if (common == NULL) {
                /* The realm was probably left */
                return;
        }

        /* Remove the user from permitted logins */
        g_debug ("Denying future login for: %s", data->login);

        add[0] = NULL;
        remove[0] = data->login;
        remove[1] = NULL;

        options = g_variant_new_array (G_VARIANT_TYPE ("{sv}"), NULL, 0);
        cc_realm_common_call_change_login_policy (common, "",
                                                  add, remove, options,
                                                  data->cancellable,
                                                  enterprise_user_revoked,
                                                  g_steal_pointer (&data));
}

static void
enterprise_user_uncached (GObject           *source,
                          GAsyncResult      *res,
                          gpointer           user_data)
{
        g_autoptr(AsyncDeleteData) data = user_data;
        CcUserPanel *self = data->self;
        ActUserManager *manager = ACT_USER_MANAGER (source);
        g_autoptr(GError) error = NULL;

        if (g_cancellable_is_cancelled (data->cancellable)) {
                return;
        }

        act_user_manager_uncache_user_finish (manager, res, &error);
        if (error == NULL) {
                /* Find realm manager */
                cc_realm_manager_new (cc_panel_get_cancellable (CC_PANEL (self)), realm_manager_found, g_steal_pointer (&data));
        }
        else {
                show_error_dialog (self, _("Failed to revoke remotely managed user"), error);
        }
}

static void
delete_enterprise_user_response (CcUserPanel *self,
                                 gint         response_id,
                                 GtkWidget   *dialog)
{
        AsyncDeleteData *data;
        ActUser *user;

        gtk_window_destroy (GTK_WINDOW (dialog));

        if (response_id != GTK_RESPONSE_ACCEPT) {
                return;
        }

        user = get_selected_user (self);

        data = g_slice_new (AsyncDeleteData);
        data->self = g_object_ref (self);
        data->cancellable = g_object_ref (cc_panel_get_cancellable (CC_PANEL (self)));
        data->login = g_strdup (act_user_get_user_name (user));

        /* Uncache the user account from the accountsservice */
        g_debug ("Uncaching remote user: %s", data->login);

        act_user_manager_uncache_user_async (self->um, data->login,
                                             data->cancellable,
                                             enterprise_user_uncached,
                                             data);
}

static void
delete_user (CcUserPanel *self)
{
        ActUser *user;
        GtkWidget *dialog;

        user = get_selected_user (self);
        if (user == NULL) {
                return;
        }
        else if (act_user_get_uid (user) == getuid ()) {
                dialog = gtk_message_dialog_new (GTK_WINDOW (gtk_widget_get_native (GTK_WIDGET (self))),
                                                 0,
                                                 GTK_MESSAGE_INFO,
                                                 GTK_BUTTONS_CLOSE,
                                                 _("You cannot delete your own account."));
                g_signal_connect (dialog, "response",
                                  G_CALLBACK (gtk_window_destroy), NULL);
        }
        else if (act_user_is_logged_in_anywhere (user)) {
                dialog = gtk_message_dialog_new (GTK_WINDOW (gtk_widget_get_native (GTK_WIDGET (self))),
                                                 0,
                                                 GTK_MESSAGE_INFO,
                                                 GTK_BUTTONS_CLOSE,
                                                 _("%s is still logged in"),
                                                get_real_or_user_name (user));

                gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
                                                          _("Deleting a user while they are logged in can leave the system in an inconsistent state."));
                g_signal_connect (dialog, "response",
                                  G_CALLBACK (gtk_window_destroy), NULL);
        }
        else if (act_user_is_local_account (user)) {
                dialog = gtk_message_dialog_new (GTK_WINDOW (gtk_widget_get_native (GTK_WIDGET (self))),
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

                g_signal_connect_object (dialog, "response",
                                         G_CALLBACK (delete_user_response), self, G_CONNECT_SWAPPED);
        }
        else {
                dialog = gtk_message_dialog_new (GTK_WINDOW (gtk_widget_get_native (GTK_WIDGET (self))),
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

                g_signal_connect_object (dialog, "response",
                                         G_CALLBACK (delete_enterprise_user_response), self, G_CONNECT_SWAPPED);
        }

        g_signal_connect (dialog, "close",
                          G_CALLBACK (gtk_window_destroy), NULL);

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
autologin_changed (CcUserPanel *self)
{
        gboolean active;
        ActUser *user;

        active = gtk_switch_get_active (self->autologin_switch);
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
        gint64 time;

        time = act_user_get_login_time (user);
        if (act_user_is_logged_in (user)) {
                return g_strdup (_("Logged in"));
        }
        else if (time > 0) {
                g_autoptr(GDateTime) date_time = NULL;
                g_autofree gchar *date_str = NULL;
                g_autofree gchar *time_str = NULL;

                date_time = g_date_time_new_from_unix_local (time);
                date_str = cc_util_get_smart_date (date_time);

                /* Translators: This is a time format string in the style of "22:58".
                   It indicates a login time which follows a date. */
                time_str = g_date_time_format (date_time, C_("login date-time", "%k:%M"));

                /* Translators: This indicates a login date-time.
                   The first %s is a date, and the second %s a time. */
                return g_strdup_printf(C_("login date-time", "%s, %s"), date_str, time_str);
        }
        else {
                return g_strdup ("—");
        }
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

static void on_permission_changed (CcUserPanel *self);
static void full_name_edit_button_toggled (CcUserPanel *self);

#ifdef HAVE_MALCONTENT
static gboolean
is_parental_controls_enabled_for_user (ActUser *user)
{
        g_autoptr(MctManager) manager = NULL;
        g_autoptr(MctAppFilter) app_filter = NULL;
        g_autoptr(GDBusConnection) system_bus = NULL;
        g_autoptr(GError) error = NULL;

        /* FIXME: should become asynchronous */
        system_bus = g_bus_get_sync (G_BUS_TYPE_SYSTEM, NULL, &error);
        if (system_bus == NULL) {
            g_warning ("Error getting system bus while trying to show user details: %s", error->message);
            return FALSE;
        }

        manager = mct_manager_new (system_bus);
        app_filter = mct_manager_get_app_filter (manager,
                                                 act_user_get_uid (user),
                                                 MCT_GET_APP_FILTER_FLAGS_NONE,
                                                 NULL,
                                                 &error);
        if (error) {
                if (!g_error_matches (error, MCT_MANAGER_ERROR, MCT_MANAGER_ERROR_DISABLED))
                        g_warning ("Error retrieving app filter for user %s: %s",
                                   act_user_get_user_name (user),
                                   error->message);

                return FALSE;
        }

        return mct_app_filter_is_enabled (app_filter);
}
#endif

static void
update_fingerprint_row_state (CcUserPanel *self, GParamSpec *spec, CcFingerprintManager *fingerprint_manager)
{
        CcFingerprintState state = cc_fingerprint_manager_get_state (fingerprint_manager);

        if (state != CC_FINGERPRINT_STATE_UPDATING) {
                gtk_widget_set_visible (GTK_WIDGET (self->fingerprint_row),
                                        state != CC_FINGERPRINT_STATE_NONE);
        }

        gtk_widget_set_sensitive (GTK_WIDGET (self->fingerprint_row),
                                  state != CC_FINGERPRINT_STATE_UPDATING);

        if (state == CC_FINGERPRINT_STATE_ENABLED)
                gtk_label_set_text (self->fingerprint_state_label, _("Enabled"));
        else if (state == CC_FINGERPRINT_STATE_DISABLED)
                gtk_label_set_text (self->fingerprint_state_label, _("Disabled"));
}

static void
show_or_hide_back_button (CcUserPanel *self)
{
        gboolean show;
        gboolean folded;

        g_object_get(self, "folded", &folded, NULL);

        show = folded || act_user_get_uid (self->selected_user) != getuid();

        gtk_widget_set_visible (GTK_WIDGET (self->back_button), show);
}

static void
show_user (ActUser *user, CcUserPanel *self)
{
        g_autofree gchar *lang = NULL;
        g_autofree gchar *name = NULL;
        gboolean show, enable;
        ActUser *current;
#ifdef HAVE_MALCONTENT
        g_autofree gchar *malcontent_control_path = NULL;
#endif

        g_set_object (&self->selected_user, user);

        setup_avatar_for_user (self->user_avatar, user);
        cc_avatar_chooser_set_user (self->avatar_chooser, user);

        gtk_label_set_label (self->full_name_label, get_real_or_user_name (user));
        gtk_editable_set_text (GTK_EDITABLE (self->full_name_entry), gtk_label_get_label (self->full_name_label));
        gtk_widget_set_tooltip_text (GTK_WIDGET (self->full_name_label), get_real_or_user_name (user));

        g_signal_handlers_block_by_func (self->full_name_edit_button, full_name_edit_button_toggled, self);
        gtk_stack_set_visible_child (self->full_name_stack, GTK_WIDGET (self->full_name_label));
        gtk_toggle_button_set_active (self->full_name_edit_button, FALSE);
        g_signal_handlers_unblock_by_func (self->full_name_edit_button, full_name_edit_button_toggled, self);

        enable = (act_user_get_account_type (user) == ACT_USER_ACCOUNT_TYPE_ADMINISTRATOR);
        gtk_switch_set_active (self->account_type_switch, enable);

        gtk_label_set_label (self->password_button_label, get_password_mode_text (user));
        enable = act_user_is_local_account (user);
        gtk_widget_set_sensitive (GTK_WIDGET (self->password_button_label), enable);

        g_signal_handlers_block_by_func (self->autologin_switch, autologin_changed, self);
        gtk_switch_set_active (self->autologin_switch, act_user_get_automatic_login (user));
        g_signal_handlers_unblock_by_func (self->autologin_switch, autologin_changed, self);
        gtk_widget_set_sensitive (GTK_WIDGET (self->autologin_switch), get_autologin_possible (user));

        lang = g_strdup (act_user_get_language (user));
        if (lang && *lang != '\0') {
                name = gnome_get_language_from_locale (lang, NULL);
        } else {
                name = g_strdup ("—");
        }
        gtk_label_set_label (self->language_button_label, name);

        /* Fingerprint: show when self, local, enabled, and possible */
        show = (act_user_get_uid (user) == getuid() &&
                act_user_is_local_account (user) &&
                (self->login_screen_settings &&
                 g_settings_get_boolean (self->login_screen_settings,
                                         "enable-fingerprint-authentication")));

        if (show) {
                if (!self->fingerprint_manager) {
                        self->fingerprint_manager = cc_fingerprint_manager_new (user);
                        g_signal_connect_object (self->fingerprint_manager,
                                                 "notify::state",
                                                 G_CALLBACK (update_fingerprint_row_state),
                                                 self, G_CONNECT_SWAPPED);
                }

                update_fingerprint_row_state (self, NULL, self->fingerprint_manager);
        } else {
                gtk_widget_set_visible (GTK_WIDGET (self->fingerprint_row), FALSE);
        }

        /* Autologin: show when local account */
        show = act_user_is_local_account (user);
        gtk_widget_set_visible (GTK_WIDGET (self->autologin_row), show);

#ifdef HAVE_MALCONTENT
        /* Parental Controls: Unavailable if user is admin or if
         * malcontent-control is not available (which can happen if
         * libmalcontent is installed but malcontent-control is not). */
        malcontent_control_path = g_find_program_in_path ("malcontent-control");

        if (act_user_get_account_type (user) == ACT_USER_ACCOUNT_TYPE_ADMINISTRATOR ||
            malcontent_control_path == NULL) {
                gtk_widget_hide (GTK_WIDGET (self->parental_controls_row));
        } else {
                GtkStyleContext *context = gtk_widget_get_style_context (GTK_WIDGET (self->parental_controls_button_label));

                if (is_parental_controls_enabled_for_user (user))
                        /* TRANSLATORS: Status of Parental Controls setup */
                        gtk_label_set_text (self->parental_controls_button_label, _("Enabled"));
                else
                        /* TRANSLATORS: Status of Parental Controls setup */
                        gtk_label_set_text (self->parental_controls_button_label, _("Disabled"));

                gtk_style_context_remove_class (context, "dim-label");
                gtk_widget_show (GTK_WIDGET (self->parental_controls_row));
        }
#endif

        /* Current user */
        show = act_user_get_uid (user) == getuid();
        gtk_widget_set_visible (GTK_WIDGET (self->account_settings_box), !show);
        gtk_widget_set_visible (GTK_WIDGET (self->remove_user_button), !show);
        gtk_widget_set_visible (GTK_WIDGET (self->back_button), !show);
        show_or_hide_back_button(self);
        gtk_widget_set_visible (GTK_WIDGET (self->other_users), show);

        /* Last login: show when administrator or current user */
        current = act_user_manager_get_user_by_id (self->um, getuid ());
        show = act_user_get_uid (user) == getuid () ||
               act_user_get_account_type (current) == ACT_USER_ACCOUNT_TYPE_ADMINISTRATOR;
        if (show) {
                g_autofree gchar *text = NULL;

                text = get_login_time_text (user);
                gtk_label_set_label (self->last_login_button_label, text);
        }
        gtk_widget_set_visible (GTK_WIDGET (self->last_login_row), show);

        enable = act_user_get_login_history (user) != NULL;
        gtk_widget_set_sensitive (GTK_WIDGET (self->last_login_row), enable);

        if (self->permission != NULL)
                on_permission_changed (self);
}

static void
full_name_entry_activate (CcUserPanel *self)
{
        const gchar *text;
        ActUser *user;

        user = get_selected_user (self);
        text = gtk_editable_get_text (GTK_EDITABLE (self->full_name_entry));
        if (g_strcmp0 (text, act_user_get_real_name (user)) != 0 &&
            is_valid_name (text)) {
                act_user_set_real_name (user, text);
        }

        gtk_toggle_button_set_active (self->full_name_edit_button, FALSE);
}

static void
full_name_edit_button_toggled (CcUserPanel *self)
{
        if (gtk_stack_get_visible_child (self->full_name_stack) == GTK_WIDGET (self->full_name_label)) {
                gtk_stack_set_visible_child (self->full_name_stack, GTK_WIDGET (self->full_name_entry));

                gtk_widget_grab_focus (GTK_WIDGET (self->full_name_entry));
        } else {
                gtk_stack_set_visible_child (self->full_name_stack, GTK_WIDGET (self->full_name_label));

                full_name_entry_activate (self);
        }
}

static gboolean
full_name_entry_key_press_cb (GtkEventController *controller,
                              guint               keyval,
                              guint               keycode,
                              GdkModifierType     state,
                              CcUserPanel        *self)
{
        if (keyval == GDK_KEY_Escape) {
                gtk_editable_set_text (GTK_EDITABLE (self->full_name_entry), act_user_get_real_name (self->selected_user));

                full_name_entry_activate (self);

                return TRUE;
        }

        return FALSE;
}

static void
account_type_changed (CcUserPanel *self)
{
        ActUser *user;
        gboolean self_selected;
        gboolean is_admin;
        ActUserAccountType account_type;

        user = get_selected_user (self);
        self_selected = act_user_get_uid (user) == geteuid ();
        is_admin = gtk_switch_get_active (self->account_type_switch);

        account_type = is_admin ? ACT_USER_ACCOUNT_TYPE_ADMINISTRATOR : ACT_USER_ACCOUNT_TYPE_STANDARD;
        if (account_type != act_user_get_account_type (user)) {
                act_user_set_account_type (user, account_type);

                if (self_selected)
                        show_restart_notification (self, NULL);
        }
}

static void
dismiss_notification (CcUserPanel *self)
{
        gtk_revealer_set_reveal_child (self->notification_revealer, FALSE);
}

static void
restart_now (CcUserPanel *self)
{
        g_autoptr(GDBusConnection) bus = NULL;

        gtk_revealer_set_reveal_child (self->notification_revealer, FALSE);

        bus = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, NULL);
        g_dbus_connection_call (bus,
                                "org.gnome.SessionManager",
                                "/org/gnome/SessionManager",
                                "org.gnome.SessionManager",
                                "Logout",
                                g_variant_new ("(u)", 0),
                                NULL, 0, G_MAXINT,
                                NULL, NULL, NULL);
}

static void
show_restart_notification (CcUserPanel *self, const gchar *locale)
{
        locale_t current_locale;
        locale_t new_locale;

        if (locale) {
                new_locale = newlocale (LC_MESSAGES_MASK, locale, (locale_t) 0);
                if (new_locale == (locale_t) 0)
                        g_warning ("Failed to create locale %s: %s", locale, g_strerror (errno));
                else
                        current_locale = uselocale (new_locale);
        }

        gtk_revealer_set_reveal_child (self->notification_revealer, TRUE);

        if (locale && new_locale != (locale_t) 0) {
                uselocale (current_locale);
                freelocale (new_locale);
        }
}

static void
language_response (CcUserPanel *self,
                   gint         response_id,
                   GtkDialog   *dialog)
{
        ActUser *user;
        const gchar *lang, *account_language;

        if (response_id != GTK_RESPONSE_OK) {
                gtk_widget_hide (GTK_WIDGET (dialog));
                return;
        }

        user = get_selected_user (self);
        account_language = act_user_get_language (user);

        lang = cc_language_chooser_get_language (CC_LANGUAGE_CHOOSER (dialog));
        if (lang) {
                g_autofree gchar *name = NULL;
                if (g_strcmp0 (lang, account_language) != 0) {
                        act_user_set_language (user, lang);
                }

                name = gnome_get_language_from_locale (lang, NULL);
                gtk_label_set_label (self->language_button_label, name);
        }

        gtk_widget_hide (GTK_WIDGET (dialog));
}

static void
change_language (CcUserPanel *self)
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
                                              GTK_WINDOW (gtk_widget_get_native (GTK_WIDGET (self))));

                g_signal_connect_object (self->language_chooser, "response",
                                         G_CALLBACK (language_response), self, G_CONNECT_SWAPPED);
        }

        if (current_language && *current_language != '\0')
                cc_language_chooser_set_language (self->language_chooser, current_language);
        gtk_window_present (GTK_WINDOW (self->language_chooser));
}

static void
change_password (CcUserPanel *self)
{
        ActUser *user;
        CcPasswordDialog *dialog;
        GtkWindow *parent;

        user = get_selected_user (self);
        dialog = cc_password_dialog_new (user);

        parent = (GtkWindow *) gtk_widget_get_native (GTK_WIDGET (self));
        gtk_window_set_transient_for (GTK_WINDOW (dialog), parent);

        gtk_window_present (GTK_WINDOW (dialog));
}

static void
change_fingerprint (CcUserPanel *self)
{
        ActUser *user;
        GtkWindow *parent;
        CcFingerprintDialog *dialog;

        user = get_selected_user (self);
        parent = (GtkWindow *) gtk_widget_get_native (GTK_WIDGET (self));

        g_assert (g_strcmp0 (g_get_user_name (), act_user_get_user_name (user)) == 0);

        dialog = cc_fingerprint_dialog_new (self->fingerprint_manager);
        gtk_window_set_transient_for (GTK_WINDOW (dialog), parent);
        gtk_window_present (GTK_WINDOW (dialog));
}

static void
show_history (CcUserPanel *self)
{
        CcLoginHistoryDialog *dialog;
        ActUser *user;
        GtkWindow *parent;

        user = get_selected_user (self);
        dialog = cc_login_history_dialog_new (user);

        parent = (GtkWindow *) gtk_widget_get_native (GTK_WIDGET (self));
        gtk_window_set_transient_for (GTK_WINDOW (dialog), parent);

        gtk_window_present (GTK_WINDOW (dialog));
}

#ifdef HAVE_MALCONTENT
static void
spawn_malcontent_control (CcUserPanel *self)
{
        ActUser *user;

        user = get_selected_user (self);

        /* no-op if the user is administrator */
        if (act_user_get_account_type (user) != ACT_USER_ACCOUNT_TYPE_ADMINISTRATOR) {
                const gchar *argv[] = {
                        "malcontent-control",
#ifdef HAVE_MALCONTENT_0_10
                        "--user",
                        act_user_get_user_name (user),
#endif  /* HAVE_MALCONTENT_0_10 */
                        NULL
                };
                g_autoptr(GError) error = NULL;
                if (!g_spawn_async (NULL, (char **)argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, &error))
                        g_debug ("Couldn't launch malcontent-control: %s", error->message);
        } else {
                g_debug ("Not launching malcontent because selected user is an admin");
        }
}
#endif

static void
users_loaded (CcUserPanel *self)
{
        GtkWidget *dialog;

        if (act_user_manager_no_service (self->um)) {
                GtkWidget *toplevel;

                toplevel = (GtkWidget *)gtk_widget_get_native (GTK_WIDGET (self));
                dialog = gtk_message_dialog_new (GTK_WINDOW (toplevel ),
                                                 GTK_DIALOG_MODAL,
                                                 GTK_MESSAGE_OTHER,
                                                 GTK_BUTTONS_CLOSE,
                                                 _("Failed to contact the accounts service"));
                gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
                                                          _("Please make sure that the AccountService is installed and enabled."));
                g_signal_connect (dialog, "response",
                                  G_CALLBACK (gtk_window_destroy),
                                  NULL);
                gtk_widget_show (dialog);

                gtk_stack_set_visible_child (self->stack, self->no_users_box);
        } else {
                gtk_stack_set_visible_child (self->stack, GTK_WIDGET (self->users_overlay));
                show_current_user (self);
        }

        g_signal_connect_object (self->um, "user-changed", G_CALLBACK (user_changed), self, G_CONNECT_SWAPPED);
        g_signal_connect_object (self->um, "user-is-logged-in-changed", G_CALLBACK (user_changed), self, G_CONNECT_SWAPPED);
        g_signal_connect_object (self->um, "user-added", G_CALLBACK (user_changed), self, G_CONNECT_SWAPPED);
        g_signal_connect_object (self->um, "user-removed", G_CALLBACK (user_changed), self, G_CONNECT_SWAPPED);
}

static void
add_unlock_tooltip (GtkWidget *widget)
{
        gtk_widget_set_tooltip_text (widget,
                                     _("This panel must be unlocked to change this setting"));
}

static void
remove_unlock_tooltip (GtkWidget *widget)
{
        gtk_widget_set_tooltip_text (widget, NULL);
}

static guint
get_num_active_admin (ActUserManager *um)
{
        GSList *list;
        GSList *l;
        guint num_admin = 0;

        list = act_user_manager_list_users (um);
        for (l = list; l != NULL; l = l->next) {
                ActUser *u = l->data;
                if (act_user_get_account_type (u) == ACT_USER_ACCOUNT_TYPE_ADMINISTRATOR && !act_user_get_locked (u)) {
                        num_admin++;
                }
        }
        g_slist_free (list);

        return num_admin;
}

static gboolean
would_demote_only_admin (ActUser *user)
{
        ActUserManager *um = act_user_manager_get_default ();

        /* Prevent the user from demoting the only admin account.
         * Returns TRUE when user is an administrator and there is only
         * one enabled administrator. */

        if (act_user_get_account_type (user) == ACT_USER_ACCOUNT_TYPE_STANDARD ||
            act_user_get_locked (user))
                return FALSE;

        if (get_num_active_admin (um) > 1)
                return FALSE;

        return TRUE;
}

static void
on_permission_changed (CcUserPanel *self)
{
        gboolean is_authorized;
        gboolean self_selected;
        ActUser *user;

        is_authorized = g_permission_get_allowed (G_PERMISSION (self->permission));

        gtk_widget_set_sensitive (self->add_user_button, is_authorized);

        user = get_selected_user (self);
        if (!user) {
                return;
        }

        self_selected = act_user_get_uid (user) == geteuid ();
        gtk_widget_set_sensitive (GTK_WIDGET (self->remove_user_button), is_authorized && !self_selected
                                  && !would_demote_only_admin (user));
        if (is_authorized) {
                gtk_widget_set_tooltip_text (GTK_WIDGET (self->remove_user_button), _("Delete the selected user account"));
        }
        else {
                gtk_widget_set_tooltip_text (GTK_WIDGET (self->remove_user_button),
                                             _("To delete the selected user account,\nclick the * icon first"));
        }

        if (!act_user_is_local_account (user)) {
                gtk_widget_set_visible (GTK_WIDGET (self->account_type_row), FALSE);
                gtk_widget_set_visible (GTK_WIDGET (self->autologin_row), FALSE);
        } else if (is_authorized && act_user_is_local_account (user)) {
                if (would_demote_only_admin (user)) {
                        gtk_widget_set_visible (GTK_WIDGET (self->account_type_row), FALSE);
                } else {
                        gtk_widget_set_visible (GTK_WIDGET (self->account_type_row), TRUE);
                }

                if (get_autologin_possible (user)) {
                        gtk_widget_set_visible (GTK_WIDGET (self->autologin_row), TRUE);
                        gtk_widget_set_sensitive (GTK_WIDGET (self->autologin_row), TRUE);
                }
        }
        else {
                gtk_widget_set_visible (GTK_WIDGET (self->account_type_row), FALSE);
                if (would_demote_only_admin (user)) {
                        gtk_widget_set_visible (GTK_WIDGET (self->account_type_row), FALSE);
                } else {
                        gtk_widget_set_visible (GTK_WIDGET (self->account_type_row), TRUE);
                }
                gtk_widget_set_sensitive (GTK_WIDGET (self->autologin_row), FALSE);
                add_unlock_tooltip (GTK_WIDGET (self->autologin_row));
        }

        /* The full name entry: insensitive if remote or not authorized and not self */
        if (!act_user_is_local_account (user)) {
                gtk_widget_set_sensitive (GTK_WIDGET (self->full_name_edit_button), FALSE);
                remove_unlock_tooltip (GTK_WIDGET (self->full_name_stack));

        } else if (is_authorized || self_selected) {
                gtk_widget_set_sensitive (GTK_WIDGET (self->full_name_edit_button), TRUE);
                remove_unlock_tooltip (GTK_WIDGET (self->full_name_stack));

        } else {
                gtk_widget_set_sensitive (GTK_WIDGET (self->full_name_edit_button), FALSE);
                add_unlock_tooltip (GTK_WIDGET (self->full_name_stack));
        }

        if (is_authorized || self_selected) {
                CcFingerprintState fingerprint_state = CC_FINGERPRINT_STATE_NONE;

                if (self->fingerprint_manager)
                        fingerprint_state = cc_fingerprint_manager_get_state (self->fingerprint_manager);

                gtk_widget_set_sensitive (GTK_WIDGET (self->user_avatar_edit_button), TRUE);
                remove_unlock_tooltip (GTK_WIDGET (self->user_avatar_edit_button));

                gtk_widget_set_sensitive (GTK_WIDGET (self->language_row), TRUE);
                remove_unlock_tooltip (GTK_WIDGET (self->language_row));

                gtk_widget_set_sensitive (GTK_WIDGET (self->password_row), TRUE);
                remove_unlock_tooltip (GTK_WIDGET (self->password_row));

                gtk_widget_set_sensitive (GTK_WIDGET (self->fingerprint_row),
                                          fingerprint_state != CC_FINGERPRINT_STATE_UPDATING);
                remove_unlock_tooltip (GTK_WIDGET (self->fingerprint_row));

                gtk_widget_set_sensitive (GTK_WIDGET (self->last_login_row), TRUE);
                remove_unlock_tooltip (GTK_WIDGET (self->last_login_row));
        }
        else {
                gtk_widget_set_sensitive (GTK_WIDGET (self->user_avatar_edit_button), FALSE);
                add_unlock_tooltip (GTK_WIDGET (self->user_avatar_edit_button));

                gtk_widget_set_sensitive (GTK_WIDGET (self->language_row), FALSE);
                add_unlock_tooltip (GTK_WIDGET (self->language_row));

                gtk_widget_set_sensitive (GTK_WIDGET (self->password_row), FALSE);
                add_unlock_tooltip (GTK_WIDGET (self->password_row));

                gtk_widget_set_sensitive (GTK_WIDGET (self->fingerprint_row), FALSE);
                add_unlock_tooltip (GTK_WIDGET (self->fingerprint_row));

                gtk_widget_set_sensitive (GTK_WIDGET (self->last_login_row), FALSE);
                add_unlock_tooltip (GTK_WIDGET (self->last_login_row));
        }
}

static void
setup_main_window (CcUserPanel *self)
{
        g_autoptr(GError) error = NULL;
        gboolean loaded;

        self->other_users_model = g_list_store_new (ACT_TYPE_USER);
        gtk_list_box_bind_model (self->other_users_listbox,
                                 G_LIST_MODEL (self->other_users_model),
                                 (GtkListBoxCreateWidgetFunc)create_user_row,
                                 self,
                                 NULL);

        add_unlock_tooltip (GTK_WIDGET (self->user_avatar));

        self->permission = (GPermission *)polkit_permission_new_sync (USER_ACCOUNTS_PERMISSION, NULL, NULL, &error);
        if (self->permission != NULL) {
                g_signal_connect_object (self->permission, "notify",
                                         G_CALLBACK (on_permission_changed), self, G_CONNECT_SWAPPED);
                on_permission_changed (self);
        } else {
                g_warning ("Cannot create '%s' permission: %s", USER_ACCOUNTS_PERMISSION, error->message);
        }

#ifdef HAVE_MALCONTENT
        g_signal_connect_object (self->parental_controls_row, "activated", G_CALLBACK (spawn_malcontent_control), self, G_CONNECT_SWAPPED);
#endif

        gtk_widget_set_tooltip_text (GTK_WIDGET (self->remove_user_button),
                                     _("To delete the selected user account,\nclick the * icon first"));

        g_object_get (self->um, "is-loaded", &loaded, NULL);
        if (loaded) {
                users_loaded (self);
                user_changed (self, NULL);
        } else {
                g_signal_connect_object (self->um, "notify::is-loaded", G_CALLBACK (users_loaded), self, G_CONNECT_SWAPPED);
        }

        self->avatar_chooser = cc_avatar_chooser_new (GTK_WIDGET (self));
        gtk_menu_button_set_popover (self->user_avatar_edit_button,
                                     GTK_WIDGET (self->avatar_chooser));
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
        CcUserPanel *self = CC_USER_PANEL (object);

        G_OBJECT_CLASS (cc_user_panel_parent_class)->constructed (object);

        cc_permission_infobar_set_permission (self->permission_infobar, self->permission);
        cc_permission_infobar_set_title (self->permission_infobar, _("Unlock to Add Users and Change Settings"));
}

static void
cc_user_panel_init (CcUserPanel *self)
{
        volatile GType type G_GNUC_UNUSED;
        g_autoptr(GtkCssProvider) provider = NULL;

        g_resources_register (cc_user_accounts_get_resource ());

        /* register types that the builder might need */
        type = cc_permission_infobar_get_type ();

        gtk_widget_init_template (GTK_WIDGET (self));

        self->um = act_user_manager_get_default ();

        provider = gtk_css_provider_new ();
        gtk_css_provider_load_from_resource (provider, "/org/gnome/control-center/user-accounts/user-accounts-dialog.css");
        gtk_style_context_add_provider_for_display (gdk_display_get_default (),
                                                    GTK_STYLE_PROVIDER (provider),
                                                    GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

        self->login_screen_settings = settings_or_null ("org.gnome.login-screen");

        setup_main_window (self);

        g_signal_connect_swapped (self,
                          "notify::folded",
                          G_CALLBACK (show_or_hide_back_button),
                          self);
}

static void
cc_user_panel_dispose (GObject *object)
{
        CcUserPanel *self = CC_USER_PANEL (object);

        g_clear_object (&self->selected_user);
        g_clear_object (&self->login_screen_settings);
        g_clear_pointer ((GtkWindow **)&self->language_chooser, gtk_window_destroy);
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
        GObjectClass   *object_class = G_OBJECT_CLASS (klass);
        GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
        CcPanelClass   *panel_class  = CC_PANEL_CLASS (klass);

        object_class->dispose = cc_user_panel_dispose;
        object_class->constructed = cc_user_panel_constructed;

        panel_class->get_help_uri = cc_user_panel_get_help_uri;

        gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/user-accounts/cc-user-panel.ui");

        gtk_widget_class_bind_template_child (widget_class, CcUserPanel, account_settings_box);
        gtk_widget_class_bind_template_child (widget_class, CcUserPanel, account_type_row);
        gtk_widget_class_bind_template_child (widget_class, CcUserPanel, account_type_switch);
        gtk_widget_class_bind_template_child (widget_class, CcUserPanel, add_user_button);
        gtk_widget_class_bind_template_child (widget_class, CcUserPanel, autologin_row);
        gtk_widget_class_bind_template_child (widget_class, CcUserPanel, autologin_switch);
        gtk_widget_class_bind_template_child (widget_class, CcUserPanel, back_button);
        gtk_widget_class_bind_template_child (widget_class, CcUserPanel, fingerprint_state_label);
        gtk_widget_class_bind_template_child (widget_class, CcUserPanel, fingerprint_row);
        gtk_widget_class_bind_template_child (widget_class, CcUserPanel, full_name_stack);
        gtk_widget_class_bind_template_child (widget_class, CcUserPanel, full_name_label);
        gtk_widget_class_bind_template_child (widget_class, CcUserPanel, full_name_edit_button);
        gtk_widget_class_bind_template_child (widget_class, CcUserPanel, full_name_entry);
        gtk_widget_class_bind_template_child (widget_class, CcUserPanel, language_button_label);
        gtk_widget_class_bind_template_child (widget_class, CcUserPanel, language_row);
        gtk_widget_class_bind_template_child (widget_class, CcUserPanel, last_login_button_label);
        gtk_widget_class_bind_template_child (widget_class, CcUserPanel, last_login_row);
        gtk_widget_class_bind_template_child (widget_class, CcUserPanel, no_users_box);
        gtk_widget_class_bind_template_child (widget_class, CcUserPanel, notification_revealer);
        gtk_widget_class_bind_template_child (widget_class, CcUserPanel, other_users);
        gtk_widget_class_bind_template_child (widget_class, CcUserPanel, other_users_row);
        gtk_widget_class_bind_template_child (widget_class, CcUserPanel, other_users_listbox);
#ifdef HAVE_MALCONTENT
        gtk_widget_class_bind_template_child (widget_class, CcUserPanel, parental_controls_button_label);
        gtk_widget_class_bind_template_child (widget_class, CcUserPanel, parental_controls_row);
#endif
        gtk_widget_class_bind_template_child (widget_class, CcUserPanel, password_button_label);
        gtk_widget_class_bind_template_child (widget_class, CcUserPanel, password_row);
        gtk_widget_class_bind_template_child (widget_class, CcUserPanel, permission_infobar);
        gtk_widget_class_bind_template_child (widget_class, CcUserPanel, remove_user_button);
        gtk_widget_class_bind_template_child (widget_class, CcUserPanel, stack);
        gtk_widget_class_bind_template_child (widget_class, CcUserPanel, user_avatar);
        gtk_widget_class_bind_template_child (widget_class, CcUserPanel, user_avatar_edit_button);
        gtk_widget_class_bind_template_child (widget_class, CcUserPanel, users_overlay);

        gtk_widget_class_bind_template_callback (widget_class, account_type_changed);
        gtk_widget_class_bind_template_callback (widget_class, add_user);
        gtk_widget_class_bind_template_callback (widget_class, autologin_changed);
        gtk_widget_class_bind_template_callback (widget_class, change_fingerprint);
        gtk_widget_class_bind_template_callback (widget_class, change_language);
        gtk_widget_class_bind_template_callback (widget_class, full_name_edit_button_toggled);
        gtk_widget_class_bind_template_callback (widget_class, full_name_entry_activate);
        gtk_widget_class_bind_template_callback (widget_class, full_name_entry_key_press_cb);
        gtk_widget_class_bind_template_callback (widget_class, change_password);
        gtk_widget_class_bind_template_callback (widget_class, delete_user);
        gtk_widget_class_bind_template_callback (widget_class, dismiss_notification);
        gtk_widget_class_bind_template_callback (widget_class, restart_now);
        gtk_widget_class_bind_template_callback (widget_class, set_selected_user);
        gtk_widget_class_bind_template_callback (widget_class, on_back_button_clicked_cb);
        gtk_widget_class_bind_template_callback (widget_class, show_history);
}
