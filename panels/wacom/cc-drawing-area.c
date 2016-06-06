/*
 * Copyright © 2016 Red Hat, Inc.
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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Carlos Garnacho <carlosg@gnome.org>
 */

#include "config.h"
#include <cairo/cairo.h>
#include "cc-drawing-area.h"

typedef struct _CcDrawingArea CcDrawingArea;

struct _CcDrawingArea {
	GtkEventBox parent;
	GdkDevice *current_device;
	cairo_surface_t *surface;
	cairo_t *cr;
};

G_DEFINE_TYPE (CcDrawingArea, cc_drawing_area, GTK_TYPE_EVENT_BOX)

static void
ensure_drawing_surface (CcDrawingArea *area,
			gint           width,
			gint           height)
{
	if (!area->surface ||
	    cairo_image_surface_get_width (area->surface) != width ||
	    cairo_image_surface_get_height (area->surface) != height) {
		cairo_surface_t *surface;

		surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32,
						      width, height);
		if (area->surface) {
			cairo_t *cr;

			cr = cairo_create (surface);
			cairo_set_source_surface (cr, area->surface, 0, 0);
			cairo_paint (cr);

			cairo_surface_destroy (area->surface);
			cairo_destroy (area->cr);
			cairo_destroy (cr);
		}

		area->surface = surface;
		area->cr = cairo_create (surface);
	}
}

static void
cc_drawing_area_size_allocate (GtkWidget     *widget,
			       GtkAllocation *allocation)
{
	CcDrawingArea *area = CC_DRAWING_AREA (widget);

	ensure_drawing_surface (area, allocation->width, allocation->height);

	GTK_WIDGET_CLASS (cc_drawing_area_parent_class)->size_allocate (widget,
									allocation);
}

static void
cc_drawing_area_map (GtkWidget *widget)
{
	GtkAllocation allocation;

	GTK_WIDGET_CLASS (cc_drawing_area_parent_class)->map (widget);

	gtk_widget_get_allocation (widget, &allocation);
	ensure_drawing_surface (CC_DRAWING_AREA (widget),
				allocation.width, allocation.height);
}

static void
cc_drawing_area_unmap (GtkWidget *widget)
{
	CcDrawingArea *area = CC_DRAWING_AREA (widget);

	if (area->cr) {
		cairo_destroy (area->cr);
		area->cr = NULL;
	}

	if (area->surface) {
		cairo_surface_destroy (area->surface);
		area->surface = NULL;
	}

	GTK_WIDGET_CLASS (cc_drawing_area_parent_class)->unmap (widget);
}

static gboolean
cc_drawing_area_draw (GtkWidget *widget,
		      cairo_t   *cr)
{
	CcDrawingArea *area = CC_DRAWING_AREA (widget);
	GtkAllocation allocation;

	GTK_WIDGET_CLASS (cc_drawing_area_parent_class)->draw (widget, cr);

	gtk_widget_get_allocation (widget, &allocation);
	cairo_set_source_rgb (cr, 1, 1, 1);
	cairo_paint (cr);

	cairo_set_source_surface (cr, area->surface, 0, 0);
	cairo_paint (cr);

	cairo_set_source_rgb (cr, 0.6, 0.6, 0.6);
	cairo_rectangle (cr, 0, 0, allocation.width, allocation.height);
	cairo_stroke (cr);

	return FALSE;
}

static gboolean
cc_drawing_area_event (GtkWidget *widget,
		       GdkEvent  *event)
{
	CcDrawingArea *area = CC_DRAWING_AREA (widget);
	GdkInputSource source;
	GdkDeviceTool *tool;
	GdkDevice *device;

	device = gdk_event_get_source_device (event);

	if (!device)
		return GDK_EVENT_PROPAGATE;

	source = gdk_device_get_source (device);
	tool = gdk_event_get_device_tool (event);

	if (source != GDK_SOURCE_PEN && source != GDK_SOURCE_ERASER)
		return GDK_EVENT_PROPAGATE;

	if (area->current_device && area->current_device != device)
		return GDK_EVENT_PROPAGATE;

	if (event->type == GDK_BUTTON_PRESS &&
	    event->button.button == 1 && !area->current_device) {
		area->current_device = device;
	} else if (event->type == GDK_BUTTON_RELEASE &&
		   event->button.button == 1 && area->current_device) {
		cairo_new_path (area->cr);
		area->current_device = NULL;
	} else if (event->type == GDK_MOTION_NOTIFY &&
		   event->motion.state & GDK_BUTTON1_MASK) {
		gdouble x, y, pressure;

		gdk_event_get_coords (event, &x, &y);
		gdk_event_get_axis (event, GDK_AXIS_PRESSURE, &pressure);

		if (gdk_device_tool_get_tool_type (tool) == GDK_DEVICE_TOOL_TYPE_ERASER) {
			cairo_set_line_width (area->cr, 10 * pressure);
			cairo_set_operator (area->cr, CAIRO_OPERATOR_DEST_OUT);
		} else {
			cairo_set_line_width (area->cr, 4 * pressure);
			cairo_set_operator (area->cr, CAIRO_OPERATOR_SATURATE);
		}

		cairo_set_source_rgba (area->cr, 0, 0, 0, pressure);
		cairo_line_to (area->cr, x, y);
		cairo_stroke (area->cr);

		cairo_move_to (area->cr, x, y);

		gtk_widget_queue_draw (widget);

		return GDK_EVENT_STOP;
	}

	return GDK_EVENT_PROPAGATE;
}

static void
cc_drawing_area_class_init (CcDrawingAreaClass *klass)
{
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

	widget_class->size_allocate = cc_drawing_area_size_allocate;
	widget_class->draw = cc_drawing_area_draw;
	widget_class->event = cc_drawing_area_event;
	widget_class->map = cc_drawing_area_map;
	widget_class->unmap = cc_drawing_area_unmap;
}

static void
cc_drawing_area_init (CcDrawingArea *area)
{
	gtk_event_box_set_above_child (GTK_EVENT_BOX (area), TRUE);
	gtk_widget_add_events (GTK_WIDGET (area),
			       GDK_BUTTON_PRESS_MASK |
			       GDK_BUTTON_RELEASE_MASK |
			       GDK_POINTER_MOTION_MASK);
}

GtkWidget *
cc_drawing_area_new (void)
{
	return g_object_new (CC_TYPE_DRAWING_AREA, NULL);
}
