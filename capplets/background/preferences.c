/* -*- mode: c; style: linux -*- */

/* preferences.c
 * Copyright (C) 2000 Helix Code, Inc.
 *
 * Written by Bradford Hovinen <hovinen@helixcode.com>
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
#include <capplet-widget.h>

#include "preferences.h"
#include "applier.h"

static GtkObjectClass *parent_class;
static Applier *applier = NULL;

static void preferences_init             (Preferences *prefs);
static void preferences_class_init       (PreferencesClass *class);

#ifdef BONOBO_CONF_ENABLE
#include <bonobo.h>
#else
static gint       xml_read_int           (xmlNodePtr node);
static xmlNodePtr xml_write_int          (gchar *name, 
					  gint number);
static gboolean   xml_read_bool          (xmlNodePtr node);
static xmlNodePtr xml_write_bool         (gchar *name,
					  gboolean value);

static gint apply_timeout_cb             (Preferences *prefs);
#endif

static GdkColor  *read_color_from_string (gchar *string);

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

	if (applier == NULL)
		applier = APPLIER (applier_new ());
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

void
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

#ifdef BONOBO_CONF_ENABLE

static GdkColor*
bonobo_color_to_gdk (Bonobo_Config_Color *color)
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
bonobo_property_bag_client_get_value_gulong (Bonobo_PropertyBag pb, gchar *propname, CORBA_Environment *ev) 
{
	BonoboArg *arg;
	gulong retval;

	arg = bonobo_property_bag_client_get_value_any (pb, propname, ev);
	if (BONOBO_EX (ev)) return 0;
	retval = BONOBO_ARG_GET_GENERAL (arg, TC_ulong, CORBA_long, ev);
	bonobo_arg_release (arg);

	return retval;
}

#define PB_GET_VALUE(v) (bonobo_property_bag_client_get_value_any (pb, (v), NULL))

GtkObject *
preferences_new_from_bonobo_pbag (Bonobo_PropertyBag pb, CORBA_Environment *ev)
{
	Preferences *prefs;

	g_return_val_if_fail (pb != CORBA_OBJECT_NIL, NULL);
	g_return_val_if_fail (ev != NULL, NULL);

	prefs = PREFERENCES (preferences_new ());
	preferences_load_from_bonobo_pbag (prefs, pb, ev);

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

	prefs->wallpaper_type = bonobo_property_bag_client_get_value_gulong (pb, "wallpaper_type", ev);
	prefs->wallpaper_filename = g_strdup (*((CORBA_char **)(PB_GET_VALUE ("wallpaper_filename"))->_value));

	prefs->wallpaper_enabled = bonobo_property_bag_client_get_value_gboolean (pb, "wallpaper_enabled", ev);
	
	prefs->color1 = bonobo_color_to_gdk ((Bonobo_Config_Color *)(PB_GET_VALUE ("color1"))->_value);
	prefs->color2 = bonobo_color_to_gdk ((Bonobo_Config_Color *)(PB_GET_VALUE ("color2"))->_value);

	prefs->opacity = BONOBO_ARG_GET_LONG (PB_GET_VALUE ("opacity"));
	if (prefs->opacity >= 100 || prefs->opacity < 0)
		prefs->adjust_opacity = FALSE;

	prefs->orientation = bonobo_property_bag_client_get_value_gulong (pb, "orientation", ev);

	if (prefs->orientation == ORIENTATION_SOLID)
		prefs->gradient_enabled = FALSE;
	else
		prefs->gradient_enabled = TRUE;
}

#define DB_GET_VALUE(v) (bonobo_config_get_value (db, (v), NULL, NULL))

GtkObject *
preferences_new_from_bonobo_db (Bonobo_ConfigDatabase db, CORBA_Environment *ev)
{
	Preferences *prefs;

	g_return_val_if_fail (db != CORBA_OBJECT_NIL, NULL);
	g_return_val_if_fail (ev != NULL, NULL);
       
	prefs = PREFERENCES (preferences_new ());
	preferences_load_from_bonobo_db (prefs, db, ev);

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

	prefs->enabled = bonobo_config_get_boolean (db, "/main/enabled", NULL);
	prefs->orientation = bonobo_config_get_ulong (db, "/main/orientation", NULL);

	if (prefs->orientation != ORIENTATION_SOLID)
		prefs->gradient_enabled = TRUE;
	else
		prefs->gradient_enabled = FALSE;

	prefs->wallpaper_type = bonobo_config_get_ulong (db, "/main/wallpaper_type", NULL);
	prefs->wallpaper_filename = g_strdup (*((CORBA_char **)(DB_GET_VALUE ("/main/wallpaper_filename"))->_value));
	
	prefs->wallpaper_enabled = BONOBO_ARG_GET_BOOLEAN (DB_GET_VALUE ("/main/wallpaper_enabled"));

	prefs->color1 = bonobo_color_to_gdk ((Bonobo_Config_Color *)(DB_GET_VALUE ("/main/color1"))->_value);
	prefs->color2 = bonobo_color_to_gdk ((Bonobo_Config_Color *)(DB_GET_VALUE ("/main/color2"))->_value);

	prefs->opacity = BONOBO_ARG_GET_LONG (DB_GET_VALUE ("/main/opacity"));
	if (prefs->opacity >= 100 || prefs->opacity < 0)
		prefs->adjust_opacity = FALSE;
}

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

#else /* !BONOBO_CONF_ENABLE */

void
preferences_load (Preferences *prefs) 
{
	gchar *string, *wp, *wp1;
	int i, wps;

	g_return_if_fail (prefs != NULL);
	g_return_if_fail (IS_PREFERENCES (prefs));

	if (prefs->color1) g_free (prefs->color1);
	string = gnome_config_get_string
		("/Background/Default/color1=#39374b");
	prefs->color1 = read_color_from_string (string);
	g_free (string);

	if (prefs->color2) g_free (prefs->color2);
	string = gnome_config_get_string
		("/Background/Default/color2=#42528f");
	prefs->color2 = read_color_from_string (string);
	g_free (string);

	string = gnome_config_get_string ("/Background/Default/Enabled=True");
	prefs->enabled = !(g_strcasecmp (string, "True"));
	g_free (string);

	string = gnome_config_get_string ("/Background/Default/type=simple");
	if (!g_strcasecmp (string, "wallpaper"))
		prefs->wallpaper_enabled = TRUE;
	else if (g_strcasecmp (string, "simple"))
		prefs->wallpaper_enabled = FALSE;
	g_free (string);

	string = gnome_config_get_string ("/Background/Default/simple=gradent");
	if (!g_strcasecmp (string, "gradient"))
		prefs->gradient_enabled = TRUE;
	else if (g_strcasecmp (string, "solid"))
		prefs->gradient_enabled = FALSE;
	g_free (string);

	string = gnome_config_get_string
		("/Background/Default/gradient=vertical");
	if (!g_strcasecmp (string, "vertical"))
		prefs->orientation = ORIENTATION_VERT;
	else if (!g_strcasecmp (string, "horizontal"))
		prefs->orientation = ORIENTATION_HORIZ;
	g_free (string);

	prefs->wallpaper_type = 
		gnome_config_get_int ("/Background/Default/wallpaperAlign=0");

	prefs->wallpaper_filename = 
		gnome_config_get_string
		("/Background/Default/wallpaper=(None)");
	prefs->wallpaper_sel_path = 
		gnome_config_get_string 
		("/Background/Default/wallpapers_dir=./");

	prefs->auto_apply =
		gnome_config_get_bool ("/Background/Default/autoApply=true");

	if (!g_strcasecmp (prefs->wallpaper_filename, "(None)")) {
		g_free (prefs->wallpaper_filename);
		prefs->wallpaper_filename = NULL;
		prefs->wallpaper_enabled = FALSE;
	} else {
		prefs->wallpaper_enabled = TRUE;
	}
	
	wps = gnome_config_get_int ("/Background/Default/wallpapers=0");

	for (i = 0; i < wps; i++) {
		wp = g_strdup_printf ("/Background/Default/wallpaper%d", i+1);
		wp1 = gnome_config_get_string (wp);
		g_free (wp);

		if (wp1 == NULL) continue;			

		prefs->wallpapers = g_slist_prepend (prefs->wallpapers, wp1);
	}

	prefs->wallpapers = g_slist_reverse (prefs->wallpapers);

	prefs->adjust_opacity =
		gnome_config_get_bool
		("/Background/Default/adjustOpacity=false"); 

	prefs->opacity =
		gnome_config_get_int ("/Background/Default/opacity=255");
}

void 
preferences_save (Preferences *prefs) 
{
	char buffer[16];
	char *wp;
	GSList *item;
	int i;

	g_return_if_fail (prefs != NULL);
	g_return_if_fail (IS_PREFERENCES (prefs));

	snprintf (buffer, sizeof(buffer), "#%02x%02x%02x",
		  prefs->color1->red >> 8,
		  prefs->color1->green >> 8,
		  prefs->color1->blue >> 8);
	gnome_config_set_string ("/Background/Default/color1", buffer);
	snprintf (buffer, sizeof(buffer), "#%02x%02x%02x",
		  prefs->color2->red >> 8,
		  prefs->color2->green >> 8,
		  prefs->color2->blue >> 8);
	gnome_config_set_string ("/Background/Default/color2", buffer);

	gnome_config_set_string ("/Background/Default/Enabled",
				 (prefs->enabled) ? "True" : "False");

	gnome_config_set_string ("/Background/Default/simple",
				 (prefs->gradient_enabled) ? 
				 "gradient" : "solid");
	gnome_config_set_string ("/Background/Default/gradient",
				 (prefs->orientation == ORIENTATION_VERT) ?
				 "vertical" : "horizontal");
    
	gnome_config_set_string ("/Background/Default/wallpaper",
				 (prefs->wallpaper_enabled) ? 
				 prefs->wallpaper_filename : "(none)");
	gnome_config_set_int ("/Background/Default/wallpaperAlign", 
			      prefs->wallpaper_type);

	gnome_config_set_int ("/Background/Default/wallpapers",
			      g_slist_length (prefs->wallpapers));

	for (i = 1, item = prefs->wallpapers; item; i++, item = item->next) {
		wp = g_strdup_printf ("/Background/Default/wallpaper%d", i);
		gnome_config_set_string (wp, (char *)item->data);
		g_free (wp);
	}

	gnome_config_set_bool ("/Background/Default/autoApply", prefs->auto_apply);
	gnome_config_set_bool ("/Background/Default/adjustOpacity", prefs->adjust_opacity);
	gnome_config_set_int ("/Background/Default/opacity", 
			      prefs->opacity);

	gnome_config_sync ();
}

void
preferences_changed (Preferences *prefs) 
{
	/* FIXME: This is a really horrible kludge... */
	if (prefs->frozen > 1) return;

	if (prefs->frozen == 0) {
		if (prefs->timeout_id)
			gtk_timeout_remove (prefs->timeout_id);

#if 0
		if (prefs->auto_apply)
			prefs->timeout_id = 
				gtk_timeout_add
				(2000, (GtkFunction) apply_timeout_cb, prefs);
#endif
	}

	applier_apply_prefs (applier, prefs, FALSE, TRUE);
}

void
preferences_apply_now (Preferences *prefs)
{
	g_return_if_fail (prefs != NULL);
	g_return_if_fail (IS_PREFERENCES (prefs));

	if (prefs->timeout_id)
		gtk_timeout_remove (prefs->timeout_id);

	prefs->timeout_id = 0;

	applier_apply_prefs (applier, prefs, TRUE, FALSE);
}

void
preferences_apply_preview (Preferences *prefs)
{
	g_return_if_fail (prefs != NULL);
	g_return_if_fail (IS_PREFERENCES (prefs));

	applier_apply_prefs (applier, prefs, FALSE, TRUE);
}

void 
preferences_freeze (Preferences *prefs) 
{
	prefs->frozen++;
}

void 
preferences_thaw (Preferences *prefs) 
{
	if (prefs->frozen > 0) prefs->frozen--;
}

Preferences *
preferences_read_xml (xmlDocPtr xml_doc) 
{
	Preferences *prefs;
	xmlNodePtr root_node, node;
	char *str;

	prefs = PREFERENCES (preferences_new ());

	root_node = xmlDocGetRootElement (xml_doc);

	if (strcmp (root_node->name, "background-properties"))
		return NULL;

	prefs->wallpaper_enabled = FALSE;
	prefs->gradient_enabled = FALSE;
	prefs->orientation = ORIENTATION_VERT;

	if (prefs->color1) {
		g_free (prefs->color1);
		prefs->color1 = NULL;
	}

	if (prefs->color2) {
		g_free (prefs->color2);
		prefs->color2 = NULL;
	}

	for (node = root_node->childs; node; node = node->next) {
		if (!strcmp (node->name, "bg-color1"))
			prefs->color1 = read_color_from_string
				(xmlNodeGetContent (node));
		else if (!strcmp (node->name, "bg-color2"))
			prefs->color2 = read_color_from_string
				(xmlNodeGetContent (node));
		else if (!strcmp (node->name, "enabled"))
			prefs->enabled = xml_read_bool (node);
		else if (!strcmp (node->name, "wallpaper"))
			prefs->wallpaper_enabled = xml_read_bool (node);
		else if (!strcmp (node->name, "gradient"))
			prefs->gradient_enabled = xml_read_bool (node);
		else if (!strcmp (node->name, "orientation")) {
			str = xmlNodeGetContent (node);

			if (!g_strcasecmp (str, "horizontal"))
				prefs->orientation = ORIENTATION_HORIZ;
			else if (!g_strcasecmp (str, "vertical"))
				prefs->orientation = ORIENTATION_VERT;
		}
		else if (!strcmp (node->name, "wallpaper-type"))
			prefs->wallpaper_type = xml_read_int (node);
		else if (!strcmp (node->name, "wallpaper-filename"))
			prefs->wallpaper_filename = 
				g_strdup (xmlNodeGetContent (node));
		else if (!strcmp (node->name, "wallpaper-sel-path"))
			prefs->wallpaper_sel_path = 
				g_strdup (xmlNodeGetContent (node));
		else if (!strcmp (node->name, "auto-apply"))
			prefs->auto_apply = xml_read_bool (node);
		else if (!strcmp (node->name, "adjust-opacity"))
			prefs->adjust_opacity = xml_read_bool (node);
		else if (!strcmp (node->name, "opacity"))
			prefs->opacity = xml_read_int (node);
	}

	return prefs;
}

xmlDocPtr 
preferences_write_xml (Preferences *prefs) 
{
	xmlDocPtr doc;
	xmlNodePtr node;
	char tmp[16];

	doc = xmlNewDoc ("1.0");

	node = xmlNewDocNode (doc, NULL, "background-properties", NULL);

	snprintf (tmp, sizeof (tmp), "#%02x%02x%02x", 
		  prefs->color1->red >> 8,
		  prefs->color1->green >> 8,
		  prefs->color1->blue >> 8);
	xmlNewChild (node, NULL, "bg-color1", tmp);

	snprintf (tmp, sizeof (tmp), "#%02x%02x%02x", 
		  prefs->color2->red >> 8,
		  prefs->color2->green >> 8,
		  prefs->color2->blue >> 8);
	xmlNewChild (node, NULL, "bg-color2", tmp);

	xmlAddChild (node, xml_write_bool ("enabled", prefs->enabled));
	xmlAddChild (node, xml_write_bool ("wallpaper",
					   prefs->wallpaper_enabled));
	xmlAddChild (node, xml_write_bool ("gradient",
					   prefs->gradient_enabled));

	xmlNewChild (node, NULL, "orientation", 
		     (prefs->orientation == ORIENTATION_VERT) ?
		     "vertical" : "horizontal");

	xmlAddChild (node, xml_write_int ("wallpaper-type",
					  prefs->wallpaper_type));

	xmlNewChild (node, NULL, "wallpaper-filename", 
		     prefs->wallpaper_filename);
	xmlNewChild (node, NULL, "wallpaper-sel-path", 
		     prefs->wallpaper_sel_path);

	xmlAddChild (node, xml_write_bool ("auto-apply",
					   prefs->auto_apply));

	if (prefs->adjust_opacity)
		xmlNewChild (node, NULL, "adjust-opacity", NULL);
	xmlAddChild (node, xml_write_int ("opacity",
					  prefs->opacity));

	xmlDocSetRootElement (doc, node);

	return doc;
}

/* Read a numeric value from a node */

static gint
xml_read_int (xmlNodePtr node) 
{
	char *text;

	text = xmlNodeGetContent (node);

	if (text == NULL) 
		return 0;
	else
		return atoi (text);
}

/* Write out a numeric value in a node */

static xmlNodePtr
xml_write_int (gchar *name, gint number) 
{
	xmlNodePtr node;
	gchar *str;

	g_return_val_if_fail (name != NULL, NULL);

	str = g_strdup_printf ("%d", number);
	node = xmlNewNode (NULL, name);
	xmlNodeSetContent (node, str);
	g_free (str);

	return node;
}

/* Read a boolean value from a node */

static gboolean
xml_read_bool (xmlNodePtr node) 
{
	char *text;

	text = xmlNodeGetContent (node);

	if (text != NULL && !g_strcasecmp (text, "true")) 
		return TRUE;
	else
		return FALSE;
}

/* Write out a boolean value in a node */

static xmlNodePtr
xml_write_bool (gchar *name, gboolean value) 
{
	xmlNodePtr node;

	g_return_val_if_fail (name != NULL, NULL);

	node = xmlNewNode (NULL, name);

	if (value)
		xmlNodeSetContent (node, "true");
	else
		xmlNodeSetContent (node, "false");

	return node;
}

static gint 
apply_timeout_cb (Preferences *prefs) 
{
	preferences_apply_now (prefs);

	return TRUE;
}

#endif /* BONOBO_CONF_ENABLE */

static GdkColor *
read_color_from_string (gchar *string) 
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

/* It'd be nice if we could just get the pixbuf the applier uses, for
 * efficiency's sake */
gboolean
preferences_need_color_opts (Preferences *prefs, GdkPixbuf *wallpaper_pixbuf)
{
	int s_width, s_height;
	int p_width, p_height;
	
	g_return_val_if_fail (prefs != NULL, TRUE);

	if (!(prefs->wallpaper_enabled && prefs->wallpaper_filename))
		return TRUE;

	if (!wallpaper_pixbuf)
		return TRUE;

	p_width = gdk_pixbuf_get_width (wallpaper_pixbuf);
	p_height = gdk_pixbuf_get_height (wallpaper_pixbuf);
	
	s_width = gdk_screen_width ();
	s_height = gdk_screen_height ();

	switch (prefs->wallpaper_type)
	{
		case WPTYPE_CENTERED:
			if (p_width >= s_width && p_height >= s_height)
				return FALSE;
			else
				return TRUE;
			break;
		case WPTYPE_SCALED_ASPECT:
			if (s_width == p_width && s_height == p_height)
				return FALSE;
			else if (((double) s_width / (double) s_height)
				 == ((double) p_width / (double) p_height))
				return FALSE;
			else
				return TRUE;
			break;
		default:
			return FALSE;
	}
}
