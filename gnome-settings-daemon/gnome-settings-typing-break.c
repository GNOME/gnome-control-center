#include "gnome-settings-daemon.h"
#include "gnome-settings-typing-break.h"
#include <unistd.h>
#include <sys/types.h>
#include <signal.h>
#include "reaper.h"
#include <string.h>

pid_t typing_monitor_pid = 0;
guint typing_monitor_idle_id = 0;

static gboolean
typing_break_timeout (gpointer data)
{
  if (typing_monitor_pid > 0)
    kill (typing_monitor_pid, SIGKILL);

  typing_monitor_idle_id = 0;

  return FALSE;
}

static void
setup_typing_break (gboolean enabled)
{
  if (enabled)
    {
      if (typing_monitor_idle_id != 0)
	g_source_remove (typing_monitor_idle_id);
      if (typing_monitor_pid == 0)
	{
	  GError *error = NULL;
	  gchar *argv[] = { "gnome-typing-monitor", NULL };

	  if (! g_spawn_async ("/",
			       argv, NULL,
			       G_SPAWN_STDOUT_TO_DEV_NULL | G_SPAWN_STDERR_TO_DEV_NULL |
			       G_SPAWN_SEARCH_PATH | G_SPAWN_DO_NOT_REAP_CHILD,
			       NULL, NULL,
			       &typing_monitor_pid,
			       &error))
	    {
	      /* FIXME: put up a warning */
	      g_print ("failed: %s\n", error->message);
	      g_error_free (error);
	      typing_monitor_pid = 0;
	    }
	}
    }
  else
    {
      if (typing_monitor_pid != 0)
	{
	  typing_monitor_idle_id = g_timeout_add (3000, typing_break_timeout, NULL);
	}
    }
}

static void
child_exited_callback (VteReaper *reaper,
		       gint       pid,
		       gint       exit_status,
		       gpointer   user_data)
{
  if (pid == typing_monitor_pid)
    {
      typing_monitor_pid = 0;
    }
}

static void 
typing_break_callback (GConfEntry *entry)
{
  if (! strcmp (entry->key, "/desktop/gnome/typing_break/enabled"))
    {
      if (entry->value->type == GCONF_VALUE_BOOL)
	setup_typing_break (gconf_value_get_bool (entry->value));
    }
}

void
gnome_settings_typing_break_init (GConfClient *client)
{
  VteReaper *reaper;

  reaper = vte_reaper_get();
  g_signal_connect (G_OBJECT (reaper), "child_exited", child_exited_callback, NULL);
  gnome_settings_daemon_register_callback ("/desktop/gnome/typing_break", typing_break_callback);
}


void
gnome_settings_typing_break_load (GConfClient *client)
{
  if (gconf_client_get_bool (client, "/desktop/gnome/typing_break/enabled", NULL))
    setup_typing_break (TRUE);
}
