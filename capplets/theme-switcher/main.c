#include <config.h>
#include "capplet-widget.h"
#include "da.h"

#define THEME_SWITCHER_VERSION "0.1"

int
main(int argc, char **argv)
{
  GtkWidget *w;

  bindtextdomain (PACKAGE, GNOMELOCALEDIR);
  textdomain (PACKAGE);

  set_tmp_rc();
  do_demo(argc, argv);
  switch (gnome_capplet_init ("theme-switcher-capplet",
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
  return 0;
}
