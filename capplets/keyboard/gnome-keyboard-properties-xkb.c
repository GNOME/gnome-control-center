/* -*- mode: c; style: linux -*- */

/* gnome-keyboard-properties-xkb.c
 * Copyright (C) 2003 Sergey V. Oudaltsov
 *
 * Written by: Sergey V. Oudaltsov <svu@users.sourceforge.net>
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
#include <gdk/gdkx.h>
#include <gconf/gconf-client.h>
#include <glade/glade.h>

#include "libgswitchit/gswitchit_xkb_config.h"

#include "capplet-util.h"
#include "gconf-property-editor.h"
#include "activate-settings-daemon.h"
#include "capplet-stock-icons.h"
#include <../accessibility/keyboard/accessibility-keyboard.h>

#include "gnome-keyboard-properties-xkb.h"

#define CWID(s) glade_xml_get_widget (chooserDialog, s)

static GSwitchItXkbConfig initialConfig;

char *
xci_desc_to_utf8 (XklConfigItem * ci)
{
  char *sd = g_strstrip (ci->description);
  return sd[0] == 0 ? g_strdup (ci->name) :
    g_locale_to_utf8 (sd, -1, NULL, NULL, NULL);
}

static GConfValue *
model_from_widget (GConfPropertyEditor * peditor, GConfValue * value)
{
  GConfValue *new_value;

  new_value = gconf_value_new (GCONF_VALUE_STRING);

  if (value->type == GCONF_VALUE_STRING)
    {
      GObject* widget = gconf_property_editor_get_ui_control(peditor);
      gchar* n = g_object_get_data (widget, "xkbModelName");
      gconf_value_set_string (new_value, n);
    }
  else
    gconf_value_set_string (new_value, _("Unknown"));

  return new_value;
}

static GConfValue *
model_to_widget (GConfPropertyEditor * peditor, GConfValue * value)
{
  GConfValue *new_value;

  new_value = gconf_value_new (GCONF_VALUE_STRING);

  if (value->type == GCONF_VALUE_STRING)
    {
      XklConfigItem ci;
      g_snprintf( ci.name, sizeof (ci.name), "%s", gconf_value_get_string( value ) );
      if ( XklConfigFindModel( &ci ) )
      {
        GObject* widget = gconf_property_editor_get_ui_control(peditor);
        gchar* d = xci_desc_to_utf8 (&ci);

        g_object_set_data_full (widget, "xkbModelName", g_strdup (ci.name), g_free);
        gconf_value_set_string (new_value, d);
        g_free (d);
      }
      else
        gconf_value_set_string (new_value, _("Unknown"));
    }

  return new_value;
}

static void
cleanup_xkb_tabs (GladeXML * dialog)
{
  GSwitchItXkbConfigTerm (&initialConfig);
  XklConfigFreeRegistry ();
  XklConfigTerm ();
  XklTerm ();
}

static void
reset_to_defaults (GtkWidget * button, GladeXML * dialog)
{
  gconf_client_set_bool (gconf_client_get_default (),
			 GSWITCHIT_CONFIG_XKB_KEY_OVERRIDE_SETTINGS,
			 TRUE, NULL);
  /* all the rest is g-s-d's business */
}

static void
update_model (GConfClient * client,
	      guint cnxn_id, GConfEntry * entry, GladeXML * dialog)
{
  enable_disable_restoring (dialog);
}

void
setup_xkb_tabs (GladeXML * dialog, GConfChangeSet * changeset)
{
  GConfClient *confClient = gconf_client_get_default ();

  XklInit (GDK_DISPLAY ());
  XklConfigInit ();
  XklConfigLoadRegistry ();

//  fill_models_option_menu (dialog);

  gconf_peditor_new_string
    (changeset, (gchar *) GSWITCHIT_CONFIG_XKB_KEY_MODEL,
     WID ("xkb_model"),
     "conv-to-widget-cb", model_to_widget,
     "conv-from-widget-cb", model_from_widget, NULL);

  fill_available_layouts_tree (dialog);
  fill_available_options_tree (dialog);
  prepare_selected_layouts_tree (dialog);
  prepare_selected_options_tree (dialog);
  fill_selected_layouts_tree (dialog);
  fill_selected_options_tree (dialog);

  register_layouts_buttons_handlers (dialog);
  register_options_buttons_handlers (dialog);
  g_signal_connect (G_OBJECT (WID ("xkb_reset_to_defaults")), "clicked",
		    G_CALLBACK (reset_to_defaults), dialog);

  g_signal_connect_swapped (G_OBJECT (WID ("xkb_model_pick")), "clicked",
		            G_CALLBACK (choose_model), dialog);

  register_layouts_gconf_listener (dialog);
  register_options_gconf_listener (dialog);

  g_signal_connect (G_OBJECT (WID ("keyboard_dialog")),
		    "destroy", G_CALLBACK (cleanup_xkb_tabs), dialog);

  gconf_client_notify_add (gconf_client_get_default (),
			   GSWITCHIT_CONFIG_XKB_KEY_MODEL,
			   (GConfClientNotifyFunc)
			   update_model, dialog, NULL, NULL);

  GSwitchItXkbConfigInit (&initialConfig, confClient);
  g_object_unref (confClient);
  GSwitchItXkbConfigLoadInitial (&initialConfig);

  enable_disable_restoring (dialog);

  g_signal_connect_swapped (G_OBJECT (WID ("enable_preview")), "toggled",
		            G_CALLBACK (preview_toggled), dialog);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (WID ("enable_preview")), FALSE);
}

void
enable_disable_restoring (GladeXML * dialog)
{
  GSwitchItXkbConfig gswic;
  GConfClient *confClient = gconf_client_get_default ();
  gboolean enable;

  GSwitchItXkbConfigInit (&gswic, confClient);
  g_object_unref (confClient);
  GSwitchItXkbConfigLoad (&gswic);

  enable = !GSwitchItXkbConfigEquals (&gswic, &initialConfig);

  GSwitchItXkbConfigTerm (&gswic);
  gtk_widget_set_sensitive (WID ("xkb_reset_to_defaults"), enable);
}
