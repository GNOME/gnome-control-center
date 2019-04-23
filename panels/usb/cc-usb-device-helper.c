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

#include <stdio.h>
#include <gio/gio.h>

#include <usb-store.h>

static int
get_auth (char *path,
          char *vendor,
          char *product)
{
  UsbStore *store;
  UsbDevice *device = NULL;
  g_autoptr(GError) error = NULL;
  gboolean authorized;

  store = usb_store_new (path);
  device = usb_store_get_device (store, vendor, product, &error);
  if (device == NULL)
    return 1;
  authorized = usb_device_get_authorization (device);

  if (authorized)
    printf ("GNOME_AUTHORIZED=1");

  return 0;
}

static gboolean
set_auth (char    *path,
          char    *name,
          char    *vendor,
          char    *product,
          char    *sysfs_path,
          gboolean authorized)
{
  UsbStore *store;
  UsbDevice *device = NULL;
  g_autoptr(GError) error = NULL;
  char *command;
  char *uevent_path;
  gint status;

  store = usb_store_new (path);
  device = usb_device_new (authorized, name, product, sysfs_path, vendor);

  if (authorized)
    usb_store_put_device (store, device, &error);
  else
    usb_store_del_device (store, device, &error);

  if (error != NULL) {
    g_warning ("Error storing device properties: %s", error->message);
    return FALSE;
  }

  uevent_path = g_strconcat (sysfs_path, "/uevent", NULL);

  /* To reload the udev rules we echo "change" into the "uevent" sysfs attribute
   * of the device. The same thing that "udevadm trigger" does. */
  command = g_strdup_printf ("/bin/sh -c 'echo change | /usr/bin/tee %s'", uevent_path);
  g_spawn_command_line_sync (command,
                             NULL, NULL,
                             &status,
                             &error);
  /* If we succeeded at storing devices properties we return TRUE, even if we fail
   * at reloading these properties. If it happens, the user just need to
   * unplug and replug the device. */
  if (error != NULL)
    g_warning ("Error reloading device properties: %s", error->message);

  return TRUE;
}

int
main (int    argc,
      char **argv)
{
  if (argc < 3)
    return 1;

  if (g_str_equal (argv[2], "get_auth")) {
    if (argc < 5)
      return 1;
    return get_auth (argv[1], argv[3], argv[4]);
  }

  if (g_str_equal (argv[2], "set_auth")) {
    gboolean authorized;
    if (argc < 8)
      return 1;

    if (g_str_equal (argv[7], "authorized"))
      authorized = TRUE;
    else if (g_str_equal (argv[7], "not_authorized"))
      authorized = FALSE;
    else
      return 1;

    return set_auth (argv[1], argv[3], argv[4], argv[5], argv[6], authorized);
  }

  return 1;
}
