/*
 * Copyright (C) 2013 Intel, Inc
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Author: Thomas Wood <thomas.wood@intel.com>
 *
 */

#include <gio/gio.h>

#ifndef SSHD_SERVICE
#define SSHD_SERVICE "sshd.service"
#endif

static const gchar *service_list[] = { SSHD_SERVICE, NULL };

static gint
enable_ssh_service ()
{
  g_autoptr(GDBusConnection) connection = NULL;
  g_autoptr(GError) error = NULL;
  g_autoptr(GVariant) start_result = NULL;
  g_autoptr(GVariant) enable_result = NULL;

  connection = g_bus_get_sync (G_BUS_TYPE_SYSTEM, NULL, &error);
  if (!connection)
    {
      g_critical ("Error connecting to D-Bus system bus: %s", error->message);
      return 1;
    }

  start_result = g_dbus_connection_call_sync (connection,
                                              "org.freedesktop.systemd1",
                                              "/org/freedesktop/systemd1",
                                              "org.freedesktop.systemd1.Manager",
                                              "StartUnit",
                                              g_variant_new ("(ss)",
                                                             SSHD_SERVICE,
                                                             "replace"),
                                              (GVariantType *) "(o)",
                                              G_DBUS_CALL_FLAGS_NONE,
                                              -1,
                                              NULL,
                                              &error);

  if (!start_result)
    {
      g_critical ("Error starting " SSHD_SERVICE ": %s", error->message);
      return 1;
    }

  enable_result = g_dbus_connection_call_sync (connection,
                                               "org.freedesktop.systemd1",
                                               "/org/freedesktop/systemd1",
                                               "org.freedesktop.systemd1.Manager",
                                               "EnableUnitFiles",
                                               g_variant_new ("(^asbb)",
                                                              service_list,
                                                              FALSE, FALSE),
                                               (GVariantType *) "(ba(sss))",
                                               G_DBUS_CALL_FLAGS_NONE,
                                               -1,
                                               NULL,
                                               &error);

  if (!enable_result)
    {
      g_critical ("Error enabling " SSHD_SERVICE ": %s", error->message);
      return 1;
    }

  return 0;
}

static gint
disable_ssh_service ()
{
  g_autoptr(GDBusConnection) connection = NULL;
  g_autoptr(GError) error = NULL;
  g_autoptr(GVariant) stop_result = NULL;
  g_autoptr(GVariant) disable_result = NULL;

  connection = g_bus_get_sync (G_BUS_TYPE_SYSTEM, NULL, &error);
  if (!connection)
    {
      g_critical ("Error connecting to D-Bus system bus: %s", error->message);
      return 1;
    }

  stop_result = g_dbus_connection_call_sync (connection,
                                             "org.freedesktop.systemd1",
                                             "/org/freedesktop/systemd1",
                                             "org.freedesktop.systemd1.Manager",
                                             "StopUnit",
                                             g_variant_new ("(ss)", SSHD_SERVICE, "replace"),
                                             (GVariantType *) "(o)",
                                             G_DBUS_CALL_FLAGS_NONE,
                                             -1,
                                             NULL,
                                             &error);
  if (!stop_result)
    {
      g_critical ("Error stopping " SSHD_SERVICE ": %s", error->message);
      return 1;
    }

  disable_result = g_dbus_connection_call_sync (connection,
                                                "org.freedesktop.systemd1",
                                                "/org/freedesktop/systemd1",
                                                "org.freedesktop.systemd1.Manager",
                                                "DisableUnitFiles",
                                                g_variant_new ("(^asb)", service_list, FALSE,
                                                               FALSE),
                                                (GVariantType *) "(a(sss))",
                                                G_DBUS_CALL_FLAGS_NONE,
                                                -1,
                                                NULL,
                                                &error);

  if (!stop_result)
    {
      g_critical ("Error disabling " SSHD_SERVICE ": %s", error->message);
      return 1;
    }

  return 0;
}

int
main (int    argc,
      char **argv)
{
  if (argc < 2)
    return 1;

  if (argv[1] == NULL)
    return 1;

  if (g_str_equal (argv[1], "enable"))
    return enable_ssh_service ();
  else if (g_str_equal (argv[1], "disable"))
    return disable_ssh_service ();

  return 1;
}
