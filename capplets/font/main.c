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
#define DESKTOP_FONT_KEY "/apps/nautilus/preferences/desktop_font"

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
	if (response_id == GTK_RESPONSE_HELP)
		capplet_help (GTK_WINDOW (dialog),
			"wgoscustdesk.xml",
			"goscustdesk-38");
	else
		gtk_main_quit ();
}


static void
setup_dialog (GladeXML *dialog)
{
  GConfClient *client;
  GtkWidget *widget;
  GObject *peditor;
  gchar *filename;
  GdkPixbuf *icon_pixbuf;

  client = gconf_client_get_default ();

  gconf_client_add_dir (client, "/desktop/gnome/interface", GCONF_CLIENT_PRELOAD_ONELEVEL, NULL);

  peditor = gconf_peditor_new_font (NULL, GTK_FONT_KEY,
		  		    WID ("application_font"),
				    PEDITOR_FONT_COMBINED, NULL);

  peditor = gconf_peditor_new_font (NULL, DESKTOP_FONT_KEY,
		  		    WID ("desktop_font"),
				    PEDITOR_FONT_COMBINED, NULL);

  widget = WID ("font_dialog");
  filename = gnome_program_locate_file (NULL, GNOME_FILE_DOMAIN_APP_PIXMAP, "keyboard-shortcut.png", TRUE, NULL);
  icon_pixbuf = gdk_pixbuf_new_from_file ("font-capplet.png", NULL);
  gtk_window_set_icon (GTK_WINDOW (widget), icon_pixbuf);
  g_free (filename);
  g_object_unref (icon_pixbuf);
  gtk_widget_show (widget);

  g_signal_connect (G_OBJECT (widget),
    "response",
    G_CALLBACK (cb_dialog_response), NULL);
}

int
main (int argc, char *argv[])
{
  GladeXML *dialog;

  bindtextdomain (GETTEXT_PACKAGE, GNOMELOCALEDIR);
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
  textdomain (GETTEXT_PACKAGE);

  gnome_program_init ("gnome-font-properties", VERSION,
		      LIBGNOMEUI_MODULE, argc, argv,
		      GNOME_PARAM_APP_DATADIR, GNOMECC_DATA_DIR,
		      NULL);

  activate_settings_daemon ();

  dialog = create_dialog ();
  setup_dialog (dialog);

  gtk_main ();

  return 0;
}
