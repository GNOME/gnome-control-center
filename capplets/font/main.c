/* This program was written with lots of love under the GPL by Jonathan
 * Blandford <jrb@gnome.org>
 */

#include <config.h>

#include <string.h>
#include <gtk/gtk.h>
#include <gconf/gconf-client.h>
#include <glade/glade.h>

#include "theme-common.h"
#include "capplet-util.h"
#include "activate-settings-daemon.h"
#include "gconf-property-editor.h"

#define GTK_FONT_KEY "/desktop/gnome/interface/font_name"
#define DESKTOP_FONT_NAME_KEY "/apps/nautilus/preferences/default_font"
#define DESKTOP_FONT_SIZE_KEY "/apps/nautilus/preferences/default_font_size"

static GladeXML *
create_dialog (void)
{
  GladeXML *dialog;

  dialog = glade_xml_new (GLADEDIR "/font-properties.glade", "font_dialog", NULL);

  return dialog;
}

static void
cb_dialog_response (GtkDialog *dialog, gint response_id)
{
	if (response_id == GTK_RESPONSE_HELP) {
		GError *error = NULL;

		/* TODO : get this written */
		gnome_help_display_desktop (NULL,
			"control-center-manual",
			"config-font.xml",
			"CONFIGURATION", &error);
		if (error) {
			g_warning ("help error: %s\n", error->message);
			g_error_free (error);
		}
	} else
		gtk_main_quit ();
}

static void
setup_dialog (GladeXML *dialog)
{
  GConfClient *client;
  GtkWidget *widget;
  GObject *peditor;

  client = gconf_client_get_default ();

  gconf_client_add_dir (client, "/desktop/gnome/interface", GCONF_CLIENT_PRELOAD_ONELEVEL, NULL);

  peditor = gconf_peditor_new_font (NULL, GTK_FONT_KEY,
		  		    WID ("application_font"),
				    PEDITOR_FONT_COMBINED, NULL);

  peditor = gconf_peditor_new_font (NULL, DESKTOP_FONT_NAME_KEY,
		  		    WID ("desktop_font"),
				    PEDITOR_FONT_NAME, NULL);

  peditor = gconf_peditor_new_font (NULL, DESKTOP_FONT_SIZE_KEY,
		  		    WID ("desktop_font"),
				    PEDITOR_FONT_SIZE, NULL);

  widget = WID ("font_dialog");
  gtk_widget_show (widget);

  g_signal_connect (G_OBJECT (widget),
    "response",
    G_CALLBACK (cb_dialog_response), NULL);
}

int
main (int argc, char *argv[])
{
  GladeXML *dialog;

  gtk_init (&argc, &argv);

  bindtextdomain (GETTEXT_PACKAGE, GNOMELOCALEDIR);
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
  textdomain (GETTEXT_PACKAGE);

  gnome_program_init (argv[0], VERSION, LIBGNOMEUI_MODULE, argc, argv,
		      GNOME_PARAM_APP_DATADIR, GNOMECC_DATA_DIR,
		      NULL);

  activate_settings_daemon ();

  dialog = create_dialog ();
  setup_dialog (dialog);

  gtk_main ();

  return 0;
}
