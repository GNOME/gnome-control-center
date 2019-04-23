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

#include "shell/cc-application.h"
#include "cc-usb-panel.h"
#include "cc-usb-resources.h"

#include "cc-usb-device-entry.h"
#include <gtk/gtk.h>
#include <gudev/gudev.h>
#include "usb-store.h"

#include <glib/gi18n.h>
#include <polkit/polkit.h>

#ifdef GDK_WINDOWING_WAYLAND
#include <gdk/gdkwayland.h>
#endif

#define USB_PERMISSION "org.gnome.controlcenter.usb"
#define KEYBOARD_PROTECTION "keyboard-protection"

struct _CcUsbPanel
{
  CcPanel        parent_instance;

  GSettings     *privacy_settings;

  /* Headerbar */
  GtkLockButton *lock_button;

  /* Polkit */
  GPermission   *permission;

  /* Main UI */
  GtkWidget     *stack;

  /* Devices */
  guint          device_added_id;
  guint          device_removed_id;

  GtkBox        *devices_box;
  GtkListBox    *devices_list;
  GtkStack      *devices_stack;
  GtkSwitch     *keyboard_protection_switch;

  GHashTable    *devices;
  GUdevClient   *udev_client;

  UsbStore      *store;
};

static void on_device_entry_row_activated_cb (CcUsbPanel    *panel,
                                              GtkListBoxRow *row);

CC_PANEL_REGISTER (CcUsbPanel, cc_usb_panel)

void cc_usb_panel_static_init_func (void)
{
  CcApplication *application;
  GdkDisplay *display;
  gboolean is_wayland = FALSE;

  #ifdef GDK_WINDOWING_WAYLAND
    display = gdk_display_get_default ();
    if (GDK_IS_WAYLAND_DISPLAY (display))
      is_wayland = TRUE;
  #endif /* GDK_WINDOWING_WAYLAND */

  application = CC_APPLICATION (g_application_get_default ());

  cc_shell_model_set_panel_visibility (cc_application_get_model (application),
                                       "usb",
                                       is_wayland ? CC_PANEL_VISIBLE : CC_PANEL_HIDDEN);

  g_debug ("USB panel visible: %s", is_wayland ? "yes" : "no");
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

  gtk_stack_set_visible_child_name (self->devices_stack, "have-keyboards");
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

  if (g_hash_table_size (self->devices) == 0)
    gtk_stack_set_visible_child_name (self->devices_stack, "no-keyboards");

}

static void
on_keyboard_settings_changed (GSettings  *settings,
                              const char *key,
                              CcUsbPanel *panel)
{
  gboolean protection;
  gboolean is_allowed;

  if (g_str_equal (key, KEYBOARD_PROTECTION) == FALSE)
    return;

  protection = g_settings_get_boolean (settings, KEYBOARD_PROTECTION);
  is_allowed = g_permission_get_allowed (panel->permission);

  gtk_widget_set_sensitive (GTK_WIDGET (panel->devices_box), protection && is_allowed);
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
  gboolean protection;
  gboolean is_allowed;

  protection = g_settings_get_boolean (panel->privacy_settings, KEYBOARD_PROTECTION);
  is_allowed = g_permission_get_allowed (permission);

  gtk_widget_set_sensitive (GTK_WIDGET (panel->devices_box), protection && is_allowed);
  gtk_widget_set_sensitive (GTK_WIDGET (panel->keyboard_protection_switch), is_allowed);
}

static void
cc_usb_panel_dispose (GObject *object)
{
  G_OBJECT_CLASS (cc_usb_panel_parent_class)->dispose (object);
}

static void
cc_usb_panel_finalize (GObject *object)
{
  CcUsbPanel *self = CC_USB_PANEL (object);

  g_clear_pointer (&self->devices, g_hash_table_unref);
  g_clear_object (&self->permission);
  g_clear_object (&self->privacy_settings);

  G_OBJECT_CLASS (cc_usb_panel_parent_class)->finalize (object);
}

static void
cc_usb_panel_init (CcUsbPanel *self)
{
  const gchar *subsystems[] = {"input", NULL};
  g_autoptr(GError) error = NULL;

  g_resources_register (cc_usb_get_resource ());

  gtk_widget_init_template (GTK_WIDGET (self));

  self->privacy_settings = g_settings_new ("org.gnome.desktop.privacy");
  g_settings_bind (self->privacy_settings, KEYBOARD_PROTECTION,
                   self->keyboard_protection_switch, "active",
                   G_SETTINGS_BIND_DEFAULT);

  g_signal_connect (self->privacy_settings, "changed",
                    G_CALLBACK (on_keyboard_settings_changed), self);

  self->devices = g_hash_table_new_full (g_str_hash, g_str_equal,
                                         g_free, NULL);

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

  gtk_stack_set_visible_child_name (self->devices_stack, "no-keyboards");
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
  gtk_widget_class_bind_template_child (widget_class, CcUsbPanel, devices_stack);
  gtk_widget_class_bind_template_child (widget_class, CcUsbPanel, keyboard_protection_switch);

  gtk_widget_class_bind_template_callback (widget_class, on_device_entry_row_activated_cb);

  object_class->dispose = cc_usb_panel_dispose;
  object_class->constructed = cc_usb_panel_constructed;
  object_class->finalize = cc_usb_panel_finalize;
}
