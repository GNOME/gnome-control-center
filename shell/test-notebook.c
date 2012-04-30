#include <stdlib.h>
#include "cc-notebook.h"

#define NUM_PAGES 5

static GHashTable *pages;
static GtkWidget *notebook;

static void
goto_page (GtkButton *button,
	   gpointer   user_data)
{
	int target = GPOINTER_TO_INT (user_data);
	GtkWidget *widget;

	if (target < 1)
		target = NUM_PAGES;
	else if (target > NUM_PAGES)
		target = 1;
	widget = g_hash_table_lookup (pages, GINT_TO_POINTER (target));
	cc_notebook_select_page (CC_NOTEBOOK (notebook), widget);
}

static GtkWidget *
create_page_contents (int page_num)
{
  GtkWidget *vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
  char *text = g_strdup_printf ("Page number %d", page_num);

  GtkWidget *hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);

  GtkWidget *back_button = gtk_button_new_with_label ("←");
  gtk_widget_set_halign (back_button, GTK_ALIGN_START);
  gtk_box_pack_start (GTK_BOX (hbox), back_button, FALSE, FALSE, 0);
  g_signal_connect (G_OBJECT (back_button), "clicked",
		    G_CALLBACK (goto_page), GINT_TO_POINTER (page_num - 1));

  GtkWidget *fwd_button = gtk_button_new_with_label ("→");
  gtk_widget_set_halign (fwd_button, GTK_ALIGN_END);
  gtk_box_pack_end (GTK_BOX (hbox), fwd_button, FALSE, FALSE, 0);
  g_signal_connect (G_OBJECT (fwd_button), "clicked",
		    G_CALLBACK (goto_page), GINT_TO_POINTER (page_num + 1));

  GtkWidget *label = gtk_label_new (text);

  gtk_box_pack_end (GTK_BOX (vbox), label, TRUE, TRUE, 0);

  gtk_widget_show_all (vbox);
  g_object_set_data_full (G_OBJECT (vbox), "display-name", text, g_free);

  g_hash_table_insert (pages, GINT_TO_POINTER (page_num), vbox);

  return vbox;
}

static void
on_page_change (CcNotebook *notebook)
{
  g_print (G_STRLOC ": Currently selected page: %s\n",
           (char *) g_object_get_data (G_OBJECT (cc_notebook_get_selected_page (notebook)), "display-name"));
}

int
main (int argc, char *argv[])
{
  guint i;

  if (gtk_clutter_init (&argc, &argv) != CLUTTER_INIT_SUCCESS)
    return EXIT_FAILURE;

  GtkWidget *window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  g_signal_connect (window, "destroy", G_CALLBACK (gtk_main_quit), NULL);
  gtk_widget_show (window);

  notebook = cc_notebook_new ();
  gtk_container_add (GTK_CONTAINER (window), notebook);
  g_signal_connect (notebook, "notify::current-page", G_CALLBACK (on_page_change), NULL);

  pages = g_hash_table_new (g_direct_hash, g_direct_equal);

  for (i = 1; i <= NUM_PAGES; i++) {
    GtkWidget *page = create_page_contents (i);
    cc_notebook_add_page ((CcNotebook *) notebook, page);
  }

  gtk_widget_show_all (window);

  gtk_main ();

  return EXIT_SUCCESS;
}
