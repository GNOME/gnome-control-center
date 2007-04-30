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
#include "appearance-themes.h"
#include "theme-thumbnail.h"
#include "activate-settings-daemon.h"

/* required for gnome_program_init(): */
#include <libgnome/libgnome.h>
#include <libgnomeui/gnome-ui-init.h>
/* ---------------------------------- */

#define VERSION "0.0"

int
main (int argc, char **argv)
{
  AppearanceData *data;
  GtkWidget *w;
  GnomeProgram *program;

  /* init */
  theme_thumbnail_factory_init (argc, argv);
  gtk_init (&argc, &argv);
  gnome_vfs_init ();
  activate_settings_daemon ();

  /* set up the data */
  data = g_new0 (AppearanceData, 1);
  data->xml = glade_xml_new (GLADEDIR "appearance.glade", NULL, NULL);
  data->argc = argc;
  data->argv = argv;

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

  w = WID ("appearance_window");
  gtk_widget_show_all (w);
  g_signal_connect (G_OBJECT (w), "delete-event", (GCallback) gtk_main_quit, NULL);


  gtk_main ();



  /* free stuff */
  g_free (data);

  return 0;
}
