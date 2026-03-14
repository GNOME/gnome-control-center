/*
 * cc-security-login-page.c
 *
 * Copyright 2026 Red Hat
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
#define G_LOG_DOMAIN "cc-security-login-page"

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "cc-security-login-page.h"
#include "cc-fingerprint-dialog.h"
#include "cc-fingerprint-manager.h"
#include "cc-password-dialog.h"
#include "cc-list-row.h"
#include "user-utils.h"

#include <adwaita.h>
#include <config.h>
#include <errno.h>
#include <locale.h>
#include <glib/gi18n.h>
#include <gio/gio.h>
#include <gtk/gtk.h>

struct _CcSecurityLoginPage {
  AdwNavigationPage  parent_instance;

  AdwActionRow *auto_login_row;
  GtkSwitch    *auto_login_switch;
  CcListRow    *fingerprint_row;
  CcListRow    *password_row;

  ActUser *user;
  GSettings            *login_screen_settings;
  CcFingerprintManager *fingerprint_manager;
};

G_DEFINE_TYPE (CcSecurityLoginPage, cc_security_login_page, ADW_TYPE_NAVIGATION_PAGE)

static gboolean
get_autologin_possible (ActUser *user)
{
  gboolean locked;
  gboolean set_password_at_login;

  locked = act_user_get_locked (user);
  set_password_at_login = (act_user_get_password_mode (user) == ACT_USER_PASSWORD_MODE_SET_AT_LOGIN);

  return !(locked || set_password_at_login);
}

static void
autologin_changed (CcSecurityLoginPage *self)
{
  ActUserManager *user_manager = act_user_manager_get_default ();
  gboolean active;

  active = gtk_switch_get_active (self->auto_login_switch);
  if (active != act_user_get_automatic_login (self->user)) {
    act_user_set_automatic_login (self->user, active);

    if (act_user_get_automatic_login (self->user)) {
      g_autoptr(GSList) list = NULL;
      GSList *l;
      list = act_user_manager_list_users (user_manager);

      for (l = list; l != NULL; l = l->next) {
        ActUser *u = l->data;

        if (act_user_get_uid (u) != act_user_get_uid (self->user)) {
          act_user_set_automatic_login (u, FALSE);
        }
      }
    }
  }
}

static void
update_fingerprint_row_state (CcSecurityLoginPage  *self,
                              GParamSpec           *spec,
                              CcFingerprintManager *manager)
{
  CcFingerprintState state = cc_fingerprint_manager_get_state (manager);
  gboolean visible = FALSE;

  visible = (act_user_get_uid (self->user) == getuid () &&
             act_user_is_local_account (self->user) &&
             (self->login_screen_settings &&
             g_settings_get_boolean (self->login_screen_settings,
                                     "enable-fingerprint-authentication")));
  gtk_widget_set_visible (GTK_WIDGET (self->fingerprint_row), visible);
  if (!visible)
    return;

  if (state != CC_FINGERPRINT_STATE_UPDATING)
      gtk_widget_set_visible (GTK_WIDGET (self->fingerprint_row),
                              state != CC_FINGERPRINT_STATE_NONE);

  gtk_widget_set_sensitive (GTK_WIDGET (self->fingerprint_row),
                            state != CC_FINGERPRINT_STATE_UPDATING);

  if (state == CC_FINGERPRINT_STATE_ENABLED)
    cc_list_row_set_secondary_label (self->fingerprint_row, _("Enabled"));
  else if (state == CC_FINGERPRINT_STATE_DISABLED)
    cc_list_row_set_secondary_label (self->fingerprint_row, _("Disabled"));
}

static void
change_fingerprint (CcSecurityLoginPage *self)
{
  CcFingerprintDialog *dialog;

  dialog = cc_fingerprint_dialog_new (self->fingerprint_manager);
  adw_dialog_present (ADW_DIALOG (dialog), GTK_WIDGET (self));
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
        text = C_("Password mode", "Set Up");
        break;
      case ACT_USER_PASSWORD_MODE_SET_AT_LOGIN:
        text = C_("Password mode", "To be set at next login");
        break;
      case ACT_USER_PASSWORD_MODE_NONE:
        text = C_("Password mode", "Not Set Up");
        break;
      default:
        g_assert_not_reached ();
    }
  }

  return text;
}

static void
change_password (CcSecurityLoginPage *self)
{
  CcPasswordDialog *dialog = cc_password_dialog_new (self->user);

  adw_dialog_present (ADW_DIALOG (dialog), GTK_WIDGET (self));
}

static void
cc_security_login_page_dispose (GObject *object)
{
  CcSecurityLoginPage *self = CC_SECURITY_LOGIN_PAGE (object);

  g_clear_object (&self->fingerprint_manager);
  g_clear_object (&self->login_screen_settings);

  G_OBJECT_CLASS (cc_security_login_page_parent_class)->dispose (object);
}

static void
cc_security_login_page_init (CcSecurityLoginPage *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  self->login_screen_settings = settings_or_null ("org.gnome.login-screen");
}

static void
cc_security_login_page_class_init (CcSecurityLoginPageClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = cc_security_login_page_dispose;

  g_type_ensure (CC_TYPE_LIST_ROW);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/system/users/cc-security-login-page.ui");

  gtk_widget_class_bind_template_child (widget_class, CcSecurityLoginPage, auto_login_row);
  gtk_widget_class_bind_template_child (widget_class, CcSecurityLoginPage, auto_login_switch);
  gtk_widget_class_bind_template_child (widget_class, CcSecurityLoginPage, fingerprint_row);
  gtk_widget_class_bind_template_child (widget_class, CcSecurityLoginPage, password_row);

  gtk_widget_class_bind_template_callback (widget_class, autologin_changed);
  gtk_widget_class_bind_template_callback (widget_class, change_password);
  gtk_widget_class_bind_template_callback (widget_class, change_fingerprint);
}

void
cc_security_login_page_set_user (CcSecurityLoginPage *self,
                                 ActUser             *user)
{
  g_clear_object (&self->user);
  self->user = g_object_ref (user);

  g_clear_object (&self->fingerprint_manager);

  cc_list_row_set_secondary_label (self->password_row, get_password_mode_text (user));

  if (!self->fingerprint_manager) {
    self->fingerprint_manager = cc_fingerprint_manager_new (user);
    g_signal_connect_object (self->fingerprint_manager,
                             "notify::state",
                             G_CALLBACK (update_fingerprint_row_state),
                             self,
                             G_CONNECT_SWAPPED);
    update_fingerprint_row_state (self, NULL, self->fingerprint_manager);
  }

  g_signal_handlers_block_by_func (self->auto_login_switch, autologin_changed, self);
  gtk_widget_set_visible (GTK_WIDGET (self->auto_login_row), get_autologin_possible (user));
  gtk_switch_set_active (self->auto_login_switch, act_user_get_automatic_login (user));
  g_signal_handlers_unblock_by_func (self->auto_login_switch, autologin_changed, self);
}
