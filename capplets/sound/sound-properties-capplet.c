/* -*- mode: c; style: linux -*- */

/* sound-properties-capplet.c
 * Copyright (C) 2001 Ximian, Inc.
 *
 * Written by Bradford Hovinen <hovinen@ximian.com>
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

#include "capplet-util.h"
#include "gconf-property-editor.h"
#include "libsounds/sound-view.h"

#include <glade/glade.h>

/* Needed only for the sound capplet */

#include <stdlib.h>
#include <sys/types.h>

#include "activate-settings-daemon.h"

#define ENABLE_ESD_KEY             "/desktop/gnome/sound/enable_esd"
#define EVENT_SOUNDS_KEY           "/desktop/gnome/sound/event_sounds"
#define VISUAL_BELL_KEY            "/apps/metacity/general/visual_bell"
#define AUDIO_BELL_KEY             "/apps/metacity/general/audible_bell"
#define VISUAL_BELL_TYPE_KEY       "/apps/metacity/general/visual_bell_type"

/* Capplet-specific prototypes */

static SoundProperties *props = NULL;

static void
props_changed_cb (SoundProperties *p, SoundEvent *event, gpointer data)
{
	sound_properties_user_save (p);
}



static GConfEnumStringPair bell_flash_enums[] = {
	{ 0, "frame_flash" },
	{ 1, "fullscreen_flash" },
	{ -1, NULL }
};

static GConfValue *
bell_flash_from_widget (GConfPropertyEditor *peditor, const GConfValue *value)
{
	GConfValue *new_value;

	new_value = gconf_value_new (GCONF_VALUE_STRING);
	gconf_value_set_string (new_value,
				gconf_enum_to_string (bell_flash_enums, gconf_value_get_int (value)));

	return new_value;
}

static GConfValue *
bell_flash_to_widget (GConfPropertyEditor *peditor, const GConfValue *value)
{
	GConfValue *new_value;
	const gchar *str;
	gint val = 2;

	str = (value && (value->type == GCONF_VALUE_STRING)) ? gconf_value_get_string (value) : NULL;

	new_value = gconf_value_new (GCONF_VALUE_INT);
	if (value->type == GCONF_VALUE_STRING) {
		gconf_string_to_enum (bell_flash_enums,
				      str,
				      &val);
	}
	gconf_value_set_int (new_value, val);

	return new_value;
}

/* create_dialog
 *
 * Create the dialog box and return it as a GtkWidget
 */

static GladeXML *
create_dialog (void) 
{
	GladeXML *dialog;
	GtkWidget *widget, *box;

	dialog = glade_xml_new (GNOMECC_DATA_DIR "/interfaces/sound-properties.glade", "prefs_widget", NULL);
	widget = glade_xml_get_widget (dialog, "prefs_widget");
	g_object_set_data (G_OBJECT (widget), "glade-data", dialog);

	props = sound_properties_new ();
	sound_properties_add_defaults (props, NULL);
	g_signal_connect (G_OBJECT (props), "event_changed",
			  (GCallback) props_changed_cb, NULL);  
	box = glade_xml_get_widget (dialog, "events_vbox");
	gtk_box_pack_start (GTK_BOX (box), sound_view_new (props),
			    TRUE, TRUE, 0);

	g_signal_connect_swapped (G_OBJECT (widget), "destroy",
				  (GCallback) gtk_object_destroy, props);

	gtk_image_set_from_file (GTK_IMAGE (WID ("bell_image")),
				 GNOMECC_DATA_DIR "/pixmaps/visual-bell.png");

	gtk_widget_set_size_request (widget, -1, 250); /* Can this be right?  Seems broken for large fonts. */

	return dialog;
}

/* setup_dialog
 *
 * Set up the property editors for our dialog
 */

static void
setup_dialog (GladeXML *dialog, GConfChangeSet *changeset) 
{
	GObject *peditor;
	gchar *visual_type;
	GtkWidget *flash_titlebar_widget, *flash_screen_widget;

	peditor = gconf_peditor_new_boolean (NULL, ENABLE_ESD_KEY, WID ("enable_toggle"), NULL);
	gconf_peditor_widget_set_guard (GCONF_PROPERTY_EDITOR (peditor), WID ("events_toggle"));
	gconf_peditor_widget_set_guard (GCONF_PROPERTY_EDITOR (peditor), WID ("events_vbox"));

	gconf_peditor_new_boolean (NULL, EVENT_SOUNDS_KEY, WID ("events_toggle"), NULL);

	gconf_peditor_new_boolean (NULL, AUDIO_BELL_KEY, WID ("bell_audible_toggle"), NULL);

	peditor = gconf_peditor_new_boolean (NULL, VISUAL_BELL_KEY, WID ("bell_visual_toggle"), NULL);
	gconf_peditor_widget_set_guard (GCONF_PROPERTY_EDITOR (peditor), WID ("bell_flash_vbox"));

	/* peditor not so convenient for the radiobuttons */
	gconf_peditor_new_select_radio (NULL,
					VISUAL_BELL_TYPE_KEY,
					gtk_radio_button_get_group (GTK_RADIO_BUTTON (WID ("bell_flash_window_radio"))),
					"conv-to-widget-cb", bell_flash_to_widget,
					"conv-from-widget-cb", bell_flash_from_widget,
					NULL);
}

/* get_legacy_settings
 *
 * Retrieve older gnome_config -style settings and store them in the
 * configuration database.
 *
 * In most cases, it's best to use the COPY_FROM_LEGACY macro defined in
 * capplets/common/capplet-util.h.
 */

static void
get_legacy_settings (void) 
{
	GConfClient *client;
	gboolean val_bool, def;

	client = gconf_client_get_default ();
	COPY_FROM_LEGACY (bool, "/desktop/gnome/sound/enable_esd", "/sound/system/settings/start_esd=false");
	COPY_FROM_LEGACY (bool, "/desktop/gnome/sound/event_sounds", "/sound/system/settings/event_sounds=false");
	g_object_unref (G_OBJECT (client));
}

static void
dialog_button_clicked_cb (GtkDialog *dialog, gint response_id, GConfChangeSet *changeset) 
{
	if (response_id == GTK_RESPONSE_HELP)
		capplet_help (GTK_WINDOW (dialog),
			"wgoscustdesk.xml",
			"goscustmulti-2");
	else
		gtk_main_quit ();
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

	bindtextdomain (GETTEXT_PACKAGE, GNOMELOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	gnome_program_init ("gnome-sound-properties", VERSION,
			    LIBGNOMEUI_MODULE, argc, argv,
			    GNOME_PARAM_POPT_TABLE, cap_options,
			    NULL);

	activate_settings_daemon ();
	
	client = gconf_client_get_default ();
	gconf_client_add_dir (client, "/desktop/gnome/sound", GCONF_CLIENT_PRELOAD_ONELEVEL, NULL);
	gconf_client_add_dir (client, "/apps/metacity/general", GCONF_CLIENT_PRELOAD_ONELEVEL, NULL);

	if (get_legacy) {
		get_legacy_settings ();
	} else {
		changeset = gconf_change_set_new ();
		dialog = create_dialog ();
		setup_dialog (dialog, changeset);

		dialog_win = gtk_dialog_new_with_buttons
			(_("Sound preferences"), NULL, GTK_DIALOG_NO_SEPARATOR,
			 GTK_STOCK_HELP, GTK_RESPONSE_HELP,
			 GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE,
			 NULL);

		g_signal_connect (G_OBJECT (dialog_win), "response", (GCallback) dialog_button_clicked_cb, changeset);
		gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog_win)->vbox), WID ("prefs_widget"), TRUE, TRUE, GNOME_PAD_SMALL);
		capplet_set_icon (dialog_win, "sound-capplet.png");
		gtk_widget_show_all (dialog_win);

		gtk_main ();
		gconf_change_set_unref (changeset);
	}
	
	g_object_unref (dialog);
	return 0;
}
