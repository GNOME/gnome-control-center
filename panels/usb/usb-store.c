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

#include "config.h"

#include "usb-store.h"
#include "usb-io.h"

#include <string.h>

struct _UsbStore
{
  GObject object;

  GFile  *root;
  GFile  *domains;
  GFile  *devices;
};


enum {
  PROP_STORE_0,

  PROP_ROOT,

  PROP_STORE_LAST
};

static GParamSpec *store_props[PROP_STORE_LAST] = { NULL, };

G_DEFINE_TYPE (UsbStore,
               usb_store,
               G_TYPE_OBJECT)


static void
usb_store_finalize (GObject *object)
{
  UsbStore *store = USB_STORE (object);

  g_clear_object (&store->root);
  g_clear_object (&store->domains);
  g_clear_object (&store->devices);

  G_OBJECT_CLASS (usb_store_parent_class)->finalize (object);
}

static void
usb_store_init (UsbStore *store)
{
}

static void
usb_store_get_property (GObject    *object,
                        guint       prop_id,
                        GValue     *value,
                        GParamSpec *pspec)
{
  UsbStore *store = USB_STORE (object);

  switch (prop_id)
    {
    case PROP_ROOT:
      g_value_set_object (value, store->root);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
usb_store_set_property (GObject      *object,
                        guint         prop_id,
                        const GValue *value,
                        GParamSpec   *pspec)
{
  UsbStore *store = USB_STORE (object);

  switch (prop_id)
    {
    case PROP_ROOT:
      store->root = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
usb_store_constructed (GObject *obj)
{
  UsbStore *store = USB_STORE (obj);

  store->devices = g_file_get_child (store->root, "devices");
  store->domains = g_file_get_child (store->root, "domains");
}

static void
usb_store_class_init (UsbStoreClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->finalize = usb_store_finalize;

  gobject_class->constructed  = usb_store_constructed;
  gobject_class->get_property = usb_store_get_property;
  gobject_class->set_property = usb_store_set_property;

  store_props[PROP_ROOT] =
    g_param_spec_object ("root",
                         NULL, NULL,
                         G_TYPE_FILE,
                         G_PARAM_READWRITE      |
                         G_PARAM_CONSTRUCT_ONLY |
                         G_PARAM_STATIC_NAME);

  g_object_class_install_properties (gobject_class,
                                     PROP_STORE_LAST,
                                     store_props);
}

#define DOMAIN_GROUP "domain"
#define DEVICE_GROUP "device"
#define USER_GROUP "user"

/* public methods */

UsbStore *
usb_store_new (const char *path)
{
  g_autoptr(GFile) root = NULL;
  UsbStore *store;

  root = g_file_new_for_path (path);
  store = g_object_new (USB_TYPE_STORE,
                        "root", root,
                        NULL);

  return store;
}

gboolean
usb_store_put_device (UsbStore  *store,
                      UsbDevice *device,
                      GError   **error)
{
  g_autoptr(GFile) entry = NULL;
  g_autoptr(GKeyFile) kf = NULL;
  g_autoptr(GError) err = NULL;
  g_autofree char *data = NULL;
  g_autofree char *path = NULL;
  g_autoptr(GFile) p = NULL;
  g_autofree const char *uid = NULL;
  const gchar *vendor;
  const gchar *product;
  gboolean ok;
  gsize len;

  g_return_val_if_fail (USB_IS_STORE (store), FALSE);
  g_return_val_if_fail (USB_IS_DEVICE (device), FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  vendor = usb_device_get_vendor (device);
  product = usb_device_get_name (device);
  uid = g_strdup_printf ("%s_%s", vendor, product);

  entry = g_file_get_child (store->devices, uid);

  p = g_file_get_parent (entry);
  ok = g_file_make_directory_with_parents (p, NULL, &err);
  if (!ok)
    g_debug ("Can't create parent directories, probably they are already there");

  kf = g_key_file_new ();

  path = g_file_get_path (entry);
  g_debug ("Using the path: %s", path);

  ok = g_key_file_load_from_file (kf,
                                  path,
                                  G_KEY_FILE_KEEP_COMMENTS,
                                  &err);

  if (!ok)
    g_debug ("Error to load from file: %s", err->message);

  g_key_file_set_boolean (kf, DEVICE_GROUP, "authorization", usb_device_get_authorization (device));
  g_key_file_set_string (kf, DEVICE_GROUP, "name", usb_device_get_name (device));
  g_key_file_set_string (kf, DEVICE_GROUP, "vendor", usb_device_get_vendor (device));
  g_key_file_set_string (kf, DEVICE_GROUP, "product_id", usb_device_get_product_id (device));


  data = g_key_file_to_data (kf, &len, error);

  if (!data)
    return FALSE;

  ok = g_file_replace_contents (entry,
                                data, len,
                                NULL, FALSE,
                                0,
                                NULL,
                                NULL, error);

  if (!ok)
    return FALSE;

  g_object_set (device,
                "store", store,
                NULL);

  return TRUE;
}

UsbDevice *
usb_store_get_device (UsbStore    *store,
                      const gchar *vendor,
                      const gchar *product_id,
                      GError     **error)
{
  g_autoptr(GKeyFile) kf = NULL;
  g_autoptr(GFile) db = NULL;
  g_autofree char *name = NULL;
  g_autofree char *data  = NULL;
  g_autofree const char *uid = NULL;
  gboolean authorization;
  gboolean ok;
  gsize len;

  g_return_val_if_fail (USB_IS_STORE (store), NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  uid = g_strdup_printf ("%s_%s", vendor, product_id);

  db = g_file_get_child (store->devices, uid);
  ok = g_file_load_contents (db, NULL,
                             &data, &len,
                             NULL,
                             error);

  if (!ok)
    return NULL;

  kf = g_key_file_new ();
  ok = g_key_file_load_from_data (kf, data, len, G_KEY_FILE_NONE, error);

  if (!ok)
    return NULL;

  name = g_key_file_get_string (kf, DEVICE_GROUP, "name", NULL);
  authorization = g_key_file_get_boolean (kf, USER_GROUP, "authorization", FALSE);

  if (name == NULL || vendor == NULL)
    {
      g_warning ("invalid device entry in store");
      return NULL;
    }

  return g_object_new (USB_TYPE_DEVICE,
                       "authorization", authorization,
                       "name", name,
                       "product_id", product_id,
                       "vendor", vendor,
                       NULL);
}

gboolean
usb_store_del_device (UsbStore  *store,
                      UsbDevice *device,
                      GError   **error)
{
  g_autoptr(GFile) devpath = NULL;
  g_autofree const char *uid = NULL;
  const gchar *vendor;
  const gchar *product_id;
  gboolean ok;

  g_return_val_if_fail (USB_IS_STORE (store), FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  vendor = usb_device_get_vendor (device);
  product_id = usb_device_get_name (device);
  uid = g_strdup_printf ("%s_%s", vendor, product_id);

  devpath = g_file_get_child (store->devices, uid);
  g_debug ("%s", g_file_get_path (devpath));
  ok = g_file_delete (devpath, NULL, error);

  return ok;
}

