#include <gnome.h>
#include <gconf/gconf-client.h>
#include <glade/glade.h>
#include <math.h>

enum
{
	DOUBLE_CLICK_TEST_OFF,
	DOUBLE_CLICK_TEST_MAYBE,
	DOUBLE_CLICK_TEST_ON,
};

GladeXML *xml;
GdkPixbuf *left_handed_pixbuf;
GdkPixbuf *right_handed_pixbuf;
GdkPixbuf *double_click_on_pixbuf;
GdkPixbuf *double_click_maybe_pixbuf;
GdkPixbuf *double_click_off_pixbuf;
GConfClient *client;

gint double_click_state = DOUBLE_CLICK_TEST_OFF;
guint32 double_click_timestamp = 0;
guint test_maybe_timeout_id = 0;
guint test_on_timeout_id = 0;

#define LEFT_HANDED_KEY "/desktop/gnome/peripherals/mouse/left_handed"
#define DOUBLE_CLICK_KEY "/desktop/gnome/peripherals/mouse/double_click"
#define MOTION_ACCELERATION_KEY "/desktop/gnome/peripherals/mouse/motion_acceleration"
#define MOTION_THRESHOLD_KEY "/desktop/gnome/peripherals/mouse/motion_threshold"
#define DRAG_THRESHOLD_KEY "/desktop/gnome/peripherals/mouse/drag_threshold"



/* normalilzation routines */
/* All of our scales but double_click are on the range 1->10 as a result, we
 * have a few routines to convert from whatever the gconf key is to our range.
 */
static gint
double_click_from_gconf (gint double_click)
{
	/* watch me be lazy */
	if (double_click < 150)
		return 100;
	else if (double_click < 250)
		return 200;
	else if (double_click < 350)
		return 300;
	else if (double_click < 450)
		return 400;
	else if (double_click < 550)
		return 500;
	else if (double_click < 650)
		return 600;
	else if (double_click < 750)
		return 700;
	else if (double_click < 850)
		return 800;
	else if (double_click < 950)
		return 900;
	else
		return 1000;
}

static gfloat
motion_acceleration_from_gconf (gfloat motion_acceleration)
{
	motion_acceleration = CLAMP (motion_acceleration, 0.2, 6.0);
	if (motion_acceleration >=1)
		return motion_acceleration + 4;
	return motion_acceleration * 5;
}

static gfloat
motion_acceleration_to_gconf (gfloat motion_acceleration)
{
	motion_acceleration = CLAMP (motion_acceleration, 1.0, 10.0);
	if (motion_acceleration < 5)
		return motion_acceleration / 5.0;
	return motion_acceleration - 4;
}
static gfloat
threshold_from_gconf (gfloat drag_threshold)
{
	return CLAMP (drag_threshold, 1, 10);
}


/* Double Click handling */

static gboolean
test_maybe_timeout (gpointer data)
{
	GtkWidget *darea;
	darea = glade_xml_get_widget (xml, "double_click_darea");
	double_click_state = DOUBLE_CLICK_TEST_OFF;
	gtk_widget_queue_draw (darea);

	*((gint *)data) = 0;

	return FALSE;
}

static gint
drawing_area_button_press_event (GtkWidget      *widget,
				 GdkEventButton *event,
				 gpointer        data)
{
	GtkWidget *scale;
	GtkWidget *darea;
	gint double_click_time;

	if (event->type != GDK_BUTTON_PRESS)
		return FALSE;

	scale = glade_xml_get_widget (xml, "delay_scale");
	double_click_time = 1000 * gtk_range_get_value (GTK_RANGE (scale));
	darea = glade_xml_get_widget (xml, "double_click_darea");

	if (test_maybe_timeout_id != 0)
		gtk_timeout_remove  (test_maybe_timeout_id);
	if (test_on_timeout_id != 0)
		gtk_timeout_remove (test_on_timeout_id);

	switch (double_click_state) {
	case DOUBLE_CLICK_TEST_OFF:
		double_click_state = DOUBLE_CLICK_TEST_MAYBE;
		test_maybe_timeout_id = gtk_timeout_add (double_click_time, test_maybe_timeout, &test_maybe_timeout_id);
		break;
	case DOUBLE_CLICK_TEST_MAYBE:
		if (event->time - double_click_timestamp < double_click_time) {
			double_click_state = DOUBLE_CLICK_TEST_ON;
			test_on_timeout_id = gtk_timeout_add (2500, test_maybe_timeout, &test_on_timeout_id);
		}
		break;
	case DOUBLE_CLICK_TEST_ON:
		double_click_state = DOUBLE_CLICK_TEST_OFF;
		break;
	}

	double_click_timestamp = event->time;
	gtk_widget_queue_draw (darea);

	return TRUE;
}

static gint
drawing_area_expose_event (GtkWidget      *widget,
			   GdkEventExpose *event,
			   gpointer        data)
{
	static gboolean first_time = 1;
	GdkPixbuf *pixbuf;

	if (first_time) {
		gdk_window_set_events (widget->window, gdk_window_get_events (widget->window) | GDK_BUTTON_PRESS_MASK);
		g_signal_connect (widget, "button_press_event", (GCallback) drawing_area_button_press_event, NULL);
		first_time = 0;
	}

	gdk_draw_rectangle (widget->window,
			    widget->style->white_gc,
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

	gdk_pixbuf_render_to_drawable_alpha (pixbuf,
					     widget->window,
					     0, 0,
					     (widget->allocation.width - gdk_pixbuf_get_width (pixbuf))/2,
					     (widget->allocation.height - gdk_pixbuf_get_height (pixbuf))/2,
					     -1, -1,
					     GDK_PIXBUF_ALPHA_FULL,
					     0,
					     GDK_RGB_DITHER_NORMAL,
					     0, 0);
		
	return TRUE;
}



/* capplet->gconf settings */

static void
left_handed_toggle_callback (GtkWidget *toggle, gpointer data)
{
	GtkWidget *image;
	image = glade_xml_get_widget (xml, "orientation_image");
	if (GTK_TOGGLE_BUTTON (toggle)->active)
		g_object_set (G_OBJECT (image),
			      "pixbuf", left_handed_pixbuf,
			      NULL);
	else
		g_object_set (G_OBJECT (image),
			      "pixbuf", right_handed_pixbuf,
			      NULL);
	gconf_client_set_bool (client, LEFT_HANDED_KEY,
			       GTK_TOGGLE_BUTTON (toggle)->active,
			       NULL);
}

static void
double_click_callback (GtkAdjustment *adjustment, gpointer data)
{
	gint double_click;

	double_click = gtk_adjustment_get_value (adjustment) * 1000;
	/* we normalize this to avoid loops */
	if (double_click != double_click_from_gconf (double_click)) {
		gtk_adjustment_set_value (adjustment, (double_click_from_gconf (double_click))/1000.0);
		return;
	}

	gconf_client_set_int (client, DOUBLE_CLICK_KEY,
			      double_click,
			      NULL);
}

static void
threshold_callback (GtkAdjustment *adjustment, gpointer key)
{
	gint threshold;

	threshold = (gint) rint (gtk_adjustment_get_value (adjustment));
	
	gconf_client_set_int (client, (char *) key,
			      threshold,
			      NULL);
}

static void
acceleration_callback (GtkAdjustment *adjustment, gpointer data)
{
	gfloat acceleration;

	acceleration = gtk_adjustment_get_value (adjustment);
	gconf_client_set_float (client, MOTION_ACCELERATION_KEY,
				motion_acceleration_to_gconf (acceleration),
				NULL);
}

/* gconf->capplet */
static void
gconf_changed_callback (GConfClient *client,
			guint        cnxn_id,
			GConfEntry  *entry,
			gpointer     user_data)
{
	GtkWidget *widget;
	const gchar *key = gconf_entry_get_key (entry);

	if (! strcmp (key, LEFT_HANDED_KEY)) {
		gboolean left_handed;

		left_handed = gconf_value_get_bool (gconf_entry_get_value (entry));
		widget = glade_xml_get_widget (xml, "left_handed_toggle");
		if (left_handed != GTK_TOGGLE_BUTTON (widget)->active)
			gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), left_handed);
	} else if (! strcmp (key, DOUBLE_CLICK_KEY)) {
		int double_click;

		double_click = gconf_value_get_int (gconf_entry_get_value (entry));
		double_click = double_click_from_gconf (double_click);
		widget = glade_xml_get_widget (xml, "delay_scale");
		if (double_click != (gint) 1000*gtk_range_get_value (GTK_RANGE (widget)))
			gtk_range_set_value (GTK_RANGE (widget), (gfloat)double_click/1000.0);
	} else if (! strcmp (key, MOTION_ACCELERATION_KEY)) {
		gfloat acceleration;

		acceleration = gconf_value_get_float (gconf_entry_get_value (entry));
		acceleration = motion_acceleration_from_gconf (acceleration);
		widget = glade_xml_get_widget (xml, "accel_scale");
		if (ABS (acceleration - gtk_range_get_value (GTK_RANGE (widget))) > 0.001)
			gtk_range_set_value (GTK_RANGE (widget), acceleration);
	} else if (! strcmp (key, MOTION_THRESHOLD_KEY)) {
		int threshold;

		threshold = gconf_value_get_int (gconf_entry_get_value (entry));
		threshold = threshold_from_gconf (threshold);
		widget = glade_xml_get_widget (xml, "sensitivity_scale");
		if (ABS (threshold - gtk_range_get_value (GTK_RANGE (widget))) > 0.001)
			gtk_range_set_value (GTK_RANGE (widget), (gfloat)threshold);
	} else if (! strcmp (key, DRAG_THRESHOLD_KEY)) {
		int threshold;

		threshold = gconf_value_get_int (gconf_entry_get_value (entry));
		threshold = threshold_from_gconf (threshold);
		widget = glade_xml_get_widget (xml, "drag_threshold_scale");
		if (ABS (threshold - gtk_range_get_value (GTK_RANGE (widget))) > 0.001)
			gtk_range_set_value (GTK_RANGE (widget), (gfloat)threshold);
	}
}

static void
setup_dialog (void)
{
	GtkSizeGroup *size_group;
	GtkWidget *widget;
	int double_click;
	int threshold;
	gfloat acceleration;

	client = gconf_client_get_default ();
	gconf_client_add_dir (client, "/desktop/gnome/peripherals/mouse", GCONF_CLIENT_PRELOAD_ONELEVEL, NULL);
	gconf_client_notify_add (client, "/desktop/gnome/peripherals/mouse",
				 gconf_changed_callback,
				 NULL, NULL, NULL);

	/* Buttons page
	 */
	/* Left-handed toggle */
	left_handed_pixbuf = gdk_pixbuf_new_from_file ("mouse-left.png", NULL);
	right_handed_pixbuf = gdk_pixbuf_new_from_file ("mouse-right.png", NULL);
	widget = glade_xml_get_widget (xml, "left_handed_toggle");
	if (gconf_client_get_bool (client, LEFT_HANDED_KEY, NULL))
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), TRUE);
	else
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), FALSE);
	g_signal_connect (widget, "toggled", (GCallback) left_handed_toggle_callback, NULL);

	widget = glade_xml_get_widget (xml, "orientation_image");
	if (gconf_client_get_bool (client, LEFT_HANDED_KEY, NULL))
		g_object_set (G_OBJECT (widget),
			      "pixbuf", left_handed_pixbuf,
			      NULL);
	else
		g_object_set (G_OBJECT (widget),
			      "pixbuf", right_handed_pixbuf,
			      NULL);

	/* Double-click time */
	double_click_on_pixbuf = gdk_pixbuf_new_from_file ("double-click-on.png", NULL);
	double_click_maybe_pixbuf = gdk_pixbuf_new_from_file ("double-click-maybe.png", NULL);
	double_click_off_pixbuf = gdk_pixbuf_new_from_file ("double-click-off.png", NULL);
	widget = glade_xml_get_widget (xml, "double_click_darea");
	g_signal_connect (widget, "expose_event", (GCallback) drawing_area_expose_event, NULL);

	double_click = gconf_client_get_int (client, DOUBLE_CLICK_KEY, NULL);
	double_click = double_click_from_gconf (double_click);
	widget = glade_xml_get_widget (xml, "delay_scale");
	gtk_range_set_value (GTK_RANGE (widget), (gfloat)double_click/1000.0);
	g_signal_connect (G_OBJECT (gtk_range_get_adjustment (GTK_RANGE (widget))),
			  "value_changed",
			  (GCallback) double_click_callback,
			  NULL);

	/* Cursors page */
	widget = glade_xml_get_widget (xml, "main_notebook");
	gtk_notebook_remove_page (GTK_NOTEBOOK (widget), 1);

	/* Motion page */
	size_group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);
	gtk_size_group_add_widget (size_group,
				   glade_xml_get_widget (xml, "acceleration_label"));
	gtk_size_group_add_widget (size_group,
				   glade_xml_get_widget (xml, "sensitivity_label"));
	gtk_size_group_add_widget (size_group,
				   glade_xml_get_widget (xml, "threshold_label"));

	size_group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);
	gtk_size_group_add_widget (size_group,
				   glade_xml_get_widget (xml, "high_label"));
	gtk_size_group_add_widget (size_group,
				   glade_xml_get_widget (xml, "fast_label"));
	gtk_size_group_add_widget (size_group,
				   glade_xml_get_widget (xml, "large_label"));

	size_group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);
	gtk_size_group_add_widget (size_group,
				   glade_xml_get_widget (xml, "low_label"));
	gtk_size_group_add_widget (size_group,
				   glade_xml_get_widget (xml, "slow_label"));
	gtk_size_group_add_widget (size_group,
				   glade_xml_get_widget (xml, "small_label"));

	widget = glade_xml_get_widget (xml, "accel_scale");
	acceleration = gconf_client_get_float (client, MOTION_ACCELERATION_KEY, NULL);
	acceleration = motion_acceleration_from_gconf (acceleration);
	gtk_range_set_value (GTK_RANGE (widget), acceleration);
	g_signal_connect (G_OBJECT (gtk_range_get_adjustment (GTK_RANGE (widget))),
			  "value_changed",
			  (GCallback) acceleration_callback,
			  NULL);

	widget = glade_xml_get_widget (xml, "sensitivity_scale");
	threshold = gconf_client_get_int (client, MOTION_THRESHOLD_KEY, NULL);
	threshold = threshold_from_gconf (threshold);
	gtk_range_set_value (GTK_RANGE (widget), threshold);
	g_signal_connect (G_OBJECT (gtk_range_get_adjustment (GTK_RANGE (widget))),
			  "value_changed",
			  (GCallback) threshold_callback,
			  MOTION_THRESHOLD_KEY);

	widget = glade_xml_get_widget (xml, "drag_threshold_scale");
	threshold = gconf_client_get_int (client, DRAG_THRESHOLD_KEY, NULL);
	threshold = threshold_from_gconf (threshold);
	gtk_range_set_value (GTK_RANGE (widget), threshold);
	g_signal_connect (G_OBJECT (gtk_range_get_adjustment (GTK_RANGE (widget))),
			  "value_changed",
			  (GCallback) threshold_callback,
			  DRAG_THRESHOLD_KEY);

	/* main dialog */
	widget = glade_xml_get_widget (xml, "mouse_properties_dialog");
	g_signal_connect (G_OBJECT (widget),
			  "destroy",
			  gtk_main_quit, NULL);
	widget = glade_xml_get_widget (xml, "close_button");
	g_signal_connect (G_OBJECT (widget),
			  "clicked",
			  gtk_main_quit, NULL);
}

int
main (int argc, char *argv[])
{
	gnome_program_init ("mouse-properties",
			    "0.1",
			    gnome_gtk_module_info_get (),
			    argc, argv,
			    NULL);

	xml = glade_xml_new ("gnome-mouse-properties.glade", NULL, NULL);
	setup_dialog ();
	gtk_widget_show_all (glade_xml_get_widget (xml, "mouse_properties_dialog"));

	gtk_main ();

	g_object_unref (G_OBJECT (client));
	return 0;
}
