#include "gnome-settings-module.h"
#include "utils.h"
#include <unistd.h>
#include <sys/types.h>
#include <signal.h>
#include "reaper.h"
#include <string.h>

typedef struct {
	GnomeSettingsModule parent;
} GnomeSettingsModuleTypingBreak;

typedef struct {
	GnomeSettingsModuleClass parent_class;
} GnomeSettingsModuleTypingBreakClass;

static GnomeSettingsModuleRunlevel gnome_settings_module_typing_break_get_runlevel (GnomeSettingsModule *module);
static gboolean gnome_settings_module_typing_break_initialize (GnomeSettingsModule *module, GConfClient *config_client);
static gboolean gnome_settings_module_typing_break_start (GnomeSettingsModule *module);
static gboolean gnome_settings_module_typing_break_stop (GnomeSettingsModule *module);

static pid_t typing_monitor_pid = 0;
static guint typing_monitor_idle_id = 0;

static void
gnome_settings_module_typing_break_class_init (GnomeSettingsModuleTypingBreakClass *klass)
{
	GnomeSettingsModuleClass *module_class;

	module_class = (GnomeSettingsModuleClass *) klass;
	module_class->get_runlevel = gnome_settings_module_typing_break_get_runlevel;
	module_class->initialize = gnome_settings_module_typing_break_initialize;
	module_class->start = gnome_settings_module_typing_break_start;
	module_class->stop = gnome_settings_module_typing_break_stop;
}

static void
gnome_settings_module_typing_break_init (GnomeSettingsModuleTypingBreak *module)
{
}

GType
gnome_settings_module_typing_break_get_type (void)
{
	static GType module_type = 0;
  
	if (!module_type) {
		static const GTypeInfo module_info = {
			sizeof (GnomeSettingsModuleTypingBreakClass),
			NULL,		/* base_init */
			NULL,		/* base_finalize */
			(GClassInitFunc) gnome_settings_module_typing_break_class_init,
			NULL,		/* class_finalize */
			NULL,		/* class_data */
			sizeof (GnomeSettingsModuleTypingBreak),
			0,		/* n_preallocs */
			(GInstanceInitFunc) gnome_settings_module_typing_break_init,
		};
      
		module_type = g_type_register_static (GNOME_SETTINGS_TYPE_MODULE,
						      "GnomeSettingsModuleTypingBreak",
						      &module_info, 0);
	}
  
	return module_type;
}

static GnomeSettingsModuleRunlevel
gnome_settings_module_typing_break_get_runlevel (GnomeSettingsModule *module)
{
	return GNOME_SETTINGS_MODULE_RUNLEVEL_SERVICES;
}

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
	if (enabled) {
		if (typing_monitor_idle_id != 0)
			g_source_remove (typing_monitor_idle_id);
		if (typing_monitor_pid == 0) {
			GError *error = NULL;
			gchar *argv[] = { "gnome-typing-monitor", "-n", NULL };

			if (! g_spawn_async ("/",
					     argv, NULL,
					     G_SPAWN_STDOUT_TO_DEV_NULL | G_SPAWN_STDERR_TO_DEV_NULL |
					     G_SPAWN_SEARCH_PATH | G_SPAWN_DO_NOT_REAP_CHILD,
					     NULL, NULL,
					     &typing_monitor_pid,
					     &error)) {
				/* FIXME: put up a warning */
				g_print ("failed: %s\n", error->message);
				g_error_free (error);
				typing_monitor_pid = 0;
			}
		}
	} else {
		if (typing_monitor_pid != 0) {
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
	if (pid == typing_monitor_pid) {
		typing_monitor_pid = 0;
	}
}

static void 
typing_break_callback (GConfEntry *entry)
{
	if (! strcmp (entry->key, "/desktop/gnome/typing_break/enabled")) {
		if (entry->value->type == GCONF_VALUE_BOOL)
			setup_typing_break (gconf_value_get_bool (entry->value));
	}
}

static gboolean
gnome_settings_module_typing_break_initialize (GnomeSettingsModule *module, GConfClient *config_client)
{
	VteReaper *reaper;

	reaper = vte_reaper_get();
	g_signal_connect (reaper, "child_exited", G_CALLBACK (child_exited_callback), NULL);
	gnome_settings_register_config_callback ("/desktop/gnome/typing_break", typing_break_callback);

	return TRUE;
}

static gboolean
really_setup_typing_break (gpointer user_data)
{
	setup_typing_break (TRUE);
	return FALSE;
}

static gboolean
gnome_settings_module_typing_break_start (GnomeSettingsModule *module)
{
	if (gconf_client_get_bool (gnome_settings_module_get_config_client (module),
				   "/desktop/gnome/typing_break/enabled", NULL))
		g_timeout_add (30000, (GSourceFunc) really_setup_typing_break, NULL);

	return TRUE;
}

static gboolean
gnome_settings_module_typing_break_stop (GnomeSettingsModule *module)
{
	return TRUE;
}
