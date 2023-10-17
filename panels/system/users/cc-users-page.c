/*
 * cc-users-page.c
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
#define G_LOG_DOMAIN "cc-users-page"

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "cc-users-page.h"
#include "cc-list-row.h"
#include "cc-user-page.h"

#include <adwaita.h>
#include <act/act.h>
#include <config.h>
#include <errno.h>
#include <locale.h>
#include <glib/gi18n.h>
#include <gio/gio.h>
#include <gtk/gtk.h>
#include <polkit/polkit.h>

#define USER_ACCOUNTS_PERMISSION "org.gnome.controlcenter.user-accounts.administration"

struct _CcUsersPage {
    CcSystemPage       parent_instance;

    AdwNavigationView *navigation;
    CcUserPage        *current_user_page;
    GtkListBox        *user_list;

    ActUserManager    *user_manager;
    GListStore        *model;
    GPermission       *permission;

    GCancellable      *cancellable;
};

G_DEFINE_TYPE (CcUsersPage, cc_users_page, CC_TYPE_SYSTEM_PAGE)

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
on_user_row_activated (CcUsersPage  *self,
                       AdwActionRow *row)
{
    CcUserPage *user_page;
    ActUser *user;
    uid_t uid;

    uid = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (row), "uid"));
    user = act_user_manager_get_user_by_id (act_user_manager_get_default (), uid);

    user_page = cc_user_page_new ();
    cc_user_page_set_user (user_page, user, self->permission);
    adw_navigation_page_set_title (ADW_NAVIGATION_PAGE (user_page), get_real_or_user_name (user));

    adw_navigation_view_push (self->navigation, ADW_NAVIGATION_PAGE (user_page));
}

static GtkWidget *
create_user_row (gpointer item, gpointer user_data)
{
    ActUser *user;
    GtkWidget *row, *user_image;

    row = adw_action_row_new ();
    gtk_list_box_row_set_activatable (GTK_LIST_BOX_ROW (row), TRUE);

    user = item;
    g_object_set_data (G_OBJECT (row), "uid", GINT_TO_POINTER (act_user_get_uid (user)));
    adw_preferences_row_set_title (ADW_PREFERENCES_ROW (row),
                                   get_real_or_user_name (user));
    user_image = adw_avatar_new (32, NULL, TRUE);
    //setup_avatar_for_user (ADW_AVATAR (user_image), user);
    adw_action_row_add_prefix (ADW_ACTION_ROW (row), user_image);

    return row;
}

static gint
sort_users (gconstpointer a, gconstpointer b, gpointer user_data)
{
    ActUser *ua, *ub;
    g_autofree gchar *name1 = NULL;
    g_autofree gchar *name2 = NULL;

    ua = ACT_USER ((gpointer*)a);
    ub = ACT_USER ((gpointer*)b);

    /* Make sure the current user is shown first */
    if (act_user_get_uid (ua) == getuid ()) {
        return -G_MAXINT32;
    } else if (act_user_get_uid (ub) == getuid ()) {
        return G_MAXINT32;
    }

    name1 = g_utf8_collate_key (get_real_or_user_name (ua), -1);
    name2 = g_utf8_collate_key (get_real_or_user_name (ub), -1);

    return strcmp (name1, name2);
}

static void
update_page_summary (CcUsersPage *self,
                     uint         n_users)
{
    g_autofree gchar *summary = NULL;

    if (n_users == 1) {
        summary = g_strdup (_("1 user account"));
    } else {
        /* Translators: %d is the number of user accounts. e.g. "3 user accounts". */
        summary = g_strdup_printf (_("%d user accounts"), n_users);
    }

    cc_system_page_set_summary (CC_SYSTEM_PAGE (self), summary);
}

static void
users_loaded (CcUsersPage *self)
{
    g_autoptr(GSList) user_list = NULL;
    GSList *l;

    g_print ("Users loaded!\n");

    user_list = act_user_manager_list_users (self->user_manager);
    for (l = user_list; l; l = l->next) {
        ActUser *user = ACT_USER (l->data);

        if (act_user_is_system_account (user)) {
            continue;
        }

        if (act_user_get_uid (user) == getuid ()) {
            cc_user_page_set_user (self->current_user_page, user, self->permission);

            continue;
        }

        g_list_store_insert_sorted (self->model,
                                    user,
                                    sort_users,
                                    self);
        g_print ("Inserting user to list\n");
    }

    update_page_summary (self, g_slist_length (user_list));
}

static void
cc_users_page_finalize (GObject *object)
{
    CcUsersPage *self = CC_USERS_PAGE (object);

    g_clear_object (&self->permission);

    G_OBJECT_CLASS (cc_users_page_parent_class)->finalize (object);
}

static void
cc_users_page_init (CcUsersPage *self)
{
    g_autoptr(GError) error = NULL;
    gboolean is_loaded = FALSE;

    gtk_widget_init_template (GTK_WIDGET (self));

    self->model = g_list_store_new (ACT_TYPE_USER);
    gtk_list_box_bind_model (self->user_list,
                             G_LIST_MODEL (self->model), 
                             (GtkListBoxCreateWidgetFunc)create_user_row,
                             self,
                             NULL);
    self->cancellable = g_cancellable_new ();

    self->permission = (GPermission *)polkit_permission_new_sync (USER_ACCOUNTS_PERMISSION, NULL, NULL, &error);
    if (self->permission == NULL) {
        g_warning ("Cannot create '%s' permission: %s", USER_ACCOUNTS_PERMISSION, error->message);
    }

    self->user_manager = act_user_manager_get_default ();
    g_signal_connect_object (self->user_manager,
                             "notify::is-loaded",
                             G_CALLBACK (users_loaded),
                             self,
                             G_CONNECT_SWAPPED);
    g_object_get (self->user_manager, "is-loaded", &is_loaded, NULL);
    if (is_loaded) {
        users_loaded (self);
    }
}

static void
cc_users_page_class_init (CcUsersPageClass * klass)
{
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->finalize = cc_users_page_finalize;

    g_type_ensure (CC_TYPE_LIST_ROW);
    g_type_ensure (CC_TYPE_USER_PAGE);

    gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/system/users/cc-users-page.ui");

    gtk_widget_class_bind_template_child (widget_class, CcUsersPage, current_user_page);
    gtk_widget_class_bind_template_child (widget_class, CcUsersPage, navigation);
    gtk_widget_class_bind_template_child (widget_class, CcUsersPage, user_list);

    gtk_widget_class_bind_template_callback (widget_class, on_user_row_activated);
}
