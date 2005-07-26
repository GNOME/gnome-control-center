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

#include "libgswitchit/gswitchit_config.h"

#include "capplet-util.h"
#include "gconf-property-editor.h"
#include "activate-settings-daemon.h"
#include "capplet-stock-icons.h"
#include <../accessibility/keyboard/accessibility-keyboard.h>

#include "gnome-keyboard-properties-xkb.h"

GSwitchItKbdConfig initialConfig;

GConfClient *xkbGConfClient;

char *
xci_desc_to_utf8 (XklConfigItem * ci)
{
  char *sd = g_strstrip (ci->description);
  return sd[0] == 0 ? g_strdup (ci->name) :
    g_locale_to_utf8 (sd, -1, NULL, NULL, NULL);
}

static void
set_model_text (GtkWidget  * entry,
		GConfValue * value)
{
  XklConfigItem ci;
  const char * model = NULL;

  if (value != NULL && value->type == GCONF_VALUE_STRING)
    {
      model = gconf_value_get_string (value);
      if (model != NULL && model[0] == '\0')
	model = NULL;
    }

  if (model == NULL)
    model = initialConfig.model;

  g_snprintf (ci.name, sizeof (ci.name), "%s", model);

  if (XklConfigFindModel (&ci))
    {
      char * d;

      d = xci_desc_to_utf8 (&ci);
      gtk_entry_set_text (GTK_ENTRY (entry), d);
      g_free (d);
    }
  else
    {
      gtk_entry_set_text (GTK_ENTRY (entry), _("Unknown"));
    }
}

static void
model_key_changed (GConfClient * client,
		   guint         cnxn_id,
		   GConfEntry  * entry,
		   GladeXML    * dialog)
{
  set_model_text (WID ("xkb_model"),
		  gconf_entry_get_value (entry));

  enable_disable_restoring (dialog);
}

static void
setup_model_entry (GladeXML * dialog)
{
  GConfValue * value;

  value = gconf_client_get (xkbGConfClient,
			    GSWITCHIT_KBD_CONFIG_KEY_MODEL,
			    NULL);
  set_model_text (WID ("xkb_model"), value);
  if (value != NULL)
    gconf_value_free (value);

  gconf_client_notify_add (xkbGConfClient,
			   GSWITCHIT_KBD_CONFIG_KEY_MODEL,
			   (GConfClientNotifyFunc) model_key_changed,
			   dialog, NULL, NULL);
}

static void
cleanup_xkb_tabs (GladeXML * dialog)
{
  GSwitchItKbdConfigTerm (&initialConfig);
  XklConfigFreeRegistry ();
  XklConfigTerm ();
  XklTerm ();
  g_object_unref (xkbGConfClient);
  xkbGConfClient = NULL;
}

static void
reset_to_defaults (GtkWidget * button, GladeXML * dialog)
{
  GSwitchItKbdConfig emptyKbdConfig;

  GSwitchItKbdConfigInit (&emptyKbdConfig, xkbGConfClient);
  GSwitchItKbdConfigSaveToGConfBackup (&emptyKbdConfig);
  GSwitchItKbdConfigSaveToGConf (&emptyKbdConfig);
  GSwitchItKbdConfigTerm (&emptyKbdConfig);

  /* all the rest is g-s-d's business */
}

static void
chk_separate_group_per_window_toggled (GConfPropertyEditor *peditor, 
                                       const gchar *key, 
                                       const GConfValue *value, 
                                       GladeXML* dialog)
{
  xkb_layouts_enable_disable_default (dialog, value && gconf_value_get_bool (value));
}

void
setup_xkb_tabs (GladeXML * dialog, GConfChangeSet * changeset)
{
  GObject * peditor;
  xkbGConfClient = gconf_client_get_default ();

  XklInit (GDK_DISPLAY ());
  XklConfigInit ();
  XklConfigLoadRegistry ();

  GSwitchItKbdConfigInit (&initialConfig, xkbGConfClient);
  GSwitchItKbdConfigLoadFromXInitial (&initialConfig);

  setup_model_entry (dialog);

  peditor = gconf_peditor_new_boolean
    (changeset, (gchar *) GSWITCHIT_CONFIG_KEY_GROUP_PER_WINDOW, 
	WID ("chk_separate_group_per_window"), NULL);

  g_signal_connect (peditor, "value-changed", (GCallback)chk_separate_group_per_window_toggled , dialog);

/* tab 2 */
  /*fill_available_layouts_tree (dialog);*/
  xkb_layouts_prepare_selected_tree (dialog, changeset);
  xkb_layouts_fill_selected_tree (dialog);

/* tab 3 */
  xkb_options_load_options (dialog);

  xkb_layouts_register_buttons_handlers (dialog);
  g_signal_connect (G_OBJECT (WID ("xkb_reset_to_defaults")), "clicked",
		    G_CALLBACK (reset_to_defaults), dialog);

  g_signal_connect_swapped (G_OBJECT (WID ("xkb_model_pick")), "clicked",
		            G_CALLBACK (choose_model), dialog);

  xkb_layouts_register_gconf_listener (dialog);
  xkb_options_register_gconf_listener (dialog);

  g_signal_connect (G_OBJECT (WID ("keyboard_dialog")),
		    "destroy", G_CALLBACK (cleanup_xkb_tabs), dialog);

  enable_disable_restoring (dialog);
  xkb_layouts_enable_disable_default (dialog, 
                                      gconf_client_get_bool (xkbGConfClient, 
                                                             GSWITCHIT_CONFIG_KEY_GROUP_PER_WINDOW, 
                                                             NULL));
}

void
enable_disable_restoring (GladeXML * dialog)
{
  GSwitchItKbdConfig gswic;
  gboolean enable;

  GSwitchItKbdConfigInit (&gswic, xkbGConfClient);
  GSwitchItKbdConfigLoadFromGConf (&gswic, NULL);

  enable = !GSwitchItKbdConfigEquals (&gswic, &initialConfig);

  GSwitchItKbdConfigTerm (&gswic);
  gtk_widget_set_sensitive (WID ("xkb_reset_to_defaults"), enable);
}
