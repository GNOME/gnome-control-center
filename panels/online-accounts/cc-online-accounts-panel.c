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

#ifdef GDK_WINDOWING_X11
#include <gdk/x11/gdkx.h>
#endif
#ifdef GDK_WINDOWING_WAYLAND
#include <gdk/wayland/gdkwayland.h>
#endif

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
  GtkLabel      *offline_label;
  GtkListBox    *providers_listbox;
  GtkButton     *remove_account_button;
  GtkBox        *accounts_vbox;

  GoaClient *client;
  GoaObject *active_object;
  GoaObject *removed_object;

  guint      remove_account_timeout_id;
  gchar     *window_export_handle;
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
  gtk_widget_hide (row);
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
  gtk_widget_show (row);
  gtk_widget_show (GTK_WIDGET (self->accounts_frame));
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

/* Auxiliary methods */

G_GNUC_NULL_TERMINATED
static char *
run_goa_helper_sync (const char *command,
                     ...)
{
  g_autoptr(GPtrArray) argv = NULL;
  g_autofree char *output = NULL;
  g_autoptr(GError) error = NULL;
  const char *param;
  va_list args;
  int status;

  argv = g_ptr_array_new_with_free_func (g_free);
  g_ptr_array_add (argv, g_strdup (LIBEXECDIR "/gnome-control-center-goa-helper"));
  g_ptr_array_add (argv, g_strdup (command));

  va_start (args, command);
  while ((param = va_arg (args, const char*)) != NULL)
    g_ptr_array_add (argv, g_strdup (param));
  va_end (args);

  g_ptr_array_add (argv, NULL);

  if (!g_spawn_sync (NULL,
                     (char **) argv->pdata,
                     NULL,
                     0,
                     NULL,
                     NULL,
                     &output,
                     NULL,
                     &status,
                     &error))
    {
      g_warning ("Failed to run online accounts helper: %s", error->message);
      return NULL;
    }

  if (!g_spawn_check_wait_status (status, NULL))
    return NULL;

  if (output == NULL || *output == '\0')
    return NULL;

  return g_steal_pointer (&output);
}

static void
run_goa_helper_in_thread_func (GTask        *task,
                               gpointer      source_object,
                               gpointer      task_data,
                               GCancellable *cancellable)
{
  g_autofree char *output = NULL;
  g_autoptr(GError) error = NULL;
  GPtrArray *argv = task_data;
  int status;

  g_spawn_sync (NULL,
                (char **) argv->pdata,
                NULL, 0, NULL, NULL,
                &output,
                NULL,
                &status,
                &error);

  if (error)
    {
      g_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  if (!g_spawn_check_wait_status (status, &error))
    {
      g_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  g_task_return_pointer (task, g_steal_pointer (&output), g_free);
}

static void
run_goa_helper_async (const gchar         *command,
                      const gchar         *param,
                      const gchar         *window_handle,
                      GCancellable        *cancellable,
                      GAsyncReadyCallback  callback,
                      gpointer             user_data)
{
  g_autoptr(GPtrArray) argv = NULL;
  g_autoptr(GTask) task = NULL;

  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  argv = g_ptr_array_new_with_free_func (g_free);
  g_ptr_array_add (argv, g_strdup (LIBEXECDIR "/gnome-control-center-goa-helper"));
  g_ptr_array_add (argv, g_strdup (command));
  g_ptr_array_add (argv, g_strdup (param));
  g_ptr_array_add (argv, g_strdup (window_handle));
  g_ptr_array_add (argv, NULL);

  task = g_task_new (NULL, cancellable, callback, user_data);
  g_task_set_source_tag (task, run_goa_helper_async);
  g_task_set_task_data (task, g_steal_pointer (&argv), (GDestroyNotify) g_ptr_array_unref);
  g_task_run_in_thread (task, run_goa_helper_in_thread_func);
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
on_show_account_finish_cb (GObject      *source_object,
                           GAsyncResult *result,
                           gpointer      user_data)
{
  CcOnlineAccountsPanel *self = CC_ONLINE_ACCOUNTS_PANEL (user_data);
  g_autofree char *output = NULL;
  g_autoptr(GError) error = NULL;

  output = g_task_propagate_pointer (G_TASK (result), &error);

  if (error)
    {
      g_warning ("Error showing account: %s", error->message);
      return;
    }

  if (g_strcmp0 (output, "remove") == 0)
    start_remove_account_timeout (self);

  self->active_object = NULL;
}

static void
show_account (CcOnlineAccountsPanel *self,
              GoaObject             *object)
{
  GoaAccount *account;

  if (!self->window_export_handle)
    return;

  self->active_object = g_object_ref (object);

  account = goa_object_peek_account (object);
  run_goa_helper_async ("show-account",
                        goa_account_get_id (account),
                        self->window_export_handle,
                        cc_panel_get_cancellable (CC_PANEL (self)),
                        on_show_account_finish_cb,
                        self);
}

static void
on_create_account_finish_cb (GObject      *source_object,
                             GAsyncResult *result,
                             gpointer      user_data)
{
  CcOnlineAccountsPanel *self = CC_ONLINE_ACCOUNTS_PANEL (user_data);
  g_autofree char *new_account_id = NULL;
  g_autoptr(GoaObject) object = NULL;
  g_autoptr(GError) error = NULL;

  new_account_id = g_task_propagate_pointer (G_TASK (result), &error);

  if (error)
    {
      g_warning ("Error showing account: %s", error->message);
      return;
    }

  if (new_account_id)
    object = goa_client_lookup_by_id (self->client, new_account_id);

  if (object)
    show_account (self, object);
}

static void
create_account (CcOnlineAccountsPanel *self,
                GVariant              *provider)
{
  g_autofree char *provider_type = NULL;

  if (!self->window_export_handle)
    return;

  g_variant_get (provider, "(ssviu)", &provider_type, NULL, NULL, NULL, NULL);

  run_goa_helper_async ("create-account",
                        provider_type,
                        self->window_export_handle,
                        cc_panel_get_cancellable (CC_PANEL (self)),
                        on_create_account_finish_cb,
                        self);
}

static void
add_provider_row (CcOnlineAccountsPanel *self,
                  GVariant              *provider)
{
  CcOnlineAccountProviderRow *row;

  row = cc_online_account_provider_row_new (provider);

  gtk_widget_show (GTK_WIDGET (row));
  gtk_list_box_append (self->providers_listbox, GTK_WIDGET (row));
}

static void
list_providers (CcOnlineAccountsPanel *self)
{
  g_autoptr(GVariant) providers_variant = NULL;
  g_autoptr(GError) error = NULL;
  g_autofree char *providers = NULL;
  GVariantIter iter;
  GVariant *provider;

  providers = run_goa_helper_sync ("list-providers", NULL);

  if (!providers)
    return;

  providers_variant = g_variant_parse (G_VARIANT_TYPE ("a(ssviu)"),
                                       providers,
                                       NULL,
                                       NULL,
                                       &error);

  if (error)
    {
      g_warning ("Error listing providers: %s", error->message);
      return;
    }

  g_variant_iter_init (&iter, providers_variant);

  while ((provider = g_variant_iter_next_value (&iter)))
    add_provider_row (self, provider);
}

static void
add_account (CcOnlineAccountsPanel *self,
             GoaObject             *object)
{
  CcOnlineAccountRow *row;

  row = cc_online_account_row_new (object);

  /* Add to the listbox */
  gtk_list_box_append (self->accounts_listbox, GTK_WIDGET (row));
  gtk_widget_show (GTK_WIDGET (self->accounts_frame));
}

static void
fill_accounts_listbox (CcOnlineAccountsPanel *self)
{
  g_autolist(GoaAccount) accounts = NULL;
  GList *l;

  accounts = goa_client_get_accounts (self->client);

  for (l = accounts; l != NULL; l = l->next)
    add_account (self, l->data);
}

#ifdef GDK_WINDOWING_WAYLAND
static void
wayland_window_exported_cb (GdkToplevel *toplevel,
                            const char  *handle,
                            gpointer     data)

{
  CcOnlineAccountsPanel *self = data;

  self->window_export_handle = g_strdup_printf ("wayland:%s", handle);
}
#endif

static void
export_window_handle (CcOnlineAccountsPanel *self)
{
  GtkNative *native = gtk_widget_get_native (GTK_WIDGET (self));

#ifdef GDK_WINDOWING_X11
  if (GDK_IS_X11_DISPLAY (gtk_widget_get_display (GTK_WIDGET (native))))
    {
      GdkSurface *surface = gtk_native_get_surface (native);
      guint32 xid = (guint32) gdk_x11_surface_get_xid (surface);

      self->window_export_handle = g_strdup_printf ("x11:%x", xid);
    }
#endif
#ifdef GDK_WINDOWING_WAYLAND
  if (GDK_IS_WAYLAND_DISPLAY (gtk_widget_get_display (GTK_WIDGET (native))))
    {
      GdkSurface *surface = gtk_native_get_surface (native);

      gdk_wayland_toplevel_export_handle (GDK_TOPLEVEL (surface),
                                          wayland_window_exported_cb,
                                          self,
                                          NULL);
    }
#endif
}

static void
unexport_window_handle (CcOnlineAccountsPanel *self)
{
  if (!self->window_export_handle)
    return;

#ifdef GDK_WINDOWING_WAYLAND
  GtkNative *native = gtk_widget_get_native (GTK_WIDGET (self));

  if (GDK_IS_WAYLAND_DISPLAY (gtk_widget_get_display (GTK_WIDGET (native))))
    {
      GdkSurface *surface = gtk_native_get_surface (native);
      gdk_wayland_toplevel_unexport_handle (GDK_TOPLEVEL (surface));
    }
#endif
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
      GVariant *provider;

      for (child = gtk_widget_get_first_child (GTK_WIDGET (self->providers_listbox));
           child;
           child = gtk_widget_get_next_sibling (child))
        {
          g_autofree gchar *provider_type = NULL;

          provider = cc_online_account_provider_row_get_provider (CC_ONLINE_ACCOUNT_PROVIDER_ROW (child));
          g_variant_get (provider, "(ssviu)", &provider_type, NULL, NULL, NULL, NULL);

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
  GVariant *a_provider, *b_provider;
  gboolean a_branded, b_branded;
  gint a_features, b_features;

  a_provider = cc_online_account_provider_row_get_provider (CC_ONLINE_ACCOUNT_PROVIDER_ROW (a));
  b_provider = cc_online_account_provider_row_get_provider (CC_ONLINE_ACCOUNT_PROVIDER_ROW (b));

  g_variant_get (a_provider, "(ssviu)", NULL, NULL, NULL, &a_features, NULL);
  g_variant_get (b_provider, "(ssviu)", NULL, NULL, NULL, &b_features, NULL);

  /* FIXME: this needs to go away once libgoa-backend is ported to GTK4 */
#define FEATURE_BRANDED (1 << 1)

  a_branded = (a_features & FEATURE_BRANDED) != 0;
  b_branded = (a_features & FEATURE_BRANDED) != 0;

#undef FEATURE_BRANDED

  if (a_branded != b_branded)
    {
      if (a_branded)
        return -1;
      else
        return 1;
    }

  return gtk_list_box_row_get_index (b) - gtk_list_box_row_get_index (a);
}

static void
on_account_added_cb (GoaClient             *client,
                     GoaObject             *object,
                     CcOnlineAccountsPanel *self)
{
  add_account (self, object);
}

static void
on_account_changed_cb (GoaClient             *client,
                       GoaObject             *object,
                       CcOnlineAccountsPanel *self)
{
  if (self->active_object == object)
    show_account (self, self->active_object);
}

static void
on_account_removed_cb (GoaClient             *client,
                       GoaObject             *object,
                       CcOnlineAccountsPanel *self)
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
on_notification_closed_cb (GtkButton             *button,
                           CcOnlineAccountsPanel *self)
{
  goa_account_call_remove (goa_object_peek_account (self->removed_object),
                           cc_panel_get_cancellable (CC_PANEL (self)),
                           (GAsyncReadyCallback) on_client_remove_account_finish_cb,
                           g_object_ref (self));

  gtk_revealer_set_reveal_child (self->notification_revealer, FALSE);

  cancel_notification_timeout (self);
  self->removed_object = NULL;
}

static void
on_undo_button_clicked_cb (GtkButton             *button,
                           CcOnlineAccountsPanel *self)
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
  GVariant *provider = cc_online_account_provider_row_get_provider (CC_ONLINE_ACCOUNT_PROVIDER_ROW (activated_row));

  create_account (self, provider);
}

static gboolean
remove_account_timeout_cb (gpointer user_data)
{
  CcOnlineAccountsPanel *self = CC_ONLINE_ACCOUNTS_PANEL (user_data);

  gtk_widget_activate (self->close_notification_button);

  return G_SOURCE_REMOVE;
}

/* CcPanel overrides */

static const char *
cc_online_accounts_panel_get_help_uri (CcPanel *panel)
{
  return "help:gnome-help/accounts";
}

/* GtkWidget overrides */

static void
cc_online_accounts_panel_realize (GtkWidget *widget)
{
  GTK_WIDGET_CLASS (cc_online_accounts_panel_parent_class)->realize (widget);

  export_window_handle (CC_ONLINE_ACCOUNTS_PANEL (widget));
}

static void
cc_online_accounts_panel_unrealize (GtkWidget *widget)
{
  unexport_window_handle (CC_ONLINE_ACCOUNTS_PANEL (widget));

  GTK_WIDGET_CLASS (cc_online_accounts_panel_parent_class)->unrealize (widget);
}

/* GObject overrides */

static void
cc_online_accounts_panel_set_property (GObject      *object,
                                       guint         property_id,
                                       const GValue *value,
                                       GParamSpec   *pspec)
{
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

          if (g_strcmp0 (first_arg, "add") == 0)
            command_add (CC_ONLINE_ACCOUNTS_PANEL (object), parameters);
          else if (first_arg != NULL)
            select_account_by_id (CC_ONLINE_ACCOUNTS_PANEL (object), first_arg);

          return;
        }
    }

  G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
cc_online_accounts_panel_constructed (GObject *object)
{
  CcOnlineAccountsPanel *self = CC_ONLINE_ACCOUNTS_PANEL (object);

  G_OBJECT_CLASS (cc_online_accounts_panel_parent_class)->constructed (object);

  list_providers (self);
}

static void
cc_online_accounts_panel_finalize (GObject *object)
{
  CcOnlineAccountsPanel *panel = CC_ONLINE_ACCOUNTS_PANEL (object);

  if (panel->removed_object != NULL)
    {
      g_autoptr(GError) error = NULL;
      goa_account_call_remove_sync (goa_object_peek_account (panel->removed_object),
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

  g_clear_object (&panel->client);

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
  object_class->constructed = cc_online_accounts_panel_constructed;

  widget_class->realize = cc_online_accounts_panel_realize;
  widget_class->unrealize = cc_online_accounts_panel_unrealize;

  g_object_class_override_property (object_class, PROP_PARAMETERS, "parameters");

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/online-accounts/cc-online-accounts-panel.ui");

  gtk_widget_class_bind_template_child (widget_class, CcOnlineAccountsPanel, accounts_frame);
  gtk_widget_class_bind_template_child (widget_class, CcOnlineAccountsPanel, accounts_listbox);
  gtk_widget_class_bind_template_child (widget_class, CcOnlineAccountsPanel, close_notification_button);
  gtk_widget_class_bind_template_child (widget_class, CcOnlineAccountsPanel, notification_label);
  gtk_widget_class_bind_template_child (widget_class, CcOnlineAccountsPanel, notification_revealer);
  gtk_widget_class_bind_template_child (widget_class, CcOnlineAccountsPanel, offline_label);
  gtk_widget_class_bind_template_child (widget_class, CcOnlineAccountsPanel, providers_listbox);

  gtk_widget_class_bind_template_callback (widget_class, on_accounts_listbox_row_activated);
  gtk_widget_class_bind_template_callback (widget_class, on_notification_closed_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_provider_row_activated_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_undo_button_clicked_cb);
}

static void
cc_online_accounts_panel_init (CcOnlineAccountsPanel *self)
{
  g_autoptr(GError) error = NULL;
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
                          self->offline_label,
                          "visible",
                          G_BINDING_SYNC_CREATE | G_BINDING_INVERT_BOOLEAN);

  g_object_bind_property (monitor,
                          "network-available",
                          self->providers_listbox,
                          "sensitive",
                          G_BINDING_SYNC_CREATE);

  /* TODO: probably want to avoid _sync() ... */
  self->client = goa_client_new_sync (cc_panel_get_cancellable (CC_PANEL (self)), &error);
  if (self->client == NULL)
    {
      g_warning ("Error getting a GoaClient: %s (%s, %d)",
                 error->message, g_quark_to_string (error->domain), error->code);
      gtk_widget_set_sensitive (GTK_WIDGET (self), FALSE);
      return;
    }

  g_signal_connect (self->client,
                    "account-added",
                    G_CALLBACK (on_account_added_cb),
                    self);

  g_signal_connect (self->client,
                    "account-changed",
                    G_CALLBACK (on_account_changed_cb),
                    self);

  g_signal_connect (self->client,
                    "account-removed",
                    G_CALLBACK (on_account_removed_cb),
                    self);

  fill_accounts_listbox (self);
  load_custom_css ();
}
