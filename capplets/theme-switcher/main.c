
#ifdef HAVE_CONFIG_H
#   include <config.h>
#endif

#include "da.h"

#define THEME_SWITCHER_VERSION "0.1"

int
main(int argc, char **argv)
{
  GtkWidget *w;

  bindtextdomain (PACKAGE, GNOMELOCALEDIR);
  bind_textdomain_codeset (PACKAGE, "UTF-8");
  textdomain (PACKAGE);

  set_tmp_rc();

  gnome_program_init ("gtk-theme-selector", THEME_SWITCHER_VERSION,
		      LIBGNOMEUI_MODULE, argc, argv, NULL);
  
  w = make_main();
  gtk_widget_show_all(w);
  
  gtk_main();

  return 0;
}
