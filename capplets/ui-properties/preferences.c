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
#include <gconf/gconf-client.h>

#include "preferences.h"

static GtkObjectClass *parent_class;

static void preferences_init             (Preferences *prefs);
static void preferences_class_init       (PreferencesClass *class);

static gint apply_timeout_cb             (Preferences *prefs);

#define DGI "/desktop/gnome/interface/"

GType
preferences_get_type (void)
{
	static GType preferences_type = 0;

	if (!preferences_type) {
		GtkTypeInfo preferences_info = {
			"Preferences",
			sizeof (Preferences),
			sizeof (PreferencesClass),
			(GtkClassInitFunc) preferences_class_init,
			(GtkObjectInitFunc) preferences_init,
			NULL,
			NULL
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

	/* FIXME: Code to set default values */
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
	new_prefs->gnome_prefs = prefs->gnome_prefs;

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
	GConfClient *client;
	
	g_return_if_fail (prefs != NULL);
	g_return_if_fail (IS_PREFERENCES (prefs));

	client = gconf_client_get_default ();
	prefs->gnome_prefs.menus_have_tearoff = gconf_client_get_bool (client, DGI "menus-have-tearoff", NULL);
	prefs->gnome_prefs.statusbar_is_interactive = gconf_client_get_bool (client, DGI "statusbar-interactive", NULL);
	prefs->gnome_prefs.menubar_relief = gconf_client_get_bool (client, DGI "menubar-relief", NULL);
	prefs->gnome_prefs.toolbar_labels = gconf_client_get_bool (client, DGI "toolbar-labels", NULL);
	prefs->gnome_prefs.menus_show_icons = gconf_client_get_bool (client, DGI "menus-have-icons", NULL);
	prefs->gnome_prefs.menubar_detachable = gconf_client_get_bool (client, DGI "menubar-detachable", NULL);
	prefs->gnome_prefs.toolbar_relief = gconf_client_get_bool (client, DGI "toolbar-relief", NULL);
	prefs->gnome_prefs.statusbar_meter_on_right = gconf_client_get_bool (client, DGI "statusbar-meter-on-right", NULL);
	prefs->gnome_prefs.toolbar_detachable = gconf_client_get_bool (client, DGI "toolbar-detachable", NULL);
	
	g_object_unref (G_OBJECT (client));
}

void 
preferences_save (Preferences *prefs) 
{

	GConfClient *client;
	
	g_return_if_fail (prefs != NULL);
	g_return_if_fail (IS_PREFERENCES (prefs));

	client = gconf_client_get_default ();
	gconf_client_set_bool (client, DGI "menus-have-tearoff", prefs->gnome_prefs.menus_have_tearoff, NULL);
	gconf_client_set_bool (client, DGI "statusbar-interactive", prefs->gnome_prefs.statusbar_is_interactive, NULL);
	gconf_client_set_bool (client, DGI "menubar-relief", prefs->gnome_prefs.menubar_relief, NULL);
	gconf_client_set_bool (client, DGI "toolbar-labels", prefs->gnome_prefs.toolbar_labels, NULL);
	gconf_client_set_bool (client, DGI "menus-have-icons", prefs->gnome_prefs.menus_show_icons, NULL);
	gconf_client_set_bool (client, DGI "menubar-detachable", prefs->gnome_prefs.menubar_detachable, NULL);
	gconf_client_set_bool (client, DGI "toolbar-relief", prefs->gnome_prefs.toolbar_relief, NULL);
	gconf_client_set_bool (client, DGI "statusbar-meter-on-right", prefs->gnome_prefs.statusbar_meter_on_right, NULL);
	gconf_client_set_bool (client, DGI "toolbar-detachable", prefs->gnome_prefs.toolbar_detachable, NULL);
	
	g_object_unref (G_OBJECT (client));
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

static gint 
apply_timeout_cb (Preferences *prefs) 
{
	preferences_apply_now (prefs);

	return TRUE;
}

int
preferences_get_dialog_buttons_style (Preferences *prefs) 
{
	g_return_val_if_fail (prefs != NULL, 0);
	g_return_val_if_fail (IS_PREFERENCES (prefs), 0);

	return prefs->gnome_prefs.dialog_buttons_style;
}

int
preferences_get_statusbar_is_interactive (Preferences *prefs)
{
	g_return_val_if_fail (prefs != NULL, 0);
	g_return_val_if_fail (IS_PREFERENCES (prefs), 0);

	return prefs->gnome_prefs.statusbar_is_interactive;
}

int
preferences_get_statusbar_meter_on_right (Preferences *prefs)
{
	g_return_val_if_fail (prefs != NULL, 0);
	g_return_val_if_fail (IS_PREFERENCES (prefs), 0);

	return prefs->gnome_prefs.statusbar_meter_on_right;
}

int
preferences_get_statusbar_meter_on_left (Preferences *prefs)
{
	g_return_val_if_fail (prefs != NULL, 0);
	g_return_val_if_fail (IS_PREFERENCES (prefs), 0);

	return !prefs->gnome_prefs.statusbar_meter_on_right;
}

int
preferences_get_menubar_detachable (Preferences *prefs)
{
	g_return_val_if_fail (prefs != NULL, 0);
	g_return_val_if_fail (IS_PREFERENCES (prefs), 0);

	return prefs->gnome_prefs.menubar_detachable;
}

int
preferences_get_menubar_relief (Preferences *prefs)
{
	g_return_val_if_fail (prefs != NULL, 0);
	g_return_val_if_fail (IS_PREFERENCES (prefs), 0);

	return prefs->gnome_prefs.menubar_relief;
}

int
preferences_get_toolbar_detachable (Preferences *prefs)
{
	g_return_val_if_fail (prefs != NULL, 0);
	g_return_val_if_fail (IS_PREFERENCES (prefs), 0);

	return prefs->gnome_prefs.toolbar_detachable;
}

int
preferences_get_toolbar_relief (Preferences *prefs)
{
	g_return_val_if_fail (prefs != NULL, 0);
	g_return_val_if_fail (IS_PREFERENCES (prefs), 0);

	return prefs->gnome_prefs.toolbar_relief;
}

int
preferences_get_toolbar_icons_only (Preferences *prefs)
{
	g_return_val_if_fail (prefs != NULL, 0);
	g_return_val_if_fail (IS_PREFERENCES (prefs), 0);

	return !prefs->gnome_prefs.toolbar_labels;
}

int
preferences_get_toolbar_text_below (Preferences *prefs)
{
	g_return_val_if_fail (prefs != NULL, 0);
	g_return_val_if_fail (IS_PREFERENCES (prefs), 0);

	return prefs->gnome_prefs.toolbar_labels;
}

int
preferences_get_dialog_centered (Preferences *prefs)
{
	g_return_val_if_fail (prefs != NULL, 0);
	g_return_val_if_fail (IS_PREFERENCES (prefs), 0);

	return TRUE;
}

int
preferences_get_menus_have_tearoff (Preferences *prefs)
{
	g_return_val_if_fail (prefs != NULL, 0);
	g_return_val_if_fail (IS_PREFERENCES (prefs), 0);

	return prefs->gnome_prefs.menus_have_tearoff;
}

int
preferences_get_menus_have_icons (Preferences *prefs)
{
	g_return_val_if_fail (prefs != NULL, 0);
	g_return_val_if_fail (IS_PREFERENCES (prefs), 0);

	return prefs->gnome_prefs.menus_show_icons;
}

GtkWindowType
preferences_get_dialog_type (Preferences *prefs)
{
	g_return_val_if_fail (prefs != NULL, 0);
	g_return_val_if_fail (IS_PREFERENCES (prefs), 0);

	return 0;
}

GtkWindowPosition
preferences_get_dialog_position (Preferences *prefs)
{
	g_return_val_if_fail (prefs != NULL, 0);
	g_return_val_if_fail (IS_PREFERENCES (prefs), 0);

	return 0;
}

GnomeMDIMode
preferences_get_mdi_mode (Preferences *prefs)
{
	g_return_val_if_fail (prefs != NULL, 0);
	g_return_val_if_fail (IS_PREFERENCES (prefs), 0);

	return 0;
}

GtkPositionType
preferences_get_mdi_tab_pos (Preferences *prefs)
{
	g_return_val_if_fail (prefs != NULL, 0);
	g_return_val_if_fail (IS_PREFERENCES (prefs), 0);

	return 0;
}

int
preferences_get_dialog_icons (Preferences *prefs)
{
	g_return_val_if_fail (prefs != NULL, 0);
	g_return_val_if_fail (IS_PREFERENCES (prefs), 0);

	return prefs->gnome_prefs.dialog_icons;
}

void
preferences_set_dialog_buttons_style (Preferences *prefs, int style)
{
	g_return_if_fail (prefs != NULL);
	g_return_if_fail (IS_PREFERENCES (prefs));

	prefs->gnome_prefs.dialog_buttons_style = style;
}

void
preferences_set_statusbar_is_interactive (Preferences *prefs, int s)
{
	g_return_if_fail (prefs != NULL);
	g_return_if_fail (IS_PREFERENCES (prefs));

	prefs->gnome_prefs.statusbar_is_interactive = s;
}

void
preferences_set_statusbar_meter_on_right (Preferences *prefs, int s)
{
	g_return_if_fail (prefs != NULL);
	g_return_if_fail (IS_PREFERENCES (prefs));

	prefs->gnome_prefs.statusbar_meter_on_right = s;
}

void
preferences_set_statusbar_meter_on_left (Preferences *prefs, int s)
{
	g_return_if_fail (prefs != NULL);
	g_return_if_fail (IS_PREFERENCES (prefs));

	prefs->gnome_prefs.statusbar_meter_on_right = !s;
}

void
preferences_set_menubar_detachable (Preferences *prefs, int s)
{
	g_return_if_fail (prefs != NULL);
	g_return_if_fail (IS_PREFERENCES (prefs));

	prefs->gnome_prefs.menubar_detachable = s;
}

void
preferences_set_menubar_relief (Preferences *prefs, int s)
{
	g_return_if_fail (prefs != NULL);
	g_return_if_fail (IS_PREFERENCES (prefs));

	prefs->gnome_prefs.menubar_relief = s;
}

void
preferences_set_toolbar_detachable (Preferences *prefs, int s)
{
	g_return_if_fail (prefs != NULL);
	g_return_if_fail (IS_PREFERENCES (prefs));

	prefs->gnome_prefs.toolbar_detachable = s;
}

void
preferences_set_toolbar_relief (Preferences *prefs, int s)
{
	g_return_if_fail (prefs != NULL);
	g_return_if_fail (IS_PREFERENCES (prefs));

	prefs->gnome_prefs.toolbar_relief = s;
}

void
preferences_set_toolbar_icons_only (Preferences *prefs, int s)
{
	g_return_if_fail (prefs != NULL);
	g_return_if_fail (IS_PREFERENCES (prefs));

	prefs->gnome_prefs.toolbar_labels = !s;
}

void
preferences_set_toolbar_text_below (Preferences *prefs, int s)
{
	g_return_if_fail (prefs != NULL);
	g_return_if_fail (IS_PREFERENCES (prefs));

	prefs->gnome_prefs.toolbar_labels = s;
}

void
preferences_set_dialog_centered (Preferences *prefs, int s)
{
	g_return_if_fail (prefs != NULL);
	g_return_if_fail (IS_PREFERENCES (prefs));

	prefs->gnome_prefs.dialog_centered = s;
}

void
preferences_set_menus_have_tearoff (Preferences *prefs, int s)
{
	g_return_if_fail (prefs != NULL);
	g_return_if_fail (IS_PREFERENCES (prefs));

	prefs->gnome_prefs.menus_have_tearoff = s;
}

void
preferences_set_menus_have_icons (Preferences *prefs, int s)
{
	g_return_if_fail (prefs != NULL);
	g_return_if_fail (IS_PREFERENCES (prefs));

	prefs->gnome_prefs.menus_show_icons = s;
}

void
preferences_set_dialog_type (Preferences *prefs, int type)
{
	g_return_if_fail (prefs != NULL);
	g_return_if_fail (IS_PREFERENCES (prefs));

	prefs->gnome_prefs.dialog_type = type;
}

void
preferences_set_dialog_position (Preferences *prefs, int pos)
{
	g_return_if_fail (prefs != NULL);
	g_return_if_fail (IS_PREFERENCES (prefs));

	prefs->gnome_prefs.dialog_position = pos;
}

void
preferences_set_mdi_mode (Preferences *prefs, int mode)
{
	g_return_if_fail (prefs != NULL);
	g_return_if_fail (IS_PREFERENCES (prefs));

	prefs->gnome_prefs.mdi_mode = mode;
}

void
preferences_set_mdi_tab_pos (Preferences *prefs, int type)
{
	g_return_if_fail (prefs != NULL);
	g_return_if_fail (IS_PREFERENCES (prefs));

	prefs->gnome_prefs.mdi_tab_pos = type;
}

void
preferences_set_dialog_icons (Preferences *prefs, int s) 
{
	g_return_if_fail (prefs != NULL);
	g_return_if_fail (IS_PREFERENCES (prefs));

	prefs->gnome_prefs.dialog_icons = s;
}
