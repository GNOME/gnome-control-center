/* -*- mode: c; style: linux -*- */

/* mouse-properties-capplet.c
 * Copyright (C) 2001 Red Hat, Inc.
 * Copyright (C) 2001 Ximian, Inc.
 *
 * Written by: Jonathon Blandford <jrb@redhat.com>,
 *             Bradford Hovinen <hovinen@ximian.com>,
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

#include <gnome.h>
#include <gconf/gconf-client.h>
#include <glade/glade.h>
#include <math.h>

#include "capplet-util.h"
#include "gconf-property-editor.h"

enum
{
	DOUBLE_CLICK_TEST_OFF,
	DOUBLE_CLICK_TEST_MAYBE,
	DOUBLE_CLICK_TEST_ON,
};

/* We use this in at least half a dozen places, so it makes sense just to
 * define the macro */

#define DOUBLE_CLICK_KEY "/desktop/gnome/peripherals/mouse/double_click"

/* Write-once data; global for convenience. Set only by load_pixbufs */

GdkPixbuf *left_handed_pixbuf;
GdkPixbuf *right_handed_pixbuf;
GdkPixbuf *double_click_on_pixbuf;
GdkPixbuf *double_click_maybe_pixbuf;
GdkPixbuf *double_click_off_pixbuf;

/* State in testing the double-click speed. Global for a great deal of
 * convenience
 */

gint double_click_state = DOUBLE_CLICK_TEST_OFF;

/* normalilzation routines */
/* All of our scales but double_click are on the range 1->10 as a result, we
 * have a few routines to convert from whatever the gconf key is to our range.
 */
static gfloat
double_click_from_gconf (gfloat double_click)
{
	return CLAMP (floor ((double_click + 50) / 100) * 100, 0, 1000) / 1000.0;
}

static gfloat
double_click_to_gconf (gfloat double_click)
{
	return double_click * 1000.0;
}

static gfloat
motion_acceleration_from_gconf (gfloat motion_acceleration)
{
	motion_acceleration = CLAMP (motion_acceleration, 0.2, 6.0);

	if (motion_acceleration >= 1)
		return motion_acceleration + 4;
	else
		return motion_acceleration * 5;
}

static gfloat
motion_acceleration_to_gconf (gfloat motion_acceleration)
{
	motion_acceleration = CLAMP (motion_acceleration, 1.0, 10.0);

	if (motion_acceleration < 5)
		return motion_acceleration / 5.0;
	else
		return motion_acceleration - 4;
}

static gfloat
threshold_from_gconf (gfloat drag_threshold)
{
	return CLAMP (drag_threshold, 1, 10);
}

/* Retrieve legacy settings */

static void
get_legacy_settings (void) 
{
}

/* Double Click handling */

struct test_data_t 
{
	gint *timeout_id;
	GtkWidget *darea;
};

/* Timeout for the double click test */

static gboolean
test_maybe_timeout (struct test_data_t *data)
{
	double_click_state = DOUBLE_CLICK_TEST_OFF;
	gtk_widget_queue_draw (data->darea);

	*data->timeout_id = 0;

	return FALSE;
}

/* Callback issued when the user clicks the double click testing area. */

static gint
drawing_area_button_press_event (GtkWidget      *widget,
				 GdkEventButton *event,
				 GConfChangeSet *changeset)
{
	gint                       double_click_time;
	GConfValue                *value;

	static struct test_data_t  data;
	static guint               test_on_timeout_id     = 0;
	static guint               test_maybe_timeout_id  = 0;
	static guint32             double_click_timestamp = 0;

	if (event->type != GDK_BUTTON_PRESS)
		return FALSE;

	if (!gconf_change_set_check_value (changeset, DOUBLE_CLICK_KEY, &value)) 
		double_click_time = gconf_client_get_int (gconf_client_get_default (), DOUBLE_CLICK_KEY, NULL);
	else
		double_click_time = gconf_value_get_int (value);

	if (test_maybe_timeout_id != 0)
		gtk_timeout_remove  (test_maybe_timeout_id);
	if (test_on_timeout_id != 0)
		gtk_timeout_remove (test_on_timeout_id);

	switch (double_click_state) {
	case DOUBLE_CLICK_TEST_OFF:
		double_click_state = DOUBLE_CLICK_TEST_MAYBE;
		data.darea = widget;
		data.timeout_id = &test_maybe_timeout_id;
		test_maybe_timeout_id = gtk_timeout_add (double_click_time, (GtkFunction) test_maybe_timeout, &data);
		break;
	case DOUBLE_CLICK_TEST_MAYBE:
		if (event->time - double_click_timestamp < double_click_time) {
			double_click_state = DOUBLE_CLICK_TEST_ON;
			data.darea = widget;
			data.timeout_id = &test_on_timeout_id;
			test_on_timeout_id = gtk_timeout_add (2500, (GtkFunction) test_maybe_timeout, &data);
		}
		break;
	case DOUBLE_CLICK_TEST_ON:
		double_click_state = DOUBLE_CLICK_TEST_OFF;
		break;
	}

	double_click_timestamp = event->time;
	gtk_widget_queue_draw (widget);

	return TRUE;
}

/* Callback to display the appropriate image on the double click testing area. */

static gint
drawing_area_expose_event (GtkWidget      *widget,
			   GdkEventExpose *event,
			   GConfChangeSet *changeset)
{
	static gboolean  first_time = TRUE;

	GdkPixbuf       *pixbuf;

	if (first_time) {
		gdk_window_set_events (widget->window, gdk_window_get_events (widget->window) | GDK_BUTTON_PRESS_MASK);
		g_signal_connect (G_OBJECT (widget), "button_press_event", (GCallback) drawing_area_button_press_event, changeset);
		first_time = FALSE;
	}

	gdk_draw_rectangle (widget->window, widget->style->white_gc,
			    TRUE, 0, 0,
			    widget->allocation.width,
			    widget->allocation.height);

	switch (double_click_state) {
	case DOUBLE_CLICK_TEST_ON:
		pixbuf = double_click_on_pixbuf;
		break;
	case DOUBLE_CLICK_TEST_MAYBE:
		pixbuf = double_click_maybe_pixbuf;
		break;
	case DOUBLE_CLICK_TEST_OFF:
		pixbuf = double_click_off_pixbuf;
		break;
	}

	gdk_pixbuf_render_to_drawable_alpha
		(pixbuf, widget->window,
		 0, 0,
		 (widget->allocation.width - gdk_pixbuf_get_width (pixbuf)) / 2,
		 (widget->allocation.height - gdk_pixbuf_get_height (pixbuf)) / 2,
		 -1, -1, GDK_PIXBUF_ALPHA_FULL, 0, GDK_RGB_DITHER_NORMAL, 0, 0);
		
	return TRUE;
}

/* Callback issued when the user switches between left- and right-handed button
 * mappings. Updates the appropriate image on the dialog.
 */

static void
left_handed_toggle_cb (GConfPropertyEditor *peditor, const gchar *key, const GConfValue *value, GtkWidget *image)
{
	if (gconf_value_get_bool (value))
		g_object_set (G_OBJECT (image),
			      "pixbuf", left_handed_pixbuf,
			      NULL);
	else
		g_object_set (G_OBJECT (image),
			      "pixbuf", right_handed_pixbuf,
			      NULL);
}

/* Load the pixbufs we are going to use */

static void
load_pixbufs (void) 
{
	static gboolean called = FALSE;

	if (called) return;

	left_handed_pixbuf        = gdk_pixbuf_new_from_file ("mouse-left.png",         NULL);
	right_handed_pixbuf       = gdk_pixbuf_new_from_file ("mouse-right.png",        NULL);
	double_click_on_pixbuf    = gdk_pixbuf_new_from_file ("double-click-on.png",    NULL);
	double_click_maybe_pixbuf = gdk_pixbuf_new_from_file ("double-click-maybe.png", NULL);
	double_click_off_pixbuf   = gdk_pixbuf_new_from_file ("double-click-off.png",   NULL);

	/* Let's be paranoid here. I like this feature :-) */
	g_object_add_weak_pointer (G_OBJECT (left_handed_pixbuf),        (gpointer *) &left_handed_pixbuf);
	g_object_add_weak_pointer (G_OBJECT (right_handed_pixbuf),       (gpointer *) &right_handed_pixbuf);
	g_object_add_weak_pointer (G_OBJECT (double_click_on_pixbuf),    (gpointer *) &double_click_on_pixbuf);
	g_object_add_weak_pointer (G_OBJECT (double_click_maybe_pixbuf), (gpointer *) &double_click_maybe_pixbuf);
	g_object_add_weak_pointer (G_OBJECT (double_click_off_pixbuf),   (gpointer *) &double_click_off_pixbuf);

	called = TRUE;
}

/* Set up the property editors in the dialog. */

static void
setup_dialog (GladeXML *dialog, GConfChangeSet *changeset)
{
	GtkWidget   *widget;
	GObject     *peditor;
	GConfValue  *value;

	int          double_click;
	int          threshold;
	gfloat       acceleration;

	/* Buttons page
	 */
	/* Left-handed toggle */
	peditor = gconf_peditor_new_boolean
		(changeset, "/desktop/gnome/peripherals/mouse/left_handed", WID ("left_handed_toggle"));
	g_signal_connect (peditor, "value-changed", (GCallback) left_handed_toggle_cb, WID ("orientation_image"));

	/* Make sure the image gets initialized correctly */
	value = gconf_client_get (gconf_client_get_default (), "/desktop/gnome/peripherals/mouse/left_handed", NULL);
	left_handed_toggle_cb (GCONF_PROPERTY_EDITOR (peditor), NULL, value, WID ("orientation_image"));
	gconf_value_free (value);

	/* Double-click time */
	g_signal_connect (WID ("double_click_darea"), "expose_event", (GCallback) drawing_area_expose_event, changeset);

	gconf_peditor_new_int_range
		(changeset, DOUBLE_CLICK_KEY, WID ("delay_scale"),
		 double_click_from_gconf, double_click_to_gconf);
	gconf_peditor_new_float_range
		(changeset, "/desktop/gnome/peripherals/mouse/motion_acceleration", WID ("accel_scale"),
		 motion_acceleration_from_gconf, motion_acceleration_to_gconf);
	gconf_peditor_new_float_range
		(changeset, "/desktop/gnome/peripherals/mouse/motion_threshold",
		 WID ("sensitivity_scale"), NULL, NULL);
	gconf_peditor_new_int_range
		(changeset, "/desktop/gnome/peripherals/mouse/drag_threshold", WID ("drag_threshold_scale"),
		 threshold_from_gconf, NULL);
}

/* Construct the dialog */

static GladeXML *
create_dialog (void) 
{
	GtkWidget    *widget;
	GladeXML     *dialog;
	GtkSizeGroup *size_group;

	dialog = glade_xml_new (GNOMECC_GLADE_DIR "/mouse-properties.glade", "prefs_widget", NULL);
	widget = glade_xml_get_widget (dialog, "prefs_widget");

	size_group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);
	gtk_size_group_add_widget (size_group, WID ("acceleration_label"));
	gtk_size_group_add_widget (size_group, WID ("sensitivity_label"));
	gtk_size_group_add_widget (size_group, WID ("threshold_label"));

	size_group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);
	gtk_size_group_add_widget (size_group, WID ("high_label"));
	gtk_size_group_add_widget (size_group, WID ("fast_label"));
	gtk_size_group_add_widget (size_group, WID ("large_label"));

	size_group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);
	gtk_size_group_add_widget (size_group, WID ("low_label"));
	gtk_size_group_add_widget (size_group, WID ("slow_label"));
	gtk_size_group_add_widget (size_group, WID ("small_label"));

	/* Remove cursors page */
	widget = glade_xml_get_widget (dialog, "main_notebook");
	gtk_notebook_remove_page (GTK_NOTEBOOK (widget), 1);

	g_object_weak_ref (G_OBJECT (widget), (GWeakNotify) g_object_unref, dialog);

	return dialog;
}

/* Callback issued when a button is clicked on the dialog */

static void
dialog_button_clicked_cb (GnomeDialog *dialog, gint button_number, GConfChangeSet *changeset) 
{
	if (button_number == 0)
		gconf_client_commit_change_set (gconf_client_get_default (), changeset, TRUE, NULL);
	else if (button_number == 1)
		gnome_dialog_close (dialog);
}

int
main (int argc, char **argv)
{
	GConfClient    *client;
	GConfChangeSet *changeset;
	GladeXML       *dialog;
	GtkWidget      *dialog_win;

	static gboolean apply_only;
	static gboolean get_legacy;
	static struct poptOption cap_options[] = {
		{ "apply", '\0', POPT_ARG_NONE, &apply_only, 0,
		  N_("Just apply settings and quit (compatibility only; now handled by daemon)"), NULL },
		{ "init-session-settings", '\0', POPT_ARG_NONE, &apply_only, 0,
		  N_("Just apply settings and quit (compatibility only; now handled by daemon)"), NULL },
		{ "get-legacy", '\0', POPT_ARG_NONE, &get_legacy, 0,
		  N_("Retrieve and store legacy settings"), NULL },
		{ NULL, '\0', 0, NULL, 0, NULL, NULL }
	};

	bindtextdomain (PACKAGE, GNOMELOCALEDIR);
	bind_textdomain_codeset (PACKAGE, "UTF-8");
	textdomain (PACKAGE);

	gnome_program_init (argv[0], VERSION, LIBGNOMEUI_MODULE, argc, argv,
			    GNOME_PARAM_POPT_TABLE, cap_options,
			    NULL);

	client = gconf_client_get_default ();
	gconf_client_add_dir (client, "/desktop/gnome/peripherals/mouse", GCONF_CLIENT_PRELOAD_ONELEVEL, NULL);

	if (get_legacy) {
		get_legacy_settings ();
	} else {
		changeset = gconf_change_set_new ();
		dialog = create_dialog ();
		load_pixbufs ();
		setup_dialog (dialog, changeset);

		dialog_win = gnome_dialog_new (_("Mouse properties"), GTK_STOCK_APPLY, GTK_STOCK_CLOSE, NULL);
		g_signal_connect (G_OBJECT (dialog_win), "clicked", (GCallback) dialog_button_clicked_cb, changeset);
		g_object_weak_ref (G_OBJECT (dialog_win), (GWeakNotify) gtk_main_quit, NULL);
		gtk_box_pack_start (GTK_BOX (GNOME_DIALOG (dialog_win)->vbox), WID ("prefs_widget"), TRUE, TRUE, GNOME_PAD_SMALL);
		gtk_widget_show_all (dialog_win);

		gtk_main ();
		gconf_change_set_unref (changeset);
	}

	return 0;
}
