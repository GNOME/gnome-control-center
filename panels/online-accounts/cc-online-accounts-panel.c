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
 * Public License along with this library; if not, see <http://www.gnu.org/licenses/>.
 *
 * Author: David Zeuthen <davidz@redhat.com>
 */

#include "config.h"

#include <gio/gio.h>
#include <string.h>
#include <glib/gi18n-lib.h>

#define GOA_API_IS_SUBJECT_TO_CHANGE
#include <goa/goa.h>
#define GOA_BACKEND_API_IS_SUBJECT_TO_CHANGE
#include <goabackend/goabackend.h>

#include "cc-online-accounts-panel.h"

#include "cc-online-accounts-add-account-dialog.h"
#include "cc-online-accounts-model.h"
#include "cc-online-accounts-resources.h"

struct _CcGoaPanel
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

static gboolean select_account_by_id (CcGoaPanel    *panel,
                                      const gchar *account_id);
static void     add_account          (CcGoaPanel *panel,
                                      GoaProvider *provider,
                                      GVariant *preseed);

CC_PANEL_REGISTER (CcGoaPanel, cc_goa_panel);

enum {
  PROP_0,
  PROP_PARAMETERS
};

static void
command_add (CcGoaPanel *panel,
             GVariant   *parameters)
{
  GVariant *v, *preseed = NULL;
  GoaProvider *provider = NULL;
  const gchar *provider_name = NULL;

  g_assert (panel != NULL);
  g_assert (parameters != NULL);

  switch (g_variant_n_children (parameters))
    {
      case 4:
        g_variant_get_child (parameters, 3, "v", &preseed);
      case 3:
        g_variant_get_child (parameters, 2, "v", &v);
        if (g_variant_is_of_type (v, G_VARIANT_TYPE_STRING))
          provider_name = g_variant_get_string (v, NULL);
        else
          g_warning ("Wrong type for the second argument (provider name) GVariant, expected 's' but got '%s'",
                     (gchar *)g_variant_get_type (v));
        g_variant_unref (v);
      case 2:
        /* Nothing to see here, move along */
      case 1:
        /* No flag to handle here */
        break;
      default:
        g_warning ("Unexpected parameters found, ignore request");
        goto out;
    }

  if (provider_name != NULL)
    {
      provider = goa_provider_get_for_provider_type (provider_name);
      if (provider == NULL)
        {
          g_warning ("Unable to get a provider for type '%s'", provider_name);
          goto out;
        }
    }

  add_account (panel, provider, preseed);

out:
  g_clear_object (&provider);
  g_clear_pointer (&preseed, g_variant_unref);
}

static void
cc_goa_panel_set_property (GObject *object,
                        guint property_id,
                        const GValue *value,
                        GParamSpec *pspec)
{
  switch (property_id)
    {
      case PROP_PARAMETERS:
        {
          GVariant *parameters, *v;
          const gchar *first_arg = NULL;

          parameters = g_value_get_variant (value);
          if (parameters == NULL)
            return;

          if (g_variant_n_children (parameters) > 0)
            {
                g_variant_get_child (parameters, 0, "v", &v);
                if (g_variant_is_of_type (v, G_VARIANT_TYPE_STRING))
                  first_arg = g_variant_get_string (v, NULL);
                else
                  g_warning ("Wrong type for the second argument GVariant, expected 's' but got '%s'",
                             (gchar *)g_variant_get_type (v));
                g_variant_unref (v);
            }

          if (g_strcmp0 (first_arg, "add") == 0)
            command_add (CC_GOA_PANEL (object), parameters);
          else if (first_arg != NULL)
            select_account_by_id (CC_GOA_PANEL (object), first_arg);

          return;
        }
    }

  G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
cc_goa_panel_finalize (GObject *object)
{
  CcGoaPanel *panel = CC_GOA_PANEL (object);

  g_clear_object (&panel->accounts_model);
  g_clear_object (&panel->client);
  g_clear_object (&panel->builder);

  G_OBJECT_CLASS (cc_goa_panel_parent_class)->finalize (object);
}

static void
cc_goa_panel_init (CcGoaPanel *panel)
{
  GtkWidget *button;
  GtkWidget *w;
  GError *error;
  GtkTreeViewColumn *column;
  GtkCellRenderer *renderer;
  GtkTreeIter iter;
  GNetworkMonitor *monitor;

  g_resources_register (cc_online_accounts_get_resource ());
  monitor = g_network_monitor_get_default();

  panel->builder = gtk_builder_new ();
  error = NULL;
  if (gtk_builder_add_from_resource (panel->builder,
                                     "/org/gnome/control-center/online-accounts/online-accounts.ui",
                                     &error) == 0)
    {
      g_warning ("Error loading UI file: %s (%s, %d)",
                 error->message, g_quark_to_string (error->domain), error->code);
      g_error_free (error);
      goto out;
    }

  panel->toolbar = GTK_WIDGET (gtk_builder_get_object (panel->builder, "accounts-tree-toolbar"));
  panel->toolbar_add_button = GTK_WIDGET (gtk_builder_get_object (panel->builder, "accounts-tree-toolbutton-add"));
  g_object_bind_property (monitor, "network-available",
                          panel->toolbar_add_button, "sensitive",
                          G_BINDING_SYNC_CREATE);
  g_signal_connect (panel->toolbar_add_button,
                    "clicked",
                    G_CALLBACK (on_toolbar_add_button_clicked),
                    panel);
  panel->toolbar_remove_button = GTK_WIDGET (gtk_builder_get_object (panel->builder, "accounts-tree-toolbutton-remove"));
  g_signal_connect (panel->toolbar_remove_button,
                    "clicked",
                    G_CALLBACK (on_toolbar_remove_button_clicked),
                    panel);

  panel->accounts_treeview = GTK_WIDGET (gtk_builder_get_object (panel->builder, "accounts-tree-treeview"));
  g_signal_connect (gtk_tree_view_get_selection (GTK_TREE_VIEW (panel->accounts_treeview)),
                    "changed",
                    G_CALLBACK (on_tree_view_selection_changed),
                    panel);

  button = GTK_WIDGET (gtk_builder_get_object (panel->builder, "accounts-button-add"));
  g_object_bind_property (monitor, "network-available",
                          button, "sensitive",
                          G_BINDING_SYNC_CREATE);
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
      g_warning ("Error getting a GoaClient: %s (%s, %d)",
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
                "follow-state", TRUE,
                "stock-size", GTK_ICON_SIZE_DIALOG,
                NULL);
  gtk_tree_view_column_set_attributes (column,
                                       renderer,
                                       "gicon", GOA_PANEL_ACCOUNTS_MODEL_COLUMN_ICON,
                                       NULL);

  renderer = gtk_cell_renderer_text_new ();
  gtk_tree_view_column_pack_start (column, renderer, TRUE);
  g_object_set (G_OBJECT (renderer),
                "ellipsize", PANGO_ELLIPSIZE_END,
                "ellipsize-set", TRUE,
                NULL);
  gtk_tree_view_column_set_attributes (column,
                                       renderer,
                                       "markup", GOA_PANEL_ACCOUNTS_MODEL_COLUMN_MARKUP,
                                       NULL);

  renderer = gtk_cell_renderer_pixbuf_new ();
  gtk_tree_view_column_pack_end (column, renderer, FALSE);
  g_object_set (G_OBJECT (renderer),
                "icon-name", "dialog-warning-symbolic",
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
  gtk_container_add (GTK_CONTAINER (panel), w);
  gtk_widget_show_all (w);
}

static const char *
cc_goa_panel_get_help_uri (CcPanel *panel)
{
  return "help:gnome-help/accounts";
}

static void
cc_goa_panel_class_init (CcGoaPanelClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  CcPanelClass *panel_class = CC_PANEL_CLASS (klass);

  panel_class->get_help_uri = cc_goa_panel_get_help_uri;

  object_class->set_property = cc_goa_panel_set_property;
  object_class->finalize = cc_goa_panel_finalize;

  g_object_class_override_property (object_class, PROP_PARAMETERS, "parameters");
}

/* ---------------------------------------------------------------------------------------------------- */

static void
show_page (CcGoaPanel *panel,
           gint page_num)
{
  GtkNotebook *notebook;
  notebook = GTK_NOTEBOOK (gtk_builder_get_object (panel->builder, "accounts-notebook"));
  gtk_notebook_set_current_page (notebook, page_num);
}

static void
show_page_nothing_selected (CcGoaPanel *panel)
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
show_page_account (CcGoaPanel  *panel,
                   GoaObject *object)
{
  GList *children;
  GList *l;
  GtkWidget *box;
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

  if (provider != NULL)
    {
      goa_provider_show_account (provider,
                                 panel->client,
                                 object,
                                 GTK_BOX (panel->accounts_vbox),
                                 NULL,
                                 NULL);
    }

  gtk_widget_show_all (panel->accounts_vbox);

  g_clear_object (&provider);
}

/* ---------------------------------------------------------------------------------------------------- */

static gboolean
select_account_by_id (CcGoaPanel    *panel,
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
      GtkTreePath *path;
      GtkTreeView *tree_view;
      GtkTreeSelection *selection;

      tree_view = GTK_TREE_VIEW (panel->accounts_treeview);
      selection = gtk_tree_view_get_selection (tree_view);
      gtk_tree_selection_select_iter (selection, &iter);
      path = gtk_tree_model_get_path (GTK_TREE_MODEL (panel->accounts_model), &iter);
      gtk_tree_view_scroll_to_cell (tree_view, path, NULL, FALSE, 0.0, 0.0);
      gtk_tree_path_free (path);
    }

  return iter_set;
}

static void
on_tree_view_selection_changed (GtkTreeSelection *selection,
                                gpointer          user_data)
{
  CcGoaPanel *panel = CC_GOA_PANEL (user_data);
  GtkTreeIter iter;

  if (gtk_tree_selection_get_selected (selection, NULL, &iter))
    {
      GoaObject *object;
      gboolean is_locked;

      gtk_tree_model_get (GTK_TREE_MODEL (panel->accounts_model),
                          &iter,
                          GOA_PANEL_ACCOUNTS_MODEL_COLUMN_OBJECT, &object,
                          -1);
      show_page_account (panel, object);

      is_locked = goa_account_get_is_locked (goa_object_peek_account (object));
      gtk_widget_set_sensitive (panel->toolbar_remove_button, !is_locked);

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
  CcGoaPanel *panel = CC_GOA_PANEL (user_data);
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
  CcGoaPanel *panel = CC_GOA_PANEL (user_data);
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
  CcGoaPanel *panel = CC_GOA_PANEL (user_data);
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

typedef struct
{
  CcGoaPanel *panel;
  GoaProvider *provider;
  GVariant *preseed;
} AddAccountData;

static void
get_all_providers_cb (GObject      *source,
                      GAsyncResult *res,
                      gpointer      user_data)
{
  AddAccountData *data = user_data;
  GtkWindow *parent;
  GtkWidget *dialog;
  GList *providers;
  GList *l;
  GoaObject *object;
  GError *error;

  providers = NULL;
  if (!goa_provider_get_all_finish (&providers, res, NULL))
    goto out;

  parent = GTK_WINDOW (cc_shell_get_toplevel (cc_panel_get_shell (CC_PANEL (data->panel))));

  dialog = goa_panel_add_account_dialog_new (data->panel->client);
  gtk_window_set_transient_for (GTK_WINDOW (dialog), parent);

  for (l = providers; l != NULL; l = l->next)
    {
      GoaProvider *provider;
      provider = GOA_PROVIDER (l->data);

      goa_panel_add_account_dialog_add_provider (GOA_PANEL_ADD_ACCOUNT_DIALOG (dialog), provider);
    }

  goa_panel_add_account_dialog_set_preseed_data (GOA_PANEL_ADD_ACCOUNT_DIALOG (dialog),
                                                 data->provider, data->preseed);

  gtk_widget_show_all (dialog);
  goa_panel_add_account_dialog_run (GOA_PANEL_ADD_ACCOUNT_DIALOG (dialog));

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
      if (goa_panel_accounts_model_get_iter_for_object (data->panel->accounts_model,
                                                        object,
                                                        &iter))
        {
          gtk_tree_selection_select_iter (gtk_tree_view_get_selection (GTK_TREE_VIEW (data->panel->accounts_treeview)),
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

  g_list_free_full (providers, g_object_unref);

out:
  g_clear_object (&data->panel);
  g_clear_object (&data->provider);
  g_clear_pointer (&data->preseed, g_variant_unref);
  g_slice_free (AddAccountData, data);
}

static void
add_account (CcGoaPanel *panel,
             GoaProvider *provider,
             GVariant *preseed)
{
  AddAccountData *data;

  data = g_slice_new0 (AddAccountData);
  data->panel = g_object_ref_sink (panel);
  data->provider = (provider != NULL ? g_object_ref (provider) : NULL);
  data->preseed = (preseed != NULL ? g_variant_ref (preseed) : NULL);
  goa_provider_get_all (get_all_providers_cb, data);
}

/* ---------------------------------------------------------------------------------------------------- */

static void
on_toolbar_add_button_clicked (GtkToolButton *button,
                               gpointer       user_data)
{
  CcGoaPanel *panel = CC_GOA_PANEL (user_data);
  add_account (panel, NULL, NULL);
}

static void
remove_account_cb (GoaAccount    *account,
                   GAsyncResult  *res,
                   gpointer       user_data)
{
  CcGoaPanel *panel = CC_GOA_PANEL (user_data);
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
  CcGoaPanel *panel = CC_GOA_PANEL (user_data);
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
  CcGoaPanel *panel = CC_GOA_PANEL (user_data);
  add_account (panel, NULL, NULL);
}
