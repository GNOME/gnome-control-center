/*
 * This file is part of libslab.
 *
 * Copyright (c) 2006 Novell, Inc.
 *
 * Libslab is free software; you can redistribute it and/or modify it under the
 * terms of the GNU Lesser General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * Libslab is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for
 * more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with libslab; if not, write to the Free Software Foundation, Inc., 51
 * Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include <libgnome/gnome-desktop-item.h>

#include "app-shell-startup.h"
#include "app-shell.h"
#include "search-bar.h"
#include "slab-section.h"

gint
apss_new_instance_cb (BonoboApplication * app, gint argc, char *argv[], gpointer data)
{
	AppShellData *app_data = (AppShellData *) data;
	SlabSection *section = SLAB_SECTION (app_data->filter_section);
	NldSearchBar *search_bar;
	gboolean visible;

	/* g_message ("new_instance_cb got called\n"); */

	/* Make sure our implementation has not changed */
	g_assert (NLD_IS_SEARCH_BAR (section->contents));
	search_bar = NLD_SEARCH_BAR (section->contents);

	g_object_get (app_data->main_gnome_app, "visible", &visible, NULL);
	if (!visible)
	{
		show_shell (app_data);
	}

	if (argc)		/* if we are passed a valid startup time */
	{
		gchar **results = g_strsplit (argv[0], "_TIME", 0);
		gint lastentry = 0;
		guint32 timestamp;

		while (results[lastentry] != NULL)
			lastentry++;
		timestamp = (guint32) g_strtod (results[lastentry - 1], NULL);
		g_strfreev (results);

		/* gdk_x11_window_move_to_current_desktop(window);  */
		gdk_x11_window_set_user_time (app_data->main_gnome_app->window, timestamp);
	}

	gtk_window_present (GTK_WINDOW (app_data->main_gnome_app));
	gtk_widget_grab_focus (GTK_WIDGET (search_bar));

	return argc;
}

gboolean
apss_already_running (int argc, char *argv[], BonoboApplication ** app,
	const gchar * name, gchar * startup_id)
{
	gchar const *envp[] = { "LANG", NULL };
	char * display_name;
	gchar *serverinfo;
	BonoboAppClient *client;
	Bonobo_RegistrationResult reg_res;

	if (bonobo_init (&argc, argv) == FALSE)
		g_error ("Problem with bonobo_init");
	if (!bonobo_activate ())
		g_error ("Problem with bonobo_activate()");

	display_name = (char *) gdk_display_get_name (gdk_display_get_default ());
	bonobo_activation_set_activation_env_value ("DISPLAY", display_name);
 
	//make this a singleton per display per user
	display_name = g_strconcat (name, display_name, NULL);
	*app = bonobo_application_new (display_name);
	g_free (display_name);

	serverinfo = bonobo_application_create_serverinfo (*app, envp);
	reg_res = bonobo_application_register_unique (*app, serverinfo, &client);
	g_free (serverinfo);

	switch (reg_res)
	{
	case Bonobo_ACTIVATION_REG_ALREADY_ACTIVE:
	{
		gchar *newargv[1];
		int i;
		
		bonobo_object_unref (BONOBO_OBJECT (*app));
		*app = NULL;
		
		newargv[0] = startup_id;
		i = bonobo_app_client_new_instance (client,
			((newargv[0] && newargv[0][0] != '\0') ? 1 : 0), newargv, NULL);
		g_object_unref (client);
		return TRUE;
	}
	case Bonobo_ACTIVATION_REG_SUCCESS:
		return FALSE;
		break;

	case Bonobo_ACTIVATION_REG_ERROR:
	default:
		g_error ("bonobo activation error when registering unique application");
		return FALSE;
	}
}
