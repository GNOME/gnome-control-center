/* -*- mode: c; style: linux -*- */

/* gnome-settings-gtk1theme.c
 *
 * Copyright © 2002 Red Hat, Inc.
 *
 * Written by Owen Taylor <otaylor@redhat.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "gnome-settings-daemon.h"
#include "gnome-settings-gtk1theme.h"

#define GTK_THEME_KEY "/desktop/gnome/interface/gtk_theme"

/* Given the theme filename, return the needed contents for the RC file
 * in the user's home directory
 */
static char *
make_contents (const char *filename)
{
	GString *result = g_string_new (NULL);

	g_string_append (result,
			 "# Autowritten by gnome-settings-daemon. Do not edit\n"
			 "\n");
	if (filename)
		g_string_append_printf (result,
					"include \"%s\"\n"
					"\n",
					filename);
	g_string_append_printf (result, 
                                "include \"%s/.gtkrc.mine\"\n",
				g_get_home_dir ());

	return g_string_free (result, FALSE);
}

/* Writes @contents to @rc_filename atomically (using rename); returns
 * %TRUE on sucess
 */
static gboolean
write_contents (const char *rc_filename,
		const char *contents)
{
	char *tmp_filename = g_strconcat (rc_filename, ".new", NULL);
	GIOChannel *channel;
	GError *err = NULL;

	channel =  g_io_channel_new_file (tmp_filename, "w", &err);
	if (!channel) {
		g_warning ("Cannot open %s: %s", tmp_filename, err->message);
		goto bail2;
	}

	if (g_io_channel_write_chars (channel, contents, -1, NULL, &err) != G_IO_STATUS_NORMAL) {
		g_warning ("Cannot open %s: %s", tmp_filename, err->message);
		goto bail0;
	}

	if (g_io_channel_flush (channel, &err) != G_IO_STATUS_NORMAL) {
		g_warning ("Error flushing %s: %s", tmp_filename, err->message);
		goto bail0;
	}

	if (g_io_channel_shutdown (channel, TRUE, &err) != G_IO_STATUS_NORMAL) {
		g_warning ("Error closing %s: %s", tmp_filename, err->message);
		goto bail1;
	}

	if (rename (tmp_filename, rc_filename) < 0) {
		g_warning ("Cannot move %s to %s: %s", tmp_filename, rc_filename, g_strerror (errno));
		goto bail1;
		
	}

	g_free (tmp_filename);
	
	return TRUE;

 bail0:
	g_io_channel_shutdown (channel, FALSE, NULL);
 bail1:
	unlink (tmp_filename);
 bail2:
	g_clear_error (&err);
	g_free (tmp_filename);

	return FALSE;
}

/* Send a client message telling GTK+-1.2 apps to reread their RC files
 */
static void
send_change_message (void)
{
	GdkEventClient sev;
	int i;
  
	for(i = 0; i < 5; i++)
		sev.data.l[i] = 0;
	
	sev.data_format = 32;
	sev.message_type = gdk_atom_intern("_GTK_READ_RCFILES", FALSE);
	
	gdk_event_send_clientmessage_toall ((GdkEvent *) &sev);
}

/* See if a theme called @theme exists in @base_dir. Takes ownership of @base_dir
 */
static char *
check_filename (char       *base_dir,
		const char *theme)
{
	char *theme_filename = g_build_filename (base_dir, theme, "gtk", "gtkrc", NULL);
	
	if (!g_file_test (theme_filename, G_FILE_TEST_EXISTS)) {
		g_free (theme_filename);
		theme_filename = NULL;
	}

	g_free (base_dir);

	return theme_filename;
}

static void
apply_settings (void)
{
	GConfClient *client = gconf_client_get_default ();
	gchar *current_theme;
	gchar *theme_filename;
	gchar *rc_filename;
	gchar *current_contents;
	gint current_length;
	gchar *new_contents;
	GError *err = NULL;

	current_theme = gconf_client_get_string (client, GTK_THEME_KEY, NULL);
	if (!current_theme)
		current_theme = g_strdup ("Default");

	/* Translate Default into Raleigh, since it's a better
	 * match than the default gtk1 theme.
	 */
	if (strcmp (current_theme, "Default") == 0) {
		g_free (current_theme);
		current_theme = g_strdup ("Raleigh");
	}

	/* Now look for a gtk1 theme with the name
	 */
	theme_filename = check_filename (g_build_filename (g_get_home_dir (),".themes", NULL),
					 current_theme);

	if (!theme_filename) {
		theme_filename = check_filename (g_build_filename (DATADIR, "themes", NULL),
						 current_theme);
	}

	/* If we don't find a match, use Raleigh
	 */
	if (!theme_filename) {
		theme_filename = check_filename (g_build_filename (DATADIR, "themes", NULL),
						 "Raleigh");
	}

	rc_filename = g_build_filename (g_get_home_dir(), ".gtkrc-1.2-gnome2", NULL);
	
	if (!g_file_get_contents (rc_filename, &current_contents, &current_length, &err) &&
	    !g_error_matches (err, G_FILE_ERROR, G_FILE_ERROR_NOENT)) {
		g_warning ("Can't get contents of %s: %s", rc_filename, err->message);
		g_clear_error (&err);
	}
	
	new_contents = make_contents (theme_filename);
	
	if (!current_contents ||
	    current_length != strlen (new_contents) ||
	    memcmp (current_contents, new_contents, current_length) != 0) {
		if (write_contents (rc_filename, new_contents))
			send_change_message ();
	}
		
	g_object_unref (client);
	g_free (new_contents);
	g_free (current_contents);
	g_free (rc_filename);
}

void
gnome_settings_gtk1_theme_init (GConfClient *client)
{
	gnome_settings_daemon_register_callback (GTK_THEME_KEY, (KeyCallbackFunc) apply_settings);
}

void
gnome_settings_gtk1_theme_load (GConfClient *client)
{
	apply_settings ();
}
