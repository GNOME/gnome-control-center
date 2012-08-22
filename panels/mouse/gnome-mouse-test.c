/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2012 Red Hat, Inc.
 *
 * Written by: Ondrej Holy <oholy@redhat.com>,
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

#include <config.h>

#include <glib/gi18n.h>
#include <string.h>
#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <gnome-settings-daemon/gsd-enums.h>
#include <math.h>

#include "gnome-mouse-test.h"

#include <sys/types.h>
#include <sys/stat.h>

#define WID(x) (GtkWidget*) gtk_builder_get_object (dialog, x)

/* Click test button sizes. */
#define SHADOW_SIZE (10.0 / 180 * size)
#define SHADOW_SHIFT_Y (-1.0 / 180 * size)
#define SHADOW_OPACITY (0.15 / 180 * size)
#define OUTER_CIRCLE_SIZE (22.0 / 180 * size)
#define ANNULUS_SIZE (6.0 / 180 * size)
#define INNER_CIRCLE_SIZE (52.0 / 180 * size)

static void setup_information_label (GtkWidget *widget);
static void setup_scroll_image      (GtkWidget *widget);

enum
{
	DOUBLE_CLICK_TEST_OFF,
	DOUBLE_CLICK_TEST_MAYBE,
	DOUBLE_CLICK_TEST_ON,
	DOUBLE_CLICK_TEST_STILL_ON,
	DOUBLE_CLICK_TEST_ALMOST_THERE,
	DOUBLE_CLICK_TEST_GEGL
};

/* State in testing the double-click speed. Global for a great deal of
 * convenience
 */
static gint double_click_state = DOUBLE_CLICK_TEST_OFF;
static gint button_state = 0;

static GSettings *mouse_settings = NULL;

static gint information_label_timeout_id = 0;
static gint button_drawing_area_timeout_id = 0;
static gint scroll_image_timeout_id = 0;

/* Double Click handling */

struct test_data_t
{
	gint *timeout_id;
	GtkWidget *widget;
};

/* Timeout for the double click test */

static gboolean
test_maybe_timeout (struct test_data_t *data)
{
	double_click_state = DOUBLE_CLICK_TEST_OFF;

	gtk_widget_queue_draw (data->widget);

	*data->timeout_id = 0;

	return FALSE;
}

/* Timeout for the information label */

static gboolean
information_label_timeout (struct test_data_t *data)
{
	setup_information_label (data->widget);

	*data->timeout_id = 0;

	return FALSE;
}

/* Timeout for the scroll image */

static gboolean
scroll_image_timeout (struct test_data_t *data)
{
	setup_scroll_image (data->widget);

	*data->timeout_id = 0;

	return FALSE;
}

/* Set information label according global state variables. */

static void
setup_information_label (GtkWidget *widget)
{
	static struct test_data_t data;
	gchar *message = NULL;
	gchar *label_text = NULL;
	gboolean double_click;

	if (information_label_timeout_id != 0) {
		g_source_remove (information_label_timeout_id);
		information_label_timeout_id = 0;
	}

	if (double_click_state == DOUBLE_CLICK_TEST_OFF) {
		gtk_label_set_label (GTK_LABEL (widget), _("Try clicking, double clicking, scrolling"));
		return;
	}

	if (double_click_state == DOUBLE_CLICK_TEST_GEGL) {
		message = _("Five clicks, GEGL time!"), "</b>";
	} else {
		double_click = (double_click_state >= DOUBLE_CLICK_TEST_ON);
		switch (button_state) {
		case 1:
			message = (double_click) ? _("Double click, primary button") : _("Single click, primary button");
			break;
		case 2:
			message = (double_click) ? _("Double click, middle button") : _("Single click, middle button");
			break;
		case 3:
			message = (double_click) ? _("Double click, secondary button") : _("Single click, secondary button");
			break;
		}
	}

	label_text = g_strconcat ("<b>", message, "</b>", NULL);
	gtk_label_set_markup (GTK_LABEL (widget), label_text);
	g_free (label_text);

	data.widget = widget;
	data.timeout_id = &information_label_timeout_id;
	information_label_timeout_id = g_timeout_add (2500,
						      (GSourceFunc) information_label_timeout,
						      &data);
}

/* Update scroll image according to the global state variables */

static void
setup_scroll_image (GtkWidget *widget)
{
	static struct test_data_t data;
	char *filename;

	if (scroll_image_timeout_id != 0) {
		g_source_remove (scroll_image_timeout_id);
		scroll_image_timeout_id = 0;
	}

	if (double_click_state == DOUBLE_CLICK_TEST_GEGL)
		filename = GNOMECC_UI_DIR "/scroll-test-gegl.svg";
	else
		filename = GNOMECC_UI_DIR "/scroll-test.svg";
	gtk_image_set_from_file (GTK_IMAGE (widget), filename);

	if (double_click_state != DOUBLE_CLICK_TEST_GEGL)
		return;

	data.widget = widget;
	data.timeout_id = &scroll_image_timeout_id;
	scroll_image_timeout_id = g_timeout_add (5000,
						 (GSourceFunc) scroll_image_timeout,
						 &data);
}


/* Callback issued when the user clicks the double click testing area. */

static gboolean
button_drawing_area_button_press_event (GtkWidget      *widget,
					GdkEventButton *event,
					GtkBuilder     *dialog)
{
	gint                       double_click_time;
	static struct test_data_t  data;
	static guint32             double_click_timestamp = 0;

	if (event->type != GDK_BUTTON_PRESS || event->button > 3)
		return FALSE;

	double_click_time = g_settings_get_int (mouse_settings, "double-click");

	if (button_drawing_area_timeout_id != 0) {
		g_source_remove  (button_drawing_area_timeout_id);
		button_drawing_area_timeout_id = 0;
	}

	/* Ignore fake double click using different buttons. */
	if (double_click_state != DOUBLE_CLICK_TEST_OFF && button_state != event->button)
		double_click_state = DOUBLE_CLICK_TEST_OFF;

	switch (double_click_state) {
	case DOUBLE_CLICK_TEST_OFF:
		double_click_state = DOUBLE_CLICK_TEST_MAYBE;
		data.widget = widget;
		data.timeout_id = &button_drawing_area_timeout_id;
		button_drawing_area_timeout_id = g_timeout_add (double_click_time, (GSourceFunc) test_maybe_timeout, &data);
		break;
	case DOUBLE_CLICK_TEST_MAYBE:
	case DOUBLE_CLICK_TEST_ON:
	case DOUBLE_CLICK_TEST_STILL_ON:
	case DOUBLE_CLICK_TEST_ALMOST_THERE:
		if (event->time - double_click_timestamp < double_click_time) {
			double_click_state++;
			data.widget = widget;
			data.timeout_id = &button_drawing_area_timeout_id;
			button_drawing_area_timeout_id = g_timeout_add (2500, (GSourceFunc) test_maybe_timeout, &data);
		} else {
			test_maybe_timeout (&data);
		}
		break;
	case DOUBLE_CLICK_TEST_GEGL:
		double_click_state = DOUBLE_CLICK_TEST_OFF;
		break;
	}

	double_click_timestamp = event->time;

	gtk_widget_queue_draw (widget);

	button_state = event->button;
	setup_information_label (WID ("information_label"));
	setup_scroll_image (WID ("image"));

	return TRUE;
}

static gboolean
button_drawing_area_draw_event (GtkWidget  *widget,
				cairo_t    *cr,
				GtkBuilder *dialog)
{
	gdouble center_x, center_y, size;
	GdkRGBA inner_color, outer_color;
	cairo_pattern_t *pattern;

	size = MAX (MIN (gtk_widget_get_allocated_width (widget), gtk_widget_get_allocated_height (widget)), 1);
	center_x = gtk_widget_get_allocated_width (widget) / 2.0;
	center_y = gtk_widget_get_allocated_height (widget) / 2.0;

	switch (double_click_state) {
	case DOUBLE_CLICK_TEST_ON:
	case DOUBLE_CLICK_TEST_STILL_ON:
	case DOUBLE_CLICK_TEST_ALMOST_THERE:
	case DOUBLE_CLICK_TEST_GEGL:
		gdk_rgba_parse (&outer_color, "#729fcf");
		gdk_rgba_parse (&inner_color, "#729fcf");
		break;
	case DOUBLE_CLICK_TEST_MAYBE:
		gdk_rgba_parse (&outer_color, "#729fcf");
		gdk_rgba_parse (&inner_color, "#ffffff");
		break;
	case DOUBLE_CLICK_TEST_OFF:
		gdk_rgba_parse (&outer_color, "#ffffff");
		gdk_rgba_parse (&inner_color, "#ffffff");
		break;
	}

	/* Draw shadow. */
	cairo_rectangle (cr, center_x - size / 2,  center_y - size / 2, size, size);
	pattern = cairo_pattern_create_radial (center_x, center_y, 0, center_x, center_y, size);
	cairo_pattern_add_color_stop_rgba (pattern, 0.5 - SHADOW_SIZE / size, 0, 0, 0, SHADOW_OPACITY);
	cairo_pattern_add_color_stop_rgba (pattern, 0.5, 0, 0, 0, 0);
	cairo_set_source (cr, pattern);
	cairo_fill (cr);

	/* Draw outer circle. */
	cairo_set_line_width (cr, OUTER_CIRCLE_SIZE);
	cairo_arc (cr, center_x, center_y + SHADOW_SHIFT_Y,
		   INNER_CIRCLE_SIZE + ANNULUS_SIZE + OUTER_CIRCLE_SIZE / 2,
		   0, 2 * G_PI);
	gdk_cairo_set_source_rgba (cr, &outer_color);
	cairo_stroke (cr);

	/* Draw inner circle. */
	cairo_set_line_width (cr, 0);
	cairo_arc (cr, center_x, center_y + SHADOW_SHIFT_Y,
		   INNER_CIRCLE_SIZE,
		   0, 2 * G_PI);
	gdk_cairo_set_source_rgba (cr, &inner_color);
	cairo_fill (cr);

	return FALSE;
}

/* Set up the property editors in the dialog. */
static void
setup_dialog (GtkBuilder *dialog)
{
	GtkAdjustment *adjustment;
	GdkRGBA color;

	g_signal_connect (WID ("button_drawing_area"), "button_press_event",
			  G_CALLBACK (button_drawing_area_button_press_event),
			  dialog);
	g_signal_connect (WID ("button_drawing_area"), "draw",
			  G_CALLBACK (button_drawing_area_draw_event),
			  dialog);

	adjustment = GTK_ADJUSTMENT (WID ("scrolled_window_adjustment"));
	gtk_adjustment_set_value (adjustment,
				  gtk_adjustment_get_upper (adjustment));

	gdk_rgba_parse (&color, "#565854");
	gtk_widget_override_background_color (WID ("viewport"), GTK_STATE_FLAG_NORMAL, &color);
	gtk_widget_override_background_color (WID ("button_drawing_area"), GTK_STATE_FLAG_NORMAL, &color);
}

GtkWidget *
gnome_mouse_test_init (GtkBuilder *dialog)
{
	mouse_settings = g_settings_new ("org.gnome.settings-daemon.peripherals.mouse");

	setup_dialog (dialog);

	return WID ("mouse_test_window");
}

void
gnome_mouse_test_dispose (GtkWidget *widget)
{
	if (mouse_settings != NULL) {
		g_object_unref (mouse_settings);
		mouse_settings = NULL;
	}

	if (information_label_timeout_id != 0) {
		g_source_remove (information_label_timeout_id);
		information_label_timeout_id = 0;
	}

	if (scroll_image_timeout_id != 0) {
		g_source_remove (scroll_image_timeout_id);
		scroll_image_timeout_id = 0;
	}

	if (button_drawing_area_timeout_id != 0) {
		g_source_remove  (button_drawing_area_timeout_id);
		button_drawing_area_timeout_id = 0;
	}
}

