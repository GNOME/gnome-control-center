/* -*- mode: c; style: linux -*- */

/* background-properties-capplet.c
 * Copyright (C) 2000-2001 Ximian, Inc.
 *
 * Written by: Bradford Hovinen <hovinen@ximian.com>,
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

#include "capplet-util.h"
#include "gconf-property-editor.h"
#include "applier.h"

/* Retrieve legacy gnome_config settings and store them in the GConf
 * database. This involves some translation of the settings' meanings.
 */

static void
get_legacy_settings (void) 
{
	int          val_int;
	char        *val_string;
	gboolean     val_boolean;
	gboolean     def;
	gchar       *val_filename;

	GConfClient *client;

	client = gconf_client_get_default ();

	gconf_client_set_bool (client, "/desktop/gnome/background/enabled",
			       gnome_config_get_bool ("/Background/Default/Enabled=true"), NULL);

	val_filename = gnome_config_get_string ("/Background/Default/wallpaper=(none)");
	gconf_client_set_string (client, "/desktop/gnome/background/wallpaper-filename",
				 val_filename, NULL);

	if (val_filename != NULL && strcmp (val_filename, "(none)"))
		gconf_client_set_bool (client, "/desktop/gnome/background/wallpaper-enabled", TRUE, NULL);
	else
		gconf_client_set_bool (client, "/desktop/gnome/background/wallpaper-enabled", FALSE, NULL);

	g_free (val_filename);

	gconf_client_set_int (client, "/desktop/gnome/background/wallpaper-type",
			      gnome_config_get_int ("/Background/Default/wallpaperAlign=0"), NULL);

	gconf_client_set_string (client, "/desktop/gnome/background/color1",
				 gnome_config_get_string ("/Background/Default/color1"), NULL);
	gconf_client_set_string (client, "/desktop/gnome/background/color2",
				 gnome_config_get_string ("/Background/Default/color2"), NULL);

	/* Code to deal with new enum - messy */
	val_int = -1;
	val_string = gnome_config_get_string_with_default ("/Background/Default/simple=solid", &def);
	if (!def) {
		if (!strcmp (val_string, "solid")) {
			val_int = ORIENTATION_SOLID;
		} else {
			g_free (val_string);
			val_string = gnome_config_get_string_with_default ("/Background/Default/gradient=vertical", &def);
			if (!def)
				val_int = (!strcmp (val_string, "vertical")) ? ORIENTATION_VERT : ORIENTATION_HORIZ;
		}
	}

	g_free (val_string);

	if (val_int != -1)
		gconf_client_set_int (client, "/desktop/gnome/background/orientation", val_int, NULL);

	val_boolean = gnome_config_get_bool_with_default ("/Background/Default/adjustOpacity=true", &def);

	if (!def && val_boolean)
		gconf_client_set_int (client, "/desktop/gnome/background/opacity",
				      gnome_config_get_int ("/Background/Default/opacity=100"), NULL);
}

/* Initial apply to the preview, and setting of the color frame's sensitivity.
 *
 * We use a double-delay mechanism: first waiting 100 ms, then working in an
 * idle handler so that the preview gets rendered after the rest of the dialog
 * is displayed. This prevents the program from appearing to stall on startup,
 * making it feel more natural to the user.
 */

static gboolean
real_realize_cb (BGPreferences *prefs) 
{
	GtkWidget *color_frame;
	BGApplier *bg_applier;

	g_return_val_if_fail (prefs != NULL, TRUE);
	g_return_val_if_fail (IS_BG_PREFERENCES (prefs), TRUE);

	if (G_OBJECT (prefs)->ref_count == 0)
		return FALSE;

	bg_applier = g_object_get_data (G_OBJECT (prefs), "applier");
	color_frame = g_object_get_data (G_OBJECT (prefs), "color-frame");

	bg_applier_apply_prefs (bg_applier, prefs);

	gtk_widget_set_sensitive (color_frame, bg_applier_render_color_p (bg_applier, prefs));

	return FALSE;
}

static gboolean
realize_2_cb (BGPreferences *prefs) 
{
	gtk_idle_add ((GtkFunction) real_realize_cb, prefs);
	return FALSE;
}

static void
realize_cb (GtkWidget *widget, BGPreferences *prefs)
{
	gtk_timeout_add (100, (GtkFunction) realize_2_cb, prefs);
}

/* Callback issued when some value changes in a property editor. This merges the
 * value with the preferences object and applies the settings to the preview. It
 * also sets the sensitivity of the color frame depending on whether the base
 * colors are visible through the wallpaper (or whether the wallpaper is
 * enabled). This cannot be done with a guard as it depends on a much more
 * complex criterion than a simple boolean configuration property.
 */

static void
peditor_value_changed (GConfPropertyEditor *peditor, const gchar *key, const GConfValue *value, BGPreferences *prefs) 
{
	GConfEntry *entry;
	BGApplier *bg_applier;
	GtkWidget *color_frame;

	entry = gconf_entry_new (key, value);
	bg_preferences_merge_entry (prefs, entry);
	gconf_entry_free (entry);

	bg_applier = g_object_get_data (G_OBJECT (prefs), "applier");

	if (GTK_WIDGET_REALIZED (bg_applier_get_preview_widget (bg_applier)))
		bg_applier_apply_prefs (bg_applier, BG_PREFERENCES (prefs));

	if (!strcmp (key, "/desktop/gnome/background/wallpaper-enabled") ||
	    !strcmp (key, "/desktop/gnome/background/wallpaper-filename") ||
	    !strcmp (key, "/desktop/gnome/background/wallpaper-type"))
	{
		color_frame = g_object_get_data (G_OBJECT (prefs), "color-frame");
		gtk_widget_set_sensitive (color_frame, bg_applier_render_color_p (bg_applier, prefs));
	}
}

/* Set up the property editors in the dialog. This also loads the preferences
 * and sets up the callbacks.
 */

static void
setup_dialog (GladeXML *dialog, GConfChangeSet *changeset, BGApplier *bg_applier)
{
	GObject     *prefs;
	GObject     *peditor;
	GConfClient *client;

	/* Override the enabled setting to make sure background is enabled */
	client = gconf_client_get_default ();
	gconf_client_set_bool (client, "/desktop/gnome/background/enabled", TRUE, NULL);

	/* Load preferences */
	prefs = bg_preferences_new ();
	bg_preferences_load (BG_PREFERENCES (prefs));

	/* We need to be able to retrieve the applier and the color frame in
	   callbacks */
	g_object_set_data (prefs, "color-frame", WID ("color_frame"));
	g_object_set_data (prefs, "applier", bg_applier);

	peditor = gconf_peditor_new_select_menu
		(changeset, "/desktop/gnome/background/orientation", WID ("color_option"), NULL);
	g_signal_connect (peditor, "value-changed", (GCallback) peditor_value_changed, prefs);

	peditor = gconf_peditor_new_color
		(changeset, "/desktop/gnome/background/color1", WID ("colorpicker1"), NULL);
	g_signal_connect (peditor, "value-changed", (GCallback) peditor_value_changed, prefs);

	peditor = gconf_peditor_new_color
		(changeset, "/desktop/gnome/background/color2", WID ("colorpicker2"), NULL);
	g_signal_connect (peditor, "value-changed", (GCallback) peditor_value_changed, prefs);

	peditor = gconf_peditor_new_filename
		(changeset, "/desktop/gnome/background/wallpaper-filename", WID ("image_fileentry"), NULL);
	g_signal_connect (peditor, "value-changed", (GCallback) peditor_value_changed, prefs);

	peditor = gconf_peditor_new_select_menu
		(changeset, "/desktop/gnome/background/wallpaper-type", WID ("image_option"), NULL);
	g_signal_connect (peditor, "value-changed", (GCallback) peditor_value_changed, prefs);

	peditor = gconf_peditor_new_boolean
		(changeset, "/desktop/gnome/background/wallpaper-enabled", WID ("picture_enabled_check"), NULL);
	g_signal_connect (peditor, "value-changed", (GCallback) peditor_value_changed, prefs);

	gconf_peditor_widget_set_guard (GCONF_PROPERTY_EDITOR (peditor), WID ("picture_frame"));

	/* Make sure preferences get applied to the preview */
	if (GTK_WIDGET_REALIZED (bg_applier_get_preview_widget (bg_applier)))
		bg_applier_apply_prefs (bg_applier, BG_PREFERENCES (prefs));
	else
		g_signal_connect_after (G_OBJECT (bg_applier_get_preview_widget (bg_applier)), "realize",
					(GCallback) realize_cb, prefs);

	/* Make sure the preferences object gets destroyed when the dialog is
	   closed */
	g_object_weak_ref (G_OBJECT (dialog), (GWeakNotify) g_object_unref, prefs);
}

/* Construct the dialog */

static GladeXML *
create_dialog (BGApplier *bg_applier) 
{
	GtkWidget *holder;
	GtkWidget *widget;
	GladeXML  *dialog;

	/* FIXME: What the hell is domain? */
	dialog = glade_xml_new (GNOMECC_DATA_DIR "/interfaces/background-properties.glade", "prefs_widget", NULL);
	widget = glade_xml_get_widget (dialog, "prefs_widget");

	/* Minor GUI addition */
	holder = WID ("prefs_widget");
	gtk_box_pack_start (GTK_BOX (holder), bg_applier_get_preview_widget (bg_applier), TRUE, TRUE, 0);
	gtk_widget_show_all (holder);

	g_object_weak_ref (G_OBJECT (widget), (GWeakNotify) g_object_unref, dialog);

	return dialog;
}

/* Callback issued when a button is clicked on the dialog */

static void
dialog_button_clicked_cb (GnomeDialog *dialog, gint response_id, GConfChangeSet *changeset) 
{
	switch (response_id) {
	case GTK_RESPONSE_APPLY:
		gconf_client_commit_change_set (gconf_client_get_default (), changeset, TRUE, NULL);
		break;

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
	GObject        *bg_applier;

	static gboolean get_legacy;
	static struct poptOption cap_options[] = {
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

	setup_session_mgmt (argv[0]);

	client = gconf_client_get_default ();
	gconf_client_add_dir (client, "/desktop/gnome/background", GCONF_CLIENT_PRELOAD_ONELEVEL, NULL);

	if (get_legacy) {
		get_legacy_settings ();
	} else {
		changeset = gconf_change_set_new ();
		bg_applier = bg_applier_new (BG_APPLIER_PREVIEW);
		dialog = create_dialog (BG_APPLIER (bg_applier));
		setup_dialog (dialog, changeset, BG_APPLIER (bg_applier));

		dialog_win = gtk_dialog_new_with_buttons
			(_("Background properties"), NULL, -1,
			 GTK_STOCK_APPLY, GTK_RESPONSE_APPLY,
			 GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE,
			 NULL);

		g_signal_connect (G_OBJECT (dialog_win), "response", (GCallback) dialog_button_clicked_cb, changeset);

		g_object_weak_ref (G_OBJECT (dialog_win), (GWeakNotify) g_object_unref, bg_applier);
		gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog_win)->vbox), WID ("prefs_widget"), TRUE, TRUE, GNOME_PAD_SMALL);
		gtk_widget_show_all (dialog_win);

		gtk_main ();
		gconf_change_set_unref (changeset);
	}

	return 0;
}
