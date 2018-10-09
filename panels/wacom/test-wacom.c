
#include "config.h"

#include <glib/gi18n.h>

#include "cc-wacom-page.h"

#define FIXED_WIDTH 675

void
cc_wacom_panel_switch_to_panel (CcWacomPanel *self, const char *panel)
{
	g_message ("Should launch %s preferences here", panel);
}

GDBusProxy *
cc_wacom_panel_get_gsd_wacom_bus_proxy (CcWacomPanel *self)
{
	g_message ("Should get the g-s-d wacom dbus proxy here");

	return NULL;
}

static void
add_page (GList *devices,
	  GtkWidget *notebook)
{
	GtkWidget *widget;
	CcWacomDevice *stylus, *eraser, *pad;
	GList *l;

	if (devices == NULL)
		return;

	stylus = eraser = pad = NULL;
	for (l = devices; l ; l = l->next) {
		stylus = l->data;
	}
	g_list_free (devices);

	widget = cc_wacom_page_new (NULL, stylus, pad);
	cc_wacom_page_set_navigation (CC_WACOM_PAGE (widget), GTK_NOTEBOOK (notebook), FALSE);
	gtk_notebook_append_page (GTK_NOTEBOOK (notebook), widget, NULL);
	gtk_widget_show (widget);
}

static gboolean
delete_event_cb (GtkWidget *widget,
		 GdkEvent  *event,
		 gpointer   user_data)
{
	gtk_main_quit ();

	return FALSE;
}

static GList *
create_fake_cintiq (void)
{
	CcWacomDevice *device;
	GList *devices;

	device = cc_wacom_device_new_fake ("Wacom Cintiq 21UX2");
	devices = g_list_prepend (NULL, device);

	return devices;
}

static GList *
create_fake_bt (void)
{
	CcWacomDevice *device;
	GList *devices;

	device = cc_wacom_device_new_fake ("Wacom Graphire Wireless");
	devices = g_list_prepend (NULL, device);

	return devices;
}

static GList *
create_fake_x201 (void)
{
	CcWacomDevice *device;
	GList *devices;

	device = cc_wacom_device_new_fake ("Wacom Serial Tablet WACf004");
	devices = g_list_prepend (NULL, device);

	return devices;
}

static GList *
create_fake_intuos4 (void)
{
	CcWacomDevice *device;
	GList *devices;

	device = cc_wacom_device_new_fake ("Wacom Intuos4 6x9");
	devices = g_list_prepend (NULL, device);

	return devices;
}

static GList *
create_fake_h610pro (void)
{
	CcWacomDevice *device;
	GList *devices;

	device = cc_wacom_device_new_fake ("Huion H610 Pro");
	devices = g_list_prepend (NULL, device);

	return devices;
}

int main (int argc, char **argv)
{
	GtkWidget *window, *notebook;
	GList *devices;

	bindtextdomain (GETTEXT_PACKAGE, GNOMELOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	gtk_init (&argc, &argv);

	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	gtk_window_set_resizable (GTK_WINDOW (window), FALSE);
	gtk_window_set_default_size (GTK_WINDOW (window), FIXED_WIDTH, -1);
	g_signal_connect (G_OBJECT (window), "delete-event",
			  G_CALLBACK (delete_event_cb), NULL);
	notebook = gtk_notebook_new ();
	gtk_notebook_set_show_tabs (GTK_NOTEBOOK (notebook), FALSE);
	gtk_notebook_set_show_border (GTK_NOTEBOOK (notebook), FALSE);
	gtk_widget_set_vexpand (notebook, TRUE);
	gtk_container_set_border_width (GTK_CONTAINER (notebook), 24);
	gtk_container_add (GTK_CONTAINER (window), notebook);
	gtk_widget_show (notebook);

	devices = create_fake_intuos4 ();
	add_page (devices, notebook);

	devices = create_fake_cintiq ();
	add_page (devices, notebook);

	devices = create_fake_bt ();
	add_page (devices, notebook);

	devices = create_fake_x201 ();
	add_page (devices, notebook);

	devices = create_fake_h610pro ();
	add_page (devices, notebook);

	gtk_widget_show (window);

	gtk_main ();

	return 0;
}
