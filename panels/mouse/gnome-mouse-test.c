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

enum
{
	DOUBLE_CLICK_TEST_OFF,
	DOUBLE_CLICK_TEST_MAYBE,
	DOUBLE_CLICK_TEST_ON
};

/* State in testing the double-click speed. Global for a great deal of
 * convenience
 */
static gint double_click_state = DOUBLE_CLICK_TEST_OFF;
static GSettings *mouse_settings = NULL;

/* Double Click handling */

struct test_data_t
{
	gint *timeout_id;
	GtkWidget *image;
};

/* Timeout for the double click test */

static gboolean
test_maybe_timeout (struct test_data_t *data)
{
	double_click_state = DOUBLE_CLICK_TEST_OFF;

	gtk_image_set_from_icon_name (GTK_IMAGE (data->image), "face-plain", GTK_ICON_SIZE_DIALOG);

	*data->timeout_id = 0;

	return FALSE;
}

/* Callback issued when the user clicks the double click testing area. */

static gboolean
event_box_button_press_event (GtkWidget   *widget,
			      GdkEventButton *event,
			      GtkBuilder   *dialog)
{
	gint                       double_click_time;
	static struct test_data_t  data;
	static gint                test_on_timeout_id     = 0;
	static gint                test_maybe_timeout_id  = 0;
	static guint32             double_click_timestamp = 0;
	GtkWidget                 *image;

	if (event->type != GDK_BUTTON_PRESS)
		return FALSE;

	image = g_object_get_data (G_OBJECT (widget), "image");

	double_click_time = g_settings_get_int (mouse_settings, "double-click");

	if (test_maybe_timeout_id != 0)
		g_source_remove  (test_maybe_timeout_id);
	if (test_on_timeout_id != 0)
		g_source_remove (test_on_timeout_id);

	switch (double_click_state) {
	case DOUBLE_CLICK_TEST_OFF:
		double_click_state = DOUBLE_CLICK_TEST_MAYBE;
		data.image = image;
		data.timeout_id = &test_maybe_timeout_id;
		test_maybe_timeout_id = g_timeout_add (double_click_time, (GSourceFunc) test_maybe_timeout, &data);
		break;
	case DOUBLE_CLICK_TEST_MAYBE:
		if (event->time - double_click_timestamp < double_click_time) {
			double_click_state = DOUBLE_CLICK_TEST_ON;
			data.image = image;
			data.timeout_id = &test_on_timeout_id;
			test_on_timeout_id = g_timeout_add (2500, (GSourceFunc) test_maybe_timeout, &data);
		}
		break;
	case DOUBLE_CLICK_TEST_ON:
		double_click_state = DOUBLE_CLICK_TEST_OFF;
		break;
	}

	double_click_timestamp = event->time;

	switch (double_click_state) {
	case DOUBLE_CLICK_TEST_ON:
		gtk_image_set_from_icon_name (GTK_IMAGE (image), "face-laugh", GTK_ICON_SIZE_INVALID);
		break;
	case DOUBLE_CLICK_TEST_MAYBE:
		gtk_image_set_from_icon_name (GTK_IMAGE (image), "face-smile", GTK_ICON_SIZE_INVALID);
		break;
	case DOUBLE_CLICK_TEST_OFF:
		gtk_image_set_from_icon_name (GTK_IMAGE (image), "face-plain", GTK_ICON_SIZE_INVALID);
		break;
	}

	return TRUE;
}

/* Set up the property editors in the dialog. */
static void
setup_dialog (GtkBuilder *dialog)
{
	GtkImage *image;

	image = GTK_IMAGE (WID ("double_click_image"));

	gtk_image_set_from_icon_name (image, "face-plain", GTK_ICON_SIZE_INVALID);
	gtk_image_set_pixel_size (image, 256);

	g_object_set_data (G_OBJECT (WID ("double_click_eventbox")), "image", GTK_WIDGET (image));
	g_signal_connect (WID ("double_click_eventbox"), "button_press_event",
			  G_CALLBACK (event_box_button_press_event), dialog);
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
}

