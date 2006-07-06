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

#include "libgswitchit/gswitchit-config.h"

#include "capplet-util.h"
#include "gconf-property-editor.h"
#include "activate-settings-daemon.h"
#include "capplet-stock-icons.h"
#include <../accessibility/keyboard/accessibility-keyboard.h>

#include "gnome-keyboard-properties-xkb.h"

static gchar* current_model_name = NULL;

static void
add_model_to_list (XklConfigRegistry * config_registry,
                   XklConfigItem * config_item,
                   GtkTreeView * models_list)
{
  GtkTreeIter iter;
  GtkListStore * list_store = GTK_LIST_STORE (gtk_tree_view_get_model (models_list));
  char *utf_model_name = xci_desc_to_utf8 (config_item);
  gtk_list_store_append( list_store, &iter );
  gtk_list_store_set( list_store, &iter, 
                      0, utf_model_name,
                      1, config_item->name, -1 );

  g_free (utf_model_name);
}

static void
xkb_model_chooser_change_sel (GtkTreeSelection* selection, 
                              GladeXML* chooser_dialog)
{
   gboolean anysel = gtk_tree_selection_get_selected (selection, NULL, NULL);
   gtk_dialog_set_response_sensitive (GTK_DIALOG (CWID ("xkb_model_chooser")),
                                      GTK_RESPONSE_OK, anysel);
}

static void
fill_models_list (GladeXML * chooser_dialog)
{
  GtkWidget* models_list = CWID( "models_list" );
  GtkTreeIter iter;
  GtkTreePath *path;
  GtkCellRenderer* renderer = gtk_cell_renderer_text_new ();
  GtkTreeViewColumn* description_col = gtk_tree_view_column_new_with_attributes (  _("Models"),
                                                                                  renderer,
                                                                                  "text", 0,
                                                                                  NULL);
  GtkListStore *list_store = gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_STRING);
  char *model_name;

  gtk_tree_view_column_set_visible (description_col, TRUE);
  gtk_tree_view_append_column (GTK_TREE_VIEW (models_list), description_col);

  gtk_tree_view_set_model (GTK_TREE_VIEW (models_list), GTK_TREE_MODEL (list_store) );

  gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (list_store),
                                        0, GTK_SORT_ASCENDING);

  xkl_config_registry_foreach_model (config_registry,
                                     (ConfigItemProcessFunc) add_model_to_list,
                                     models_list);

  if (current_model_name != NULL)
  {
    if (gtk_tree_model_get_iter_first (GTK_TREE_MODEL (list_store), &iter))
    {
      do
      {
        gtk_tree_model_get (GTK_TREE_MODEL (list_store), &iter, 
                            1, &model_name, -1);
        if (!g_ascii_strcasecmp(model_name, current_model_name))
        {
          gtk_tree_selection_select_iter (gtk_tree_view_get_selection (GTK_TREE_VIEW (models_list)), &iter);
          path = gtk_tree_model_get_path (GTK_TREE_MODEL (list_store), &iter);
          gtk_tree_view_scroll_to_cell (GTK_TREE_VIEW (models_list),
                                        path, NULL, TRUE, 0.5, 0);
          gtk_tree_path_free (path);
        }
        g_free (model_name);
      } while (gtk_tree_model_iter_next (GTK_TREE_MODEL (list_store), &iter));
    }
  }

  g_signal_connect (G_OBJECT (gtk_tree_view_get_selection (GTK_TREE_VIEW (models_list))), 
                    "changed",
                    G_CALLBACK (xkb_model_chooser_change_sel),
                    chooser_dialog);
}

static void
xkb_model_chooser_response (GtkDialog *dialog,
                            gint response,
                            GladeXML *chooser_dialog)
{
  if (response == GTK_RESPONSE_OK)
    {
      GtkWidget* models_list = CWID( "models_list" );
      GtkTreeSelection* selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (models_list));
      GtkTreeIter iter;
      GtkTreeModel* list_store = NULL;
      if (gtk_tree_selection_get_selected (selection, &list_store, &iter))
        {
          gchar* model_name = NULL;
          gtk_tree_model_get (list_store, &iter, 
                              1, &model_name, -1);

          gconf_client_set_string (xkb_gconf_client,
			           GSWITCHIT_KBD_CONFIG_KEY_MODEL,
			           model_name, NULL);
          g_free(model_name);
        }
    }
}

void
choose_model(GladeXML * dialog)
{
  GladeXML* chooser_dialog = glade_xml_new (GNOMECC_DATA_DIR "/interfaces/gnome-keyboard-properties.glade", "xkb_model_chooser", NULL);
  GtkWidget* chooser = CWID ( "xkb_model_chooser");
  gtk_window_set_transient_for (GTK_WINDOW (chooser), GTK_WINDOW (WID ("keyboard_dialog")));
  current_model_name = gconf_client_get_string (xkb_gconf_client,
			           GSWITCHIT_KBD_CONFIG_KEY_MODEL, NULL);
  fill_models_list (chooser_dialog);
  g_signal_connect (G_OBJECT (chooser),
		    "response", G_CALLBACK (xkb_model_chooser_response), chooser_dialog);
  gtk_dialog_run (GTK_DIALOG (chooser));
  gtk_widget_destroy (chooser);
  g_free (current_model_name);
}
