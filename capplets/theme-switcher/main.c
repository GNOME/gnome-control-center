
#ifdef HAVE_CONFIG_H
#   include <config.h>
#endif

#include "capplet-widget.h"
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
  switch (gnome_capplet_init ("gtk-theme-selector",
			      THEME_SWITCHER_VERSION, argc, argv, NULL, 0, NULL)) {
  case -1:
	  exit (1);
  case 1:
	  return 0;
  }  
  w = make_main();
  gtk_widget_show_all(w);
  send_socket();
  
  gtk_main();
  /* This doesn't work any more -- why? */
#if 0
  /* Pause here until our child exits and the socket can be safely
   * destroyed
   */
  if (child_pid > 0)
    waitpid(child_pid, NULL, 0);
#endif

  return 0;
}
