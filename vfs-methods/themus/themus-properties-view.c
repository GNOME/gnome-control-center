/* -*- mode: C; c-basic-offset: 4 -*-
 * themus - utilities for GNOME themes
 * Copyright (C) 2003  Andrew Sobala <aes@gnome.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include "themus-properties-view.h"
#include <gnome-theme-info.h>

#include <gtk/gtk.h>
#include <libbonobo.h>
#include <atk/atk.h>

#define LOAD_BUFFER_SIZE 8192

struct ThemusThemePropertiesViewDetails {
	char *location;
	GtkWidget *table;
	GtkWidget *description_caption;
	GtkWidget *description;
	GtkWidget *gtk_caption;
	GtkWidget *gtk_theme;
	GtkWidget *window_caption;
	GtkWidget *window_theme;
	GtkWidget *icon_caption;
	GtkWidget *icon_theme;
};

enum {
	PROP_URI,
};

static GObjectClass *parent_class = NULL;

static void
add_atk_relation (GtkWidget 		*obj1, 
		  GtkWidget 		*obj2, 
		  AtkRelationType 	rel_type)
{
	AtkObject *atk_obj1, *atk_obj2;
	AtkRelationSet *relation_set;
	AtkRelation *relation;
	
	g_return_if_fail (GTK_IS_WIDGET(obj1));
	g_return_if_fail (GTK_IS_WIDGET(obj2));
	
	atk_obj1 = gtk_widget_get_accessible(obj1);
			
	atk_obj2 = gtk_widget_get_accessible(obj2);
	
	relation_set = atk_object_ref_relation_set (atk_obj1);
	relation = atk_relation_new(&atk_obj2, 1, rel_type);
	atk_relation_set_add(relation_set, relation);
	g_object_unref(G_OBJECT (relation));
	
}

static void
themus_theme_properties_view_finalize (GObject *object)
{
	ThemusThemePropertiesView *view;

	view = THEMUS_THEME_PROPERTIES_VIEW (object);
	g_free (view->details);

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
load_location (ThemusThemePropertiesView *view,
	       const char *location)
{
	GnomeVFSURI *uri;
	GnomeThemeMetaInfo *theme;
	
	g_assert (THEMUS_IS_THEME_PROPERTIES_VIEW (view));
	g_assert (location != NULL);
	
	uri = gnome_vfs_uri_new (location);

	theme = gnome_theme_read_meta_theme (uri);
	gnome_vfs_uri_unref (uri);
	
	gtk_label_set_text (GTK_LABEL (view->details->description),
						g_strdup (theme->comment));
	gtk_label_set_text (GTK_LABEL (view->details->gtk_theme),
						g_strdup (theme->gtk_theme_name));
	gtk_label_set_text (GTK_LABEL (view->details->window_theme),
						g_strdup (theme->metacity_theme_name));
	gtk_label_set_text (GTK_LABEL (view->details->icon_theme),
						g_strdup (theme->icon_theme_name));
}

static void
get_property (BonoboPropertyBag *bag,
	      BonoboArg         *arg,
	      guint              arg_id,
	      CORBA_Environment *ev,
	      gpointer           user_data)
{
	ThemusThemePropertiesView *view = user_data;

	if (arg_id == PROP_URI) {
		BONOBO_ARG_SET_STRING (arg, view->details->location);
	}
}

static void
set_property (BonoboPropertyBag *bag,
	      const BonoboArg   *arg,
	      guint              arg_id,
	      CORBA_Environment *ev,
	      gpointer           user_data)
{
	ThemusThemePropertiesView *view = user_data;

	if (arg_id == PROP_URI) {
		load_location (view, BONOBO_ARG_GET_STRING (arg));
	}
}

static void
themus_theme_properties_view_class_init (ThemusThemePropertiesViewClass *class)
{
	parent_class = g_type_class_peek_parent (class);
	
	G_OBJECT_CLASS (class)->finalize = themus_theme_properties_view_finalize;
}

static void
do_table_attach (GtkWidget *table, GtkWidget *child, guint x, guint y, guint w, guint h,
guint xalign)
{
	gtk_misc_set_alignment (GTK_MISC (child), xalign, 0.5);
	gtk_table_attach (GTK_TABLE (table), child, x, x + w, y, y + h, GTK_FILL, 0, 6, 6);
}

static void
themus_theme_properties_view_init (ThemusThemePropertiesView *view)
{
	BonoboPropertyBag *pb;

	gnome_theme_init (FALSE);

	view->details = g_new0 (ThemusThemePropertiesViewDetails, 1);

	view->details->table = gtk_table_new (3, 2, FALSE);
	view->details->description_caption = gtk_label_new (NULL);
	gtk_label_set_markup (GTK_LABEL (view->details->description_caption),
			g_strconcat ("<b>", _("Description"), ":</b>", NULL));
	view->details->description = gtk_label_new (NULL);
	gtk_label_set_line_wrap (GTK_LABEL (view->details->description), TRUE);
	view->details->gtk_caption = gtk_label_new (NULL);
	gtk_label_set_markup (GTK_LABEL (view->details->gtk_caption),
			g_strconcat ("<b>", _("Control theme"), ":</b>", NULL));
	view->details->gtk_theme = gtk_label_new (NULL);
	view->details->window_caption = gtk_label_new (NULL);
	gtk_label_set_markup (GTK_LABEL (view->details->window_caption),
			g_strconcat ("<b>", _("Window border theme"), ":</b>", NULL));
	view->details->window_theme = gtk_label_new (NULL);
	view->details->icon_caption = gtk_label_new (NULL);
	gtk_label_set_markup (GTK_LABEL (view->details->icon_caption),
			g_strconcat ("<b>", _("Icon theme"), ":</b>", NULL));
	view->details->icon_theme = gtk_label_new (NULL);
	
	do_table_attach (view->details->table, view->details->description_caption,
							0, 0, 1, 1, 1.0);
	do_table_attach (view->details->table, view->details->description,
							1, 0, 1, 1, 0.0);
	do_table_attach (view->details->table, view->details->gtk_caption,
							0, 1, 1, 1, 1.0);
	do_table_attach (view->details->table, view->details->gtk_theme,
							1, 1, 1, 1, 0.0);
	do_table_attach (view->details->table, view->details->window_caption,
							0, 2, 1, 1, 1.0);
	do_table_attach (view->details->table, view->details->window_theme,
							1, 2, 1, 1, 0.0);
	do_table_attach (view->details->table, view->details->icon_caption,
							0, 3, 1, 1, 1.0);
	do_table_attach (view->details->table, view->details->icon_theme,
							1, 3, 1, 1, 0.0);
	
	add_atk_relation (view->details->gtk_caption, view->details->gtk_theme,
						ATK_RELATION_LABEL_FOR);
	add_atk_relation (view->details->gtk_theme, view->details->gtk_caption,
						ATK_RELATION_LABELLED_BY);
	add_atk_relation (view->details->window_caption, view->details->window_theme,
						ATK_RELATION_LABEL_FOR);
	add_atk_relation (view->details->window_theme, view->details->window_caption,
						ATK_RELATION_LABELLED_BY);
	add_atk_relation (view->details->icon_caption, view->details->icon_theme,
						ATK_RELATION_LABEL_FOR);
	add_atk_relation (view->details->icon_theme, view->details->icon_caption,
						ATK_RELATION_LABELLED_BY);
	
	gtk_widget_show_all (view->details->table);
	
	bonobo_control_construct (BONOBO_CONTROL (view), view->details->table);
	pb = bonobo_property_bag_new (get_property, set_property,
				      view);
	bonobo_property_bag_add (pb, "URI", 0, BONOBO_ARG_STRING,
				 NULL, _("URI currently displayed"), 0);

	bonobo_control_set_properties (BONOBO_CONTROL (view), BONOBO_OBJREF (pb), NULL);
	bonobo_object_release_unref (BONOBO_OBJREF (pb), NULL);
}

BONOBO_TYPE_FUNC (ThemusThemePropertiesView, BONOBO_TYPE_CONTROL, themus_theme_properties_view);
