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

#include <config.h>

#include "cc-usb-device-entry.h"

#include "cc-usb-resources.h"

#include "usb-device.h"

#include <glib/gi18n.h>

#define RESOURCE_UI "/org/gnome/control-center/usb/cc-usb-device-entry.ui"

struct _CcUsbDeviceEntry
{
  GtkListBoxRow parent;

  UsbDevice   *device;

  /* main ui */
  GtkLabel *name_label;
  GtkLabel *status_label;
};

G_DEFINE_TYPE (CcUsbDeviceEntry, cc_usb_device_entry, GTK_TYPE_LIST_BOX_ROW);

static void
entry_set_name (CcUsbDeviceEntry *entry)
{
  const char *name = NULL;
  UsbDevice *dev = entry->device;

  g_return_if_fail (dev != NULL);

  name = usb_device_get_name (dev);
  gtk_label_set_label (entry->name_label, name);
}

void
entry_update_status (CcUsbDeviceEntry *entry)
{
  gboolean authorized;
  g_autofree char *label = NULL;

  authorized = usb_device_get_authorization (entry->device);
  label = authorized ? g_strdup_printf ("Full") : g_strdup_printf ("Limited");
  gtk_label_set_label (entry->status_label, label);
}

static void
cc_usb_device_entry_finalize (GObject *object)
{
  G_OBJECT_CLASS (cc_usb_device_entry_parent_class)->finalize (object);
}

static void
cc_usb_device_entry_class_init (CcUsbDeviceEntryClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = cc_usb_device_entry_finalize;

  gtk_widget_class_set_template_from_resource (widget_class, RESOURCE_UI);
  gtk_widget_class_bind_template_child (widget_class, CcUsbDeviceEntry, name_label);
  gtk_widget_class_bind_template_child (widget_class, CcUsbDeviceEntry, status_label);
}

static void
cc_usb_device_entry_init (CcUsbDeviceEntry *entry)
{
  g_resources_register (cc_usb_get_resource ());
  gtk_widget_init_template (GTK_WIDGET (entry));
}

/* public function */

CcUsbDeviceEntry *
cc_usb_device_entry_new (UsbDevice *device)
{
  CcUsbDeviceEntry *entry;

  entry = g_object_new (CC_TYPE_USB_DEVICE_ENTRY, NULL);
  entry->device = g_object_ref (device);

  entry_set_name (entry);
  entry_update_status (entry);

  return entry;
}

UsbDevice *
cc_usb_device_entry_get_device (CcUsbDeviceEntry *entry)
{
  g_return_val_if_fail (entry != NULL, NULL);
  g_return_val_if_fail (CC_IS_USB_DEVICE_ENTRY (entry), NULL);

  return entry->device;
}
