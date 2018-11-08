#include <config.h>
#include <gtk/gtk.h>

#include "cc-mouse-resources.h"
#include "cc-mouse-test.h"

static gboolean
delete_event_cb (GtkWidget *widget, GdkEvent *event, gpointer user_data)
{
  gtk_main_quit ();

  return FALSE;
}

int main (int argc, char **argv)
{
  GtkWidget *widget;
  GtkWidget *window;

  gtk_init (&argc, &argv);

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_widget_show (window);
  widget = cc_mouse_test_new ();
  gtk_widget_show (widget);
  gtk_container_add (GTK_CONTAINER (window), widget);

  g_signal_connect (G_OBJECT (window), "delete-event", G_CALLBACK (delete_event_cb), NULL);

  gtk_main ();

  return 0;
}
