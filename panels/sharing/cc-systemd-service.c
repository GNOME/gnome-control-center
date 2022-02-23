/*
 * Copyright (C) 2013 Intel, Inc
 * Copyright (C) 2022 Red Hat, Inc
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

#include "cc-systemd-service.h"

gboolean
cc_enable_service (const char  *service,
                   GBusType     bus_type,
                   GError     **error)
{
  g_autoptr(GDBusConnection) connection = NULL;
  g_autoptr(GVariant) start_result = NULL;
  g_autoptr(GVariant) enable_result = NULL;
  const char *service_list[] = { service, NULL };

  connection = g_bus_get_sync (bus_type, NULL, error);
  if (!connection)
    {
      g_prefix_error_literal (error, "Failed connecting to D-Bus system bus: ");
      return FALSE;
    }

  start_result = g_dbus_connection_call_sync (connection,
                                              "org.freedesktop.systemd1",
                                              "/org/freedesktop/systemd1",
                                              "org.freedesktop.systemd1.Manager",
                                              "StartUnit",
                                              g_variant_new ("(ss)",
                                                             service,
                                                             "replace"),
                                              (GVariantType *) "(o)",
                                              G_DBUS_CALL_FLAGS_NONE,
                                              -1,
                                              NULL,
                                              error);

  if (!start_result)
    {
      g_prefix_error_literal (error, "Failed to start service: ");
      return FALSE;
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
                                               error);

  if (!enable_result)
    {
      g_prefix_error_literal (error, "Failed to enable service: ");
      return FALSE;
    }

  return TRUE;
}

gboolean
cc_disable_service (const char  *service,
                    GBusType     bus_type,
                    GError     **error)
{
  g_autoptr(GDBusConnection) connection = NULL;
  g_autoptr(GVariant) stop_result = NULL;
  g_autoptr(GVariant) disable_result = NULL;
  const char *service_list[] = { service, NULL };

  connection = g_bus_get_sync (bus_type, NULL, error);
  if (!connection)
    {
      g_prefix_error_literal (error, "Failed connecting to D-Bus system bus: ");
      return FALSE;
    }

  stop_result = g_dbus_connection_call_sync (connection,
                                             "org.freedesktop.systemd1",
                                             "/org/freedesktop/systemd1",
                                             "org.freedesktop.systemd1.Manager",
                                             "StopUnit",
                                             g_variant_new ("(ss)", service, "replace"),
                                             (GVariantType *) "(o)",
                                             G_DBUS_CALL_FLAGS_NONE,
                                             -1,
                                             NULL,
                                             error);
  if (!stop_result)
    {
      g_prefix_error_literal (error, "Failed to stop service: ");
      return FALSE;
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
                                                error);

  if (!stop_result)
    {
      g_prefix_error_literal (error, "Failed to disable service: ");
      return FALSE;
    }

  return TRUE;
}
