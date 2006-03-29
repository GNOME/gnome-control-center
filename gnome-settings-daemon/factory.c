#include <config.h>

#include "gnome-settings-daemon.h"

#include <string.h>
#include <bonobo.h>
#include <bonobo/bonobo-object.h>
#include <bonobo/bonobo-generic-factory.h>

#include <gconf/gconf.h>

#include <libgnome/gnome-init.h>
#include <libgnomeui/gnome-ui-init.h>
#include <libgnomeui/gnome-client.h>

static BonoboObject *services_server = NULL;
GConfClient *conf_client = NULL;

static void
register_server (BonoboObject *server)
{
  Bonobo_RegistrationResult ret;
  GSList *reg_env;
  char *display;
  char *p;

  display = gdk_display_get_name (gdk_display_get_default ());

  reg_env = bonobo_activation_registration_env_set (NULL, "DISPLAY", display);

  ret = bonobo_activation_register_active_server ("OAFIID:GNOME_SettingsDaemon",
						  BONOBO_OBJREF (server),
						  reg_env);
  if (ret != Bonobo_ACTIVATION_REG_SUCCESS) {
    g_warning ("Encountered problems registering the settings daemon with bonobo-activation. "
	       "Clients may not detect that the settings daemon is already running.");
  }

  bonobo_activation_registration_env_free (reg_env);
}

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
  
  if (!bonobo_init (&argc, argv)) {
    g_error (_("Could not initialize Bonobo"));
  }

  gconf_init (argc, argv, NULL); /* exits w/ message on failure */ 

  /* start the settings daemon */
  services_server = BONOBO_OBJECT (gnome_settings_daemon_new ());

  register_server (services_server);

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
