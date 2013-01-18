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
#define SSHD_SERVICE "sshd.service"

static const gchar *service_list[] = { SSHD_SERVICE, NULL };

static gint
enable_ssh_service ()
{
  GDBusConnection *connection;
  GError *error = NULL;
  GVariant *temp_variant;

  connection = g_bus_get_sync (G_BUS_TYPE_SYSTEM, NULL, &error);
  if (!connection)
    {
      g_critical ("Error connecting to D-Bus system bus: %s", error->message);
      g_clear_error (&error);
      return 1;
    }

  temp_variant = g_dbus_connection_call_sync (connection,
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

  if (!temp_variant)
    {
      g_critical ("Error starting " SSHD_SERVICE ": %s", error->message);
      g_clear_error (&error);
      return 1;
    }

  g_variant_unref (temp_variant);

  temp_variant = g_dbus_connection_call_sync (connection,
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

  if (!temp_variant)
    {
      g_critical ("Error enabling " SSHD_SERVICE ": %s", error->message);
      g_clear_error (&error);
      return 1;
    }

  g_variant_unref (temp_variant);

  return 0;
}

static gint
disable_ssh_service ()
{
  GDBusConnection *connection;
  GError *error = NULL;
  GVariant *temp_variant;

  connection = g_bus_get_sync (G_BUS_TYPE_SYSTEM, NULL, &error);
  if (!connection)
    {
      g_critical ("Error connecting to D-Bus system bus: %s", error->message);
      g_clear_error (&error);
      return 1;
    }

  temp_variant = g_dbus_connection_call_sync (connection,
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
  if (!temp_variant)
    {
      g_critical ("Error stopping " SSHD_SERVICE ": %s", error->message);
      g_clear_error (&error);
      return 1;
    }

  g_variant_unref (temp_variant);

  temp_variant = g_dbus_connection_call_sync (connection,
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

  if (!temp_variant)
    {
      g_critical ("Error disabling " SSHD_SERVICE ": %s", error->message);
      g_clear_error (&error);
      return 1;
    }

  g_variant_unref (temp_variant);

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
