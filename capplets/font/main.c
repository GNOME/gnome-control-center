#include <gnome.h>
#include <glade/glade.h>
#include <config.h>

#include "gconf-property-editor.h"

static GladeXML *xml;
static GConfChangeSet *changeset;
	

#define WID(w) (glade_xml_get_widget (xml, (w)))

enum
{
	RESPONSE_APPLY = 1,
	RESPONSE_CLOSE
};

void
response_cb (GtkDialog *dialog, gint r, gpointer data)
{
	switch (r)
	{
	case RESPONSE_APPLY:
		gconf_client_commit_change_set (gconf_client_get_default (), changeset, TRUE, NULL);
		
		/* We may want to revert to the default schema-ed value */
		if (!gconf_client_get_bool (gconf_client_get_default (), "/desktop/gnome/interface/use_custom_font", NULL))
		{
			g_print ("Unsetting\n");
			gconf_client_unset (gconf_client_get_default (), "/desktop/gnome/interface/font_name", NULL);
		}		
		
		break;
	case RESPONSE_CLOSE:
		gtk_main_quit ();
		break;
	}
}

static void
setup_dialog (void)
{
	GObject *peditor;

	peditor = gconf_peditor_new_boolean (changeset, "/desktop/gnome/interface/use_custom_font", WID ("custom_check"), NULL);
	gconf_peditor_widget_set_guard (GCONF_PROPERTY_EDITOR (peditor), WID ("font_picker"));
	peditor = gconf_peditor_new_font (changeset, "/desktop/gnome/interface/font_name", WID ("font_picker"), NULL);
}
	
int
main (int argc, char **argv)
{
	gnome_program_init ("gnome2-font-properties", VERSION,
			    LIBGNOMEUI_MODULE, argc, argv, NULL);

	xml = glade_xml_new (GNOMECC_DATA_DIR "/interfaces/font-properties.glade", NULL, NULL);

	changeset = gconf_change_set_new ();
	setup_dialog ();
	
	glade_xml_signal_autoconnect (xml);
	gtk_main ();

	gconf_change_set_unref (changeset);
 
	return 0;
}

