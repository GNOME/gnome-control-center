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

#include "preferences.h"

static GtkObjectClass *parent_class;

static void preferences_init             (Preferences *prefs);
static void preferences_class_init       (PreferencesClass *class);

static gint       xml_read_int           (xmlNodePtr node,
					  gchar *propname);
static xmlNodePtr xml_write_int          (gchar *name, 
					  gchar *propname, 
					  gint number);

static gint apply_timeout_cb             (Preferences *prefs);

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
	prefs->frozen = FALSE;
	prefs->gnome_prefs = g_new0 (GnomePreferences, 1);
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

	return object;
}

GtkObject *
preferences_clone (Preferences *prefs)
{
	GtkObject *object;
	Preferences *new_prefs;

	g_return_val_if_fail (prefs != NULL, NULL);
	g_return_val_if_fail (IS_PREFERENCES (prefs), NULL);
	g_return_val_if_fail (prefs->gnome_prefs != NULL, NULL);

	object = preferences_new ();

	new_prefs = PREFERENCES (object);
	memcpy (new_prefs->gnome_prefs, prefs->gnome_prefs, 
		sizeof (GnomePreferences));
	new_prefs->dialog_use_icons = prefs->dialog_use_icons;

	return object;
}

void
preferences_destroy (GtkObject *object) 
{
	Preferences *prefs;

	g_return_if_fail (object != NULL);
	g_return_if_fail (IS_PREFERENCES (object));

	prefs = PREFERENCES (object);

	if (prefs->gnome_prefs) g_free (prefs->gnome_prefs);

	parent_class->destroy (object);
}

void
preferences_load (Preferences *prefs) 
{
	g_return_if_fail (prefs != NULL);
	g_return_if_fail (IS_PREFERENCES (prefs));
	g_return_if_fail (prefs->gnome_prefs != NULL);

	gnome_preferences_load_custom (prefs->gnome_prefs);

	prefs->dialog_use_icons = 
		gnome_config_get_bool ("/Gnome/Icons/ButtonUseIcons=true");
}

void 
preferences_save (Preferences *prefs) 
{
	g_return_if_fail (prefs != NULL);
	g_return_if_fail (IS_PREFERENCES (prefs));
	g_return_if_fail (prefs->gnome_prefs != NULL);

	gnome_preferences_save_custom (prefs->gnome_prefs);

	gnome_config_set_bool ("/Gnome/Icons/ButtonUseIcons", 
			       prefs->dialog_use_icons);
	gnome_config_sync ();
}

void
preferences_changed (Preferences *prefs) 
{
	if (prefs->frozen) return;

	if (prefs->timeout_id)
		gtk_timeout_remove (prefs->timeout_id);
}

void
preferences_apply_now (Preferences *prefs)
{
	g_return_if_fail (prefs != NULL);
	g_return_if_fail (IS_PREFERENCES (prefs));

	if (prefs->timeout_id)
		gtk_timeout_remove (prefs->timeout_id);

	prefs->timeout_id = 0;

	preferences_save (prefs);
}

void
preferences_freeze (Preferences *prefs) 
{
	g_return_if_fail (prefs != NULL);
	g_return_if_fail (IS_PREFERENCES (prefs));

	prefs->frozen++;
}

void
preferences_thaw (Preferences *prefs) 
{
	g_return_if_fail (prefs != NULL);
	g_return_if_fail (IS_PREFERENCES (prefs));

	if (prefs->frozen > 0) prefs->frozen--;
}

Preferences *
preferences_read_xml (xmlDocPtr xml_doc) 
{
	Preferences *prefs;
	xmlNodePtr root_node, node;

	prefs = PREFERENCES (preferences_new ());

	root_node = xmlDocGetRootElement (xml_doc);

	if (strcmp (root_node->name, "ui-properties"))
		return NULL;

	for (node = root_node->childs; node; node = node->next) {
		if (!strcmp (node->name, "dialog-buttons-style"))
			prefs->gnome_prefs->dialog_buttons_style = 
				xml_read_int (node, "style");
		else if (!strcmp (node->name, "property-box-buttons-ok"))
			prefs->gnome_prefs->property_box_buttons_ok = TRUE;
		else if (!strcmp (node->name, "property-box-buttons-apply"))
			prefs->gnome_prefs->property_box_buttons_apply = TRUE;
		else if (!strcmp (node->name, "property-box-buttons-close"))
			prefs->gnome_prefs->property_box_buttons_close = TRUE;
		else if (!strcmp (node->name, "property-box-buttons-help"))
			prefs->gnome_prefs->property_box_buttons_help = TRUE;
		else if (!strcmp (node->name, "statusbar-not-dialog"))
			prefs->gnome_prefs->statusbar_not_dialog = TRUE;
		else if (!strcmp (node->name, "statusbar-is-interactive"))
			prefs->gnome_prefs->statusbar_is_interactive = TRUE;
		else if (!strcmp (node->name, "statusbar-meter-on-right"))
			prefs->gnome_prefs->statusbar_meter_on_right = TRUE;
		else if (!strcmp (node->name, "menubar-detachable"))
			prefs->gnome_prefs->menubar_detachable = TRUE;
		else if (!strcmp (node->name, "menubar-relief"))
			prefs->gnome_prefs->menubar_relief = TRUE;
		else if (!strcmp (node->name, "toolbar-detachable"))
			prefs->gnome_prefs->toolbar_detachable = TRUE;
		else if (!strcmp (node->name, "toolbar-relief"))
			prefs->gnome_prefs->toolbar_relief = TRUE;
		else if (!strcmp (node->name, "toolbar-relief-btn"))
			prefs->gnome_prefs->toolbar_relief_btn = TRUE;
		else if (!strcmp (node->name, "toolbar-lines"))
			prefs->gnome_prefs->toolbar_lines = TRUE;
		else if (!strcmp (node->name, "toolbar-labels"))
			prefs->gnome_prefs->toolbar_labels = TRUE;
		else if (!strcmp (node->name, "dialog-centered"))
			prefs->gnome_prefs->dialog_centered = TRUE;
		else if (!strcmp (node->name, "menus-have-tearoff"))
			prefs->gnome_prefs->menus_have_tearoff = TRUE;
		else if (!strcmp (node->name, "menus-have-icons"))
			prefs->gnome_prefs->menus_have_icons = TRUE;
		else if (!strcmp (node->name, "disable-imlib-cache"))
			prefs->gnome_prefs->disable_imlib_cache = TRUE;
		else if (!strcmp (node->name, "dialog-type"))
			prefs->gnome_prefs->dialog_type = 
				xml_read_int (node, "type");
		else if (!strcmp (node->name, "dialog-position"))
			prefs->gnome_prefs->dialog_position = 
				xml_read_int (node, "position");
		else if (!strcmp (node->name, "mdi-mode"))
			prefs->gnome_prefs->mdi_mode = 
				xml_read_int (node, "mode");
		else if (!strcmp (node->name, "mdi-tab-pos"))
			prefs->gnome_prefs->mdi_tab_pos = 
				xml_read_int (node, "pos");
		else if (!strcmp (node->name, "dialog-use-icons"))
			prefs->dialog_use_icons = TRUE;
	}

	return prefs;
}

xmlDocPtr 
preferences_write_xml (Preferences *prefs) 
{
	xmlDocPtr doc;
	xmlNodePtr node;

	doc = xmlNewDoc ("1.0");

	node = xmlNewDocNode (doc, NULL, "ui-properties", NULL);

	xmlAddChild (node, 
		     xml_write_int ("dialog-buttons-style", "style",
				    prefs->gnome_prefs->dialog_buttons_style));

	if (prefs->gnome_prefs->property_box_buttons_ok)
		xmlNewChild (node, NULL, "property-box-buttons-ok", NULL);
	if (prefs->gnome_prefs->property_box_buttons_apply)
		xmlNewChild (node, NULL, "property-box-buttons-apply", NULL);
	if (prefs->gnome_prefs->property_box_buttons_close)
		xmlNewChild (node, NULL, "property-box-buttons-close", NULL);
	if (prefs->gnome_prefs->property_box_buttons_help)
		xmlNewChild (node, NULL, "property-box-buttons-help", NULL);
	if (prefs->gnome_prefs->statusbar_not_dialog)
		xmlNewChild (node, NULL, "statusbar-not-dialog", NULL);
	if (prefs->gnome_prefs->statusbar_is_interactive)
		xmlNewChild (node, NULL, "statusbar-is-interactive", NULL);
	if (prefs->gnome_prefs->statusbar_meter_on_right)
		xmlNewChild (node, NULL, "statusbar-meter-on-right", NULL);
	if (prefs->gnome_prefs->menubar_detachable)
		xmlNewChild (node, NULL, "menubar-detachable", NULL);
	if (prefs->gnome_prefs->menubar_relief)
		xmlNewChild (node, NULL, "menubar-relief", NULL);
	if (prefs->gnome_prefs->toolbar_detachable)
		xmlNewChild (node, NULL, "toolbar-detachable", NULL);
	if (prefs->gnome_prefs->toolbar_relief)
		xmlNewChild (node, NULL, "toolbar-relief", NULL);
	if (prefs->gnome_prefs->toolbar_relief_btn)
		xmlNewChild (node, NULL, "toolbar-relief-btn", NULL);
	if (prefs->gnome_prefs->toolbar_lines)
		xmlNewChild (node, NULL, "toolbar-lines", NULL);
	if (prefs->gnome_prefs->toolbar_labels)
		xmlNewChild (node, NULL, "toolbar-labels", NULL);
	if (prefs->gnome_prefs->dialog_centered)
		xmlNewChild (node, NULL, "dialog-centered", NULL);
	if (prefs->gnome_prefs->menus_have_tearoff)
		xmlNewChild (node, NULL, "menus-have-tearoff", NULL);
	if (prefs->gnome_prefs->menus_have_icons)
		xmlNewChild (node, NULL, "menus-have-icons", NULL);
	if (prefs->gnome_prefs->disable_imlib_cache)
		xmlNewChild (node, NULL, "disable-imlib-cache", NULL);

	xmlAddChild (node, 
		     xml_write_int ("dialog-type", "type",
				    prefs->gnome_prefs->dialog_type));
	xmlAddChild (node, 
		     xml_write_int ("dialog-position", "position",
				    prefs->gnome_prefs->dialog_position));
	xmlAddChild (node, 
		     xml_write_int ("mdi-mode", "mode",
				    prefs->gnome_prefs->mdi_mode));
	xmlAddChild (node, 
		     xml_write_int ("mdi-tab-pos", "pos",
				    prefs->gnome_prefs->mdi_tab_pos));

	if (prefs->dialog_use_icons)
		xmlNewChild (node, NULL, "dialog-use-icons", NULL);

	xmlDocSetRootElement (doc, node);

	return doc;
}

/* Read a numeric value from a node */

static gint
xml_read_int (xmlNodePtr node, char *propname) 
{
	char *text;

	if (propname == NULL)
		text = xmlNodeGetContent (node);
	else
		text = xmlGetProp (node, propname);

	if (text == NULL) 
		return 0;
	else
		return atoi (text);
}

/* Write out a numeric value in a node */

static xmlNodePtr
xml_write_int (gchar *name, gchar *propname, gint number) 
{
	xmlNodePtr node;
	gchar *str;

	g_return_val_if_fail (name != NULL, NULL);

	str = g_strdup_printf ("%d", number);

	node = xmlNewNode (NULL, name);

	if (propname == NULL)
		xmlNodeSetContent (node, str);
	else
		xmlSetProp (node, propname, str);

	g_free (str);

	return node;
}

static gint 
apply_timeout_cb (Preferences *prefs) 
{
	preferences_apply_now (prefs);

	return TRUE;
}

GtkButtonBoxStyle 
preferences_get_dialog_buttons_style (Preferences *prefs) 
{
	g_return_val_if_fail (prefs != NULL, 0);
	g_return_val_if_fail (IS_PREFERENCES (prefs), 0);
	g_return_val_if_fail (prefs->gnome_prefs != NULL, 0);

	return prefs->gnome_prefs->dialog_buttons_style;
}

int
preferences_get_property_box_buttons_ok (Preferences *prefs) 
{
	g_return_val_if_fail (prefs != NULL, 0);
	g_return_val_if_fail (IS_PREFERENCES (prefs), 0);
	g_return_val_if_fail (prefs->gnome_prefs != NULL, 0);

	return prefs->gnome_prefs->property_box_buttons_ok;
}

int
preferences_get_property_box_buttons_apply (Preferences *prefs) 
{
	g_return_val_if_fail (prefs != NULL, 0);
	g_return_val_if_fail (IS_PREFERENCES (prefs), 0);
	g_return_val_if_fail (prefs->gnome_prefs != NULL, 0);

	return prefs->gnome_prefs->property_box_buttons_apply;
}

int
preferences_get_property_box_buttons_close (Preferences *prefs) 
{
	g_return_val_if_fail (prefs != NULL, 0);
	g_return_val_if_fail (IS_PREFERENCES (prefs), 0);
	g_return_val_if_fail (prefs->gnome_prefs != NULL, 0);

	return prefs->gnome_prefs->property_box_buttons_close;
}

int
preferences_get_property_box_buttons_help (Preferences *prefs) 
{
	g_return_val_if_fail (prefs != NULL, 0);
	g_return_val_if_fail (IS_PREFERENCES (prefs), 0);
	g_return_val_if_fail (prefs->gnome_prefs != NULL, 0);

	return prefs->gnome_prefs->property_box_buttons_help;
}

int
preferences_get_statusbar_not_dialog (Preferences *prefs)
{
	g_return_val_if_fail (prefs != NULL, 0);
	g_return_val_if_fail (IS_PREFERENCES (prefs), 0);
	g_return_val_if_fail (prefs->gnome_prefs != NULL, 0);

	return prefs->gnome_prefs->statusbar_not_dialog;
}

int
preferences_get_statusbar_is_interactive (Preferences *prefs)
{
	g_return_val_if_fail (prefs != NULL, 0);
	g_return_val_if_fail (IS_PREFERENCES (prefs), 0);
	g_return_val_if_fail (prefs->gnome_prefs != NULL, 0);

	return prefs->gnome_prefs->statusbar_is_interactive;
}

int
preferences_get_statusbar_meter_on_right (Preferences *prefs)
{
	g_return_val_if_fail (prefs != NULL, 0);
	g_return_val_if_fail (IS_PREFERENCES (prefs), 0);
	g_return_val_if_fail (prefs->gnome_prefs != NULL, 0);

	return prefs->gnome_prefs->statusbar_meter_on_right;
}

int
preferences_get_statusbar_meter_on_left (Preferences *prefs)
{
	g_return_val_if_fail (prefs != NULL, 0);
	g_return_val_if_fail (IS_PREFERENCES (prefs), 0);
	g_return_val_if_fail (prefs->gnome_prefs != NULL, 0);

	return !prefs->gnome_prefs->statusbar_meter_on_right;
}

int
preferences_get_menubar_detachable (Preferences *prefs)
{
	g_return_val_if_fail (prefs != NULL, 0);
	g_return_val_if_fail (IS_PREFERENCES (prefs), 0);
	g_return_val_if_fail (prefs->gnome_prefs != NULL, 0);

	return prefs->gnome_prefs->menubar_detachable;
}

int
preferences_get_menubar_relief (Preferences *prefs)
{
	g_return_val_if_fail (prefs != NULL, 0);
	g_return_val_if_fail (IS_PREFERENCES (prefs), 0);
	g_return_val_if_fail (prefs->gnome_prefs != NULL, 0);

	return prefs->gnome_prefs->menubar_relief;
}

int
preferences_get_toolbar_detachable (Preferences *prefs)
{
	g_return_val_if_fail (prefs != NULL, 0);
	g_return_val_if_fail (IS_PREFERENCES (prefs), 0);
	g_return_val_if_fail (prefs->gnome_prefs != NULL, 0);

	return prefs->gnome_prefs->toolbar_detachable;
}

int
preferences_get_toolbar_relief (Preferences *prefs)
{
	g_return_val_if_fail (prefs != NULL, 0);
	g_return_val_if_fail (IS_PREFERENCES (prefs), 0);
	g_return_val_if_fail (prefs->gnome_prefs != NULL, 0);

	return prefs->gnome_prefs->toolbar_relief;
}

int
preferences_get_toolbar_relief_btn (Preferences *prefs)
{
	g_return_val_if_fail (prefs != NULL, 0);
	g_return_val_if_fail (IS_PREFERENCES (prefs), 0);
	g_return_val_if_fail (prefs->gnome_prefs != NULL, 0);

	return prefs->gnome_prefs->toolbar_relief_btn;
}

int
preferences_get_toolbar_lines (Preferences *prefs)
{
	g_return_val_if_fail (prefs != NULL, 0);
	g_return_val_if_fail (IS_PREFERENCES (prefs), 0);
	g_return_val_if_fail (prefs->gnome_prefs != NULL, 0);

	return prefs->gnome_prefs->toolbar_lines;
}

int
preferences_get_toolbar_icons_only (Preferences *prefs)
{
	g_return_val_if_fail (prefs != NULL, 0);
	g_return_val_if_fail (IS_PREFERENCES (prefs), 0);
	g_return_val_if_fail (prefs->gnome_prefs != NULL, 0);

	return !prefs->gnome_prefs->toolbar_labels;
}

int
preferences_get_toolbar_text_below (Preferences *prefs)
{
	g_return_val_if_fail (prefs != NULL, 0);
	g_return_val_if_fail (IS_PREFERENCES (prefs), 0);
	g_return_val_if_fail (prefs->gnome_prefs != NULL, 0);

	return prefs->gnome_prefs->toolbar_labels;
}

int
preferences_get_dialog_centered (Preferences *prefs)
{
	g_return_val_if_fail (prefs != NULL, 0);
	g_return_val_if_fail (IS_PREFERENCES (prefs), 0);
	g_return_val_if_fail (prefs->gnome_prefs != NULL, 0);

	return prefs->gnome_prefs->dialog_centered;
}

int
preferences_get_menus_have_tearoff (Preferences *prefs)
{
	g_return_val_if_fail (prefs != NULL, 0);
	g_return_val_if_fail (IS_PREFERENCES (prefs), 0);
	g_return_val_if_fail (prefs->gnome_prefs != NULL, 0);

	return prefs->gnome_prefs->menus_have_tearoff;
}

int
preferences_get_menus_have_icons (Preferences *prefs)
{
	g_return_val_if_fail (prefs != NULL, 0);
	g_return_val_if_fail (IS_PREFERENCES (prefs), 0);
	g_return_val_if_fail (prefs->gnome_prefs != NULL, 0);

	return prefs->gnome_prefs->menus_have_icons;
}

int
preferences_get_disable_imlib_cache (Preferences *prefs)
{
	g_return_val_if_fail (prefs != NULL, 0);
	g_return_val_if_fail (IS_PREFERENCES (prefs), 0);
	g_return_val_if_fail (prefs->gnome_prefs != NULL, 0);

	return prefs->gnome_prefs->disable_imlib_cache;
}

GtkWindowType
preferences_get_dialog_type (Preferences *prefs)
{
	g_return_val_if_fail (prefs != NULL, 0);
	g_return_val_if_fail (IS_PREFERENCES (prefs), 0);
	g_return_val_if_fail (prefs->gnome_prefs != NULL, 0);

	return prefs->gnome_prefs->dialog_type;
}

GtkWindowPosition
preferences_get_dialog_position (Preferences *prefs)
{
	g_return_val_if_fail (prefs != NULL, 0);
	g_return_val_if_fail (IS_PREFERENCES (prefs), 0);
	g_return_val_if_fail (prefs->gnome_prefs != NULL, 0);

	return prefs->gnome_prefs->dialog_position;
}

GnomeMDIMode
preferences_get_mdi_mode (Preferences *prefs)
{
	g_return_val_if_fail (prefs != NULL, 0);
	g_return_val_if_fail (IS_PREFERENCES (prefs), 0);
	g_return_val_if_fail (prefs->gnome_prefs != NULL, 0);

	return prefs->gnome_prefs->mdi_mode;
}

GtkPositionType
preferences_get_mdi_tab_pos (Preferences *prefs)
{
	g_return_val_if_fail (prefs != NULL, 0);
	g_return_val_if_fail (IS_PREFERENCES (prefs), 0);
	g_return_val_if_fail (prefs->gnome_prefs != NULL, 0);

	return prefs->gnome_prefs->mdi_tab_pos;
}

int
preferences_get_dialog_icons (Preferences *prefs)
{
	g_return_val_if_fail (prefs != NULL, 0);
	g_return_val_if_fail (IS_PREFERENCES (prefs), 0);
	g_return_val_if_fail (prefs->gnome_prefs != NULL, 0);

	return prefs->dialog_use_icons;
}

void
preferences_set_dialog_buttons_style (Preferences *prefs, int style)
{
	g_return_if_fail (prefs != NULL);
	g_return_if_fail (IS_PREFERENCES (prefs));
	g_return_if_fail (prefs->gnome_prefs != NULL);

	prefs->gnome_prefs->dialog_buttons_style = style;
}

void
preferences_set_property_box_buttons_ok (Preferences *prefs, int s) 
{
	g_return_if_fail (prefs != NULL);
	g_return_if_fail (IS_PREFERENCES (prefs));
	g_return_if_fail (prefs->gnome_prefs != NULL);

	prefs->gnome_prefs->property_box_buttons_ok = s;
}

void
set_property_set_property_box_buttons_apply (Preferences *prefs, int s)
{
	g_return_if_fail (prefs != NULL);
	g_return_if_fail (IS_PREFERENCES (prefs));
	g_return_if_fail (prefs->gnome_prefs != NULL);

	prefs->gnome_prefs->property_box_buttons_apply = s;
}

void
preferences_set_property_box_buttons_close (Preferences *prefs, int s)
{
	g_return_if_fail (prefs != NULL);
	g_return_if_fail (IS_PREFERENCES (prefs));
	g_return_if_fail (prefs->gnome_prefs != NULL);

	prefs->gnome_prefs->property_box_buttons_close = s;
}

void
preferences_set_property_box_buttons_help (Preferences *prefs, int s)
{
	g_return_if_fail (prefs != NULL);
	g_return_if_fail (IS_PREFERENCES (prefs));
	g_return_if_fail (prefs->gnome_prefs != NULL);

	prefs->gnome_prefs->property_box_buttons_help = s;
}

void
preferences_set_statusbar_not_dialog (Preferences *prefs, int s)
{
	g_return_if_fail (prefs != NULL);
	g_return_if_fail (IS_PREFERENCES (prefs));
	g_return_if_fail (prefs->gnome_prefs != NULL);

	prefs->gnome_prefs->statusbar_not_dialog = s;
}

void
preferences_set_statusbar_is_interactive (Preferences *prefs, int s)
{
	g_return_if_fail (prefs != NULL);
	g_return_if_fail (IS_PREFERENCES (prefs));
	g_return_if_fail (prefs->gnome_prefs != NULL);

	prefs->gnome_prefs->statusbar_is_interactive = s;
}

void
preferences_set_statusbar_meter_on_right (Preferences *prefs, int s)
{
	g_return_if_fail (prefs != NULL);
	g_return_if_fail (IS_PREFERENCES (prefs));
	g_return_if_fail (prefs->gnome_prefs != NULL);

	prefs->gnome_prefs->statusbar_meter_on_right = s;
}

void
preferences_set_statusbar_meter_on_left (Preferences *prefs, int s)
{
	g_return_if_fail (prefs != NULL);
	g_return_if_fail (IS_PREFERENCES (prefs));
	g_return_if_fail (prefs->gnome_prefs != NULL);

	prefs->gnome_prefs->statusbar_meter_on_right = !s;
}

void
preferences_set_menubar_detachable (Preferences *prefs, int s)
{
	g_return_if_fail (prefs != NULL);
	g_return_if_fail (IS_PREFERENCES (prefs));
	g_return_if_fail (prefs->gnome_prefs != NULL);

	prefs->gnome_prefs->menubar_detachable = s;
}

void
preferences_set_menubar_relief (Preferences *prefs, int s)
{
	g_return_if_fail (prefs != NULL);
	g_return_if_fail (IS_PREFERENCES (prefs));
	g_return_if_fail (prefs->gnome_prefs != NULL);

	prefs->gnome_prefs->menubar_relief = s;
}

void
preferences_set_toolbar_detachable (Preferences *prefs, int s)
{
	g_return_if_fail (prefs != NULL);
	g_return_if_fail (IS_PREFERENCES (prefs));
	g_return_if_fail (prefs->gnome_prefs != NULL);

	prefs->gnome_prefs->toolbar_detachable = s;
}

void
preferences_set_toolbar_relief (Preferences *prefs, int s)
{
	g_return_if_fail (prefs != NULL);
	g_return_if_fail (IS_PREFERENCES (prefs));
	g_return_if_fail (prefs->gnome_prefs != NULL);

	prefs->gnome_prefs->toolbar_relief = s;
}

void
preferences_set_toolbar_relief_btn (Preferences *prefs, int s)
{
	g_return_if_fail (prefs != NULL);
	g_return_if_fail (IS_PREFERENCES (prefs));
	g_return_if_fail (prefs->gnome_prefs != NULL);

	prefs->gnome_prefs->toolbar_relief_btn = s;
}

void
preferences_set_toolbar_lines (Preferences *prefs, int s)
{
	g_return_if_fail (prefs != NULL);
	g_return_if_fail (IS_PREFERENCES (prefs));
	g_return_if_fail (prefs->gnome_prefs != NULL);

	prefs->gnome_prefs->toolbar_lines = s;
}

void
preferences_set_toolbar_icons_only (Preferences *prefs, int s)
{
	g_return_if_fail (prefs != NULL);
	g_return_if_fail (IS_PREFERENCES (prefs));
	g_return_if_fail (prefs->gnome_prefs != NULL);

	prefs->gnome_prefs->toolbar_labels = !s;
}

void
preferences_set_toolbar_text_below (Preferences *prefs, int s)
{
	g_return_if_fail (prefs != NULL);
	g_return_if_fail (IS_PREFERENCES (prefs));
	g_return_if_fail (prefs->gnome_prefs != NULL);

	prefs->gnome_prefs->toolbar_labels = s;
}

void
preferences_set_dialog_centered (Preferences *prefs, int s)
{
	g_return_if_fail (prefs != NULL);
	g_return_if_fail (IS_PREFERENCES (prefs));
	g_return_if_fail (prefs->gnome_prefs != NULL);

	prefs->gnome_prefs->dialog_centered = s;
}

void
preferences_set_menus_have_tearoff (Preferences *prefs, int s)
{
	g_return_if_fail (prefs != NULL);
	g_return_if_fail (IS_PREFERENCES (prefs));
	g_return_if_fail (prefs->gnome_prefs != NULL);

	prefs->gnome_prefs->menus_have_tearoff = s;
}

void
preferences_set_menus_have_icons (Preferences *prefs, int s)
{
	g_return_if_fail (prefs != NULL);
	g_return_if_fail (IS_PREFERENCES (prefs));
	g_return_if_fail (prefs->gnome_prefs != NULL);

	prefs->gnome_prefs->menus_have_icons = s;
}

void
preferences_set_disable_imlib_cache (Preferences *prefs, int s)
{
	g_return_if_fail (prefs != NULL);
	g_return_if_fail (IS_PREFERENCES (prefs));
	g_return_if_fail (prefs->gnome_prefs != NULL);

	prefs->gnome_prefs->disable_imlib_cache = s;
}

void
preferences_set_dialog_type (Preferences *prefs, int type)
{
	g_return_if_fail (prefs != NULL);
	g_return_if_fail (IS_PREFERENCES (prefs));
	g_return_if_fail (prefs->gnome_prefs != NULL);

	prefs->gnome_prefs->dialog_type = type;
}

void
preferences_set_dialog_position (Preferences *prefs, int pos)
{
	g_return_if_fail (prefs != NULL);
	g_return_if_fail (IS_PREFERENCES (prefs));
	g_return_if_fail (prefs->gnome_prefs != NULL);

	prefs->gnome_prefs->dialog_position = pos;
}

void
preferences_set_mdi_mode (Preferences *prefs, int mode)
{
	g_return_if_fail (prefs != NULL);
	g_return_if_fail (IS_PREFERENCES (prefs));
	g_return_if_fail (prefs->gnome_prefs != NULL);

	prefs->gnome_prefs->mdi_mode = mode;
}

void
preferences_set_mdi_tab_pos (Preferences *prefs, int type)
{
	g_return_if_fail (prefs != NULL);
	g_return_if_fail (IS_PREFERENCES (prefs));
	g_return_if_fail (prefs->gnome_prefs != NULL);

	prefs->gnome_prefs->mdi_tab_pos = type;
}

void
preferences_set_dialog_icons (Preferences *prefs, int s) 
{
	g_return_if_fail (prefs != NULL);
	g_return_if_fail (IS_PREFERENCES (prefs));
	g_return_if_fail (prefs->gnome_prefs != NULL);

	prefs->dialog_use_icons = s;
}
