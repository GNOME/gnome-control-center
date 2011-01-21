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

#define WID(s) GTK_WIDGET (gtk_builder_get_object (dialog, s))

G_DEFINE_DYNAMIC_TYPE (CcRegionPanel, cc_region_panel, CC_TYPE_PANEL)

#define REGION_PANEL_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), CC_TYPE_REGION_PANEL, CcRegionPanelPrivate))

struct _CcRegionPanelPrivate {
	GtkBuilder *builder;
};


static void
cc_region_panel_get_property (GObject * object,
				guint property_id,
				GValue * value, GParamSpec * pspec)
{
	switch (property_id) {
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id,
						   pspec);
	}
}

static void
cc_region_panel_set_property (GObject * object,
				guint property_id,
				const GValue * value, GParamSpec * pspec)
{
	switch (property_id) {
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id,
						   pspec);
	}
}

static void
cc_region_panel_dispose (GObject * object)
{
	CcRegionPanelPrivate *priv = CC_REGION_PANEL (object)->priv;

	if (priv->builder) {
		g_object_unref (priv->builder);
		priv->builder = NULL;
	}

	G_OBJECT_CLASS (cc_region_panel_parent_class)->dispose (object);
}

static void
cc_region_panel_finalize (GObject * object)
{
	G_OBJECT_CLASS (cc_region_panel_parent_class)->finalize (object);
}

static void
cc_region_panel_class_init (CcRegionPanelClass * klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	g_type_class_add_private (klass, sizeof (CcRegionPanelPrivate));

	object_class->get_property = cc_region_panel_get_property;
	object_class->set_property = cc_region_panel_set_property;
	object_class->dispose = cc_region_panel_dispose;
	object_class->finalize = cc_region_panel_finalize;
}

static void
cc_region_panel_class_finalize (CcRegionPanelClass * klass)
{
}

static void
setup_images (GtkBuilder * dialog)
{
	GtkWidget *image;

	image =
	    gtk_image_new_from_stock (GTK_STOCK_ADD, GTK_ICON_SIZE_BUTTON);
	gtk_button_set_image (GTK_BUTTON (WID ("xkb_layouts_add")), image);

	image =
	    gtk_image_new_from_stock (GTK_STOCK_REFRESH,
				      GTK_ICON_SIZE_BUTTON);
	gtk_button_set_image (GTK_BUTTON (WID ("xkb_reset_to_defaults")),
			      image);
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
				   GNOMECC_UI_DIR
				   "/gnome-region-panel.ui",
				   &error);
	if (error != NULL) {
		g_warning ("Error loading UI file: %s", error->message);
		return;
	}

	setup_images (priv->builder);
	setup_xkb_tabs (priv->builder);

	prefs_widget = (GtkWidget *) gtk_builder_get_object (priv->builder,
							     "region_notebook");
	gtk_widget_set_size_request (GTK_WIDGET (prefs_widget), -1, 400);

	gtk_widget_reparent (prefs_widget, GTK_WIDGET (self));
}

void
cc_region_panel_register (GIOModule * module)
{
	cc_region_panel_register_type (G_TYPE_MODULE (module));
	g_io_extension_point_implement (CC_SHELL_PANEL_EXTENSION_POINT,
					CC_TYPE_REGION_PANEL,
					"region", 0);
}
