#define __GPM_PRINTER_C__

/*
 *  GNOME Print Manager
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * Authors:
 *  <Write your name here if you are>
 *   Lauris Kaplinski <lauris@ximian.com>
 *
 *  Copyright (C) 2002 Ximian, Inc. and authors
 */

#include <config.h>

#include <string.h>

#include <gconf/gconf-client.h>
#include <libgnome/gnome-i18n.h>
#include <libgnome/gnome-program.h>
#include <gtk/gtkmain.h>
#include <gtk/gtkdialog.h>
#include <gtk/gtktogglebutton.h>
#include <gtk/gtktogglebutton.h>
#include <libgnomeui/gnome-ui-init.h>
#include <libgnomeui/gnome-font-picker.h>
#include <glade/glade.h>

#include "activate-settings-daemon.h"

static GladeXML *xml;
	
#define FONT_CAPPLET_DIR "/desktop/gnome/interface"
#define FONT_CAPPLET_KEY_FONT_NAME "/desktop/gnome/interface/font_name"

#define WID(w) (glade_xml_get_widget (xml, (w)))

void
response_cb (GtkDialog *dialog, gint r, gpointer data)
{
	gtk_main_quit ();
}

static void
font_capplet_custom_toggled (GtkToggleButton *tb, GConfClient *client)
{
	gchar *sval;

	sval = gconf_client_get_string (client, FONT_CAPPLET_KEY_FONT_NAME, NULL);

	if (gtk_toggle_button_get_active (tb)) {
		const gchar *fontname;
		/* Use custom font */
		fontname = gnome_font_picker_get_font_name (GNOME_FONT_PICKER (WID ("font_picker")));
		if (fontname) {
			/* We are really nice here */
			if (!sval || strcmp (fontname, sval)) {
				gconf_client_set_string (client, FONT_CAPPLET_KEY_FONT_NAME, fontname, NULL);
			}
		}
	} else {
		/* Do not use custom font */
		if (sval) {
			gconf_client_unset (client, FONT_CAPPLET_KEY_FONT_NAME, NULL);
		}
	}

	gtk_widget_set_sensitive (WID ("font_picker"), gtk_toggle_button_get_active (tb));

	if (sval) g_free (sval);
}

static void
font_capplet_font_set (GnomeFontPicker *fp, const gchar *fontname, GConfClient *client)
{
	gchar *sval;

	sval = gconf_client_get_string (client, FONT_CAPPLET_KEY_FONT_NAME, NULL);

	if (fontname) {
		if (!sval || strcmp (fontname, sval)) {
			gconf_client_set_string (client, FONT_CAPPLET_KEY_FONT_NAME, fontname, NULL);
		}
	}

	if (sval) g_free (sval);
}

static void
font_capplet_value_notify (GConfClient *client, guint id, GConfEntry *entry, gpointer data)
{
	if (entry->value) {
		const gchar *sval;
		/* We have value set */
		sval = gconf_value_get_string (entry->value);
		if (sval) {
			gnome_font_picker_set_font_name (GNOME_FONT_PICKER (WID ("font_picker")), sval);
			gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (WID ("custom_check")), TRUE);
		}
	} else {
		/* No value set */
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (WID ("custom_check")), FALSE);
	}
}

static void
setup_dialog (GConfClient *client)
{
	gchar *sval;
#if 0
	GObject *peditor;

	peditor = gconf_peditor_new_boolean (NULL, /*changeset,*/ "/desktop/gnome/interface/use_custom_font", WID ("custom_check"), NULL);
	gconf_peditor_widget_set_guard (GCONF_PROPERTY_EDITOR (peditor), WID ("font_picker"));
	peditor = gconf_peditor_new_font (NULL, /*changeset,*/ "/desktop/gnome/interface/font_name", WID ("font_picker"), NULL);
#endif

	/* Set initial values */
	sval = gconf_client_get_string (client, FONT_CAPPLET_KEY_FONT_NAME, NULL);
	if (sval) gnome_font_picker_set_font_name (GNOME_FONT_PICKER (WID ("font_picker")), sval);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (WID ("custom_check")), sval != NULL);
	gtk_widget_set_sensitive (WID ("font_picker"), sval != NULL);
	if (sval) g_free (sval);

	g_signal_connect (G_OBJECT (WID ("custom_check")), "toggled", G_CALLBACK (font_capplet_custom_toggled), client);
	g_signal_connect (G_OBJECT (WID ("font_picker")), "font_set", G_CALLBACK (font_capplet_font_set), client);
	gconf_client_notify_add (client, FONT_CAPPLET_KEY_FONT_NAME, font_capplet_value_notify, NULL, NULL, NULL);
}
	
int
main (int argc, char **argv)
{
	GConfClient *client;

	bindtextdomain (GETTEXT_PACKAGE, GNOMELOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	gnome_program_init ("gnome2-font-properties", VERSION,
			    LIBGNOMEUI_MODULE, argc, argv, NULL);

	xml = glade_xml_new (GNOMECC_DATA_DIR "/interfaces/font-properties.glade", NULL, NULL);

	activate_settings_daemon ();

	client = gconf_client_get_default ();
	gconf_client_add_dir (client, FONT_CAPPLET_DIR, GCONF_CLIENT_PRELOAD_ONELEVEL, NULL);

	setup_dialog (client);
	
	glade_xml_signal_autoconnect (xml);
	gtk_main ();
 
	return 0;
}

