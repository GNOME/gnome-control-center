/*
 * cc-enterprise-login-dialog.c
 *
 * Copyright 2023 Red Hat Inc
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Author(s):
 *   Felipe Borges <felipeborges@gnome.org>
 */

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "cc-enterprise-login-dialog"

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "cc-enterprise-login-dialog.h"
#include "cc-entry-feedback.h"
#include "cc-realm-manager.h"
#include "cc-list-row.h"
#include "user-utils.h"

#include <adwaita.h>
#include <act/act.h>
#include <config.h>
#include <errno.h>
#include <locale.h>
#include <glib/gi18n.h>
#include <gio/gio.h>
#include <gtk/gtk.h>

#define DOMAIN_CHECK_TIMEOUT 600

struct _CcEnterpriseLoginDialog {
    AdwWindow            parent_instance;

    AdwNavigationView   *navigation;
    AdwNavigationPage   *offline_page;
    AdwNavigationPage   *main_page;
    AdwNavigationPage   *domain_enroll_page;
    AdwPreferencesPage  *enrol_preferences_page;
    AdwToastOverlay     *toast_overlay;

    GtkButton           *add_button;
    GtkSpinner          *spinner;

    AdwEntryRow         *domain_row;
    CcEntryFeedback     *domain_feedback;
    AdwEntryRow         *username_row;
    AdwPasswordEntryRow *password_row;

    GtkButton           *enrol_button;
    AdwEntryRow         *admin_name_row;
    AdwPasswordEntryRow *admin_password_row;

    GCancellable        *cancellable;
    guint                realmd_watch;
    CcRealmManager      *realm_manager;
    CcRealmObject       *selected_realm;
    gboolean             join_prompted;

    gint                 domain_timeout_id;
};

G_DEFINE_TYPE (CcEnterpriseLoginDialog, cc_enterprise_login_dialog, ADW_TYPE_WINDOW)

static void
on_enrol_page_validate (CcEnterpriseLoginDialog *self)
{
    gtk_widget_set_sensitive (GTK_WIDGET (self->enrol_button),
                              strlen (gtk_editable_get_text (GTK_EDITABLE (self->admin_name_row))) > 0 &&
                              strlen (gtk_editable_get_text (GTK_EDITABLE (self->admin_password_row))) > 0);
}

static void
on_domain_enrol_clicked_cb (CcEnterpriseLoginDialog *self)
{
    adw_navigation_view_pop (self->navigation);
}

static void
validate_dialog (CcEnterpriseLoginDialog *self)
{
    gtk_widget_set_sensitive (GTK_WIDGET (self->add_button),
                              (self->selected_realm != NULL) &&
                              strlen (gtk_editable_get_text (GTK_EDITABLE (self->username_row))) > 0 &&
                              strlen (gtk_editable_get_text (GTK_EDITABLE (self->password_row))) > 0);
}

static void
clear_realm_manager (CcEnterpriseLoginDialog *self)
{
    if (self->realm_manager) {
        g_clear_object (&self->realm_manager);
    }
}

static void
on_realm_manager_created (GObject *source,
                          GAsyncResult *result,
                          gpointer user_data)
{
    g_autoptr(CcEnterpriseLoginDialog) self = CC_ENTERPRISE_LOGIN_DIALOG (user_data);
    g_autoptr(GError) error = NULL;

    clear_realm_manager (self);

    self->realm_manager = cc_realm_manager_new_finish (result, &error);
    if (error != NULL) {
        g_warning ("Couldn't contact realmd service: %s", error->message);
        return;
    }

    if (g_cancellable_is_cancelled (self->cancellable))
        return;

    /* Show the 'Enterprise Login' stuff */
    adw_navigation_view_push (self->navigation, self->main_page);
}

static void
on_realmd_appeared (GDBusConnection *connection,
                    const gchar *name,
                    const gchar *name_owner,
                    gpointer user_data)
{
    CcEnterpriseLoginDialog *self = CC_ENTERPRISE_LOGIN_DIALOG (user_data);
    cc_realm_manager_new (self->cancellable, on_realm_manager_created,
                          g_object_ref (self));
}

static void
on_realmd_disappeared (GDBusConnection *unused1,
                       const gchar *unused2,
                       gpointer user_data)
{
    CcEnterpriseLoginDialog *self = CC_ENTERPRISE_LOGIN_DIALOG (user_data);

    clear_realm_manager (self);
    adw_navigation_view_push (self->navigation, self->offline_page);
}

static void
on_realm_discover_input (GObject       *source,
                         GAsyncResult  *result,
                         gpointer       user_data)
{
    CcEnterpriseLoginDialog *self = CC_ENTERPRISE_LOGIN_DIALOG (user_data);
    g_autoptr(GError) error = NULL;
    GList *realms;

    if (g_cancellable_is_cancelled (self->cancellable))
        return;

    realms = cc_realm_manager_discover_finish (self->realm_manager, result, &error);

    /* Found a realm, log user into domain */
    if (error == NULL) {
        g_assert (realms != NULL);
        self->selected_realm = g_object_ref (realms->data);

        cc_entry_feedback_update (self->domain_feedback, "emblem-ok", _("Valid domain"));
    } else {
        cc_entry_feedback_update (self->domain_feedback, "dialog-warning", _("Invalid domain"));
    }

    validate_dialog (self);
    g_list_free_full (realms, g_object_unref);
}

static void
validate_domain (CcEnterpriseLoginDialog *self)
{
    self->domain_timeout_id = 0;

    self->join_prompted = FALSE;
    cc_realm_manager_discover (self->realm_manager,
                               gtk_editable_get_text (GTK_EDITABLE (self->domain_row)),
                               self->cancellable,
                               on_realm_discover_input,
                               g_object_ref (self));
}

static void
on_domain_entry_changed_cb (CcEnterpriseLoginDialog *self)
{
    const gchar *domain;

    if (self->domain_timeout_id != 0) {
        g_source_remove (self->domain_timeout_id);
        self->domain_timeout_id = 0;
    }

    g_clear_object (&self->selected_realm);
    domain = gtk_editable_get_text (GTK_EDITABLE (self->domain_row));
    if (strlen (domain) == 0) {
        cc_entry_feedback_reset (self->domain_feedback);

        return;
    }

    cc_entry_feedback_update (self->domain_feedback, "content-loading-symbolic", _("Checking domain…"));
    self->domain_timeout_id = g_timeout_add (DOMAIN_CHECK_TIMEOUT, (GSourceFunc)validate_domain, self);
}

static void
on_realm_joined (GObject      *source,
                 GAsyncResult *result,
                 gpointer      user_data)
{
    g_autoptr(CcEnterpriseLoginDialog) self = CC_ENTERPRISE_LOGIN_DIALOG (user_data);
    g_autoptr(GError) error = NULL;

    if (g_cancellable_is_cancelled (self->cancellable))
        return;

    cc_realm_join_finish (self->selected_realm, result, &error);
}

static void
on_realm_login (GObject      *source,
                GAsyncResult *result,
                gpointer      user_data)
{
    g_autoptr(CcEnterpriseLoginDialog) self = CC_ENTERPRISE_LOGIN_DIALOG (user_data);
    g_autoptr(GError) error = NULL;
    g_autoptr(GBytes) creds = NULL;
    const gchar *message = NULL;

    gtk_widget_set_visible (GTK_WIDGET (self->spinner), FALSE);
    if (g_cancellable_is_cancelled (self->cancellable))
        return;

    creds = cc_realm_login_finish (result, &error);
    /*
     * User login is valid, but cannot authenticate right now (eg: user needs
     * to change password at next login etc.)
     */
    if (g_error_matches (error, CC_REALM_ERROR, CC_REALM_ERROR_CANNOT_AUTH)) {
        g_clear_error (&error);
        creds = NULL;
    }

    if (error == NULL) {
        /* Already joined to the domain, just register this user */
        if (cc_realm_is_configured (self->selected_realm)) {
            g_debug ("Already joined to this realm");
            //enterprise_permit_user_login (self);

        /* Join the domain, try using the user's creds */
        } else if (creds == NULL ||
               !cc_realm_join_as_user (self->selected_realm,
                                       gtk_editable_get_text (GTK_EDITABLE (self->username_row)),
                                       gtk_editable_get_text (GTK_EDITABLE (self->password_row)),
                                       creds, self->cancellable,
                                       on_realm_joined,
                                       g_object_ref (self))) {
            const gchar *domain, *description;

            /* If we can't do user auth, try to authenticate as admin */
            g_debug ("Cannot join with user credentials");

            domain = gtk_editable_get_text (GTK_EDITABLE (self->domain_row));
            description = g_strdup_printf (_("To add an enterprise login account, this device needs to be enrolled with %s. To enrol, have your domain administrator enter their name and password."), domain);

            adw_preferences_page_set_description (self->enrol_preferences_page, description);
            adw_navigation_view_push (self->navigation, self->domain_enroll_page);
            return;
        }

    /* A problem with the user's login name or password */
    } else if (g_error_matches (error, CC_REALM_ERROR, CC_REALM_ERROR_BAD_LOGIN)) {
        g_debug ("Problem with the user's login: %s", error->message);
        message = _("That login name didn’t work");

    } else if (g_error_matches (error, CC_REALM_ERROR, CC_REALM_ERROR_BAD_PASSWORD)) {
        g_debug ("Problem with the user's password: %s", error->message);
        message = _("That login password didn’t work");

    /* Other login failure */
    } else {
        g_dbus_error_strip_remote_error (error);
        g_message ("Couldn't log in as user: %s", error->message);

        message = _("Failed to log into domain");
    }

    adw_toast_overlay_add_toast (self->toast_overlay, adw_toast_new (message));
}

static void
add_user (CcEnterpriseLoginDialog *self)
{
    gtk_widget_set_visible (GTK_WIDGET (self->spinner), TRUE);

    cc_realm_login (self->selected_realm,
                    gtk_editable_get_text (GTK_EDITABLE (self->username_row)),
                    gtk_editable_get_text (GTK_EDITABLE (self->password_row)),
                    self->cancellable,
                    on_realm_login,
                    g_object_ref (self));
}

static void
cc_enterprise_login_dialog_dispose (GObject *object)
{
    CcEnterpriseLoginDialog *self = CC_ENTERPRISE_LOGIN_DIALOG (object);

    if (self->cancellable)
        g_cancellable_cancel (self->cancellable);
    g_clear_object (&self->cancellable);

    if (self->realmd_watch)
        g_bus_unwatch_name (self->realmd_watch);
    self->realmd_watch = 0;

    g_clear_object (&self->realm_manager);

    if (self->domain_timeout_id != 0) {
        g_source_remove (self->domain_timeout_id);
        self->domain_timeout_id = 0;
    }

    G_OBJECT_CLASS (cc_enterprise_login_dialog_parent_class)->dispose (object);
}

static void
check_network_availability (CcEnterpriseLoginDialog *self)
{
    GNetworkMonitor *monitor;

    monitor = g_network_monitor_get_default ();
    if (!g_network_monitor_get_network_available (monitor)) {
        adw_navigation_view_pop_to_page (self->navigation, self->offline_page);
    }
}

static void
cc_enterprise_login_dialog_init (CcEnterpriseLoginDialog *self)
{
    gtk_widget_init_template (GTK_WIDGET (self));

    self->cancellable = g_cancellable_new ();

    check_network_availability (self);

    self->realmd_watch = g_bus_watch_name (G_BUS_TYPE_SYSTEM,
                                           "org.freedesktop.realmd",
                                           G_BUS_NAME_WATCHER_FLAGS_AUTO_START,
                                           on_realmd_appeared, on_realmd_disappeared,
                                           self, NULL);
}

static void
cc_enterprise_login_dialog_class_init (CcEnterpriseLoginDialogClass * klass)
{
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->dispose = cc_enterprise_login_dialog_dispose;

    g_type_ensure (CC_TYPE_ENTRY_FEEDBACK);
    g_type_ensure (CC_TYPE_LIST_ROW);

    gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/system/users/cc-enterprise-login-dialog.ui");

    gtk_widget_class_bind_template_child (widget_class, CcEnterpriseLoginDialog, admin_name_row);
    gtk_widget_class_bind_template_child (widget_class, CcEnterpriseLoginDialog, admin_password_row);
    gtk_widget_class_bind_template_child (widget_class, CcEnterpriseLoginDialog, add_button);
    gtk_widget_class_bind_template_child (widget_class, CcEnterpriseLoginDialog, spinner);
    gtk_widget_class_bind_template_child (widget_class, CcEnterpriseLoginDialog, domain_enroll_page);
    gtk_widget_class_bind_template_child (widget_class, CcEnterpriseLoginDialog, enrol_button);
    gtk_widget_class_bind_template_child (widget_class, CcEnterpriseLoginDialog, enrol_preferences_page);
    gtk_widget_class_bind_template_child (widget_class, CcEnterpriseLoginDialog, navigation);
    gtk_widget_class_bind_template_child (widget_class, CcEnterpriseLoginDialog, main_page);
    gtk_widget_class_bind_template_child (widget_class, CcEnterpriseLoginDialog, offline_page);
    gtk_widget_class_bind_template_child (widget_class, CcEnterpriseLoginDialog, domain_feedback);
    gtk_widget_class_bind_template_child (widget_class, CcEnterpriseLoginDialog, domain_row);
    gtk_widget_class_bind_template_child (widget_class, CcEnterpriseLoginDialog, username_row);
    gtk_widget_class_bind_template_child (widget_class, CcEnterpriseLoginDialog, password_row);
    gtk_widget_class_bind_template_child (widget_class, CcEnterpriseLoginDialog, toast_overlay);

    gtk_widget_class_bind_template_callback (widget_class, add_user);
    gtk_widget_class_bind_template_callback (widget_class, validate_dialog);
    gtk_widget_class_bind_template_callback (widget_class, on_domain_entry_changed_cb);
    gtk_widget_class_bind_template_callback (widget_class, on_domain_enrol_clicked_cb);
    gtk_widget_class_bind_template_callback (widget_class, on_enrol_page_validate);
}

CcEnterpriseLoginDialog *
cc_enterprise_login_dialog_new (void)
{
    return g_object_new (CC_TYPE_ENTERPRISE_LOGIN_DIALOG, NULL);
}
