/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2019 Purism SPC
 *
 * Written by: Adrien Plazas <adrien.plazas@puri.sm>
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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include <config.h>

#include <glib/gi18n.h>
#include <math.h>

#include "gnome-mouse-scroll-test.h"

struct _CcMouseScrollTest
{
	GtkDrawingArea parent_instance;

	GdkPixbuf *scroll_test;
	GdkPixbuf *scroll_test_gegl;
	GdkPixbuf *image;
	gboolean show_gegl;
	gint width;
	gint height;
};

G_DEFINE_TYPE (CcMouseScrollTest, cc_mouse_scroll_test, GTK_TYPE_DRAWING_AREA);

static void
load_images (CcMouseScrollTest *self)
{
	GError *error = NULL;

	g_clear_object (&self->scroll_test);
	self->scroll_test = gdk_pixbuf_new_from_resource_at_scale ("/org/gnome/control-center/mouse/scroll-test.svg",
								   self->width * gtk_widget_get_scale_factor (GTK_WIDGET (self)),
								   -1,
								   TRUE,
								   &error);
	if (error) {
		g_critical ("%s", error->message);
		g_clear_error (&error);
	}

	g_clear_object (&self->scroll_test_gegl);
	self->scroll_test_gegl = gdk_pixbuf_new_from_resource_at_scale ("/org/gnome/control-center/mouse/scroll-test-gegl.svg",
									self->width * gtk_widget_get_scale_factor (GTK_WIDGET (self)),
									-1,
									TRUE,
									&error);
	if (error) {
		g_critical ("%s", error->message);
		g_clear_error (&error);
	}

	self->image = (self->show_gegl) ? self->scroll_test_gegl : self->scroll_test;
	gtk_widget_queue_resize (GTK_WIDGET (self));
}

static void
cc_mouse_scroll_test_finalize (GObject *object)
{
	CcMouseScrollTest *self = CC_MOUSE_SCROLL_TEST (object);

	g_clear_object (&self->scroll_test);
	g_clear_object (&self->scroll_test_gegl);

	G_OBJECT_CLASS (cc_mouse_scroll_test_parent_class)->finalize (object);
}

static GtkSizeRequestMode
cc_mouse_scroll_test_get_request_mode (GtkWidget *widget)
{
	return GTK_SIZE_REQUEST_HEIGHT_FOR_WIDTH;
}

static void
cc_mouse_scroll_test_get_preferred_width (GtkWidget *widget,
                                          gint      *minimum,
                                          gint      *natural)
{
	CcMouseScrollTest *self = CC_MOUSE_SCROLL_TEST (widget);

	if (minimum)
		*minimum = 1;
	if (natural)
		*natural = self->width;
}

static void
cc_mouse_scroll_test_get_preferred_height_and_baseline_for_width (GtkWidget *widget,
                                                                  gint       width,
                                                                  gint      *minimum,
                                                                  gint      *natural,
                                                                  gint      *minimum_baseline,
                                                                  gint      *natural_baseline)
{
	CcMouseScrollTest *self = CC_MOUSE_SCROLL_TEST (widget);
	gint source_width = self->width;
	gint source_height = self->height;
	gdouble ratio = (gdouble) ABS (width) / (gdouble) ABS (source_width);
	gint height = CLAMP ((gint) (source_height * ratio), 1, source_height);

	if (minimum)
		*minimum = height;
	if (natural)
		*natural = height;
}

static gboolean
cc_mouse_scroll_test_draw (GtkWidget *widget,
                           cairo_t   *cr)
{
	CcMouseScrollTest *self = CC_MOUSE_SCROLL_TEST (widget);
	cairo_surface_t *surface = gdk_cairo_surface_create_from_pixbuf (self->image, gtk_widget_get_scale_factor (widget), NULL);
	int alloc_width = gtk_widget_get_allocated_width (widget);
	int alloc_height = gtk_widget_get_allocated_height (widget);
	int image_width = self->width;
	int image_height = self->height;
	gdouble scale = CLAMP ((gdouble) alloc_width / (gdouble) image_width, 0.0, 1.0);
	int pad_x = (alloc_width - CLAMP ((int) (image_width * scale), 1, image_width)) / 2;
	int pad_y = (alloc_height - CLAMP ((int) (image_height * scale), 1, image_height)) / 2;

	cairo_scale (cr, scale, scale);
	cairo_set_source_surface (cr, surface, pad_x, pad_y);
	cairo_paint (cr);

	cairo_surface_destroy (surface);

	return TRUE;
}

static void
cc_mouse_scroll_test_class_init (CcMouseScrollTestClass *klass)
{
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = cc_mouse_scroll_test_finalize;

	widget_class->get_request_mode = cc_mouse_scroll_test_get_request_mode;
	widget_class->get_preferred_width = cc_mouse_scroll_test_get_preferred_width;
	widget_class->get_preferred_height_and_baseline_for_width = cc_mouse_scroll_test_get_preferred_height_and_baseline_for_width;
	widget_class->draw = cc_mouse_scroll_test_draw;
}

static void
cc_mouse_scroll_test_init (CcMouseScrollTest *self)
{
	g_autoptr(GdkPixbuf) pixbuf;
	GError *error = NULL;

	pixbuf = gdk_pixbuf_new_from_resource ("/org/gnome/control-center/mouse/scroll-test.svg", &error);
	if (error) {
		g_critical ("%s", error->message);
		g_clear_error (&error);
	} else {
		self->width = gdk_pixbuf_get_width (pixbuf);
		self->height = gdk_pixbuf_get_height (pixbuf);
	}
	load_images (self);

	g_signal_connect (self, "notify::scale-factor", G_CALLBACK (load_images), NULL);
}

void
cc_mouse_scroll_test_set_show_gegl (CcMouseScrollTest *self,
                                    gboolean           show_gegl)
{
	g_assert (CC_IS_MOUSE_SCROLL_TEST (self));

	self->show_gegl = !!show_gegl;

	self->image = self->show_gegl ? self->scroll_test_gegl : self->scroll_test;
	gtk_widget_queue_resize (GTK_WIDGET (self));
}
