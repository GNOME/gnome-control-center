#include <stdlib.h>
#include "cc-notebook.h"

enum {
  PAGE_1,
  PAGE_2,
  PAGE_3,
  PAGE_4,
  PAGE_5,

  N_PAGES
};

static int pages[N_PAGES] = { 0, };

static GtkWidget *
create_page_contents (const char *text)
{
  GtkWidget *vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);

  GtkWidget *hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);

  GtkWidget *back_button = gtk_button_new_with_label ("←");
  gtk_widget_set_halign (back_button, GTK_ALIGN_START);
  gtk_box_pack_start (GTK_BOX (hbox), back_button, FALSE, FALSE, 0);

  GtkWidget *fwd_button = gtk_button_new_with_label ("→");
  gtk_widget_set_halign (fwd_button, GTK_ALIGN_END);
  gtk_box_pack_end (GTK_BOX (hbox), fwd_button, FALSE, FALSE, 0);

  GtkWidget *label = gtk_label_new (text);
  gtk_widget_set_name (label, text);

  gtk_box_pack_end (GTK_BOX (vbox), label, TRUE, TRUE, 0);

  gtk_widget_show_all (vbox);

  return vbox;
}

static void
on_page_change (CcNotebook *notebook)
{
  g_print (G_STRLOC ": Currently selected page: %s\n",
           gtk_widget_get_name (cc_notebook_get_selected_page (notebook)));
}

int
main (int argc, char *argv[])
{
  if (gtk_clutter_init (&argc, &argv) != CLUTTER_INIT_SUCCESS)
    return EXIT_FAILURE;

  GtkWidget *window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  g_signal_connect (window, "destroy", G_CALLBACK (gtk_main_quit), NULL);
  gtk_widget_show (window);

  GtkWidget *notebook = cc_notebook_new ();
  gtk_container_add (GTK_CONTAINER (window), notebook);
  g_signal_connect (notebook, "notify::current-page", G_CALLBACK (on_page_change), NULL);

  pages[PAGE_1] = cc_notebook_add_page ((CcNotebook *) notebook, create_page_contents ("Page number 1"));
  pages[PAGE_2] = cc_notebook_add_page ((CcNotebook *) notebook, create_page_contents ("Page number 2"));
  pages[PAGE_3] = cc_notebook_add_page ((CcNotebook *) notebook, create_page_contents ("Page number 3"));
  pages[PAGE_4] = cc_notebook_add_page ((CcNotebook *) notebook, create_page_contents ("Page number 4"));
  pages[PAGE_5] = cc_notebook_add_page ((CcNotebook *) notebook, create_page_contents ("Page number 5"));

  gtk_widget_show_all (window);

  gtk_main ();

  return EXIT_SUCCESS;
}
