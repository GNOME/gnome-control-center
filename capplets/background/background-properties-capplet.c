/* -*- mode: c; style: linux -*- */

/* main.c
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
#include <bonobo.h>
#include <gconf/gconf.h>
#include <glade/glade.h>
#include "capplet-util.h"
#include "gconf-property-editor.h"
#include "applier.h"

/* Apply settings to the root window. This will be moved to
 * gnome-settings-daemon shortly.
 */

static void
apply_settings ()
{
	GObject           *prefs;
	Applier           *applier;

	applier = APPLIER (applier_new (APPLIER_ROOT));

	prefs = preferences_new ();
	preferences_load (PREFERENCES (prefs));

	applier_apply_prefs (applier, PREFERENCES (prefs));

	g_object_unref (G_OBJECT (prefs));
	g_object_unref (G_OBJECT (applier));
}

/* Retrieve legacy gnome_config settings and store them in the GConf
 * database. This involves some translation of the settings' meanings.
 */

static void
get_legacy_settings (void) 
{
	int val_int;
	char *val_string;
	gboolean val_boolean;
	gboolean def;
	gchar *val_filename;

	GConfEngine *engine;

	static const int wallpaper_types[] = { 0, 1, 3, 2 };

	engine = gconf_engine_get_default ();

	gconf_engine_set_bool (engine, "/background-properties/enabled",
			       gnome_config_get_bool ("/Background/Default/Enabled=true"), NULL);

	val_filename = gnome_config_get_string ("/Background/Default/wallpaper=(none)");
	gconf_engine_set_string (engine, "/background-properties/wallpaper-filename",
				 val_filename, NULL);

	if (val_filename != NULL && strcmp (val_filename, "(none)"))
		gconf_engine_set_bool (engine, "/background-properties/wallpaper-enabled", TRUE, NULL);
	else
		gconf_engine_set_bool (engine, "/background-properties/wallpaper-enabled", FALSE, NULL);

	g_free (val_filename);

	gconf_engine_set_int (engine, "/background-properties/wallpaper-type",
			      gnome_config_get_int ("/Background/Default/wallpaperAlign=0"), NULL);

	gconf_engine_set_string (engine, "/background-properties/color1",
				 gnome_config_get_string ("/Background/Default/color1"), NULL);
	gconf_engine_set_string (engine, "/background-properties/color2",
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
		gconf_engine_set_int (engine, "/background-properties/orientation", val_int, NULL);

	val_boolean = gnome_config_get_bool_with_default ("/Background/Default/adjustOpacity=true", &def);

	if (!def && val_boolean)
		gconf_engine_set_int (engine, "/background-properties/opacity",
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
real_realize_cb (Preferences *prefs) 
{
	GtkWidget *color_frame;
	Applier   *applier;

	g_return_val_if_fail (prefs != NULL, TRUE);
	g_return_val_if_fail (IS_PREFERENCES (prefs), TRUE);

	if (G_OBJECT (prefs)->ref_count == 0)
		return FALSE;

	applier = g_object_get_data (G_OBJECT (prefs), "applier");
	color_frame = g_object_get_data (G_OBJECT (prefs), "color-frame");

	applier_apply_prefs (applier, prefs);

	gtk_widget_set_sensitive (color_frame, applier_render_color_p (applier, prefs));

	return FALSE;
}

static gboolean
realize_2_cb (Preferences *prefs) 
{
	gtk_idle_add ((GtkFunction) real_realize_cb, prefs);
	return FALSE;
}

static void
realize_cb (GtkWidget *widget, Preferences *prefs)
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
peditor_value_changed (GConfPropertyEditor *peditor, const gchar *key, const GConfValue *value, Preferences *prefs) 
{
	GConfEntry *entry;
	Applier *applier;
	GtkWidget *color_frame;

	entry = gconf_entry_new (key, value);
	preferences_merge_entry (prefs, entry);
	gconf_entry_free (entry);

	applier = g_object_get_data (G_OBJECT (prefs), "applier");

	if (GTK_WIDGET_REALIZED (applier_get_preview_widget (applier)))
		applier_apply_prefs (applier, PREFERENCES (prefs));

	if (!strcmp (key, "/background-properties/wallpaper-enabled") ||
	    !strcmp (key, "/background-properties/wallpaper-filename") ||
	    !strcmp (key, "/background-properties/wallpaper-type"))
	{
		color_frame = g_object_get_data (G_OBJECT (prefs), "color-frame");
		gtk_widget_set_sensitive (color_frame, applier_render_color_p (applier, prefs));
	}
}

/* Set up the property editors in the dialog. This also loads the preferences
 * and sets up the callbacks.
 */

static void
setup_dialog (GladeXML *dialog, GConfChangeSet *changeset, Applier *applier)
{
	GObject                       *prefs;
	GObject                       *peditor;
	GConfEngine                   *engine;

	/* Override the enabled setting to make sure background is enabled */
	engine = gconf_engine_get_default ();
	gconf_engine_set_bool (engine, "enabled", TRUE, NULL);

	/* Load preferences */
	prefs = preferences_new ();
	preferences_load (PREFERENCES (prefs));

	/* We need to be able to retrieve the applier and the color frame in
	   callbacks */
	g_object_set_data (prefs, "color-frame", WID ("color_frame"));
	g_object_set_data (prefs, "applier", applier);

	peditor = gconf_peditor_new_select_menu
		(changeset, "/background-properties/orientation", WID ("color_option"));
	g_signal_connect (peditor, "value-changed", (GCallback) peditor_value_changed, prefs);

	peditor = gconf_peditor_new_color
		(changeset, "/background-properties/color1", WID ("colorpicker1"));
	g_signal_connect (peditor, "value-changed", (GCallback) peditor_value_changed, prefs);

	peditor = gconf_peditor_new_color
		(changeset, "/background-properties/color2", WID ("colorpicker2"));
	g_signal_connect (peditor, "value-changed", (GCallback) peditor_value_changed, prefs);

	peditor = gconf_peditor_new_filename
		(changeset, "/background-properties/wallpaper-filename", WID ("image_fileentry"));
	g_signal_connect (peditor, "value-changed", (GCallback) peditor_value_changed, prefs);

	peditor = gconf_peditor_new_select_menu
		(changeset, "/background-properties/wallpaper-type", WID ("image_option"));
	g_signal_connect (peditor, "value-changed", (GCallback) peditor_value_changed, prefs);

	peditor = gconf_peditor_new_boolean
		(changeset, "/background-properties/wallpaper-enabled", WID ("picture_enabled_check"));
	g_signal_connect (peditor, "value-changed", (GCallback) peditor_value_changed, prefs);

	gconf_peditor_widget_set_guard (GCONF_PROPERTY_EDITOR (peditor), WID ("picture_frame"));

	/* Make sure preferences get applied to the preview */
	if (GTK_WIDGET_REALIZED (applier_get_preview_widget (applier)))
		applier_apply_prefs (applier, PREFERENCES (prefs));
	else
		g_signal_connect_after (G_OBJECT (applier_get_preview_widget (applier)), "realize",
					(GCallback) realize_cb, prefs);

	/* Make sure the preferences object gets destroyed when the dialog is
	   closed */
	g_object_weak_ref (G_OBJECT (dialog), (GWeakNotify) g_object_unref, prefs);
}

/* Construct the dialog */

static GladeXML *
create_dialog (Applier *applier) 
{
	GtkWidget *holder;
	GtkWidget *widget;
	GladeXML  *dialog;

	/* FIXME: What the hell is domain? */
	dialog = glade_xml_new (GNOMECC_GLADE_DIR "/background-properties.glade", "prefs_widget", NULL);
	widget = glade_xml_get_widget (dialog, "prefs_widget");

	/* Minor GUI addition */
	holder = WID ("prefs_widget");
	gtk_box_pack_start (GTK_BOX (holder), applier_get_preview_widget (applier), TRUE, TRUE, 0);
	gtk_widget_show_all (holder);

	g_object_weak_ref (G_OBJECT (widget), (GWeakNotify) g_object_unref, dialog);

	return dialog;
}

/* Callback issued when a button is clicked on the dialog */

static void
dialog_button_clicked_cb (GnomeDialog *dialog, gint button_number, GConfChangeSet *changeset) 
{
	if (button_number == 0) {
		gconf_engine_commit_change_set (gconf_engine_get_default (), changeset, TRUE, NULL);
		apply_settings ();
	}
	else if (button_number == 1) {
		gnome_dialog_close (dialog);
	}
}

int
main (int argc, char **argv) 
{
	GConfChangeSet *changeset;
	GladeXML       *dialog;
	GtkWidget      *dialog_win;
	GObject        *applier;

	static gboolean apply_only;
	static gboolean get_legacy;
	static struct poptOption cap_options[] = {
		{ "apply", '\0', POPT_ARG_NONE, &apply_only, 0,
		  N_("Just apply settings and quit"), NULL },
		{ "init-session-settings", '\0', POPT_ARG_NONE, &apply_only, 0,
		  N_("Just apply settings and quit"), NULL },
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

	if (apply_only) {
		apply_settings ();
	}
	else if (get_legacy) {
		get_legacy_settings ();
	} else {
		changeset = gconf_change_set_new ();
		applier = applier_new (APPLIER_PREVIEW);
		dialog = create_dialog (APPLIER (applier));
		setup_dialog (dialog, changeset, APPLIER (applier));

		dialog_win = gnome_dialog_new (_("Background properties"), GTK_STOCK_APPLY, GTK_STOCK_CLOSE, NULL);
		g_signal_connect (G_OBJECT (dialog_win), "clicked", (GCallback) dialog_button_clicked_cb, changeset);
		g_object_weak_ref (G_OBJECT (dialog_win), (GWeakNotify) g_object_unref, applier);
		g_object_weak_ref (G_OBJECT (dialog_win), (GWeakNotify) gtk_main_quit, NULL);
		gtk_box_pack_start (GTK_BOX (GNOME_DIALOG (dialog_win)->vbox), WID ("prefs_widget"), TRUE, TRUE, GNOME_PAD_SMALL);
		gtk_widget_show_all (dialog_win);

		gtk_main ();
		gconf_change_set_unref (changeset);
	}

	return 0;
}
