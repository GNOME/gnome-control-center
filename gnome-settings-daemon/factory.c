#include <config.h>

#include "gnome-settings-daemon.h"

#include <bonobo.h>
#include <bonobo/bonobo-object.h>
#include <bonobo/bonobo-generic-factory.h>

#include <libgnome/gnome-init.h>
#include <libgnomeui/gnome-ui-init.h>
#include <libgnomeui/gnome-client.h>

static BonoboObject *services_server = NULL;

int main (int argc, char *argv [])
{
  GnomeClient *session;
  Bonobo_RegistrationResult ret;
  gchar *restart_argv[] = { "gnome-settings-daemon", *argv, 0 };

  gnome_program_init ("gnome-settings-daemon", VERSION, LIBGNOMEUI_MODULE,
		      argc, argv, NULL);
  
  if (!bonobo_init (&argc, argv)) {
    g_error (_("Could not initialize Bonobo"));
  }

  gconf_init (argc, argv, NULL); /* exits w/ message on failure */ 

  /* start the settings daemon */
  services_server = BONOBO_OBJECT (gnome_settings_daemon_new ());

  ret = bonobo_activation_active_server_register ("OAFIID:GNOME_SettingsDaemon",
						  BONOBO_OBJREF (services_server));
  if (ret != Bonobo_ACTIVATION_REG_SUCCESS) {
    g_warning ("Encountered problems registering the settings daemon with bonobo-activation. "
	       "Clients may not detect that the settings daemon is already running.");
  }

  session = gnome_master_client ();
  gnome_client_set_restart_command (session, 2, restart_argv);
  gnome_client_set_restart_style (session, GNOME_RESTART_IMMEDIATELY);
  gnome_client_set_priority      (session, 5);

  gtk_main();

  return -1;
}                                                                             
