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

static gchar* currentModelName = NULL;

static void
add_model_to_list (const XklConfigItemPtr configItem, GtkTreeView * modelsList)
{
  GtkTreeIter iter;
  GtkListStore * listStore = GTK_LIST_STORE (gtk_tree_view_get_model (modelsList));
  char *utfModelName = xci_desc_to_utf8 (configItem);
  gtk_list_store_append( listStore, &iter );
  gtk_list_store_set( listStore, &iter, 
                      0, utfModelName,
                      1, configItem->name, -1 );

  if (currentModelName != NULL &&
      !g_ascii_strcasecmp(configItem->name, currentModelName))
    {
       gtk_tree_selection_select_iter (gtk_tree_view_get_selection (modelsList), &iter);
    }
  g_free (utfModelName);
}

static void
xkb_model_chooser_change_sel (GtkTreeSelection* selection, 
                              GladeXML* chooserDialog)
{
   gboolean anysel = gtk_tree_selection_get_selected (selection, NULL, NULL);
   gtk_dialog_set_response_sensitive (GTK_DIALOG (CWID ("xkb_model_chooser")),
                                      GTK_RESPONSE_OK, anysel);
}

static void
fill_models_list (GladeXML * chooserDialog)
{
  GtkWidget* modelsList = CWID( "models_list" );
  GtkCellRenderer* renderer = gtk_cell_renderer_text_new ();
  GtkTreeViewColumn* descriptionCol = gtk_tree_view_column_new_with_attributes (  _("Models"),
                                                                                  renderer,
                                                                                  "text", 0,
                                                                                  NULL);
  GtkListStore *listStore = gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_STRING);

  gtk_tree_view_column_set_visible (descriptionCol, TRUE);
  gtk_tree_view_append_column (GTK_TREE_VIEW (modelsList), descriptionCol);

  gtk_tree_view_set_model (GTK_TREE_VIEW (modelsList), GTK_TREE_MODEL (listStore) );

  XklConfigEnumModels ((ConfigItemProcessFunc)
		       add_model_to_list, modelsList);

  g_signal_connect (G_OBJECT (gtk_tree_view_get_selection (GTK_TREE_VIEW (modelsList))), 
                    "changed",
                    G_CALLBACK (xkb_model_chooser_change_sel),
                    chooserDialog);
}

static void
xkb_model_chooser_response (GtkDialog *dialog,
                            gint response,
                            GladeXML *chooserDialog)
{
  if (response == GTK_RESPONSE_OK)
    {
      GtkWidget* modelsList = CWID( "models_list" );
      GtkTreeSelection* selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (modelsList));
      GtkTreeIter iter;
      GtkTreeModel* listStore = NULL;
      if (gtk_tree_selection_get_selected (selection, &listStore, &iter))
        {
          gchar* modelName = NULL;
          gtk_tree_model_get (listStore, &iter, 
                              1, &modelName, -1);

          gconf_client_set_string (gconf_client_get_default (),
			           GSWITCHIT_CONFIG_XKB_KEY_MODEL,
			           modelName, NULL);
          g_free(modelName);
        }
    }
}

void
choose_model(GladeXML * dialog)
{
  GladeXML* chooserDialog = glade_xml_new (GNOMECC_DATA_DIR "/interfaces/gnome-keyboard-properties.glade", "xkb_model_chooser", NULL);
  GtkWidget* chooser = CWID ( "xkb_model_chooser");
  gtk_window_set_transient_for (GTK_WINDOW (chooser), GTK_WINDOW (WID ("keyboard_dialog")));
  currentModelName = gconf_client_get_string (gconf_client_get_default (),
			           GSWITCHIT_CONFIG_XKB_KEY_MODEL, NULL);
  fill_models_list (chooserDialog);
  g_signal_connect (G_OBJECT (chooser),
		    "response", G_CALLBACK (xkb_model_chooser_response), chooserDialog);
  gtk_dialog_run (GTK_DIALOG (chooser));
  gtk_widget_destroy (chooser);
  g_free (currentModelName);
}
