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
#include "capplet-stock-icons.h"
#include <../accessibility/keyboard/accessibility-keyboard.h>

#include "gnome-keyboard-properties-xkb.h"

enum
{
	RESPONSE_APPLY = 1,
	RESPONSE_CLOSE
};

static GladeXML *
create_dialog (void)
{
	GladeXML *dialog;
	GtkSizeGroup *size_group;

	dialog = glade_xml_new (GNOMECC_DATA_DIR "/interfaces/gnome-keyboard-properties.glade", "keyboard_dialog", NULL);

	size_group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);
	gtk_size_group_add_widget (size_group, WID ("repeat_slow_label"));
	gtk_size_group_add_widget (size_group, WID ("delay_short_label"));
	gtk_size_group_add_widget (size_group, WID ("blink_slow_label"));

	size_group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);
	gtk_size_group_add_widget (size_group, WID ("repeat_fast_label"));
	gtk_size_group_add_widget (size_group, WID ("delay_long_label"));
	gtk_size_group_add_widget (size_group, WID ("blink_fast_label"));

	size_group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);
	gtk_size_group_add_widget (size_group, WID ("repeat_delay_scale"));
	gtk_size_group_add_widget (size_group, WID ("repeat_speed_scale"));
	gtk_size_group_add_widget (size_group, WID ("cursor_blink_time_scale"));

	return dialog;
}

static GConfValue *
blink_from_widget (GConfPropertyEditor *peditor, const GConfValue *value)
{
	GConfValue *new_value;

	new_value = gconf_value_new (GCONF_VALUE_INT);
	gconf_value_set_int (new_value, 2600 - gconf_value_get_int (value));

	return new_value;
}

static GConfValue *
blink_to_widget (GConfPropertyEditor *peditor, const GConfValue *value)
{
	GConfValue *new_value;
	gint current_rate;

	current_rate = gconf_value_get_int (value);
	new_value = gconf_value_new (GCONF_VALUE_INT);
	gconf_value_set_int (new_value, CLAMP (2600 - current_rate, 100, 2500));

	return new_value;
}

static void
accessibility_button_clicked (GtkWidget *widget,
			      gpointer   data)
{
	GError *err = NULL;
	if (!g_spawn_command_line_async ("gnome-accessibility-keyboard-properties", &err))
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
			"user-guide.xml",
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

	/* load all the images */
	program = gnome_program_get ();

	capplet_init_stock_icons ();
	
	peditor = gconf_peditor_new_boolean
		(changeset, "/desktop/gnome/peripherals/keyboard/repeat", WID ("repeat_toggle"), NULL);
	gconf_peditor_widget_set_guard (GCONF_PROPERTY_EDITOR (peditor), WID ("repeat_table"));

	gconf_peditor_new_numeric_range
		(changeset, "/desktop/gnome/peripherals/keyboard/delay", WID ("repeat_delay_scale"),
		 NULL);

	gconf_peditor_new_numeric_range
		(changeset, "/desktop/gnome/peripherals/keyboard/rate", WID ("repeat_speed_scale"),
		 NULL);

	peditor = gconf_peditor_new_boolean
		(changeset, "/desktop/gnome/interface/cursor_blink", WID ("cursor_toggle"), NULL);
	gconf_peditor_widget_set_guard (GCONF_PROPERTY_EDITOR (peditor), WID ("cursor_hbox"));
	gconf_peditor_new_numeric_range
		(changeset, "/desktop/gnome/interface/cursor_blink_time", WID ("cursor_blink_time_scale"),
		 "conv-to-widget-cb", blink_to_widget,
		 "conv-from-widget-cb", blink_from_widget,
		 NULL);

	/* Ergonomics */
	peditor = gconf_peditor_new_boolean
		(changeset, "/desktop/gnome/typing_break/enabled", WID ("break_enabled_toggle"), NULL);
	gconf_peditor_widget_set_guard (GCONF_PROPERTY_EDITOR (peditor), WID ("break_details_table"));
	gconf_peditor_new_numeric_range
		(changeset, "/desktop/gnome/typing_break/type_time", WID ("break_enabled_spin"), NULL);
	gconf_peditor_new_numeric_range
		(changeset, "/desktop/gnome/typing_break/break_time", WID ("break_interval_spin"), NULL);
	gconf_peditor_new_boolean
		(changeset, "/desktop/gnome/typing_break/allow_postpone", WID ("break_postponement_toggle"), NULL);
	g_signal_connect (G_OBJECT (WID ("keyboard_dialog")), "response", (GCallback) dialog_response, changeset);
	
	gtk_label_set_use_markup (GTK_LABEL (GTK_BIN (WID ("break_enabled_toggle"))->child), TRUE);
	
        setup_xkb_tabs(dialog,changeset);
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

	static gboolean apply_only = FALSE;
	static gboolean get_legacy = FALSE;
	static gboolean switch_to_typing_break_page = FALSE;

	static struct poptOption cap_options[] = {
		{ "apply", '\0', POPT_ARG_NONE, &apply_only, 0,
		  N_("Just apply settings and quit (compatibility only; now handled by daemon)"), NULL },
		{ "init-session-settings", '\0', POPT_ARG_NONE, &apply_only, 0,
		  N_("Just apply settings and quit (compatibility only; now handled by daemon)"), NULL },
		{ "get-legacy", '\0', POPT_ARG_NONE, &get_legacy, 0,
		  N_("Retrieve and store legacy settings"), NULL },
		{ "typing-break", '\0', POPT_ARG_NONE, &switch_to_typing_break_page, 0,
		  N_("Start the page with the typing break settings showing"), NULL },
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
		if (switch_to_typing_break_page) {
			gtk_notebook_set_current_page (GTK_NOTEBOOK (WID ("keyboard_notebook")), 3);
		}
		capplet_set_icon (WID ("keyboard_dialog"),
				  "keyboard-capplet.png");
		gtk_widget_show (WID ("keyboard_dialog"));
		gtk_main ();
	}

	return 0;
}
