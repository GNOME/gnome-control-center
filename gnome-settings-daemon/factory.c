#include <config.h>

#include "gnome-settings-daemon.h"

#include <libintl.h>
#include <string.h>

#include <gconf/gconf.h>

#include <libgnome/gnome-init.h>
#include <libgnomeui/gnome-ui-init.h>
#include <libgnomeui/gnome-client.h>

GConfClient *conf_client = NULL;

int main (int argc, char *argv [])
{
  GnomeClient *session;
  gchar *restart_argv[] = { "gnome-settings-daemon", NULL, NULL };

  restart_argv[1] = *argv;

  bindtextdomain (GETTEXT_PACKAGE, GNOMELOCALEDIR);
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
  textdomain (GETTEXT_PACKAGE);

  gnome_program_init ("gnome-settings-daemon", VERSION,
		      LIBGNOMEUI_MODULE,
		      argc, argv,
		      GNOME_CLIENT_PARAM_SM_CONNECT, FALSE,
		      NULL);
  
  gconf_init (argc, argv, NULL); /* exits w/ message on failure */ 

  gnome_settings_daemon_new ();

  session = gnome_master_client ();
  gnome_client_set_restart_command (session, 2, restart_argv);
  gnome_client_set_restart_style (session, GNOME_RESTART_IMMEDIATELY);
  gnome_client_set_priority      (session, 5);
  g_signal_connect (session, "die",
		    G_CALLBACK (gtk_main_quit), NULL);

  gtk_main();

  /* cleanup */
  if (conf_client)
	  g_object_unref (conf_client);

  return -1;
}
