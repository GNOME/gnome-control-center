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

static void
application_prepare_action_cb (GApplication *application, GVariant *arguments,
                               GVariant *platform_data, GnomeControlCenter *shell)
{
  GVariantIter iter;
  GVariant *temp = NULL;

  gnome_control_center_present (shell);

  /* we only care about the first argv */
  g_variant_iter_init (&iter, arguments);
  temp = g_variant_iter_next_value (&iter);
  if (temp != NULL)
    {
      GError *err = NULL;
      const gchar *id = g_variant_get_bytestring (temp);
      if (!cc_shell_set_active_panel_from_id (CC_SHELL (shell), id, &err))
        {
          if (err)
            {
              g_warning ("Could not load setting panel \"%s\": %s", id,
                         err->message);
              g_error_free (err);
            }
        }
    }
}

int
main (int argc, char **argv)
{
  GnomeControlCenter *shell;
  GtkApplication *application;

  bindtextdomain (GETTEXT_PACKAGE, GNOMELOCALEDIR);
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
  textdomain (GETTEXT_PACKAGE);


  g_thread_init (NULL);
  gtk_init (&argc, &argv);

  shell = gnome_control_center_new ();

  /* enforce single instance of this application */
  application = gtk_application_new ("org.gnome.ControlCenter", &argc, &argv);
  g_signal_connect (application, "prepare-activation",
                    G_CALLBACK (application_prepare_action_cb), shell);

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

  gtk_application_run (application);

  g_object_unref (shell);
  g_object_unref (application);

  return 0;
}
