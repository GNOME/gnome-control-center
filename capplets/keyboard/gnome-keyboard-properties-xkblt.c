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

#include "libgswitchit/gswitchit_config.h"

#include "capplet-util.h"
#include "gconf-property-editor.h"
#include "activate-settings-daemon.h"
#include "capplet-stock-icons.h"
#include <../accessibility/keyboard/accessibility-keyboard.h>

#include "gnome-keyboard-properties-xkb.h"
#include "libkbdraw/keyboard-drawing.h"

#define GROUP_SWITCHERS_GROUP "grp"
#define DEFAULT_GROUP_SWITCH "grp:alts_toggle"

#define SLT_COL_DESCRIPTION 0
#define SLT_COL_DEFAULT 1
#define SLT_COL_ID 2

#define ALT_COL_DESCRIPTION 0
#define ALT_COL_ID 1

#define CWID(s) glade_xml_get_widget (chooserDialog, s)

#ifdef HAVE_X11_EXTENSIONS_XKB_H
#include "X11/XKBlib.h"
/**
 * BAD STYLE: Taken from xklavier_private_xkb.h
 * Any ideas on architectural improvements are WELCOME
 */
extern Bool _XklXkbConfigPrepareNative( const XklConfigRecPtr data, XkbComponentNamesPtr componentNamesPtr );
extern void _XklXkbConfigCleanupNative( XkbComponentNamesPtr componentNamesPtr );
/* */
#endif

static GtkTreeIter current1stLevelIter;
static const char *current1stLevelId;

static int idx2Select = -1;
static int maxSelectedLayouts = -1;
static int defaultGroup = -1;

static GtkCellRenderer *textRenderer;
static GtkCellRenderer *toggleRenderer;

static GtkWidget* kbdraw;

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
save_default_group (int aDefaultGroup)
{
  if (aDefaultGroup != gconf_client_get_int (xkbGConfClient, 
                                             GSWITCHIT_CONFIG_KEY_DEFAULT_GROUP, 
                                             NULL))
    gconf_client_set_int (xkbGConfClient, 
                          GSWITCHIT_CONFIG_KEY_DEFAULT_GROUP, 
                          aDefaultGroup, 
                          NULL);
}

static void
def_group_in_ui_changed (GtkCellRendererToggle *cell_renderer,
                         gchar *path, 
                         GladeXML * dialog)
{
  GtkTreePath *chpath = gtk_tree_path_new_from_string (path);
  int newDefaultGroup = -1;
  gboolean previouslySelected = gtk_cell_renderer_toggle_get_active (cell_renderer);

  if (!previouslySelected) /* prev state - non-selected! */
  {
    int *indices = gtk_tree_path_get_indices (chpath);
    newDefaultGroup = indices[0];
  }

  save_default_group (newDefaultGroup);
  gtk_tree_path_free (chpath);
}

static void
def_group_in_gconf_changed (GConfClient * client,
                            guint cnxn_id,
                            GConfEntry * entry, GladeXML* dialog)
{
  GConfValue *value = gconf_entry_get_value (entry);

  if (value->type == GCONF_VALUE_INT)
    {
      defaultGroup = gconf_value_get_int (value);
      GtkWidget* treeView = WID ("xkb_layouts_selected");
      GtkTreeModel *model = GTK_TREE_MODEL (gtk_tree_view_get_model (GTK_TREE_VIEW (treeView)));
      GtkTreeIter iter;
      int counter = 0;
      if (gtk_tree_model_get_iter_first (model, &iter))
        {
          do
            {
              gboolean curVal;
              gtk_tree_model_get (model, &iter, 
                                  SLT_COL_DEFAULT, &curVal,
                                  -1);
              if (curVal != ( counter == defaultGroup))
                gtk_list_store_set (GTK_LIST_STORE (model), &iter, 
                                    SLT_COL_DEFAULT, counter == defaultGroup,
                                    -1);
              counter++;
            }
          while (gtk_tree_model_iter_next (model, &iter));
        }
    }
}

static void
add_variant_to_available_layouts_tree (const XklConfigItemPtr configItem, 
                                       GladeXML * chooserDialog)
{
  GtkWidget *layoutsTree = CWID ("xkb_layouts_available");
  GtkTreeIter iter;
  GtkTreeStore *treeStore =
    GTK_TREE_STORE (gtk_tree_view_get_model (GTK_TREE_VIEW (layoutsTree)));
  const gchar *fullLayoutName = GSwitchItKbdConfigMergeItems (current1stLevelId,
							      configItem->name);
  char *utfVariantName = xci_desc_to_utf8 (configItem);

  gtk_tree_store_append (treeStore, &iter, &current1stLevelIter);
  gtk_tree_store_set (treeStore, &iter, 
		      ALT_COL_DESCRIPTION, utfVariantName, 
		      ALT_COL_ID, fullLayoutName, -1);
  g_free (utfVariantName);
}

static void
add_layout_to_available_layouts_tree (const XklConfigItemPtr configItem, 
                                      GladeXML * chooserDialog)
{
  GtkWidget *layoutsTree = CWID ("xkb_layouts_available");
  GtkTreeStore *treeStore =
    GTK_TREE_STORE (gtk_tree_view_get_model (GTK_TREE_VIEW (layoutsTree)));
  char *utfLayoutName = xci_desc_to_utf8 (configItem);

  gtk_tree_store_append (treeStore, &current1stLevelIter, NULL);
  gtk_tree_store_set (treeStore, &current1stLevelIter, 
		      ALT_COL_DESCRIPTION, utfLayoutName, 
		      ALT_COL_ID, configItem->name, -1);
  g_free (utfLayoutName);

  current1stLevelId = configItem->name;

  XklConfigEnumLayoutVariants (configItem->name,
			       (ConfigItemProcessFunc)add_variant_to_available_layouts_tree, 
                               chooserDialog);
}

static void
xkb_layouts_enable_disable_buttons (GladeXML * dialog)
{
  GtkWidget *addLayoutBtn = WID ("xkb_layouts_add");
  GtkWidget *delLayoutBtn = WID ("xkb_layouts_remove");
  GtkWidget *upLayoutBtn = WID ("xkb_layouts_up");
  GtkWidget *dnLayoutBtn = WID ("xkb_layouts_down");
  GtkWidget *selectedLayoutsTree = WID ("xkb_layouts_selected");

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
			    (nSelectedLayouts < maxSelectedLayouts ||
				maxSelectedLayouts == 0));
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

static void
xkb_layout_chooser_enable_disable_buttons (GladeXML * chooserDialog)
{
  GtkWidget *availableLayoutsTree = CWID ("xkb_layouts_available");
  GtkTreeSelection *aSelection =
    gtk_tree_view_get_selection (GTK_TREE_VIEW (availableLayoutsTree));
  const int nSelectedAvailableLayouts =
    gtk_tree_selection_count_selected_rows (aSelection);
			    
  gtk_dialog_set_response_sensitive (GTK_DIALOG (CWID ("xkb_layout_chooser")),
                                      GTK_RESPONSE_OK, nSelectedAvailableLayouts > 0);
}

void xkb_layouts_enable_disable_default (GladeXML * dialog,
                                         gboolean enable)
{
  GValue val = {0};
  g_value_init (&val, G_TYPE_BOOLEAN);
  g_value_set_boolean (&val, enable);
  g_object_set_property (G_OBJECT (toggleRenderer), "activatable", &val);
}

void
xkb_layouts_prepare_selected_tree (GladeXML * dialog, GConfChangeSet * changeset)
{
  GtkListStore *listStore =
    gtk_list_store_new (3, G_TYPE_STRING, G_TYPE_BOOLEAN, G_TYPE_STRING);
  GtkWidget *treeView = WID ("xkb_layouts_selected");
  GtkTreeViewColumn * descColumn, * defColumn;
  GtkTreeSelection *selection;

  textRenderer = GTK_CELL_RENDERER (gtk_cell_renderer_text_new ());
  toggleRenderer = GTK_CELL_RENDERER (gtk_cell_renderer_toggle_new ());

  descColumn = gtk_tree_view_column_new_with_attributes (_("Layout"),
                                                         textRenderer,
                                                         "text", SLT_COL_DESCRIPTION,
                                                         NULL);
  defColumn = gtk_tree_view_column_new_with_attributes (_("Default"),
                                                        toggleRenderer,
                                                        "active", SLT_COL_DEFAULT,
                                                        NULL);
  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (treeView));

  gtk_tree_view_set_model (GTK_TREE_VIEW (treeView),
			   GTK_TREE_MODEL (listStore));

  gtk_tree_view_column_set_sizing (descColumn, GTK_TREE_VIEW_COLUMN_AUTOSIZE);
  gtk_tree_view_column_set_sizing (defColumn, GTK_TREE_VIEW_COLUMN_AUTOSIZE);
  gtk_tree_view_column_set_resizable (descColumn, TRUE);
  gtk_tree_view_column_set_resizable (defColumn, TRUE);

  gtk_tree_view_append_column (GTK_TREE_VIEW (treeView), descColumn);
  gtk_tree_view_append_column (GTK_TREE_VIEW (treeView), defColumn);

  g_signal_connect_swapped (G_OBJECT (selection), "changed",
			    G_CALLBACK
			    (xkb_layouts_enable_disable_buttons), dialog);
  maxSelectedLayouts = XklGetMaxNumGroups();

  gconf_client_notify_add (xkbGConfClient,
                           GSWITCHIT_CONFIG_KEY_DEFAULT_GROUP,
                           (GConfClientNotifyFunc)def_group_in_gconf_changed,
                           dialog,
                           NULL,
                           NULL);
  g_signal_connect (G_OBJECT (toggleRenderer), "toggled",
                    G_CALLBACK (def_group_in_ui_changed), dialog);
}

static void
xkb_layout_chooser_selection_changed (GladeXML * chooserDialog)
{
#ifdef HAVE_X11_EXTENSIONS_XKB_H
  GtkWidget *availableLayoutsTree = CWID ("xkb_layouts_available");
  GtkTreeSelection *selection =
    gtk_tree_view_get_selection (GTK_TREE_VIEW (availableLayoutsTree));
  GtkTreeIter selectedIter;
  GtkTreeModel *model;
  if (kbdraw != NULL &&
      gtk_tree_selection_get_selected (selection, &model, &selectedIter))
    {
      gchar *id;
      XklConfigRec data;
      char **p, *layout, *variant;
      int i;
      XkbComponentNamesRec componentNames;

      gtk_tree_model_get (model, &selectedIter, ALT_COL_ID, &id, -1);
      XklConfigRecInit (&data);
      if (XklConfigGetFromServer (&data))
        {
          if( ( p = data.layouts ) != NULL )
          {
            for( i = data.numLayouts; --i >= 0; )
              free( *p++ );
          }

          if( ( p = data.variants ) != NULL )
          {
            for( i = data.numVariants; --i >= 0; )
              free( *p++ );
          }
          data.numLayouts = 
          data.numVariants = 1;
          data.layouts = realloc (data.layouts, sizeof (char*));
          data.variants = realloc (data.variants, sizeof (char*));
          if (GSwitchItKbdConfigSplitItems (id, &layout, &variant)
              && variant != NULL) 
            {
              data.layouts[0] = (layout == NULL) ? NULL : strdup (layout);
              data.variants[0] = (variant == NULL) ? NULL : strdup (variant);
            } else 
            {
              data.layouts[0] = (id == NULL) ? NULL : strdup (id);
              data.variants[0] = NULL;
            }
          if (_XklXkbConfigPrepareNative (&data, &componentNames))
            {
              keyboard_drawing_set_keyboard (KEYBOARD_DRAWING (kbdraw), &componentNames);

              _XklXkbConfigCleanupNative( &componentNames );
            }
        }
      XklConfigRecDestroy (&data);
    }
#endif
  xkb_layout_chooser_enable_disable_buttons (chooserDialog);
}

void
xkb_layouts_fill_selected_tree (GladeXML * dialog)
{
  GConfEntry *gce;
  GSList *layouts = xkb_layouts_get_selected_list ();
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
      if (GSwitchItKbdConfigGetDescriptions (visible, &sl, &l, &sv, &v))
	visible = GSwitchItKbdConfigFormatFullLayout (l, v);
      v1 = g_strdup (visible);
      utfVisible = g_locale_to_utf8 (g_strstrip (v1), -1, NULL, NULL, NULL);
      gtk_list_store_set (listStore, &iter,
			  SLT_COL_DESCRIPTION, utfVisible, 
			  SLT_COL_DEFAULT, FALSE, 
			  SLT_COL_ID, curLayout->data, -1);
      g_free (utfVisible);
      g_free (v1);
    }

  clear_xkb_elements_list (layouts);
  xkb_layouts_enable_disable_buttons (dialog);
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

  gce = gconf_client_get_entry (xkbGConfClient, 
                                GSWITCHIT_CONFIG_KEY_DEFAULT_GROUP, 
                                NULL, 
                                TRUE, 
                                NULL);
  def_group_in_gconf_changed (xkbGConfClient, -1, gce, dialog);
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
xkb_layouts_fill_available_tree (GladeXML * chooserDialog)
{
  GtkTreeStore *treeStore =
    gtk_tree_store_new (2, G_TYPE_STRING, G_TYPE_STRING);
  GtkWidget *treeView = CWID ("xkb_layouts_available");
  GtkCellRenderer *renderer =
    GTK_CELL_RENDERER (gtk_cell_renderer_text_new ());
  GtkTreeViewColumn *column = gtk_tree_view_column_new_with_attributes (NULL,
									renderer,
									"text",
									ALT_COL_DESCRIPTION,
									NULL);
  GtkTreeSelection *selection =
    gtk_tree_view_get_selection (GTK_TREE_VIEW (treeView));

  gtk_tree_view_set_model (GTK_TREE_VIEW (treeView),
			   GTK_TREE_MODEL (treeStore));
  gtk_tree_view_append_column (GTK_TREE_VIEW (treeView), column);

  XklConfigEnumLayouts ((ConfigItemProcessFunc)
			add_layout_to_available_layouts_tree, chooserDialog);

  sort_tree_content (treeView);
  g_signal_connect_swapped (G_OBJECT (selection), "changed",
			    G_CALLBACK
			    (xkb_layout_chooser_selection_changed), chooserDialog);
}

static void
add_selected_layout (GtkWidget * button, GladeXML * dialog)
{
  xkb_layout_choose (dialog);
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
      GSList *layoutsList = xkb_layouts_get_selected_list ();
      GtkTreePath *path = gtk_tree_model_get_path (model,
						   &selectedIter);
      if (path != NULL)
	{
	  int *indices = gtk_tree_path_get_indices (path);
          int idx = indices[0];
	  char *id = NULL;
	  GSList *node2Remove = g_slist_nth (layoutsList, idx);

	  layoutsList = g_slist_remove_link (layoutsList, node2Remove);

	  id = (char *) node2Remove->data;
	  g_slist_free_1 (node2Remove);

	  if (offset == 0)
            {
	      g_free (id);
              if (defaultGroup > idx)
                save_default_group (defaultGroup - 1);
              else if (defaultGroup == idx)
                save_default_group (-1);
            }
	  else
	    {
	      layoutsList =
		g_slist_insert (layoutsList, id, idx + offset);
	      idx2Select = idx + offset;
              if (idx == defaultGroup)
		save_default_group (idx2Select);
              else if (idx2Select == defaultGroup)
		save_default_group (idx);
	    }

	  xkb_layouts_set_selected_list (layoutsList);
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
xkb_layouts_register_buttons_handlers (GladeXML * dialog)
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
xkb_layout_chooser_response(GtkDialog *dialog,
                            gint response,
                            GladeXML *chooserDialog)
{
  if (response == GTK_RESPONSE_OK)
    {
      GtkTreeSelection *selection =
        gtk_tree_view_get_selection (GTK_TREE_VIEW
                                     (CWID ("xkb_layouts_available")));
      GtkTreeIter selectedIter;
      GtkTreeModel *model;
      if (gtk_tree_selection_get_selected (selection, &model, &selectedIter))
        {
          gchar *id;
          GSList *layoutsList = xkb_layouts_get_selected_list ();
          gtk_tree_model_get (model, &selectedIter, 
                              ALT_COL_ID, &id, -1);
          layoutsList = g_slist_append (layoutsList, id);
          xkb_layouts_set_selected_list (layoutsList);
          /* process default switcher */
          if (g_slist_length(layoutsList) >= 2)
            {
              GSList *optionsList = xkb_options_get_selected_list ();
              gboolean anySwitcher = False;
              GSList *option = optionsList;
              while (option != NULL)
                {
                  char *g, *o;
                  if (GSwitchItKbdConfigSplitItems (option->data, &g, &o))
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
                      const gchar* id = GSwitchItKbdConfigMergeItems (GROUP_SWITCHERS_GROUP, DEFAULT_GROUP_SWITCH);
                      optionsList = g_slist_append (optionsList, g_strdup (id));
                      xkb_options_set_selected_list (optionsList);
                    }
                }
              clear_xkb_elements_list (optionsList);
            }
          clear_xkb_elements_list (layoutsList);
        }
    }
}

static void
xkb_layouts_update_list (GConfClient * client,
		     guint cnxn_id, GConfEntry * entry, GladeXML * dialog)
{
  xkb_layouts_fill_selected_tree (dialog);
  enable_disable_restoring (dialog);
}

void
xkb_layouts_register_gconf_listener (GladeXML * dialog)
{
  gconf_client_notify_add (xkbGConfClient,
			   GSWITCHIT_KBD_CONFIG_KEY_LAYOUTS,
			   (GConfClientNotifyFunc)
			   xkb_layouts_update_list, dialog, NULL, NULL);
}

void
xkb_layout_choose (GladeXML * dialog)
{
  GladeXML* chooserDialog = glade_xml_new (GNOMECC_DATA_DIR "/interfaces/gnome-keyboard-properties.glade", "xkb_layout_chooser", NULL);
  GtkWidget* chooser = CWID ( "xkb_layout_chooser");

  gtk_window_set_transient_for (GTK_WINDOW (chooser), GTK_WINDOW (WID ("keyboard_dialog")));

  xkb_layouts_fill_available_tree (chooserDialog);
  xkb_layout_chooser_selection_changed (chooserDialog);
  
#ifdef HAVE_X11_EXTENSIONS_XKB_H
  if (!strcmp (XklGetBackendName(), "XKB"))
  {
    kbdraw = create_preview_widget (dialog);

    gtk_container_add (GTK_CONTAINER (CWID ("vboxPreview")), kbdraw);
    gtk_widget_show_all (kbdraw);
  } else
#endif
  {
    gtk_widget_hide_all (CWID ("vboxPreview"));
  }

  g_signal_connect (G_OBJECT (chooser),
		    "response", G_CALLBACK (xkb_layout_chooser_response), chooserDialog);

  gtk_dialog_run (GTK_DIALOG (chooser));
  gtk_widget_destroy (chooser);

  kbdraw = NULL;
}
