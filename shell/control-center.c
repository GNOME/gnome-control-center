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

#include <glib/gi18n.h>

#include "gnome-control-center.h"

#include <gtk/gtk.h>
#include <string.h>


static int
application_command_line_cb (GApplication  *application,
			     GApplicationCommandLine  *command_line,
			     GnomeControlCenter      *shell)
{
  int argc;
  char **argv;
  int retval = 0;

  argv = g_application_command_line_get_arguments (command_line, &argc);
  if (argc == 2)
    {
      gchar *start_id;
      GError *err = NULL;

      start_id = argv[1];

      if (!cc_shell_set_active_panel_from_id (CC_SHELL (shell), start_id, &err))
        {
          g_warning ("Could not load setting panel \"%s\": %s", start_id,
                     (err) ? err->message : "Unknown error");
          retval = 1;
          if (err)
            {
              g_error_free (err);
              err = NULL;
            }
        }
    }
  g_strfreev (argv);
  return retval;
}

static void
application_startup_cb (GApplication       *application,
			GnomeControlCenter *shell)
{
  gnome_control_center_show (shell, GTK_APPLICATION (application));
  gnome_control_center_present (shell);
}

int
main (int argc, char **argv)
{
  GnomeControlCenter *shell;
  GtkApplication *application;
  int status;

  bindtextdomain (GETTEXT_PACKAGE, GNOMELOCALEDIR);
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
  textdomain (GETTEXT_PACKAGE);


  g_thread_init (NULL);
  gtk_init (&argc, &argv);

  shell = gnome_control_center_new ();

  /* enforce single instance of this application */
  application = gtk_application_new ("org.gnome.ControlCenter", G_APPLICATION_HANDLES_COMMAND_LINE);
  g_signal_connect (application, "startup",
                    G_CALLBACK (application_startup_cb), shell);
  g_signal_connect (application, "command-line",
                    G_CALLBACK (application_command_line_cb), shell);

  status = g_application_run (G_APPLICATION (application), argc, argv);

  g_object_unref (application);

  return status;
}
