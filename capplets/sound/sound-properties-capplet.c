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

/* Capplet-specific prototypes */

static SoundProperties *props = NULL;

/* create_dialog
 *
 * Create the dialog box and return it as a GtkWidget
 */

static GladeXML *
create_dialog (void) 
{
	GladeXML *data;
	GtkWidget *widget, *box;

	data = glade_xml_new (GNOMECC_DATA_DIR "/interfaces/sound-properties.glade", "prefs_widget", NULL);
	widget = glade_xml_get_widget (data, "prefs_widget");
	g_object_set_data (G_OBJECT (widget), "glade-data", data);

	props = sound_properties_new ();
	sound_properties_add_defaults (props, NULL);
	box = glade_xml_get_widget (data, "events_vbox");
	gtk_box_pack_start (GTK_BOX (box), sound_view_new (props),
			    TRUE, TRUE, 0);

	g_signal_connect_swapped (G_OBJECT (widget), "destroy",
				  (GCallback) gtk_object_destroy, props);

	gtk_widget_set_size_request (widget, -1, 250);

	return data;
}

/* setup_dialog
 *
 * Set up the property editors for our dialog
 */

static void
setup_dialog (GladeXML *dialog, GConfChangeSet *changeset) 
{
	GObject *peditor;

	peditor = gconf_peditor_new_boolean (NULL, "/desktop/gnome/sound/enable_esd", WID ("enable_toggle"), NULL);
	gconf_peditor_widget_set_guard (GCONF_PROPERTY_EDITOR (peditor), WID ("events_toggle"));
	gconf_peditor_widget_set_guard (GCONF_PROPERTY_EDITOR (peditor), WID ("events_vbox"));
	peditor = gconf_peditor_new_boolean (NULL, "/desktop/gnome/sound/event_sounds", WID ("events_toggle"), NULL);
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
	switch (response_id) {
	case GTK_RESPONSE_CLOSE:
		gtk_main_quit ();
		break;
	}
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

	gnome_program_init (argv[0], VERSION, LIBGNOMEUI_MODULE, argc, argv,
			    GNOME_PARAM_POPT_TABLE, cap_options,
			    NULL);

	client = gconf_client_get_default ();
	gconf_client_add_dir (client, "/desktop/gnome/sound", GCONF_CLIENT_PRELOAD_ONELEVEL, NULL);

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

		dialog_win = gtk_dialog_new_with_buttons
			(_("Sound properties"), NULL, -1,
			 GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE,
			 NULL);

		g_signal_connect (G_OBJECT (dialog_win), "response", (GCallback) dialog_button_clicked_cb, changeset);
		gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog_win)->vbox), WID ("prefs_widget"), TRUE, TRUE, GNOME_PAD_SMALL);
		gtk_widget_show_all (dialog_win);

		gtk_main ();
		gconf_change_set_unref (changeset);
	}

	return 0;
}
