#include <config.h>
#include "capplet-widget.h"
#include <libgnomeui/gnome-window-icon.h>
#include "da.h"

#define THEME_SWITCHER_VERSION "0.1"

int
main(int argc, char **argv)
{
  GtkWidget *w;
  gint child_pid;

  bindtextdomain (PACKAGE, GNOMELOCALEDIR);
  textdomain (PACKAGE);

  set_tmp_rc();
  child_pid = do_demo(argc, argv);
  switch (gnome_capplet_init ("theme-switcher-capplet",
			      THEME_SWITCHER_VERSION, argc, argv, NULL, 0, NULL)) {
  case -1:
	  exit (1);
  case 1:
	  return 0;
  }
  gnome_window_icon_set_default_from_file (GNOME_ICONDIR"/gnome-ccthemes.png");
  w = make_main();
  gtk_widget_show_all(w);
  send_socket();
  
  gtk_main();
  /* Pause here until our child exits and the socket can be safely
   * destroyed
   */
  if (child_pid > 0)
    waitpid(child_pid, NULL, 0);
  
  return 0;
}
