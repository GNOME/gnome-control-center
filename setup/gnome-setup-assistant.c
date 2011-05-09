#include <stdlib.h>
#include <gtk/gtk.h>

static void
prepare_cb (GtkAssistant *assi, gpointer data)
{
        GtkWidget *page;
        gint n;

        n = gtk_assistant_get_current_page (assi);
        page = gtk_assistant_get_nth_page (assi, n);

        gtk_assistant_set_page_complete (assi, page, TRUE);
}

static void
disable_autostart (void)
{
        GSettings *settings;

        settings = g_settings_new ("org.gnome.control-center.setup");
        g_settings_set_boolean (settings, "need-setup", FALSE);
}

static void
close_cb (GtkAssistant *assi, gpointer data)
{
        disable_autostart ();

        gtk_main_quit ();
}

int
main (int argc, char *argv[])
{
        GtkBuilder *builder;
        GtkAssistant *assi;
        GtkWidget *widget;
        GError *error;
        const gchar *filename;

        gtk_init (&argc, &argv);

        filename = UIDIR "/setup.ui";
        if (!g_file_test (filename, G_FILE_TEST_EXISTS))
                filename = "setup.ui";

        builder = gtk_builder_new ();
        error = NULL;
        if (!gtk_builder_add_from_file (builder, filename, &error)) {
                g_error ("%s", error->message);
                g_error_free (error);
                exit (1);
        }

        assi = (GtkAssistant *) gtk_builder_get_object (builder, "gnome-setup-assistant");

        gtk_assistant_commit (assi);

        g_signal_connect (G_OBJECT (assi), "prepare",
                          G_CALLBACK (prepare_cb), NULL);
        g_signal_connect (G_OBJECT (assi), "close",
                          G_CALLBACK (close_cb), NULL);

        widget = (GtkWidget *) gtk_builder_get_object (builder, "welcome-image");
        filename = UIDIR "/welcome-image.jpg";
        if (!g_file_test (filename, G_FILE_TEST_EXISTS))
                filename = "welcome-image.jpg";
        gtk_image_set_from_file (GTK_IMAGE (widget), filename);

        gtk_window_present (GTK_WINDOW (assi));

        gtk_main ();

        g_settings_sync ();

        return 0;
}
