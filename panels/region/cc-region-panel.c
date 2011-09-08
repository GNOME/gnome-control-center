/*
 * Copyright (C) 2010 Intel, Inc
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * Author: Sergey Udaltsov <svu@gnome.org>
 *
 */

#include "cc-region-panel.h"
#include <gtk/gtk.h>

#include "gnome-region-panel-xkb.h"
#include "gnome-region-panel-lang.h"
#include "gnome-region-panel-formats.h"
#include "gnome-region-panel-system.h"

G_DEFINE_DYNAMIC_TYPE (CcRegionPanel, cc_region_panel, CC_TYPE_PANEL)

#define REGION_PANEL_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), CC_TYPE_REGION_PANEL, CcRegionPanelPrivate))

struct _CcRegionPanelPrivate {
	GtkBuilder *builder;
};

enum {
	PROP_0,
	PROP_ARGV
};

enum {
	LANGUAGE_PAGE,
	FORMATS_PAGE,
	LAYOUTS_PAGE,
	SYSTEM_PAGE
};

static void
cc_region_panel_set_page (CcRegionPanel *panel,
			  const char    *page)
{
	GtkWidget *notebook;
	int page_num;

	if (g_strcmp0 (page, "formats") == 0)
		page_num = FORMATS_PAGE;
	else if (g_strcmp0 (page, "layouts") == 0)
		page_num = LAYOUTS_PAGE;
	else if (g_strcmp0 (page, "system") == 0)
		page_num = SYSTEM_PAGE;
	else
		page_num = LANGUAGE_PAGE;

	notebook = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder, "region_notebook"));
	gtk_notebook_set_current_page (GTK_NOTEBOOK (notebook), page_num);
}

static void
cc_region_panel_set_property (GObject * object,
			      guint property_id,
			      const GValue * value,
			      GParamSpec * pspec)
{
	CcRegionPanel *self;

	self = CC_REGION_PANEL (object);

	switch (property_id) {
        case PROP_ARGV: {
                gchar **args;

                args = g_value_get_boxed (value);

                if (args && args[0]) {
                        cc_region_panel_set_page (self, args[0]);
                }
                break;
        }
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id,
						   pspec);
	}
}

static void
cc_region_panel_finalize (GObject * object)
{
	CcRegionPanel *panel;

	panel = CC_REGION_PANEL (object);

	if (panel->priv && panel->priv->builder)
		g_object_unref (panel->priv->builder);

	G_OBJECT_CLASS (cc_region_panel_parent_class)->finalize (object);
}

static void
cc_region_panel_class_init (CcRegionPanelClass * klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	g_type_class_add_private (klass, sizeof (CcRegionPanelPrivate));

	object_class->set_property = cc_region_panel_set_property;
	object_class->finalize = cc_region_panel_finalize;

	g_object_class_override_property (object_class, PROP_ARGV, "argv");
}

static void
cc_region_panel_class_finalize (CcRegionPanelClass * klass)
{
}

static void
cc_region_panel_init (CcRegionPanel * self)
{
	CcRegionPanelPrivate *priv;
	GtkWidget *prefs_widget;
	GError *error = NULL;

	priv = self->priv = REGION_PANEL_PRIVATE (self);

	priv->builder = gtk_builder_new ();

	gtk_builder_add_from_file (priv->builder,
				   GNOMECC_UI_DIR "/gnome-region-panel.ui",
				   &error);
	if (error != NULL) {
		g_warning ("Error loading UI file: %s", error->message);
		g_error_free (error);
		return;
	}

	prefs_widget = (GtkWidget *) gtk_builder_get_object (priv->builder,
							     "region_notebook");
	gtk_widget_set_size_request (GTK_WIDGET (prefs_widget), -1, 400);

	gtk_widget_reparent (prefs_widget, GTK_WIDGET (self));

	setup_xkb_tabs (priv->builder);
	setup_language (priv->builder);
	setup_formats (priv->builder);
	setup_system (priv->builder);
}

void
cc_region_panel_register (GIOModule * module)
{
	cc_region_panel_register_type (G_TYPE_MODULE (module));
	g_io_extension_point_implement (CC_SHELL_PANEL_EXTENSION_POINT,
					CC_TYPE_REGION_PANEL,
					"region", 0);
}
