/* -*- mode: c; style: linux -*- */

/* preferences.c
 * Copyright (C) 2000 Helix Code, Inc.
 *
 * Written by Bradford Hovinen <hovinen@helixcode.com>,
 *            Jaka Mocnik <jaka.mocnik@kiss.uni-lj.si>
 *
 * Based on gnome-core/desktop-properties/property-keyboard.c
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
#include <gdk/gdkx.h>
#include <X11/X.h>

#ifdef HAVE_X11_EXTENSIONS_XF86MISC_H
#include <X11/extensions/xf86misc.h>
#endif

#include "preferences.h"

static GtkObjectClass *parent_class;

#ifdef HAVE_X11_EXTENSIONS_XF86MISC_H
static XF86MiscKbdSettings kbdsettings;
#endif

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

	/* Code to initialize preferences object to defaults */
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

	object = preferences_new ();

	new_prefs = PREFERENCES (object);
	new_prefs->rate = prefs->rate;
	new_prefs->delay = prefs->delay;
	new_prefs->repeat = prefs->repeat;
	new_prefs->volume = prefs->volume;
	new_prefs->click = prefs->click;

	return object;
}

void
preferences_destroy (GtkObject *object) 
{
	Preferences *prefs;

	g_return_if_fail (object != NULL);
	g_return_if_fail (IS_PREFERENCES (object));

	prefs = PREFERENCES (object);

	parent_class->destroy (object);
}

void
preferences_load (Preferences *prefs) 
{
	XKeyboardState kbdstate;
	gboolean repeat_default, click_default;
        gint event_base_return, error_base_return;

	g_return_if_fail (prefs != NULL);
	g_return_if_fail (IS_PREFERENCES (prefs));

	prefs->rate = gnome_config_get_int ("/Desktop/Keyboard/rate=-");
	prefs->delay = gnome_config_get_int ("/Desktop/Keyboard/delay=-1");
	prefs->repeat = gnome_config_get_bool_with_default
		("/Desktop/Keyboard/repeat=true", &repeat_default);
        prefs->volume = gnome_config_get_int
		("/Desktop/Keyboard/clickvolume=-1");
        prefs->click = gnome_config_get_bool_with_default
		("/Desktop/Keyboard/click=false", &click_default);

	XGetKeyboardControl (GDK_DISPLAY (), &kbdstate);

	if (repeat_default)
		prefs->repeat = kbdstate.global_auto_repeat;

	if (prefs->rate == -1 || prefs->delay == -1) {
#ifdef HAVE_X11_EXTENSIONS_XF86MISC_H
		if (XF86MiscQueryExtension (GDK_DISPLAY (),
					    &event_base_return,
					    &error_base_return) == True) 
		{
                        XF86MiscGetKbdSettings (GDK_DISPLAY (), &kbdsettings);
                        prefs->rate = kbdsettings.rate;
                        prefs->delay = kbdsettings.delay;
                } else {
                        prefs->rate = 5;
                        prefs->delay = 500;
                }
#else
		/* FIXME: how to get the keyboard speed on non-xf86? */
		prefs->rate = 5;
		prefs->delay = 500;
#endif
	}

	if (click_default)
		prefs->click = (kbdstate.key_click_percent == 0);

	if (prefs->volume == -1)
		prefs->volume = kbdstate.key_click_percent;
}

void 
preferences_save (Preferences *prefs) 
{
	g_return_if_fail (prefs != NULL);
	g_return_if_fail (IS_PREFERENCES (prefs));

	gnome_config_set_bool ("/Desktop/Keyboard/repeat", prefs->repeat);
	gnome_config_set_int ("/Desktop/Keyboard/delay", prefs->delay);
	gnome_config_set_int ("/Desktop/Keyboard/rate", prefs->rate);
	gnome_config_set_bool ("/Desktop/Keyboard/click", prefs->click);
	gnome_config_set_int ("/Desktop/Keyboard/clickvolume", prefs->volume);

	gnome_config_sync ();
}

void
preferences_changed (Preferences *prefs) 
{
	if (prefs->frozen) return;

	if (prefs->timeout_id)
		gtk_timeout_remove (prefs->timeout_id);

	preferences_apply_now (prefs);
}

void
preferences_apply_now (Preferences *prefs)
{
	XKeyboardControl kbdcontrol;
        int event_base_return, error_base_return;

	g_return_if_fail (prefs != NULL);
	g_return_if_fail (IS_PREFERENCES (prefs));

	if (prefs->timeout_id)
		gtk_timeout_remove (prefs->timeout_id);

	prefs->timeout_id = 0;

        if (prefs->repeat) {
		XAutoRepeatOn (GDK_DISPLAY ());
#ifdef HAVE_X11_EXTENSIONS_XF86MISC_H
		if (XF86MiscQueryExtension (GDK_DISPLAY (),
					    &event_base_return,
					    &error_base_return) == True)
		{
                        kbdsettings.rate = prefs->rate;
                        kbdsettings.delay = prefs->delay;
                        XF86MiscSetKbdSettings (GDK_DISPLAY (), &kbdsettings);
                } else {
                        XAutoRepeatOff (GDK_DISPLAY ());
                }
#endif
	} else {
		XAutoRepeatOff (GDK_DISPLAY ());
	}

	kbdcontrol.key_click_percent = 
		prefs->click ? prefs->volume : 0;
	XChangeKeyboardControl (GDK_DISPLAY (), KBKeyClickPercent, 
				&kbdcontrol);
}

void preferences_freeze (Preferences *prefs) 
{
	prefs->frozen = TRUE;
}

void preferences_thaw (Preferences *prefs) 
{
	prefs->frozen = FALSE;
}

Preferences *
preferences_read_xml (xmlDocPtr xml_doc) 
{
	Preferences *prefs;
	xmlNodePtr root_node, node;

	prefs = PREFERENCES (preferences_new ());

	root_node = xmlDocGetRootElement (xml_doc);

	if (strcmp (root_node->name, "keyboard-properties"))
		return NULL;

	for (node = root_node->childs; node; node = node->next) {
                if (!strcmp (node->name, "rate"))
                        prefs->rate = xml_read_int (node, NULL);
                else if (!strcmp (node->name, "delay"))
                        prefs->delay = xml_read_int (node, NULL);
                else if (!strcmp (node->name, "repeat"))
                        prefs->repeat = TRUE;
                else if (!strcmp (node->name, "volume"))
                        prefs->volume = xml_read_int (node, NULL);
                else if (!strcmp (node->name, "click"))
                        prefs->click = TRUE;
	}

	return prefs;
}

xmlDocPtr 
preferences_write_xml (Preferences *prefs) 
{
	xmlDocPtr doc;
	xmlNodePtr node;
	char *tmp;

	doc = xmlNewDoc ("1.0");

	node = xmlNewDocNode (doc, NULL, "keyboard-properties", NULL);

        xmlAddChild (node, xml_write_int ("rate", NULL, prefs->rate));
        xmlAddChild (node, xml_write_int ("delay", NULL, prefs->delay));

        if (prefs->repeat)
                xmlNewChild (node, NULL, "repeat", NULL);

        xmlAddChild (node, xml_write_int ("volume", NULL,
					  prefs->volume));

        if (prefs->click)
                xmlNewChild (node, NULL, "click", NULL);

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
