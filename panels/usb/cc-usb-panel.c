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
  CcPanel       parent_instance;

  GCancellable *cancel;

  GtkLockButton *lock_button;

  /* Main UI */
  GtkWidget    *stack;
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
  G_OBJECT_CLASS (cc_usb_panel_parent_class)->constructed (object);
}

static void
cc_usb_panel_init (CcUsbPanel *self)
{
  GtkWidget *prefs_widget;

  g_resources_register (cc_usb_get_resource ());

  prefs_widget = cc_usb_properties_new ();
  gtk_widget_show (prefs_widget);

  self->stack = gtk_stack_new ();
  gtk_widget_show (self->stack);
  gtk_stack_add_named (GTK_STACK (self->stack), prefs_widget, "prefs_widget");

  gtk_container_add (GTK_CONTAINER (self), self->stack);
}

static void
cc_usb_panel_class_init (CcUsbPanelClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  //CcPanelClass *panel_class = CC_PANEL_CLASS (klass);

  object_class->dispose = cc_usb_panel_dispose;
  object_class->constructed = cc_usb_panel_constructed;
}
