/* gnome-ui-properties.c
 * Copyright (C) 2002 Jonathan Blandford
 *
 * Written by: Jonathan Blandford <jrb@gnome.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gnome.h>
#include <gconf/gconf-client.h>
#include <glade/glade.h>

#include "capplet-util.h"
#include "gconf-property-editor.h"

enum
{
	RESPONSE_APPLY = 1,
	RESPONSE_CLOSE
};

static GConfEnumStringPair toolbar_style_enums[] = {
  { 0, "both" },
  { 1, "icons" },
  { 2, "text" }
};

static GConfValue *
toolbar_from_widget (GConfValue *value) 
{
  GConfValue *new_value;

  new_value = gconf_value_new (GCONF_VALUE_STRING);
  gconf_value_set_string (new_value,
			  gconf_enum_to_string (toolbar_style_enums, gconf_value_get_int (value)));

  return new_value;
}

static GConfValue *
toolbar_to_widget (GConfValue *value) 
{
  GConfValue *new_value;
  gint val = 2;

  new_value = gconf_value_new (GCONF_VALUE_INT);
  gconf_string_to_enum (toolbar_style_enums,
			gconf_value_get_string (value),
			&val);
  gconf_value_set_int (new_value, val);

  return new_value;
}


static void
dialog_button_clicked_cb (GtkDialog *dialog, gint response_id, GConfChangeSet *changeset) 
{
  switch (response_id)
    {
    case RESPONSE_CLOSE:
    case GTK_RESPONSE_DELETE_EVENT:
    default:
      gtk_main_quit ();
      break;
    }
}

static GladeXML *
create_dialog (void)
{
  GladeXML *dialog;

  dialog = glade_xml_new (GNOMECC_DATA_DIR "/interfaces/gnome2-ui-properties.glade", "gnome_ui_properties_dialog", NULL);
  /*  dialog = glade_xml_new ("gnome2-ui-properties.glade", "gnome_ui_properties_dialog", NULL);*/
  return dialog;
}

static void
setup_dialog (GladeXML *dialog, GConfChangeSet *changeset)
{
  GtkWidget *widget;

  gconf_peditor_new_boolean
    (changeset, "/desktop/gnome/interface/toolbar_detachable", WID ("detachable_toolbars_toggle"), NULL);

  gconf_peditor_new_boolean
    (changeset, "/desktop/gnome/interface/menus_have_icons", WID ("menu_icons_toggle"), NULL);

  gconf_peditor_new_select_menu
    (changeset, "/desktop/gnome/interface/toolbar_style", WID ("toolbar_style_omenu"),
     "conv-to-widget-cb", toolbar_to_widget,
     "conv-from-widget-cb", toolbar_from_widget,
     NULL);


  widget = WID ("gnome_ui_properties_dialog");
  g_signal_connect (G_OBJECT (widget), "response",
		    (GCallback) dialog_button_clicked_cb, changeset);
  gtk_widget_show_all (widget);

}

int
main (int argc, char **argv)
{
  GConfClient    *client;
  GConfChangeSet *changeset = NULL;
  GladeXML       *dialog;

  bindtextdomain (GETTEXT_PACKAGE, GNOMELOCALEDIR);
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
  textdomain (GETTEXT_PACKAGE);

  gnome_program_init (argv[0], VERSION, LIBGNOMEUI_MODULE, argc, argv,
		      GNOME_PARAM_APP_DATADIR, GNOMECC_DATA_DIR,
		      NULL);

  client = gconf_client_get_default ();
  gconf_client_add_dir (client, "/desktop/gnome/interface", GCONF_CLIENT_PRELOAD_ONELEVEL, NULL);

  dialog = create_dialog ();
  setup_dialog (dialog, changeset);

  gtk_main ();

  return 0;
}
