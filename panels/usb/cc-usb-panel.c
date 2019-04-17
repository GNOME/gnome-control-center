/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * Copyright (C) 2019 GNOME
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
 * Authors: Ludovico de Nittis <denittis@gnome.org>
 *
 */

#include "cc-usb-panel.h"
#include "cc-usb-resources.h"

#include "gnome-usb-properties.h"
#include <gtk/gtk.h>

#include <glib/gi18n.h>
#include <polkit/polkit.h>

#define USB_PERMISSION "org.gnome.controlcenter.usb"

struct _CcUsbPanel
{
  CcPanel        parent_instance;

  GCancellable  *cancel;

  GtkLockButton *lock_button;

  /* Polkit */
  GPermission   *permission;

  /* Main UI */
  GtkWidget     *stack;
};

CC_PANEL_REGISTER (CcUsbPanel, cc_usb_panel)

static void
cc_usb_panel_dispose (GObject *object)
{
  G_OBJECT_CLASS (cc_usb_panel_parent_class)->dispose (object);
}

static void
cc_usb_panel_constructed (GObject *object)
{
  CcUsbPanel *self = CC_USB_PANEL (object);
  CcShell *shell;
  GtkWidget *button;

  G_OBJECT_CLASS (cc_usb_panel_parent_class)->constructed (object);

  /* Add Unlock button to shell header */
  shell = cc_panel_get_shell (CC_PANEL (self));

  button = gtk_lock_button_new (self->permission);

  gtk_widget_set_valign (button, GTK_ALIGN_CENTER);
  gtk_widget_set_visible (button, TRUE);

  cc_shell_embed_widget_in_header (shell, button);
}

static void
on_permission_notify_cb (GPermission *permission,
                         GParamSpec  *pspec,
                         CcUsbPanel *panel)
{
  gboolean is_allowed = g_permission_get_allowed (permission);
  g_debug ("permission: %i", is_allowed);

  //gtk_widget_set_sensitive (GTK_WIDGET (panel->authmode_switch), is_allowed);
}


static void
cc_usb_panel_init (CcUsbPanel *self)
{
  GtkWidget *prefs_widget;
  g_autoptr(GError) error = NULL;

  g_resources_register (cc_usb_get_resource ());

  prefs_widget = cc_usb_properties_new ();
  gtk_widget_show (prefs_widget);

  self->stack = gtk_stack_new ();
  gtk_widget_show (self->stack);
  gtk_stack_add_named (GTK_STACK (self->stack), prefs_widget, "prefs_widget");

  gtk_container_add (GTK_CONTAINER (self), self->stack);

  self->permission = (GPermission *)polkit_permission_new_sync (USB_PERMISSION, NULL, NULL, &error);
  if (self->permission != NULL) {
    g_signal_connect_object (self->permission, "notify",
                             G_CALLBACK (on_permission_notify_cb),
                             self,
                             G_CONNECT_AFTER);

    g_debug ("Polkit permission initialized");
    //on_permission_changed (self);
  } else {
    g_warning ("Cannot create '%s' permission: %s", USB_PERMISSION, error->message);
  }

}

static void
cc_usb_panel_class_init (CcUsbPanelClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  //CcPanelClass *panel_class = CC_PANEL_CLASS (klass);

  object_class->dispose = cc_usb_panel_dispose;
  object_class->constructed = cc_usb_panel_constructed;
}
