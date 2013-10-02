#include <gtk/gtk.h>
#include "cc-background-chooser-dialog.h"

static void
on_dialog_response (GtkDialog *dialog,
		    int        response_id,
		    gpointer   user_data)
{
	g_message ("response: %d", response_id);
	gtk_widget_destroy (GTK_WIDGET (dialog));
	gtk_main_quit ();
}

int main (int argc, char **argv)
{
	GtkWidget *dialog;

	gtk_init (&argc, &argv);

	dialog = cc_background_chooser_dialog_new ();
	g_signal_connect (G_OBJECT (dialog), "response",
			  G_CALLBACK (on_dialog_response), NULL);
	gtk_widget_show_all (dialog);

	gtk_main ();

	return 0;
}
