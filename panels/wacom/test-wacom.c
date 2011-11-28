#include "cc-wacom-page.h"
#include "gsd-wacom-device.h"

static void
add_page (GList *devices,
	  GtkWidget *notebook)
{
	GtkWidget *widget;
	GsdWacomDevice *stylus, *eraser;
	GList *l;

	stylus = eraser = NULL;
	for (l = devices; l ; l = l->next) {
		switch (gsd_wacom_device_get_device_type (l->data)) {
		case WACOM_TYPE_ERASER:
			eraser = l->data;
			break;
		case WACOM_TYPE_STYLUS:
			stylus = l->data;
			break;
		default:
			/* Nothing */
			;
		}
	}
	g_list_free (devices);

	widget = cc_wacom_page_new (stylus, eraser);
	cc_wacom_page_set_navigation (CC_WACOM_PAGE (widget), GTK_NOTEBOOK (notebook), FALSE);
	gtk_notebook_append_page (GTK_NOTEBOOK (notebook), widget, NULL);
}

int main (int argc, char **argv)
{
	GtkWidget *window, *notebook;
	GList *devices;

	gtk_init (&argc, &argv);

	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	notebook = gtk_notebook_new ();
	gtk_notebook_set_show_tabs (GTK_NOTEBOOK (notebook), FALSE);
	gtk_widget_set_vexpand (notebook, TRUE);
	gtk_container_set_border_width (GTK_CONTAINER (notebook), 24);
	gtk_container_add (GTK_CONTAINER (window), notebook);

	devices = gsd_wacom_device_create_fake_cintiq ();
	add_page (devices, notebook);

	devices = gsd_wacom_device_create_fake_bt ();
	add_page (devices, notebook);

	devices = gsd_wacom_device_create_fake_x201 ();
	add_page (devices, notebook);

	gtk_widget_show_all (window);

	gtk_main ();

	return 0;
}
