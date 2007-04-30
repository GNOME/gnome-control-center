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
