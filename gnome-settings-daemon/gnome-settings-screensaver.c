/* -*- mode: c; style: linux -*- */

/* gnome-settings-screensaver.c
 *
 * Copyright (C) 2002 Sun Microsystems, Inc.
 *
 * Written by Jacob Berkman <jacob@ximian.com>
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

#include <config.h>

#include <libgnome/gnome-i18n.h>
#include "gnome-settings-screensaver.h"

#include <gtk/gtkcheckbutton.h>
#include <gtk/gtkbox.h>
#include <gtk/gtkmessagedialog.h>

#define START_SCREENSAVER_KEY   "/apps/gnome_settings_daemon/screensaver/start_screensaver"
#define SHOW_STARTUP_ERRORS_KEY "/apps/gnome_settings_daemon/screensaver/show_startup_errors"
#define XSCREENSAVER_COMMAND    "xscreensaver -nosplash"

void
gnome_settings_screensaver_init (GConfClient *client)
{
	/*
	 * do nothing.
	 *
	 * our settings only apply to startup, and the screensaver
	 * settings are all in xscreensaver and not gconf.
	 *
	 * we could have xscreensaver-demo run gconftool-2 directly,
	 * and start / stop xscreensaver here
	 *
	 */
}

static void
key_toggled_cb (GtkWidget *toggle, gpointer data)
{
	GConfClient *client;

	client = gconf_client_get_default ();
	gconf_client_set_bool (client, 
			       SHOW_STARTUP_ERRORS_KEY,
			       gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (toggle))
			       ? 0 : 1,
			       NULL);
}

void
gnome_settings_screensaver_load (GConfClient *client)
{
	GError *gerr = NULL;
	gboolean start_screensaver;
	gboolean show_error;
	GtkWidget *dialog, *toggle;
 
	start_screensaver = gconf_client_get_bool (client, START_SCREENSAVER_KEY, NULL);

	if (!start_screensaver)
		return;

	if (g_spawn_command_line_async (XSCREENSAVER_COMMAND, &gerr))
		return;
	
	show_error = gconf_client_get_bool (client, SHOW_STARTUP_ERRORS_KEY, NULL);
	if (!show_error) {
		g_error_free (gerr);
		return;
	}

	dialog = gtk_message_dialog_new (NULL,
					 0, 
					 GTK_MESSAGE_ERROR,
					 GTK_BUTTONS_OK,
					 _("There was an error starting up the screensaver:\n\n"
					 "%s\n\n"
					 "Screensaver functionality will not work in this session."),
					 gerr->message);

	g_signal_connect (dialog, "response",
			  G_CALLBACK (gtk_widget_destroy),
			  NULL);

	toggle = gtk_check_button_new_with_mnemonic (
		_("_Do not show this message again"));
	gtk_widget_show (toggle);

	if (gconf_client_key_is_writable (client, SHOW_STARTUP_ERRORS_KEY, NULL))
		g_signal_connect (toggle, "toggled",
				  G_CALLBACK (key_toggled_cb),
				  NULL);
	else
		gtk_widget_set_sensitive (toggle, FALSE);
					  
	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox), 
			    toggle,
			    FALSE, FALSE, 0);

	gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);		

	gtk_widget_show (dialog);
}

