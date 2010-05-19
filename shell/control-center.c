/*
 * Copyright (c) 2009, 2010 Intel, Inc.
 * Copyright (c) 2010 Red Hat, Inc.
 *
 * The Control Center is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * The Control Center is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with the Control Center; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Author: Thomas Wood <thos@gnome.org>
 */

#include "config.h"

#include "gnome-control-center.h"

#include <gtk/gtk.h>
#include <string.h>


#include <unique/unique.h>


enum
{
  CC_SHELL_RAISE_COMMAND = 1
};


static UniqueResponse
message_received (UniqueApp          *app,
                  gint                command,
                  UniqueMessageData  *message_data,
                  guint               time_,
                  GnomeControlCenter *shell)
{
  const gchar *id;
  gsize len;

  gnome_control_center_present (shell);

  id = (gchar*) unique_message_data_get (message_data, &len);

  if (id)
    {
      GError *err = NULL;

      if (!cc_shell_set_active_panel_from_id (CC_SHELL (shell), id, &err))
        {
          if (err)
            {
              g_warning ("Could not load setting panel \"%s\": %s", id,
                         err->message);
              g_error_free (err);
              err = NULL;
            }
        }
    }

  return GTK_RESPONSE_OK;
}

int
main (int argc, char **argv)
{
  GnomeControlCenter *shell;
  UniqueApp *unique;

  g_thread_init (NULL);
  gtk_init (&argc, &argv);

  /* use Unique to enforce single instance of this application */
  unique = unique_app_new_with_commands ("org.gnome.ControlCenter",
                                         NULL,
                                         "raise",
                                         CC_SHELL_RAISE_COMMAND,
                                         NULL);

  /* check if the application is already running */
  if (unique_app_is_running (unique))
    {
      UniqueMessageData *data;

      if (argc == 2)
        {
          data = unique_message_data_new ();
          unique_message_data_set (data, (guchar*) argv[1],
                                   strlen(argv[1]) + 1);
        }
      else
        data = NULL;

      unique_app_send_message (unique, 1, data);

      gdk_notify_startup_complete ();
      return 0;
    }


  shell = gnome_control_center_new ();

  g_signal_connect (unique, "message-received", G_CALLBACK (message_received),
                    shell);

  if (argc == 2)
    {
      gchar *start_id;
      GError *err = NULL;

      start_id = argv[1];

      if (!cc_shell_set_active_panel_from_id (CC_SHELL (shell), start_id, &err))
        {
          g_warning ("Could not load setting panel \"%s\": %s", start_id,
                     (err) ? err->message : "Unknown error");
          if (err)
            {
              g_error_free (err);
              err = NULL;
            }
        }
    }

  gtk_main ();

  g_object_unref (shell);

  return 0;
}
