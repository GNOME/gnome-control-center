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

#include <glade/glade.h>
#include <gtk/gtksignal.h>
#include "capplet-util.h"
#include "applier.h"

static Applier *applier = NULL;

static void
apply_settings (Bonobo_ConfigDatabase db)
{
	CORBA_Environment ev;
	
	CORBA_exception_init (&ev);
	if (!applier)
		applier = APPLIER (applier_new ());

	applier_apply_prefs (applier, CORBA_OBJECT_NIL, db, &ev, TRUE, FALSE);
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
bonobo_config_set_filename (Bonobo_ConfigDatabase db,
			    const char *key,
			    const char *value,
			    CORBA_Environment *opt_ev)
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
	int val_ulong, val_long;

	COPY_FROM_LEGACY (filename, "/main/wallpaper_filename", string, "/Background/Default/wallpaper=none");
	COPY_FROM_LEGACY (ulong, "/main/wallpaper_type", int, "/Background/Default/wallpaperAlign=0");
	copy_color_from_legacy (db, "/main/color1", "/Background/Default/color1");
	copy_color_from_legacy (db, "/main/color2", "/Background/Default/color2");

	/* Code to deal with new enum - messy */
	val_ulong = -1;
	val_string = gnome_config_get_string_with_default ("/Background/Default/simple=solid", &def);
	if (!def)
	{
		if (!strcmp (val_string, "solid"))
			val_ulong = 0;
		else
		{
			g_free (val_string);
			val_string = gnome_config_get_string_with_default ("/Background/Default/gradient=vertical", &def);
			if (!def)
				val_ulong = !strcmp (val_string, "vertical");
		}
	}
	
	g_free (val_string);
	
	if (val_ulong != -1)
		bonobo_config_set_ulong (db, "/main/orientation", val_ulong, NULL);
	
	val_boolean = gnome_config_get_bool_with_default ("/Background/Default/adjustOpacity=true", &def);
	if (!def && val_boolean)
	{
		COPY_FROM_LEGACY (long, "/main/opacity", int, "/Background/Default/opacity=100");
	}
}

static void
property_change_cb (BonoboListener *listener,
		    char *event_name,
		    CORBA_any *any,
		    CORBA_Environment *ev,
		    Bonobo_PropertyBag pb)
{
	applier_apply_prefs (applier, pb, CORBA_OBJECT_NIL, ev, FALSE, TRUE);
}

static void
realize_cb (GtkWidget *widget, Bonobo_PropertyBag bag)
{
	CORBA_Environment ev;

	CORBA_exception_init (&ev);
	applier_apply_prefs (applier, bag, CORBA_OBJECT_NIL, &ev, FALSE, TRUE);
	CORBA_exception_free (&ev);
}

#define CUSTOM_CREATE_PEDITOR(type, corba_type, key, widget)                                              \
        {                                                                              \
		BonoboPEditor *ed = BONOBO_PEDITOR                                     \
			(bonobo_peditor_##type##_construct (WID (widget)));            \
		bonobo_peditor_set_property (ed, bag, key, TC_##corba_type, NULL);           \
	}


static void
setup_dialog (GtkWidget *widget, Bonobo_PropertyBag bag)
{
	BonoboPEditor *ed;
	GladeXML *dialog;

	dialog = gtk_object_get_data (GTK_OBJECT (widget), "glade-data");
	ed = BONOBO_PEDITOR (bonobo_peditor_option_construct (0, WID ("color_option")));
	bonobo_peditor_set_property (ed, bag, "orientation", TC_ulong, NULL);

	CUSTOM_CREATE_PEDITOR (color, Bonobo_Config_Color, "color1", "colorpicker1");	
	CUSTOM_CREATE_PEDITOR (color, Bonobo_Config_Color, "color2", "colorpicker2");	
	CUSTOM_CREATE_PEDITOR (filename, Bonobo_Config_FileName, "wallpaper_filename", "image_fileentry");	

	ed = BONOBO_PEDITOR (bonobo_peditor_option_construct (0, WID ("image_option")));
	bonobo_peditor_set_property (ed, bag, "wallpaper_type", TC_ulong, NULL);

	CUSTOM_CREATE_PEDITOR (int_range, long, "opacity", "opacity_spin");	

	bonobo_event_source_client_add_listener (bag, property_change_cb,
						 NULL, NULL, bag);

	gtk_signal_connect_after (GTK_OBJECT (applier_class_get_preview_widget ()), "realize", realize_cb, bag);
}

static GtkWidget*
create_dialog (void) 
{
	GtkWidget *holder;
	GtkWidget *widget;
	GladeXML *dialog;

	dialog = glade_xml_new (GNOMECC_GLADE_DIR "/background-properties.glade", "prefs_widget");
	widget = glade_xml_get_widget (dialog, "prefs_widget");
	gtk_object_set_data (GTK_OBJECT (widget), "glade-data", dialog);

	applier = APPLIER (applier_new ());

	/* Minor GUI addition */
	holder = WID ("preview_holder");
	gtk_box_pack_start (GTK_BOX (holder),
			    applier_class_get_preview_widget (),
			    TRUE, TRUE, 0);
	gtk_widget_show_all (holder);

	gtk_signal_connect_object (GTK_OBJECT (widget), "destroy",
				   GTK_SIGNAL_FUNC (gtk_object_destroy),
			   	   GTK_OBJECT (dialog));

	return widget;
}

int
main (int argc, char **argv) 
{
	glade_gnome_init ();
	capplet_init (argc, argv, apply_settings, create_dialog, setup_dialog, get_legacy_settings);

	gnome_window_icon_set_default_from_file
		(GNOMECC_ICONS_DIR"/gnome-ccbackground.png");
	return 0;
}
