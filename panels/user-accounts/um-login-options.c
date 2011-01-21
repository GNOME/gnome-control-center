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
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <glib.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <gconf/gconf-client.h>
#include <gconf/gconf-value.h>
#include <polkit/polkit.h>

#include "um-login-options.h"
#include "um-lockbutton.h"
#include "um-user-manager.h"
#include "um-user.h"

struct _UmLoginOptions {
        GtkWidget *autologin_combo;
        GtkWidget *userlist_check;
        GtkWidget *power_check;
        GtkWidget *hints_check;
        GtkWidget *guest_check;
        GtkWidget *lock_button;
        GPermission *permission;

        UmUserManager *manager;

        DBusGProxy *proxy;
        DBusGConnection *connection;
};

enum {
        AUTOLOGIN_NAME_COL,
        AUTOLOGIN_USER_COL,
        NUM_AUTOLOGIN_COLS
};

static gint
sort_login_users (GtkTreeModel *model,
                  GtkTreeIter  *a,
                  GtkTreeIter  *b,
                  gpointer      data)
{
        UmUser *ua, *ub;
        gint result;

        gtk_tree_model_get (model, a, AUTOLOGIN_USER_COL, &ua, -1);
        gtk_tree_model_get (model, b, AUTOLOGIN_USER_COL, &ub, -1);

        if (ua == NULL)
                result = -1;
        else if (ub == NULL)
                result = 1;
        else if (um_user_get_uid (ua) == getuid ())
                result = -1;
        else if (um_user_get_uid (ub) == getuid ())
                result = 1;
        else
                result = um_user_collate (ua, ub);

        if (ua)
                g_object_unref (ua);

        if (ub)
                g_object_unref (ub);

        return result;
}

static void
user_added (UmUserManager *um, UmUser *user, UmLoginOptions *d)
{
        GtkComboBox *combo;
        GtkListStore *store;
        GtkTreeIter iter;

        g_debug ("adding user '%s', %d", um_user_get_user_name (user), um_user_get_automatic_login (user));
        combo = GTK_COMBO_BOX (d->autologin_combo);
        store = (GtkListStore*)gtk_combo_box_get_model (combo);
        gtk_list_store_append (store, &iter);
        gtk_list_store_set (store, &iter,
                            AUTOLOGIN_NAME_COL, um_user_get_display_name (user),
                            AUTOLOGIN_USER_COL, user,
                            -1);

        if (um_user_get_automatic_login (user)) {
                gtk_combo_box_set_active_iter (combo, &iter);
        }
}

static void
user_removed (UmUserManager *um, UmUser *user, UmLoginOptions *d)
{
        GtkComboBox *combo;
        GtkTreeModel *model;
        GtkListStore *store;
        GtkTreeIter iter;
        UmUser *u;

        combo = GTK_COMBO_BOX (d->autologin_combo);
        model = gtk_combo_box_get_model (combo);
        store = (GtkListStore*)model;

        gtk_combo_box_get_active_iter (combo, &iter);
        gtk_tree_model_get (model, &iter, AUTOLOGIN_USER_COL, &u, -1);
        if (u != NULL) {
                if (um_user_get_uid (user) == um_user_get_uid (u)) {
                        /* autologin user got removed, set back to Disabled */
                        gtk_list_store_remove (store, &iter);
                        gtk_combo_box_set_active (combo, 0);
                        g_object_unref (u);
                        return;
                }
                g_object_unref (u);
        }
        if (gtk_tree_model_get_iter_first (model, &iter)) {
                do {
                        gtk_tree_model_get (model, &iter, AUTOLOGIN_USER_COL, &u, -1);

                        if (u != NULL) {
                                if (um_user_get_uid (user) == um_user_get_uid (u)) {
                                        gtk_list_store_remove (store, &iter);
                                        g_object_unref (u);
                                        return;
                                }
                                g_object_unref (u);
                        }
                } while (gtk_tree_model_iter_next (model, &iter));
        }
}

static void
user_changed (UmUserManager  *manager,
              UmUser         *user,
              UmLoginOptions *d)
{
        /* FIXME */
}

static void
users_loaded (UmUserManager  *manager,
              UmLoginOptions *d)
{
        GSList *list, *l;
        UmUser *user;

        list = um_user_manager_list_users (manager);
        for (l = list; l; l = l->next) {
                user = l->data;
                user_added (manager, user, d);
        }
        g_slist_free (list);

        g_signal_connect (manager, "user-added", G_CALLBACK (user_added), d);
        g_signal_connect (manager, "user-removed", G_CALLBACK (user_removed), d);
        g_signal_connect (manager, "user-changed", G_CALLBACK (user_changed), d);
}

static void update_login_options (GtkWidget *widget, UmLoginOptions *d);

static void
update_boolean_from_gconf (GtkWidget      *widget,
                           UmLoginOptions *d)
{
        gchar *cmdline;
        gboolean value;
        gchar *std_out;
        gchar *std_err;
        gint status;
        GError *error;
        const gchar *key;

        key = g_object_get_data (G_OBJECT (widget), "gconf-key");

        /* GConf fail.
         * gconfd does not pick up any changes in the default or mandatory
         * databases at runtime. Even a SIGHUP doesn't seem to help. So we
         * have to use gconftool to go get the current mandatory values.
         */
        cmdline = g_strdup_printf ("gconftool-2 --direct --config-source=\"xml:readonly:/etc/gconf/gconf.xml.defaults;xml:readonly:/etc/gconf/gconf.xml.mandatory\" --get %s", key);

        error = NULL;
        std_out = NULL;
        std_err = NULL;
        if (!g_spawn_command_line_sync (cmdline, &std_out, &std_err, &status, &error)) {
                g_warning ("Failed to run '%s': %s", cmdline, error->message);
                g_error_free (error);
                g_free (cmdline);
                g_free (std_out);
                g_free (std_err);
                return;
        }
        if (WEXITSTATUS (status) != 0) {
                g_warning ("Failed to run '%s': %s", cmdline, std_err);
                g_free (cmdline);
                g_free (std_out);
                g_free (std_err);
                return;
        }

        if (strlen (std_out) > 0 && std_out[strlen (std_out) - 1] == '\n') {
                std_out[strlen (std_out) - 1] = 0;
        }

        if (g_strcmp0 (std_out, "true") == 0) {
                value = TRUE;
        }
        else {
                value = FALSE;
        }
        g_signal_handlers_block_by_func (widget, update_login_options, d);
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), !value);
        g_signal_handlers_unblock_by_func (widget, update_login_options, d);

        g_free (cmdline);
        g_free (std_out);
        g_free (std_err);
}

static void
update_login_options (GtkWidget      *widget,
                      UmLoginOptions *d)
{
        GError *error;
        gboolean active;
        GConfValue *value;
        const gchar *key = NULL;
        gchar *value_string;

        if (widget == d->userlist_check ||
            widget == d->power_check) {
                active = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));
                key = g_object_get_data (G_OBJECT (widget), "gconf-key");
        }
        else {
                g_warning ("unhandled option in update_login_options");
                return;
        }

        error = NULL;
        value = gconf_value_new (GCONF_VALUE_BOOL);
        gconf_value_set_bool (value, !active);
        value_string = gconf_value_encode (value);
        if (!dbus_g_proxy_call (d->proxy, "SetMandatoryValue",
                                &error,
                                G_TYPE_STRING, key,
                                G_TYPE_STRING, value_string,
                                G_TYPE_INVALID,
                                G_TYPE_INVALID)) {
               g_warning ("error calling SetMandatoryValue: %s\n", error->message);
               g_error_free (error);
       }
       g_free (value_string);
       gconf_value_free (value);
       update_boolean_from_gconf (widget, d);
}

static void
update_autologin (GtkWidget      *widget,
                  UmLoginOptions *d)
{
        GtkComboBox *combo = GTK_COMBO_BOX (widget);
        GtkTreeModel *model;
        GtkTreeIter iter;
        UmUser *user;
        gboolean enabled;

        if (!gtk_widget_is_sensitive (widget))
                return;

        model = gtk_combo_box_get_model (combo);
        gtk_combo_box_get_active_iter (combo, &iter);
        gtk_tree_model_get (model, &iter, AUTOLOGIN_USER_COL, &user, -1);
        if (user) {
                enabled = TRUE;
        }
        else {
                enabled = FALSE;
                user = um_user_manager_get_user_by_id (d->manager, getuid ());
                g_object_ref (user);
        }

        um_user_set_automatic_login (user, enabled);

        g_object_unref (user);
}

static void
on_permission_changed (GPermission *permission,
                       GParamSpec  *spec,
                       gpointer     data)
{
        UmLoginOptions *d = data;
        gboolean authorized;

        authorized = g_permission_get_allowed (G_PERMISSION (d->permission));

        gtk_widget_set_sensitive (d->autologin_combo, authorized);
        gtk_widget_set_sensitive (d->userlist_check, authorized);
        gtk_widget_set_sensitive (d->power_check, authorized);
        gtk_widget_set_sensitive (d->hints_check, authorized);
        gtk_widget_set_sensitive (d->guest_check, authorized);
}

UmLoginOptions *
um_login_options_new (GtkBuilder *builder)
{
        GtkWidget *widget;
        GtkWidget *box;
        GtkListStore *store;
        GtkTreeIter iter;
        GError *error;
        UmLoginOptions *um;

        /* TODO: get actual login screen options */

        um = g_new0 (UmLoginOptions, 1);

        um->manager = um_user_manager_ref_default ();
        g_signal_connect (um->manager, "users-loaded",
                          G_CALLBACK (users_loaded), um);

        widget = (GtkWidget *) gtk_builder_get_object (builder, "dm-automatic-login-combobox");
        um->autologin_combo = widget;

        store = gtk_list_store_new (2, G_TYPE_STRING, UM_TYPE_USER);
        gtk_combo_box_set_model (GTK_COMBO_BOX (widget), GTK_TREE_MODEL (store));
        gtk_list_store_append (store, &iter);
        gtk_list_store_set (store, &iter,
                            AUTOLOGIN_NAME_COL, _("Disabled"),
                            AUTOLOGIN_USER_COL, NULL,
                            -1);

        gtk_combo_box_set_active (GTK_COMBO_BOX (widget), 0);

        gtk_tree_sortable_set_default_sort_func (GTK_TREE_SORTABLE (store), sort_login_users, NULL, NULL);
        gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (store), GTK_TREE_SORTABLE_DEFAULT_SORT_COLUMN_ID, GTK_SORT_ASCENDING);

        g_signal_connect (widget, "changed",
                          G_CALLBACK (update_autologin), um);

        widget = (GtkWidget *) gtk_builder_get_object (builder, "dm-show-user-list-checkbutton");
        um->userlist_check = widget;
        g_signal_connect (widget, "toggled",
                          G_CALLBACK (update_login_options), um);
        g_object_set_data (G_OBJECT (widget), "gconf-key",
                           "/apps/gdm/simple-greeter/disable_user_list");
        update_boolean_from_gconf (widget, um);

        widget = (GtkWidget *) gtk_builder_get_object (builder, "dm-show-power-buttons-checkbutton");
        um->power_check = widget;
        g_signal_connect(widget, "toggled",
                         G_CALLBACK (update_login_options), um);
        g_object_set_data (G_OBJECT (widget), "gconf-key",
                           "/apps/gdm/simple-greeter/disable_restart_buttons");
        update_boolean_from_gconf (widget, um);

        widget = (GtkWidget *) gtk_builder_get_object (builder, "dm-show-password-hints-checkbutton");
        um->hints_check = widget;
        g_signal_connect (widget, "toggled",
                          G_CALLBACK (update_login_options), um);

        widget = (GtkWidget *) gtk_builder_get_object (builder, "dm-allow-guest-login-checkbutton");
        um->guest_check = widget;
        g_signal_connect (widget, "toggled",
                          G_CALLBACK (update_login_options), um);

        um->permission = polkit_permission_new_sync ("org.freedesktop.accounts.set-login-option", NULL, NULL, NULL);
        if (um->permission != NULL) {
                widget = um_lock_button_new (um->permission);
                gtk_widget_show (widget);
                box = (GtkWidget *)gtk_builder_get_object (builder, "lockbutton-alignment");
                gtk_container_add (GTK_CONTAINER (box), widget);
                g_signal_connect (um->permission, "notify",
                                  G_CALLBACK (on_permission_changed), um);
                on_permission_changed (um->permission, NULL, um);
                um->lock_button = widget;
        }

        error = NULL;
        um->connection = dbus_g_bus_get (DBUS_BUS_SYSTEM, &error);
        if (error != NULL) {
                g_warning ("Failed to get system bus connection: %s", error->message);
                g_error_free (error);
        }

        um->proxy = dbus_g_proxy_new_for_name (um->connection,
                                               "org.gnome.GConf.Defaults",
                                               "/",
                                               "org.gnome.GConf.Defaults");

        if (um->proxy == NULL) {
                g_warning ("Cannot connect to GConf defaults mechanism");
        }

        return um;
}

void
um_login_options_free (UmLoginOptions *um)
{
  if (um->manager)
    g_object_unref (um->manager);
  if (um->proxy)
    g_object_unref (um->proxy);
  if (um->connection)
    dbus_g_connection_unref (um->connection);

  g_free (um);
}

