/* -*- mode: c; style: linux -*- */

/* keyboard-properties.c
 * Copyright (C) 2000-2001 Ximian, Inc.
 * Copyright (C) 2001 Jonathan Blandford
 *
 * Written by: Bradford Hovinen <hovinen@ximian.com>
 *             Richard Hestilow <hestilow@ximian.com>
 *	       Jonathan Blandford <jrb@redhat.com>
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

#include <gnome.h>
#include <gconf/gconf-client.h>
#include <glade/glade.h>

#include "capplet-util.h"
#include "gconf-property-editor.h"

static GladeXML *
create_dialog (void)
{
	GladeXML *dialog;

	dialog = glade_xml_new (GNOMECC_DATA_DIR "/interfaces/gnome-keyboard-properties.glade", "keyboard_dialog", NULL);

	return dialog;
}

static GConfValue *
rate_to_widget (GConfValue *value) 
{
	GConfValue *new_value;
	int rate;

	rate = gconf_value_get_int (value);

	new_value = gconf_value_new (GCONF_VALUE_INT);

	if (rate >= (255 + 192) / 2)
		gconf_value_set_int (new_value, 0);
	else if (rate >= (192 + 64) / 2)
		gconf_value_set_int (new_value, 1);
	else if (rate >= (64 + 1) / 2)
		gconf_value_set_int (new_value, 2);
	else
		gconf_value_set_int (new_value, 3);

	return new_value;
}

static GConfValue *
rate_from_widget (GConfValue *value) 
{
	static int rates[] = {
		255, 192, 64, 1
	};

	GConfValue *new_value;

	new_value = gconf_value_new (GCONF_VALUE_INT);
	gconf_value_set_int (new_value, rates[gconf_value_get_int (value)]);
	return new_value;
}

static GConfValue *
delay_to_widget (GConfValue *value) 
{
	GConfValue *new_value;
	int delay;

	delay = gconf_value_get_int (value);

	new_value = gconf_value_new (GCONF_VALUE_INT);

	if (delay >= (1000 + 700) / 2)
		gconf_value_set_int (new_value, 0);
	else if (delay >= (700 + 300) / 2)
		gconf_value_set_int (new_value, 1);
	else if (delay >= (300) / 2)
		gconf_value_set_int (new_value, 2);
	else
		gconf_value_set_int (new_value, 3);

	return new_value;
}

static GConfValue *
delay_from_widget (GConfValue *value) 
{
	static int delays[] = {
		1000, 700, 300, 0
	};

	GConfValue *new_value;

	new_value = gconf_value_new (GCONF_VALUE_INT);
	gconf_value_set_int (new_value, delays[gconf_value_get_int (value)]);
	return new_value;
}

static GConfEnumStringPair bell_enums[] = {
	{ 0, "custom" },
	{ 1, "on" },
	{ 2, "off" }
};

static GConfValue *
bell_from_widget (GConfValue *value) 
{
	GConfValue *new_value;

	new_value = gconf_value_new (GCONF_VALUE_STRING);
	gconf_value_set_string (new_value,
				gconf_enum_to_string (bell_enums, gconf_value_get_int (value)));

	return new_value;
}

static GConfValue *
bell_to_widget (GConfValue *value) 
{
	GConfValue *new_value;
	gint val = 2;

	new_value = gconf_value_new (GCONF_VALUE_INT);
	gconf_string_to_enum (bell_enums,
			      gconf_value_get_string (value),
			      &val);
	gconf_value_set_int (new_value, val);

	return new_value;
}

static void
bell_guard (GtkWidget *toggle,
	    GladeXML  *dialog)
{
	gtk_widget_set_sensitive (WID ("bell_custom_fileentry"), GTK_TOGGLE_BUTTON (toggle)->active);
}

static gboolean
mnemonic_activate (GtkWidget *toggle,
		   gboolean   group_cycling,
		   GtkWidget *entry)
{
	if (! group_cycling)
		gtk_widget_grab_focus (entry);
	return FALSE;
}


static void
dialog_response (GtkWidget *widget,
		 gint       response,
		 GladeXML  *dialog)
{
	if (response == GTK_RESPONSE_HELP)
		return;

	gtk_main_quit ();
}

static void
setup_dialog (GladeXML       *dialog,
	      GConfChangeSet *changeset)
{
	GObject *peditor;
	GnomeProgram *program;
	gchar *filename;

	/* load all the images */
	program = gnome_program_get ();
	filename = gnome_program_locate_file (program, GNOME_FILE_DOMAIN_APP_PIXMAP, "keyboard-repeat.png", TRUE, NULL);
	gtk_image_set_from_file (GTK_IMAGE (WID ("repeat_image")), filename);
	g_free (filename);
	filename = gnome_program_locate_file (program, GNOME_FILE_DOMAIN_APP_PIXMAP, "keyboard-cursor.png", TRUE, NULL);
	gtk_image_set_from_file (GTK_IMAGE (WID ("cursor_image")), filename);
	g_free (filename);
	filename = gnome_program_locate_file (program, GNOME_FILE_DOMAIN_APP_PIXMAP, "keyboard-volume.png", TRUE, NULL);
	gtk_image_set_from_file (GTK_IMAGE (WID ("volume_image")), filename);
	g_free (filename);
	filename = gnome_program_locate_file (program, GNOME_FILE_DOMAIN_APP_PIXMAP, "keyboard-bell.png", TRUE, NULL);
	gtk_image_set_from_file (GTK_IMAGE (WID ("bell_image")), filename);
	g_free (filename);

	peditor = gconf_peditor_new_boolean
		(changeset, "/desktop/gnome/peripherals/keyboard/repeat", WID ("repeat_toggle"), NULL);
	gconf_peditor_widget_set_guard (GCONF_PROPERTY_EDITOR (peditor), WID ("repeat_table"));

	gconf_peditor_new_select_menu
		(changeset, "/gnome/desktop/peripherals/keyboard/delay", WID ("repeat_delay_omenu"),
		 "conv-to-widget-cb", delay_to_widget,
		 "conv-from-widget-cb", delay_from_widget,
		 NULL);

	gconf_peditor_new_select_menu
		(changeset, "/gnome/desktop/peripherals/keyboard/rate", WID ("repeat_speed_omenu"),
		 "conv-to-widget-cb", rate_to_widget,
		 "conv-from-widget-cb", rate_from_widget,
		 NULL);

	peditor = gconf_peditor_new_boolean
		(changeset, "/desktop/gnome/interface/cursor_blink", WID ("cursor_toggle"), NULL);
	gconf_peditor_widget_set_guard (GCONF_PROPERTY_EDITOR (peditor), WID ("cursor_hbox"));
	gconf_peditor_new_numeric_range
		(changeset, "/desktop/gnome/interface/cursor_blink_time", WID ("cursor_blink_time_scale"), NULL);


	peditor = gconf_peditor_new_boolean
		(changeset, "/desktop/gnome/peripherals/keyboard/click", WID ("volume_toggle"), NULL);
	gconf_peditor_widget_set_guard (GCONF_PROPERTY_EDITOR (peditor), WID ("volume_hbox"));
	gconf_peditor_new_numeric_range
		(changeset, "/desktop/gnome/peripherals/keyboard/clickvolume", WID ("volume_scale"), NULL);
	
	g_signal_connect (G_OBJECT (WID ("bell_custom_radio")), "toggled", (GCallback) bell_guard, dialog);
	peditor = gconf_peditor_new_select_radio
		(changeset, "/desktop/gnome/peripherals/keyboard/bell_mode",
		 gtk_radio_button_get_group (GTK_RADIO_BUTTON (WID ("bell_disabled_radio"))),
		 "conv-to-widget-cb", bell_to_widget,
		 "conv-from-widget-cb", bell_from_widget,
		 NULL);
	g_signal_connect (G_OBJECT (WID ("bell_custom_radio")), "mnemonic_activate", (GCallback) mnemonic_activate, WID ("bell_custom_entry"));
	g_signal_connect (G_OBJECT (WID ("keyboard_dialog")), "response", (GCallback) dialog_response, dialog);
}

static void
get_legacy_settings (void)
{
	GConfClient *client;
	gboolean val_bool, def;
	gulong val_int;

	client = gconf_client_get_default ();

	COPY_FROM_LEGACY (bool, "/gnome/desktop/peripherals/keyboard/repeat",        "/Desktop/Keyboard/repeat=true");
	COPY_FROM_LEGACY (bool, "/gnome/desktop/peripherals/keyboard/click",         "/Desktop/Keyboard/click=true");
	COPY_FROM_LEGACY (int,  "/gnome/desktop/peripherals/keyboard/rate",          "/Desktop/Keyboard/rate=30");
	COPY_FROM_LEGACY (int,  "/gnome/desktop/peripherals/keyboard/delay",         "/Desktop/Keyboard/delay=500");
	COPY_FROM_LEGACY (int,  "/gnome/desktop/peripherals/keyboard/volume",        "/Desktop/Keyboard/clickvolume=0");
	COPY_FROM_LEGACY (int,  "/gnome/desktop/peripherals/keyboard/bell_volume",   "/Desktop/Bell/percent=50");
	COPY_FROM_LEGACY (int,  "/gnome/desktop/peripherals/keyboard/bell_pitch",    "/Desktop/Bell/pitch=50");
	COPY_FROM_LEGACY (int,  "/gnome/desktop/peripherals/keyboard/bell_duration", "/Desktop/Bell/duration=100");
}


int
main (int argc, char **argv) 
{
	GConfClient    *client;
	GladeXML       *dialog;

	static gboolean apply_only;
	static gboolean get_legacy;
	static struct poptOption cap_options[] = {
		{ "apply", '\0', POPT_ARG_NONE, &apply_only, 0,
		  N_("Just apply settings and quit (compatibility only; now handled by daemon)"), NULL },
		{ "init-session-settings", '\0', POPT_ARG_NONE, &apply_only, 0,
		  N_("Just apply settings and quit (compatibility only; now handled by daemon)"), NULL },
		{ "get-legacy", '\0', POPT_ARG_NONE, &get_legacy, 0,
		  N_("Retrieve and store legacy settings"), NULL },
		{ NULL, '\0', 0, NULL, 0, NULL, NULL }
	};

	bindtextdomain (PACKAGE, GNOMELOCALEDIR);
	bind_textdomain_codeset (PACKAGE, "UTF-8");
	textdomain (PACKAGE);

	gnome_program_init (argv[0], VERSION, LIBGNOMEUI_MODULE, argc, argv,
			    GNOME_PARAM_POPT_TABLE, cap_options,
			    GNOME_PARAM_APP_DATADIR, GNOMECC_DATA_DIR,
			    NULL);

	client = gconf_client_get_default ();
	gconf_client_add_dir (client, "/desktop/gnome/peripherals/keyboard", GCONF_CLIENT_PRELOAD_ONELEVEL, NULL);
	gconf_client_add_dir (client, "/desktop/gnome/interface", GCONF_CLIENT_PRELOAD_ONELEVEL, NULL);

	if (get_legacy) {
		get_legacy_settings ();
	} else {
		dialog = create_dialog ();
		setup_dialog (dialog, NULL);

		gtk_widget_show_all (WID ("keyboard_dialog"));
		gtk_main ();
	}

	return 0;
}
