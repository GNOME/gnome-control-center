/* -*- mode: c; style: linux -*- */

/* main.c
 * Copyright (C) 2000-2001 Ximian, Inc.
 *
 * Written by: Bradford Hovinen <hovinen@ximian.com>
 *             Richard Hestilow <hestilow@ximian.com>
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
#include <bonobo/bonobo-property-bag-client.h>
#include <glade/glade.h>
#include <gtk/gtksignal.h>
#include "capplet-util.h"
#include "applier.h"

static void
bonobo_config_set_filename (Bonobo_ConfigDatabase db,
			    const char *key,
			    const char *value,
			    CORBA_Environment *opt_ev);

/* Popt option for compat reasons */
static gchar *background_image = NULL;

const struct poptOption options [] = {
	{ "background-image", 'b', POPT_ARG_STRING, &background_image, 0,
	  N_("Set background image."), N_("IMAGE-FILE") },
	{NULL, '\0', 0, NULL, 0}
};

static void
apply_settings (Bonobo_ConfigDatabase db)
{
	GtkObject         *prefs;
	Applier           *applier;
	CORBA_Environment  ev;
	
	CORBA_exception_init (&ev);

	applier = APPLIER (applier_new ());

	/* Hackity hackty */
	if (background_image != NULL) {
		bonobo_config_set_filename (db, "/main/wallpaper_filename", background_image, NULL);
		Bonobo_ConfigDatabase_sync (db, &ev);
	}

	prefs = preferences_new_from_bonobo_db (db, &ev);

	if (BONOBO_EX (&ev) || prefs == NULL) {
		g_critical ("Could not retrieve configuration from database (%s)", ev._repo_id);
	} else {
		applier_apply_prefs (applier, PREFERENCES (prefs), TRUE, FALSE);
		gtk_object_destroy (GTK_OBJECT (prefs));
	}

	gtk_object_destroy (GTK_OBJECT (applier));

	CORBA_exception_free (&ev);
}

static CORBA_any*
gdk_color_to_bonobo (const gchar *colorstr)
{
	GdkColor tmp;
	CORBA_Environment ev;
	DynamicAny_DynAny dyn;
	CORBA_any *any;
	
	g_return_val_if_fail (colorstr != NULL, NULL);

	CORBA_exception_init (&ev);
	
	gdk_color_parse (colorstr, &tmp);
	
	dyn = CORBA_ORB_create_dyn_struct (bonobo_orb (),
					   TC_Bonobo_Config_Color, &ev);

	DynamicAny_DynAny_insert_double (dyn, ((double)tmp.red)/65535, &ev);
	DynamicAny_DynAny_next (dyn, &ev);
	DynamicAny_DynAny_insert_double (dyn, ((double)tmp.green)/65535, &ev);
	DynamicAny_DynAny_next (dyn, &ev);
	DynamicAny_DynAny_insert_double (dyn, ((double)tmp.blue)/65535, &ev);
	DynamicAny_DynAny_next (dyn, &ev);
	DynamicAny_DynAny_insert_double (dyn, 0, &ev);

	any = DynamicAny_DynAny_to_any (dyn, &ev);

	CORBA_Object_release ((CORBA_Object) dyn, &ev);
	CORBA_exception_free (&ev);

	return any;
}

static void
copy_color_from_legacy (Bonobo_ConfigDatabase db,
			const gchar *key, const gchar *legacy_key)
{
	gboolean def;
	gchar *val_string;
       
	g_return_if_fail (key != NULL);
	g_return_if_fail (legacy_key != NULL);

	val_string = gnome_config_get_string_with_default (legacy_key, &def);

	if (!def)
	{
		CORBA_any *color = gdk_color_to_bonobo (val_string);
		bonobo_config_set_value (db, key, color, NULL);
		bonobo_arg_release (color);
	}
	
	g_free (val_string);
}

static void
bonobo_config_set_filename (Bonobo_ConfigDatabase  db,
			    const char            *key,
			    const char            *value,
			    CORBA_Environment     *opt_ev)
{
	CORBA_any *any;
	
	any = bonobo_arg_new (TC_Bonobo_Config_FileName);
	*((CORBA_char **)(any->_value)) = CORBA_string_dup ((value)?(value):"");
	bonobo_config_set_value (db, key, any, opt_ev);
	bonobo_arg_release (any);	
}

static void
get_legacy_settings (Bonobo_ConfigDatabase db) 
{
	gboolean val_boolean, def;
	gchar *val_string, *val_filename;
	int val_ulong = -1, val_long = -1;

	static const int wallpaper_types[] = { 0, 1, 3, 2 };

	COPY_FROM_LEGACY (boolean, "/main/enabled", bool, "/Background/Default/Enabled=true");
	COPY_FROM_LEGACY (filename, "/main/wallpaper_filename", string, "/Background/Default/wallpaper=(none)");

	if (val_filename != NULL && strcmp (val_filename, "(none)"))
		bonobo_config_set_boolean (db, "/main/wallpaper_enabled", TRUE, NULL);
	else
		bonobo_config_set_boolean (db, "/main/wallpaper_enabled", FALSE, NULL);

	val_ulong = gnome_config_get_int ("/Background/Default/wallpaperAlign=0");
	bonobo_config_set_ulong (db, "/main/wallpaper_type", wallpaper_types[val_ulong], NULL);

	copy_color_from_legacy (db, "/main/color1", "/Background/Default/color1");
	copy_color_from_legacy (db, "/main/color2", "/Background/Default/color2");

	/* Code to deal with new enum - messy */
	val_ulong = -1;
	val_string = gnome_config_get_string_with_default ("/Background/Default/simple=solid", &def);
	if (!def) {
		if (!strcmp (val_string, "solid")) {
			val_ulong = ORIENTATION_SOLID;
		} else {
			g_free (val_string);
			val_string = gnome_config_get_string_with_default ("/Background/Default/gradient=vertical", &def);
			if (!def)
				val_ulong = (!strcmp (val_string, "vertical")) ? ORIENTATION_VERT : ORIENTATION_HORIZ;
		}
	}

	g_free (val_string);

	if (val_ulong != -1)
		bonobo_config_set_ulong (db, "/main/orientation", val_ulong, NULL);

	val_boolean = gnome_config_get_bool_with_default ("/Background/Default/adjustOpacity=true", &def);

	if (!def && val_boolean)
		COPY_FROM_LEGACY (long, "/main/opacity", int, "/Background/Default/opacity=100");
}

static void
property_change_cb (BonoboListener     *listener,
		    char               *event_name,
		    CORBA_any          *any,
		    CORBA_Environment  *ev,
		    Preferences        *prefs)
{
	GladeXML *dialog;
	Applier  *applier;
	
	g_return_if_fail (prefs != NULL);
	g_return_if_fail (IS_PREFERENCES (prefs));

	if (GTK_OBJECT_DESTROYED (prefs))
		return;

	dialog = gtk_object_get_data (GTK_OBJECT (prefs), "glade-data");

	preferences_apply_event (prefs, event_name, any);
	applier = gtk_object_get_data (GTK_OBJECT (WID ("prefs_widget")), "applier");
	applier_apply_prefs (applier, prefs, FALSE, TRUE);

	if (!strcmp (event_name, "Bonobo/Property:change:wallpaper_type")
	    || !strcmp (event_name, "Bonobo/Property:change:wallpaper_filename")
	    || !strcmp (event_name, "Bonobo/Property:change:wallpaper_enabled"))
		gtk_widget_set_sensitive
			(WID ("color_frame"), applier_render_color_p (applier));
}

static gboolean
real_realize_cb (Preferences *prefs) 
{
	GladeXML *dialog;
	Applier  *applier;

	g_return_val_if_fail (prefs != NULL, TRUE);
	g_return_val_if_fail (IS_PREFERENCES (prefs), TRUE);

	if (GTK_OBJECT_DESTROYED (prefs))
		return FALSE;

	dialog = gtk_object_get_data (GTK_OBJECT (prefs), "glade-data");
	applier = gtk_object_get_data (GTK_OBJECT (WID ("prefs_widget")), "applier");

	applier_apply_prefs (applier, prefs, FALSE, TRUE);

	gtk_widget_set_sensitive (WID ("color_frame"), applier_render_color_p (applier));

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

#define CUSTOM_CREATE_PEDITOR(type, corba_type, key, widget)                           \
        {                                                                              \
		BonoboPEditor *ed = BONOBO_PEDITOR                                     \
			(bonobo_peditor_##type##_construct (WID (widget)));            \
		bonobo_peditor_set_property (ed, bag, key, TC_##corba_type, NULL);     \
	}

static void
setup_dialog (GtkWidget *widget, Bonobo_PropertyBag bag)
{
	GladeXML          *dialog;
	Applier           *applier;
	GtkObject         *prefs;

	CORBA_Environment  ev;

	CORBA_exception_init (&ev);

	dialog = gtk_object_get_data (GTK_OBJECT (widget), "glade-data");
	CUSTOM_CREATE_PEDITOR (option_menu, ulong, "orientation", "color_option");	

	CUSTOM_CREATE_PEDITOR (color, Bonobo_Config_Color, "color1", "colorpicker1");	
	CUSTOM_CREATE_PEDITOR (color, Bonobo_Config_Color, "color2", "colorpicker2");	
	CUSTOM_CREATE_PEDITOR (filename, Bonobo_Config_FileName, "wallpaper_filename", "image_fileentry");	

	CUSTOM_CREATE_PEDITOR (option_menu, ulong, "wallpaper_type", "image_option");	
	CUSTOM_CREATE_PEDITOR (int_range, long, "opacity", "opacity_spin");

	CREATE_PEDITOR (boolean, "wallpaper_enabled", "picture_enabled_check");

	bonobo_peditor_set_guard (WID ("picture_frame"), bag, "wallpaper_enabled");
			
	/* Disable opacity controls */
	gtk_widget_hide (WID ("opacity_spin"));
	gtk_widget_hide (WID ("opacity_label"));

	bonobo_property_bag_client_set_value_gboolean (bag, "enabled", TRUE, NULL);

	prefs = preferences_new_from_bonobo_pbag (bag, &ev);

	if (BONOBO_EX (&ev) || prefs == NULL)
		g_error ("Could not retrieve configuration from property bag (%s)", ev._repo_id);

	gtk_object_set_data (prefs, "glade-data", dialog);
	bonobo_event_source_client_add_listener (bag, (BonoboListenerCallbackFn) property_change_cb,
						 NULL, NULL, prefs);

	applier = gtk_object_get_data (GTK_OBJECT (widget), "applier");

	if (GTK_WIDGET_REALIZED (applier_get_preview_widget (applier)))
		applier_apply_prefs (applier, PREFERENCES (prefs), FALSE, TRUE);
	else
		gtk_signal_connect_after (GTK_OBJECT (applier_get_preview_widget (applier)), "realize", realize_cb, prefs);

	gtk_signal_connect_object (GTK_OBJECT (widget), "destroy",
				   GTK_SIGNAL_FUNC (gtk_object_destroy), prefs);

	CORBA_exception_free (&ev);
}

static GtkWidget*
create_dialog (void) 
{
	GtkWidget *holder;
	GtkWidget *widget;
	GladeXML  *dialog;
	Applier   *applier;

	dialog = glade_xml_new (GNOMECC_GLADE_DIR "/background-properties.glade", "prefs_widget");
	widget = glade_xml_get_widget (dialog, "prefs_widget");
	gtk_object_set_data (GTK_OBJECT (widget), "glade-data", dialog);

	applier = APPLIER (applier_new ());
	gtk_object_set_data (GTK_OBJECT (widget), "applier", applier);
	gtk_signal_connect_object (GTK_OBJECT (widget), "destroy", GTK_SIGNAL_FUNC (gtk_object_destroy), GTK_OBJECT (applier));

	/* Minor GUI addition */
	holder = WID ("preview_holder");
	gtk_box_pack_start (GTK_BOX (holder), applier_get_preview_widget (applier), TRUE, TRUE, 0);
	gtk_widget_show_all (holder);

	gtk_signal_connect_object (GTK_OBJECT (widget), "destroy", GTK_SIGNAL_FUNC (gtk_object_destroy), GTK_OBJECT (dialog));

	return widget;
}

int
main (int argc, char **argv) 
{
	const gchar* legacy_files[] = { "Background", NULL };

	glade_gnome_init ();
	gnomelib_register_popt_table (options, "background options");

	capplet_init (argc, argv, legacy_files, apply_settings, create_dialog, setup_dialog, get_legacy_settings);

	return 0;
}
