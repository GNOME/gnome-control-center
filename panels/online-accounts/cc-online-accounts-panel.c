/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/*
 * Copyright (C) 2011, 2012 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General
 * Public License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place, Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Author: David Zeuthen <davidz@redhat.com>
 */

#include "config.h"

#include <string.h>
#include <glib/gi18n-lib.h>

#define GOA_API_IS_SUBJECT_TO_CHANGE
#include <goa/goa.h>
#define GOA_BACKEND_API_IS_SUBJECT_TO_CHANGE
#include <goabackend/goabackend.h>

#include "cc-online-accounts-panel.h"

#include "cc-online-accounts-add-account-dialog.h"
#include "cc-online-accounts-model.h"

typedef struct _GoaPanelClass GoaPanelClass;

struct _GoaPanel
{
  CcPanel parent_instance;

  GtkBuilder *builder;

  GoaClient *client;

  GoaPanelAccountsModel *accounts_model;

  GtkWidget *toolbar;
  GtkWidget *toolbar_add_button;
  GtkWidget *toolbar_remove_button;
  GtkWidget *accounts_treeview;
  GtkWidget *accounts_vbox;
};

struct _GoaPanelClass
{
  CcPanelClass parent_class;
};

static void on_model_row_deleted (GtkTreeModel *tree_model,
                                  GtkTreePath  *path,
                                  gpointer      user_data);
static void on_model_row_inserted (GtkTreeModel *tree_model,
                                   GtkTreePath  *path,
                                   GtkTreeIter  *iter,
                                   gpointer      user_data);

static void on_tree_view_selection_changed (GtkTreeSelection *selection,
                                            gpointer          user_data);

static void on_toolbar_add_button_clicked (GtkToolButton *button,
                                           gpointer       user_data);
static void on_toolbar_remove_button_clicked (GtkToolButton *button,
                                              gpointer       user_data);

static void on_add_button_clicked (GtkButton *button,
                                   gpointer   user_data);

static void on_account_changed (GoaClient  *client,
                                GoaObject  *object,
                                gpointer    user_data);

static gboolean select_account_by_id (GoaPanel    *panel,
                                      const gchar *account_id);

CC_PANEL_REGISTER (GoaPanel, goa_panel);

enum {
  PROP_0,
  PROP_ARGV
};

static void
goa_panel_set_property (GObject *object,
                        guint property_id,
                        const GValue *value,
                        GParamSpec *pspec)
{
  switch (property_id)
    {
      case PROP_ARGV:
        {
          gchar **args;

          args = g_value_get_boxed (value);

          if (args != NULL && *args != '\0')
            select_account_by_id (GOA_PANEL (object), args[0]);
          return;
        }
    }

  G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
goa_panel_finalize (GObject *object)
{
  GoaPanel *panel = GOA_PANEL (object);

  if (panel->accounts_model != NULL)
    g_clear_object (&panel->accounts_model);

  if (panel->client != NULL)
    g_object_unref (panel->client);
  g_object_unref (panel->builder);

  G_OBJECT_CLASS (goa_panel_parent_class)->finalize (object);
}

static void
goa_panel_init (GoaPanel *panel)
{
  GtkWidget *button;
  GtkWidget *w;
  GError *error;
  GtkStyleContext *context;
  GtkTreeViewColumn *column;
  GtkCellRenderer *renderer;
  GtkTreeIter iter;

  panel->builder = gtk_builder_new ();
  error = NULL;
  if (gtk_builder_add_from_file (panel->builder,
                                 GNOMECC_UI_DIR "/online-accounts.ui",
                                 &error) == 0)
    {
      goa_warning ("Error loading UI file: %s (%s, %d)",
                   error->message, g_quark_to_string (error->domain), error->code);
      g_error_free (error);
      goto out;
    }

  panel->toolbar = GTK_WIDGET (gtk_builder_get_object (panel->builder, "accounts-tree-toolbar"));
  panel->toolbar_add_button = GTK_WIDGET (gtk_builder_get_object (panel->builder, "accounts-tree-toolbutton-add"));
  g_signal_connect (panel->toolbar_add_button,
                    "clicked",
                    G_CALLBACK (on_toolbar_add_button_clicked),
                    panel);
  panel->toolbar_remove_button = GTK_WIDGET (gtk_builder_get_object (panel->builder, "accounts-tree-toolbutton-remove"));
  g_signal_connect (panel->toolbar_remove_button,
                    "clicked",
                    G_CALLBACK (on_toolbar_remove_button_clicked),
                    panel);

  context = gtk_widget_get_style_context (GTK_WIDGET (gtk_builder_get_object (panel->builder, "accounts-tree-scrolledwindow")));
  gtk_style_context_set_junction_sides (context, GTK_JUNCTION_BOTTOM);
  context = gtk_widget_get_style_context (panel->toolbar);
  gtk_style_context_set_junction_sides (context, GTK_JUNCTION_TOP);

  panel->accounts_treeview = GTK_WIDGET (gtk_builder_get_object (panel->builder, "accounts-tree-treeview"));
  g_signal_connect (gtk_tree_view_get_selection (GTK_TREE_VIEW (panel->accounts_treeview)),
                    "changed",
                    G_CALLBACK (on_tree_view_selection_changed),
                    panel);

  button = GTK_WIDGET (gtk_builder_get_object (panel->builder, "accounts-button-add"));
  g_signal_connect (button,
                    "clicked",
                    G_CALLBACK (on_add_button_clicked),
                    panel);

  panel->accounts_vbox = GTK_WIDGET (gtk_builder_get_object (panel->builder, "accounts-vbox"));

  /* TODO: probably want to avoid _sync() ... */
  error = NULL;
  panel->client = goa_client_new_sync (NULL /* GCancellable */, &error);
  if (panel->client == NULL)
    {
      goa_warning ("Error getting a GoaClient: %s (%s, %d)",
                   error->message, g_quark_to_string (error->domain), error->code);
      w = GTK_WIDGET (gtk_builder_get_object (panel->builder, "goa-top-widget"));
      gtk_widget_set_sensitive (w, FALSE);
      g_error_free (error);
      goto out;
    }
  g_signal_connect (panel->client,
                    "account-changed",
                    G_CALLBACK (on_account_changed),
                    panel);

  panel->accounts_model = goa_panel_accounts_model_new (panel->client);
  gtk_tree_view_set_model (GTK_TREE_VIEW (panel->accounts_treeview), GTK_TREE_MODEL (panel->accounts_model));
  g_signal_connect (panel->accounts_model, "row-deleted", G_CALLBACK (on_model_row_deleted), panel);
  g_signal_connect (panel->accounts_model, "row-inserted", G_CALLBACK (on_model_row_inserted), panel);

  column = gtk_tree_view_column_new ();
  gtk_tree_view_append_column (GTK_TREE_VIEW (panel->accounts_treeview), column);

  renderer = gtk_cell_renderer_pixbuf_new ();
  gtk_tree_view_column_pack_start (column, renderer, FALSE);
  g_object_set (G_OBJECT (renderer),
                "stock-size", GTK_ICON_SIZE_DIALOG,
                NULL);
  gtk_tree_view_column_set_attributes (column,
                                       renderer,
                                       "gicon", GOA_PANEL_ACCOUNTS_MODEL_COLUMN_ICON,
                                       NULL);

  renderer = gtk_cell_renderer_text_new ();
  gtk_tree_view_column_pack_start (column, renderer, FALSE);
  g_object_set (G_OBJECT (renderer),
                "ellipsize", PANGO_ELLIPSIZE_END,
                "ellipsize-set", TRUE,
                "width-chars", 30,
                NULL);
  gtk_tree_view_column_set_attributes (column,
                                       renderer,
                                       "markup", GOA_PANEL_ACCOUNTS_MODEL_COLUMN_MARKUP,
                                       NULL);

  renderer = gtk_cell_renderer_pixbuf_new ();
  gtk_tree_view_column_pack_end (column, renderer, FALSE);
  g_object_set (G_OBJECT (renderer),
                "icon-name", "dialog-error-symbolic",
                NULL);
  gtk_tree_view_column_set_attributes (column,
                                       renderer,
                                       "visible", GOA_PANEL_ACCOUNTS_MODEL_COLUMN_ATTENTION_NEEDED,
                                       NULL);

  /* Select the first row, if any */
  if (gtk_tree_model_get_iter_first (GTK_TREE_MODEL (panel->accounts_model),
                                     &iter))
    gtk_tree_selection_select_iter (gtk_tree_view_get_selection (GTK_TREE_VIEW (panel->accounts_treeview)),
                                    &iter);

 out:
  w = GTK_WIDGET (gtk_builder_get_object (panel->builder, "goa-top-widget"));
  gtk_widget_reparent (w, GTK_WIDGET (panel));
  gtk_widget_show_all (w);
}

static const char *
goa_panel_get_help_uri (CcPanel *panel)
{
  return "help:gnome-help/accounts";
}

static void
goa_panel_class_init (GoaPanelClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  CcPanelClass *panel_class = CC_PANEL_CLASS (klass);

  panel_class->get_help_uri = goa_panel_get_help_uri;

  object_class->set_property = goa_panel_set_property;
  object_class->finalize = goa_panel_finalize;

  g_object_class_override_property (object_class, PROP_ARGV, "argv");
}

/* ---------------------------------------------------------------------------------------------------- */

static void
goa_panel_register (GIOModule *module)
{
  goa_panel_register_type (G_TYPE_MODULE (module));
  g_io_extension_point_implement (CC_SHELL_PANEL_EXTENSION_POINT,
                                  GOA_TYPE_PANEL,
                                  "online-accounts",
                                  0);
}

void
g_io_module_load (GIOModule *module)
{
  bindtextdomain (GETTEXT_PACKAGE, GNOMELOCALEDIR);
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
  goa_panel_register (module);
}

void
g_io_module_unload (GIOModule *module)
{
}

/* ---------------------------------------------------------------------------------------------------- */

static void
show_page (GoaPanel *panel,
           gint page_num)
{
  GtkNotebook *notebook;
  notebook = GTK_NOTEBOOK (gtk_builder_get_object (panel->builder, "accounts-notebook"));
  gtk_notebook_set_current_page (notebook, page_num);
}

static void
show_page_nothing_selected (GoaPanel *panel)
{
  GtkWidget *box;
  GtkWidget *label;

  show_page (panel, 0);

  box = GTK_WIDGET (gtk_builder_get_object (panel->builder, "accounts-tree-box"));
  gtk_widget_set_sensitive (box, FALSE);

  label = GTK_WIDGET (gtk_builder_get_object (panel->builder, "accounts-tree-label"));
  gtk_widget_show (label);
}

static void
on_info_bar_response (GtkInfoBar *info_bar,
                      gint        response_id,
                      gpointer    user_data)
{
  GoaPanel *panel = GOA_PANEL (user_data);
  GtkTreeIter iter;

  if (gtk_tree_selection_get_selected (gtk_tree_view_get_selection (GTK_TREE_VIEW (panel->accounts_treeview)),
                                       NULL,
                                       &iter))
    {
      GoaProvider *provider;
      const gchar *provider_type;
      GoaAccount *account;
      GoaObject *object;
      GtkWindow *parent;
      GError *error;

      gtk_tree_model_get (GTK_TREE_MODEL (panel->accounts_model),
                          &iter,
                          GOA_PANEL_ACCOUNTS_MODEL_COLUMN_OBJECT, &object,
                          -1);

      account = goa_object_peek_account (object);
      provider_type = goa_account_get_provider_type (account);
      provider = goa_provider_get_for_provider_type (provider_type);

      parent = GTK_WINDOW (cc_shell_get_toplevel (cc_panel_get_shell (CC_PANEL (panel))));

      error = NULL;
      if (!goa_provider_refresh_account (provider,
                                         panel->client,
                                         object,
                                         parent,
                                         &error))
        {
          if (!(error->domain == GOA_ERROR && error->code == GOA_ERROR_DIALOG_DISMISSED))
            {
              GtkWidget *dialog;
              dialog = gtk_message_dialog_new (parent,
                                               GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                               GTK_MESSAGE_ERROR,
                                               GTK_BUTTONS_CLOSE,
                                               _("Error logging into the account"));
              gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
                                                        "%s",
                                                        error->message);
              gtk_widget_show_all (dialog);
              gtk_dialog_run (GTK_DIALOG (dialog));
              gtk_widget_destroy (dialog);
            }
          g_error_free (error);
        }
      g_object_unref (provider);
      g_object_unref (object);
    }
}

static void
show_page_account (GoaPanel  *panel,
                   GoaObject *object)
{
  GList *children;
  GList *l;
  GtkWidget *box;
  GtkWidget *grid;
  GtkWidget *left_grid;
  GtkWidget *right_grid;
  GtkWidget *bar;
  GtkWidget *label;
  GoaProvider *provider;
  GoaAccount *account;
  const gchar *provider_type;

  provider = NULL;

  show_page (panel, 1);
  box = GTK_WIDGET (gtk_builder_get_object (panel->builder, "accounts-tree-box"));
  gtk_widget_set_sensitive (box, TRUE);

  label = GTK_WIDGET (gtk_builder_get_object (panel->builder, "accounts-tree-label"));
  gtk_widget_hide (label);

  /* Out with the old */
  children = gtk_container_get_children (GTK_CONTAINER (panel->accounts_vbox));
  for (l = children; l != NULL; l = l->next)
    gtk_container_remove (GTK_CONTAINER (panel->accounts_vbox), GTK_WIDGET (l->data));
  g_list_free (children);

  account = goa_object_peek_account (object);
  provider_type = goa_account_get_provider_type (account);
  provider = goa_provider_get_for_provider_type (provider_type);

  /* And in with the new */
  if (goa_account_get_attention_needed (account))
    {
      bar = gtk_info_bar_new ();
      label = gtk_label_new (_("Expired credentials. Please log in again."));
      gtk_container_add (GTK_CONTAINER (gtk_info_bar_get_content_area (GTK_INFO_BAR (bar))), label);
      if (provider != NULL)
        gtk_info_bar_add_button (GTK_INFO_BAR (bar), _("_Log In"), GTK_RESPONSE_OK);
      gtk_box_pack_start (GTK_BOX (panel->accounts_vbox), bar, FALSE, TRUE, 0);
      g_signal_connect (bar, "response", G_CALLBACK (on_info_bar_response), panel);
    }

  left_grid = gtk_grid_new ();
  gtk_widget_set_halign (left_grid, GTK_ALIGN_END);
  gtk_widget_set_hexpand (left_grid, TRUE);
  gtk_orientable_set_orientation (GTK_ORIENTABLE (left_grid), GTK_ORIENTATION_VERTICAL);
  gtk_grid_set_row_spacing (GTK_GRID (left_grid), 0);

  right_grid = gtk_grid_new ();
  gtk_widget_set_hexpand (right_grid, TRUE);
  gtk_orientable_set_orientation (GTK_ORIENTABLE (right_grid), GTK_ORIENTATION_VERTICAL);
  gtk_grid_set_row_spacing (GTK_GRID (right_grid), 0);

  if (provider != NULL)
    {
      goa_provider_show_account (provider,
                                 panel->client,
                                 object,
                                 GTK_BOX (panel->accounts_vbox),
                                 GTK_GRID (left_grid),
                                 GTK_GRID (right_grid));
    }

  grid = gtk_grid_new ();
  gtk_orientable_set_orientation (GTK_ORIENTABLE (grid), GTK_ORIENTATION_HORIZONTAL);
  gtk_grid_set_column_spacing (GTK_GRID (grid), 12);
  gtk_container_add (GTK_CONTAINER (grid), left_grid);
  gtk_container_add (GTK_CONTAINER (grid), right_grid);
  gtk_box_pack_start (GTK_BOX (panel->accounts_vbox), grid, FALSE, TRUE, 0);

  gtk_widget_show_all (panel->accounts_vbox);

  if (provider != NULL)
    g_object_unref (provider);
}

/* ---------------------------------------------------------------------------------------------------- */

static gboolean
select_account_by_id (GoaPanel    *panel,
                      const gchar *account_id)
{
  GoaObject *goa_object = NULL;
  GtkTreeIter iter;
  gboolean iter_set = FALSE;

  goa_object = goa_client_lookup_by_id (panel->client, account_id);
  if (goa_object != NULL)
    {
      iter_set = goa_panel_accounts_model_get_iter_for_object (panel->accounts_model,
                                                               goa_object,
                                                               &iter);
      g_object_unref (goa_object);
    }

  if (iter_set)
    {
      GtkTreeView *tree_view;
      GtkTreeSelection *selection;

      tree_view = GTK_TREE_VIEW (panel->accounts_treeview);
      selection = gtk_tree_view_get_selection (tree_view);
      gtk_tree_selection_select_iter (selection, &iter);
    }

  return iter_set;
}

static void
on_tree_view_selection_changed (GtkTreeSelection *selection,
                                gpointer          user_data)
{
  GoaPanel *panel = GOA_PANEL (user_data);
  GtkTreeIter iter;

  if (gtk_tree_selection_get_selected (selection, NULL, &iter))
    {
      GoaObject *object;
      gtk_tree_model_get (GTK_TREE_MODEL (panel->accounts_model),
                          &iter,
                          GOA_PANEL_ACCOUNTS_MODEL_COLUMN_OBJECT, &object,
                          -1);
      show_page_account (panel, object);
      g_object_unref (object);
    }
  else
    {
      show_page_nothing_selected (panel);
    }
}

static void
on_account_changed (GoaClient  *client,
                    GoaObject  *object,
                    gpointer    user_data)
{
  GoaPanel *panel = GOA_PANEL (user_data);
  GtkTreeIter iter;

  if (gtk_tree_selection_get_selected (gtk_tree_view_get_selection (GTK_TREE_VIEW (panel->accounts_treeview)),
                                       NULL,
                                       &iter))
    {
      GoaObject *selected_object;
      gtk_tree_model_get (GTK_TREE_MODEL (panel->accounts_model),
                          &iter,
                          GOA_PANEL_ACCOUNTS_MODEL_COLUMN_OBJECT, &selected_object,
                          -1);
      if (selected_object == object)
        show_page_account (panel, selected_object);
      g_object_unref (selected_object);
    }
}

/* ---------------------------------------------------------------------------------------------------- */

static void
on_model_row_changed (GtkTreeModel *tree_model,
                      GtkTreePath  *path,
                      GtkTreeIter  *iter,
                      gpointer      user_data)
{
  GtkTreeSelection *selection = GTK_TREE_SELECTION (user_data);

  gtk_tree_selection_select_iter (selection, iter);
  g_signal_handlers_disconnect_by_func (tree_model, G_CALLBACK (on_model_row_changed), user_data);
}

static void
on_model_row_deleted (GtkTreeModel *tree_model,
                      GtkTreePath  *path,
                      gpointer      user_data)
{
  GoaPanel *panel = GOA_PANEL (user_data);
  GtkTreeIter iter;
  GtkTreeSelection *selection;

  if (!gtk_tree_model_get_iter (tree_model, &iter, path))
    {
      if (!gtk_tree_path_prev (path))
        return;
    }

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (panel->accounts_treeview));
  gtk_tree_selection_select_path (selection, path);
}

static void
on_model_row_inserted (GtkTreeModel *tree_model,
                       GtkTreePath  *path,
                       GtkTreeIter  *iter,
                       gpointer      user_data)
{
  GoaPanel *panel = GOA_PANEL (user_data);
  GtkTreeSelection *selection;

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (panel->accounts_treeview));
  if (gtk_tree_selection_get_selected (selection, NULL, NULL))
    return;

  /* An empty row has been inserted and is going to be filled in, so
   * we expect selection to stay valid.
   */
  g_signal_connect (tree_model, "row-changed", G_CALLBACK (on_model_row_changed), selection);
}

/* ---------------------------------------------------------------------------------------------------- */

static void
add_account (GoaPanel *panel)
{
  GtkWindow *parent;
  GtkWidget *dialog;
  gint response;
  GList *providers;
  GList *l;
  GoaObject *object;
  GError *error;

  providers = NULL;

  parent = GTK_WINDOW (cc_shell_get_toplevel (cc_panel_get_shell (CC_PANEL (panel))));

  dialog = goa_panel_add_account_dialog_new (panel->client);
  gtk_window_set_transient_for (GTK_WINDOW (dialog), parent);

  providers = goa_provider_get_all ();
  for (l = providers; l != NULL; l = l->next)
    {
      GoaProvider *provider;

      provider = GOA_PROVIDER (l->data);
      goa_panel_add_account_dialog_add_provider (GOA_PANEL_ADD_ACCOUNT_DIALOG (dialog), provider);
    }

  gtk_widget_show_all (dialog);
  response = gtk_dialog_run (GTK_DIALOG (dialog));
  if (response != GTK_RESPONSE_OK)
    {
      gtk_widget_destroy (dialog);
      goto out;
    }

  error = NULL;
  object = goa_panel_add_account_dialog_get_account (GOA_PANEL_ADD_ACCOUNT_DIALOG (dialog), &error);
  gtk_widget_destroy (dialog);

  /* We might have an object even when error is set.
   * eg., if we failed to store the credentials in the keyring.
   */

  if (object != NULL)
    {
      GtkTreeIter iter;
      /* navigate to newly created object */
      if (goa_panel_accounts_model_get_iter_for_object (panel->accounts_model,
                                                        object,
                                                        &iter))
        {
          gtk_tree_selection_select_iter (gtk_tree_view_get_selection (GTK_TREE_VIEW (panel->accounts_treeview)),
                                          &iter);
        }
      g_object_unref (object);
    }

  if (error != NULL)
    {
      if (!(error->domain == GOA_ERROR && error->code == GOA_ERROR_DIALOG_DISMISSED))
        {
          dialog = gtk_message_dialog_new (parent,
                                           GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                           GTK_MESSAGE_ERROR,
                                           GTK_BUTTONS_CLOSE,
                                           _("Error creating account"));
          gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
                                                    "%s",
                                                    error->message);
          gtk_widget_show_all (dialog);
          gtk_dialog_run (GTK_DIALOG (dialog));
          gtk_widget_destroy (dialog);
        }
      g_error_free (error);
    }

 out:
  g_list_foreach (providers, (GFunc) g_object_unref, NULL);
  g_list_free (providers);
}

/* ---------------------------------------------------------------------------------------------------- */

static void
on_toolbar_add_button_clicked (GtkToolButton *button,
                               gpointer       user_data)
{
  GoaPanel *panel = GOA_PANEL (user_data);
  add_account (panel);
}

static void
remove_account_cb (GoaAccount    *account,
                   GAsyncResult  *res,
                   gpointer       user_data)
{
  GoaPanel *panel = GOA_PANEL (user_data);
  GError *error;

  error = NULL;
  if (!goa_account_call_remove_finish (account, res, &error))
    {
      GtkWidget *dialog;
      dialog = gtk_message_dialog_new (GTK_WINDOW (cc_shell_get_toplevel (cc_panel_get_shell (CC_PANEL (panel)))),
                                       GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                       GTK_MESSAGE_ERROR,
                                       GTK_BUTTONS_CLOSE,
                                       _("Error removing account"));
      gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
                                                "%s",
                                                error->message);
      gtk_widget_show_all (dialog);
      gtk_dialog_run (GTK_DIALOG (dialog));
      gtk_widget_destroy (dialog);
      g_error_free (error);
    }
  g_object_unref (panel);
}

static void
on_toolbar_remove_button_clicked (GtkToolButton *button,
                                  gpointer       user_data)
{
  GoaPanel *panel = GOA_PANEL (user_data);
  GtkTreeIter iter;

  if (gtk_tree_selection_get_selected (gtk_tree_view_get_selection (GTK_TREE_VIEW (panel->accounts_treeview)),
                                       NULL,
                                       &iter))
    {
      GoaObject *object;
      GtkWidget *dialog;
      gint response;

      gtk_tree_model_get (GTK_TREE_MODEL (panel->accounts_model),
                          &iter,
                          GOA_PANEL_ACCOUNTS_MODEL_COLUMN_OBJECT, &object,
                          -1);

      dialog = gtk_message_dialog_new (GTK_WINDOW (cc_shell_get_toplevel (cc_panel_get_shell (CC_PANEL (panel)))),
                                       GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                       GTK_MESSAGE_QUESTION,
                                       GTK_BUTTONS_CANCEL,
                                       _("Are you sure you want to remove the account?"));
      gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
                                                _("This will not remove the account on the server."));
      gtk_dialog_add_button (GTK_DIALOG (dialog), _("_Remove"), GTK_RESPONSE_OK);
      gtk_widget_show_all (dialog);
      response = gtk_dialog_run (GTK_DIALOG (dialog));
      gtk_widget_destroy (dialog);

      if (response == GTK_RESPONSE_OK)
        {
          goa_account_call_remove (goa_object_peek_account (object),
                                   NULL, /* GCancellable */
                                   (GAsyncReadyCallback) remove_account_cb,
                                   g_object_ref (panel));
        }
      g_object_unref (object);
    }
}

/* ---------------------------------------------------------------------------------------------------- */

static void
on_add_button_clicked (GtkButton *button,
                       gpointer   user_data)
{
  GoaPanel *panel = GOA_PANEL (user_data);
  add_account (panel);
}
