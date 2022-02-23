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

#include "cc-systemd-service.h"

#ifndef SSHD_SERVICE
#define SSHD_SERVICE "sshd.service"
#endif

int
main (int    argc,
      char **argv)
{
  if (argc < 2)
    return 1;

  if (argv[1] == NULL)
    return 1;

  if (g_str_equal (argv[1], "enable"))
    {
      g_autoptr(GError) error = NULL;

      if (!cc_enable_service (SSHD_SERVICE, G_BUS_TYPE_SYSTEM, &error))
        {
          g_critical ("Failed to enable %s: %s", SSHD_SERVICE, error->message);
          return EXIT_FAILURE;
        }
      else
        {
          return EXIT_SUCCESS;
        }
    }
  else if (g_str_equal (argv[1], "disable"))
    {
      g_autoptr(GError) error = NULL;

      if (!cc_disable_service (SSHD_SERVICE, G_BUS_TYPE_SYSTEM, &error))
        {
          g_critical ("Failed to enable %s: %s", SSHD_SERVICE, error->message);
          return EXIT_FAILURE;
        }
      else
        {
          return EXIT_SUCCESS;
        }
    }

  return 1;
}
