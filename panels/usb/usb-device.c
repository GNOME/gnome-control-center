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

#include "config.h"

#include "usb-device.h"

#include <dirent.h>
#include <libudev.h>

/* internal methods */
static void device_set_authorization_internal (UsbDevice *dev,
                                               gboolean   authorized,
                                               gboolean   notify);

struct _UsbDevice
{
  GObject    object;

  /* device props */
  gboolean   authorized;
  char      *name;
  char      *product_id;
  char      *sysfs_path;
  char      *vendor;
};


enum {
  PROP_0,

  /* internal properties */
  PROP_OBJECT_ID,

  /* exported properties */
  PROP_AUTHORIZATION,
  PROP_NAME,
  PROP_PRODUCT_ID,
  PROP_SYSFS_PATH,
  PROP_VENDOR
};

//static GParamSpec *props[PROP_LAST] = {NULL, };

enum {
  SIGNAL_AUTHORIZATION_CHANGED,
  SIGNAL_LAST
};

static guint signals[SIGNAL_LAST] = {0};

G_DEFINE_TYPE (UsbDevice,
               usb_device,
               G_TYPE_OBJECT)

static void
usb_device_finalize (GObject *object)
{
  UsbDevice *dev = USB_DEVICE (object);

  g_free (dev->name);
  g_free (dev->product_id);
  g_free (dev->sysfs_path);
  g_free (dev->vendor);
}

static void
usb_device_init (UsbDevice *dev)
{
}

static void
usb_device_get_property (GObject    *object,
                         guint       prop_id,
                         GValue     *value,
                         GParamSpec *pspec)
{
  UsbDevice *dev = USB_DEVICE (object);

  switch (prop_id)
    {
      case PROP_AUTHORIZATION:
        g_value_set_boolean (value, dev->authorized);
        break;

      case PROP_NAME:
        g_value_set_string (value, dev->name);
        break;

      case PROP_PRODUCT_ID:
        g_value_set_string (value, dev->product_id);
        break;

      case PROP_SYSFS_PATH:
        g_value_set_string (value, dev->sysfs_path);
        break;

      case PROP_VENDOR:
        g_value_set_string (value, dev->vendor);
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
usb_device_set_property (GObject      *object,
                         guint         prop_id,
                         const GValue *value,
                         GParamSpec   *pspec)
{
  UsbDevice *dev = USB_DEVICE (object);

  switch (prop_id)
    {
      case PROP_AUTHORIZATION:
      device_set_authorization_internal (dev,
                                         g_value_get_boolean (value),
                                         FALSE);
      break;

      case PROP_NAME:
        g_clear_pointer (&dev->name, g_free);
        dev->name = g_value_dup_string (value);
        break;

      case PROP_PRODUCT_ID:
        g_return_if_fail (dev->product_id == NULL);
        dev->product_id = g_value_dup_string (value);
        break;

      case PROP_SYSFS_PATH:
        g_clear_pointer (&dev->sysfs_path, g_free);
        dev->sysfs_path = g_value_dup_string (value);
        break;

      case PROP_VENDOR:
        g_clear_pointer (&dev->vendor, g_free);
        dev->vendor = g_value_dup_string (value);
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}


static void
usb_device_class_init (UsbDeviceClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->finalize = usb_device_finalize;

  gobject_class->get_property = usb_device_get_property;
  gobject_class->set_property = usb_device_set_property;

  g_object_class_install_property (gobject_class,
                                   PROP_AUTHORIZATION,
                                   g_param_spec_boolean ("authorization",
                                                         "authorization",
                                                         NULL,
                                                         FALSE,
                                                         G_PARAM_CONSTRUCT_ONLY |
                                                         G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class,
                                   PROP_NAME,
                                   g_param_spec_string ("name",
                                                        "name",
                                                        NULL,
                                                        NULL,
                                                        G_PARAM_CONSTRUCT_ONLY |
                                                        G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class,
                                   PROP_PRODUCT_ID,
                                   g_param_spec_string ("product_id",
                                                        "product_id",
                                                        NULL,
                                                        NULL,
                                                        G_PARAM_CONSTRUCT_ONLY |
                                                        G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class,
                                   PROP_SYSFS_PATH,
                                   g_param_spec_string ("sysfs_path",
                                                        "sysfs_path",
                                                        NULL,
                                                        NULL,
                                                        G_PARAM_CONSTRUCT_ONLY |
                                                        G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class,
                                   PROP_VENDOR,
                                   g_param_spec_string ("vendor",
                                                        "vendor",
                                                        NULL,
                                                        NULL,
                                                        G_PARAM_CONSTRUCT_ONLY |
                                                        G_PARAM_READWRITE));
}

/* internal methods */
static void
device_set_authorization_internal (UsbDevice *dev,
                                   gboolean   authorized,
                                   gboolean   notify)
{
  gboolean before;

  before = dev->authorized;
  if (before == authorized)
    return;

  dev->authorized = authorized;

  g_signal_emit (dev, signals[SIGNAL_AUTHORIZATION_CHANGED], 0, before);
}

/* public methods */

UsbDevice *
usb_device_new (gboolean    authorized,
                const char *name,
                const char *product_id,
                const char *sysfs_path,
                const char *vendor)
{
  UsbDevice *dev;

  dev = g_object_new (USB_TYPE_DEVICE,
                      "authorization", authorized,
                      "name", name,
                      "product_id", product_id,
                      "sysfs_path", sysfs_path,
                      "vendor", vendor,
                      NULL);
  return dev;
}

const char *
usb_device_get_name (UsbDevice *dev)
{
  g_return_val_if_fail (USB_IS_DEVICE (dev), NULL);

  return dev->name;
}

const char *
usb_device_get_product_id (UsbDevice *dev)
{
  g_return_val_if_fail (USB_IS_DEVICE (dev), NULL);

  return dev->product_id;
}

gboolean
usb_device_get_authorization (UsbDevice *dev)
{
  g_return_val_if_fail (dev != NULL, FALSE);

  return dev->authorized;
}

const char *
usb_device_get_sysfs_path (UsbDevice *dev)
{
  g_return_val_if_fail (USB_IS_DEVICE (dev), NULL);

  return dev->sysfs_path;
}

const char *
usb_device_get_vendor (UsbDevice *dev)
{
  g_return_val_if_fail (USB_IS_DEVICE (dev), NULL);

  return dev->vendor;
}

gboolean
usb_device_set_authorization (UsbDevice *dev, gboolean authorization)
{
  char *command;
  char *auth_str;
  gint status;
  g_autoptr(GError) error = NULL;

  g_return_val_if_fail (dev != NULL, FALSE);

  if (authorization)
    auth_str = "authorized";
  else
    auth_str = "not_authorized";

  command = g_strdup_printf ("pkexec %s/cc-usb-device-helper %s %s \"%s\" %s %s \"%s\" %s",
                             LIBEXECDIR,
                             "/tmp/usb",
                             "set_auth",
                             dev->name,
                             dev->vendor,
                             dev->product_id,
                             dev->sysfs_path,
                             auth_str);

  g_spawn_command_line_sync (command,
                             NULL, NULL,
                             &status,
                             &error);

  if (error != NULL) {
    g_warning ("An error occurred launching the USB helper: %s", error->message);
    return FALSE;
  }

  dev->authorized = authorization;

  return TRUE;
}
