/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright 2012  Red Hat, Inc,
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Marek Kasik <mkasik@redhat.com>
 */

#include "config.h"

#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <glib.h>
#include <glib/gi18n.h>
#include <glib/gstdio.h>
#include <gtk/gtk.h>

#include <cups/cups.h>
#include <cups/ppd.h>

#include "pp-ppd-selection-dialog.h"

static void pp_ppd_selection_dialog_hide (PpPPDSelectionDialog *dialog);

enum
{
  PPD_NAMES_COLUMN = 0,
  PPD_DISPLAY_NAMES_COLUMN
};

enum
{
  PPD_MANUFACTURERS_NAMES_COLUMN = 0,
  PPD_MANUFACTURERS_DISPLAY_NAMES_COLUMN
};


struct _PpPPDSelectionDialog {
  GtkBuilder *builder;
  GtkWidget  *parent;
  GtkWidget  *dialog;

  UserResponseCallback user_callback;
  gpointer             user_data;

  gchar           *ppd_name;
  gchar           *ppd_display_name;
  GtkResponseType  response;
  gchar           *manufacturer;

  PPDList *list;
};

static void
manufacturer_selection_changed_cb (GtkTreeSelection *selection,
                                   gpointer          user_data)
{
  PpPPDSelectionDialog *dialog = (PpPPDSelectionDialog *) user_data;
  GtkListStore         *store;
  GtkTreeModel         *model;
  GtkTreeIter           iter;
  GtkTreeView          *models_treeview;
  gchar                *manufacturer_name = NULL;
  gint                  i, index;

  if (gtk_tree_selection_get_selected (selection, &model, &iter))
    {
      gtk_tree_model_get (model, &iter,
			  PPD_MANUFACTURERS_NAMES_COLUMN, &manufacturer_name,
			  -1);
    }

  if (manufacturer_name)
    {
      index = -1;
      for (i = 0; i < dialog->list->num_of_manufacturers; i++)
        {
          if (g_strcmp0 (manufacturer_name,
                         dialog->list->manufacturers[i]->manufacturer_name) == 0)
            {
              index = i;
              break;
            }
        }

      if (index >= 0)
        {
          models_treeview = (GtkTreeView*)
            gtk_builder_get_object (dialog->builder, "ppd-selection-models-treeview");

          store = gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_STRING);

          for (i = 0; i < dialog->list->manufacturers[index]->num_of_ppds; i++)
            {
              gtk_list_store_append (store, &iter);
              gtk_list_store_set (store, &iter,
                                  PPD_NAMES_COLUMN, dialog->list->manufacturers[index]->ppds[i]->ppd_name,
                                  PPD_DISPLAY_NAMES_COLUMN, dialog->list->manufacturers[index]->ppds[i]->ppd_display_name,
                                  -1);
            }

          gtk_tree_view_set_model (models_treeview, GTK_TREE_MODEL (store));
          g_object_unref (store);
          gtk_tree_view_columns_autosize (models_treeview);
        }

      g_free (manufacturer_name);
    }
}

static void
model_selection_changed_cb (GtkTreeSelection *selection,
                            gpointer          user_data)
{
  PpPPDSelectionDialog *dialog = (PpPPDSelectionDialog *) user_data;
  GtkTreeModel         *model;
  GtkTreeIter           iter;
  GtkWidget            *widget;
  gchar                *model_name = NULL;

  if (gtk_tree_selection_get_selected (selection, &model, &iter))
    {
      gtk_tree_model_get (model, &iter,
                          PPD_NAMES_COLUMN, &model_name,
			  -1);
    }

  widget = (GtkWidget*)
    gtk_builder_get_object (dialog->builder, "ppd-selection-select-button");

  if (model_name)
    {
      gtk_widget_set_sensitive (widget, TRUE);
      g_free (model_name);
    }
  else
    {
      gtk_widget_set_sensitive (widget, FALSE);
    }
}

static void
fill_ppds_list (PpPPDSelectionDialog *dialog)
{
  GtkTreeSelection *selection;
  GtkListStore     *store;
  GtkTreePath      *path;
  GtkTreeView      *treeview;
  GtkTreeIter       iter;
  GtkTreeIter      *preselect_iter = NULL;
  GtkWidget        *widget;
  gint              i;

  widget = (GtkWidget*)
    gtk_builder_get_object (dialog->builder, "ppd-spinner");
  gtk_widget_hide (widget);
  gtk_spinner_stop (GTK_SPINNER (widget));

  widget = (GtkWidget*)
    gtk_builder_get_object (dialog->builder, "progress-label");
  gtk_widget_hide (widget);

  treeview = (GtkTreeView*)
    gtk_builder_get_object (dialog->builder, "ppd-selection-manufacturers-treeview");

  if (dialog->list)
    {
      store = gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_STRING);

      for (i = 0; i < dialog->list->num_of_manufacturers; i++)
        {
          gtk_list_store_append (store, &iter);
          gtk_list_store_set (store, &iter,
                              PPD_MANUFACTURERS_NAMES_COLUMN, dialog->list->manufacturers[i]->manufacturer_name,
                              PPD_MANUFACTURERS_DISPLAY_NAMES_COLUMN, dialog->list->manufacturers[i]->manufacturer_display_name,
                              -1);

          if (g_strcmp0 (dialog->manufacturer,
                         dialog->list->manufacturers[i]->manufacturer_display_name) == 0)
            {
              preselect_iter = gtk_tree_iter_copy (&iter);
            }
        }

      gtk_tree_view_set_model (treeview, GTK_TREE_MODEL (store));

      if (preselect_iter &&
          (selection = gtk_tree_view_get_selection (treeview)) != NULL)
        {
          gtk_tree_selection_select_iter (selection, preselect_iter);
          path = gtk_tree_model_get_path (GTK_TREE_MODEL (store), preselect_iter);
          gtk_tree_view_scroll_to_cell (treeview, path, NULL, TRUE, 0.5, 0.0);
          gtk_tree_path_free (path);
          gtk_tree_iter_free (preselect_iter);
        }

      g_object_unref (store);
    }
}

static void
populate_dialog (PpPPDSelectionDialog *dialog)
{
  GtkTreeViewColumn *column;
  GtkCellRenderer   *renderer;
  GtkTreeView       *manufacturers_treeview;
  GtkTreeView       *models_treeview;
  GtkWidget         *widget;
  GtkWidget         *header;

  manufacturers_treeview = (GtkTreeView*)
    gtk_builder_get_object (dialog->builder, "ppd-selection-manufacturers-treeview");

  renderer = gtk_cell_renderer_text_new ();
  gtk_cell_renderer_set_padding (renderer, 10, 0);

  /* Translators: Name of column showing printer manufacturers */
  column = gtk_tree_view_column_new_with_attributes (_("Manufacturer"), renderer,
                                                     "text", PPD_MANUFACTURERS_DISPLAY_NAMES_COLUMN, NULL);
  gtk_tree_view_column_set_expand (column, TRUE);
  header = gtk_label_new (gtk_tree_view_column_get_title (column));
  gtk_widget_set_margin_start (header, 10);
  gtk_tree_view_column_set_widget (column, header);
  gtk_widget_show (header);
  gtk_tree_view_append_column (manufacturers_treeview, column);


  models_treeview = (GtkTreeView*)
    gtk_builder_get_object (dialog->builder, "ppd-selection-models-treeview");

  renderer = gtk_cell_renderer_text_new ();
  gtk_cell_renderer_set_padding (renderer, 10, 0);

  /* Translators: Name of column showing printer drivers */
  column = gtk_tree_view_column_new_with_attributes (_("Driver"), renderer,
                                                     "text", PPD_DISPLAY_NAMES_COLUMN,
                                                     NULL);
  gtk_tree_view_column_set_expand (column, TRUE);
  header = gtk_label_new (gtk_tree_view_column_get_title (column));
  gtk_widget_set_margin_start (header, 10);
  gtk_tree_view_column_set_widget (column, header);
  gtk_widget_show (header);
  gtk_tree_view_append_column (models_treeview, column);


  g_signal_connect (gtk_tree_view_get_selection (models_treeview),
                    "changed", G_CALLBACK (model_selection_changed_cb), dialog);

  g_signal_connect (gtk_tree_view_get_selection (manufacturers_treeview),
                    "changed", G_CALLBACK (manufacturer_selection_changed_cb), dialog);

  gtk_widget_show_all (dialog->dialog);

  if (!dialog->list)
    {
      widget = (GtkWidget*)
        gtk_builder_get_object (dialog->builder, "ppd-spinner");
      gtk_widget_show (widget);
      gtk_spinner_start (GTK_SPINNER (widget));

      widget = (GtkWidget*)
        gtk_builder_get_object (dialog->builder, "progress-label");
      gtk_widget_show (widget);
    }
  else
    {
      fill_ppds_list (dialog);
    }
}

static void
ppd_selection_dialog_response_cb (GtkDialog *dialog,
                                  gint       response_id,
                                  gpointer   user_data)
{
  PpPPDSelectionDialog *ppd_selection_dialog = (PpPPDSelectionDialog*) user_data;
  GtkTreeSelection     *selection;
  GtkTreeModel         *model;
  GtkTreeView          *models_treeview;
  GtkTreeIter           iter;

  pp_ppd_selection_dialog_hide (ppd_selection_dialog);

  ppd_selection_dialog->response = response_id;
  if (response_id == GTK_RESPONSE_OK)
    {
      models_treeview = (GtkTreeView*)
        gtk_builder_get_object (ppd_selection_dialog->builder, "ppd-selection-models-treeview");

      if (models_treeview)
        {
          selection = gtk_tree_view_get_selection (models_treeview);

          if (selection)
            {
              if (gtk_tree_selection_get_selected (selection, &model, &iter))
                {
                  gtk_tree_model_get (model, &iter,
                                      PPD_NAMES_COLUMN, &ppd_selection_dialog->ppd_name,
                                      PPD_DISPLAY_NAMES_COLUMN, &ppd_selection_dialog->ppd_display_name,
            			  -1);
                }
            }
        }
    }

  ppd_selection_dialog->user_callback (GTK_DIALOG (ppd_selection_dialog->dialog),
                                       response_id,
                                       ppd_selection_dialog->user_data);
}

PpPPDSelectionDialog *
pp_ppd_selection_dialog_new (GtkWindow            *parent,
                             PPDList              *ppd_list,
                             const gchar          *manufacturer,
                             UserResponseCallback  user_callback,
                             gpointer              user_data)
{
  PpPPDSelectionDialog *dialog;
  GtkWidget            *widget;
  GError               *error = NULL;
  gchar                *objects[] = { "ppd-selection-dialog", NULL };
  guint                 builder_result;

  dialog = g_new0 (PpPPDSelectionDialog, 1);

  dialog->builder = gtk_builder_new ();
  dialog->parent = GTK_WIDGET (parent);

  builder_result = gtk_builder_add_objects_from_resource (dialog->builder,
                                                          "/org/gnome/control-center/printers/ppd-selection-dialog.ui",
                                                          objects, &error);

  if (builder_result == 0)
    {
      g_warning ("Could not load ui: %s", error->message);
      g_error_free (error);
      return NULL;
    }

  dialog->dialog = (GtkWidget *) gtk_builder_get_object (dialog->builder, "ppd-selection-dialog");
  dialog->user_callback = user_callback;
  dialog->user_data = user_data;

  dialog->response = GTK_RESPONSE_NONE;
  dialog->list = ppd_list_copy (ppd_list);

  dialog->manufacturer = get_standard_manufacturers_name (manufacturer);

  /* connect signals */
  g_signal_connect (dialog->dialog, "delete-event", G_CALLBACK (gtk_widget_hide_on_delete), NULL);
  g_signal_connect (dialog->dialog, "response", G_CALLBACK (ppd_selection_dialog_response_cb), dialog);

  gtk_window_set_transient_for (GTK_WINDOW (dialog->dialog), GTK_WINDOW (parent));

  widget = (GtkWidget*)
    gtk_builder_get_object (dialog->builder, "ppd-spinner");
  gtk_spinner_start (GTK_SPINNER (widget));

  populate_dialog (dialog);

  gtk_window_present (GTK_WINDOW (dialog->dialog));
  gtk_widget_show_all (GTK_WIDGET (dialog->dialog));

  return dialog;
}

void
pp_ppd_selection_dialog_free (PpPPDSelectionDialog *dialog)
{
  gtk_widget_destroy (GTK_WIDGET (dialog->dialog));

  g_object_unref (dialog->builder);

  g_free (dialog->ppd_name);

  g_free (dialog->ppd_display_name);

  g_free (dialog->manufacturer);

  g_free (dialog);
}

gchar *
pp_ppd_selection_dialog_get_ppd_name (PpPPDSelectionDialog *dialog)
{
  return g_strdup (dialog->ppd_name);
}

gchar *
pp_ppd_selection_dialog_get_ppd_display_name (PpPPDSelectionDialog *dialog)
{
  return g_strdup (dialog->ppd_display_name);
}

void
pp_ppd_selection_dialog_set_ppd_list (PpPPDSelectionDialog *dialog,
                                      PPDList              *list)
{
  dialog->list = list;
  fill_ppds_list (dialog);
}

static void
pp_ppd_selection_dialog_hide (PpPPDSelectionDialog *dialog)
{
  gtk_widget_hide (GTK_WIDGET (dialog->dialog));
}
