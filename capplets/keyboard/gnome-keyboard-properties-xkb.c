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
#include <gconf/gconf-client.h>
#include <glade/glade.h>

#include "libgswitchit/gswitchit_xkb_config.h"

#include "capplet-util.h"
#include "gconf-property-editor.h"
#include "activate-settings-daemon.h"
#include "capplet-stock-icons.h"
#include <../accessibility/keyboard/accessibility-keyboard.h>

#include "gnome-keyboard-properties-xkb.h"

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
  GConfValue *new_value = gconf_value_new (GCONF_VALUE_STRING);
  const char *rvs = "";
  if (value->type == GCONF_VALUE_INT)
    {
      GtkWidget *omenu =
	GTK_WIDGET (gconf_property_editor_get_ui_control (peditor));
      const int ivalue = gconf_value_get_int (value);
      GtkWidget *menu = gtk_option_menu_get_menu (GTK_OPTION_MENU (omenu));
      GList *items = GTK_MENU_SHELL (menu)->children;
      while (items != NULL)
	{
	  GtkWidget *item = GTK_WIDGET (items->data);
	  const int itemNo =
	    GPOINTER_TO_INT (g_object_get_data (G_OBJECT (item), "itemNo"));
	  if (itemNo == ivalue)
	    {
	      rvs = (const char *)
		g_object_get_data (G_OBJECT (item), "itemId");
	      break;
	    }
	  items = items->next;
	}
    }
  gconf_value_set_string (new_value, rvs);
  return new_value;
}

static GConfValue *
model_to_widget (GConfPropertyEditor * peditor, GConfValue * value)
{
  GConfValue *new_value;
  int rvi = -1;

  new_value = gconf_value_new (GCONF_VALUE_INT);

  if (value->type == GCONF_VALUE_STRING)
    {
      GtkWidget *omenu =
	GTK_WIDGET (gconf_property_editor_get_ui_control (peditor));
      const char *svalue = gconf_value_get_string (value);
      GtkWidget *menu = gtk_option_menu_get_menu (GTK_OPTION_MENU (omenu));
      GList *items = GTK_MENU_SHELL (menu)->children;
      while (items != NULL)
	{
	  GtkWidget *item = GTK_WIDGET (items->data);
	  const char *itemId = (const char *)
	    g_object_get_data (G_OBJECT (item), "itemId");
	  if (!g_strcasecmp (itemId, svalue))
	    {
	      rvi =
		GPOINTER_TO_INT (g_object_get_data
				 (G_OBJECT (item), "itemNo"));
	      break;
	    }
	  items = items->next;
	}
    }
  gconf_value_set_int (new_value, rvi);

  return new_value;
}

static void
cleanup_xkb_tabs (GladeXML * dialog)
{
  XklConfigFreeRegistry ();
  XklConfigTerm ();
}

static void
add_model_to_option_menu (const XklConfigItemPtr configItem, GtkWidget * menu)
{
  GList *existingItemNode = GTK_MENU_SHELL (menu)->children;
  char *utfModelName = xci_desc_to_utf8 (configItem);
  GtkWidget *menuItem = gtk_menu_item_new_with_label (utfModelName);
  int position = 0;
  g_object_set_data_full (G_OBJECT (menuItem), "itemId",
			  g_strdup (configItem->name),
			  (GDestroyNotify) g_free);
  for (; existingItemNode != NULL;
       position++, existingItemNode = existingItemNode->next)
    {
      GtkWidget *menuItem = GTK_WIDGET (existingItemNode->data);
      GtkWidget *lbl = GTK_BIN (menuItem)->child;
      const char *txt = gtk_label_get_text (GTK_LABEL (lbl));
      if (g_utf8_collate(txt, utfModelName) > 0)
	break;
    }
  g_free (utfModelName);
  gtk_menu_shell_insert (GTK_MENU_SHELL (menu),
			 GTK_WIDGET (menuItem), position);
}

static void
fill_models_option_menu (GladeXML * dialog)
{
  GtkWidget *menu = gtk_menu_new ();
  int itemCounter = 0;
  GList *items;
  XklConfigEnumModels ((ConfigItemProcessFunc)
		       add_model_to_option_menu, menu);

  items = GTK_MENU_SHELL (menu)->children;
  while (items != NULL)
    {
      GtkWidget *menuItem = GTK_WIDGET (items->data);
      g_object_set_data (G_OBJECT (menuItem), "itemNo",
			 GINT_TO_POINTER (itemCounter++));
      items = items->next;
    }

  gtk_option_menu_set_menu (GTK_OPTION_MENU (WID ("xkb_models")),
			    GTK_WIDGET (menu));
  gtk_widget_show_all (menu);
}


static void
reset_to_defaults (GtkWidget * button, GladeXML * dialog)
{
  gconf_client_set_bool (gconf_client_get_default (),
			 GSWITCHIT_CONFIG_XKB_KEY_OVERRIDE_SETTINGS,
			 TRUE, NULL);
  /* all the rest is g-s-d's business */
}

void
setup_xkb_tabs (GladeXML * dialog, GConfChangeSet * changeset)
{
  XklConfigInit ();
  XklConfigLoadRegistry ();

  fill_models_option_menu (dialog);

  gconf_peditor_new_select_menu
    (changeset, (gchar *) GSWITCHIT_CONFIG_XKB_KEY_MODEL,
     WID ("xkb_models"),
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

  register_layouts_gconf_listener (dialog);
  register_options_gconf_listener (dialog);

  g_signal_connect (G_OBJECT (WID ("keyboard_dialog")),
		    "destroy", G_CALLBACK (cleanup_xkb_tabs), dialog);
}
