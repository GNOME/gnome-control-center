/* -*- mode: c; style: linux -*- */

/* keyboard-properties.c
 * Copyright (C) 2000-2001 Ximian, Inc.
 *
 * Written by: Bradford Hovinen <hovinen@ximian.com>
 *             Richard Hestilow <hestilow@ximian.com>
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
#include <math.h>

#include "capplet-util.h"
#include "gconf-property-editor.h"

#if 0
#  include <libgnomeui/gnome-window-icon.h>
#endif

#include <gdk/gdkx.h>
#include <X11/X.h>

/* Conversion between GConf values and widget values */

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

static int
get_int_from_changeset (GConfChangeSet *changeset, gchar *key) 
{
	GConfValue *value;

	if (gconf_change_set_check_value (changeset, key, &value))
		return gconf_value_get_int (value);	
	else
		return gconf_client_get_int (gconf_client_get_default (), key, NULL);
}

static void
bell_cb (GtkWidget *widget, GConfChangeSet *changeset)
{
	XKeyboardState backup;
	XKeyboardControl kbdcontrol;

	XGetKeyboardControl (GDK_DISPLAY (), &backup);

	kbdcontrol.bell_percent  = get_int_from_changeset (changeset, "/gnome/desktop/peripherals/keyboard/bell_volume");
	kbdcontrol.bell_pitch    = get_int_from_changeset (changeset, "/gnome/desktop/peripherals/keyboard/bell_pitch");
	kbdcontrol.bell_duration = get_int_from_changeset (changeset, "/gnome/desktop/peripherals/keyboard/bell_duration");

	XChangeKeyboardControl (GDK_DISPLAY (),
				KBBellPercent | KBBellPitch | KBBellDuration, 
				&kbdcontrol);

	XBell (GDK_DISPLAY (), 0);

	kbdcontrol.bell_percent = backup.bell_percent;
	kbdcontrol.bell_pitch = backup.bell_pitch;
	kbdcontrol.bell_duration = backup.bell_duration;

	XChangeKeyboardControl (GDK_DISPLAY (),
				KBBellPercent | KBBellPitch | KBBellDuration, 
				&kbdcontrol);
}

static void
setup_dialog (GladeXML *dialog, GConfChangeSet *changeset)
{
	GObject *peditor;

	peditor = gconf_peditor_new_boolean (changeset, "/gnome/desktop/peripherals/keyboard/repeat", WID ("repeat_toggle"));
	gconf_peditor_widget_set_guard (GCONF_PROPERTY_EDITOR (peditor), WID ("repeat_table"));

	peditor = gconf_peditor_new_select_menu (changeset, "/gnome/desktop/peripherals/keyboard/delay", WID ("delay_menu"));

	g_object_set (peditor,
		      "conv-to-widget-cb", delay_to_widget,
		      "conv-from-widget-cb", delay_from_widget,
		      NULL);

	peditor = gconf_peditor_new_select_menu (changeset, "/gnome/desktop/peripherals/keyboard/rate", WID ("repeat_menu"));

	g_object_set (peditor,
		      "conv-to-widget-cb", rate_to_widget,
		      "conv-from-widget-cb", rate_from_widget,
		      NULL);

	peditor = gconf_peditor_new_boolean (changeset, "/gnome/desktop/peripherals/keyboard/click", WID ("click_toggle"));
	gconf_peditor_widget_set_guard (GCONF_PROPERTY_EDITOR (peditor), WID ("click_hbox"));

	gconf_peditor_new_numeric_range (changeset, "/gnome/desktop/peripherals/keyboard/volume", WID ("click_volume_entry"));

	/* Bell properties */
	gconf_peditor_new_numeric_range (changeset, "/gnome/desktop/peripherals/keyboard/bell_volume", WID ("bell_volume_range"));
	gconf_peditor_new_numeric_range (changeset, "/gnome/desktop/peripherals/keyboard/bell_pitch", WID ("bell_pitch_range"));
	gconf_peditor_new_numeric_range (changeset, "/gnome/desktop/peripherals/keyboard/bell_duration", WID ("bell_duration_range"));

	g_signal_connect (G_OBJECT (WID ("bell_test_button")), "clicked", (GCallback) bell_cb, changeset);
}

static GladeXML *
create_dialog (void)
{
	GladeXML *dialog;

	dialog = glade_xml_new (GNOMECC_GLADE_DIR "/keyboard-properties.glade", "prefs_widget", NULL);

	/* Minor GUI addition */
	/* FIXME: There should be a way to do this using glade alone */
	gtk_image_set_from_stock (GTK_IMAGE (WID ("bell_test_image")), GNOME_STOCK_VOLUME, GTK_ICON_SIZE_BUTTON);

	return dialog;
}

static void
dialog_button_clicked_cb (GnomeDialog *dialog, gint button_number, GConfChangeSet *changeset) 
{
	if (button_number == 0)
		gconf_client_commit_change_set (gconf_client_get_default (), changeset, TRUE, NULL);
	else if (button_number == 1)
		gnome_dialog_close (dialog);
}

int
main (int argc, char **argv) 
{
	GConfClient    *client;
	GConfChangeSet *changeset;
	GladeXML       *dialog;
	GtkWidget      *dialog_win;

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
			    NULL);

	client = gconf_client_get_default ();
	gconf_client_add_dir (client, "/desktop/gnome/peripherals/keyboard", GCONF_CLIENT_PRELOAD_ONELEVEL, NULL);

	if (get_legacy) {
		get_legacy_settings ();
	} else {
		changeset = gconf_change_set_new ();
		dialog = create_dialog ();
		setup_dialog (dialog, changeset);

#if 0
		gnome_window_icon_set_default_from_file
			(GNOMECC_ICONS_DIR "keyboard-capplet.png");
#endif

		dialog_win = gnome_dialog_new (_("Keyboard properties"), GTK_STOCK_APPLY, GTK_STOCK_CLOSE, NULL);
		g_signal_connect (G_OBJECT (dialog_win), "clicked", (GCallback) dialog_button_clicked_cb, changeset);
		g_object_weak_ref (G_OBJECT (dialog_win), (GWeakNotify) gtk_main_quit, NULL);
		gtk_box_pack_start (GTK_BOX (GNOME_DIALOG (dialog_win)->vbox), WID ("prefs_widget"), TRUE, TRUE, GNOME_PAD_SMALL);
		gtk_widget_show_all (dialog_win);

		gtk_main ();
		gconf_change_set_unref (changeset);
	}

	return 0;
}
