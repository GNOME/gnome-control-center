/*
 * Copyright 2026 Adrian Vovk
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
 */

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "cc-factory-reset-page"

#include <glib/gi18n.h>

#include "cc-list-row.h"
#include "cc-secure-shell.h"
#include "cc-secure-shell-page.h"
#include "cc-systemd-service.h"

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

struct _CcFactoryResetPage {
  AdwNavigationPage parent_instance;

  AdwPreferencesGroup *users_group;
  AdwPreferencesGroup *apps_group;
  AdwActionRow *users_row;
  AdwActionRow *factory_reset_row;

  GDBusProxy *logind_proxy;
};

G_DEFINE_TYPE (CcFactoryResetPage, cc_factory_reset_page, ADW_TYPE_NAVIGATION_PAGE)

static void
on_confirm_clicked (CcFactoryResetPage *self)
{
  // TODO:
}

static void
cc_factory_reset_page_dispose (GObject *object)
{
  CcSecureShellPage *self = (CcSecureShellPage *) object;

  g_object_clear (&logind_proxy);

  G_OBJECT_CLASS (cc_secure_shell_page_parent_class)->dispose (object);
}

static void
cc_factory_reset_page_class_init (CcSecureShellPageClass * klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GObjectClass   *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = cc_factory_reset_page_dispose;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/system/factory-reset/cc-factory-reset-page.ui");

  gtk_widget_class_bind_template_child (widget_class, CcFactoryResetPage, hostname_row);
  gtk_widget_class_bind_template_child (widget_class, CcSecureShellPage, secure_shell_row);
  gtk_widget_class_bind_template_child (widget_class, CcSecureShellPage, toast_overlay);

  gtk_widget_class_bind_template_callback (widget_class, on_copy_ssh_command_button_clicked);
  gtk_widget_class_bind_template_callback (widget_class, secure_shell_row_activate);
}

static void
cc_factory_reset_page_init (CcFactoryResetPage *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}
