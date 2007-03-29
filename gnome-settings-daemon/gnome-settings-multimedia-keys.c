/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2001-2003 Bastien Nocera <hadess@hadess.net>
 * Copyright (C) 2006      William Jon McCann <mccann@jhu.edu>
 *
 * gnome-settings-multimedia-keys.c
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, 
 * USA.
 */

#include <config.h>

#include <string.h>
#include <sys/file.h>
#include <X11/X.h>

/* Gnome headers */
#include <glib/gi18n.h>
#include <gdk/gdkx.h>
#include <gtk/gtk.h>

#include <gconf/gconf-client.h>

#include "eggaccelerators.h"

#include "actions/acme.h"
#include "actions/acme-volume.h"
#include "gsd-media-keys-window.h"
#include "gnome-settings-dbus.h"

#define VOLUME_STEP 6           /* percents for one volume button press */

/* we exclude shift, GDK_CONTROL_MASK and GDK_MOD1_MASK since we know what
   these modifiers mean
   these are the mods whose combinations are bound by the keygrabbing code */
#define IGNORED_MODS (0x2000 /*Xkb modifier*/ | GDK_LOCK_MASK  | \
	GDK_MOD2_MASK | GDK_MOD3_MASK | GDK_MOD4_MASK | GDK_MOD5_MASK)
/* these are the ones we actually use for global keys, we always only check
 * for these set */
#define USED_MODS (GDK_SHIFT_MASK | GDK_CONTROL_MASK | GDK_MOD1_MASK)

typedef struct Acme Acme;

struct Acme {
	AcmeVolume *volobj;
	GtkWidget *dialog;
	GConfClient *conf_client;

	/* Multihead stuff */
	GdkScreen *current_screen;
	GSList *screens;

	/* GnomeSettingsServer */
	GObject *server;
};

static void
acme_error (char * msg)
{
	GtkWidget *error_dialog;

	error_dialog =
	    gtk_message_dialog_new (NULL,
			    GTK_DIALOG_MODAL,
			    GTK_MESSAGE_ERROR,
			    GTK_BUTTONS_OK,
			    "%s", msg);
	gtk_dialog_set_default_response (GTK_DIALOG (error_dialog),
			GTK_RESPONSE_OK);
	gtk_widget_show (error_dialog);
	g_signal_connect (G_OBJECT (error_dialog),
		"response",
		G_CALLBACK (gtk_widget_destroy), NULL);
}

static char *
get_term_command (Acme *acme) {
	gchar *cmd_term;
	gchar *cmd = NULL;

	cmd_term = gconf_client_get_string (acme->conf_client,
				   "/desktop/gnome/applications/terminal/exec", NULL);
	if ((cmd_term != NULL) && (strcmp (cmd_term, "") != 0)) {
		gchar *cmd_args;
		cmd_args = gconf_client_get_string (acme->conf_client,
					   "/desktop/gnome/applications/terminal/exec_arg", NULL);
		if ((cmd_args != NULL) && (strcmp (cmd_term, "") != 0))
			cmd = g_strdup_printf ("%s %s -e", cmd_term, cmd_args);
		else
			cmd = g_strdup_printf ("%s -e", cmd_term);
		
		g_free (cmd_args);
	} 
	g_free (cmd_term);
	return cmd;
}

static void
execute (Acme *acme, char *cmd, gboolean sync, gboolean need_term)
{
	gboolean retval;
	gchar **argv;
	gint argc;
	char *exec;
	char *term = NULL;

	retval = FALSE;

	if (need_term) {
		term = get_term_command (acme);
		if (term == NULL) {
			acme_error (_("Could not get default terminal. Verify that your default "
			              "terminal command is set and points to a valid application."));
			return;
		}
	}
	
	if (term) {
		exec = g_strdup_printf ("%s %s", term, cmd);
		g_free (term);
	} else
		exec = g_strdup (cmd);

	if (g_shell_parse_argv (exec, &argc, &argv, NULL)) {
		if (sync != FALSE) {
			retval = g_spawn_sync (g_get_home_dir (),
			                       argv, NULL, G_SPAWN_SEARCH_PATH,
			                       NULL, NULL, NULL, NULL, NULL, NULL);
		}
		else {
			retval = g_spawn_async (g_get_home_dir (),
			                        argv, NULL, G_SPAWN_SEARCH_PATH,
			                        NULL, NULL, NULL, NULL);
		}
		g_strfreev (argv);
	}

	if (retval == FALSE)
	{
		char *msg;
		msg = g_strdup_printf
			(_("Couldn't execute command: %s\n"
			   "Verify that this is a valid command."),
			 exec);

		acme_error (msg);
		g_free (msg);
	}
	g_free (exec);
}

static void
do_sleep_action (char *cmd1, char *cmd2)
{
	if (g_spawn_command_line_async (cmd1, NULL) == FALSE)
	{
		if (g_spawn_command_line_async (cmd2, NULL) == FALSE)
		{
			acme_error (_("Couldn't put the machine to sleep.\n"
					"Verify that the machine is correctly configured."));
		}
	}
}

static void
dialog_init (Acme *acme)
{
	if (acme->dialog != NULL &&
	    !gsd_media_keys_window_is_valid (GSD_MEDIA_KEYS_WINDOW (acme->dialog)))
	{
		g_object_unref (acme->dialog);
		acme->dialog = NULL;
	}

	if (acme->dialog == NULL) {
		acme->dialog = gsd_media_keys_window_new ();
	}
}

static gboolean
grab_key_real (Key *key, GdkWindow *root, gboolean grab, int result)
{
	gdk_error_trap_push ();
	if (grab)
		XGrabKey (GDK_DISPLAY(), key->keycode, (result | key->state),
				GDK_WINDOW_XID (root), True, GrabModeAsync, GrabModeAsync);
	else
		XUngrabKey(GDK_DISPLAY(), key->keycode, (result | key->state),
				GDK_WINDOW_XID (root));
	gdk_flush ();

	gdk_error_trap_pop ();

	return TRUE;
}

/* inspired from all_combinations from gnome-panel/gnome-panel/global-keys.c */
#define N_BITS 32
static void
grab_key (Acme *acme, Key *key, gboolean grab)
{
	int indexes[N_BITS];/*indexes of bits we need to flip*/
	int i, bit, bits_set_cnt;
	int uppervalue;
	guint mask_to_traverse = IGNORED_MODS & ~ key->state;

	bit = 0;
	for (i = 0; i < N_BITS; i++) {
		if (mask_to_traverse & (1<<i))
			indexes[bit++]=i;
	}

	bits_set_cnt = bit;

	uppervalue = 1<<bits_set_cnt;
	for (i = 0; i < uppervalue; i++) {
		GSList *l;
		int j, result = 0;

		for (j = 0; j < bits_set_cnt; j++) {
			if (i & (1<<j))
				result |= (1<<indexes[j]);
		}

		for (l = acme->screens; l ; l = l->next) {
			GdkScreen *screen = l->data;
			if (grab_key_real (key, gdk_screen_get_root_window (screen), grab, result) == FALSE)
				return;
		}
	}
}

static gboolean
is_valid_shortcut (const char *string)
{
	if (string == NULL || string[0] == '\0')
		return FALSE;
	if (strcmp (string, "disabled") == 0)
		return FALSE;

	return TRUE;
}

static void
update_kbd_cb (GConfClient *client, guint id, GConfEntry *entry, gpointer data)
{
	Acme *acme = (Acme *) data;
	int i;
	gboolean found = FALSE;

	g_return_if_fail (entry->key != NULL);

	/* Find the key that was modified */
	for (i = 0; i < HANDLED_KEYS; i++)
	{
		if (strcmp (entry->key, keys[i].gconf_key) == 0)
		{
			char *tmp;
			Key *key;

			found = TRUE;

			if (keys[i].key != NULL)
				grab_key (acme, keys[i].key, FALSE);

			g_free (keys[i].key);
			keys[i].key = NULL;

			tmp = gconf_client_get_string (acme->conf_client,
					keys[i].gconf_key, NULL);

			if (is_valid_shortcut (tmp) == FALSE)
			{
				g_free (tmp);
				break;
			}

			key = g_new0 (Key, 1);
			if (egg_accelerator_parse_virtual (tmp, &key->keysym, &key->keycode, &key->state) == FALSE
			    || key->keycode == 0)
			{
				g_free (tmp);
				g_free (key);
				break;
			}

			grab_key (acme, key, TRUE);
			keys[i].key = key;

			g_free (tmp);

			break;
		}
	}

	if (found != FALSE)
		return;
}

static void
init_screens (Acme *acme)
{
	GdkDisplay *display = gdk_display_get_default ();
	int i;

	if (gdk_display_get_n_screens (display) == 1) {
		GdkScreen *screen = gdk_screen_get_default ();
		acme->screens = g_slist_append (acme->screens, screen);
		acme->current_screen = screen;
		return;
	}

	for (i = 0; i < gdk_display_get_n_screens (display); i++)
	{
		GdkScreen *screen;

		screen = gdk_display_get_screen (display, i);
		if (screen == NULL)
			continue;
		acme->screens = g_slist_append (acme->screens, screen);
	}

	acme->current_screen = (GdkScreen *)acme->screens->data;
}

static void
init_kbd (Acme *acme)
{
	int i;

	for (i = 0; i < HANDLED_KEYS; i++)
	{
		char *tmp;
		Key *key;

		gconf_client_notify_add (acme->conf_client,
				keys[i].gconf_key, update_kbd_cb,
				acme, NULL, NULL);

		tmp = gconf_client_get_string (acme->conf_client,
				keys[i].gconf_key, NULL);
		if (!is_valid_shortcut (tmp)) {
		        g_free (tmp);
			continue;
		}

		key = g_new0 (Key, 1);
		if (!egg_accelerator_parse_virtual (tmp, &key->keysym, &key->keycode, &key->state)
		    || key->keycode == 0)
		{
		        g_free (tmp);
			g_free (key);
			continue;
		}
	/*avoid grabbing all the keyboard when KeyCode cannot be retrieved */
		if (key->keycode == AnyKey)
		  {
		    g_warning ("The shortcut key \"%s\" cannot be found on the current system, ignoring the binding", tmp);
		    g_free (tmp);
		    g_free (key);
		    continue;
		  }

		g_free (tmp);

		keys[i].key = key;

		grab_key (acme, key, TRUE);
	}
}

static void
dialog_show (Acme *acme)
{
	int orig_w, orig_h;
	int screen_w, screen_h;
	int x, y;
	int pointer_x, pointer_y;
	GtkRequisition win_req;
	GdkScreen *pointer_screen;
	GdkRectangle geometry;
	int monitor;

	gtk_window_set_screen (GTK_WINDOW (acme->dialog), acme->current_screen);

	/*
	 * get the window size
	 * if the window hasn't been mapped, it doesn't necessarily
	 * know its true size, yet, so we need to jump through hoops
	 */
	gtk_window_get_default_size (GTK_WINDOW (acme->dialog), &orig_w, &orig_h);
	gtk_widget_size_request (acme->dialog, &win_req);

	if (win_req.width > orig_w)
		orig_w = win_req.width;
	if (win_req.height > orig_h)
		orig_h = win_req.height;

	pointer_screen = NULL;
	gdk_display_get_pointer (gdk_screen_get_display (acme->current_screen),
				 &pointer_screen, &pointer_x,
				 &pointer_y, NULL);
	if (pointer_screen != acme->current_screen) {
		/* The pointer isn't on the current screen, so just
		 * assume the default monitor
		 */
		monitor = 0;
	} else {
		monitor = gdk_screen_get_monitor_at_point (acme->current_screen,
							   pointer_x, pointer_y);
	}

	gdk_screen_get_monitor_geometry (acme->current_screen, monitor,
					 &geometry);

	screen_w = geometry.width;
	screen_h = geometry.height;

	x = ((screen_w - orig_w) / 2) + geometry.x;
	y = geometry.y + (screen_h / 2) + (screen_h / 2 - orig_h) / 2;

	gtk_window_move (GTK_WINDOW (acme->dialog), x, y);

	gtk_widget_show (acme->dialog);

	gdk_display_sync (gdk_screen_get_display (acme->current_screen));
}

static void
do_unknown_action (Acme *acme, const char *url)
{
	char *string;

	g_return_if_fail (url != NULL);

	string = gconf_client_get_string (acme->conf_client,
			"/desktop/gnome/url-handlers/unknown/command",
			NULL);

	if ((string != NULL) && (strcmp (string, "") != 0)) {
		gchar *cmd;
		cmd = g_strdup_printf (string, url);
		execute (acme, cmd, FALSE, FALSE);
		g_free (cmd);
	}
	g_free (string);
}

static void
do_help_action (Acme *acme)
{
	char *string;

	string = gconf_client_get_string (acme->conf_client,
			"/desktop/gnome/url-handlers/ghelp/command",
			NULL);

	if ((string != NULL) && (strcmp (string, "") != 0)) {
		gchar *cmd;
		cmd = g_strdup_printf (string, "");
		execute (acme, cmd, FALSE, FALSE);
		g_free (cmd);
	} else 
		do_unknown_action (acme, "ghelp:");

	g_free (string);
}

static void
do_mail_action (Acme *acme)
{
	char *string;

	string = gconf_client_get_string (acme->conf_client,
			"/desktop/gnome/url-handlers/mailto/command",
			NULL);

	if ((string != NULL) && (strcmp (string, "") != 0)) {
		gchar *cmd;
		cmd = g_strdup_printf (string, "");
		execute (acme, cmd, FALSE, gconf_client_get_bool (acme->conf_client,
					   "/desktop/gnome/url-handlers/mailto/needs_terminal", NULL));
		g_free (cmd);
	}
	g_free (string);
}

static void
do_media_action (Acme *acme)
{
	char *command;

	command = gconf_client_get_string (acme->conf_client,
					   "/desktop/gnome/applications/media/exec", NULL);
	if ((command != NULL) && (strcmp (command, "") != 0)) {
		execute (acme, command, FALSE, gconf_client_get_bool (acme->conf_client,
					   "/desktop/gnome/applications/media/needs_term", NULL));
	}
	g_free (command);
}

static void
do_www_action (Acme *acme, const char *url)
{
	char *string;

	string = gconf_client_get_string (acme->conf_client,
		"/desktop/gnome/url-handlers/http/command",
		 NULL);

	if ((string != NULL) && (strcmp (string, "") != 0)) {
		gchar *cmd;

		if (url == NULL)
			cmd = g_strdup_printf (string, "");
		else
			cmd = g_strdup_printf (string, url);

		execute (acme, cmd, FALSE, gconf_client_get_bool (acme->conf_client,
					   "/desktop/gnome/url-handlers/http/needs_terminal", NULL));
		g_free (cmd);
	} else {
		do_unknown_action (acme, url ? url : "");
	}
	g_free (string);
}

static void
do_exit_action (Acme *acme)
{
	execute (acme, "gnome-session-save --kill", FALSE, FALSE);
}

static void
do_eject_action (Acme *acme)
{
	char *command;

	dialog_init (acme);
	gsd_media_keys_window_set_action (GSD_MEDIA_KEYS_WINDOW (acme->dialog),
					  GSD_MEDIA_KEYS_WINDOW_ACTION_EJECT);
	dialog_show (acme);

	command = gconf_client_get_string (acme->conf_client,
					   GCONF_MISC_DIR "/eject_command", NULL);
	if ((command != NULL) && (strcmp (command, "") != 0))
		execute (acme, command, FALSE, FALSE);
	else
		execute (acme, "eject", FALSE, FALSE);

	g_free (command);
}

static void
do_sound_action (Acme *acme, int type)
{
	gboolean muted;
	int vol;
	int vol_step;

	if (acme->volobj == NULL)
		return;

	vol_step = gconf_client_get_int (acme->conf_client,
					 GCONF_MISC_DIR "/volume_step", NULL);

	if (vol_step == 0)
		vol_step = VOLUME_STEP;

	/* FIXME: this is racy */
	vol = acme_volume_get_volume (acme->volobj);
	muted = acme_volume_get_mute (acme->volobj);

	switch (type) {
	case MUTE_KEY:
		acme_volume_mute_toggle (acme->volobj);
		break;
	case VOLUME_DOWN_KEY:
		if (muted)
		{
			acme_volume_mute_toggle(acme->volobj);
		} else {
			acme_volume_set_volume (acme->volobj, vol - vol_step);
		}
		break;
	case VOLUME_UP_KEY:
		if (muted)
		{
			acme_volume_mute_toggle(acme->volobj);
		} else {
			acme_volume_set_volume (acme->volobj, vol + vol_step);
		}
		break;
	}

	muted = acme_volume_get_mute (acme->volobj);
	vol = acme_volume_get_volume (acme->volobj);

	/* FIXME: AcmeVolume should probably emit signals
	   instead of doing it like this */
	dialog_init (acme);
	gsd_media_keys_window_set_volume_muted (GSD_MEDIA_KEYS_WINDOW (acme->dialog),
						muted);
	gsd_media_keys_window_set_volume_level (GSD_MEDIA_KEYS_WINDOW (acme->dialog),
						vol);
	gsd_media_keys_window_set_action (GSD_MEDIA_KEYS_WINDOW (acme->dialog),
					  GSD_MEDIA_KEYS_WINDOW_ACTION_VOLUME);
	dialog_show (acme);
}

static gboolean
do_multimedia_player_action (Acme *acme, const gchar *key)
{
	return gnome_settings_server_media_player_key_pressed (acme->server, key);
}

static gboolean
do_action (Acme *acme, int type)
{
	gchar *cmd;

	switch (type) {
	case MUTE_KEY:
	case VOLUME_DOWN_KEY:
	case VOLUME_UP_KEY:
		do_sound_action (acme, type);
		break;
	case POWER_KEY:
		do_exit_action (acme);
		break;
	case EJECT_KEY:
		do_eject_action (acme);
		break;
	case HOME_KEY:
		execute (acme, "nautilus", FALSE, FALSE);
		break;
	case SEARCH_KEY:
		execute (acme, "gnome-search-tool", FALSE, FALSE);
		break;
	case EMAIL_KEY:
		do_mail_action (acme);
		break;
	case SLEEP_KEY:
		do_sleep_action ("apm", "xset dpms force off");
		break;
	case SCREENSAVER_KEY:
		if ((cmd = g_find_program_in_path ("gnome-screensaver-command")))
			execute (acme, "gnome-screensaver-command --lock", FALSE, FALSE);
		else
			execute (acme, "xscreensaver-command -lock", FALSE, FALSE);

		g_free (cmd);
		break;
	case HELP_KEY:
		do_help_action (acme);
		break;
	case WWW_KEY:
		do_www_action (acme, NULL);
		break;
	case MEDIA_KEY:
		do_media_action (acme);
		break;
	case PLAY_KEY:
		return do_multimedia_player_action (acme, "Play");
		break;
	case PAUSE_KEY:
		return do_multimedia_player_action (acme, "Pause");
		break;
	case STOP_KEY:
		return do_multimedia_player_action (acme, "Stop");
		break;
	case PREVIOUS_KEY:
		return do_multimedia_player_action (acme, "Previous");
		break;
	case NEXT_KEY:
		return do_multimedia_player_action (acme, "Next");
		break;
	default:
		g_assert_not_reached ();
	}

	return FALSE;
}

static GdkScreen *
acme_get_screen_from_event (Acme *acme, XAnyEvent *xanyev)
{
	GdkWindow *window;
	GdkScreen *screen;
	GSList *l;

	/* Look for which screen we're receiving events */
	for (l = acme->screens; l != NULL; l = l->next)
	{
		screen = (GdkScreen *) l->data;
		window = gdk_screen_get_root_window (screen);

		if (GDK_WINDOW_XID (window) == xanyev->window)
		{
			return screen;
		}
	}

	return NULL;
}

static GdkFilterReturn
acme_filter_events (GdkXEvent *xevent, GdkEvent *event, gpointer data)
{
	Acme *acme = (Acme *) data;
	XEvent *xev = (XEvent *) xevent;
	XAnyEvent *xanyev = (XAnyEvent *) xevent;
	guint keycode, state;
	int i;

	/* verify we have a key event */
	if (xev->xany.type != KeyPress &&
			xev->xany.type != KeyRelease)
		return GDK_FILTER_CONTINUE;

	keycode = xev->xkey.keycode;
	state = xev->xkey.state;

	for (i = 0; i < HANDLED_KEYS; i++)
	{
		if (keys[i].key == NULL)
			continue;

		if (keys[i].key->keycode == keycode &&
				(state & USED_MODS) == keys[i].key->state)
		{
			switch (keys[i].key_type) {
			case VOLUME_DOWN_KEY:
			case VOLUME_UP_KEY:
				/* auto-repeatable keys */
				if (xev->type != KeyPress)
					return GDK_FILTER_CONTINUE;
				break;
			default:
				if (xev->type != KeyRelease)
					return GDK_FILTER_CONTINUE;
			}

			acme->current_screen = acme_get_screen_from_event
				(acme, xanyev);

			if (do_action (acme, keys[i].key_type) == FALSE)
				return GDK_FILTER_REMOVE;
			else
				return GDK_FILTER_CONTINUE;
		}
	}

	return GDK_FILTER_CONTINUE;
}

void
gnome_settings_multimedia_keys_init (GConfClient *client)
{
}

void
gnome_settings_multimedia_keys_load (GConfClient *client)
{
	GSList *l;
	Acme   *acme;

	acme = g_new0 (Acme, 1);

	acme->conf_client = client;
	gconf_client_add_dir (acme->conf_client,
			      GCONF_BINDING_DIR,
			      GCONF_CLIENT_PRELOAD_ONELEVEL,
			      NULL);

	init_screens (acme);
	init_kbd (acme);

	acme->server = gnome_settings_server_get ();

	/* initialise Volume handler */
	acme->volobj = acme_volume_new();

	/* Start filtering the events */
	for (l = acme->screens; l != NULL; l = l->next)
		gdk_window_add_filter (gdk_screen_get_root_window (l->data),
			acme_filter_events, (gpointer) acme);
}

