/* cc-permission-infobar.c
 *
 * Copyright (C) 2020 Red Hat, Inc
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
 * Author(s):
 *   Felipe Borges <felipeborges@gnome.org>
 *
 */

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "cc-permission-infobar"

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include "cc-permission-infobar.h"

struct _CcPermissionInfobar
{
  GtkRevealer    parent_instance;

  GtkLockButton *lock_button;
};

G_DEFINE_TYPE (CcPermissionInfobar, cc_permission_infobar, GTK_TYPE_REVEALER)

static void
on_permission_changed (CcPermissionInfobar *self)
{
  GPermission *permission;
  gboolean is_authorized;

  permission = gtk_lock_button_get_permission (self->lock_button);
  is_authorized = g_permission_get_allowed (permission);

  gtk_revealer_set_reveal_child (GTK_REVEALER (self), !is_authorized);
}

static void
cc_permission_infobar_class_init (CcPermissionInfobarClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  gtk_widget_class_set_template_from_resource (widget_class,
                                               "/org/gnome/control-center/"
                                               "common/cc-permission-infobar.ui");

  gtk_widget_class_bind_template_child (widget_class, CcPermissionInfobar, lock_button);
}

static void
cc_permission_infobar_init (CcPermissionInfobar *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

void
cc_permission_infobar_set_permission (CcPermissionInfobar *self,
                                      GPermission         *permission)
{
  g_return_if_fail (CC_IS_PERMISSION_INFOBAR (self));

  gtk_lock_button_set_permission (self->lock_button, permission);

  g_signal_connect_object (permission, "notify",
                           G_CALLBACK (on_permission_changed),
                           self,
                           G_CONNECT_SWAPPED);
  on_permission_changed (self);
}
