#include <config.h>
#include <gtk/gtk.h>
#include <gconf/gconf-client.h>
#include <glade/glade.h>

#include "capplet-util.h"
#include "gconf-property-editor.h"
#include "activate-settings-daemon.h"

#define ACCESSIBILITY_KEY       "/desktop/gnome/interface/accessibility"
#define ACCESSIBILITY_KEY_DIR   "/desktop/gnome/interface"

static gboolean initial_state;

static GladeXML *
create_dialog (void)
{
	GladeXML *dialog;

	dialog = glade_xml_new (GLADEDIR "/at-enable-dialog.glade", "at_properties_dialog", NULL);

	if (dialog) {
		GtkWidget *image;

		image = gtk_image_new_from_stock (GTK_STOCK_QUIT, GTK_ICON_SIZE_BUTTON);
		gtk_button_set_image (GTK_BUTTON (WID ("at_close_logout_button")), image);

		image = gtk_image_new_from_stock (GTK_STOCK_JUMP_TO, GTK_ICON_SIZE_BUTTON);
		gtk_button_set_image (GTK_BUTTON (WID ("at_pref_button")), image);

		image = gtk_image_new_from_stock (GTK_STOCK_JUMP_TO, GTK_ICON_SIZE_BUTTON);
		gtk_button_set_image (GTK_BUTTON (WID ("keyboard_button")), image);

		image = gtk_image_new_from_stock (GTK_STOCK_JUMP_TO, GTK_ICON_SIZE_BUTTON);
		gtk_button_set_image (GTK_BUTTON (WID ("login_button")), image);

		gtk_image_set_from_file (GTK_IMAGE (WID ("at_enable_image")),
					 PIXMAPDIR "/at-startup.png");

		gtk_image_set_from_file (GTK_IMAGE (WID ("at_applications_image")),
					 PIXMAPDIR "/at-support.png");
	}

	return dialog;
}

static void
cb_at_preferences (GtkDialog *dialog, gint response_id)
{
	g_spawn_command_line_async("gnome-default-applications-properties", NULL);
}

static void
cb_keyboard_preferences (GtkDialog *dialog, gint response_id)
{
	g_spawn_command_line_async("gnome-accessibility-keyboard-properties", NULL);
}

static void
cb_login_preferences (GtkDialog *dialog, gint response_id)
{
	g_spawn_command_line_async("gdmsetup", NULL);
}

static void
cb_dialog_response (GtkDialog *dialog, gint response_id)
{
	GnomeClient *client;
	if (response_id == GTK_RESPONSE_HELP)
		capplet_help (GTK_WINDOW (dialog),
			      "user-guide.xml",
			      "goscustaccess-11");
	else if (response_id == GTK_RESPONSE_CLOSE || response_id == GTK_RESPONSE_DELETE_EVENT)
		gtk_main_quit ();
	else {
	        g_message ("CLOSE AND LOGOUT!");
		if (!(client = gnome_master_client ())) {

			gtk_main_quit ();
		}
		gnome_client_request_save (client, GNOME_SAVE_GLOBAL, TRUE,
					   GNOME_INTERACT_ANY, FALSE, TRUE);
	}
}

static void
close_logout_update (GladeXML *dialog)
{
	GConfClient *client = gconf_client_get_default ();
	gboolean curr_state = gconf_client_get_bool (client, ACCESSIBILITY_KEY, NULL);

	gtk_widget_set_sensitive (WID ("at_close_logout_button"), initial_state != curr_state);
	g_object_unref (client);
}

static void
at_enable_toggled (GtkToggleButton *toggle_button,
		   GladeXML        *dialog)
{
	GConfClient *client = gconf_client_get_default ();
	gboolean is_enabled = gtk_toggle_button_get_active (toggle_button);

	gconf_client_set_bool (client, ACCESSIBILITY_KEY,
			       is_enabled,
			       NULL);
	g_object_unref (client);
}

static void
at_enable_update  (GConfClient *client,
		   GladeXML    *dialog)
{
	gboolean is_enabled = gconf_client_get_bool (client, ACCESSIBILITY_KEY, NULL);

	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (WID ("at_enable_toggle")),
				      is_enabled);
}

static void
at_enable_changed (GConfClient *client,
		   guint        cnxn_id,
		   GConfEntry  *entry,
		   gpointer     user_data)
{
	at_enable_update (client, user_data);
	close_logout_update (user_data);
}

static void
setup_dialog (GladeXML *dialog)
{
	GConfClient *client;
	GtkWidget *widget;
	GObject *peditor;

	client = gconf_client_get_default ();

	gconf_client_add_dir (client, ACCESSIBILITY_KEY_DIR,
			      GCONF_CLIENT_PRELOAD_ONELEVEL, NULL);

	widget = WID ("at_enable_toggle");
	g_signal_connect (widget, "toggled",
			  G_CALLBACK (at_enable_toggled),
			  dialog);

	peditor = gconf_peditor_new_boolean (NULL, ACCESSIBILITY_KEY,
					     widget,
					     NULL);

	initial_state = gconf_client_get_bool (client, ACCESSIBILITY_KEY, NULL);

	at_enable_update (client, dialog);

	gconf_client_notify_add (client, ACCESSIBILITY_KEY_DIR,
				 at_enable_changed,
				 dialog, NULL, NULL);

	g_signal_connect (G_OBJECT (WID("at_pref_button")),
			  "clicked",
			  G_CALLBACK (cb_at_preferences), NULL);

	g_signal_connect (G_OBJECT (WID("keyboard_button")),
			  "clicked",
			  G_CALLBACK (cb_keyboard_preferences), NULL);

	g_signal_connect (G_OBJECT (WID("login_button")),
			  "clicked",
			  G_CALLBACK (cb_login_preferences), NULL);

	widget = WID ("at_properties_dialog");
	capplet_set_icon (widget, "gnome-settings-accessibility-technologies");

	g_signal_connect (G_OBJECT (widget),
			  "response",
			  G_CALLBACK (cb_dialog_response), NULL);

	gtk_widget_show (widget);

	g_object_unref (client);
}

int
main (int argc, char *argv[])
{
	GnomeProgram *program;
	GladeXML *dialog;

	bindtextdomain (GETTEXT_PACKAGE, GNOMELOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	program = gnome_program_init ("gnome-at-properties", VERSION,
				      LIBGNOMEUI_MODULE, argc, argv,
				      GNOME_PARAM_APP_DATADIR, GNOMECC_DATA_DIR,
				      NULL);

	activate_settings_daemon ();

	dialog = create_dialog ();

	if (dialog) {

		setup_dialog (dialog);

		gtk_main ();

		g_object_unref (dialog);
	}

	g_object_unref (program);

	return 0;
}
