/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * Copyright (C) 2019 GNOME
 * Copyright (C) 2019 Collabora Ltd
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

#include "cc-usb-device-entry.h"
#include <gtk/gtk.h>
#include <gudev/gudev.h>
#include "usb-store.h"

#include <glib/gi18n.h>
#include <polkit/polkit.h>

#define USB_PERMISSION "org.gnome.controlcenter.usb"

struct _CcUsbPanel
{
  CcPanel        parent_instance;

  GCancellable  *cancel;

  /* Headerbar */
  GtkLockButton *lock_button;

  /* Polkit */
  GPermission   *permission;

  /* Main UI */
  GtkWidget     *stack;

  /* Devices */
  guint device_added_id;
  guint device_removed_id;

  GtkBox *devices_box;
  GtkListBox *devices_list;

  GHashTable *devices;
  GUdevClient *udev_client;

  UsbStore *store;
};

/* Panel signals */
static gboolean on_keyboard_protection_state_set_cb (CcUsbPanel *panel,
                                                     gboolean    state,
                                                     GtkSwitch  *toggle);

static void on_device_entry_row_activated_cb (CcUsbPanel    *panel,
                                              GtkListBoxRow *row);

CC_PANEL_REGISTER (CcUsbPanel, cc_usb_panel)

static void
cc_usb_panel_dispose (GObject *object)
{
  G_OBJECT_CLASS (cc_usb_panel_parent_class)->dispose (object);
}

static gboolean
on_keyboard_protection_state_set_cb (CcUsbPanel *panel,
                                     gboolean    enable,
                                     GtkSwitch  *toggle)
{
  return TRUE;
}

static void
on_device_entry_row_activated_cb (CcUsbPanel    *panel,
                                  GtkListBoxRow *row)
{
  CcUsbDeviceEntry *entry;
  UsbDevice *device;
  gboolean authorized;

  if (!CC_IS_USB_DEVICE_ENTRY (row))
    return;

  entry = CC_USB_DEVICE_ENTRY (row);
  device = cc_usb_device_entry_get_device (entry);

  authorized = !usb_device_get_authorization (device);

  if (usb_device_set_authorization (device, authorized))
    entry_update_status (entry);
  return;
}

static gboolean
device_is_keyboard (GUdevDevice *device)
{
  const gchar *device_file;

  device_file = g_udev_device_get_device_file (device);

  if (!device_file || strstr (device_file, "/event") == NULL)
    return FALSE;

  return g_udev_device_get_property_as_boolean (device, "ID_INPUT_KEYBOARD");
}

static void
add_single_device (GUdevDevice     *device,
                   CcUsbPanel *self)
{
  CcUsbDeviceEntry *entry;
  g_autofree const char *auth = NULL;
  const gchar *devpath;
  const gchar *vendor;
  const gchar *product;
  const gchar *name;
  const gchar *sysfs_path;
  UsbDevice *dev;
  g_autoptr(GUdevDevice) parent = NULL;

  vendor = g_strdup (g_udev_device_get_property (device, "ID_VENDOR_ID"));
  product = g_strdup (g_udev_device_get_property (device, "ID_MODEL_ID"));
  devpath = g_strdup (g_udev_device_get_device_file (device));
  sysfs_path = g_strdup (g_udev_device_get_sysfs_path (device));

  if (vendor == NULL || product == NULL)
      return;

  g_debug ("vendor: %s product: %s devpath: %s", vendor, product, devpath);
  g_debug ("sysfspath: %s", sysfs_path);

  auth = g_strdup (g_udev_device_get_property (device, "GNOME_AUTHORIZED"));

  parent = g_udev_device_get_parent (device);
  g_assert (parent != NULL);
  name = g_udev_device_get_sysfs_attr (parent, "name");

  dev = usb_device_new (g_strcmp0 (auth, "1") == 0,
                        name,
                        product,
                        sysfs_path,
                        vendor);

  entry = cc_usb_device_entry_new (dev);
  gtk_container_add (GTK_CONTAINER (self->devices_list), GTK_WIDGET (entry));
  g_hash_table_insert (self->devices, (gpointer) devpath, entry);
}

static void
initialize_keyboards_list (CcUsbPanel *self)
{
  const gchar *subsystems[] = { "input", NULL };
  g_autoptr(GList) devices = NULL;
  GList *l;

  devices = g_udev_client_query_by_subsystem (self->udev_client,
                subsystems[0]);

  for (l = devices; l; l = l->next) {
    g_autoptr(GUdevDevice) device = l->data;
    if (device_is_keyboard (device))
      add_single_device (device, self);
  }
}

static void
remove_device (GUdevDevice *device,
               CcUsbPanel  *self)
{
  CcUsbDeviceEntry *entry;
  g_autofree const gchar *devpath = NULL;
  const gchar *vendor;
  const gchar *product;

  vendor = g_strdup (g_udev_device_get_property (device, "ID_VENDOR_ID"));
  product = g_strdup (g_udev_device_get_property (device, "ID_MODEL_ID"));
  devpath = g_strdup (g_udev_device_get_device_file (device));

  g_debug ("vendor: %s product: %s devpath: %s", vendor, product, devpath);
  entry = g_hash_table_lookup (self->devices, devpath);

  /*If the removed device was not in the list we do nothing. */
  if (!entry)
    return;

  gtk_widget_destroy (GTK_WIDGET (entry));
  g_hash_table_remove (self->devices, devpath);
}

static void
udev_event_cb (GUdevClient *client,
               gchar       *action,
               GUdevDevice *device,
               CcUsbPanel  *self)
{
  if (!device_is_keyboard (device))
    return;

  if (g_strcmp0 (action, "add") == 0)
    add_single_device (device, self);
  else if (g_strcmp0 (action, "remove") == 0)
    remove_device (device, self);
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
                         CcUsbPanel  *panel)
{
  gboolean is_allowed = g_permission_get_allowed (permission);

  gtk_widget_set_sensitive (GTK_WIDGET (panel->devices_box), is_allowed);
}


static void
cc_usb_panel_init (CcUsbPanel *self)
{
  const gchar *subsystems[] = {"input", NULL};
  g_autoptr(GError) error = NULL;

  g_resources_register (cc_usb_get_resource ());

  gtk_widget_init_template (GTK_WIDGET (self));

  self->devices = g_hash_table_new_full (g_str_hash, g_str_equal,
                                         (GDestroyNotify) g_free,
                                         (GDestroyNotify) g_object_unref);

  self->udev_client = g_udev_client_new (subsystems);
  g_signal_connect (self->udev_client, "uevent",
                    G_CALLBACK (udev_event_cb), self);

  self->permission = (GPermission *)polkit_permission_new_sync (USB_PERMISSION, NULL, NULL, &error);
  if (self->permission != NULL) {
    g_signal_connect_object (self->permission, "notify",
                             G_CALLBACK (on_permission_notify_cb),
                             self,
                             G_CONNECT_AFTER);

    g_debug ("Polkit permission initialized");
    on_permission_notify_cb (self->permission, NULL, self);
  } else {
    g_warning ("Cannot create '%s' permission: %s", USB_PERMISSION, error->message);
  }

  initialize_keyboards_list (self);
}

static void
cc_usb_panel_class_init (CcUsbPanelClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/usb/cc-usb-panel.ui");

  gtk_widget_class_bind_template_child (widget_class, CcUsbPanel, devices_box);
  gtk_widget_class_bind_template_child (widget_class, CcUsbPanel, devices_list);

  gtk_widget_class_bind_template_callback (widget_class, on_keyboard_protection_state_set_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_device_entry_row_activated_cb);

  object_class->dispose = cc_usb_panel_dispose;
  object_class->constructed = cc_usb_panel_constructed;
}
