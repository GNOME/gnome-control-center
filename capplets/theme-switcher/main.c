#include "da.h"

int
main(int argc, char **argv)
{
  GtkWidget *w;

  set_tmp_rc();
  do_demo(argc, argv);
  gnome_capplet_init ("theme-switcher-capplet", NULL, argc, argv, 0, NULL);
  
  w = make_main();
  gtk_widget_show_all(w);
  send_socket();
  
  gtk_main();
  return 0;
}
