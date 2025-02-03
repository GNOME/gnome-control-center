/*
 * Copyright 2024 Red Hat, Inc
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
#define G_LOG_DOMAIN "cc-secure-shell-page"

#include <glib/gi18n.h>

#include "cc-hostname.h"
#include "cc-list-row.h"
#include "cc-secure-shell.h"
#include "cc-secure-shell-page.h"
#include "cc-systemd-service.h"

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

struct _CcSecureShellPage {
  AdwDialog       parent_instance;

  AdwActionRow    *hostname_row;
  AdwSwitchRow    *secure_shell_row;
  AdwToastOverlay *toast_overlay;

  GCancellable *cancellable;
};

G_DEFINE_TYPE (CcSecureShellPage, cc_secure_shell_page, ADW_TYPE_DIALOG)

static void
on_copy_ssh_command_button_clicked (CcSecureShellPage *self)
{
  gdk_clipboard_set_text (gtk_widget_get_clipboard (GTK_WIDGET (self)),
                          adw_action_row_get_subtitle (ADW_ACTION_ROW (self->hostname_row)));

  adw_toast_overlay_add_toast (self->toast_overlay, adw_toast_new (_("Command copied to clipboard")));
}

static void
secure_shell_row_activate (CcSecureShellPage *self)
{
  cc_secure_shell_set_enabled (self->cancellable, self->secure_shell_row);
}

static void
cc_secure_shell_page_dispose (GObject *object)
{
  CcSecureShellPage *self = (CcSecureShellPage *) object;

  g_cancellable_cancel (self->cancellable);
  g_clear_object (&self->cancellable);

  G_OBJECT_CLASS (cc_secure_shell_page_parent_class)->dispose (object);
}

static void
cc_secure_shell_page_class_init (CcSecureShellPageClass * klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GObjectClass   *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = cc_secure_shell_page_dispose;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/system/secure-shell/cc-secure-shell-page.ui");

  gtk_widget_class_bind_template_child (widget_class, CcSecureShellPage, hostname_row);
  gtk_widget_class_bind_template_child (widget_class, CcSecureShellPage, secure_shell_row);
  gtk_widget_class_bind_template_child (widget_class, CcSecureShellPage, toast_overlay);

  gtk_widget_class_bind_template_callback (widget_class, on_copy_ssh_command_button_clicked);
  gtk_widget_class_bind_template_callback (widget_class, secure_shell_row_activate);
}

static void
cc_secure_shell_page_init (CcSecureShellPage *self)
{
  g_autofree gchar *hostname = NULL;
  g_autofree gchar *command = NULL;

  gtk_widget_init_template (GTK_WIDGET (self));

  self->cancellable = g_cancellable_new ();

  cc_secure_shell_get_enabled (self->secure_shell_row);
  g_signal_connect_object (self->secure_shell_row,
                           "notify::active",
                           G_CALLBACK (secure_shell_row_activate),
                           self,
                           G_CONNECT_SWAPPED);

  hostname = cc_hostname_get_static_hostname (cc_hostname_get_default ());
  command = g_strdup_printf ("ssh %s", hostname);
  adw_action_row_set_subtitle (self->hostname_row, command);
}
