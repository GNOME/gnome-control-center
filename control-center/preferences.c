/* -*- mode: c; style: linux -*- */

/* preferences.c
 * Copyright (C) 2000, 2001 Ximian, Inc.
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
#  include <config.h>
#endif

#include <glib.h>
#include <gnome.h>

#include "preferences.h"

#include <glade/glade.h>

#include <gconf/gconf.h>

static GnomeCCPreferences *old_prefs;

static GtkWidget *prefs_dialog;
static GladeXML *prefs_dialog_data;

enum {
	CHANGED_SIGNAL,
	LAST_SIGNAL
};

static gint gnomecc_preferences_signals[LAST_SIGNAL] = { 0 };

static void gnomecc_preferences_init (GnomeCCPreferences *prefs);
static void gnomecc_preferences_class_init (GnomeCCPreferencesClass *klass);

static void set_single_window_controls_sensitive (GladeXML *data, gboolean s);

guint
gnomecc_preferences_get_type (void) 
{
	static guint gnomecc_preferences_type;

	if (!gnomecc_preferences_type) {
		static const GTypeInfo gnomecc_preferences_info = {
			sizeof (GnomeCCPreferencesClass),
			NULL, /* base_init */
			NULL, /* base_finalize */
			(GClassInitFunc) gnomecc_preferences_class_init,
			NULL, /* class_finalize */
			NULL, /* class_data */
			sizeof (GnomeCCPreferences),
			0 /* n_preallocs */,
			(GInstanceInitFunc) gnomecc_preferences_init
		};

		gnomecc_preferences_type =
			g_type_register_static (gtk_object_get_type (),
					"GnomeCCPreferences",
					&gnomecc_preferences_info,
					0);
	}

	return gnomecc_preferences_type;
}

static void 
gnomecc_preferences_init (GnomeCCPreferences *prefs) 
{
	prefs->layout = LAYOUT_NONE;
	prefs->embed = FALSE;
	prefs->single_window = TRUE;
}

static void 
gnomecc_preferences_class_init (GnomeCCPreferencesClass *klass) 
{
	GtkObjectClass *object_class;

	object_class = GTK_OBJECT_CLASS (klass);

	gnomecc_preferences_signals[CHANGED_SIGNAL] =
		gtk_signal_new ("changed", GTK_RUN_FIRST, 
				GTK_CLASS_TYPE (object_class),
				GTK_SIGNAL_OFFSET (GnomeCCPreferencesClass, 
						   changed),
				gtk_marshal_NONE__NONE, 
				GTK_TYPE_NONE, 0);
#if 0
	gtk_object_class_add_signals (object_class, 
				      gnomecc_preferences_signals,
				      LAST_SIGNAL);
#endif
}

GnomeCCPreferences *
gnomecc_preferences_new (void) 
{
	return gtk_type_new (gnomecc_preferences_get_type ());
}

GnomeCCPreferences *
gnomecc_preferences_clone (GnomeCCPreferences *prefs) 
{
	GnomeCCPreferences *new_prefs;

	new_prefs = gnomecc_preferences_new ();
	gnomecc_preferences_copy (new_prefs, prefs);

	return new_prefs;
}

void
gnomecc_preferences_copy (GnomeCCPreferences *new, GnomeCCPreferences *old) 
{
	new->layout = old->layout;
	new->single_window = old->single_window;
	new->embed = old->embed;
}

void 
gnomecc_preferences_load (GnomeCCPreferences *prefs) 
{
	g_return_if_fail (prefs != NULL);

#if 0
	engine = gconf_engine_get_default ();

	prefs->embed = gconf_engine_get_bool (engine, "/apps/control-center/appearance/embed", NULL);
	prefs->single_window = gconf_engine_get_bool (engine, "/apps/control-center/appearance/single-window", NULL);
	prefs->layout = gconf_engine_get_int (engine, "/apps/control-center/appearance/layout", NULL);
#endif

	prefs->embed = FALSE;
	prefs->single_window = TRUE;
	prefs->layout = 1;
}

void 
gnomecc_preferences_save (GnomeCCPreferences *prefs) 
{
	GConfEngine *engine;

	g_return_if_fail (prefs != NULL);

	engine = gconf_engine_get_default ();

	gconf_engine_set_bool (engine, "/apps/control-center/appearance/embed", prefs->embed, NULL);
	gconf_engine_set_bool (engine, "/apps/control-center/appearance/single-window", prefs->single_window, NULL);
	gconf_engine_set_bool (engine, "/apps/control-center/appearance/layout", prefs->layout, NULL);
}

static void
place_preferences (GladeXML *prefs_data, GnomeCCPreferences *prefs) 
{
	GtkWidget *widget;
	char *w;

	widget = glade_xml_get_widget (prefs_data, prefs->embed 
				       ? "embed_widget" : "no_embed_widget");
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), TRUE);

	widget = glade_xml_get_widget (prefs_data, prefs->single_window
				       ? "single_widget" : "multiple_widget");
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), TRUE);

	switch (prefs->layout) {
#ifdef USE_HTML
	case LAYOUT_HTML: w = "html_widget"; break;
#endif
	case LAYOUT_TREE: w = "tree_widget"; break;
	case LAYOUT_ICON_LIST: w = "icon_list_widget"; break;
	default: w = NULL; break;
	}

	if (!w) return;

	widget = glade_xml_get_widget (prefs_data, w);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), TRUE);
}

static void
read_preferences (GladeXML *prefs_data, GnomeCCPreferences *prefs) 
{
	GtkWidget *widget;

	widget = glade_xml_get_widget (prefs_data, "embed_widget");
	prefs->embed = 
		gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));

	widget = glade_xml_get_widget (prefs_data, "single_widget");
	prefs->single_window =
		gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));

	widget = glade_xml_get_widget (prefs_data, "tree_widget");
	if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget)))
		prefs->layout = LAYOUT_TREE;
	else {
#ifdef USE_HTML
		widget = glade_xml_get_widget (prefs_data, "html_widget");
		if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget)))
			prefs->layout = LAYOUT_HTML;
		else
#endif
			prefs->layout = LAYOUT_ICON_LIST;
	}

	gnomecc_preferences_save (prefs);

	gtk_signal_emit (GTK_OBJECT (prefs),
			 gnomecc_preferences_signals[CHANGED_SIGNAL]);
}

static void
prefs_dialog_ok_cb (GtkWidget *widget, GladeXML *data) 
{
	GnomeCCPreferences *prefs;

	prefs = gtk_object_get_data (GTK_OBJECT (data), "prefs_struct");
	read_preferences (data, prefs);
	gnome_dialog_close (GNOME_DIALOG (prefs_dialog));
	prefs_dialog = NULL;
	prefs_dialog_data = NULL;
}

static void
prefs_dialog_apply_cb (GtkWidget *widget, GladeXML *data) 
{
	GnomeCCPreferences *prefs;

	prefs = gtk_object_get_data (GTK_OBJECT (data), "prefs_struct");
	read_preferences (data, prefs);
}

static void
prefs_dialog_cancel_cb (GtkWidget *widget, GladeXML *data) 
{
	GnomeCCPreferences *prefs;

	prefs = gtk_object_get_data (GTK_OBJECT (data), "prefs_struct");
	gnomecc_preferences_copy (prefs, old_prefs);
	gtk_signal_emit (GTK_OBJECT (prefs),
			 gnomecc_preferences_signals[CHANGED_SIGNAL]);

	gnome_dialog_close (GNOME_DIALOG (prefs_dialog));
	prefs_dialog = NULL;
	prefs_dialog_data = NULL;
}

static void
set_single_window_controls_sensitive (GladeXML *data, gboolean s) 
{
#if 0
	GtkWidget *widget;

	widget = glade_xml_get_widget (prefs_dialog_data, "single_widget");
	gtk_widget_set_sensitive (widget, s);
	widget = glade_xml_get_widget (prefs_dialog_data, "multiple_widget");
	gtk_widget_set_sensitive (widget, s);
#endif
}

void
tree_widget_toggled_cb (GtkWidget *widget) 
{
	set_single_window_controls_sensitive
		(prefs_dialog_data,
		 !gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget)));
}

GtkWidget *
gnomecc_preferences_get_config_dialog (GnomeCCPreferences *prefs) 
{
	if (prefs_dialog_data) return prefs_dialog;

	old_prefs = gnomecc_preferences_clone (prefs);

	prefs_dialog_data = 
		glade_xml_new (GLADEDIR "/gnomecc.glade", "preferences_dialog",
			       NULL);

	if (!prefs_dialog_data) {
		g_warning ("Could not find data for preferences dialog");
		return NULL;
	}

	place_preferences (prefs_dialog_data, prefs);

	prefs_dialog = glade_xml_get_widget (prefs_dialog_data, 
					     "preferences_dialog");

	gnome_dialog_button_connect
		(GNOME_DIALOG (prefs_dialog), 0,
		 GTK_SIGNAL_FUNC (prefs_dialog_ok_cb),
		 prefs_dialog_data);

	gnome_dialog_button_connect
		(GNOME_DIALOG (prefs_dialog), 1,
		 GTK_SIGNAL_FUNC (prefs_dialog_apply_cb),
		 prefs_dialog_data);

	gnome_dialog_button_connect
		(GNOME_DIALOG (prefs_dialog), 2,
		 GTK_SIGNAL_FUNC (prefs_dialog_cancel_cb),
		 prefs_dialog_data);

	gtk_object_set_data (GTK_OBJECT (prefs_dialog_data), 
			     "prefs_struct", prefs);

	glade_xml_signal_connect (prefs_dialog_data,
				  "tree_widget_toggled_cb",
				  (GCallback) tree_widget_toggled_cb);

	return prefs_dialog;
}
