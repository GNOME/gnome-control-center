/* -*- mode: c; style: linux -*- */

/* gnome-keyboard-properties-xkblt.c
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

#define GROUP_SWITCHERS_GROUP "grp"
#define DEFAULT_GROUP_SWITCH "grp:alts_toggle"

static GtkTreeIter current1stLevelIter;
static const char *current1stLevelId;
static int idx2Select = -1;
static int maxSelectedLayouts = -1;

void
clear_xkb_elements_list (GSList * list)
{
  while (list != NULL)
    {
      GSList *p = list;
      list = list->next;
      g_free (p->data);
      g_slist_free_1 (p);
    }
}

static void
add_variant_to_available_layouts_tree (const XklConfigItemPtr
				       configItem, GladeXML * dialog)
{
  GtkWidget *layoutsTree = WID ("xkb_layouts_available");
  GtkTreeIter iter;
  GtkTreeStore *treeStore =
    GTK_TREE_STORE (gtk_tree_view_get_model (GTK_TREE_VIEW (layoutsTree)));
  const gchar *fullLayoutName = GSwitchItConfigMergeItems (current1stLevelId,
							   configItem->name);
  char *utfVariantName = xci_desc_to_utf8 (configItem);

  gtk_tree_store_append (treeStore, &iter, &current1stLevelIter);
  gtk_tree_store_set (treeStore, &iter, 0, utfVariantName, 1,
		      fullLayoutName, -1);
  g_free (utfVariantName);
}

static void
add_layout_to_available_layouts_tree (const XklConfigItemPtr
				      configItem, GladeXML * dialog)
{
  GtkWidget *layoutsTree = WID ("xkb_layouts_available");
  GtkTreeStore *treeStore =
    GTK_TREE_STORE (gtk_tree_view_get_model (GTK_TREE_VIEW (layoutsTree)));
  char *utfLayoutName = xci_desc_to_utf8 (configItem);

  gtk_tree_store_append (treeStore, &current1stLevelIter, NULL);
  gtk_tree_store_set (treeStore, &current1stLevelIter, 0,
		      utfLayoutName, 1, configItem->name, -1);
  g_free (utfLayoutName);

  current1stLevelId = configItem->name;

  XklConfigEnumLayoutVariants (configItem->name,
			       (ConfigItemProcessFunc)
			       add_variant_to_available_layouts_tree, dialog);
}

static void
enable_disable_layouts_buttons (GladeXML * dialog)
{
  GtkWidget *addLayoutBtn = WID ("xkb_layouts_add");
  GtkWidget *delLayoutBtn = WID ("xkb_layouts_remove");
  GtkWidget *upLayoutBtn = WID ("xkb_layouts_up");
  GtkWidget *dnLayoutBtn = WID ("xkb_layouts_down");
  GtkWidget *availableLayoutsTree = WID ("xkb_layouts_available");
  GtkWidget *selectedLayoutsTree = WID ("xkb_layouts_selected");

  GtkTreeSelection *aSelection =
    gtk_tree_view_get_selection (GTK_TREE_VIEW (availableLayoutsTree));
  const int nSelectedAvailableLayouts =
    gtk_tree_selection_count_selected_rows (aSelection);
  GtkTreeSelection *sSelection =
    gtk_tree_view_get_selection (GTK_TREE_VIEW (selectedLayoutsTree));
  const int nSelectedSelectedLayouts =
    gtk_tree_selection_count_selected_rows (sSelection);
  gboolean canMoveUp = FALSE;
  gboolean canMoveDn = FALSE;
  GtkTreeIter iter;
  GtkTreeModel *selectedLayoutsModel = gtk_tree_view_get_model
    (GTK_TREE_VIEW (selectedLayoutsTree));
  const int nSelectedLayouts =
    gtk_tree_model_iter_n_children (selectedLayoutsModel,
				    NULL);

  gtk_widget_set_sensitive (addLayoutBtn,
			    (nSelectedAvailableLayouts > 0)
			    && (nSelectedLayouts < maxSelectedLayouts));
  gtk_widget_set_sensitive (delLayoutBtn, nSelectedSelectedLayouts > 0);

  if (gtk_tree_selection_get_selected (sSelection, NULL, &iter))
    {
      GtkTreePath *path = gtk_tree_model_get_path (selectedLayoutsModel,
						   &iter);
      if (path != NULL)
	{
	  int *indices = gtk_tree_path_get_indices (path);
	  int idx = indices[0];
	  canMoveUp = idx > 0;
	  canMoveDn = idx < (nSelectedLayouts - 1);
	  gtk_tree_path_free (path);
	}
    }
  gtk_widget_set_sensitive (upLayoutBtn, canMoveUp);
  gtk_widget_set_sensitive (dnLayoutBtn, canMoveDn);
}

void
prepare_selected_layouts_tree (GladeXML * dialog)
{
  GtkListStore *listStore =
    gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_STRING);
  GtkWidget *treeView = WID ("xkb_layouts_selected");
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
			    (enable_disable_layouts_buttons), dialog);
  maxSelectedLayouts = ( XklGetBackendFeatures() & XKLF_MULTIPLE_LAYOUTS_SUPPORTED ) ? XkbNumKbdGroups : 1;
}

void
fill_selected_layouts_tree (GladeXML * dialog)
{
  GSList *layouts = get_selected_layouts_list ();
  GSList *curLayout;
  GtkListStore *listStore =
    GTK_LIST_STORE (gtk_tree_view_get_model
		    (GTK_TREE_VIEW (WID ("xkb_layouts_selected"))));
  gtk_list_store_clear (listStore);

  for (curLayout = layouts; curLayout != NULL; curLayout = curLayout->next)
    {
      GtkTreeIter iter;
      char *l, *sl, *v, *sv;
      char *v1, *utfVisible;
      const char *visible = (char *) curLayout->data;
      gtk_list_store_append (listStore, &iter);
      if (GSwitchItConfigGetDescriptions (visible, &sl, &l, &sv, &v))
	visible = GSwitchItConfigFormatFullLayout (l, v);
      v1 = g_strdup (visible);
      utfVisible = g_locale_to_utf8 (g_strstrip (v1), -1, NULL, NULL, NULL);
      gtk_list_store_set (listStore, &iter,
			  0, utfVisible, 1, curLayout->data, -1);
      g_free (utfVisible);
      g_free (v1);
    }

  clear_xkb_elements_list (layouts);
  enable_disable_layouts_buttons (dialog);
  if (idx2Select != -1)
    {
      GtkTreeSelection *selection =
	gtk_tree_view_get_selection ((GTK_TREE_VIEW
				      (WID ("xkb_layouts_selected"))));
      GtkTreePath *path = gtk_tree_path_new_from_indices (idx2Select, -1);
      gtk_tree_selection_select_path (selection, path);
      gtk_tree_path_free (path);
      idx2Select = -1;
    }
}

void
sort_tree_content (GtkWidget * treeView)
{
  GtkTreeModel *treeModel =
    gtk_tree_view_get_model (GTK_TREE_VIEW (treeView));
  GtkTreeModel *sortedTreeModel;
  /* replace the store with the sorted version */
  sortedTreeModel = gtk_tree_model_sort_new_with_model (treeModel);
  gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE
					(sortedTreeModel), 0,
					GTK_SORT_ASCENDING);
  gtk_tree_view_set_model (GTK_TREE_VIEW (treeView), sortedTreeModel);
}

void
fill_available_layouts_tree (GladeXML * dialog)
{
  GtkTreeStore *treeStore =
    gtk_tree_store_new (2, G_TYPE_STRING, G_TYPE_STRING);
  GtkWidget *treeView = WID ("xkb_layouts_available");
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

  XklConfigEnumLayouts ((ConfigItemProcessFunc)
			add_layout_to_available_layouts_tree, dialog);

  sort_tree_content (treeView);
  g_signal_connect_swapped (G_OBJECT (selection), "changed",
			    G_CALLBACK
			    (enable_disable_layouts_buttons), dialog);
}

static void
add_selected_layout (GtkWidget * button, GladeXML * dialog)
{
  GtkTreeSelection *selection =
    gtk_tree_view_get_selection (GTK_TREE_VIEW
				 (WID ("xkb_layouts_available")));
  GtkTreeIter selectedIter;
  GtkTreeModel *model;
  if (gtk_tree_selection_get_selected (selection, &model, &selectedIter))
    {
      gchar *id;
      GSList *layoutsList = get_selected_layouts_list ();
      gtk_tree_model_get (model, &selectedIter, 1, &id, -1);
      layoutsList = g_slist_append (layoutsList, id);
      set_selected_layouts_list (layoutsList);
      // process default switcher
      if (g_slist_length(layoutsList) >= 2)
        {
          GSList *optionsList = get_selected_options_list ();
          gboolean anySwitcher = False;
          GSList *option = optionsList;
          while (option != NULL)
            {
              char *g, *o;
              if (GSwitchItConfigSplitItems (option->data, &g, &o))
                {
                  if (!g_ascii_strcasecmp (g, GROUP_SWITCHERS_GROUP))
                    {
                      anySwitcher = True;
                      break;
                    }
                }
              option = option->next;
            }
          if (!anySwitcher)
            {
              XklConfigItem ci;
              g_snprintf( ci.name, XKL_MAX_CI_NAME_LENGTH, DEFAULT_GROUP_SWITCH );           
              if (XklConfigFindOption( GROUP_SWITCHERS_GROUP,
                                       &ci ))

                {
                  const gchar* id = GSwitchItConfigMergeItems (GROUP_SWITCHERS_GROUP, DEFAULT_GROUP_SWITCH);
                  optionsList = g_slist_append (optionsList, g_strdup (id));
                  set_selected_options_list (optionsList);
                }
            }
          clear_xkb_elements_list (optionsList);
        }
      clear_xkb_elements_list (layoutsList);
    }
}

static void
move_selected_layout (GladeXML * dialog, int offset)
{
  GtkTreeSelection *selection =
    gtk_tree_view_get_selection (GTK_TREE_VIEW
				 (WID ("xkb_layouts_selected")));
  GtkTreeIter selectedIter;
  GtkTreeModel *model;
  if (gtk_tree_selection_get_selected (selection, &model, &selectedIter))
    {
      GSList *layoutsList = get_selected_layouts_list ();
      GtkTreePath *path = gtk_tree_model_get_path (model,
						   &selectedIter);
      if (path != NULL)
	{
	  int *indices = gtk_tree_path_get_indices (path);
	  char *id = NULL;
	  GSList *node2Remove = g_slist_nth (layoutsList, indices[0]);

	  layoutsList = g_slist_remove_link (layoutsList, node2Remove);

	  id = (char *) node2Remove->data;
	  g_slist_free_1 (node2Remove);

	  if (offset == 0)
	    g_free (id);
	  else
	    {
	      layoutsList =
		g_slist_insert (layoutsList, id, indices[0] + offset);
	      idx2Select = indices[0] + offset;
	    }

	  set_selected_layouts_list (layoutsList);
	  gtk_tree_path_free (path);
	}
      clear_xkb_elements_list (layoutsList);
    }
}

static void
remove_selected_layout (GtkWidget * button, GladeXML * dialog)
{
  move_selected_layout (dialog, 0);
}

static void
up_selected_layout (GtkWidget * button, GladeXML * dialog)
{
  move_selected_layout (dialog, -1);
}

static void
down_selected_layout (GtkWidget * button, GladeXML * dialog)
{
  move_selected_layout (dialog, +1);
}

void
register_layouts_buttons_handlers (GladeXML * dialog)
{
  g_signal_connect (G_OBJECT (WID ("xkb_layouts_add")), "clicked",
		    G_CALLBACK (add_selected_layout), dialog);
  g_signal_connect (G_OBJECT (WID ("xkb_layouts_remove")), "clicked",
		    G_CALLBACK (remove_selected_layout), dialog);
  g_signal_connect (G_OBJECT (WID ("xkb_layouts_up")), "clicked",
		    G_CALLBACK (up_selected_layout), dialog);
  g_signal_connect (G_OBJECT (WID ("xkb_layouts_down")), "clicked",
		    G_CALLBACK (down_selected_layout), dialog);
}

static void
update_layouts_list (GConfClient * client,
		     guint cnxn_id, GConfEntry * entry, GladeXML * dialog)
{
  fill_selected_layouts_tree (dialog);
  enable_disable_restoring (dialog);
}

void
register_layouts_gconf_listener (GladeXML * dialog)
{
  gconf_client_notify_add (gconf_client_get_default (),
			   GSWITCHIT_CONFIG_XKB_KEY_LAYOUTS,
			   (GConfClientNotifyFunc)
			   update_layouts_list, dialog, NULL, NULL);
}
