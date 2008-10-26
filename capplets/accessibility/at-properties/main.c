#include <config.h>
#include <gtk/gtk.h>
#include <gconf/gconf-client.h>
#include <glade/glade.h>

#include <dbus/dbus-glib.h>
#include <dbus/dbus-glib-lowlevel.h>

#define GSM_SERVICE_DBUS   "org.gnome.SessionManager"
#define GSM_PATH_DBUS      "/org/gnome/SessionManager"
#define GSM_INTERFACE_DBUS "org.gnome.SessionManager"

enum {
        GSM_LOGOUT_MODE_NORMAL = 0,
        GSM_LOGOUT_MODE_NO_CONFIRMATION,
        GSM_LOGOUT_MODE_FORCE
};

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
		gchar *prog;

		image = gtk_image_new_from_stock (GTK_STOCK_QUIT, GTK_ICON_SIZE_BUTTON);
		gtk_button_set_image (GTK_BUTTON (WID ("at_close_logout_button")), image);

		image = gtk_image_new_from_stock (GTK_STOCK_JUMP_TO, GTK_ICON_SIZE_BUTTON);
		gtk_button_set_image (GTK_BUTTON (WID ("at_pref_button")), image);

		image = gtk_image_new_from_stock (GTK_STOCK_JUMP_TO, GTK_ICON_SIZE_BUTTON);
		gtk_button_set_image (GTK_BUTTON (WID ("keyboard_button")), image);

		image = gtk_image_new_from_stock (GTK_STOCK_JUMP_TO, GTK_ICON_SIZE_BUTTON);
		gtk_button_set_image (GTK_BUTTON (WID ("mouse_button")), image);

		image = gtk_image_new_from_stock (GTK_STOCK_JUMP_TO, GTK_ICON_SIZE_BUTTON);
		gtk_button_set_image (GTK_BUTTON (WID ("login_button")), image);

		gtk_image_set_from_file (GTK_IMAGE (WID ("at_enable_image")),
					 PIXMAPDIR "/at-startup.png");

		gtk_image_set_from_file (GTK_IMAGE (WID ("at_applications_image")),
					 PIXMAPDIR "/at-support.png");

		prog = g_find_program_in_path ("gdmsetup");
		if (prog == NULL)
			gtk_widget_hide (WID ("login_button"));

		g_free (prog);
	}

	return dialog;
}

static void
cb_at_preferences (GtkDialog *dialog, gint response_id)
{
	g_spawn_command_line_async ("gnome-default-applications-properties --show-page=a11y", NULL);
}

static void
cb_keyboard_preferences (GtkDialog *dialog, gint response_id)
{
	g_spawn_command_line_async ("gnome-keyboard-properties --a11y", NULL);
}

static void
cb_mouse_preferences (GtkDialog *dialog, gint response_id)
{
	g_spawn_command_line_async ("gnome-mouse-properties --show-page=accessibility", NULL);
}

static void
cb_login_preferences (GtkDialog *dialog, gint response_id)
{
	g_spawn_command_line_async ("gdmsetup", NULL);
}

/* get_session_bus(), get_sm_proxy(), and do_logout() are all
 * based on code from gnome-session-save.c from gnome-session.
 */
static DBusGConnection *
get_session_bus (void)
{
        DBusGConnection *bus;
        GError *error = NULL;

        bus = dbus_g_bus_get (DBUS_BUS_SESSION, &error);

        if (bus == NULL) {
                g_warning ("Couldn't connect to session bus: %s", error->message);
                g_error_free (error);
        }

        return bus;
}

static DBusGProxy *
get_sm_proxy (void)
{
        DBusGConnection *connection;
        DBusGProxy      *sm_proxy;

        if (!(connection = get_session_bus ()))
		return NULL;

        sm_proxy = dbus_g_proxy_new_for_name (connection,
					      GSM_SERVICE_DBUS,
					      GSM_PATH_DBUS,
					      GSM_INTERFACE_DBUS);

        return sm_proxy;
}

static gboolean
do_logout (GError **err)
{
        DBusGProxy *sm_proxy;
        GError     *error;
        gboolean    res;

        sm_proxy = get_sm_proxy ();
        if (sm_proxy == NULL)
		return FALSE;

        res = dbus_g_proxy_call (sm_proxy,
                                 "Logout",
                                 &error,
                                 G_TYPE_UINT, 0,   /* '0' means 'log out normally' */
                                 G_TYPE_INVALID,
                                 G_TYPE_INVALID);

        if (sm_proxy)
                g_object_unref (sm_proxy);

	return res;
}

static void
cb_dialog_response (GtkDialog *dialog, gint response_id)
{
	if (response_id == GTK_RESPONSE_HELP)
		capplet_help (GTK_WINDOW (dialog),
			      "goscustaccess-11");
	else if (response_id == GTK_RESPONSE_CLOSE || response_id == GTK_RESPONSE_DELETE_EVENT)
		gtk_main_quit ();
	else {
	        g_message ("CLOSE AND LOGOUT!");

		if (!do_logout (NULL))
			gtk_main_quit ();
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
at_enable_update (GConfClient *client,
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

	g_signal_connect (G_OBJECT (WID("mouse_button")),
			  "clicked",
			  G_CALLBACK (cb_mouse_preferences), NULL);

	g_signal_connect (G_OBJECT (WID("login_button")),
			  "clicked",
			  G_CALLBACK (cb_login_preferences), NULL);

	widget = WID ("at_properties_dialog");
	capplet_set_icon (widget, "preferences-desktop-accessibility");

	g_signal_connect (G_OBJECT (widget),
			  "response",
			  G_CALLBACK (cb_dialog_response), NULL);

	gtk_widget_show (widget);

	g_object_unref (client);
}

int
main (int argc, char *argv[])
{
	GladeXML *dialog;

	capplet_init (NULL, &argc, &argv);

	activate_settings_daemon ();

	dialog = create_dialog ();

	if (dialog) {

		setup_dialog (dialog);

		gtk_main ();

		g_object_unref (dialog);
	}

	return 0;
}
