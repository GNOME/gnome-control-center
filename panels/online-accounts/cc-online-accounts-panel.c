/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/*
 * Copyright (C) 2011 - 2017 Red Hat, Inc.
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

#include "cc-online-accounts-panel.h"
#include "cc-online-account-provider-row.h"
#include "cc-online-account-row.h"
#include "cc-online-accounts-resources.h"

#define GOA_API_IS_SUBJECT_TO_CHANGE
#define GOA_BACKEND_API_IS_SUBJECT_TO_CHANGE
#include <goabackend/goabackend.h>

struct _CcOnlineAccountsPanel
{
  CcPanel parent_instance;

  GtkFrame      *accounts_frame;
  GtkListBox    *accounts_listbox;
  GtkWidget     *close_notification_button;
  GtkDialog     *edit_account_dialog;
  GtkHeaderBar  *edit_account_headerbar;
  GtkBox        *editor_box;
  GtkLabel      *notification_label;
  GtkRevealer   *notification_revealer;
  AdwBanner     *offline_banner;
  GtkListBox    *providers_listbox;
  GtkButton     *remove_account_button;
  GtkBox        *accounts_vbox;

  GoaClient *client;
  GoaObject *active_object;
  GoaObject *removed_object;
  GVariant  *parameters;

  guint      remove_account_timeout_id;
};

static gboolean remove_account_timeout_cb (gpointer user_data);

CC_PANEL_REGISTER (CcOnlineAccountsPanel, cc_online_accounts_panel);

enum {
  PROP_0,
  PROP_PARAMETERS
};

/* Rows methods */

typedef void (*RowForAccountCallback) (CcOnlineAccountsPanel *self, GtkWidget *row, GList *other_rows);

static void
hide_row_for_account_cb (CcOnlineAccountsPanel *self,
                         GtkWidget             *row,
                         GList                 *other_rows)
{
  gtk_widget_set_visible (row, FALSE);
  gtk_widget_set_visible (GTK_WIDGET (self->accounts_frame), other_rows != NULL);
}

static void
remove_row_for_account_cb (CcOnlineAccountsPanel *self,
                           GtkWidget             *row,
                           GList                 *other_rows)
{
  gtk_list_box_remove (self->accounts_listbox, row);
  gtk_widget_set_visible (GTK_WIDGET (self->accounts_frame), other_rows != NULL);
}

static void
show_row_for_account_cb (CcOnlineAccountsPanel *self,
                         GtkWidget             *row,
                         GList                 *other_rows)
{
  gtk_widget_set_visible (row, TRUE);
  gtk_widget_set_visible (GTK_WIDGET (self->accounts_frame), TRUE);
}

static void
modify_row_for_account (CcOnlineAccountsPanel *self,
                        GoaObject             *object,
                        RowForAccountCallback  callback)
{
  GtkWidget *child;
  GList *children = NULL;
  GList *l;

  for (child = gtk_widget_get_first_child (GTK_WIDGET (self->accounts_listbox));
       child;
       child = gtk_widget_get_next_sibling (child))
    {
      children = g_list_prepend (children, child);
    }

  children = g_list_reverse (children);

  for (l = children; l != NULL; l = l->next)
    {
      GoaObject *row_object;

      row_object = cc_online_account_row_get_object (CC_ONLINE_ACCOUNT_ROW (l->data));
      if (row_object == object)
        {
          GtkWidget *row = GTK_WIDGET (l->data);

          children = g_list_remove_link (children, l);
          callback (self, row, children);
          g_list_free (l);
          break;
        }
    }

  g_list_free (children);
}

static void
cancel_notification_timeout (CcOnlineAccountsPanel *self)
{
  g_clear_handle_id (&self->remove_account_timeout_id, g_source_remove);
  self->removed_object = NULL;
}

static void
start_remove_account_timeout (CcOnlineAccountsPanel *self)
{
  GoaAccount *account;
  g_autofree gchar *id = NULL;
  g_autofree gchar *label = NULL;

  if (self->active_object == NULL)
    return;

  if (self->removed_object != NULL)
    gtk_widget_activate (self->close_notification_button);

  self->removed_object = g_steal_pointer (&self->active_object);

  account = goa_object_peek_account (self->removed_object);
  id = g_strdup_printf ("<b>%s</b>", goa_account_get_presentation_identity (account));
  /* Translators: The %s is the username (eg., debarshi.ray@gmail.com
   * or rishi).
   */
  label = g_strdup_printf (_("%s removed"), id);
  gtk_label_set_markup (self->notification_label, label);
  gtk_revealer_set_reveal_child (self->notification_revealer, TRUE);

  modify_row_for_account (self, self->removed_object, hide_row_for_account_cb);
  self->remove_account_timeout_id = g_timeout_add_seconds (10, remove_account_timeout_cb, self);
}

static void
on_remove_button_clicked_cb (CcOnlineAccountsPanel *self,
                             GtkButton *button)
{
  GtkRoot *root;

  start_remove_account_timeout (self);
  self->active_object = NULL;

  root = gtk_widget_get_root (GTK_WIDGET (button));
  if (root != NULL)
    gtk_window_destroy (GTK_WINDOW (root));
}

static void
show_account (CcOnlineAccountsPanel *self,
              GoaObject             *object)
{
  g_autoptr(GoaProvider) provider = NULL;
  g_autofree char *title = NULL;
  GoaAccount *account;
  GtkWidget *content_area;
  GtkWidget *button;
  GtkWidget *dialog;
  GtkWidget *box;
  const char *provider_type;

  g_set_object (&self->active_object, object);
  account = goa_object_peek_account (object);

  /* Find the provider with a matching type */
  account = goa_object_get_account (object);
  provider_type = goa_account_get_provider_type (account);
  provider = goa_provider_get_for_provider_type (provider_type);
  if (provider == NULL)
    {
      g_warning ("Error showing account: Unsupported provider");
      return;
    }

  dialog = g_object_new (GTK_TYPE_DIALOG,
                         "use-header-bar", 1,
                         "modal", TRUE,
                         "transient-for", gtk_widget_get_root (GTK_WIDGET (self)),
                         NULL);
  /* Keep account alive so that the switches are still bound to it */
  g_object_set_data_full (G_OBJECT (dialog), "goa-account", account, g_object_unref);

  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 42);
  gtk_widget_set_margin_bottom (box, 24);

  content_area = gtk_dialog_get_content_area (GTK_DIALOG (dialog));
  g_object_set (content_area,
                "margin-top", 6,
                "margin-end", 6,
                "margin-bottom", 6,
                "margin-start", 6,
                NULL);
  gtk_box_append (GTK_BOX (content_area), box);

  goa_provider_show_account (provider,
                             self->client,
                             object,
                             GTK_BOX (box),
                             NULL,
                             NULL);

  /* translators: This is the title of the "Show Account" dialog. The
   * %s is the name of the provider. e.g., 'Google'. */
  title = g_strdup_printf (_("%s Account"), goa_account_get_provider_name (account));
  gtk_window_set_title (GTK_WINDOW (dialog), title);

  button = gtk_button_new_with_label (_("Remove Account"));
  gtk_widget_set_margin_start (box, 24);
  gtk_widget_set_margin_end (box, 24);
  gtk_widget_set_halign (button, GTK_ALIGN_END);
  gtk_widget_set_valign (button, GTK_ALIGN_END);
  gtk_widget_set_visible (button, !goa_account_get_is_locked (account));
  gtk_widget_add_css_class (button, "destructive-action");
  gtk_box_append (GTK_BOX (box), button);
  g_signal_connect_swapped (button,
                            "clicked",
                            G_CALLBACK (on_remove_button_clicked_cb),
                            self);

  gtk_window_present (GTK_WINDOW (dialog));
}

static void
create_account (CcOnlineAccountsPanel *self,
                GoaProvider           *provider)
{
  g_autoptr(GError) error = NULL;
  GtkWidget *dialog;
  GtkWidget *content;
  GoaObject *object;

  g_return_if_fail (GOA_IS_PROVIDER (provider));

  dialog = g_object_new (GTK_TYPE_DIALOG,
                         "use-header-bar", 1,
                         "default-width", 500,
                         "default-height", 350,
                         "modal", TRUE,
                         "transient-for", gtk_widget_get_root (GTK_WIDGET (self)),
                         NULL);

  content = gtk_dialog_get_content_area (GTK_DIALOG (dialog));
  object = goa_provider_add_account (provider,
                                     self->client,
                                     GTK_DIALOG (dialog),
                                     GTK_BOX (content),
                                     &error);
  gtk_window_destroy (GTK_WINDOW (dialog));

  if (error)
    {
      g_warning ("Error creating account: %s", error->message);
      return;
    }

  show_account (self, object);
}

static void
select_account_by_id (CcOnlineAccountsPanel  *self,
                      const gchar            *account_id)
{
  GtkWidget *child;

  for (child = gtk_widget_get_first_child (GTK_WIDGET (self->accounts_listbox));
       child;
       child = gtk_widget_get_next_sibling (child))
    {
      GoaAccount *account;
      GoaObject *row_object;

      row_object = cc_online_account_row_get_object (CC_ONLINE_ACCOUNT_ROW (child));
      account = goa_object_peek_account (row_object);

      if (g_strcmp0 (goa_account_get_id (account), account_id) == 0)
        {
          show_account (self, row_object);
          break;
        }
    }
}

static void
command_add (CcOnlineAccountsPanel *self,
             GVariant              *parameters)
{
  const gchar *provider_name = NULL;
  g_autoptr(GVariant) v = NULL;

  g_assert (self != NULL);
  g_assert (parameters != NULL);

  switch (g_variant_n_children (parameters))
    {
      case 2:
        g_variant_get_child (parameters, 1, "v", &v);
        if (g_variant_is_of_type (v, G_VARIANT_TYPE_STRING))
          provider_name = g_variant_get_string (v, NULL);
        else
          g_warning ("Wrong type for the second argument (provider name) GVariant, expected 's' but got '%s'",
                     (gchar *)g_variant_get_type (v));
        break;

      default:
        g_warning ("Unexpected parameters found, ignore request");
        return;
    }

  if (provider_name != NULL)
    {
      GtkWidget *child;
      GoaProvider *provider;

      for (child = gtk_widget_get_first_child (GTK_WIDGET (self->providers_listbox));
           child;
           child = gtk_widget_get_next_sibling (child))
        {
          const char *provider_type = NULL;

          provider = cc_online_account_provider_row_get_provider (CC_ONLINE_ACCOUNT_PROVIDER_ROW (child));
          provider_type = goa_provider_get_provider_type (provider);

          if (g_strcmp0 (provider_type, provider_name) == 0)
            break;
        }

      if (child == NULL)
        {
          g_warning ("Unable to get a provider for type '%s'", provider_name);
          return;
        }

      create_account (self, provider);
    }
}

static void
load_custom_css (void)
{
  g_autoptr(GtkCssProvider) provider = NULL;

  provider = gtk_css_provider_new ();
  gtk_css_provider_load_from_resource (provider, "/org/gnome/control-center/online-accounts/online-accounts.css");
  gtk_style_context_add_provider_for_display (gdk_display_get_default (),
                                              GTK_STYLE_PROVIDER (provider),
                                              GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
}

/* Callbacks */

static gint
sort_accounts_func (GtkListBoxRow *a,
                    GtkListBoxRow *b,
                    gpointer       user_data)
{
  GoaAccount *a_account, *b_account;
  GoaObject *a_object, *b_object;

  a_object = cc_online_account_row_get_object (CC_ONLINE_ACCOUNT_ROW (a));
  a_account = goa_object_peek_account (a_object);

  b_object = cc_online_account_row_get_object (CC_ONLINE_ACCOUNT_ROW (b));
  b_account = goa_object_peek_account (b_object);

  return g_strcmp0 (goa_account_get_id (a_account), goa_account_get_id (b_account));
}

static gint
sort_providers_func (GtkListBoxRow *a,
                     GtkListBoxRow *b,
                     gpointer       user_data)
{
  GoaProvider *a_provider, *b_provider;
  gboolean a_branded, b_branded;
  GoaProviderFeatures a_features, b_features;

  a_provider = cc_online_account_provider_row_get_provider (CC_ONLINE_ACCOUNT_PROVIDER_ROW (a));
  a_features = goa_provider_get_provider_features (a_provider);
  a_branded = (a_features & GOA_PROVIDER_FEATURE_BRANDED) != 0;

  b_provider = cc_online_account_provider_row_get_provider (CC_ONLINE_ACCOUNT_PROVIDER_ROW (b));
  b_features = goa_provider_get_provider_features (b_provider);
  b_branded = (b_features & GOA_PROVIDER_FEATURE_BRANDED) != 0;

  if (a_branded != b_branded)
    return a_branded ? -1 : 1;

  return gtk_list_box_row_get_index (b) - gtk_list_box_row_get_index (a);
}

static void
add_account (CcOnlineAccountsPanel *self,
             GoaObject             *object)
{
  CcOnlineAccountRow *row;

  row = cc_online_account_row_new (object);
  gtk_list_box_append (self->accounts_listbox, GTK_WIDGET (row));
  gtk_widget_set_visible (GTK_WIDGET (self->accounts_frame), TRUE);
}

static void
add_provider (CcOnlineAccountsPanel *self,
              GoaProvider           *provider)
{
  CcOnlineAccountProviderRow *row;

  row = cc_online_account_provider_row_new (provider);
  gtk_list_box_append (self->providers_listbox, GTK_WIDGET (row));
}

static void
on_account_added_cb (CcOnlineAccountsPanel *self,
                     GoaObject             *object)
{
  add_account (self, object);
}

static void
on_account_changed_cb (CcOnlineAccountsPanel *self,
                       GoaObject             *object)
{
  if (self->active_object == object)
    show_account (self, self->active_object);
}

static void
on_account_removed_cb (CcOnlineAccountsPanel *self,
                       GoaObject             *object)
{
  modify_row_for_account (self, object, remove_row_for_account_cb);
}

static void
on_accounts_listbox_row_activated (CcOnlineAccountsPanel *self,
                                   GtkListBoxRow         *activated_row)
{
  GoaObject *object = cc_online_account_row_get_object (CC_ONLINE_ACCOUNT_ROW (activated_row));

  show_account (self, object);
}

static void
on_client_remove_account_finish_cb (GoaAccount   *account,
                                    GAsyncResult *res,
                                    gpointer      user_data)
{
  g_autoptr(CcOnlineAccountsPanel) self = CC_ONLINE_ACCOUNTS_PANEL (user_data);
  g_autoptr(GError) error = NULL;

  goa_account_call_remove_finish (account, res, &error);

  if (error)
    {
      GtkWidget *dialog;
      dialog = gtk_message_dialog_new (GTK_WINDOW (cc_shell_get_toplevel (cc_panel_get_shell (CC_PANEL (self)))),
                                       GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                       GTK_MESSAGE_ERROR,
                                       GTK_BUTTONS_CLOSE,
                                       _("Error removing account"));
      gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
                                                "%s",
                                                error->message);
      gtk_window_present (GTK_WINDOW (dialog));
    }
}

static void
on_notification_closed_cb (CcOnlineAccountsPanel *self)
{
  if (self->removed_object != NULL)
    {
      goa_account_call_remove (goa_object_peek_account (self->removed_object),
                               cc_panel_get_cancellable (CC_PANEL (self)),
                               (GAsyncReadyCallback) on_client_remove_account_finish_cb,
                               g_object_ref (self));
    }

  gtk_revealer_set_reveal_child (self->notification_revealer, FALSE);

  cancel_notification_timeout (self);
  self->removed_object = NULL;
}

static void
on_undo_button_clicked_cb (CcOnlineAccountsPanel *self)
{
  /* Simply show the account row and hide the notification */
  modify_row_for_account (self, self->removed_object, show_row_for_account_cb);
  gtk_revealer_set_reveal_child (self->notification_revealer, FALSE);

  cancel_notification_timeout (self);
  self->removed_object = NULL;
}

static void
on_provider_row_activated_cb (CcOnlineAccountsPanel *self,
                              GtkListBoxRow         *activated_row)
{
  GoaProvider *provider = cc_online_account_provider_row_get_provider (CC_ONLINE_ACCOUNT_PROVIDER_ROW (activated_row));

  create_account (self, provider);
}

static gboolean
remove_account_timeout_cb (gpointer user_data)
{
  CcOnlineAccountsPanel *self = CC_ONLINE_ACCOUNTS_PANEL (user_data);

  gtk_widget_activate (self->close_notification_button);
  self->remove_account_timeout_id = 0;

  return G_SOURCE_REMOVE;
}

static void
goa_provider_get_all_cb (GObject      *object,
                         GAsyncResult *res,
                         gpointer      user_data)
{
  g_autoptr (CcOnlineAccountsPanel) self = CC_ONLINE_ACCOUNTS_PANEL (user_data);
  g_autolist(GoaProvider) providers = NULL;
  g_autolist(GoaAccount) accounts = NULL;
  g_autoptr (GError) error = NULL;

  /* goa_provider_get_all() doesn't have a cancellable argument, so check if
   * the panel cancellable was triggered.
   */
  if (g_cancellable_is_cancelled (cc_panel_get_cancellable (CC_PANEL (self))))
    return;

  if (!goa_provider_get_all_finish (&providers, res, &error))
    {
      g_warning ("Error listing providers: %s", error->message);
      return;
    }

  for (const GList *iter = providers; iter != NULL; iter = iter->next)
    add_provider (self, GOA_PROVIDER (iter->data));

  /* Load existing accounts */
  accounts = goa_client_get_accounts (self->client);

  for (const GList *iter = accounts; iter != NULL; iter = iter->next)
    add_account (self, GOA_OBJECT (iter->data));

  g_signal_connect_swapped (self->client,
                            "account-added",
                            G_CALLBACK (on_account_added_cb),
                            self);

  g_signal_connect_swapped (self->client,
                            "account-changed",
                            G_CALLBACK (on_account_changed_cb),
                            self);

  g_signal_connect_swapped (self->client,
                            "account-removed",
                            G_CALLBACK (on_account_removed_cb),
                            self);

  /* With the client ready, check if we have a pending command */
  gtk_widget_set_sensitive (GTK_WIDGET (self), TRUE);

  if (self->parameters != NULL)
    {
      g_autoptr (GVariant) parameters = NULL;

      parameters = g_steal_pointer (&self->parameters);
      g_object_set (self, "parameters", parameters, NULL);
    }
}

static void
goa_client_new_cb (GObject      *object,
                   GAsyncResult *res,
                   gpointer      user_data)
{
  g_autoptr (CcOnlineAccountsPanel) self = CC_ONLINE_ACCOUNTS_PANEL (user_data);
  g_autoptr (GError) error = NULL;

  self->client = goa_client_new_finish (res, &error);
  if (self->client == NULL)
    {
      g_warning ("Error connect to service: %s", error->message);
      gtk_widget_set_sensitive (GTK_WIDGET (self), FALSE);
      return;
    }

  goa_provider_get_all (goa_provider_get_all_cb, g_object_ref (self));
}

/* CcPanel overrides */

static const char *
cc_online_accounts_panel_get_help_uri (CcPanel *panel)
{
  return "help:gnome-help/accounts";
}

/* GObject overrides */

static void
cc_online_accounts_panel_set_property (GObject      *object,
                                       guint         property_id,
                                       const GValue *value,
                                       GParamSpec   *pspec)
{
  CcOnlineAccountsPanel *self = CC_ONLINE_ACCOUNTS_PANEL (object);

  switch (property_id)
    {
      case PROP_PARAMETERS:
        {
          GVariant *parameters;
          g_autoptr(GVariant) v = NULL;
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
            }

          /* Waiting for the client to load */
          if (!gtk_widget_get_sensitive (GTK_WIDGET (self)))
            {
              g_clear_pointer (&self->parameters, g_variant_unref);
              self->parameters = g_value_dup_variant (value);
            }
          else if (g_strcmp0 (first_arg, "add") == 0)
            {
              command_add (CC_ONLINE_ACCOUNTS_PANEL (object), self->parameters);
            }
          else if (first_arg != NULL)
            {
              select_account_by_id (CC_ONLINE_ACCOUNTS_PANEL (object), first_arg);
            }

          return;
        }
    }

  G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
cc_online_accounts_panel_finalize (GObject *object)
{
  CcOnlineAccountsPanel *self = CC_ONLINE_ACCOUNTS_PANEL (object);

  if (self->removed_object != NULL)
    {
      g_autoptr(GError) error = NULL;
      goa_account_call_remove_sync (goa_object_peek_account (self->removed_object),
                                    NULL, /* GCancellable */
                                    &error);

      if (error != NULL)
        {
          g_warning ("Error removing account: %s (%s, %d)",
                     error->message,
                     g_quark_to_string (error->domain),
                     error->code);
        }
    }

  g_clear_object (&self->client);
  g_clear_pointer (&self->parameters, g_variant_unref);

  G_OBJECT_CLASS (cc_online_accounts_panel_parent_class)->finalize (object);
}

static void
cc_online_accounts_panel_class_init (CcOnlineAccountsPanelClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  CcPanelClass *panel_class = CC_PANEL_CLASS (klass);

  panel_class->get_help_uri = cc_online_accounts_panel_get_help_uri;

  object_class->set_property = cc_online_accounts_panel_set_property;
  object_class->finalize = cc_online_accounts_panel_finalize;

  g_object_class_override_property (object_class, PROP_PARAMETERS, "parameters");

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/online-accounts/cc-online-accounts-panel.ui");

  gtk_widget_class_bind_template_child (widget_class, CcOnlineAccountsPanel, accounts_frame);
  gtk_widget_class_bind_template_child (widget_class, CcOnlineAccountsPanel, accounts_listbox);
  gtk_widget_class_bind_template_child (widget_class, CcOnlineAccountsPanel, close_notification_button);
  gtk_widget_class_bind_template_child (widget_class, CcOnlineAccountsPanel, notification_label);
  gtk_widget_class_bind_template_child (widget_class, CcOnlineAccountsPanel, notification_revealer);
  gtk_widget_class_bind_template_child (widget_class, CcOnlineAccountsPanel, offline_banner);
  gtk_widget_class_bind_template_child (widget_class, CcOnlineAccountsPanel, providers_listbox);

  gtk_widget_class_bind_template_callback (widget_class, on_accounts_listbox_row_activated);
  gtk_widget_class_bind_template_callback (widget_class, on_notification_closed_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_provider_row_activated_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_undo_button_clicked_cb);
}

static void
cc_online_accounts_panel_init (CcOnlineAccountsPanel *self)
{
  GNetworkMonitor *monitor;

  g_resources_register (cc_online_accounts_get_resource ());

  gtk_widget_init_template (GTK_WIDGET (self));

  gtk_list_box_set_sort_func (self->accounts_listbox,
                              sort_accounts_func,
                              self,
                              NULL);

  gtk_list_box_set_sort_func (self->providers_listbox,
                              sort_providers_func,
                              self,
                              NULL);

  monitor = g_network_monitor_get_default();
  g_object_bind_property (monitor,
                          "network-available",
                          self->offline_banner,
                          "revealed",
                          G_BINDING_SYNC_CREATE | G_BINDING_INVERT_BOOLEAN);

  g_object_bind_property (monitor,
                          "network-available",
                          self->providers_listbox,
                          "sensitive",
                          G_BINDING_SYNC_CREATE);

  load_custom_css ();

  /* Disable the panel while we wait for the client */
  gtk_widget_set_sensitive (GTK_WIDGET (self), FALSE);
  goa_client_new (cc_panel_get_cancellable (CC_PANEL (self)),
                  goa_client_new_cb,
                  g_object_ref (self));
}
