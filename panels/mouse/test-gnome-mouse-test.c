#include <config.h>
#include <gtk/gtk.h>

#include "gnome-mouse-test.h"

static gboolean
delete_event_cb (GtkWidget *widget, GdkEvent *event, gpointer user_data)
{
  GtkWidget *window = user_data;

  gnome_mouse_test_dispose (window);

  gtk_main_quit ();

  return FALSE;
}

int main (int argc, char **argv)
{
  GtkBuilder *builder;
  GtkWidget *window;
  GError *error = NULL;

  gtk_init (&argc, &argv);

  builder = gtk_builder_new ();

  gtk_builder_add_from_file (builder, "gnome-mouse-test.ui", &error);
  if (error != NULL)
    {
      g_warning ("Error loading UI file: %s", error->message);
      return 1;
    }

  window = gnome_mouse_test_init (builder);
  gtk_widget_show_all (window);

  g_signal_connect (G_OBJECT (window), "delete-event",
  G_CALLBACK (delete_event_cb), window);

  gtk_main ();

  return 0;
}
