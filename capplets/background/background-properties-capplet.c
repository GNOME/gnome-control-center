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

#include <string.h>

#include <gnome.h>
#include <gconf/gconf-client.h>
#include <glade/glade.h>

#include "capplet-util.h"
#include "gconf-property-editor.h"
#include "applier.h"
#include "preview-file-selection.h"
#include "activate-settings-daemon.h"

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
	gchar	    *path;
	gchar	    *prefix;

	GConfClient *client;

	/* gnome_config needs to be told to use the Gnome1 prefix */
	path = g_build_filename (g_get_home_dir (), ".gnome", "Background", NULL);
	prefix = g_strconcat ("=", path, "=", "/Default/", NULL);
	gnome_config_push_prefix (prefix);
	g_free (prefix);
	g_free (path);

	client = gconf_client_get_default ();

	gconf_client_set_bool (client, BG_PREFERENCES_DRAW_BACKGROUND,
			       gnome_config_get_bool ("Enabled=true"), NULL);

	val_filename = gnome_config_get_string ("wallpaper=(none)");

	if (val_filename != NULL && strcmp (val_filename, "(none)"))
	{
		gconf_client_set_string (client, BG_PREFERENCES_PICTURE_FILENAME,
					 val_filename, NULL);
		gconf_client_set_string (client, BG_PREFERENCES_PICTURE_OPTIONS,
			 bg_preferences_get_wptype_as_string (gnome_config_get_int ("wallpaperAlign=0")), NULL);
	}
	else
		gconf_client_set_string (client, BG_PREFERENCES_PICTURE_OPTIONS, "none", NULL);

	g_free (val_filename);


	gconf_client_set_string (client, BG_PREFERENCES_PRIMARY_COLOR,
				 gnome_config_get_string ("color1"), NULL);
	gconf_client_set_string (client, BG_PREFERENCES_SECONDARY_COLOR,
				 gnome_config_get_string ("color2"), NULL);

	/* Code to deal with new enum - messy */
	val_int = -1;
	val_string = gnome_config_get_string_with_default ("simple=solid", &def);
	if (!def) {
		if (!strcmp (val_string, "solid")) {
			val_int = ORIENTATION_SOLID;
		} else {
			g_free (val_string);
			val_string = gnome_config_get_string_with_default ("gradient=vertical", &def);
			if (!def)
				val_int = (!strcmp (val_string, "vertical")) ? ORIENTATION_VERT : ORIENTATION_HORIZ;
		}
	}

	g_free (val_string);

	if (val_int != -1)
		gconf_client_set_string (client, BG_PREFERENCES_COLOR_SHADING_TYPE, bg_preferences_get_orientation_as_string (val_int), NULL);

	val_boolean = gnome_config_get_bool_with_default ("adjustOpacity=true", &def);

	if (!def && val_boolean)
		gconf_client_set_int (client, BG_PREFERENCES_PICTURE_OPACITY,
				      gnome_config_get_int ("opacity=100"), NULL);
	else
		gconf_client_set_int (client, BG_PREFERENCES_PICTURE_OPACITY, 100, NULL);

	gnome_config_pop_prefix ();
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

	if (!strcmp (key, BG_PREFERENCES_PICTURE_FILENAME) ||
	    !strcmp (key, BG_PREFERENCES_PICTURE_OPTIONS))
	{
		color_frame = g_object_get_data (G_OBJECT (prefs), "color-frame");
		gtk_widget_set_sensitive (color_frame, bg_applier_render_color_p (bg_applier, prefs));
	}
}

/* Returns the wallpaper enum set before we disabled it */
static int 
get_val_true_cb (GConfPropertyEditor *peditor, gpointer data)
{
	BGPreferences *prefs = (BGPreferences*) data;
	return prefs->wallpaper_type;
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
	gconf_client_set_bool (client, BG_PREFERENCES_DRAW_BACKGROUND, TRUE, NULL);

	/* Load preferences */
	prefs = bg_preferences_new ();
	bg_preferences_load (BG_PREFERENCES (prefs));

	/* We need to be able to retrieve the applier and the color frame in
	   callbacks */
	g_object_set_data (prefs, "color-frame", WID ("color_frame"));
	g_object_set_data (prefs, "applier", bg_applier);

	peditor = gconf_peditor_new_select_menu_with_enum
		(changeset, BG_PREFERENCES_COLOR_SHADING_TYPE, WID ("color_option"), bg_preferences_orientation_get_type (), NULL);
	g_signal_connect (peditor, "value-changed", (GCallback) peditor_value_changed, prefs);

	peditor = gconf_peditor_new_color
		(changeset, BG_PREFERENCES_PRIMARY_COLOR, WID ("colorpicker1"), NULL);
	g_signal_connect (peditor, "value-changed", (GCallback) peditor_value_changed, prefs);

	peditor = gconf_peditor_new_color
		(changeset, BG_PREFERENCES_SECONDARY_COLOR, WID ("colorpicker2"), NULL);
	g_signal_connect (peditor, "value-changed", (GCallback) peditor_value_changed, prefs);

	peditor = gconf_peditor_new_filename
		(changeset, BG_PREFERENCES_PICTURE_FILENAME, WID ("image_fileentry"), NULL);
	g_signal_connect (peditor, "value-changed", (GCallback) peditor_value_changed, prefs);

	peditor = gconf_peditor_new_select_menu_with_enum
		(changeset, BG_PREFERENCES_PICTURE_OPTIONS, WID ("image_option"), bg_preferences_wptype_get_type (), NULL);
	g_signal_connect (peditor, "value-changed", (GCallback) peditor_value_changed, prefs);

	peditor = gconf_peditor_new_enum_toggle
		(changeset, BG_PREFERENCES_PICTURE_OPTIONS, WID ("picture_enabled_check"), bg_preferences_wptype_get_type (), get_val_true_cb, WPTYPE_NONE, prefs, NULL);
	g_signal_connect (peditor, "value-changed", (GCallback) peditor_value_changed, prefs);

	gconf_peditor_widget_set_guard (GCONF_PROPERTY_EDITOR (peditor), WID ("picture_frame"));

	/* Make sure preferences get applied to the preview */
	if (GTK_WIDGET_REALIZED (bg_applier_get_preview_widget (bg_applier)))
		bg_applier_apply_prefs (bg_applier, BG_PREFERENCES (prefs));
	else
		g_signal_connect_after (G_OBJECT (bg_applier_get_preview_widget (bg_applier)), "realize",
					(GCallback) realize_cb, prefs);

	preview_file_selection_hookup_file_entry (GNOME_FILE_ENTRY (WID ("image_fileentry")), _("Please select a background image"));

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
	GObject        *bg_applier;

	static gboolean get_legacy;
	static struct poptOption cap_options[] = {
		{ "get-legacy", '\0', POPT_ARG_NONE, &get_legacy, 0,
		  N_("Retrieve and store legacy settings"), NULL },
		{ NULL, '\0', 0, NULL, 0, NULL, NULL }
	};

	bindtextdomain (GETTEXT_PACKAGE, GNOMELOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	gnome_program_init (argv[0], VERSION, LIBGNOMEUI_MODULE, argc, argv,
			    GNOME_PARAM_POPT_TABLE, cap_options,
			    NULL);

	activate_settings_daemon ();

	client = gconf_client_get_default ();
	gconf_client_add_dir (client, "/desktop/gnome/background", GCONF_CLIENT_PRELOAD_ONELEVEL, NULL);

	if (get_legacy) {
		get_legacy_settings ();
	} else {
		bg_applier = bg_applier_new (BG_APPLIER_PREVIEW);
		dialog = create_dialog (BG_APPLIER (bg_applier));
		setup_dialog (dialog, NULL, BG_APPLIER (bg_applier));

		dialog_win = gtk_dialog_new_with_buttons
			(_("Background properties"), NULL, -1,
			 GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE,
			 NULL);

		g_signal_connect (G_OBJECT (dialog_win), "response", (GCallback) dialog_button_clicked_cb, NULL);

		g_object_weak_ref (G_OBJECT (dialog_win), (GWeakNotify) g_object_unref, bg_applier);
		gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog_win)->vbox), WID ("prefs_widget"), TRUE, TRUE, GNOME_PAD_SMALL);
		gtk_widget_show_all (dialog_win);

		gtk_main ();
		gconf_change_set_unref (changeset);
	}

	return 0;
}
