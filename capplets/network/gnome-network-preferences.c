
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
#include <libgnomevfs/gnome-vfs-uri.h>

#include "capplet-util.h"
#include "gconf-property-editor.h"

enum ProxyMode
{
	PROXYMODE_NONE,
	PROXYMODE_MANUAL,
	PROXYMODE_AUTO
};

static GEnumValue proxytype_values[] = {
	{ PROXYMODE_NONE, "PROXYMODE_NONE", "none"},
	{ PROXYMODE_MANUAL, "PROXYMODE_MANUAL", "manual"},
	{ PROXYMODE_AUTO, "PROXYMODE_AUTO", "auto"},
	{ 0, NULL, NULL }
};

#define USE_PROXY_KEY   "/system/http_proxy/use_http_proxy"
#define HTTP_PROXY_HOST_KEY  "/system/http_proxy/host"
#define HTTP_PROXY_PORT_KEY  "/system/http_proxy/port"
#define HTTP_USE_AUTH_KEY    "/system/http_proxy/use_authentication"
#define HTTP_AUTH_USER_KEY   "/system/http_proxy/authentication_user"
#define HTTP_AUTH_PASSWD_KEY "/system/http_proxy/authentication_password"
#define PROXY_MODE_KEY "/system/proxy/mode"
#define SECURE_PROXY_HOST_KEY  "/system/proxy/secure_host"
#define SECURE_PROXY_PORT_KEY  "/system/proxy/secure_port"
#define FTP_PROXY_HOST_KEY  "/system/proxy/ftp_host"
#define FTP_PROXY_PORT_KEY  "/system/proxy/ftp_port"
#define SOCKS_PROXY_HOST_KEY  "/system/proxy/socks_host"
#define SOCKS_PROXY_PORT_KEY  "/system/proxy/socks_port"
#define PROXY_AUTOCONFIG_URL_KEY  "/system/proxy/autoconfig_url"

static GtkWidget *details_dialog = NULL;

static void
cb_dialog_response (GtkDialog *dialog, gint response_id)
{
	if (response_id == GTK_RESPONSE_HELP)
		capplet_help (GTK_WINDOW (dialog),
			"wgoscustdesk.xml",
			"goscustdesk-50");
	else
		gtk_main_quit ();
}

static void
cb_details_dialog_response (GtkDialog *dialog, gint response_id)
{
	if (response_id == GTK_RESPONSE_HELP)
		capplet_help (GTK_WINDOW (dialog),
			      "wgoscustdesk.xml",
			      "goscustdesk-50");
	else {
		gtk_widget_destroy (GTK_WIDGET (dialog));
		details_dialog = NULL;
	}
}

static void
cb_use_auth_toggled (GtkToggleButton *toggle,
		     GtkWidget *table)
{
	gtk_widget_set_sensitive (table, toggle->active);
}

static void
cb_http_details_button_clicked (GtkWidget *button,
			        GtkWidget *parent)
{
	GladeXML  *dialog;
	GtkWidget *widget;
	GConfPropertyEditor *peditor;

	if (details_dialog != NULL) {
		gtk_window_present (GTK_WINDOW (details_dialog));
		gtk_widget_grab_focus (details_dialog);
		return;
	}

	dialog = glade_xml_new (GNOMECC_DATA_DIR "/interfaces/gnome-network-preferences.glade",
				"details_dialog", NULL);

	details_dialog = widget = WID ("details_dialog");

	gtk_window_set_transient_for (GTK_WINDOW (widget), GTK_WINDOW (parent));

	gtk_label_set_use_markup (GTK_LABEL (GTK_BIN (WID ("use_auth_checkbutton"))->child), TRUE);

	g_signal_connect (G_OBJECT (WID ("use_auth_checkbutton")),
			  "toggled",
			  G_CALLBACK (cb_use_auth_toggled),
			  WID ("auth_table"));

	peditor = GCONF_PROPERTY_EDITOR (gconf_peditor_new_boolean (
			NULL, HTTP_USE_AUTH_KEY, WID ("use_auth_checkbutton"),
			NULL));
	peditor = GCONF_PROPERTY_EDITOR (gconf_peditor_new_string (
			NULL, HTTP_AUTH_USER_KEY, WID ("username_entry"),
			NULL));
	peditor = GCONF_PROPERTY_EDITOR (gconf_peditor_new_string (
			NULL, HTTP_AUTH_PASSWD_KEY, WID ("password_entry"), 
			NULL));

	g_signal_connect (widget, "response",
			  G_CALLBACK (cb_details_dialog_response), NULL);
	
	capplet_set_icon (widget, "gnome-globe.png");

	gtk_widget_show_all (widget);
}

static GConfValue *
extract_proxy_host (GConfPropertyEditor *peditor, const GConfValue *orig)
{
	char const  *entered_text = gconf_value_get_string (orig);
	GConfValue  *res = NULL;

	if (entered_text != NULL) {
		GnomeVFSURI *uri = gnome_vfs_uri_new (entered_text);
		if (uri != NULL) {
			char const  *host	  = gnome_vfs_uri_get_host_name (uri);
			if (host != NULL) {
				res = gconf_value_new (GCONF_VALUE_STRING);
				gconf_value_set_string (res, host);
			}
			gnome_vfs_uri_unref (uri);
		}
	}

	if (res != NULL)
		return res;
	return gconf_value_copy (orig);
}

static void
proxy_mode_radiobutton_clicked_cb (GtkWidget *widget,
				   GladeXML *dialog)
{
	GSList *mode_group;
	int mode;
	
	if (!gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON(widget)))
		return;
		
	mode_group = g_slist_copy (gtk_radio_button_get_group 
		(GTK_RADIO_BUTTON (WID ("none_radiobutton"))));
	mode_group = g_slist_reverse (mode_group);
	mode = g_slist_index (mode_group, widget);
	g_slist_free (mode_group);
	
	gtk_widget_set_sensitive (WID ("manual_box"), 
				  mode == PROXYMODE_MANUAL);
	gtk_widget_set_sensitive (WID ("auto_box"),
				  mode == PROXYMODE_AUTO);
}

static void
connect_sensitivity_signals (GladeXML *dialog, GSList *mode_group)
{
	for (; mode_group != NULL; mode_group = mode_group->next)
	{
		g_signal_connect (G_OBJECT (mode_group->data), "clicked",
				  G_CALLBACK(proxy_mode_radiobutton_clicked_cb),
				  dialog);
	}
}

static void
setup_dialog (GladeXML *dialog)
{
	GConfPropertyEditor *peditor;
	GSList *mode_group;
	GType mode_type = 0;
	
	mode_type = g_enum_register_static ("NetworkPreferencesProxyType",
				            proxytype_values);

	/* Hackety hack */
	gtk_label_set_use_markup (GTK_LABEL (GTK_BIN (WID ("none_radiobutton"))->child), TRUE);
	gtk_label_set_use_markup (GTK_LABEL (GTK_BIN (WID ("manual_radiobutton"))->child), TRUE);
	gtk_label_set_use_markup (GTK_LABEL (GTK_BIN (WID ("auto_radiobutton"))->child), TRUE);
	
	/* Mode */
	mode_group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (WID ("none_radiobutton")));
	connect_sensitivity_signals (dialog, mode_group);

	peditor = GCONF_PROPERTY_EDITOR (gconf_peditor_new_select_radio_with_enum (NULL, 
			PROXY_MODE_KEY, mode_group, mode_type, 
			TRUE, NULL));
	
	/* Http */
	peditor = GCONF_PROPERTY_EDITOR (gconf_peditor_new_string (
			NULL, HTTP_PROXY_HOST_KEY, WID ("http_host_entry"),
			"conv-from-widget-cb", extract_proxy_host,
			NULL));
	peditor = GCONF_PROPERTY_EDITOR (gconf_peditor_new_integer (
			NULL, HTTP_PROXY_PORT_KEY, WID ("http_port_spinbutton"), 
			NULL));
	g_signal_connect (G_OBJECT (WID ("details_button")),
			  "clicked",
			  G_CALLBACK (cb_http_details_button_clicked),
			  WID ("network_dialog"));

	/* Secure */
	peditor = GCONF_PROPERTY_EDITOR (gconf_peditor_new_string (
			NULL, SECURE_PROXY_HOST_KEY, WID ("secure_host_entry"),
			"conv-from-widget-cb", extract_proxy_host,
			NULL));
	peditor = GCONF_PROPERTY_EDITOR (gconf_peditor_new_integer (
			NULL, SECURE_PROXY_PORT_KEY, WID ("secure_port_spinbutton"), 
			NULL));

	/* Ftp */
	peditor = GCONF_PROPERTY_EDITOR (gconf_peditor_new_string (
			NULL, FTP_PROXY_HOST_KEY, WID ("ftp_host_entry"),
			"conv-from-widget-cb", extract_proxy_host,
			NULL));
	peditor = GCONF_PROPERTY_EDITOR (gconf_peditor_new_integer (
			NULL, FTP_PROXY_PORT_KEY, WID ("ftp_port_spinbutton"), 
			NULL));

	/* Socks */
	peditor = GCONF_PROPERTY_EDITOR (gconf_peditor_new_string (
			NULL, SOCKS_PROXY_HOST_KEY, WID ("socks_host_entry"),
			"conv-from-widget-cb", extract_proxy_host,
			NULL));
	peditor = GCONF_PROPERTY_EDITOR (gconf_peditor_new_integer (
			NULL, SOCKS_PROXY_PORT_KEY, WID ("socks_port_spinbutton"), 
			NULL));

	/* Autoconfiguration */
	peditor = GCONF_PROPERTY_EDITOR (gconf_peditor_new_string (
			NULL, PROXY_AUTOCONFIG_URL_KEY, WID ("autoconfig_entry"),
			NULL));

	g_signal_connect (WID ("network_dialog"), "response",
			  G_CALLBACK (cb_dialog_response), NULL);
}

int
main (int argc, char **argv) 
{
	GladeXML    *dialog;
	GConfClient *client;
	GtkWidget   *widget;

	bindtextdomain (GETTEXT_PACKAGE, GNOMELOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	gnome_program_init ("gnome-network-preferences", VERSION,
			    LIBGNOMEUI_MODULE,
			    argc, argv, GNOME_PARAM_NONE);

	client = gconf_client_get_default ();
	gconf_client_add_dir (client, "/system/gnome-vfs",
			      GCONF_CLIENT_PRELOAD_ONELEVEL, NULL);

	dialog = glade_xml_new (GNOMECC_DATA_DIR "/interfaces/gnome-network-preferences.glade",
				"network_dialog", NULL);

	setup_dialog (dialog);
	widget = WID ("network_dialog");
	capplet_set_icon (widget, "gnome-globe.png");
	gtk_widget_show_all (widget);
	gtk_main ();

	g_object_unref (client);

	return 0;
}
