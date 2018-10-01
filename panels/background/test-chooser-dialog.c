#include <gtk/gtk.h>
#include "cc-background-chooser-dialog.h"

static void
on_dialog_response (GtkDialog *dialog,
		    int        response_id,
		    gpointer   user_data)
{
	g_debug ("response: %d", response_id);
	if (response_id == GTK_RESPONSE_OK) {
		g_autoptr(CcBackgroundItem) item = NULL;

		item = cc_background_chooser_dialog_get_item (CC_BACKGROUND_CHOOSER_DIALOG (dialog));
		cc_background_item_dump (item);
	}
	gtk_widget_destroy (GTK_WIDGET (dialog));
	gtk_main_quit ();
}

int main (int argc, char **argv)
{
	GtkWidget *dialog;

	g_setenv ("G_MESSAGES_DEBUG", "all", TRUE);

	gtk_init (&argc, &argv);

	dialog = cc_background_chooser_dialog_new (NULL);
	gtk_window_set_modal (GTK_WINDOW (dialog), FALSE);
	g_signal_connect (G_OBJECT (dialog), "response",
			  G_CALLBACK (on_dialog_response), NULL);
	gtk_widget_show (dialog);

	gtk_main ();

	return 0;
}
