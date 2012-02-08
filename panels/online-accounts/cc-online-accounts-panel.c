/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/*
 * Copyright (C) 2011 Red Hat, Inc.
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

static void on_tree_view_selection_changed (GtkTreeSelection *selection,
                                            gpointer          user_data);

static void on_toolbar_add_button_clicked (GtkToolButton *button,
                                           gpointer       user_data);
static void on_toolbar_remove_button_clicked (GtkToolButton *button,
                                              gpointer       user_data);

static void on_account_changed (GoaClient  *client,
                                GoaObject  *object,
                                gpointer    user_data);

G_DEFINE_DYNAMIC_TYPE (GoaPanel, goa_panel, CC_TYPE_PANEL);

static void
goa_panel_finalize (GObject *object)
{
  GoaPanel *panel = GOA_PANEL (object);

  g_signal_handlers_disconnect_by_func (gtk_tree_view_get_selection (GTK_TREE_VIEW (panel->accounts_treeview)),
                                        G_CALLBACK (on_tree_view_selection_changed),
                                        panel);
  if (panel->accounts_model != NULL)
    g_object_unref (panel->accounts_model);
  if (panel->client != NULL)
    {
      g_signal_handlers_disconnect_by_func (panel->client,
                                            G_CALLBACK (on_account_changed),
                                            panel);
      g_object_unref (panel->client);
    }
  g_object_unref (panel->builder);

  G_OBJECT_CLASS (goa_panel_parent_class)->finalize (object);
}

static void
goa_panel_init (GoaPanel *panel)
{
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

static void
goa_panel_class_init (GoaPanelClass *klass)
{
}

static void
goa_panel_class_finalize (GoaPanelClass *klass)
{
  GObjectClass *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = goa_panel_finalize;
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
  show_page (panel, 0);
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
  GtkWidget *table;
  GtkWidget *bar;
  GtkWidget *label;
  GoaProvider *provider;
  GoaAccount *account;
  const gchar *provider_type;

  provider = NULL;

  show_page (panel, 1);

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

  table = gtk_table_new (3, 2, FALSE);
  gtk_table_set_row_spacings (GTK_TABLE (table), 0);
  gtk_table_set_col_spacings (GTK_TABLE (table), 4);
  gtk_box_pack_start (GTK_BOX (panel->accounts_vbox), table, FALSE, TRUE, 0);

  if (provider != NULL)
    {
      goa_provider_show_account (provider,
                                 panel->client,
                                 object,
                                 GTK_BOX (panel->accounts_vbox),
                                 GTK_TABLE (table));
    }

  gtk_widget_show_all (panel->accounts_vbox);

  if (provider != NULL)
    g_object_unref (provider);
}

/* ---------------------------------------------------------------------------------------------------- */

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
on_toolbar_add_button_clicked (GtkToolButton *button,
                               gpointer       user_data)
{
  GoaPanel *panel = GOA_PANEL (user_data);
  GtkWindow *parent;
  GtkWidget *add_account_button;
  GtkWidget *dialog;
  GtkWidget *vbox;
  GtkWidget *label;
  GtkWidget *grid;
  GtkWidget *combo_box;
  gint response;
  GList *providers;
  GoaProvider *provider;
  GList *children;
  GList *l;
  GoaObject *object;
  GError *error;

  provider = NULL;
  providers = NULL;

  parent = GTK_WINDOW (cc_shell_get_toplevel (cc_panel_get_shell (CC_PANEL (panel))));

  dialog = gtk_dialog_new ();
  gtk_container_set_border_width (GTK_CONTAINER (dialog), 12);
  gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);
  gtk_window_set_resizable (GTK_WINDOW (dialog), FALSE);
  gtk_window_set_transient_for (GTK_WINDOW (dialog), parent);
  /* translators: This is the title of the "Add Account" dialogue.
   * The title is not visible when using GNOME Shell */
  gtk_window_set_title (GTK_WINDOW (dialog), _("Add Account"));

  vbox = gtk_dialog_get_content_area (GTK_DIALOG (dialog));
  gtk_box_set_spacing (GTK_BOX (vbox), 12);

  label = gtk_label_new (_("To add a new account, first select the account type"));
  gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, TRUE, 0);

  label = gtk_label_new (_("Account Type:"));
  combo_box = gtk_combo_box_text_new ();
  providers = goa_provider_get_all ();
  for (l = providers; l != NULL; l = l->next)
    {
      gchar *provider_name;

      provider = GOA_PROVIDER (l->data);
      provider_name = goa_provider_get_provider_name (provider, NULL);
      gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (combo_box),
                                 goa_provider_get_provider_type (provider),
                                 provider_name);
      g_free (provider_name);
      provider = NULL;
    }
  gtk_combo_box_set_active (GTK_COMBO_BOX (combo_box), 0);

  grid = gtk_grid_new ();
  gtk_grid_set_column_spacing (GTK_GRID (grid), 10);
  gtk_box_pack_start (GTK_BOX (vbox), grid, FALSE, TRUE, 0);

  gtk_grid_attach (GTK_GRID (grid), label, 0, 0, 1, 1);
  gtk_widget_set_hexpand (combo_box, TRUE);
  gtk_grid_attach (GTK_GRID (grid), combo_box, 1, 0, 1, 1);

  gtk_dialog_add_button (GTK_DIALOG (dialog),
                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL);
  add_account_button = gtk_dialog_add_button (GTK_DIALOG (dialog),
                                              _("_Add..."), GTK_RESPONSE_OK);

  gtk_widget_show_all (dialog);
  response = gtk_dialog_run (GTK_DIALOG (dialog));
  if (response != GTK_RESPONSE_OK)
    {
      gtk_widget_destroy (dialog);
      goto out;
    }
  gtk_container_remove (GTK_CONTAINER (gtk_widget_get_parent (add_account_button)), add_account_button);

  provider = goa_provider_get_for_provider_type (gtk_combo_box_get_active_id (GTK_COMBO_BOX (combo_box)));
  g_assert (provider != NULL);

  /* Prepare GtkDialog for the provider */
  children = gtk_container_get_children (GTK_CONTAINER (vbox));
  for (l = children; l != NULL; l = l->next)
    {
      GtkWidget *child = GTK_WIDGET (l->data);
      if (child != gtk_dialog_get_action_area (GTK_DIALOG (dialog)))
        gtk_container_remove (GTK_CONTAINER (vbox), child);
    }
  g_list_free (children);

  error = NULL;
  object = goa_provider_add_account (provider,
                                     panel->client,
                                     GTK_DIALOG (dialog),
                                     GTK_BOX (vbox),
                                     &error);
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
      gtk_widget_destroy (dialog);
    }
  else
    {
      gtk_widget_destroy (dialog);
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
  if (provider != NULL)
    g_object_unref (provider);
  g_list_foreach (providers, (GFunc) g_object_unref, NULL);
  g_list_free (providers);
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
