/* -*- mode: c; style: linux -*- */

/* keyboard-properties.c
 * Copyright (C) 2000-2001 Ximian, Inc.
 * Copyright (C) 2001 Jonathan Blandford
 *
 * Written by: Bradford Hovinen <hovinen@ximian.com>
 *             Rachel Hestilow <hestilow@ximian.com>
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
#include "activate-settings-daemon.h"
#include <../accessibility/keyboard/accessibility-keyboard.h>

enum
{
	RESPONSE_APPLY = 1,
	RESPONSE_CLOSE
};

static GladeXML *
create_dialog (void)
{
	GladeXML *dialog;

	dialog = glade_xml_new (GNOMECC_DATA_DIR "/interfaces/gnome-keyboard-properties.glade", "keyboard_dialog", NULL);

	return dialog;
}

static GConfEnumStringPair bell_enums[] = {
	{ 0, "off" },
	{ 1, "on" },
	{ 2, "custom" }
};

static GConfValue *
bell_from_widget (GConfPropertyEditor *peditor, const GConfValue *value)
{
	GConfValue *new_value;

	new_value = gconf_value_new (GCONF_VALUE_STRING);
	gconf_value_set_string (new_value,
				gconf_enum_to_string (bell_enums, gconf_value_get_int (value)));

	return new_value;
}

static GConfValue *
bell_to_widget (GConfPropertyEditor *peditor, const GConfValue *value)
{
	GConfValue *new_value;
	const gchar *str;
	gint val = 2;

	str = (value && (value->type == GCONF_VALUE_STRING)) ? gconf_value_get_string (value) : NULL;

	new_value = gconf_value_new (GCONF_VALUE_INT);
	if (value->type == GCONF_VALUE_STRING) {
		gconf_string_to_enum (bell_enums,
				      str,
				      &val);
	}
	gconf_value_set_int (new_value, val);

	return new_value;
}


static GConfValue *
blink_from_widget (GConfPropertyEditor *peditor, const GConfValue *value)
{
	GConfValue *new_value;

	new_value = gconf_value_new (GCONF_VALUE_INT);
	gconf_value_set_int (new_value, 2600 - (int) gconf_value_get_float (value));

	return new_value;
}

static GConfValue *
blink_to_widget (GConfPropertyEditor *peditor, const GConfValue *value)
{
	GConfValue *new_value;
	gint current_rate;

	current_rate = gconf_value_get_int (value);
	new_value = gconf_value_new (GCONF_VALUE_FLOAT);
	gconf_value_set_float (new_value, CLAMP (2600 - current_rate, 100, 2500));

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
accessibility_button_clicked (GtkWidget *widget,
			      gpointer   data)
{
	GError *err = NULL;
	if (!g_spawn_command_line_async ("gnome-accessibility-keyboard-properties", NULL))
		capplet_error_dialog (GTK_WINDOW (gtk_widget_get_toplevel (widget)), 
			_("There was an error launching the keyboard capplet : %s"),
			err);
}

static void
dialog_response (GtkWidget *widget,
		 gint       response_id,
		 GConfChangeSet *changeset)
{
	if (response_id == GTK_RESPONSE_HELP)
		capplet_help (GTK_WINDOW (widget),
			"wgoscustdesk.xml",
			"goscustperiph-2");
	else if (response_id == 0)
		accessibility_button_clicked (NULL, NULL);
	else
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

	gconf_peditor_new_numeric_range
		(changeset, "/desktop/gnome/peripherals/keyboard/delay", WID ("repeat_delay_scale"),
		 "conv-to-widget-cb",   gconf_value_int_to_float,
		 "conv-from-widget-cb", gconf_value_float_to_int,
		 NULL);

	gconf_peditor_new_numeric_range
		(changeset, "/desktop/gnome/peripherals/keyboard/rate", WID ("repeat_speed_scale"),
		 "conv-to-widget-cb",   gconf_value_int_to_float,
		 "conv-from-widget-cb", gconf_value_float_to_int,
		 NULL);

	peditor = gconf_peditor_new_boolean
		(changeset, "/desktop/gnome/interface/cursor_blink", WID ("cursor_toggle"), NULL);
	gconf_peditor_widget_set_guard (GCONF_PROPERTY_EDITOR (peditor), WID ("cursor_hbox"));
	gconf_peditor_new_numeric_range
		(changeset, "/desktop/gnome/interface/cursor_blink_time", WID ("cursor_blink_time_scale"),
		 "conv-to-widget-cb", blink_to_widget,
		 "conv-from-widget-cb", blink_from_widget,
		 NULL);

	peditor = gconf_peditor_new_boolean
		(changeset, "/desktop/gnome/peripherals/keyboard/click", WID ("volume_toggle"), NULL);
	gconf_peditor_widget_set_guard (GCONF_PROPERTY_EDITOR (peditor), WID ("volume_hbox"));
	gconf_peditor_new_numeric_range
		(changeset, "/desktop/gnome/peripherals/keyboard/click_volume", WID ("volume_scale"),
		 "conv-to-widget-cb",   gconf_value_int_to_float,
		 "conv-from-widget-cb", gconf_value_float_to_int,
		 NULL);
	
	g_signal_connect (G_OBJECT (WID ("bell_custom_radio")), "toggled", (GCallback) bell_guard, dialog);
	peditor = gconf_peditor_new_select_radio
		(changeset, "/desktop/gnome/peripherals/keyboard/bell_mode",
		 gtk_radio_button_get_group (GTK_RADIO_BUTTON (WID ("bell_disabled_radio"))),
		 "conv-to-widget-cb", bell_to_widget,
		 "conv-from-widget-cb", bell_from_widget,
		 NULL);
	g_signal_connect (G_OBJECT (WID ("bell_custom_radio")), "mnemonic_activate", (GCallback) mnemonic_activate, WID ("bell_custom_entry"));
	g_signal_connect (G_OBJECT (WID ("keyboard_dialog")), "response", (GCallback) dialog_response, changeset);
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
#if 0
	COPY_FROM_LEGACY (int,  "/gnome/desktop/peripherals/keyboard/bell_volume",   "/Desktop/Bell/percent=50");
#endif
	COPY_FROM_LEGACY (int,  "/gnome/desktop/peripherals/keyboard/bell_pitch",    "/Desktop/Bell/pitch=50");
	COPY_FROM_LEGACY (int,  "/gnome/desktop/peripherals/keyboard/bell_duration", "/Desktop/Bell/duration=100");
}

#if 0
static void
setup_accessibility (GladeXML *dialog, GConfChangeSet *changeset)
{
	GtkWidget *notebook = WID ("notebook1");
	GtkWidget *label = gtk_label_new_with_mnemonic (_("_Accessibility"));
	GtkWidget *page = setup_accessX_dialog (changeset, FALSE);
	gtk_notebook_append_page (GTK_NOTEBOOK (notebook), page, label);
}
#endif

int
main (int argc, char **argv) 
{
	GConfClient    *client;
	GConfChangeSet *changeset;
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

	bindtextdomain (GETTEXT_PACKAGE, GNOMELOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	gnome_program_init ("gnome-keyboard-properties", VERSION, LIBGNOMEUI_MODULE, argc, argv,
			    GNOME_PARAM_POPT_TABLE, cap_options,
			    GNOME_PARAM_APP_DATADIR, GNOMECC_DATA_DIR,
			    NULL);

	activate_settings_daemon ();
	
	client = gconf_client_get_default ();
	gconf_client_add_dir (client, "/desktop/gnome/peripherals/keyboard", GCONF_CLIENT_PRELOAD_ONELEVEL, NULL);
	gconf_client_add_dir (client, "/desktop/gnome/interface", GCONF_CLIENT_PRELOAD_ONELEVEL, NULL);

	if (get_legacy) {
		get_legacy_settings ();
	} else {
		changeset = NULL;
		dialog = create_dialog ();
		setup_dialog (dialog, changeset);
		capplet_set_icon (WID ("keyboard_dialog"),
			"keyboard-capplet.png");
		gtk_widget_show (WID ("keyboard_dialog"));
		gtk_main ();
	}

	return 0;
}
