/*
 * Copyright (C) 2007 The GNOME Foundation
 * Written by Thomas Wood <thos@gnome.org>
 * All Rights Reserved
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
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "appearance.h"
#include "appearance-desktop.h"
#include "appearance-themes.h"
#include "appearance-ui.h"
#include "theme-thumbnail.h"
#include "activate-settings-daemon.h"

/* required for gnome_program_init(): */
#include <libgnome/libgnome.h>
#include <libgnomeui/gnome-ui-init.h>
/* ---------------------------------- */

static AppearanceData *
init_appearance_data (int argc, char **argv)
{
  AppearanceData *data = NULL;
  gchar *gladefile;
  GladeXML *ui;

  g_thread_init (NULL);
  theme_thumbnail_factory_init (argc, argv);
  gtk_init (&argc, &argv);
  gnome_vfs_init ();
  activate_settings_daemon ();

  /* set up the data */
  gladefile = g_build_filename (GNOMECC_GLADE_DIR, "appearance.glade", NULL);
  ui = glade_xml_new (gladefile, NULL, NULL);
  g_free (gladefile);

  if (ui) {
    data = g_new (AppearanceData, 1);
    data->client = gconf_client_get_default ();
    data->xml = ui;
    data->argc = argc;
    data->argv = argv;
  }

  return data;
}

int
main (int argc, char **argv)
{
  AppearanceData *data;
  GtkWidget *w;
  GnomeProgram *program;

  /* init */
  data = init_appearance_data (argc, argv);
  if (!data)
    return 1;

  /* this appears to be required for gnome_wm_manager_init (), which is called
   * inside gnome_meta_theme_set ();
   * TODO: try to remove if possible
   */
  program = gnome_program_init ("appearance", VERSION,
        LIBGNOMEUI_MODULE, argc, argv,
        GNOME_PARAM_APP_DATADIR, GNOMECC_DATA_DIR,
        NULL);

  /* init tabs */
  themes_init (data);
  desktop_init (data);
  ui_init (data);
  /*
   * fonts_init (data);
   * desktop_init (data);
   * etc...
   */

  /* prepare the main window */
  w = WID ("appearance_window");
  gtk_widget_show_all (w);
  g_signal_connect (G_OBJECT (w), "delete-event", (GCallback) gtk_main_quit, NULL);

  /* start the mainloop */
  gtk_main ();

  /* free stuff */
  g_object_unref (data->client);
  g_object_unref (data->xml);
  g_free (data);
  g_object_unref (program);

  return 0;
}
