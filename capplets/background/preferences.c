/* -*- mode: c; style: linux -*- */

/* preferences.c
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
# include "config.h"
#endif

#include <stdlib.h>

#include <gnome.h>
#include <gdk-pixbuf/gdk-pixbuf-xlibrgb.h>
#include <bonobo.h>

#include "preferences.h"

/* Note that there is a really bad bug in Bonobo */

#undef BONOBO_RET_EX
#define BONOBO_RET_EX(ev)		\
	G_STMT_START{			\
		if (BONOBO_EX (ev))	\
			return;		\
	}G_STMT_END

/* Copied from bonobo-conf bonobo-config-database.c
 *
 * Note to Dietmar: You should include these functions in
 * bonobo-config-database.c
 */

#define MAKE_GET_SIMPLE(c_type, default, name, corba_tc, extract_fn)          \
static c_type local_bonobo_config_get_##name  (Bonobo_ConfigDatabase  db,     \
				               const char            *key,    \
				               CORBA_Environment     *opt_ev) \
{                                                                             \
	CORBA_any *value;                                                     \
	c_type retval;                                                        \
	if (!(value = bonobo_config_get_value (db, key, corba_tc, opt_ev)))   \
		return default;                                               \
	retval = extract_fn;                                                  \
	CORBA_free (value);                                                   \
	return retval;                                                        \
}

static GtkObjectClass *parent_class;

static void      preferences_init             (Preferences               *prefs);
static void      preferences_class_init       (PreferencesClass          *class);

static void      preferences_destroy          (GtkObject                 *object);

static GdkColor *read_color_from_string       (const gchar               *string);
static GdkColor *bonobo_color_to_gdk          (const Bonobo_Config_Color *color);

static gulong    local_bonobo_property_bag_client_get_value_gulong   (Bonobo_PropertyBag  pb,
								      const gchar        *propname,
								      CORBA_Environment  *ev);
static GdkColor *local_bonobo_property_bag_client_get_value_color    (Bonobo_PropertyBag  pb,
								      const gchar        *propname,
								      CORBA_Environment  *ev);
static gchar    *local_bonobo_property_bag_client_get_value_filename (Bonobo_PropertyBag  pb,
								      const gchar        *propname,
								      CORBA_Environment  *ev);

MAKE_GET_SIMPLE (gchar *, NULL, filename, TC_Bonobo_Config_FileName, g_strdup (((CORBA_char **) value->_value)[0]))
MAKE_GET_SIMPLE (GdkColor *, NULL, color, TC_Bonobo_Config_Color, bonobo_color_to_gdk ((Bonobo_Config_Color *) value->_value))

guint
preferences_get_type (void)
{
	static guint preferences_type = 0;

	if (!preferences_type) {
		GtkTypeInfo preferences_info = {
			"Preferences",
			sizeof (Preferences),
			sizeof (PreferencesClass),
			(GtkClassInitFunc) preferences_class_init,
			(GtkObjectInitFunc) preferences_init,
			(GtkArgSetFunc) NULL,
			(GtkArgGetFunc) NULL
		};

		preferences_type = 
			gtk_type_unique (gtk_object_get_type (), 
					 &preferences_info);
	}

	return preferences_type;
}

static void
preferences_init (Preferences *prefs)
{
	prefs->frozen             = FALSE;

	/* Load default values */
	prefs->color1             = read_color_from_string ("#39374b");
	prefs->color2             = read_color_from_string ("#42528f");
	prefs->enabled            = TRUE;
	prefs->wallpaper_enabled  = FALSE;
	prefs->gradient_enabled   = TRUE;
	prefs->orientation        = ORIENTATION_VERT;
	prefs->wallpaper_type     = WPTYPE_TILED;
	prefs->wallpaper_filename = NULL;
	prefs->wallpaper_sel_path = g_strdup (g_get_home_dir ());
	prefs->auto_apply         = TRUE;
	prefs->wallpapers         = NULL;
	prefs->adjust_opacity     = TRUE;
	prefs->opacity            = 255;
}

static void
preferences_class_init (PreferencesClass *class) 
{
	GtkObjectClass *object_class;

	object_class = (GtkObjectClass *) class;
	object_class->destroy = preferences_destroy;

	parent_class = 
		GTK_OBJECT_CLASS (gtk_type_class (gtk_object_get_type ()));
}

GtkObject *
preferences_new (void) 
{
	GtkObject *object;

	object = gtk_type_new (preferences_get_type ());
	PREFERENCES (object)->enabled = TRUE;

	return object;
}

GtkObject *
preferences_clone (const Preferences *prefs)
{
	GtkObject *object;
	Preferences *new_prefs;

	g_return_val_if_fail (prefs != NULL, NULL);
	g_return_val_if_fail (IS_PREFERENCES (prefs), NULL);

	object = preferences_new ();

	new_prefs = PREFERENCES (object);

	new_prefs->enabled            = prefs->enabled;
	new_prefs->gradient_enabled   = prefs->gradient_enabled;
	new_prefs->wallpaper_enabled  = prefs->wallpaper_enabled;
	new_prefs->orientation        = prefs->orientation;
	new_prefs->wallpaper_type     = prefs->wallpaper_type;

	if (prefs->color1)
		new_prefs->color1 = gdk_color_copy (prefs->color1);
	if (prefs->color2)
		new_prefs->color2 = gdk_color_copy (prefs->color2);

	new_prefs->wallpaper_filename = g_strdup (prefs->wallpaper_filename);
	new_prefs->wallpaper_sel_path = g_strdup (prefs->wallpaper_sel_path);;

	new_prefs->auto_apply         = prefs->auto_apply;
	new_prefs->adjust_opacity     = prefs->adjust_opacity;
	new_prefs->opacity            = prefs->opacity;

	return object;
}

static void
preferences_destroy (GtkObject *object) 
{
	Preferences *prefs;

	g_return_if_fail (object != NULL);
	g_return_if_fail (IS_PREFERENCES (object));

	prefs = PREFERENCES (object);

	g_free (prefs->wallpaper_filename);
	g_free (prefs->wallpaper_sel_path);

	parent_class->destroy (object);
}

GtkObject *
preferences_new_from_bonobo_pbag (Bonobo_PropertyBag pb, CORBA_Environment *ev)
{
	Preferences *prefs;

	g_return_val_if_fail (pb != CORBA_OBJECT_NIL, NULL);
	g_return_val_if_fail (ev != NULL, NULL);

	prefs = PREFERENCES (preferences_new ());
	preferences_load_from_bonobo_pbag (prefs, pb, ev);

	if (BONOBO_EX (ev)) {
		gtk_object_destroy (GTK_OBJECT (prefs));
		return NULL;
	}

	return GTK_OBJECT (prefs);
}

void
preferences_load_from_bonobo_pbag (Preferences        *prefs,
				   Bonobo_PropertyBag  pb,
				   CORBA_Environment  *ev) 
{
	g_return_if_fail (prefs != NULL);
	g_return_if_fail (IS_PREFERENCES (prefs));
	g_return_if_fail (pb != CORBA_OBJECT_NIL);
	g_return_if_fail (ev != NULL);

	prefs->enabled = bonobo_property_bag_client_get_value_gboolean (pb, "enabled", ev);

	if (BONOBO_EX (ev) && !strcmp (ev->_repo_id, "IDL:Bonobo/PropertyBag/NotFound:1.0")) {
		prefs->enabled = TRUE;
		CORBA_exception_init (ev);
	} else {
		BONOBO_RET_EX (ev);
	}

	prefs->wallpaper_type = local_bonobo_property_bag_client_get_value_gulong (pb, "wallpaper_type", ev); BONOBO_RET_EX (ev);
	prefs->wallpaper_filename = local_bonobo_property_bag_client_get_value_filename (pb, "wallpaper_filename", ev); BONOBO_RET_EX (ev);

	prefs->wallpaper_enabled = bonobo_property_bag_client_get_value_gboolean (pb, "wallpaper_enabled", ev);

	if (BONOBO_EX (ev) && !strcmp (ev->_repo_id, "IDL:Bonobo/PropertyBag/NotFound:1.0")) {
		prefs->wallpaper_enabled = (prefs->wallpaper_filename != NULL && strcmp (prefs->wallpaper_filename, "(none)"));
		CORBA_exception_init (ev);
	} else {
		BONOBO_RET_EX (ev);
	}

	prefs->color1 = local_bonobo_property_bag_client_get_value_color (pb, "color1", ev); BONOBO_RET_EX (ev);
	prefs->color2 = local_bonobo_property_bag_client_get_value_color (pb, "color2", ev); BONOBO_RET_EX (ev);

	prefs->opacity = bonobo_property_bag_client_get_value_glong (pb, "opacity", ev); BONOBO_RET_EX (ev);
	if (prefs->opacity >= 100 || prefs->opacity < 0)
		prefs->adjust_opacity = FALSE;

	prefs->orientation = local_bonobo_property_bag_client_get_value_gulong (pb, "orientation", ev); BONOBO_RET_EX (ev);

	if (prefs->orientation == ORIENTATION_SOLID)
		prefs->gradient_enabled = FALSE;
	else
		prefs->gradient_enabled = TRUE;
}

GtkObject *
preferences_new_from_bonobo_db (Bonobo_ConfigDatabase db, CORBA_Environment *ev)
{
	Preferences *prefs;

	g_return_val_if_fail (db != CORBA_OBJECT_NIL, NULL);
	g_return_val_if_fail (ev != NULL, NULL);
       
	prefs = PREFERENCES (preferences_new ());
	preferences_load_from_bonobo_db (prefs, db, ev);

	if (BONOBO_EX (ev)) {
		gtk_object_destroy (GTK_OBJECT (prefs));
		return NULL;
	}

	return GTK_OBJECT (prefs);
}

void
preferences_load_from_bonobo_db (Preferences           *prefs,
				 Bonobo_ConfigDatabase  db,
				 CORBA_Environment     *ev)
{
	g_return_if_fail (prefs != NULL);
	g_return_if_fail (IS_PREFERENCES (prefs));
	g_return_if_fail (db != CORBA_OBJECT_NIL);
	g_return_if_fail (ev != NULL);

	prefs->enabled = bonobo_config_get_boolean (db, CP "enabled", ev);

	if (BONOBO_EX (ev) && !strcmp (ev->_repo_id, "IDL:Bonobo/ConfigDatabase/NotFound:1.0")) {
		prefs->enabled = TRUE;
		CORBA_exception_init (ev);
	} else {
		BONOBO_RET_EX (ev);
	}

	prefs->orientation = bonobo_config_get_ulong (db, CP "orientation", ev); BONOBO_RET_EX (ev);

	if (prefs->orientation != ORIENTATION_SOLID)
		prefs->gradient_enabled = TRUE;
	else
		prefs->gradient_enabled = FALSE;

	prefs->wallpaper_type = bonobo_config_get_ulong (db, CP "wallpaper_type", ev); BONOBO_RET_EX (ev);
	prefs->wallpaper_filename = local_bonobo_config_get_filename (db, CP "wallpaper_filename", ev); BONOBO_RET_EX (ev);

	prefs->wallpaper_enabled = bonobo_config_get_boolean (db, CP "wallpaper_enabled", ev);

	if (BONOBO_EX (ev) && !strcmp (ev->_repo_id, "IDL:Bonobo/ConfigDatabase/NotFound:1.0")) {
		prefs->wallpaper_enabled = (prefs->wallpaper_filename != NULL && strcmp (prefs->wallpaper_filename, "(none)"));
		CORBA_exception_init (ev);
	} else {
		BONOBO_RET_EX (ev);
	}

	prefs->color1 = local_bonobo_config_get_color (db, CP "color1", ev); BONOBO_RET_EX (ev);
	prefs->color2 = local_bonobo_config_get_color (db, CP "color2", ev); BONOBO_RET_EX (ev);

	prefs->opacity = bonobo_config_get_long (db, CP "opacity", ev); BONOBO_RET_EX (ev);

	if (prefs->opacity >= 100 || prefs->opacity < 0)
		prefs->adjust_opacity = FALSE;
}

/* Parse the event name given (the event being notification of a property having
 * changed and apply that change to the preferences structure. Eliminates the
 * need to reload the structure entirely on every event notification
 */

void
preferences_apply_event (Preferences     *prefs,
			 const gchar     *event_name,
			 const CORBA_any *value) 
{
	const gchar *name;

	g_return_if_fail (prefs != NULL);
	g_return_if_fail (IS_PREFERENCES (prefs));
	g_return_if_fail (event_name != NULL);

	if (strncmp (event_name, "Bonobo/Property:change:", strlen ("Bonobo/Property:change:")))
		return;

	name = event_name + strlen ("Bonobo/Property:change:");

	if (!strcmp (name, "wallpaper_type")) {
		prefs->wallpaper_type = BONOBO_ARG_GET_GENERAL (value, TC_ulong, CORBA_long, NULL);
	}
	else if (!strcmp (name, "wallpaper_filename")) {
		if (!bonobo_arg_type_is_equal (value->_type, TC_Bonobo_Config_FileName, NULL)) {
			g_warning ("Filename property not of filename type");
			return;
		}

		prefs->wallpaper_filename = g_strdup (*((char **)value->_value));

		if (prefs->wallpaper_filename != NULL &&
		    strcmp (prefs->wallpaper_filename, "") != 0 &&
		    strcmp (prefs->wallpaper_filename, "(none)") != 0)
			prefs->wallpaper_enabled = TRUE;
		else
			prefs->wallpaper_enabled = FALSE;
	}
	else if (!strcmp (name, "color1")) {
		prefs->color1 = bonobo_color_to_gdk ((Bonobo_Config_Color *)value->_value);
	}
	else if (!strcmp (name, "color2")) {
		prefs->color2 = bonobo_color_to_gdk ((Bonobo_Config_Color *)value->_value);
	}
	else if (!strcmp (name, "opacity")) {
		prefs->opacity = BONOBO_ARG_GET_LONG (value);

		if (prefs->opacity >= 100)
			prefs->adjust_opacity = FALSE;
	}
	else if (!strcmp (name, "orientation")) {
		prefs->orientation = BONOBO_ARG_GET_GENERAL (value, TC_ulong, CORBA_long, NULL);

		if (prefs->orientation == ORIENTATION_SOLID)
			prefs->gradient_enabled = FALSE;
		else
			prefs->gradient_enabled = TRUE;
	}
	else if (!strcmp (name, "wallpaper_enabled")) {
		prefs->wallpaper_enabled = BONOBO_ARG_GET_BOOLEAN (value);
	} else {
		g_warning ("%s: Unknown property: %s", __FUNCTION__, name);
	}
}

/**
 * preferences_save:
 * @prefs:
 *
 * Save a preferences structure using the legacy gnome_config API
 **/

void
preferences_save (const Preferences *prefs)
{
	static const gint wallpaper_types[] = { 0, 1, 3, 2 };
	gchar *color;

	gnome_config_pop_prefix ();
	gnome_config_set_bool ("/Background/Default/Enabled", prefs->enabled);
	gnome_config_set_string ("/Background/Default/wallpaper",
				 (prefs->wallpaper_filename) ? prefs->wallpaper_filename : "none");
	gnome_config_set_int ("/Background/Default/wallpaperAlign", wallpaper_types[prefs->wallpaper_type]);

	color = g_strdup_printf ("#%02x%02x%02x",
		prefs->color1->red >> 8,
		prefs->color1->green >> 8,
		prefs->color1->blue >> 8);
	gnome_config_set_string ("/Background/Default/color1", color);
	g_free (color);

	color = g_strdup_printf ("#%02x%02x%02x",
		prefs->color2->red >> 8,
		prefs->color2->green >> 8,
		prefs->color2->blue >> 8);
	gnome_config_set_string ("/Background/Default/color2", color);
	g_free (color);

	gnome_config_set_string ("/Background/Default/simple",
		       		 (prefs->gradient_enabled) ? "gradient" : "solid");
	gnome_config_set_string ("/Background/Default/gradient",
				   (prefs->orientation == ORIENTATION_VERT) ? "vertical" : "horizontal");
	
	gnome_config_set_bool ("/Background/Default/adjustOpacity", prefs->adjust_opacity);
	gnome_config_set_int ("/Background/Default/opacity", prefs->opacity);

	gnome_config_sync ();
}

static GdkColor *
read_color_from_string (const gchar *string) 
{
	GdkColor *color;
	gint32 rgb;

	color = g_new0 (GdkColor, 1);

	gdk_color_parse (string, color);
	rgb = ((color->red >> 8) << 16) ||
		((color->green >> 8) << 8) ||
		(color->blue >> 8);
	color->pixel = xlib_rgb_xpixel_from_rgb (rgb);

	return color;
}

static GdkColor *
bonobo_color_to_gdk (const Bonobo_Config_Color *color)
{
	GdkColor *ret;
	
	g_return_val_if_fail (color != NULL, NULL);

	ret = g_new0 (GdkColor, 1);
	ret->red = color->r * 65535;
	ret->green = color->g * 65535;
	ret->blue = color->b * 65535;

	return ret;
}

static gulong
local_bonobo_property_bag_client_get_value_gulong (Bonobo_PropertyBag  pb,
						   const gchar        *propname,
						   CORBA_Environment  *ev) 
{
	BonoboArg *arg;
	gulong retval;

	arg = bonobo_property_bag_client_get_value_any (pb, propname, ev);
	if (BONOBO_EX (ev)) return 0;
	retval = BONOBO_ARG_GET_GENERAL (arg, TC_ulong, CORBA_long, ev);
	bonobo_arg_release (arg);

	return retval;
}

static GdkColor *
local_bonobo_property_bag_client_get_value_color (Bonobo_PropertyBag  pb,
						  const gchar        *propname,
						  CORBA_Environment  *ev) 
{
	BonoboArg *arg;
	GdkColor *retval;

	arg = bonobo_property_bag_client_get_value_any (pb, propname, ev);
	if (BONOBO_EX (ev)) return 0;
	retval = bonobo_color_to_gdk ((Bonobo_Config_Color *) arg->_value);
	bonobo_arg_release (arg);

	return retval;
}

static gchar *
local_bonobo_property_bag_client_get_value_filename (Bonobo_PropertyBag  pb,
						     const gchar        *propname,
						     CORBA_Environment  *ev) 
{
	BonoboArg *arg;
	gchar *retval;

	arg = bonobo_property_bag_client_get_value_any (pb, propname, ev);
	if (BONOBO_EX (ev)) return 0;
	retval = g_strdup (((CORBA_char **) arg->_value)[0]);
	bonobo_arg_release (arg);

	return retval;
}
