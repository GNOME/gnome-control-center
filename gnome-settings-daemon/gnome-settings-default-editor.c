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

#include "gnome-settings-daemon.h"
#include "gnome-settings-default-editor.h"

#include <libgnomevfs/gnome-vfs-mime-handlers.h>
#include <libgnomevfs/gnome-vfs-mime-monitor.h>

#include <string.h>

/* #define DE_DEBUG */

#define SYNC_CHANGES_KEY "/apps/gnome_settings_daemon/default_editor/sync_text_types"

static gboolean sync_changes;

#if DE_DEBUG
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

	if (!sync_changes_cb)
		return;

	
	star_app  = gnome_vfs_mime_get_default_application ("text/*");
	plain_app = gnome_vfs_mime_get_default_application ("text/plain");

	if (!strcmp (star_app->id, plain_app->id))
		return;

#if DE_DEBUG
	g_message ("Synching text/plain to text/*...");
#endif

	action = gnome_vfs_mime_get_default_action_type ("text/plain");

	gnome_vfs_mime_set_default_application ("text/*", plain_app->id);
	gnome_vfs_mime_set_default_action_type ("text/*", action);

	PRINT_STATE;
}

void
gnome_settings_default_editor_init (GConfClient *client)
{
	sync_changes = gconf_client_get_bool (client, SYNC_CHANGES_KEY, NULL);

	gnome_settings_daemon_register_callback (SYNC_CHANGES_KEY, sync_changes_cb);

	g_signal_connect (gnome_vfs_mime_monitor_get (), "data_changed",
			  G_CALLBACK (vfs_change_cb), client);			  
}

void
gnome_settings_default_editor_load (GConfClient *client)
{
	vfs_change_cb (NULL, client);
}
