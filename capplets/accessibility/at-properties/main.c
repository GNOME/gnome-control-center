#include <config.h>
#include <gtk/gtk.h>
#include <gconf/gconf-client.h>
#include <glade/glade.h>

#include "capplet-util.h"
#include "gconf-property-editor.h"
#include "activate-settings-daemon.h"
#include "at-startup-session.h"

#define ACCESSIBILITY_KEY       "/desktop/gnome/interface/accessibility"
#define ACCESSIBILITY_KEY_DIR   "/desktop/gnome/interface"
#define AT_STARTUP_DIR          "/desktop/gnome/accessibility/startup"
#define AT_STARTUP_KEY          "/desktop/gnome/accessibility/startup/exec_ats"
#define SR_PREFS_DIR            "/apps/gnopernicus/srcore"

static AtStartupState at_startup_state, at_startup_state_initial;

static void
init_startup_state (GladeXML *dialog)
{
	GConfClient *client = gconf_client_get_default ();
	
	at_startup_state.flags = (gconf_client_get_bool (client, 
							 ACCESSIBILITY_KEY, 
							 NULL)) ? 1 : 0;

	at_startup_state_init (&at_startup_state);

	at_startup_state_initial.flags = at_startup_state.flags;
	g_object_unref (client);

	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (WID ("chkKeyboard")),
				      at_startup_state.enabled.osk);	
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (WID ("chkScreenreader")),
				      at_startup_state.enabled.screenreader);	
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (WID ("chkMagnifier")),
				      at_startup_state.enabled.magnifier);	
}

static GladeXML *
create_dialog (void)
{
	GladeXML *dialog;
	
	dialog = glade_xml_new (GLADEDIR "/at-enable-dialog.glade", "dlgATPrefs", NULL);
	
	gtk_image_set_from_stock (GTK_IMAGE (WID ("image1")),
				  GTK_STOCK_QUIT, GTK_ICON_SIZE_BUTTON);
	
	gtk_image_set_from_file (GTK_IMAGE (WID ("image2")),
				 PIXMAPDIR "/at-support.png");
	
	gtk_image_set_from_file (GTK_IMAGE (WID ("image3")),
				 PIXMAPDIR "/at-startup.png");
	
	return dialog;
}

static void
cb_dialog_response (GtkDialog *dialog, gint response_id)
{
	GnomeClient *client;
	if (response_id == GTK_RESPONSE_HELP)
		capplet_help (GTK_WINDOW (dialog),
			      "foo.xml",
			      "bar");
	else if (response_id == GTK_RESPONSE_CLOSE)
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
	gboolean has_changed = 
		(at_startup_state.flags != at_startup_state_initial.flags) && 
		gconf_client_get_bool (client, ACCESSIBILITY_KEY, NULL);

	gtk_widget_set_sensitive (WID ("btnCloseLogout"), has_changed);
	g_object_unref (client);
}

static void
at_startup_toggled (GtkToggleButton *toggle_button,
		    GladeXML        *dialog)
{
	if (toggle_button == GTK_TOGGLE_BUTTON (WID ("chkKeyboard"))) {
		at_startup_state.enabled.osk = gtk_toggle_button_get_active (toggle_button);
	}
	else if (toggle_button == GTK_TOGGLE_BUTTON (WID ("chkMagnifier"))) {
		at_startup_state.enabled.magnifier = gtk_toggle_button_get_active (toggle_button);
	}
	else if (toggle_button == GTK_TOGGLE_BUTTON (WID ("chkScreenreader"))) {
		at_startup_state.enabled.screenreader = gtk_toggle_button_get_active (toggle_button);
	}
	
	at_startup_state_update (&at_startup_state);
	close_logout_update (dialog);
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
	at_startup_state.enabled.support = is_enabled;
	g_object_unref (client);
}

static void
at_startup_update_ui (GConfClient *client,
		      GladeXML    *dialog)
{
	at_startup_state_init (&at_startup_state);
  
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (WID ("chkKeyboard")),
				      at_startup_state.enabled.osk);	
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (WID ("chkScreenreader")),
				      at_startup_state.enabled.screenreader);	
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (WID ("chkMagnifier")),
				      at_startup_state.enabled.magnifier);	
}

static void
at_enable_update  (GConfClient *client,
		   GladeXML    *dialog)
{
	gboolean is_enabled = gconf_client_get_bool (client, ACCESSIBILITY_KEY, NULL);
	
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (WID ("chkEnableAT")),
				      is_enabled);
	
	gtk_widget_set_sensitive (WID ("fraApps"), is_enabled);
	gtk_widget_set_sensitive (WID ("chkKeyboard"), is_enabled);
	gtk_widget_set_sensitive (WID ("chkMagnifier"), is_enabled);
	gtk_widget_set_sensitive (WID ("chkScreenreader"), is_enabled);
}

static void
at_startup_changed (GConfClient *client,
		    guint        cnxn_id,
		    GConfEntry  *entry,
		    gpointer     user_data)
{
	at_startup_state_init (&at_startup_state);
	at_startup_update_ui (client, user_data);
	close_logout_update (user_data); 
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
	
	widget = WID ("chkEnableAT");
	g_signal_connect (widget, "toggled",
			  G_CALLBACK (at_enable_toggled),   
			  dialog);
	
	peditor = gconf_peditor_new_boolean (NULL, ACCESSIBILITY_KEY,
					     widget,
					     NULL);
	
	at_enable_update (client, dialog);
	
	gconf_client_notify_add (client, ACCESSIBILITY_KEY_DIR,
				 at_enable_changed,
				 dialog, NULL, NULL);
	
	gconf_client_add_dir (client, AT_STARTUP_DIR, 
			      GCONF_CLIENT_PRELOAD_ONELEVEL, NULL);

	gconf_client_add_dir (client, SR_PREFS_DIR, 
			      GCONF_CLIENT_PRELOAD_ONELEVEL, NULL);

	gconf_client_notify_add (client, AT_STARTUP_DIR,
				 at_startup_changed,
				 dialog, NULL, NULL);
	
	gconf_client_notify_add (client, SR_PREFS_DIR,
				 at_startup_changed,
				 dialog, NULL, NULL);
	
	widget = WID ("chkKeyboard");

	g_signal_connect (widget, "toggled",
			  G_CALLBACK (at_startup_toggled), 
			  dialog);
	
	widget = WID ("chkMagnifier");
	g_signal_connect (widget, "toggled",
			  G_CALLBACK (at_startup_toggled), 
			  dialog);
	
	widget = WID ("chkScreenreader");
	g_signal_connect (widget, "toggled",
			  G_CALLBACK (at_startup_toggled), 
			  dialog);
	
	widget = WID ("dlgATPrefs");
	capplet_set_icon (widget, "at-enable-capplet.png");
	
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
	
	bindtextdomain (GETTEXT_PACKAGE, GNOMELOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);
	
	gnome_program_init ("gnome-at-properties", VERSION,
			    LIBGNOMEUI_MODULE, argc, argv,
			    GNOME_PARAM_APP_DATADIR, GNOMECC_DATA_DIR,
			    NULL);
	
	activate_settings_daemon ();
	
	dialog = create_dialog ();
	
	init_startup_state (dialog);
	
	setup_dialog (dialog);

	gtk_main ();
	
	return 0;
}
