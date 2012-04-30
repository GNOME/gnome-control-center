#include <gtk/gtk.h>
#include <clutter-gtk/clutter-gtk.h>
#include "cc-notebook.h"

int main (int argc, char **argv)
{
	GtkWidget *window, *notebook;

	if (gtk_clutter_init (&argc, &argv) == FALSE) {
		g_warning ("ARGGGHH");
		return 1;
	}

	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	gtk_window_resize (GTK_WINDOW (window), 500, 500);

	notebook = cc_notebook_new ();
	gtk_container_add (GTK_CONTAINER (window), notebook);

	gtk_container_add (GTK_CONTAINER (notebook), gtk_label_new ("fhdkhfjksdhkfjsdhk"));
	gtk_container_add (GTK_CONTAINER (notebook), gtk_label_new ("fhdkhfjksdhkfjsdhk2"));

	gtk_widget_show_all (window);
	gtk_main ();

	return 0;
}
