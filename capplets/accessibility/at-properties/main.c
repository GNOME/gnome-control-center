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

	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (WID ("at_keyboard_toggle")),
				      at_startup_state.enabled.osk);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (WID ("at_screenreader_toggle")),
				      at_startup_state.enabled.screenreader);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (WID ("at_magnifier_toggle")),
				      at_startup_state.enabled.magnifier);

	gtk_widget_set_sensitive (WID ("at_keyboard_toggle"),
				  at_startup_state.enabled.osk_installed);
	gtk_widget_set_sensitive (WID ("at_screenreader_toggle"),
				  at_startup_state.enabled.screenreader_installed);
	gtk_widget_set_sensitive (WID ("at_magnifier_toggle"),
				  at_startup_state.enabled.magnifier_installed);

	if (at_startup_state.enabled.osk_installed &&
	    at_startup_state.enabled.screenreader_installed &&
	    at_startup_state.enabled.magnifier_installed) {
		gtk_widget_hide (WID ("at_applications_warning_label"));
		gtk_widget_hide (WID ("at_applications_hseparator"));
	} else {
		gchar *warning_label;

		gtk_widget_show (WID ("at_applications_warning_label"));
		gtk_widget_show (WID ("at_applications_hseparator"));
		if (!at_startup_state.enabled.osk_installed &&
		    !(at_startup_state.enabled.screenreader_installed ||
		      at_startup_state.enabled.magnifier_installed)) {
			warning_label = g_strdup_printf ("<i>%s</i>", _("No Assistive Technology is available on your system.  The 'gok' package must be installed in order to get on-screen keyboard support, and the 'gnopernicus' package must be installed for screenreading and magnifying capabilities."));
		} else if (!at_startup_state.enabled.osk_installed) {
			warning_label = g_strdup_printf ("<i>%s</i>", _("Not all available assistive technologies are installed on your system.  The 'gok' package must be installed in order to get on-screen keyboard support."));
		} else {
			warning_label = g_strdup_printf ("<i>%s</i>", _("Not all available assistive technologies are installed on your system.  The 'gnopernicus' package must be installed for screenreading and magnifying capabilities."));
		}
		gtk_label_set_markup (GTK_LABEL (WID ("at_applications_warning_label")), warning_label);
		g_free (warning_label);
	}
}

static GladeXML *
create_dialog (void)
{
	GladeXML *dialog;
	
	dialog = glade_xml_new (GLADEDIR "/at-enable-dialog.glade", "at_properties_dialog", NULL);
	
	gtk_image_set_from_stock (GTK_IMAGE (WID ("at_close_and_logout_image")),
				  GTK_STOCK_QUIT, GTK_ICON_SIZE_BUTTON);
	
	gtk_image_set_from_file (GTK_IMAGE (WID ("at_enable_image")),
				 PIXMAPDIR "/at-support.png");
	
	gtk_image_set_from_file (GTK_IMAGE (WID ("at_applications_image")),
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
	gboolean has_changed = 
		(at_startup_state.flags != at_startup_state_initial.flags) && 
		gconf_client_get_bool (client, ACCESSIBILITY_KEY, NULL);

	gtk_widget_set_sensitive (WID ("at_close_logout_button"), has_changed);
	g_object_unref (client);
}

static void
at_startup_toggled (GtkToggleButton *toggle_button,
		    GladeXML        *dialog)
{
	if (toggle_button == GTK_TOGGLE_BUTTON (WID ("at_keyboard_toggle"))) {
		at_startup_state.enabled.osk = gtk_toggle_button_get_active (toggle_button);
	}
	else if (toggle_button == GTK_TOGGLE_BUTTON (WID ("at_magnifier_toggle"))) {
		at_startup_state.enabled.magnifier = gtk_toggle_button_get_active (toggle_button);
	}
	else if (toggle_button == GTK_TOGGLE_BUTTON (WID ("at_screenreader_toggle"))) {
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
  
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (WID ("at_keyboard_toggle")),
				      at_startup_state.enabled.osk);	
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (WID ("at_screenreader_toggle")),
				      at_startup_state.enabled.screenreader);	
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (WID ("at_magnifier_toggle")),
				      at_startup_state.enabled.magnifier);	
}

static void
at_enable_update  (GConfClient *client,
		   GladeXML    *dialog)
{
	gboolean is_enabled = gconf_client_get_bool (client, ACCESSIBILITY_KEY, NULL);
	
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (WID ("at_enable_toggle")),
				      is_enabled);
	
	gtk_widget_set_sensitive (WID ("at_applications_frame"), is_enabled);
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
	
	widget = WID ("at_enable_toggle");
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
	
	widget = WID ("at_keyboard_toggle");

	g_signal_connect (widget, "toggled",
			  G_CALLBACK (at_startup_toggled), 
			  dialog);
	
	widget = WID ("at_magnifier_toggle");
	g_signal_connect (widget, "toggled",
			  G_CALLBACK (at_startup_toggled), 
			  dialog);
	
	widget = WID ("at_screenreader_toggle");
	g_signal_connect (widget, "toggled",
			  G_CALLBACK (at_startup_toggled), 
			  dialog);
	
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
