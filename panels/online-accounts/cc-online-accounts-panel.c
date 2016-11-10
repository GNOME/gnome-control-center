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
#include "cc-online-accounts-resources.h"

#include "shell/list-box-helper.h"

struct _CcGoaPanel
{
  CcPanel parent_instance;

  GoaClient *client;
  GoaObject *active_object;
  GoaObject *removed_object;

  GtkWidget *accounts_listbox;
  GtkWidget *edit_account_dialog;
  GtkWidget *edit_account_headerbar;
  GtkWidget *new_account_vbox;
  GtkWidget *notification_label;
  GtkWidget *notification_revealer;
  GtkWidget *providers_listbox;
  GtkWidget *stack;
  GtkWidget *accounts_vbox;

  guint      remove_account_timeout_id;
};

static gboolean on_edit_account_dialog_delete_event (CcGoaPanel *self);

static void on_listbox_row_activated (CcGoaPanel    *self,
                                      GtkListBoxRow *activated_row);

static void fill_accounts_listbox (CcGoaPanel *self);

static void on_account_added (GoaClient  *client,
                              GoaObject  *object,
                              gpointer    user_data);

static void on_account_changed (GoaClient  *client,
                                GoaObject  *object,
                                gpointer    user_data);

static void on_account_removed (GoaClient  *client,
                                GoaObject  *object,
                                gpointer    user_data);

static void select_account_by_id (CcGoaPanel    *panel,
                                  const gchar   *account_id);

static void get_all_providers_cb (GObject      *source,
                                  GAsyncResult *res,
                                  gpointer      user_data);

static void show_page_account (CcGoaPanel *panel,
                               GoaObject  *object);

static void on_remove_button_clicked (CcGoaPanel *self);

static void on_notification_closed (GtkButton  *button,
                                    CcGoaPanel *self);

static void on_undo_button_clicked (GtkButton  *button,
                                    CcGoaPanel *self);

CC_PANEL_REGISTER (CcGoaPanel, cc_goa_panel);

enum {
  PROP_0,
  PROP_PARAMETERS
};

/* ---------------------------------------------------------------------------------------------------- */

static void
reset_headerbar (CcGoaPanel *self)
{
  gtk_header_bar_set_title (GTK_HEADER_BAR (self->edit_account_headerbar), NULL);
  gtk_header_bar_set_subtitle (GTK_HEADER_BAR (self->edit_account_headerbar), NULL);
  gtk_header_bar_set_show_close_button (GTK_HEADER_BAR (self->edit_account_headerbar), TRUE);

  /* Remove any leftover widgets */
  gtk_container_foreach (GTK_CONTAINER (self->edit_account_headerbar),
                         (GtkCallback) gtk_widget_destroy,
                         NULL);

}

/* ---------------------------------------------------------------------------------------------------- */

static void
add_provider_row (CcGoaPanel  *self,
                  GoaProvider *provider)
{
  GIcon *icon;
  GtkWidget *image;
  GtkWidget *label;
  GtkWidget *row;
  GtkWidget *row_grid;
  gchar *markup;
  gchar *name;

  row = gtk_list_box_row_new ();
  gtk_container_add (GTK_CONTAINER (self->providers_listbox), row);

  row_grid = gtk_grid_new ();
  gtk_orientable_set_orientation (GTK_ORIENTABLE (row_grid), GTK_ORIENTATION_HORIZONTAL);
  gtk_grid_set_column_spacing (GTK_GRID (row_grid), 6);
  gtk_container_add (GTK_CONTAINER (row), row_grid);

  if (provider == NULL)
    {
      g_object_set_data (G_OBJECT (row), "goa-provider", NULL);
      icon = g_themed_icon_new_with_default_fallbacks ("goa-account");
      name = g_strdup (C_("Online Account", "Other"));
    }
  else
    {
      g_object_set_data_full (G_OBJECT (row), "goa-provider", g_object_ref (provider), g_object_unref);
      icon = goa_provider_get_provider_icon (provider, NULL);
      name = goa_provider_get_provider_name (provider, NULL);
    }

  image = gtk_image_new_from_gicon (icon, GTK_ICON_SIZE_DIALOG);
  gtk_container_add (GTK_CONTAINER (row_grid), image);

  markup = g_strdup_printf ("<b>%s</b>", name);
  label = gtk_label_new (NULL);
  gtk_label_set_markup (GTK_LABEL (label), markup);
  gtk_container_add (GTK_CONTAINER (row_grid), label);

  gtk_widget_show_all (row);

  g_free (markup);
  g_free (name);
  g_object_unref (icon);
}

static void
on_provider_row_activated (CcGoaPanel    *self,
                           GtkListBoxRow *activated_row)
{
  GoaProvider *provider;
  GoaObject *object;
  GError *error;

  error = NULL;
  provider = g_object_get_data (G_OBJECT (activated_row), "goa-provider");

  gtk_container_foreach (GTK_CONTAINER (self->new_account_vbox),
                         (GtkCallback) gtk_widget_destroy,
                         NULL);

  reset_headerbar (self);

  /* Move to the new account page */
  gtk_stack_set_visible_child_name (GTK_STACK (self->stack), "new-account");

  /* This spins gtk_dialog_run() */
  object = goa_provider_add_account (provider,
                                     self->client,
                                     GTK_DIALOG (self->edit_account_dialog),
                                     GTK_BOX (self->new_account_vbox),
                                     &error);

  if (object == NULL)
    gtk_widget_hide (self->edit_account_dialog);
  else
    show_page_account (self, object);
}

/* ---------------------------------------------------------------------------------------------------- */

static gint
sort_func (GtkListBoxRow *a,
           GtkListBoxRow *b,
           gpointer       user_data)
{
  GoaObject *a_obj, *b_obj;
  GoaAccount *a_account, *b_account;

  a_obj = g_object_get_data (G_OBJECT (a), "goa-object");
  a_account = goa_object_peek_account (a_obj);

  b_obj = g_object_get_data (G_OBJECT (b), "goa-object");
  b_account = goa_object_peek_account (b_obj);

  return g_strcmp0 (goa_account_get_id (a_account), goa_account_get_id (b_account));
}

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

  g_clear_object (&panel->client);

  G_OBJECT_CLASS (cc_goa_panel_parent_class)->finalize (object);
}

static void
cc_goa_panel_init (CcGoaPanel *panel)
{
  GError *error;
  GNetworkMonitor *monitor;

  g_resources_register (cc_online_accounts_get_resource ());

  gtk_widget_init_template (GTK_WIDGET (panel));

  gtk_list_box_set_header_func (GTK_LIST_BOX (panel->accounts_listbox),
                                cc_list_box_update_header_func,
                                NULL,
                                NULL);
  gtk_list_box_set_sort_func (GTK_LIST_BOX (panel->accounts_listbox),
                              sort_func,
                              panel,
                              NULL);

  gtk_list_box_set_header_func (GTK_LIST_BOX (panel->providers_listbox),
                                cc_list_box_update_header_func,
                                NULL,
                                NULL);

  monitor = g_network_monitor_get_default();

  g_object_bind_property (monitor, "network-available",
                          panel->providers_listbox, "sensitive",
                          G_BINDING_SYNC_CREATE);

  /* TODO: probably want to avoid _sync() ... */
  error = NULL;
  panel->client = goa_client_new_sync (NULL /* GCancellable */, &error);
  if (panel->client == NULL)
    {
      g_warning ("Error getting a GoaClient: %s (%s, %d)",
                 error->message, g_quark_to_string (error->domain), error->code);
      gtk_widget_set_sensitive (GTK_WIDGET (panel), FALSE);
      g_error_free (error);
      return;
    }

  g_signal_connect (panel->client,
                    "account-added",
                    G_CALLBACK (on_account_added),
                    panel);

  g_signal_connect (panel->client,
                    "account-changed",
                    G_CALLBACK (on_account_changed),
                    panel);

  g_signal_connect (panel->client,
                    "account-removed",
                    G_CALLBACK (on_account_removed),
                    panel);

  fill_accounts_listbox (panel);
  goa_provider_get_all (get_all_providers_cb, panel);

  gtk_widget_show_all (GTK_WIDGET (panel));
}

static const char *
cc_goa_panel_get_help_uri (CcPanel *panel)
{
  return "help:gnome-help/accounts";
}

static void
cc_goa_panel_constructed (GObject *object)
{
  CcGoaPanel *self = CC_GOA_PANEL (object);
  GtkWindow *parent;

  /* Setup account editor dialog */
  parent = GTK_WINDOW (cc_shell_get_toplevel (cc_panel_get_shell (CC_PANEL (self))));

  gtk_window_set_transient_for (GTK_WINDOW (self->edit_account_dialog), parent);

  G_OBJECT_CLASS (cc_goa_panel_parent_class)->constructed (object);
}

static void
cc_goa_panel_class_init (CcGoaPanelClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  CcPanelClass *panel_class = CC_PANEL_CLASS (klass);

  panel_class->get_help_uri = cc_goa_panel_get_help_uri;

  object_class->set_property = cc_goa_panel_set_property;
  object_class->finalize = cc_goa_panel_finalize;
  object_class->constructed = cc_goa_panel_constructed;

  g_object_class_override_property (object_class, PROP_PARAMETERS, "parameters");

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/online-accounts/online-accounts.ui");

  gtk_widget_class_bind_template_child (widget_class, CcGoaPanel, accounts_listbox);
  gtk_widget_class_bind_template_child (widget_class, CcGoaPanel, accounts_vbox);
  gtk_widget_class_bind_template_child (widget_class, CcGoaPanel, edit_account_dialog);
  gtk_widget_class_bind_template_child (widget_class, CcGoaPanel, edit_account_headerbar);
  gtk_widget_class_bind_template_child (widget_class, CcGoaPanel, new_account_vbox);
  gtk_widget_class_bind_template_child (widget_class, CcGoaPanel, notification_label);
  gtk_widget_class_bind_template_child (widget_class, CcGoaPanel, notification_revealer);
  gtk_widget_class_bind_template_child (widget_class, CcGoaPanel, providers_listbox);
  gtk_widget_class_bind_template_child (widget_class, CcGoaPanel, stack);

  gtk_widget_class_bind_template_callback (widget_class, on_edit_account_dialog_delete_event);
  gtk_widget_class_bind_template_callback (widget_class, on_listbox_row_activated);
  gtk_widget_class_bind_template_callback (widget_class, on_notification_closed);
  gtk_widget_class_bind_template_callback (widget_class, on_provider_row_activated);
  gtk_widget_class_bind_template_callback (widget_class, on_remove_button_clicked);
  gtk_widget_class_bind_template_callback (widget_class, on_undo_button_clicked);
}

/* ---------------------------------------------------------------------------------------------------- */

static void
show_page_nothing_selected (CcGoaPanel *panel)
{
}

static void
show_page_account (CcGoaPanel  *panel,
                   GoaObject *object)
{
  GList *children;
  GList *l;
  GoaProvider *provider;
  GoaAccount *account;
  const gchar *provider_name;
  const gchar *provider_type;
  gchar *title;

  provider = NULL;

  panel->active_object = object;
  reset_headerbar (panel);

  /* Move to the account editor page */
  gtk_stack_set_visible_child_name (GTK_STACK (panel->stack), "editor");

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

  provider_name = goa_account_get_provider_name (account);
  /* translators: This is the title of the "Show Account" dialog. The
   * %s is the name of the provider. e.g., 'Google'. */
  title = g_strdup_printf (_("%s Account"), provider_name);
  gtk_header_bar_set_title (GTK_HEADER_BAR (panel->edit_account_headerbar), title);
  g_free (title);

  gtk_widget_show_all (panel->accounts_vbox);
  gtk_widget_show (panel->edit_account_dialog);

  g_clear_object (&provider);
}

/* ---------------------------------------------------------------------------------------------------- */

static void
select_account_by_id (CcGoaPanel    *panel,
                      const gchar *account_id)
{
  GList *children, *l;

  children = gtk_container_get_children (GTK_CONTAINER (panel->accounts_listbox));

  for (l = children; l != NULL; l = l->next)
    {
      GoaAccount *account;
      GoaObject *row_object;

      row_object = g_object_get_data (l->data, "goa-object");
      account = goa_object_peek_account (row_object);

      if (g_strcmp0 (goa_account_get_id (account), account_id) == 0)
        {
          show_page_account (panel, row_object);
          break;
        }
    }

  g_list_free (children);
}

static gboolean
on_edit_account_dialog_delete_event (CcGoaPanel *self)
{
  self->active_object = NULL;
  gtk_widget_hide (self->edit_account_dialog);
  return TRUE;
}

static void
on_listbox_row_activated (CcGoaPanel    *self,
                          GtkListBoxRow *activated_row)
{
  GoaObject *object;

  object = g_object_get_data (G_OBJECT (activated_row), "goa-object");
  show_page_account (self, object);
}

static void
fill_accounts_listbox (CcGoaPanel *self)
{
  GList *accounts, *l;

  accounts = goa_client_get_accounts (self->client);

  if (accounts == NULL)
    {
      show_page_nothing_selected (self);
    }
  else
    {
      for (l = accounts; l != NULL; l = l->next)
        on_account_added (self->client, l->data, self);
    }

  g_list_free_full (accounts, g_object_unref);
}

static void
on_account_added (GoaClient *client,
                  GoaObject *object,
                  gpointer   user_data)
{
  CcGoaPanel *self = user_data;
  GtkWidget *row, *icon, *label, *box;
  GoaAccount *account;
  GError *error;
  GIcon *gicon;
  gchar* title = NULL;

  account = goa_object_peek_account (object);

  /* The main grid */
  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
  gtk_widget_show (box);

  /* The provider icon */
  icon = gtk_image_new ();

  error = NULL;
  gicon = g_icon_new_for_string (goa_account_get_provider_icon (account), &error);
  if (error != NULL)
    {
      g_warning ("Error creating GIcon for account: %s (%s, %d)",
                 error->message,
                 g_quark_to_string (error->domain),
                 error->code);

      g_clear_error (&error);
    }
  else
    {
      gtk_image_set_from_gicon (GTK_IMAGE (icon), gicon, GTK_ICON_SIZE_DIALOG);
    }

  gtk_container_add (GTK_CONTAINER (box), icon);

  /* The name of the provider */
  title = g_strdup_printf ("<b>%s</b>\n<small>%s</small>",
                           goa_account_get_provider_name (account),
                           goa_account_get_presentation_identity (account));

  label = g_object_new (GTK_TYPE_LABEL,
                        "ellipsize", PANGO_ELLIPSIZE_END,
                        "label", title,
                        "xalign", 0.0,
                        "use-markup", TRUE,
                        "hexpand", TRUE,
                        NULL);
  gtk_container_add (GTK_CONTAINER (box), label);

  /* "Needs attention" icon */
  icon = gtk_image_new_from_icon_name ("dialog-warning-symbolic", GTK_ICON_SIZE_BUTTON);
  gtk_widget_set_no_show_all (icon, TRUE);
  g_object_bind_property (goa_object_peek_account (object),
                          "attention-needed",
                          icon,
                          "visible",
                          G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE);
  gtk_container_add (GTK_CONTAINER (box), icon);

  /* The row */
  row = gtk_list_box_row_new ();
  g_object_set_data (G_OBJECT (row), "goa-object", object);
  gtk_container_add (GTK_CONTAINER (row), box);

  /* Add to the listbox */
  gtk_container_add (GTK_CONTAINER (self->accounts_listbox), row);
  gtk_widget_show_all (row);

  g_clear_pointer (&title, g_free);
  g_clear_object (&gicon);
}

static void
on_account_changed (GoaClient  *client,
                    GoaObject  *object,
                    gpointer    user_data)
{
  CcGoaPanel *panel = CC_GOA_PANEL (user_data);

  if (panel->active_object != object)
    return;

  show_page_account (panel, panel->active_object);
}

static void
on_account_removed (GoaClient *client,
                    GoaObject *object,
                    gpointer   user_data)
{
  CcGoaPanel *self = user_data;
  GList *children, *l;

  children = gtk_container_get_children (GTK_CONTAINER (self->accounts_listbox));

  for (l = children; l != NULL; l = l->next)
    {
      GoaObject *row_object;

      row_object = GOA_OBJECT (g_object_get_data (l->data, "goa-object"));

      if (row_object == object)
        {
          gtk_widget_destroy (l->data);
          break;
        }
    }

  g_list_free (children);
}

/* ---------------------------------------------------------------------------------------------------- */

static void
get_all_providers_cb (GObject      *source,
                      GAsyncResult *res,
                      gpointer      user_data)
{
  CcGoaPanel *self = user_data;
  GList *providers;
  GList *l;

  providers = NULL;
  if (!goa_provider_get_all_finish (&providers, res, NULL))
    return;

  for (l = providers; l != NULL; l = l->next)
    {
      GoaProvider *provider;
      provider = GOA_PROVIDER (l->data);

      add_provider_row (self, provider);
    }

  g_list_free_full (providers, g_object_unref);
}


/* ---------------------------------------------------------------------------------------------------- */


static GtkWidget *
get_row_for_account (CcGoaPanel *self,
                     GoaObject *object)
{
  GtkWidget *row;
  GList *children, *l;

  row = NULL;
  children = gtk_container_get_children (GTK_CONTAINER (self->accounts_listbox));

  for (l = children; l != NULL; l = l->next)
    {
      GoaObject *row_object;

      row_object = g_object_get_data (G_OBJECT (l->data), "goa-object");
      if (row_object == object)
        {
          row = l->data;
          break;
        }
    }

  g_list_free (children);
  return row;
}

static void
cancel_notification_timeout (CcGoaPanel *self)
{
  if (self->remove_account_timeout_id == 0)
    return;

  g_source_remove (self->remove_account_timeout_id);

  self->remove_account_timeout_id = 0;
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
on_notification_closed (GtkButton  *button,
                        CcGoaPanel *self)
{
  goa_account_call_remove (goa_object_peek_account (self->removed_object),
                           NULL, /* GCancellable */
                           (GAsyncReadyCallback) remove_account_cb,
                           g_object_ref (self));

  gtk_revealer_set_reveal_child (GTK_REVEALER (self->notification_revealer), FALSE);

  cancel_notification_timeout (self);
  self->removed_object = NULL;
}

static void
on_undo_button_clicked (GtkButton  *button,
                        CcGoaPanel *self)
{
  GtkWidget *row;

  /* Simply show the account row and hide the notification */
  row = get_row_for_account (self, self->removed_object);
  gtk_widget_show (row);

  gtk_revealer_set_reveal_child (GTK_REVEALER (self->notification_revealer), FALSE);

  cancel_notification_timeout (self);
  self->removed_object = NULL;
}

static gboolean
on_remove_account_timeout (gpointer user_data)
{
  on_notification_closed (NULL, user_data);
  return G_SOURCE_REMOVE;
}

static void
on_remove_button_clicked (CcGoaPanel *panel)
{
  GoaAccount *account;
  GtkWidget *row;
  gchar *label;

  if (panel->active_object == NULL)
    return;

  if (panel->removed_object != NULL)
    on_notification_closed (NULL, panel);

  panel->removed_object = panel->active_object;
  panel->active_object = NULL;

  account = goa_object_peek_account (panel->removed_object);
  /* Translators: The %s is the username (eg., debarshi.ray@gmail.com
   * or rishi).
   */
  label = g_strdup_printf (_("<b>%s</b> removed"), goa_account_get_presentation_identity (account));
  gtk_label_set_markup (GTK_LABEL (panel->notification_label), label);
  gtk_revealer_set_reveal_child (GTK_REVEALER (panel->notification_revealer), TRUE);

  row = get_row_for_account (panel, panel->removed_object);

  gtk_widget_hide (panel->edit_account_dialog);
  gtk_widget_hide (row);

  panel->remove_account_timeout_id = g_timeout_add_seconds (10, on_remove_account_timeout, panel);

  g_free (label);
}
