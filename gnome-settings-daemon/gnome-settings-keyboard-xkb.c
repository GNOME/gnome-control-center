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

//#include <glib/gstrfuncs.h>
#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <gconf/gconf-client.h>
#include "gnome-settings-xmodmap.h"

#include <string.h>
#include <time.h>

#include <libgnome/gnome-i18n.h>

#include <libxklavier/xklavier.h>
#include <libxklavier/xklavier_config.h>
#include <libgswitchit/gswitchit_config.h>

#include "gnome-settings-keyboard-xkb.h"
#include "gnome-settings-daemon.h"

static GSwitchItConfig currentConfig;
static GSwitchItKbdConfig currentKbdConfig;

/* never terminated */
static GSwitchItKbdConfig initialSysKbdConfig;

static gboolean initedOk;

static PostActivationCallback paCallback = NULL;
static void *paCallbackUserData = NULL;

static const char DISABLE_XMM_WARNING_KEY[] =
    "/desktop/gnome/peripherals/keyboard/disable_xmm_and_xkb_warning";
static const char KNOWN_FILES_KEY[] =
    "/desktop/gnome/peripherals/keyboard/general/known_file_list";

typedef enum {
	RESPONSE_USE_X,
	RESPONSE_USE_GNOME
} SysConfigChangedMsgResponse;

#define noGSDKX

#ifdef GSDKX
static FILE *logfile;

static void
gnome_settings_keyboard_log_appender (const char file[], const char function[],
                      int level, const char format[], va_list args)
{
        time_t now = time (NULL);
        fprintf (logfile, "[%08ld,%03d,%s:%s/] \t", now,
               level, file, function);
        vfprintf (logfile, format, args);
	fflush(logfile);
}
#endif

static void
activation_error (void)
{
	char *vendor = ServerVendor (GDK_DISPLAY ());
	int release = VendorRelease (GDK_DISPLAY ());
	gboolean badXFree430Release = (!strcmp (vendor,
						"The XFree86 Project, Inc"))
	    && (release / 100000 == 403);

	GtkWidget *msg = gtk_message_dialog_new_with_markup (NULL,
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
							      "- The result of %s\n"
							      "- The result of %s"),
							     vendor,
							     release,
							     badXFree430Release
							     ?
							     _
							     ("You are using XFree 4.3.0.\n"
							      "There are known problems with complex XKB configurations.\n"
							      "Try using simpler configuration or taking more fresh version of XFree software.")
							     : "",
							     "<b>xprop -root | grep XKB</b>",
							     "<b>gconftool-2 -R /desktop/gnome/peripherals/keyboard/kbd</b>");
	g_signal_connect (msg, "response",
			  G_CALLBACK (gtk_widget_destroy), NULL);
	gtk_widget_show (msg);
}

static void
apply_settings (void)
{
	if (!initedOk)
		return;

	GSwitchItConfigLoadFromGConf (&currentConfig);
	/* again, probably it would be nice to compare things 
	   before activating them */
	GSwitchItConfigActivate (&currentConfig);
}

static void
apply_xkb_settings (void)
{
	GConfClient *confClient;
	GSwitchItKbdConfig currentSysKbdConfig;

	if (!initedOk)
		return;

	confClient = gconf_client_get_default ();
	GSwitchItKbdConfigInit (&currentSysKbdConfig, confClient);
	g_object_unref (confClient);

	GSwitchItKbdConfigLoadFromGConf (&currentKbdConfig);

	if (currentKbdConfig.overrideSettings) {
		/* initialization - from the system settings */
		GSwitchItKbdConfigSaveToGConf (&initialSysKbdConfig);
	} else {
		GSwitchItKbdConfigLoadFromXCurrent (&currentSysKbdConfig);
		/* Activate - only if different! */
		if (!GSwitchItKbdConfigEquals (&currentKbdConfig, &currentSysKbdConfig))
		{
			if (GSwitchItKbdConfigActivate (&currentKbdConfig)) {
				if (paCallback != NULL) {
					(*paCallback) (paCallbackUserData);
				}
			} else {
				g_warning
				    ("Could not activate the XKB configuration");
				activation_error ();
			}
		} else
			XklDebug (100, "Actual KBD configuration was not changed: redundant notification\n");
	}

	GSwitchItKbdConfigTerm (&currentSysKbdConfig);
}

static void
gnome_settings_keyboard_xkb_sysconfig_changed_response (GtkDialog * dialog,
							SysConfigChangedMsgResponse
							what2do)
{
	switch (what2do) {
	case RESPONSE_USE_X:
		GSwitchItKbdConfigSaveToGConf (&initialSysKbdConfig);
		break;
	case RESPONSE_USE_GNOME:
		/* Do absolutely nothing - just keep things the way they are */
		break;
	}
	gtk_widget_destroy (GTK_WIDGET (dialog));
}

static void
gnome_settings_keyboard_xkb_analyze_sysconfig (void)
{
	GConfClient *confClient;
	GSwitchItKbdConfig backupGConfKbdConfig;
	gboolean isConfigChanged;

	if (!initedOk)
		return;
	confClient = gconf_client_get_default ();
	GSwitchItKbdConfigInit (&backupGConfKbdConfig, confClient);
	GSwitchItKbdConfigInit (&initialSysKbdConfig, confClient);
	g_object_unref (confClient);
	GSwitchItKbdConfigLoadFromGConfBackup (&backupGConfKbdConfig);
	GSwitchItKbdConfigLoadFromXInitial (&initialSysKbdConfig);
	initialSysKbdConfig.overrideSettings = FALSE;

	isConfigChanged = g_slist_length (backupGConfKbdConfig.layouts) &&
	    !GSwitchItKbdConfigEquals (&initialSysKbdConfig, &backupGConfKbdConfig);
	/* config was changed!!! */
	if (isConfigChanged) {
		GtkWidget *msg = gtk_message_dialog_new_with_markup (NULL,
								     0,
								     GTK_MESSAGE_INFO,
								     GTK_BUTTONS_NONE,
/* !! temporary one */
								     _
								     ("The X system keyboard settings differ from your current GNOME "
								      "keyboard settings.  Which set would you like to use?"));
		gtk_dialog_add_buttons (GTK_DIALOG (msg),
					_("Use X settings"),
					RESPONSE_USE_X,
					_("Use GNOME settings"),
					RESPONSE_USE_GNOME, NULL);
		gtk_dialog_set_default_response (GTK_DIALOG (msg),
						 RESPONSE_USE_GNOME);
		g_signal_connect (msg, "response",
				  G_CALLBACK
				  (gnome_settings_keyboard_xkb_sysconfig_changed_response),
				  NULL);
		gtk_widget_show (msg);
	}
	GSwitchItKbdConfigSaveToGConfBackup (&initialSysKbdConfig);
	GSwitchItKbdConfigTerm (&backupGConfKbdConfig);
}

static gboolean 
gnome_settings_chk_file_list (void)
{
	GDir *homeDir;
	G_CONST_RETURN gchar *fname;
	GSList *file_list = NULL;
	GSList *last_login_file_list = NULL;
	GSList *tmp = NULL;
	GSList *tmp_l = NULL;
	gboolean new_file_exist = FALSE;
	GConfClient *confClient = gconf_client_get_default ();

	homeDir = g_dir_open (g_get_home_dir (), 0, NULL);
	while ((fname = g_dir_read_name (homeDir)) != NULL) {
		if (g_strrstr (fname, "modmap")) {
			file_list = g_slist_append (file_list, g_strdup (fname));
		}
	}

	last_login_file_list = gconf_client_get_list (confClient, KNOWN_FILES_KEY, GCONF_VALUE_STRING, NULL);

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
			}
			else
				tmp_l = tmp_l->next;
		}
		if (new_file_exist)
			break;
		else
			tmp = tmp->next;
	}

	if (new_file_exist)
		gconf_client_set_list (confClient, KNOWN_FILES_KEY, GCONF_VALUE_STRING, file_list, NULL);

	g_slist_foreach (file_list, (GFunc) g_free, NULL);
	g_slist_free (file_list);

	g_slist_foreach (last_login_file_list, (GFunc) g_free, NULL);
	g_slist_free (last_login_file_list);

	g_object_unref (G_OBJECT (confClient));

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
    (PostActivationCallback fun, void *userData)
{
	paCallback = fun;
	paCallbackUserData = userData;
}

static GdkFilterReturn
gnome_settings_keyboard_xkb_evt_filter (GdkXEvent * xev,
                           GdkEvent * event)
{
        XEvent *xevent = (XEvent *) xev;
        XklFilterEvents (xevent);
        return GDK_FILTER_CONTINUE;
}

void
gnome_settings_keyboard_xkb_init (GConfClient * client)
{
#ifdef GSDKX
	XklSetDebugLevel (200);
	logfile = fopen ("/tmp/gsdkx.log", "a");
        XklSetLogAppender (gnome_settings_keyboard_log_appender);
#endif

	if (!XklInit (GDK_DISPLAY ())) {
		initedOk = TRUE;
		XklBackupNamesProp ();
		gnome_settings_keyboard_xkb_analyze_sysconfig ();
		gnome_settings_keyboard_xkb_chk_lcl_xmm ();

		gnome_settings_daemon_register_callback
		    (GSWITCHIT_CONFIG_DIR,
		     (KeyCallbackFunc) apply_settings);

		gnome_settings_daemon_register_callback
		    (GSWITCHIT_KBD_CONFIG_DIR,
		     (KeyCallbackFunc) apply_xkb_settings);

		gdk_window_add_filter (NULL,
				       (GdkFilterFunc) gnome_settings_keyboard_xkb_evt_filter,
				       NULL);
		gdk_window_add_filter (gdk_get_default_root_window(),
				       (GdkFilterFunc) gnome_settings_keyboard_xkb_evt_filter,
				       NULL);
		XklStartListen (XKLL_MANAGE_LAYOUTS | XKLL_MANAGE_WINDOW_STATES);
	}
}

void
gnome_settings_keyboard_xkb_load (GConfClient * client)
{
	GConfClient *confClient;
	confClient = gconf_client_get_default ();

	GSwitchItConfigInit (&currentConfig, confClient);
	apply_settings ();

	GSwitchItKbdConfigInit (&currentKbdConfig, confClient);
	apply_xkb_settings ();

	g_object_unref (confClient);
}
