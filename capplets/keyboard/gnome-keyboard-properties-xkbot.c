/* -*- mode: c; style: linux -*- */

/* gnome-keyboard-properties-xkbot.c
 * Copyright (C) 2003 Sergey V. Oudaltsov
 *
 * Written by: Sergey V. Oudaltsov <svu@users.sourceforge.net>
 *             John Spray <spray_john@users.sourceforge.net>
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

#include "libgswitchit/gswitchit_config.h"

#include "capplet-util.h"
#include "gconf-property-editor.h"
#include "activate-settings-daemon.h"
#include "capplet-stock-icons.h"
#include <../accessibility/keyboard/accessibility-keyboard.h>

#include "gnome-keyboard-properties-xkb.h"

static const char *current1stLevelId = NULL;
static GtkWidget *currentVbox = NULL;
static GtkWidget *currentNoneRadio = NULL;
static gboolean currentMultiSelect = FALSE;
static GSList *currentRadioGroup = NULL;

#define OPTION_ID_PROP "optionID"
#define SELCOUNTER_PROP "selectionCounter"
#define GCONFSTATE_PROP "gconfState"
#define EXPANDERS_PROP "expandersList"

static GtkWidget *
xkb_options_get_expander (GtkWidget * option_button)
{
  return gtk_widget_get_parent (
           gtk_widget_get_parent (
           gtk_widget_get_parent (option_button)));
}

static int
xkb_options_expander_selcounter_get (GtkWidget * expander)
{
  return GPOINTER_TO_INT(g_object_get_data (G_OBJECT (expander), SELCOUNTER_PROP));
}

static void
xkb_options_expander_selcounter_add (GtkWidget * expander, int value)
{
  g_object_set_data (G_OBJECT (expander), SELCOUNTER_PROP,
    GINT_TO_POINTER (xkb_options_expander_selcounter_get (expander) + value));
}

static void
xkb_options_expander_highlight (GtkWidget * expander)
{
  char * utfGroupName = g_object_get_data (G_OBJECT (expander), "utfGroupName");
  int counter = xkb_options_expander_selcounter_get (expander);
  gchar *titlemarkup = g_strconcat (counter > 0 ? "<span weight=\"bold\">" : "<span>", 
                                    utfGroupName, "</span>", NULL);
  gtk_expander_set_label (GTK_EXPANDER (expander), titlemarkup);
  g_free (titlemarkup);
}

/* Add optionname from the backend's selection list if it's not
   already in there. */
static void
xkb_options_select (gchar *optionname)
{
  gboolean already_selected = FALSE;
  GSList *optionsList = xkb_options_get_selected_list ();
  GSList *option;
  for (option = optionsList ; option != NULL ; option = option->next)
    if (!strcmp ((gchar*)option->data, optionname))
      already_selected = TRUE;

  if (!already_selected)
    optionsList = g_slist_append (optionsList, g_strdup (optionname));
  xkb_options_set_selected_list (optionsList);

  clear_xkb_elements_list (optionsList);
}

/* Remove all occurences of optionname from the backend's selection list */
static void
xkb_options_deselect (gchar *optionname)
{
  GSList *optionsList = xkb_options_get_selected_list ();
  GSList *nodetmp;
  GSList *option = optionsList;
  while (option != NULL)
    {
      gchar *id = (char *) option->data;
      if (!strcmp(id, optionname))
        {
          nodetmp = option->next;
          g_free (id);
          optionsList = g_slist_remove_link (optionsList, option);
          g_slist_free_1 (option);
          option=nodetmp;
        }
      else
        option = option->next;
    }
  xkb_options_set_selected_list (optionsList);
  clear_xkb_elements_list (optionsList);
}

/* Return true if optionname describes a string already in the backend's
   list of selected options */
static gboolean
xkb_options_is_selected (gchar *optionname)
{
  gboolean retval = FALSE;
  GSList *optionsList = xkb_options_get_selected_list ();
  GSList *option;
  for (option = optionsList ; option != NULL ; option = option->next)
    {
      if (!strcmp ((gchar*)option->data, optionname))
        retval = TRUE;
    }
  clear_xkb_elements_list (optionsList);
  return retval;
}

/* Update xkb backend to reflect the new UI state */
static void
option_toggled_cb (GtkWidget *checkbutton, gpointer data)
{
  gpointer optionID = g_object_get_data (G_OBJECT (checkbutton), OPTION_ID_PROP);
  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (checkbutton)))
      xkb_options_select (optionID);
  else
      xkb_options_deselect (optionID);
}

/* Update UI state from xkb backend */
static void
option_update_cb (GConfClient * client,
                         guint cnxn_id, GConfEntry * entry, gpointer data)
{
  GtkToggleButton *toggle = GTK_TOGGLE_BUTTON (data);
  GtkWidget *expander = xkb_options_get_expander (GTK_WIDGET (toggle));
  gboolean old_state = gtk_toggle_button_get_active (toggle);
  gboolean new_state = xkb_options_is_selected (
                         g_object_get_data (G_OBJECT (toggle), OPTION_ID_PROP));
  int old_gstate = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (toggle), GCONFSTATE_PROP));
  int state_diff = new_state - old_gstate;

  if (GTK_WIDGET_TYPE (toggle) == GTK_TYPE_RADIO_BUTTON &&
      old_state == TRUE && new_state == FALSE)
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (g_object_get_data (G_OBJECT (toggle), "NoneRadio")), TRUE);
  else
    gtk_toggle_button_set_active (toggle, new_state);

  g_object_set_data (G_OBJECT (toggle), GCONFSTATE_PROP, GINT_TO_POINTER (new_state));
  xkb_options_expander_selcounter_add (expander, state_diff);
  xkb_options_expander_highlight (expander);
}

/* Add a check_button or radio_button to control a particular option
   This function makes particular use of the current... variables at
   the top of this file. */
static void
xkb_options_add_option (const XklConfigItemPtr
                                          configItem, GladeXML * dialog)
{
  GtkWidget *option_check;
  gchar *utfOptionName = xci_desc_to_utf8 (configItem);
  /* Copy this out because we'll load it into the widget with set_data */
  gchar *fullOptionName = g_strdup(
    GSwitchItKbdConfigMergeItems (current1stLevelId, configItem->name));
  gboolean initial_state;

  if (currentMultiSelect)
    option_check = gtk_check_button_new_with_label (utfOptionName);
  else
    {
      if (currentRadioGroup == NULL)
        {
          /* The first radio in a group is to be "Default", meaning none of
             the below options are to be included in the selected list.
             This is a HIG-compliant alternative to allowing no
             selection in the group. */
          option_check = gtk_radio_button_new_with_label (currentRadioGroup, _("Default"));
          gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (option_check), TRUE);
          gtk_box_pack_start_defaults (GTK_BOX (currentVbox), option_check);
          currentRadioGroup = gtk_radio_button_get_group (GTK_RADIO_BUTTON (option_check));
          currentNoneRadio = option_check;
        }
      option_check = gtk_radio_button_new_with_label (currentRadioGroup, utfOptionName);
      currentRadioGroup = gtk_radio_button_get_group (GTK_RADIO_BUTTON (option_check));
      g_object_set_data (G_OBJECT (option_check), "NoneRadio", currentNoneRadio);
    }
  g_free (utfOptionName);

  initial_state = xkb_options_is_selected (fullOptionName);

  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (option_check), initial_state);

  g_object_set_data_full (G_OBJECT (option_check), OPTION_ID_PROP, fullOptionName, g_free);

  g_signal_connect (G_OBJECT (option_check), "toggled", G_CALLBACK (option_toggled_cb), NULL);

  gconf_client_notify_add (xkbGConfClient,
			   GSWITCHIT_KBD_CONFIG_KEY_OPTIONS,
			   (GConfClientNotifyFunc)
			   option_update_cb, option_check, NULL, NULL);

  gtk_box_pack_start_defaults (GTK_BOX (currentVbox), option_check);

  xkb_options_expander_selcounter_add (xkb_options_get_expander (option_check), initial_state);
  g_object_set_data (G_OBJECT (option_check), GCONFSTATE_PROP, GINT_TO_POINTER (initial_state));
}

/* Add a group of options: create title and layout widgets and then
   add widgets for all the options in the group. */
static void
xkb_options_add_group (const XklConfigItemPtr
                                         configItem,
                                         Bool allowMultipleSelection,
                                         GladeXML * dialog)
{
  GtkWidget *expander, *align, *vbox;

  GSList * expanders_list = g_object_get_data (G_OBJECT (dialog), EXPANDERS_PROP);

  gchar *utfGroupName = xci_desc_to_utf8 (configItem);
  gchar *titlemarkup = g_strconcat ("<span>", utfGroupName, "</span>", NULL);

  expander = gtk_expander_new (titlemarkup);
  g_object_set_data_full (G_OBJECT (expander), "utfGroupName", utfGroupName, g_free);

  g_free (titlemarkup);
  gtk_expander_set_use_markup (GTK_EXPANDER (expander), TRUE);
  align = gtk_alignment_new (0, 0, 1, 1);
  gtk_alignment_set_padding (GTK_ALIGNMENT (align), 6, 12, 12, 0);
  vbox = gtk_vbox_new (TRUE, 6);
  gtk_container_add (GTK_CONTAINER (align), vbox);
  gtk_container_add (GTK_CONTAINER (expander), align);
  currentVbox = vbox;

  currentMultiSelect = (gboolean) allowMultipleSelection;
  currentRadioGroup = NULL;

  current1stLevelId = configItem->name;
  XklConfigEnumOptions (configItem->name, (ConfigItemProcessFunc)
                        xkb_options_add_option, dialog);

  xkb_options_expander_highlight (expander);

  expanders_list = g_slist_append (expanders_list, expander);
  g_object_set_data (G_OBJECT (dialog), EXPANDERS_PROP, expanders_list);
}

static gint
xkb_options_expanders_compare (GtkWidget * expander1, GtkWidget * expander2)
{
  const gchar *t1 = gtk_expander_get_label (GTK_EXPANDER (expander1));
  const gchar *t2 = gtk_expander_get_label (GTK_EXPANDER (expander2));
  return g_utf8_collate (t1, t2);
}

/* Create widgets to represent the options made available by the backend */
void
xkb_options_load_options (GladeXML * dialog)
{
  GtkWidget *opts_vbox = WID ("options_vbox");
  GSList * expanders_list;
  GtkWidget * expander;

  /* fill the list */
  XklConfigEnumOptionGroups ((GroupProcessFunc)
			     xkb_options_add_group, dialog);
  /* sort it */
  expanders_list = g_object_get_data (G_OBJECT (dialog), EXPANDERS_PROP);
  expanders_list = g_slist_sort (expanders_list, (GCompareFunc)xkb_options_expanders_compare);
  while (expanders_list)
  {
    expander = GTK_WIDGET (expanders_list->data);
    gtk_box_pack_start (GTK_BOX (opts_vbox), expander, FALSE, FALSE, 0);
    expanders_list = expanders_list->next;
  }

  /* just cleanup */
  expanders_list = g_object_get_data (G_OBJECT (dialog), EXPANDERS_PROP);
  g_object_set_data (G_OBJECT (dialog), EXPANDERS_PROP, NULL);
  g_slist_free (expanders_list);

  gtk_widget_show_all (opts_vbox);
}

/* Respond to a change in the xkb gconf settings */
static void
xkb_options_update (GConfClient * client,
                    guint cnxn_id, GConfEntry * entry, GladeXML * dialog)
{
  /* Updating options is handled by gconf notifies for each widget
     This is here to avoid calling it N_OPTIONS times for each gconf
     change.*/
  enable_disable_restoring (dialog);
}

void
xkb_options_register_gconf_listener (GladeXML * dialog)
{
  gconf_client_notify_add (xkbGConfClient,
			   GSWITCHIT_KBD_CONFIG_KEY_OPTIONS,
			   (GConfClientNotifyFunc)
			   xkb_options_update, dialog, NULL, NULL);
}
