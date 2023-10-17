/*
 * cc-user-page.c
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
#define G_LOG_DOMAIN "cc-user-page"

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "cc-user-page.h"
#include "cc-language-chooser.h"
#include "cc-list-row.h"
#include "cc-password-dialog.h"
#include "cc-permission-infobar.h"
#include "user-utils.h"

#include <config.h>
#include <errno.h>
#include <locale.h>
#include <glib/gi18n.h>
#include <gio/gio.h>
#include <gtk/gtk.h>

#define GNOME_DESKTOP_USE_UNSTABLE_API
#include <libgnome-desktop/gnome-languages.h>

struct _CcUserPage {
    AdwNavigationPage  parent_instance;

    CcListRow           *account_type_row;
    GtkSwitch           *account_type_switch;
    AdwSwitchRow        *auto_login_row;
    CcListRow           *language_row;
    AdwEntryRow         *fullname_row;
    CcListRow           *password_row;
    CcPermissionInfobar *permission_infobar;

    GtkBox            *action_area;
    GtkWidget         *remove_user_button;

    ActUser           *user;
    gboolean           locked;
    gboolean           can_be_demoted;
};

static GtkBuildableIface *parent_buildable_iface;
static void cc_user_page_buildable_init (GtkBuildableIface *iface);

G_DEFINE_TYPE_WITH_CODE (CcUserPage, cc_user_page, ADW_TYPE_NAVIGATION_PAGE,
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_BUILDABLE, cc_user_page_buildable_init))

enum {
    PROP_0,
    PROP_LOCKED
};

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
    if (act_user_get_account_type (user) == ACT_USER_ACCOUNT_TYPE_STANDARD || act_user_get_locked (user)) {
        return FALSE;
    }

    if (get_num_active_admin (um) > 1) {
        return FALSE;
    }

    return TRUE;
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

static gchar *
get_user_language (ActUser *user)
{
    g_autofree gchar *lang = NULL;

    lang = g_strdup (act_user_get_language (user));
    if (lang && *lang != '\0') {
        return gnome_get_language_from_locale (lang, NULL);
    }

    return g_strdup  ("—");
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
     if (invisible_char == 0) {
        invisible_char = 0x2022;
     }

     g_object_ref_sink (entry);
     g_object_unref (entry);

     /* five bullets */
     p = invisible_text;
     for (i = 0; i < 5; i++) {
        p += g_unichar_to_utf8 (invisible_char, p);
     }
     *p = 0;

     return invisible_text;
}

static const gchar *
get_password_mode_text (ActUser *user)
{
    const gchar *text;

    if (act_user_get_locked (user)) {
        text = C_("Password mode", "Account disabled");
    } else {
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

static const gchar *
get_real_or_user_name (ActUser *user)
{
    const gchar *name;

    name = act_user_get_real_name (user);
    if (name == NULL) {
        name = act_user_get_user_name (user);
    }

    return name;
}

static void
account_type_changed (CcUserPage *self)
{
    ActUserAccountType account_type;
    gboolean is_admin;

    is_admin = gtk_switch_get_active (self->account_type_switch);
    account_type = is_admin ? ACT_USER_ACCOUNT_TYPE_ADMINISTRATOR : ACT_USER_ACCOUNT_TYPE_STANDARD;

    if (account_type != act_user_get_account_type (self->user)) {
        act_user_set_account_type (self->user, account_type);
    }
}

static void
fullname_entry_apply_cb (CcUserPage *self)
{
    const gchar *text;

    text = gtk_editable_get_text (GTK_EDITABLE (self->fullname_row));
    if (g_strcmp0 (text, act_user_get_real_name (self->user)) != 0 && is_valid_name (text)) {
        act_user_set_real_name (self->user, text);

        g_debug ("Updating real name to: %s", text);
    }
}

static void
language_chooser_response (CcUserPage        *self,
                           guint              response_id,
                           CcLanguageChooser *chooser)
{
    g_autofree gchar *language_name = NULL;
    const gchar *selected_language;

    if (response_id != GTK_RESPONSE_OK) {
        gtk_window_destroy (GTK_WINDOW (chooser));

        return;
    }

    selected_language = cc_language_chooser_get_language (chooser);
    if (!selected_language) {
        return;
    }

    if (g_strcmp0 (selected_language, act_user_get_language (self->user)) == 0) {
        return;
    }

    act_user_set_language (self->user, selected_language);

    language_name = gnome_get_language_from_locale (selected_language, NULL);
    cc_list_row_set_secondary_label (self->language_row, language_name);
} 

static void
change_language (CcUserPage *self)
{
    CcLanguageChooser *language_chooser;
    const gchar *current_language;

    current_language = act_user_get_language (self->user);
    language_chooser = cc_language_chooser_new ();

    g_signal_connect_object (language_chooser, "response",
                             G_CALLBACK (language_chooser_response), self,
                             G_CONNECT_SWAPPED);

    if (current_language && *current_language != '\0') {
        cc_language_chooser_set_language (language_chooser, current_language);
    }

    gtk_window_set_transient_for (GTK_WINDOW (language_chooser),
                                  GTK_WINDOW (gtk_widget_get_native (GTK_WIDGET (self))));
    gtk_window_present (GTK_WINDOW (language_chooser));
}

static void
change_password (CcUserPage *self)
{
    CcPasswordDialog *dialog = cc_password_dialog_new (self->user);
    GtkWindow *parent;

    parent = (GtkWindow *)gtk_widget_get_native (GTK_WIDGET (self));
    gtk_window_set_transient_for (GTK_WINDOW (dialog), parent);

    gtk_window_present (GTK_WINDOW (dialog));
}

static void
cc_user_page_buildable_add_child (GtkBuildable *buildable,
                                  GtkBuilder   *builder,
                                  GObject      *child,
                                  const gchar  *type)
{
    CcUserPage *self = CC_USER_PAGE (buildable);

    gtk_box_append (self->action_area, GTK_WIDGET (child));
}

static void
cc_user_page_buildable_init (GtkBuildableIface *iface)
{
    parent_buildable_iface = g_type_interface_peek_parent (iface);
    iface->add_child = cc_user_page_buildable_add_child;
}

static void
cc_user_page_get_property (GObject    *object,
                           guint       prop_id,
                           GValue     *value,
                           GParamSpec *pspec)
{
    CcUserPage *self = CC_USER_PAGE (object);

    switch (prop_id) {
    case PROP_LOCKED:
        g_value_set_boolean (value, self->locked);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
cc_user_page_set_property (GObject      *object,
                           guint         prop_id,
                           const GValue *value,
                           GParamSpec   *pspec)
{
  CcUserPage *self = CC_USER_PAGE (object);

  switch (prop_id) {
  case PROP_LOCKED:
      self->locked = g_value_get_boolean (value);
      break;
  default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}
static void
cc_user_page_finalize (GObject *object)
{
    CcUserPage *self = CC_USER_PAGE (object);

    G_OBJECT_CLASS (cc_user_page_parent_class)->finalize (object);
}

static void
cc_user_page_class_init (CcUserPageClass * klass)
{
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
    GObjectClass   *object_class = G_OBJECT_CLASS (klass);

    object_class->finalize = cc_user_page_finalize;
    object_class->get_property = cc_user_page_get_property;
    object_class->set_property = cc_user_page_set_property;

    g_object_class_install_property (object_class,
                                     PROP_LOCKED,
                                     g_param_spec_boolean ("locked",
                                                           "Locked",
                                                           "Whether the panel is locked",
                                                           TRUE,
                                                           G_PARAM_READWRITE));
    g_type_ensure (CC_TYPE_LIST_ROW);

    gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/system/users/cc-user-page.ui");

    gtk_widget_class_bind_template_child (widget_class, CcUserPage, account_type_row);
    gtk_widget_class_bind_template_child (widget_class, CcUserPage, account_type_switch);
    gtk_widget_class_bind_template_child (widget_class, CcUserPage, auto_login_row);
    gtk_widget_class_bind_template_child (widget_class, CcUserPage, action_area);
    gtk_widget_class_bind_template_child (widget_class, CcUserPage, language_row);
    gtk_widget_class_bind_template_child (widget_class, CcUserPage, fullname_row);
    gtk_widget_class_bind_template_child (widget_class, CcUserPage, password_row);
    gtk_widget_class_bind_template_child (widget_class, CcUserPage, permission_infobar);
    gtk_widget_class_bind_template_child (widget_class, CcUserPage, remove_user_button);

    gtk_widget_class_bind_template_callback (widget_class, account_type_changed);
    gtk_widget_class_bind_template_callback (widget_class, change_language);
    gtk_widget_class_bind_template_callback (widget_class, change_password);
    gtk_widget_class_bind_template_callback (widget_class, fullname_entry_apply_cb);
}

static void
cc_user_page_init (CcUserPage *self)
{
    gtk_widget_init_template (GTK_WIDGET (self));
}

CcUserPage *
cc_user_page_new (void)
{
    return CC_USER_PAGE (g_object_new (CC_TYPE_USER_PAGE, NULL));
}

void
cc_user_page_set_user (CcUserPage  *self,
                       ActUser     *user,
                       GPermission *permission)
{
    gboolean is_current_user = FALSE;

    g_assert (CC_IS_USER_PAGE (self));
    g_assert (ACT_IS_USER (user));

    self->user = g_object_ref (user);

    gtk_editable_set_text (GTK_EDITABLE (self->fullname_row), act_user_get_real_name (user));

    gtk_widget_set_visible (GTK_WIDGET (self->account_type_row), !would_demote_only_admin (user));
    gtk_switch_set_active (self->account_type_switch, act_user_get_account_type (user) == ACT_USER_ACCOUNT_TYPE_ADMINISTRATOR);

    gtk_widget_set_visible (GTK_WIDGET (self->auto_login_row), get_autologin_possible (user));
    adw_switch_row_set_active (self->auto_login_row, act_user_get_automatic_login (user));

    cc_list_row_set_secondary_label (self->password_row, get_password_mode_text (user));
    cc_list_row_set_secondary_label (self->language_row, get_user_language (user));

    is_current_user = (act_user_get_uid (user) == getuid ());
    gtk_widget_set_visible (self->remove_user_button, !is_current_user);

    cc_permission_infobar_set_permission (self->permission_infobar, permission);
    cc_permission_infobar_set_title (self->permission_infobar, _("Some settings are locked"));
    g_object_bind_property (permission, "allowed", self, "locked", G_BINDING_SYNC_CREATE | G_BINDING_INVERT_BOOLEAN);
}
