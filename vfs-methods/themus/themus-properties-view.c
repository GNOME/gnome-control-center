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

#include <config.h>

#include "themus-properties-view.h"
#include <gnome-theme-info.h>

#include <glib/gi18n-lib.h>
#include <gtk/gtk.h>

#define LOAD_BUFFER_SIZE 8192

struct ThemusPropertiesViewDetails {
    GtkWidget *description_caption;
    GtkWidget *description;
    GtkWidget *gtk_caption;
    GtkWidget *gtk_theme;
    GtkWidget *window_caption;
    GtkWidget *window_theme;
    GtkWidget *icon_caption;
    GtkWidget *icon_theme;
};

static GType tpv_type = 0;
static GObjectClass *parent_class = NULL;
static void themus_properties_view_init (ThemusPropertiesView *self);
static void themus_properties_view_class_init (ThemusPropertiesViewClass *class);
static void themus_properties_view_destroy (GtkObject *object);

GType
themus_properties_view_get_type (void)
{
    return tpv_type;
}

void
themus_properties_view_register_type (GTypeModule *module)
{
    static const GTypeInfo info = {
	sizeof (ThemusPropertiesViewClass),
        (GBaseInitFunc) NULL,
        (GBaseFinalizeFunc) NULL,
        (GClassInitFunc) themus_properties_view_class_init,
        NULL,
        NULL,
        sizeof (ThemusPropertiesView),
        0,
        (GInstanceInitFunc) themus_properties_view_init
    };

    tpv_type = g_type_module_register_type (module, GTK_TYPE_TABLE,
					    "ThemusPropertiesView",
					    &info, 0);
}

static void
themus_properties_view_class_init (ThemusPropertiesViewClass *class)
{
    g_type_class_add_private (class, sizeof (ThemusPropertiesViewDetails));
    parent_class = g_type_class_peek_parent (class);
    GTK_OBJECT_CLASS (class)->destroy = themus_properties_view_destroy;
}

static void
add_atk_relation (GtkWidget 		*obj1, 
		  GtkWidget 		*obj2, 
		  AtkRelationType 	 rel_type)
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
do_table_attach (GtkTable *table, GtkWidget *child,
		 guint x, guint y, guint w, guint h, guint xalign)
{
    gtk_misc_set_alignment (GTK_MISC (child), xalign, 0.5);
    gtk_table_attach (table, child, x, x + w, y, y + h, GTK_FILL, 0, 6, 6);
}

static void
themus_properties_view_init (ThemusPropertiesView *self)
{
    gchar *str;

    self->details = G_TYPE_INSTANCE_GET_PRIVATE (self,
						 THEMUS_TYPE_PROPERTIES_VIEW,
						 ThemusPropertiesViewDetails);

    gnome_theme_init (FALSE);

    gtk_table_resize (GTK_TABLE (self), 3, 2);
    gtk_table_set_homogeneous (GTK_TABLE (self), FALSE);

    self->details->description_caption = gtk_label_new (NULL);
    str = g_strconcat ("<b>", _("Description"), ":</b>", NULL);
    gtk_label_set_markup (GTK_LABEL (self->details->description_caption), str);
    g_free (str);
    self->details->description = gtk_label_new (NULL);
    gtk_label_set_line_wrap (GTK_LABEL (self->details->description), TRUE);

    self->details->gtk_caption = gtk_label_new (NULL);
    str = g_strconcat ("<b>", _("Control theme"), ":</b>", NULL);
    gtk_label_set_markup (GTK_LABEL (self->details->gtk_caption), str);
    g_free (str);
    self->details->gtk_theme = gtk_label_new (NULL);

    self->details->window_caption = gtk_label_new (NULL);
    str = g_strconcat ("<b>", _("Window border theme"), ":</b>", NULL);
    gtk_label_set_markup (GTK_LABEL (self->details->window_caption), str);
    g_free (str);
    self->details->window_theme = gtk_label_new (NULL);

    self->details->icon_caption = gtk_label_new (NULL);
    str = g_strconcat ("<b>", _("Icon theme"), ":</b>", NULL);
    gtk_label_set_markup (GTK_LABEL (self->details->icon_caption), str);
    g_free (str);
    self->details->icon_theme = gtk_label_new (NULL);
	
    do_table_attach (GTK_TABLE (self), self->details->description_caption,
		     0, 0, 1, 1, 1.0);
    do_table_attach (GTK_TABLE (self), self->details->description,
		     1, 0, 1, 1, 0.0);
    do_table_attach (GTK_TABLE (self), self->details->gtk_caption,
		     0, 1, 1, 1, 1.0);
    do_table_attach (GTK_TABLE (self), self->details->gtk_theme,
		     1, 1, 1, 1, 0.0);
    do_table_attach (GTK_TABLE (self), self->details->window_caption,
		     0, 2, 1, 1, 1.0);
    do_table_attach (GTK_TABLE (self), self->details->window_theme,
		     1, 2, 1, 1, 0.0);
    do_table_attach (GTK_TABLE (self), self->details->icon_caption,
		     0, 3, 1, 1, 1.0);
    do_table_attach (GTK_TABLE (self), self->details->icon_theme,
		     1, 3, 1, 1, 0.0);
	
    add_atk_relation (self->details->gtk_caption, self->details->gtk_theme,
		      ATK_RELATION_LABEL_FOR);
    add_atk_relation (self->details->gtk_theme, self->details->gtk_caption,
		      ATK_RELATION_LABELLED_BY);
    add_atk_relation (self->details->window_caption, self->details->window_theme,
		      ATK_RELATION_LABEL_FOR);
    add_atk_relation (self->details->window_theme, self->details->window_caption,
		      ATK_RELATION_LABELLED_BY);
    add_atk_relation (self->details->icon_caption, self->details->icon_theme,
		      ATK_RELATION_LABEL_FOR);
    add_atk_relation (self->details->icon_theme, self->details->icon_caption,
		      ATK_RELATION_LABELLED_BY);

    gtk_widget_show_all (GTK_WIDGET (self));
}

static void
themus_properties_view_destroy (GtkObject *object)
{
    ThemusPropertiesView *self;

    self = THEMUS_PROPERTIES_VIEW (object);

    self->details->description_caption = NULL;
    self->details->description = NULL;
    self->details->gtk_caption = NULL;
    self->details->gtk_theme = NULL;
    self->details->window_caption = NULL;
    self->details->window_theme = NULL;
    self->details->icon_caption = NULL;
    self->details->icon_theme = NULL;

    GTK_OBJECT_CLASS (parent_class)->destroy (object);
}

GtkWidget *
themus_properties_view_new (const char *location)
{
    ThemusPropertiesView *self;

    self = g_object_new (THEMUS_TYPE_PROPERTIES_VIEW, NULL);
    themus_properties_view_set_location (self, location);

    return GTK_WIDGET (self);
}

void
themus_properties_view_set_location (ThemusPropertiesView *self,
				     const char *location)
{
    GnomeVFSURI *uri;
    GnomeThemeMetaInfo *theme;
	
    g_assert (THEMUS_IS_PROPERTIES_VIEW (self));

    if (location) {
	uri = gnome_vfs_uri_new (location);

	theme = gnome_theme_read_meta_theme (uri);
	gnome_vfs_uri_unref (uri);
	
	gtk_label_set_text (GTK_LABEL (self->details->description),
			    theme->comment);
	gtk_label_set_text (GTK_LABEL (self->details->gtk_theme),
			    theme->gtk_theme_name);
	gtk_label_set_text (GTK_LABEL (self->details->window_theme),
			    theme->metacity_theme_name);
	gtk_label_set_text (GTK_LABEL (self->details->icon_theme),
			    theme->icon_theme_name);

	gnome_theme_meta_info_free (theme);
    } else {
	gtk_label_set_text (GTK_LABEL (self->details->description), "");
	gtk_label_set_text (GTK_LABEL (self->details->gtk_theme), "");
	gtk_label_set_text (GTK_LABEL (self->details->window_theme), "");
	gtk_label_set_text (GTK_LABEL (self->details->icon_theme), "");
    }
}

