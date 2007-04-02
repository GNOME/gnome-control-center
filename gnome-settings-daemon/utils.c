/*
 * Copyright (C) 2007 The GNOME Foundation
 *
 * Authors:  Rodrigo Moya
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include "utils.h"

static GHashTable *directories = NULL;

static void
child_watch_cb (GPid pid, gint status, gpointer user_data)
{
	gchar *command = user_data;

	if (!WIFEXITED (status) || WEXITSTATUS (status)) {
		g_warning ("Command %s failed", command);
	}

	g_free (command);

}

/*
 * Helper function for spawn_with_input() - write an entire
 * string to a fd.
 */

static gboolean
write_all (int         fd,
	   const char *buf,
	   gsize       to_write)
{
	while (to_write > 0) {
		gssize count = write (fd, buf, to_write);
		if (count < 0) {
			if (errno != EINTR)
				return FALSE;
		} else {
			to_write -= count;
			buf += count;
		}
	}

	return TRUE;
}

/**
 * gnome_settings_spawn_with_input:
 * @argv: command line to run
 * @input: string to write to the child process. 
 * 
 * Spawns a child process specified by @argv, writes the text in
 * @input to it, then waits for the child to exit. Any failures
 * are output through g_warning(); if you wanted to use this in
 * cases where errors need to be presented to the user, some
 * modification would be needed.
 **/
void
gnome_settings_spawn_with_input (char **argv, const char *input)
{
	int child_pid;
	int inpipe;
	GError *err = NULL;
	gchar *command;
  
	if (!g_spawn_async_with_pipes (NULL /* working directory */, argv, NULL /* envp */,
				       G_SPAWN_SEARCH_PATH | G_SPAWN_DO_NOT_REAP_CHILD,
				       NULL, NULL, /* child setup and data */
				       &child_pid,
				       &inpipe, NULL, NULL, /* stdin, stdout, stderr */
				       &err)) {
		command = g_strjoinv (" ", argv);
		g_warning ("Could not execute %s: %s", command, err->message);
		g_error_free (err);
		g_free (command);

		return;
	}

	command = g_strjoinv (" ", argv);
	if (input) {
		if (!write_all (inpipe, input, strlen (input))) {
			g_warning ("Could not write input to %s", command);
		}

		close (inpipe);
	}
  
	g_child_watch_add (child_pid, (GChildWatchFunc) child_watch_cb, command);
}

extern GConfClient *conf_client;

GConfClient *
gnome_settings_get_config_client (void)
{
	if (!conf_client)
		conf_client = gconf_client_get_default ();

	return conf_client;
}

static void
config_notify (GConfClient *client,
               guint        cnxn_id,
               GConfEntry  *entry,
               gpointer     user_data)
{
	GSList *callbacks;

	callbacks = g_hash_table_lookup (directories, entry->key);
	if (callbacks) {
		GSList *sl;
		for (sl = callbacks; sl;  sl = sl->next)
			((GnomeSettingsConfigCallback) sl->data) (entry);
	}
}

void
gnome_settings_register_config_callback (const char *dir, GnomeSettingsConfigCallback func)
{
	GSList *callbacks;

	if (!directories)
		directories = g_hash_table_new (g_str_hash, g_str_equal);

	callbacks = g_hash_table_lookup (directories, dir);
	if (callbacks) {
		GSList *sl;

		for (sl = callbacks; sl; sl = sl->next) {
			if (func == sl->data)
				return;
		}

		/* callback not registered, so add it */
		callbacks = g_slist_append (callbacks, func);
	} else {
		GError *error = NULL;

		callbacks = g_slist_append (callbacks, func);
		g_hash_table_insert (directories, g_strdup (dir), callbacks);

		gconf_client_add_dir (gnome_settings_get_config_client (), dir,
				      GCONF_CLIENT_PRELOAD_ONELEVEL, NULL);
		gconf_client_notify_add (gnome_settings_get_config_client (), dir,
					 config_notify, NULL, NULL, &error);
		if (error) {
			g_warning ("Could not listen for changes to configuration in '%s': %s\n",
				   dir, error->message);
			g_error_free (error);
		}
	}
}
