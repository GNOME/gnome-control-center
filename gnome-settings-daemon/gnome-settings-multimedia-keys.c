/* 
 * Copyright (C) 2001,2002,2003 Bastien Nocera <hadess@hadess.net>
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

#include <glade/glade.h>
#include <gconf/gconf-client.h>

#include "eggaccelerators.h"

#if defined(__powerpc__) && defined (__linux__)
#define USE_FBLEVEL
#include "actions/acme-fb-level.h"
#else
#undef USE_FBLEVEL
#endif

#include "actions/acme.h"
#include "actions/acme-volume.h"

#define DIALOG_TIMEOUT 1000     /* dialog timeout in ms */
#define VOLUME_STEP 6           /* percents for one volume button press */
                                                                                
/* we exclude shift, GDK_CONTROL_MASK and GDK_MOD1_MASK since we know what
   these modifiers mean
   these are the mods whose combinations are bound by the keygrabbing code */
#define IGNORED_MODS (0x2000 /*Xkb modifier*/ | GDK_LOCK_MASK  | \
	GDK_MOD2_MASK | GDK_MOD3_MASK | GDK_MOD4_MASK | GDK_MOD5_MASK)
/* these are the ones we actually use for global keys, we always only check
 * for these set */
#define USED_MODS (GDK_SHIFT_MASK | GDK_CONTROL_MASK | GDK_MOD1_MASK)

typedef struct {
	AcmeVolume *volobj;
#ifdef USE_FBLEVEL
	AcmeFblevel *levobj;
#endif
	GladeXML *xml;
	GtkWidget *dialog;
	GConfClient *conf_client;
	guint dialog_timeout;

	/* Multihead stuff */
	GdkScreen *current_screen;
	GSList *screens;
} Acme;

enum {
	ICON_MUTED,
	ICON_LOUD,
	ICON_BRIGHT,
	ICON_EJECT,
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

static void
execute (char *cmd, gboolean sync)
{
	gboolean retval;
	gchar **argv;
	gint argc;
	
	retval = FALSE;

	if (g_shell_parse_argv (cmd, &argc, &argv, NULL)) {
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
			   "Verify that this command exists."),
			 cmd);

		acme_error (msg);
		g_free (msg);
	}
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

#ifdef USE_FBLEVEL
static char*
permission_problem_string (const char *file)
{
	return g_strdup_printf (_("Permissions on the file %s are broken\n"), file);
}

static void
fblevel_problem_cb (void)
{
	char *msg;

	msg = permission_problem_string ("/dev/pmu");
	acme_error (msg);
	g_free (msg);

	return;
}
#endif

static char *images[] = {
	PIXMAPSDIR "/gnome-speakernotes-muted.png",
	PIXMAPSDIR "/gnome-speakernotes.png",
	PIXMAPSDIR "/acme-brightness.png",
	PIXMAPSDIR "/acme-eject.png",
};

static void
acme_image_set (Acme *acme, int icon)
{
	GtkWidget *image;

	image = glade_xml_get_widget (acme->xml, "image1");
	g_return_if_fail (image != NULL);

	if (icon > ICON_EJECT)
		g_assert_not_reached ();

	gtk_image_set_from_file (GTK_IMAGE(image), images[icon]);
}

static void
dialog_init (Acme *acme)
{
	if (acme->xml == NULL) {
		glade_gnome_init ();
		acme->xml = glade_xml_new (DATADIR "/control-center-2.0/interfaces/acme.glade", NULL, NULL);

		if (acme->xml == NULL) {
			acme_error (_("Couldn't load the Glade file.\n"
				      "Make sure that this daemon is properly installed."));
			return;
		}
		acme->dialog = glade_xml_get_widget (acme->xml, "dialog");
		acme_image_set (acme, ICON_LOUD);
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

static void
unhookup_keysym (int keycode)
{
	char *command;

	if (keycode <= 0)
		return;

	command = g_strdup_printf ("xmodmap -e \"keycode %d = \"", keycode);

	g_spawn_command_line_sync (command, NULL, NULL, NULL, NULL);
	g_free (command);
}

static gboolean
hookup_keysym (int keycode, const char *keysym)
{
	char *command;

	if (keycode <= 0)
		return TRUE;

	command = g_strdup_printf ("xmodmap -e \"keycode %d = %s\"",
			keycode, keysym);

	g_spawn_command_line_sync (command, NULL, NULL, NULL, NULL);
	g_free (command);

	return FALSE;
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
	for (i = 0; i < PLAY_KEY; i++)
	{
		if (strcmp (entry->key, keys[i].gconf_key) == 0)
		{
			const char *tmp;
			Key *key;

			found = TRUE;

			if (keys[i].key != NULL)
				grab_key (acme, keys[i].key, FALSE);

			g_free (keys[i].key);
			keys[i].key = NULL;

			tmp = gconf_client_get_string (acme->conf_client,
					keys[i].gconf_key, NULL);

			if (is_valid_shortcut (tmp) == FALSE)
				break;

			key = g_new0 (Key, 1);
			if (egg_accelerator_parse_virtual (tmp, &key->keysym, &key->keycode, &key->state) == FALSE)
			{
				g_free (key);
				break;
			}

			grab_key (acme, key, TRUE);
			keys[i].key = key;

			break;
		}
	}

	if (found != FALSE)
		return;

	for (i = PLAY_KEY; i < HANDLED_KEYS; i++)
	{
		if (strcmp (entry->key, keys[i].gconf_key) == 0)
		{
			const char *tmp;
			Key *key;

			if (keys[i].key != NULL)
				unhookup_keysym (keys[i].key->keycode);

			g_free (keys[i].key);
			keys[i].key = NULL;

			tmp = gconf_client_get_string (acme->conf_client,
					keys[i].gconf_key, NULL);

			if (is_valid_shortcut (tmp) == FALSE)
				break;

			key = g_new0 (Key, 1);
			if (egg_accelerator_parse_virtual (tmp, &key->keysym, &key->keycode, &key->state) == FALSE)
			{
				g_free (key);
				break;
			}

			switch (keys[i].key_type) {
			case PLAY_KEY:
				hookup_keysym (key->keycode, "XF86AudioPlay");
				break;
			case PAUSE_KEY:
				hookup_keysym (key->keycode, "XF86AudioPause");
				break;
			case STOP_KEY:
				hookup_keysym (key->keycode, "XF86AudioStop");
				break;
			case PREVIOUS_KEY:
				hookup_keysym (key->keycode, "XF86AudioPrev");
				break;
			case NEXT_KEY:
				hookup_keysym (key->keycode, "XF86AudioNext");
				break;
			}

			keys[i].key = key;
		}
	}
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

	for (i = 0; i < PLAY_KEY; i++)
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
		if (!egg_accelerator_parse_virtual (tmp, &key->keysym, &key->keycode, &key->state))
		{
		        g_free (tmp);
			g_free (key);
			continue;
		}
		g_free (tmp);

		keys[i].key = key;

		grab_key (acme, key, TRUE);
	}

	for (i = PLAY_KEY; i < HANDLED_KEYS; i++)
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
		if (egg_accelerator_parse_virtual (tmp, &key->keysym, &key->keycode, &key->state) == FALSE)
		{
		        g_free (tmp);
			g_free (key);
			continue;
		}
		g_free (tmp);

		keys[i].key = key;

		switch (keys[i].key_type) {
		case PLAY_KEY:
			hookup_keysym (keys[i].key->keycode,
					"XF86AudioPlay");
			break;
		case PAUSE_KEY:
			hookup_keysym (keys[i].key->keycode,
					"XF86AudioPause");
			break;
		case STOP_KEY:
			hookup_keysym (keys[i].key->keycode,
					"XF86AudioStop");
			break;
		case PREVIOUS_KEY:
			hookup_keysym (keys[i].key->keycode,
					"XF86AudioPrev");
			break;
		case NEXT_KEY:
			hookup_keysym (keys[i].key->keycode,
					"XF86AudioNext");
			break;
		}
	}

	return;
}

static gboolean
dialog_hide (Acme *acme)
{
	gtk_widget_hide (acme->dialog);
	acme->dialog_timeout = 0;
	return FALSE;
}

static void
dialog_show (Acme *acme)
{
	int orig_x, orig_y, orig_w, orig_h, orig_d;
	int screen_w, screen_h;
	int x, y;
	int pointer_x, pointer_y;
	GdkScreen *pointer_screen;
	GdkRectangle geometry;
	int monitor;

	gtk_window_set_screen (GTK_WINDOW (acme->dialog), acme->current_screen);
	gtk_widget_realize (GTK_WIDGET (acme->dialog));

	gdk_window_get_geometry (GTK_WIDGET (acme->dialog)->window,
				 &orig_x, &orig_y,
				 &orig_w, &orig_h, &orig_d);

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

	acme->dialog_timeout = gtk_timeout_add (DIALOG_TIMEOUT,
			(GtkFunction) dialog_hide, acme);
}

static void
do_unknown_action (Acme *acme, const char *url)
{
	char *string, *command;

	g_return_if_fail (url != NULL);

	string = gconf_client_get_string (acme->conf_client,
			"/desktop/gnome/url-handlers/unknown/command",
			NULL);

	if (string == NULL || strcmp (string, "") == 0)
		return;

	command = g_strdup_printf (string, url);

	execute (command, FALSE);

	g_free (command);
	g_free (string);
}

static void
do_help_action (Acme *acme)
{
	char *string, *command;

	string = gconf_client_get_string (acme->conf_client,
			"/desktop/gnome/url-handlers/ghelp/command",
			NULL);

	if (string == NULL && strcmp (string, "") == 0)
	{
		do_unknown_action (acme, "ghelp:");
		return;
	}

	command = g_strdup_printf (string, "");

	execute (command, FALSE);

	g_free (command);
	g_free (string);
}

static void
do_mail_action (Acme *acme)
{
	char *string, *command;

	string = gconf_client_get_string (acme->conf_client,
			"/desktop/gnome/url-handlers/mailto/command",
			NULL);

	if (string == NULL || strcmp (string, "") == 0)
		return;

	command = g_strdup_printf (string, "");

	execute (command, FALSE);

	g_free (command);
	g_free (string);
}

static void
do_www_action (Acme *acme, const char *url)
{
	char *string, *command;

	string = gconf_client_get_string (acme->conf_client,
		"/desktop/gnome/url-handlers/http/command",
		 NULL);

	if (string == NULL || strcmp (string, "") == 0)
	{
		do_unknown_action (acme, url ? url : "");
		return;
	}

	if (url == NULL)
		command = g_strdup_printf (string, "");
	else
		command = g_strdup_printf (string, url);

	execute (command, FALSE);

	g_free (command);
	g_free (string);
}

static void
do_exit_action (Acme *acme)
{
	execute ("gnome-session-save --kill", FALSE);
}

static void
do_eject_action (Acme *acme)
{
	GtkWidget *progress;
	char *command;

	if (acme->dialog_timeout != 0)
	{
		gtk_timeout_remove (acme->dialog_timeout);
		acme->dialog_timeout = 0;
	}

	dialog_init (acme);
	progress = glade_xml_get_widget (acme->xml, "progressbar");
	gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (progress),
			(double) 0);
	gtk_widget_set_sensitive (progress, FALSE);

	acme_image_set (acme, ICON_EJECT);
	dialog_show (acme);

	command = gconf_client_get_string (acme->conf_client,
			GCONF_MISC_DIR "/eject_command", NULL);
	if ((command != NULL) && (strcmp (command, "") != 0))
		execute (command, TRUE);
	else
		execute ("eject", TRUE);

	gtk_widget_set_sensitive (progress, TRUE);
}

#ifdef USE_FBLEVEL
static void
do_brightness_action (Acme *acme, int type)
{
	GtkWidget *progress;
	int level;

	if (acme->levobj == NULL)
		return;

	if (acme->dialog_timeout != 0)
	{
		gtk_timeout_remove (acme->dialog_timeout);
		acme->dialog_timeout = 0;
	}

	level = acme_fblevel_get_level (acme->levobj);

	dialog_init (acme);
	acme_image_set (acme, ICON_BRIGHT);

	switch (type) {
	case BRIGHT_DOWN_KEY:
		acme_fblevel_set_level (acme->levobj, level - 1);
		break;
	case BRIGHT_UP_KEY:
		acme_fblevel_set_level (acme->levobj, level + 1);
		break;
	}

	level = acme_fblevel_get_level (acme->levobj);

	progress = glade_xml_get_widget (acme->xml, "progressbar");
	gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (progress),
			(double) level / 15);

	dialog_show (acme);
}
#endif

static void
do_sound_action (Acme *acme, int type)
{
	GtkWidget *progress;
	gboolean muted;
	int vol;
	int vol_step;

	if (acme->volobj == NULL)
		return;

	vol_step = gconf_client_get_int (acme->conf_client,
			GCONF_MISC_DIR "/volume_step", NULL);

	if (vol_step == 0)
		vol_step = VOLUME_STEP;

	if (acme->dialog_timeout != 0)
	{
		gtk_timeout_remove (acme->dialog_timeout);
		acme->dialog_timeout = 0;
	}

	vol = acme_volume_get_volume (acme->volobj);
	muted = acme_volume_get_mute (acme->volobj);

	switch (type) {
	case MUTE_KEY:
		acme_volume_mute_toggle(acme->volobj);
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

	muted = acme_volume_get_mute(acme->volobj);
	dialog_init (acme);
	acme_image_set (acme, muted ? ICON_MUTED : ICON_LOUD);

	vol = acme_volume_get_volume (acme->volobj);

	progress = glade_xml_get_widget (acme->xml, "progressbar");
	gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (progress),
			(double) vol / 100);

	dialog_show (acme);
}

static void
do_action (Acme *acme, int type)
{
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
		execute ("nautilus", FALSE);
		break;
	case SEARCH_KEY:
		execute ("gnome-search-tool", FALSE);
		break;
	case EMAIL_KEY:
		do_mail_action (acme);
		break;
	case SLEEP_KEY:
		do_sleep_action ("apm", "xset dpms force off");
		break;
	case SCREENSAVER_KEY:
		execute ("xscreensaver-command -lock", FALSE);
		break;
	case HELP_KEY:
		do_help_action (acme);
		break;
	case WWW_KEY:
		do_www_action (acme, NULL);
		break;
#ifdef USE_FBLEVEL
	case BRIGHT_DOWN_KEY:
	case BRIGHT_UP_KEY:
		do_brightness_action (acme, type);
		break;
#endif
	default:
		g_assert_not_reached ();
	}
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

	keycode = xev->xkey.keycode;
	state = xev->xkey.state;

	for (i = 0; i < PLAY_KEY; i++)
	{
		if (keys[i].key == NULL)
			continue;

		if (keys[i].key->keycode == keycode &&
				(state & USED_MODS) == keys[i].key->state)
		{
			switch (keys[i].key_type) {
			case VOLUME_DOWN_KEY:
			case VOLUME_UP_KEY:
#ifdef USE_FBLEVEL
			case BRIGHT_DOWN_KEY:
			case BRIGHT_UP_KEY:
#endif
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

			do_action (acme, keys[i].key_type);
			return GDK_FILTER_REMOVE;
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
	acme->xml = NULL;

	acme->conf_client = client;
	gconf_client_add_dir (acme->conf_client,
		GCONF_BINDING_DIR,
		GCONF_CLIENT_PRELOAD_ONELEVEL,
		NULL);

	init_screens (acme);
	init_kbd (acme);
	acme->dialog_timeout = 0;

	/* initialise Volume handler */
	acme->volobj = acme_volume_new();

#ifdef USE_FBLEVEL
	/* initialise Frame Buffer level handler */
	if (acme_fblevel_is_powerbook () != FALSE)
	{
		acme->levobj = acme_fblevel_new();
		if (acme->levobj == NULL)
			fblevel_problem_cb ();
	}
#endif

	/* Start filtering the events */
	for (l = acme->screens; l != NULL; l = l->next)
		gdk_window_add_filter (gdk_screen_get_root_window (l->data),
			acme_filter_events, (gpointer) acme);
}

