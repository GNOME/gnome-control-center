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

#include <glib/gstrfuncs.h>
#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <gconf/gconf-client.h>

#include <string.h>

#include <libgnome/gnome-i18n.h>

#include <libxklavier/xklavier.h>
#include <libxklavier/xklavier_config.h>
#include <libgswitchit/gswitchit_xkb_config.h>

#include "gnome-settings-keyboard-xkb.h"
#include "gnome-settings-daemon.h"

static GSwitchItXkbConfig gswic;
static gboolean initedOk;

static PostActivationCallback paCallback = NULL;
static void *paCallbackUserData = NULL;

static const char DISABLE_XMM_WARNING_KEY[] =
    "/desktop/gnome/peripherals/keyboard/disable_xmm_and_xkb_warning";

typedef enum {
	RESPONSE_USE_X,
	RESPONSE_USE_GNOME
} SysConfigChangedMsgResponse;

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
							      "Probably internal X server problem.\n\nX server version data:\n%s\n%d\n%s\n"
							      "If you report this situation as a bug, please include:\n"
							      "- The result of <b>xprop -root | grep XKB</b>\n"
							      "- The result of <b>gconftool-2 -R /desktop/gnome/peripherals/keyboard/xkb</b>"),
							     vendor,
							     release,
							     badXFree430Release
							     ?
							     _
							     ("You are using XFree 4.3.0.\n"
							      "There are known problems with complex XKB configurations.\n"
							      "Try using simpler configuration or taking more fresh version of XFree software.")
							     : "");
	g_signal_connect (msg, "response",
			  G_CALLBACK (gtk_widget_destroy), NULL);
	gtk_widget_show (msg);
}

static void
apply_settings (void)
{
	GConfClient *confClient;

	if (!initedOk)
		return;

	confClient = gconf_client_get_default ();
	GSwitchItXkbConfigInit (&gswic, confClient);
	g_object_unref (confClient);
	GSwitchItXkbConfigLoad (&gswic);

	if (gswic.overrideSettings) {
		/* initialization - from the system settings */
		GSwitchItXkbConfigLoadInitial (&gswic);
		gswic.overrideSettings = FALSE;
		GSwitchItXkbConfigSave (&gswic);
	} else {
		if (GSwitchItXkbConfigActivate (&gswic)) {
			if (paCallback != NULL) {
				(*paCallback) (paCallbackUserData);
			}
		} else {
			g_warning
			    ("Could not activate the XKB configuration");
			activation_error ();
		}
	}

	GSwitchItXkbConfigTerm (&gswic);
}

static void
gnome_settings_keyboard_xkb_sysconfig_changed_response (GtkDialog * dialog,
							SysConfigChangedMsgResponse
							what2do,
							GSwitchItXkbConfig
							* pgswicNow)
{
	switch (what2do) {
	case RESPONSE_USE_X:
		GSwitchItXkbConfigSave (pgswicNow);
		break;
	case RESPONSE_USE_GNOME:
		/* Do absolutely nothing - just keep things the way they are */
		break;
	}
	gtk_widget_destroy (GTK_WIDGET (dialog));
	GSwitchItXkbConfigTerm (pgswicNow);
	g_free (pgswicNow);
}

static void
gnome_settings_keyboard_xkb_analyze_sysconfig (void)
{
	GConfClient *confClient;
	GSwitchItXkbConfig gswicWas, *pgswicNow;

	if (!initedOk)
		return;
	pgswicNow = g_new (GSwitchItXkbConfig, 1);
	confClient = gconf_client_get_default ();
	GSwitchItXkbConfigInit (&gswicWas, confClient);
	GSwitchItXkbConfigInit (pgswicNow, confClient);
	g_object_unref (confClient);
	GSwitchItXkbConfigLoadSysBackup (&gswicWas);
	GSwitchItXkbConfigLoadInitial (pgswicNow);

	/* config was changed!!! */
	if (g_slist_length (gswicWas.layouts) &&
	    !GSwitchItXkbConfigEquals (pgswicNow, &gswicWas)) {
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
				  pgswicNow);
		gtk_widget_show (msg);
	}
	GSwitchItXkbConfigSaveSysBackup (pgswicNow);
	GSwitchItXkbConfigTerm (&gswicWas);
}

static void
gnome_settings_keyboard_xkb_chk_lcl_xmm_response (GtkDialog * dlg,
						  gint response)
{
	GConfClient *gcc = gconf_client_get_default ();
	switch (response) {
	case GTK_RESPONSE_OK:
		gconf_client_set_bool (gcc, DISABLE_XMM_WARNING_KEY, TRUE,
				       NULL);
		break;
	}
	g_object_unref (G_OBJECT (gcc));
	gtk_widget_destroy (GTK_WIDGET (dlg));
}

static void
gnome_settings_keyboard_xkb_chk_lcl_xmm (void)
{
	GConfClient *gcc = gconf_client_get_default ();
	gboolean disableWarning =
	    gconf_client_get_bool (gcc, DISABLE_XMM_WARNING_KEY, NULL);
	GDir *homeDir;
	G_CONST_RETURN gchar *fname;
	g_object_unref (G_OBJECT (gcc));
	if (disableWarning)
		return;

	homeDir = g_dir_open (g_get_home_dir (), 0, NULL);
	if (homeDir == NULL)
		return;
	while ((fname = g_dir_read_name (homeDir)) != NULL)
		if (strlen (fname) >= 8
		    && !g_ascii_strncasecmp (fname, ".xmodmap", 8)) {
			GtkWidget *msg =
			    gtk_message_dialog_new_with_markup (NULL, 0,
								GTK_MESSAGE_WARNING,
								GTK_BUTTONS_OK,
								_
								("You have a keyboard remapping file (%s) in your home directory whose contents will now be ignored."
								 " You can use the keyboard preferences to restore them."),
				fname);
			g_signal_connect (msg, "response",
					  G_CALLBACK
					  (gnome_settings_keyboard_xkb_chk_lcl_xmm_response),
					  NULL);
			gtk_widget_show (msg);
			break;
		}
	g_dir_close (homeDir);
}

void
gnome_settings_keyboard_xkb_set_post_activation_callback
    (PostActivationCallback fun, void *userData)
{
	paCallback = fun;
	paCallbackUserData = userData;
}

void
gnome_settings_keyboard_xkb_init (GConfClient * client)
{
	if (!XklInit (GDK_DISPLAY ())) {
		initedOk = TRUE;
		XklBackupNamesProp ();
		gnome_settings_keyboard_xkb_analyze_sysconfig ();
		gnome_settings_keyboard_xkb_chk_lcl_xmm ();
		gnome_settings_daemon_register_callback
		    ("/desktop/gnome/peripherals/keyboard/xkb",
		     (KeyCallbackFunc) apply_settings);
	}
}

void
gnome_settings_keyboard_xkb_load (GConfClient * client)
{
	apply_settings ();
}
