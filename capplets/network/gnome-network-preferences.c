
/* gnome-network-preferences.c: network preferences capplet
 *
 * Copyright (C) 2002 Sun Microsystems Inc.
 *
 * Written by: Mark McLoughlin <mark@skynet.ie>
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
#  include <config.h>
#endif

#include <libgnome/libgnome.h>
#include <gconf/gconf-client.h>
#include <glade/glade.h>

#include "capplet-util.h"
#include "gconf-property-editor.h"

#define USE_PROXY_KEY   "/system/http_proxy/use_http_proxy"
#define PROXY_HOST_KEY  "/system/http_proxy/host"
#define PROXY_PORT_KEY  "/system/http_proxy/port"
#define USE_AUTH_KEY    "/system/http_proxy/use_authentication"
#define AUTH_USER_KEY   "/system/http_proxy/authentication_user"
#define AUTH_PASSWD_KEY "/system/http_proxy/authentication_password"

static void
cb_dialog_response (GtkDialog *dialog, gint response_id)
{
	if (response_id == GTK_RESPONSE_HELP) {
		GError *error = NULL;

		/* TODO : get this written */
		gnome_help_display_desktop (NULL,
			"control-center-manual",
			"config-network.xml",
			"CONFIGURATION", &error);
		if (error) {
			g_warning ("help error: %s\n", error->message);
			g_error_free (error);
		}
	} else
		gtk_main_quit ();
}

static void
setup_dialog (GladeXML *dialog)
{
	GConfPropertyEditor *peditor;

	peditor = GCONF_PROPERTY_EDITOR (gconf_peditor_new_boolean (
			NULL, USE_PROXY_KEY, WID ("proxy_toggle"), NULL));
	gconf_peditor_widget_set_guard (peditor, WID ("host_port_table"));

	peditor = GCONF_PROPERTY_EDITOR (gconf_peditor_new_string (
			NULL, PROXY_HOST_KEY, WID ("host_entry"), NULL));

	peditor = GCONF_PROPERTY_EDITOR (gconf_peditor_new_integer (
			NULL, PROXY_PORT_KEY, WID ("port_entry"), NULL));

	peditor = GCONF_PROPERTY_EDITOR (gconf_peditor_new_boolean (
			NULL, USE_AUTH_KEY, WID ("proxy_auth_toggle"), NULL));
	gconf_peditor_widget_set_guard (peditor, WID ("username_password_table"));

	peditor = GCONF_PROPERTY_EDITOR (gconf_peditor_new_string (
			NULL, AUTH_USER_KEY, WID ("user_entry"), NULL));

	peditor = GCONF_PROPERTY_EDITOR (gconf_peditor_new_string (
			NULL, AUTH_PASSWD_KEY, WID ("passwd_entry"), NULL));

	/* FIXME: remove when I fix libglade */
	gtk_entry_set_invisible_char (GTK_ENTRY (WID ("passwd_entry")), '*');

	g_signal_connect (WID ("network_dialog"), "response",
			  G_CALLBACK (cb_dialog_response), NULL);
}

int
main (int argc, char **argv) 
{
	GladeXML    *dialog;
	GConfClient *client;

	bindtextdomain (GETTEXT_PACKAGE, GNOMELOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	gnome_program_init (argv [0], VERSION, LIBGNOMEUI_MODULE,
			    argc, argv, GNOME_PARAM_NONE);

	client = gconf_client_get_default ();
	gconf_client_add_dir (client, "/system/gnome-vfs",
			      GCONF_CLIENT_PRELOAD_ONELEVEL, NULL);

	dialog = glade_xml_new (GNOMECC_DATA_DIR "/interfaces/gnome-network-preferences.glade",
				"network_dialog", NULL);

	setup_dialog (dialog);

	gtk_widget_show_all (WID ("network_dialog"));
	gtk_main ();

	g_object_unref (client);

	return 0;
}
