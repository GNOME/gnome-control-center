/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright 2013 Red Hat, Inc,
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * Written by:
 *   Jasper St. Pierre <jstpierre@mecheye.net>
 *   Bogdan Ciobanu <bgdn.ciobanu@gmail.com>
 */

#include "online-avatars.h"

#include <gio/gio.h>

static GMainLoop *loop;

static void
got_gravatar (GObject      *source_object,
              GAsyncResult *result,
              gpointer      user_data)
{
    GError *error = NULL;
    GBytes *bytes = get_gravatar_from_email_finish (result, &error);

    if (error) {
        g_warning ("Failed to fetch gravatar: %s\n", error->message);
        g_error_free (error);
    } else {
        g_file_set_contents ("out.png",
                             g_bytes_get_data (bytes, NULL),
                             g_bytes_get_size (bytes),
                             NULL);
    }

    g_main_loop_quit (loop);
}

int
main (int argc, char *argv[])
{
    get_gravatar_from_email ("jstpierre@mecheye.net", NULL,
                             got_gravatar, NULL);

    loop = g_main_loop_new (NULL, FALSE);

    g_main_loop_run (loop);
    g_main_loop_unref (loop);

    return 0;
}
