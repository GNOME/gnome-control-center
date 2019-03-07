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

static int
get_auth (char *vendor,
          char *product)
{
  //TODO check the db
  if (g_str_equal (vendor, "2516"))
    printf ("GNOME_AUTHORIZED_AUTO=1");
  return 0;
}

int
main (int    argc,
      char **argv)
{
  if (argc < 4)
    return 1;

  if (argv[1] == NULL || argv[2] == NULL || argv[3] == NULL)
    return 1;

  if (g_str_equal (argv[1], "auth"))
    return get_auth (argv[2], argv[3]);

  return 1;
}
