/* -*- mode: c; style: linux -*- */

/* gnome-keyboard-properties-xkbot.c
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

#include "libgswitchit/gswitchit_config.h"

#include "capplet-util.h"
#include "gconf-property-editor.h"
#include "activate-settings-daemon.h"
#include "capplet-stock-icons.h"
#include <../accessibility/keyboard/accessibility-keyboard.h>

#include "gnome-keyboard-properties-xkb.h"

static GtkTreeIter current1stLevelIter;
static const char *current1stLevelId;

static gboolean
can_add_option (GladeXML * dialog)
{
  GtkWidget *availableOptionsTree = WID ("xkb_options_available");
  GtkWidget *selectedOptionsTree = WID ("xkb_options_selected");
  GtkTreeSelection *aSelection =
    gtk_tree_view_get_selection (GTK_TREE_VIEW (availableOptionsTree));
  GtkTreeIter aiter, siter, groupIter;
  GtkTreeModel *availableOptionsModel, *selectedOptionsModel;
  GtkTreePath *path, *groupPath;
  char *selectedOptionId = NULL, *selectedGroupId =
    NULL, *selectedFullOptionId = NULL;
  gboolean retval = FALSE, multipleAllowed = TRUE;
  int depth;

  if (!gtk_tree_selection_get_selected
      (aSelection, &availableOptionsModel, &aiter))
    return FALSE;

  path = gtk_tree_model_get_path (availableOptionsModel, &aiter);
  if (path == NULL)
    return FALSE;

  depth = gtk_tree_path_get_depth (path);

  if (depth != 2)
    {
      gtk_tree_path_free (path);
      return FALSE;
    }

  if (!gtk_tree_model_iter_parent (availableOptionsModel, &groupIter, &aiter))
    {
      gtk_tree_path_free (path);
      return FALSE;
    }
  groupPath = gtk_tree_model_get_path (availableOptionsModel, &groupIter);
  if (groupPath == NULL)
    {
      gtk_tree_path_free (path);
      return FALSE;
    }

  gtk_tree_model_get (availableOptionsModel, &groupIter, 2,
		      &multipleAllowed, -1);

  gtk_tree_model_get (availableOptionsModel, &aiter, 1,
		      &selectedFullOptionId, -1);

  if (!GSwitchItKbdConfigSplitItems
      (selectedFullOptionId, &selectedGroupId, &selectedOptionId))
    {
      gtk_tree_path_free (groupPath);
      gtk_tree_path_free (path);
      return FALSE;
    }
  selectedGroupId = g_strdup (selectedGroupId);
  selectedOptionId = g_strdup (selectedOptionId);

  selectedOptionsModel =
    gtk_tree_view_get_model (GTK_TREE_VIEW (selectedOptionsTree));

  retval = TRUE;

  if (gtk_tree_model_get_iter_first (selectedOptionsModel, &siter))
    {
      do
	{
	  char *sid = NULL;
	  gtk_tree_model_get (selectedOptionsModel, &siter, 1, &sid, -1);
	  if (multipleAllowed)
	    {
	      // look for the _same_ option - and do not allow it twice
	      if (!g_strcasecmp (sid, selectedFullOptionId))
		{
		  retval = FALSE;
		}
	    }
	  else
	    {
	      // look for options within same group
	      char *sgid = NULL, *soid = NULL;
	      gtk_tree_model_get (selectedOptionsModel, &siter, 1, &sid, -1);
	      if (GSwitchItKbdConfigSplitItems
		  (sid, &sgid, &soid)
		  && !g_strcasecmp (sgid, selectedGroupId))
		{
		  retval = FALSE;
		}
	    }
	  g_free (sid);
	}
      while (retval && gtk_tree_model_iter_next
	     (selectedOptionsModel, &siter));
    }

  g_free (selectedFullOptionId);
  g_free (selectedGroupId);
  g_free (selectedOptionId);

  gtk_tree_path_free (groupPath);
  gtk_tree_path_free (path);
  return retval;
}

static void
xkb_options_enable_disable_buttons (GladeXML * dialog)
{
  GtkWidget *addOptionBtn = WID ("xkb_options_add");
  GtkWidget *delOptionBtn = WID ("xkb_options_remove");
  GtkWidget *selectedOptionsTree = WID ("xkb_options_selected");

  GtkTreeSelection *sSelection =
    gtk_tree_view_get_selection (GTK_TREE_VIEW (selectedOptionsTree));
  const int nSelectedSelectedOptions =
    gtk_tree_selection_count_selected_rows (sSelection);

  gtk_widget_set_sensitive (addOptionBtn, can_add_option (dialog));
  gtk_widget_set_sensitive (delOptionBtn, nSelectedSelectedOptions > 0);
}

void
xkb_options_prepare_selected_tree (GladeXML * dialog)
{
  GtkListStore *listStore =
    gtk_list_store_new (3, G_TYPE_STRING, G_TYPE_STRING,
			G_TYPE_STRING);
  GtkWidget *treeView = WID ("xkb_options_selected");
  GtkCellRenderer *renderer =
    GTK_CELL_RENDERER (gtk_cell_renderer_text_new ());
  GtkTreeViewColumn *column = gtk_tree_view_column_new_with_attributes (NULL,
									renderer,
									"text",
									0,
									NULL);
  GtkTreeSelection *selection =
    gtk_tree_view_get_selection (GTK_TREE_VIEW (treeView));
  gtk_tree_view_set_model (GTK_TREE_VIEW (treeView),
			   GTK_TREE_MODEL (listStore));
  gtk_tree_view_append_column (GTK_TREE_VIEW (treeView), column);
  g_signal_connect_swapped (G_OBJECT (selection), "changed",
			    G_CALLBACK
			    (xkb_options_enable_disable_buttons), dialog);
}

static void
xkb_options_add_selected (GtkWidget * button, GladeXML * dialog)
{
  GtkTreeSelection *selection =
    gtk_tree_view_get_selection (GTK_TREE_VIEW
				 (WID ("xkb_options_available")));
  GtkTreeIter selectedIter;
  GtkTreeModel *model;
  if (gtk_tree_selection_get_selected (selection, &model, &selectedIter))
    {
      gchar *id;
      GSList *optionsList = xkb_options_get_selected_list ();
      gtk_tree_model_get (model, &selectedIter, 1, &id, -1);
      optionsList = g_slist_append (optionsList, id);
      xkb_options_set_selected_list (optionsList);
      clear_xkb_elements_list (optionsList);
    }
}

static void
xkb_options_remove_selected (GtkWidget * button, GladeXML * dialog)
{
  GtkTreeSelection *selection =
    gtk_tree_view_get_selection (GTK_TREE_VIEW
				 (WID ("xkb_options_selected")));
  GtkTreeIter selectedIter;
  GtkTreeModel *model;
  if (gtk_tree_selection_get_selected (selection, &model, &selectedIter))
    {
      GSList *optionsList = xkb_options_get_selected_list ();
      GtkTreePath *path = gtk_tree_model_get_path (model,
						   &selectedIter);
      if (path != NULL)
	{
	  int *indices = gtk_tree_path_get_indices (path);
	  char *id = NULL;
	  GSList *node2Remove = g_slist_nth (optionsList, indices[0]);

	  optionsList = g_slist_remove_link (optionsList, node2Remove);

	  id = (char *) node2Remove->data;
	  g_slist_free_1 (node2Remove);

	  g_free (id);

	  xkb_options_set_selected_list (optionsList);
	  gtk_tree_path_free (path);
	}
      clear_xkb_elements_list (optionsList);
    }
}

static void
xkb_options_add_option_to_available_tree (const XklConfigItemPtr
                                          configItem, GladeXML * dialog)
{
  GtkWidget *optionsTree = WID ("xkb_options_available");
  GtkTreeIter iter;
  GtkTreeStore *treeStore =
    GTK_TREE_STORE (gtk_tree_view_get_model (GTK_TREE_VIEW (optionsTree)));
  const gchar *fullOptionName = GSwitchItKbdConfigMergeItems (current1stLevelId,
							   configItem->name);
  char *utfOptionName = xci_desc_to_utf8 (configItem);

  gtk_tree_store_append (treeStore, &iter, &current1stLevelIter);
  gtk_tree_store_set (treeStore, &iter, 0, utfOptionName, 1,
		      fullOptionName, -1);
  g_free (utfOptionName);
}

static void
xkb_options_add_group_to_available_tree (const XklConfigItemPtr
                                         configItem,
                                         Bool allowMultipleSelection,
                                         GladeXML * dialog)
{
  GtkWidget *optionsTree = WID ("xkb_options_available");
  GtkTreeStore *treeStore =
    GTK_TREE_STORE (gtk_tree_view_get_model (GTK_TREE_VIEW (optionsTree)));
  char *utfGroupName = xci_desc_to_utf8 (configItem);

  gtk_tree_store_append (treeStore, &current1stLevelIter, NULL);
  gtk_tree_store_set (treeStore, &current1stLevelIter, 0,
		      utfGroupName, 1, configItem->name, 2,
		      (gboolean) allowMultipleSelection, -1);
  g_free (utfGroupName);

  current1stLevelId = configItem->name;

  XklConfigEnumOptions (configItem->name, (ConfigItemProcessFunc)
			xkb_options_add_option_to_available_tree, dialog);
}

void
xkb_options_fill_available_tree (GladeXML * dialog)
{
  GtkTreeStore *treeStore =
    gtk_tree_store_new (3, G_TYPE_STRING, G_TYPE_STRING,
			G_TYPE_BOOLEAN);
  GtkWidget *treeView = WID ("xkb_options_available");
  GtkCellRenderer *renderer =
    GTK_CELL_RENDERER (gtk_cell_renderer_text_new ());
  GtkTreeViewColumn *column = gtk_tree_view_column_new_with_attributes (NULL,
									renderer,
									"text",
									0,
									NULL);
  GtkTreeSelection *selection =
    gtk_tree_view_get_selection (GTK_TREE_VIEW (treeView));

  gtk_tree_view_set_model (GTK_TREE_VIEW (treeView),
			   GTK_TREE_MODEL (treeStore));
  gtk_tree_view_append_column (GTK_TREE_VIEW (treeView), column);

  XklConfigEnumOptionGroups ((GroupProcessFunc)
			     xkb_options_add_group_to_available_tree, dialog);

  sort_tree_content (treeView);

  g_signal_connect_swapped (G_OBJECT (selection), "changed",
			    G_CALLBACK
			    (xkb_options_enable_disable_buttons), dialog);
}

void
xkb_options_fill_selected_tree (GladeXML * dialog)
{
  GSList *options = xkb_options_get_selected_list ();
  GSList *curOption;
  GtkListStore *listStore =
    GTK_LIST_STORE (gtk_tree_view_get_model
		    (GTK_TREE_VIEW (WID ("xkb_options_selected"))));
  gtk_list_store_clear (listStore);

  for (curOption = options; curOption != NULL; curOption = curOption->next)
    {
      GtkTreeIter iter;
      char *groupName, *optionName;
      const char *visible = (char *) curOption->data;

      if (GSwitchItKbdConfigSplitItems (visible, &groupName, &optionName))
	{
	  XklConfigItem citem;
	  char *v1, *utfVisible;
	  g_snprintf (citem.name, sizeof (citem.name), "%s", optionName);
	  if (XklConfigFindOption (groupName, &citem))
	    {
	      visible = citem.description;
	    }
	  v1 = g_strdup (visible);
	  utfVisible =
	    g_locale_to_utf8 (g_strstrip (v1), -1, NULL, NULL, NULL);
	  gtk_list_store_append (listStore, &iter);
	  gtk_list_store_set (listStore, &iter,
			      0, utfVisible, 1, curOption->data, -1);
	  g_free (utfVisible);
	  g_free (v1);
	}

    }

  clear_xkb_elements_list (options);
  xkb_options_enable_disable_buttons (dialog);
}

void
xkb_options_register_buttons_handlers (GladeXML * dialog)
{
  g_signal_connect (G_OBJECT (WID ("xkb_options_add")), "clicked",
		    G_CALLBACK (xkb_options_add_selected), dialog);
  g_signal_connect (G_OBJECT (WID ("xkb_options_remove")), "clicked",
		    G_CALLBACK (xkb_options_remove_selected), dialog);
}
static void
xkb_options_update_list (GConfClient * client,
                         guint cnxn_id, GConfEntry * entry, GladeXML * dialog)
{
  xkb_options_fill_selected_tree (dialog);
  enable_disable_restoring (dialog);
}

void
xkb_options_register_gconf_listener (GladeXML * dialog)
{
  gconf_client_notify_add (xkbGConfClient,
			   GSWITCHIT_KBD_CONFIG_KEY_OPTIONS,
			   (GConfClientNotifyFunc)
			   xkb_options_update_list, dialog, NULL, NULL);
}
