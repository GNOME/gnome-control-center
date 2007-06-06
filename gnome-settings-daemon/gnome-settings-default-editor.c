/*
 * gnome-settings-default-editor.h: sync default editor changes to mime database
 *
 * Copyright 2002 Sun Microsystems, Inc.
 *
 * Author: jacob berkman  <jacob@ximian.com>
 *
 */

/*
 * WARNING: This is a hack.
 *
 * All it does is keep the "text / *" and "text/plain" mime type
 * handlers in sync with each other.  The reason we do this is because
 * there is no UI for editing the text / * handler, and this is probably
 * what the user actually wants to do.
 */

#include <config.h>

#include "gnome-settings-module.h"
#include "utils.h"

#include <libgnomevfs/gnome-vfs-mime-handlers.h>
#include <libgnomevfs/gnome-vfs-mime-monitor.h>

#include <string.h>

typedef struct {
	GnomeSettingsModule parent;
} GnomeSettingsModuleDefaultEditor;

typedef struct {
	GnomeSettingsModuleClass parent_class;
} GnomeSettingsModuleDefaultEditorClass;

static GnomeSettingsModuleRunlevel gnome_settings_module_default_editor_get_runlevel (GnomeSettingsModule *module);
static gboolean gnome_settings_module_default_editor_initialize (GnomeSettingsModule *module,
								 GConfClient *config_client);
static gboolean gnome_settings_module_default_editor_start (GnomeSettingsModule *module);
static gboolean gnome_settings_module_default_editor_stop (GnomeSettingsModule *module);

/* #define DE_DEBUG */

#define SYNC_CHANGES_KEY "/apps/gnome_settings_daemon/default_editor/sync_text_types"

static gboolean sync_changes;

static void
gnome_settings_module_default_editor_class_init (GnomeSettingsModuleDefaultEditorClass *klass)
{
	GnomeSettingsModuleClass *module_class;

	module_class = (GnomeSettingsModuleClass *) klass;
	module_class->get_runlevel = gnome_settings_module_default_editor_get_runlevel;
	module_class->initialize = gnome_settings_module_default_editor_initialize;
	module_class->start = gnome_settings_module_default_editor_start;
	module_class->stop = gnome_settings_module_default_editor_stop;
}

static void
gnome_settings_module_default_editor_init (GnomeSettingsModuleDefaultEditor *module)
{
}

GType
gnome_settings_module_default_editor_get_type (void)
{
	static GType module_type = 0;
  
	if (!module_type) {
		static const GTypeInfo module_info = {
			sizeof (GnomeSettingsModuleDefaultEditorClass),
			NULL,		/* base_init */
			NULL,		/* base_finalize */
			(GClassInitFunc) gnome_settings_module_default_editor_class_init,
			NULL,		/* class_finalize */
			NULL,		/* class_data */
			sizeof (GnomeSettingsModuleDefaultEditor),
			0,		/* n_preallocs */
			(GInstanceInitFunc) gnome_settings_module_default_editor_init,
		};
      
		module_type = g_type_register_static (GNOME_SETTINGS_TYPE_MODULE,
						      "GnomeSettingsModuleDefaultEditor",
						      &module_info, 0);
	}
  
	return module_type;
}

static GnomeSettingsModuleRunlevel
gnome_settings_module_default_editor_get_runlevel (GnomeSettingsModule *module)
{
	return GNOME_SETTINGS_MODULE_RUNLEVEL_GNOME_SETTINGS;
}

#ifdef DE_DEBUG
static void
print_mime_app (const char *mime_type)
{
	GnomeVFSMimeApplication *mime_app;

	mime_app = gnome_vfs_mime_get_default_application (mime_type);

	g_message ("Default info for %s (%p):\n"
		   "\t        id: %s\n"
		   "\t      name: %s\n"
		   "\t   command: %s\n"
		   "\tneeds term: %s\n",
		   mime_type, mime_app,
		   mime_app->id, 
		   mime_app->name, 
		   mime_app->command,
		   mime_app->requires_terminal ? "Yes" : "No");
}

static void
print_state (void)
{
	if (sync_changes)
		g_message ("Synching changes.");
	else
		g_message ("Not synching changes.");

	print_mime_app ("text/*");
	print_mime_app ("text/plain");
}
#define PRINT_STATE print_state()
#else
#define PRINT_STATE
#endif

static void
sync_changes_cb (GConfEntry *entry)
{
	GConfValue *value = gconf_entry_get_value (entry);
	sync_changes = gconf_value_get_bool (value);

	PRINT_STATE;
}

static void
vfs_change_cb (GnomeVFSMIMEMonitor *monitor, GConfClient *client)
{
	GnomeVFSMimeApplication *star_app, *plain_app;
	GnomeVFSMimeActionType action;

	PRINT_STATE;

	if (!sync_changes)
		return;
	
	star_app  = gnome_vfs_mime_get_default_application ("text/*");
	plain_app = gnome_vfs_mime_get_default_application ("text/plain");

	if (star_app == NULL || plain_app == NULL) {
	        if (star_app != NULL) {
	            gnome_vfs_mime_application_free (star_app);
	        }
	        if (plain_app != NULL) {
	            gnome_vfs_mime_application_free (plain_app);
	        }
		return;
	}
	if (!strcmp (star_app->id, plain_app->id)) {
	        gnome_vfs_mime_application_free (star_app);
	        gnome_vfs_mime_application_free (plain_app);
		return;
	}

#ifdef DE_DEBUG
	g_message ("Synching text/plain to text/*...");
#endif

	action = gnome_vfs_mime_get_default_action_type ("text/plain");

	gnome_vfs_mime_set_default_application ("text/*", plain_app->id);
	gnome_vfs_mime_application_free (plain_app);

	gnome_vfs_mime_set_default_action_type ("text/*", action);

	PRINT_STATE;
}

static gboolean
gnome_settings_module_default_editor_initialize (GnomeSettingsModule *module, GConfClient *config_client)
{
	sync_changes = gconf_client_get_bool (config_client, SYNC_CHANGES_KEY, NULL);

	gnome_settings_register_config_callback (SYNC_CHANGES_KEY, sync_changes_cb);

	g_signal_connect (gnome_vfs_mime_monitor_get (), "data_changed",
			  G_CALLBACK (vfs_change_cb), config_client);

	return TRUE;
}

static gboolean
gnome_settings_module_default_editor_start (GnomeSettingsModule *module)
{
	vfs_change_cb (NULL, gnome_settings_module_get_config_client (module));

	return TRUE;
}

static gboolean
gnome_settings_module_default_editor_stop (GnomeSettingsModule *module)
{
	return TRUE;
}
