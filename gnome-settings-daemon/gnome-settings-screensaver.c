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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <glib/gi18n.h>
#include <gtk/gtkcheckbutton.h>
#include <gtk/gtkbox.h>
#include <gtk/gtkmessagedialog.h>
#include "gnome-settings-module.h"

#define START_SCREENSAVER_KEY   "/apps/gnome_settings_daemon/screensaver/start_screensaver"
#define SHOW_STARTUP_ERRORS_KEY "/apps/gnome_settings_daemon/screensaver/show_startup_errors"

typedef struct {
	GnomeSettingsModule parent;

	GPid screensaver_pid;
	gboolean start_screensaver;
	gboolean have_gscreensaver;
	gboolean have_xscreensaver;
} GnomeSettingsModuleScreensaver;

typedef struct {
	GnomeSettingsModuleClass parent_class;
} GnomeSettingsModuleScreensaverClass;

static GnomeSettingsModuleRunlevel gnome_settings_module_screensaver_get_runlevel (GnomeSettingsModule *module);
static gboolean gnome_settings_module_screensaver_initialize (GnomeSettingsModule *module, GConfClient *config_client);
static gboolean gnome_settings_module_screensaver_start (GnomeSettingsModule *module);
static gboolean gnome_settings_module_screensaver_stop (GnomeSettingsModule *module);

static void
gnome_settings_module_screensaver_class_init (GnomeSettingsModuleScreensaverClass *klass)
{
	GnomeSettingsModuleClass *module_class;

	module_class = (GnomeSettingsModuleClass *) klass;
	module_class->get_runlevel = gnome_settings_module_screensaver_get_runlevel;
	module_class->initialize = gnome_settings_module_screensaver_initialize;
	module_class->start = gnome_settings_module_screensaver_start;
	module_class->stop = gnome_settings_module_screensaver_stop;
}

static void
gnome_settings_module_screensaver_init (GnomeSettingsModuleScreensaver *module)
{
}

GType
gnome_settings_module_screensaver_get_type (void)
{
	static GType module_type = 0;

	if (!module_type) {
		static const GTypeInfo module_info = {
			sizeof (GnomeSettingsModuleScreensaverClass),
			NULL,		/* base_init */
			NULL,		/* base_finalize */
			(GClassInitFunc) gnome_settings_module_screensaver_class_init,
			NULL,		/* class_finalize */
			NULL,		/* class_data */
			sizeof (GnomeSettingsModuleScreensaver),
			0,		/* n_preallocs */
			(GInstanceInitFunc) gnome_settings_module_screensaver_init,
		};

		module_type = g_type_register_static (GNOME_SETTINGS_TYPE_MODULE,
						      "GnomeSettingsModuleScreensaver",
						      &module_info, 0);
	}

	return module_type;
}

static GnomeSettingsModuleRunlevel
gnome_settings_module_screensaver_get_runlevel (GnomeSettingsModule *module)
{
	return GNOME_SETTINGS_MODULE_RUNLEVEL_SERVICES;
}

static gboolean
gnome_settings_module_screensaver_initialize (GnomeSettingsModule *module, GConfClient *config_client)
{
	gchar *ss_cmd;
	GnomeSettingsModuleScreensaver *module_ss = (GnomeSettingsModuleScreensaver *) module;

	/*
	 * with gnome-screensaver, all settings are loaded internally
	 * from gconf at startup
	 *
	 * with xscreensaver, our settings only apply to startup, and
	 * the screensaver settings are all in xscreensaver and not
	 * gconf.
	 *
	 * we could have xscreensaver-demo run gconftool-2 directly,
	 * and start / stop xscreensaver here
	 *
	 */

	module_ss->start_screensaver = gconf_client_get_bool (config_client, START_SCREENSAVER_KEY, NULL);

	if ((ss_cmd = g_find_program_in_path ("gnome-screensaver"))) {
		module_ss->have_gscreensaver = TRUE;
		g_free (ss_cmd);
	} else
		module_ss->have_gscreensaver = FALSE;

	if ((ss_cmd = g_find_program_in_path ("xscreensaver"))) {
		module_ss->have_xscreensaver = TRUE;
		g_free (ss_cmd);
	} else
		module_ss->have_xscreensaver = FALSE;

	return TRUE;
}

static void
key_toggled_cb (GtkWidget *toggle, gpointer data)
{
	GConfClient *client;
	GnomeSettingsModuleScreensaver *module_ss = data;

	client = gnome_settings_module_get_config_client (module_ss);
	gconf_client_set_bool (client,
			       SHOW_STARTUP_ERRORS_KEY,
			       gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (toggle))
			       ? 0 : 1,
			       NULL);
}

static gboolean
gnome_settings_module_screensaver_start (GnomeSettingsModule *module)
{
	GError *gerr = NULL;
	gboolean show_error;
	GtkWidget *dialog, *toggle;
	gchar *args[3];
	GnomeSettingsModuleScreensaver *module_ss = (GnomeSettingsModuleScreensaver *) module;

	if (!module_ss->start_screensaver)
		return TRUE;

	if (module_ss->have_gscreensaver) {
		args[0] = "gnome-screensaver";
		args[1] = NULL;
	} else if (module_ss->have_xscreensaver) {
		args[0] = "xscreensaver";
		args[1] = "-nosplash";
	} else
		return FALSE;
	args[2] = NULL;

	if (g_spawn_async (g_get_home_dir (), args, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL, &module_ss->screensaver_pid, &gerr))
		return TRUE;

	show_error = gconf_client_get_bool (gnome_settings_module_get_config_client (module),
					    SHOW_STARTUP_ERRORS_KEY, NULL);
	if (!show_error) {
		g_error_free (gerr);
		return FALSE;
	}

	dialog = gtk_message_dialog_new (NULL,
					 0,
					 GTK_MESSAGE_ERROR,
					 GTK_BUTTONS_OK,
					 _("There was an error starting up the screensaver:\n\n"
					 "%s\n\n"
					 "Screensaver functionality will not work in this session."),
					 gerr->message);
	g_error_free (gerr);

	g_signal_connect (dialog, "response",
			  G_CALLBACK (gtk_widget_destroy),
			  NULL);

	toggle = gtk_check_button_new_with_mnemonic (_("_Do not show this message again"));
	gtk_widget_show (toggle);

	if (gconf_client_key_is_writable (gnome_settings_module_get_config_client (module),
					  SHOW_STARTUP_ERRORS_KEY, NULL))
		g_signal_connect (toggle, "toggled",
				  G_CALLBACK (key_toggled_cb),
				  module_ss);
	else
		gtk_widget_set_sensitive (toggle, FALSE);

	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox),
			    toggle,
			    FALSE, FALSE, 0);

	gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);

	gtk_widget_show (dialog);

	return FALSE;
}

static gboolean
gnome_settings_module_screensaver_stop (GnomeSettingsModule *module)
{
	GnomeSettingsModuleScreensaver *module_ss = (GnomeSettingsModuleScreensaver *) module;

	g_spawn_close_pid (module_ss->screensaver_pid);

	return TRUE;
}
