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

#include <glib/gi18n.h>
#include <string.h>
#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <libudev.h>
#include <gudev/gudev.h>

#include "gnome-usb-properties.h"
#include "cc-usb-device-entry.h"
#include "gsd-input-helper.h"
#include "gsd-device-manager.h"
#include "list-box-helper.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>

struct _CcUsbProperties
{
  GtkBin parent_instance;

  GtkWidget *scrolled_window;

  GsdDeviceManager *device_manager;
  guint device_added_id;
  guint device_removed_id;

  GtkBox *devices_box;
  GtkListBox *devices_list;

  GHashTable *devices;
  GUdevClient *udev_client;
};

/* Panel signals */
static gboolean on_keyboard_protection_state_set_cb (CcUsbProperties *panel,
                                                     gboolean         state,
                                                     GtkSwitch       *toggle);

static void on_device_entry_row_activated_cb (CcUsbProperties *panel,
                                              GtkListBoxRow   *row);

G_DEFINE_TYPE (CcUsbProperties, cc_usb_properties, GTK_TYPE_BIN);

static gboolean
on_keyboard_protection_state_set_cb (CcUsbProperties *panel,
                                     gboolean         enable,
                                     GtkSwitch       *toggle)
{
  return TRUE;
}

static void
on_device_entry_row_activated_cb (CcUsbProperties *panel,
                                  GtkListBoxRow   *row)
{
  CcUsbDeviceEntry *entry;
  UsbDevice *device;
  gboolean authorized;

  if (!CC_IS_USB_DEVICE_ENTRY (row))
    return;

  entry = CC_USB_DEVICE_ENTRY (row);
  device = cc_usb_device_entry_get_device (entry);

  authorized = usb_device_get_authorization (device);
  usb_device_set_authorization (device, !authorized);
  entry_update_status (entry);
  return;
}

static void
add_single_device (GsdDevice *device,
                   CcUsbProperties *self)
{
  CcUsbDeviceEntry *entry;
  g_autofree const char *auth = NULL;
  const gchar *devpath;
  const gchar *vendor;
  const gchar *product;
  g_autoptr(GUdevDevice) this_udev_device = NULL;
  UsbDevice *dev;
  GsdDeviceType type;

  type = gsd_device_get_device_type (device);
  /*If the device is not a keyboard we don't add it to the list. */
  if (!(type & GSD_DEVICE_TYPE_KEYBOARD))
    return;

  gsd_device_get_device_ids (device, &vendor, &product);

  devpath = g_strdup (gsd_device_get_device_file (device));
  g_debug ("vendor: %s product: %s devpath: %s", vendor, product, devpath);

  this_udev_device = g_udev_client_query_by_device_file (self->udev_client, devpath);
  auth = g_strdup (g_udev_device_get_property (this_udev_device, "AUTHORIZED"));

  dev = usb_device_new (g_strcmp0 (auth, "1") == 0,
                        gsd_device_get_name (device),
                        g_ascii_strup (product, strlen (product)),
                        g_ascii_strup (vendor, strlen (vendor)));
  entry = cc_usb_device_entry_new (dev);
  gtk_container_add (GTK_CONTAINER (self->devices_list), GTK_WIDGET (entry));
  g_hash_table_insert (self->devices, (gpointer) devpath, entry);
}

static void
initialize_keyboards_list (CcUsbProperties *self)
{
  g_autoptr(GList) kb_devices = NULL;

  kb_devices = gsd_device_manager_list_devices (self->device_manager, GSD_DEVICE_TYPE_KEYBOARD);

  for (; kb_devices != NULL; kb_devices = kb_devices->next) {
    add_single_device (kb_devices->data, self);
  }
}

/* Set up the property editors in the dialog. */
static void
setup_dialog (CcUsbProperties *self)
{
  initialize_keyboards_list (self);

  gtk_widget_show_all (GTK_WIDGET (self->devices_list));
  gtk_widget_show_all (GTK_WIDGET (self->devices_box));
}

static void
device_added (GsdDeviceManager *device_manager,
              GsdDevice *device,
              CcUsbProperties *self)
{
  add_single_device (device, self);
}

static void
device_removed (GsdDeviceManager *device_manager,
                GsdDevice *device,
                CcUsbProperties *self)
{
  CcUsbDeviceEntry *entry;
  g_autofree const gchar *devpath = NULL;
  const gchar *vendor;
  const gchar *product;

  gsd_device_get_device_ids (device, &vendor, &product);

  devpath = g_strdup (gsd_device_get_device_file (device));
  g_debug ("vendor: %s product: %s devpath: %s", vendor, product, devpath);
  entry = g_hash_table_lookup (self->devices, devpath);

  /*If the removed device was not in the list we do nothing. */
  if (!entry)
    return;

  gtk_widget_destroy (GTK_WIDGET (entry));
  g_hash_table_remove (self->devices, devpath);
}

static void
on_content_size_changed (CcUsbProperties *self,
                         GtkAllocation   *allocation)
{
  if (allocation->height < 490) {
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (self->scrolled_window),
                                    GTK_POLICY_NEVER, GTK_POLICY_NEVER);
  } else {
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (self->scrolled_window),
                                    GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_min_content_height (GTK_SCROLLED_WINDOW (self->scrolled_window), 490);
  }
}

static void
cc_usb_properties_finalize (GObject *object)
{
  CcUsbProperties *self = CC_USB_PROPERTIES (object);

  if (self->device_manager != NULL) {
    g_signal_handler_disconnect (self->device_manager, self->device_added_id);
    self->device_added_id = 0;
    g_signal_handler_disconnect (self->device_manager, self->device_removed_id);
    self->device_removed_id = 0;
    self->device_manager = NULL;
  }

  G_OBJECT_CLASS (cc_usb_properties_parent_class)->finalize (object);
}

static void
cc_usb_properties_class_init (CcUsbPropertiesClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = cc_usb_properties_finalize;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/usb/gnome-usb-properties.ui");

  gtk_widget_class_bind_template_child (widget_class, CcUsbProperties, devices_box);
  gtk_widget_class_bind_template_child (widget_class, CcUsbProperties, devices_list);
  gtk_widget_class_bind_template_child (widget_class, CcUsbProperties, scrolled_window);

  gtk_widget_class_bind_template_callback (widget_class, on_content_size_changed);
  gtk_widget_class_bind_template_callback (widget_class, on_keyboard_protection_state_set_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_device_entry_row_activated_cb);
}

static void
cc_usb_properties_init (CcUsbProperties *self)
{
  const gchar *subsystems[] = {"usb", NULL};

  gtk_widget_init_template (GTK_WIDGET (self));

  self->devices = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, NULL);

	self->udev_client = g_udev_client_new (subsystems);

  self->device_manager = gsd_device_manager_get ();
  self->device_added_id = g_signal_connect (self->device_manager, "device-added",
                                            G_CALLBACK (device_added), self);
  self->device_removed_id = g_signal_connect (self->device_manager, "device-removed",
                                              G_CALLBACK (device_removed), self);

  setup_dialog (self);
}

GtkWidget *
cc_usb_properties_new (void)
{
  return (GtkWidget *) g_object_new (CC_TYPE_USB_PROPERTIES, NULL);
}
