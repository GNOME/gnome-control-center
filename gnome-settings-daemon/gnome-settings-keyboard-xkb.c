/* -*- mode: c; style: linux -*- */

/* gnome-settings-keyboard-xkb.c
 *
 * Copyright © 2001 Udaltsoft
 *
 * Written by Sergey V. Oudaltsov <svu@users.sourceforge.net>
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <glib/gi18n.h>
#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <gconf/gconf-client.h>
#include "gnome-settings-xmodmap.h"

#include <string.h>
#include <time.h>

#include <libgnomekbd/gkbd-config-registry.h>
#include <libgnomekbd/gkbd-desktop-config.h>
#include <libgnomekbd/gkbd-keyboard-config.h>

#include "gnome-settings-keyboard-xkb.h"
#include "gnome-settings-daemon.h"

XklEngine *xkl_engine;

static GkbdDesktopConfig current_config;
static GkbdKeyboardConfig current_kbd_config;

/* never terminated */
static GkbdKeyboardConfig initial_sys_kbd_config;

static gboolean inited_ok;

static PostActivationCallback pa_callback = NULL;
static void *pa_callback_user_data = NULL;

static const char KNOWN_FILES_KEY[] =
    "/desktop/gnome/peripherals/keyboard/general/known_file_list";
static const char DISABLE_SYSCONF_CHANGED_WARNING_KEY[] =
    "/desktop/gnome/peripherals/keyboard/general/disable_sysconfig_changed_warning";

typedef enum {
	RESPONSE_USE_X,
	RESPONSE_USE_GNOME
} SysConfigChangedMsgResponse;

#define noGSDKX

#ifdef GSDKX
static FILE *logfile;

static void
gnome_settings_keyboard_log_appender (const char file[],
				      const char function[], int level,
				      const char format[], va_list args)
{
	time_t now = time (NULL);
	fprintf (logfile, "[%08ld,%03d,%s:%s/] \t", now,
		 level, file, function);
	vfprintf (logfile, format, args);
	fflush (logfile);
}
#endif

static void
activation_error (void)
{
	char const *vendor = ServerVendor (GDK_DISPLAY ());
	int release = VendorRelease (GDK_DISPLAY ());
	gboolean badXFree430Release = (vendor != NULL)
	    && (0 == strcmp (vendor, "The XFree86 Project, Inc"))
	    && (release / 100000 == 403);

	GtkWidget *dialog;

	/* VNC viewers will not work, do not barrage them with warnings */
	if (NULL != vendor && NULL != strstr (vendor, "VNC"))
		return;

	dialog = gtk_message_dialog_new_with_markup (NULL,
						     0,
						     GTK_MESSAGE_ERROR,
						     GTK_BUTTONS_CLOSE,
						     _
						     ("Error activating XKB configuration.\n"
						      "It can happen under various circumstances:\n"
						      "- a bug in libxklavier library\n"
						      "- a bug in X server (xkbcomp, xmodmap utilities)\n"
						      "- X server with incompatible libxkbfile implementation\n\n"
						      "X server version data:\n%s\n%d\n%s\n"
						      "If you report this situation as a bug, please include:\n"
						      "- The result of <b>%s</b>\n"
						      "- The result of <b>%s</b>"),
						     vendor,
						     release,
						     badXFree430Release
						     ?
						     _
						     ("You are using XFree 4.3.0.\n"
						      "There are known problems with complex XKB configurations.\n"
						      "Try using a simpler configuration or taking a fresher version of XFree software.")
						     : "",
						     "xprop -root | grep XKB",
						     "gconftool-2 -R /desktop/gnome/peripherals/keyboard/kbd");
	g_signal_connect (dialog, "response",
			  G_CALLBACK (gtk_widget_destroy), NULL);
	gnome_settings_delayed_show_dialog (dialog);
}

static void
apply_settings (void)
{
	if (!inited_ok)
		return;

	gkbd_desktop_config_load_from_gconf (&current_config);
	/* again, probably it would be nice to compare things 
	   before activating them */
	gkbd_desktop_config_activate (&current_config);
}

static void
apply_xkb_settings (void)
{
	GConfClient *conf_client;
	GkbdKeyboardConfig current_sys_kbd_config;

	if (!inited_ok)
		return;

	conf_client = gnome_settings_get_config_client ();
	gkbd_keyboard_config_init (&current_sys_kbd_config, conf_client,
				   xkl_engine);

	gkbd_keyboard_config_load_from_gconf (&current_kbd_config,
					      &initial_sys_kbd_config);

	gkbd_keyboard_config_load_from_x_current (&current_sys_kbd_config, NULL);
	/* Activate - only if different! */
	if (!gkbd_keyboard_config_equals
	    (&current_kbd_config, &current_sys_kbd_config)) {
		if (gkbd_keyboard_config_activate (&current_kbd_config)) {
			gkbd_keyboard_config_save_to_gconf_backup
			    (&initial_sys_kbd_config);
			if (pa_callback != NULL) {
				(*pa_callback) (pa_callback_user_data);
			}
		} else {
			g_warning
			    ("Could not activate the XKB configuration");
			activation_error ();
		}
	} else
		xkl_debug (100,
			   "Actual KBD configuration was not changed: redundant notification\n");

	gkbd_keyboard_config_term (&current_sys_kbd_config);
}

static void
gnome_settings_keyboard_xkb_sysconfig_changed_response (GtkDialog * dialog,
							SysConfigChangedMsgResponse
							what2do)
{
	GConfClient *conf_client;
	GkbdKeyboardConfig empty_kbd_config;
	gboolean dont_show_again =
	    gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON
					  (g_object_get_data
					   (G_OBJECT (dialog),
					    "chkDontShowAgain")));

	conf_client = gnome_settings_get_config_client ();

	switch (what2do) {
	case RESPONSE_USE_X:
		gkbd_keyboard_config_init (&empty_kbd_config, conf_client,
					   xkl_engine);
		gkbd_keyboard_config_save_to_gconf (&empty_kbd_config);
		break;
	case RESPONSE_USE_GNOME:
		/* Do absolutely nothing - just keep things the way they are */
		break;
	}

	if (dont_show_again)
		gconf_client_set_bool (conf_client,
				       DISABLE_SYSCONF_CHANGED_WARNING_KEY,
				       TRUE, NULL);

	gtk_widget_destroy (GTK_WIDGET (dialog));
}

static void
gnome_settings_keyboard_xkb_analyze_sysconfig (void)
{
	GConfClient *conf_client;
	GkbdKeyboardConfig backup_gconf_kbd_config;
	gboolean is_config_changed, dont_show;

	if (!inited_ok)
		return;
	conf_client = gnome_settings_get_config_client ();
	gkbd_keyboard_config_init (&backup_gconf_kbd_config, conf_client,
				   xkl_engine);
	gkbd_keyboard_config_init (&initial_sys_kbd_config, conf_client,
				   xkl_engine);
	dont_show =
	    gconf_client_get_bool (conf_client,
				   DISABLE_SYSCONF_CHANGED_WARNING_KEY,
				   NULL);
	gkbd_keyboard_config_load_from_gconf_backup
	    (&backup_gconf_kbd_config);
	gkbd_keyboard_config_load_from_x_initial (&initial_sys_kbd_config, NULL);

	is_config_changed =
	    g_slist_length (backup_gconf_kbd_config.layouts_variants)
	    && !gkbd_keyboard_config_equals (&initial_sys_kbd_config,
					     &backup_gconf_kbd_config);

	/* config was changed!!! */
	if (is_config_changed) {
		if (dont_show) {
			g_warning
			    ("The system configuration changed - but we remain silent\n");
		} else {
			GtkWidget *chk_dont_show_again =
			    gtk_check_button_new_with_mnemonic (_
								("Do _not show this warning again"));
			GtkWidget *align_dont_show_again =
			    gtk_alignment_new (0.5, 0.5, 0, 0);
			GtkWidget *msg;

			char *gnome_settings =
			    gkbd_keyboard_config_to_string
			    (&backup_gconf_kbd_config);
			char *system_settings =
			    gkbd_keyboard_config_to_string
			    (&initial_sys_kbd_config);

			msg = gtk_message_dialog_new_with_markup (NULL, 0, GTK_MESSAGE_INFO, GTK_BUTTONS_NONE,	/* !! temporary one */
								  _
								  ("<b>The X system keyboard settings differ from your current GNOME keyboard settings.</b>\n\n"
								   "Expected was %s, but the the following settings were found: %s.\n\n"
								   "Which set would you like to use?"),
								  gnome_settings,
								  system_settings);

			g_free (gnome_settings);
			g_free (system_settings);

			gtk_window_set_icon_name (GTK_WINDOW (msg),
						  "gnome-dev-keyboard");

			gtk_container_set_border_width (GTK_CONTAINER
							(align_dont_show_again),
							12);
			gtk_container_add (GTK_CONTAINER
					   (align_dont_show_again),
					   chk_dont_show_again);
			gtk_container_add (GTK_CONTAINER
					   ((GTK_DIALOG (msg))->vbox),
					   align_dont_show_again);

			gtk_dialog_add_buttons (GTK_DIALOG (msg),
						_("Use X settings"),
						RESPONSE_USE_X,
						_("Keep GNOME settings"),
						RESPONSE_USE_GNOME, NULL);
			gtk_dialog_set_default_response (GTK_DIALOG (msg),
							 RESPONSE_USE_GNOME);
			g_object_set_data (G_OBJECT (msg),
					   "chkDontShowAgain",
					   chk_dont_show_again);
			g_signal_connect (msg, "response",
					  G_CALLBACK
					  (gnome_settings_keyboard_xkb_sysconfig_changed_response),
					  NULL);
			gnome_settings_delayed_show_dialog (msg);
		}
	}
	gkbd_keyboard_config_term (&backup_gconf_kbd_config);
}

static gboolean
gnome_settings_chk_file_list (void)
{
	GDir *home_dir;
	G_CONST_RETURN gchar *fname;
	GSList *file_list = NULL;
	GSList *last_login_file_list = NULL;
	GSList *tmp = NULL;
	GSList *tmp_l = NULL;
	gboolean new_file_exist = FALSE;
	GConfClient *conf_client =
	    gnome_settings_get_config_client ();

	home_dir = g_dir_open (g_get_home_dir (), 0, NULL);
	while ((fname = g_dir_read_name (home_dir)) != NULL) {
		if (g_strrstr (fname, "modmap")) {
			file_list =
			    g_slist_append (file_list, g_strdup (fname));
		}
	}
	g_dir_close (home_dir);

	last_login_file_list =
	    gconf_client_get_list (conf_client, KNOWN_FILES_KEY,
				   GCONF_VALUE_STRING, NULL);

	/* Compare between the two file list, currently available modmap files
	   and the files available in the last log in */
	tmp = file_list;
	while (tmp != NULL) {
		tmp_l = last_login_file_list;
		new_file_exist = TRUE;
		while (tmp_l != NULL) {
			if (strcmp (tmp->data, tmp_l->data) == 0) {
				new_file_exist = FALSE;
				break;
			} else
				tmp_l = tmp_l->next;
		}
		if (new_file_exist)
			break;
		else
			tmp = tmp->next;
	}

	if (new_file_exist)
		gconf_client_set_list (conf_client, KNOWN_FILES_KEY,
				       GCONF_VALUE_STRING, file_list,
				       NULL);

	g_slist_foreach (file_list, (GFunc) g_free, NULL);
	g_slist_free (file_list);

	g_slist_foreach (last_login_file_list, (GFunc) g_free, NULL);
	g_slist_free (last_login_file_list);

	return new_file_exist;

}

static void
gnome_settings_keyboard_xkb_chk_lcl_xmm (void)
{
	if (gnome_settings_chk_file_list ())
		gnome_settings_modmap_dialog_call ();
	gnome_settings_load_modmap_files ();


}

void
 gnome_settings_keyboard_xkb_set_post_activation_callback
    (PostActivationCallback fun, void *user_data) {
	pa_callback = fun;
	pa_callback_user_data = user_data;
}

static GdkFilterReturn
gnome_settings_keyboard_xkb_evt_filter (GdkXEvent * xev, GdkEvent * event)
{
	XEvent *xevent = (XEvent *) xev;
	xkl_engine_filter_events (xkl_engine, xevent);
	return GDK_FILTER_CONTINUE;
}

void
gnome_settings_keyboard_xkb_init (GConfClient * client)
{
	GObject *reg = NULL;
#ifdef GSDKX
	xkl_set_debug_level (200);
	logfile = fopen ("/tmp/gsdkx.log", "a");
	xkl_set_log_appender (gnome_settings_keyboard_log_appender);
#endif

	xkl_engine = xkl_engine_get_instance (GDK_DISPLAY ());
	if (xkl_engine) {
		inited_ok = TRUE;
		xkl_engine_backup_names_prop (xkl_engine);
		gnome_settings_keyboard_xkb_analyze_sysconfig ();
		gnome_settings_keyboard_xkb_chk_lcl_xmm ();

		gnome_settings_register_config_callback
		    (GKBD_DESKTOP_CONFIG_DIR,
		     (GnomeSettingsConfigCallback) apply_settings);

		gnome_settings_register_config_callback
		    (GKBD_KEYBOARD_CONFIG_DIR,
		     (GnomeSettingsConfigCallback) apply_xkb_settings);

		gdk_window_add_filter (NULL, (GdkFilterFunc)
				       gnome_settings_keyboard_xkb_evt_filter,
				       NULL);
		xkl_engine_start_listen (xkl_engine,
					 XKLL_MANAGE_LAYOUTS |
					 XKLL_MANAGE_WINDOW_STATES);

		reg =
		    g_object_new (gkbd_config_registry_get_type (), NULL);
	}
}

void
gnome_settings_keyboard_xkb_load (GConfClient * client)
{
	gkbd_desktop_config_init (&current_config, client, xkl_engine);
	apply_settings ();

	gkbd_keyboard_config_init (&current_kbd_config, client,
				   xkl_engine);
	apply_xkb_settings ();
}
