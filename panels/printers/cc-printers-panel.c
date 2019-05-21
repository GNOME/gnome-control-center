/*
 * Copyright (C) 2010 Red Hat, Inc
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
 */

#include <config.h>

#include "shell/cc-object-storage.h"

#include "cc-printers-panel.h"
#include "cc-printers-resources.h"
#include "pp-printer.h"

#include <string.h>
#include <glib/gi18n-lib.h>
#include <glib/gstdio.h>
#include <polkit/polkit.h>
#include <gdesktop-enums.h>

#include <cups/cups.h>
#include <cups/ppd.h>

#include <math.h>

#include "pp-new-printer-dialog.h"
#include "pp-ppd-selection-dialog.h"
#include "pp-utils.h"
#include "pp-cups.h"
#include "pp-printer-entry.h"
#include "pp-job.h"

#include "cc-util.h"

#define RENEW_INTERVAL        500
#define SUBSCRIPTION_DURATION 600

#define CUPS_DBUS_NAME      "org.cups.cupsd.Notifier"
#define CUPS_DBUS_PATH      "/org/cups/cupsd/Notifier"
#define CUPS_DBUS_INTERFACE "org.cups.cupsd.Notifier"

#define CUPS_STATUS_CHECK_INTERVAL 5

#if (CUPS_VERSION_MAJOR > 1) || (CUPS_VERSION_MINOR > 5)
#define HAVE_CUPS_1_6 1
#endif

#ifndef HAVE_CUPS_1_6
#define ippGetState(ipp) ipp->state
#define ippGetStatusCode(ipp) ipp->request.status.status_code
#define ippGetString(attr, element, language) attr->values[element].string.text
#endif

struct _CcPrintersPanel
{
  CcPanel parent_instance;

  GtkBuilder *builder;

  cups_dest_t *dests;
  int num_dests;

  GPermission *permission;
  gboolean is_authorized;

  GSettings *lockdown_settings;

  PpNewPrinterDialog   *pp_new_printer_dialog;
  PpPPDSelectionDialog *pp_ppd_selection_dialog;

  GDBusProxy      *cups_proxy;
  GDBusConnection *cups_bus_connection;
  gint             subscription_id;
  guint            subscription_renewal_id;
  guint            cups_status_check_id;
  guint            dbus_subscription_id;
  guint            remove_printer_timeout_id;

  GtkWidget    *headerbar_buttons;
  GtkRevealer  *notification;
  PPDList      *all_ppds_list;
  GCancellable *get_all_ppds_cancellable;
  GCancellable *subscription_renew_cancellable;
  GCancellable *actualize_printers_list_cancellable;
  GCancellable *cups_status_check_cancellable;

  gchar    *new_printer_name;
  gchar    *new_printer_location;
  gchar    *new_printer_make_and_model;
  gboolean  new_printer_on_network;

  gchar    *renamed_printer_name;
  gchar    *old_printer_name;
  gchar    *deleted_printer_name;

  GHashTable *printer_entries;
  gboolean    entries_filled;
  GVariant   *action;

  GtkSizeGroup *size_group;

  gpointer dummy;
};

CC_PANEL_REGISTER (CcPrintersPanel, cc_printers_panel)

#define PAGE_LOCK "_lock"
#define PAGE_ADDPRINTER "_addprinter"

typedef struct
{
  gchar        *printer_name;
  GCancellable *cancellable;
} SetPPDItem;

enum {
  PROP_0,
  PROP_PARAMETERS
};

static void actualize_printers_list (CcPrintersPanel *self);
static void update_sensitivity (gpointer user_data);
static void detach_from_cups_notifier (gpointer data);
static void free_dests (CcPrintersPanel *self);

static void
execute_action (CcPrintersPanel *self,
                GVariant        *action)
{
  PpPrinterEntry         *printer_entry;
  const gchar            *action_name;
  const gchar            *printer_name;
  GVariant               *variant;
  GVariant               *action_variant;
  gint                    count;

  count = g_variant_n_children (action);
  if (count == 2)
    {
      g_variant_get_child (action, 0, "v", &action_variant);
      action_name = g_variant_get_string (action_variant, NULL);

      /* authenticate-jobs printer-name */
      if (g_strcmp0 (action_name, "authenticate-jobs") == 0)
        {
          g_variant_get_child (action, 1, "v", &variant);
          printer_name = g_variant_get_string (variant, NULL);

          printer_entry = PP_PRINTER_ENTRY (g_hash_table_lookup (self->printer_entries, printer_name));
          if (printer_entry != NULL)
            pp_printer_entry_authenticate_jobs (printer_entry);
          else
            g_warning ("Could not find printer \"%s\"!", printer_name);

          g_variant_unref (variant);
        }
      /* show-jobs printer-name */
      else if (g_strcmp0 (action_name, "show-jobs") == 0)
        {
          g_variant_get_child (action, 1, "v", &variant);
          printer_name = g_variant_get_string (variant, NULL);

          printer_entry = PP_PRINTER_ENTRY (g_hash_table_lookup (self->printer_entries, printer_name));
          if (printer_entry != NULL)
            pp_printer_entry_show_jobs_dialog (printer_entry);
          else
            g_warning ("Could not find printer \"%s\"!", printer_name);

          g_variant_unref (variant);
        }

      g_variant_unref (action_variant);
    }
}

static void
cc_printers_panel_get_property (GObject    *object,
                               guint       property_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  switch (property_id)
    {
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
cc_printers_panel_set_property (GObject      *object,
                                guint         property_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  CcPrintersPanel        *self = CC_PRINTERS_PANEL (object);
  GVariant               *parameters;

  switch (property_id)
    {
      case PROP_PARAMETERS:
        parameters = g_value_get_variant (value);
        if (parameters != NULL && g_variant_n_children (parameters) > 0)
          {
            if (self->entries_filled)
              {
                execute_action (CC_PRINTERS_PANEL (object), parameters);
              }
            else
              {
                if (self->action != NULL)
                  g_variant_unref (self->action);
                self->action = g_variant_ref (parameters);
              }
          }
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
cc_printers_panel_constructed (GObject *object)
{
  CcPrintersPanel *self = CC_PRINTERS_PANEL (object);
  GtkWidget *widget;
  CcShell *shell;

  G_OBJECT_CLASS (cc_printers_panel_parent_class)->constructed (object);

  shell = cc_panel_get_shell (CC_PANEL (self));
  cc_shell_embed_widget_in_header (shell, self->headerbar_buttons, GTK_POS_RIGHT);

  widget = (GtkWidget*)
    gtk_builder_get_object (self->builder, "lock-button");
  gtk_lock_button_set_permission (GTK_LOCK_BUTTON (widget), self->permission);

  widget = (GtkWidget*)
    gtk_builder_get_object (self->builder, "search-button");
  cc_shell_embed_widget_in_header (shell, widget, GTK_POS_RIGHT);

  widget = (GtkWidget*)
    gtk_builder_get_object (self->builder, "search-bar");
  g_signal_connect_object (shell,
                           "key-press-event",
                           G_CALLBACK (gtk_search_bar_handle_event),
                           widget,
                           G_CONNECT_SWAPPED);
}

static void
printer_removed_cb (GObject      *source_object,
                    GAsyncResult *result,
                    gpointer      user_data)
{
  g_autoptr(GError) error = NULL;

  pp_printer_delete_finish (PP_PRINTER (source_object), result, &error);
  g_object_unref (source_object);

  if (error != NULL)
    g_warning ("Printer could not be deleted: %s", error->message);
}

static void
cc_printers_panel_dispose (GObject *object)
{
  CcPrintersPanel *self = CC_PRINTERS_PANEL (object);

  g_cancellable_cancel (self->subscription_renew_cancellable);
  g_cancellable_cancel (self->actualize_printers_list_cancellable);
  g_cancellable_cancel (self->cups_status_check_cancellable);
  g_cancellable_cancel (self->get_all_ppds_cancellable);

  detach_from_cups_notifier (CC_PRINTERS_PANEL (object));

  if (self->deleted_printer_name != NULL)
    {
      PpPrinter *printer = pp_printer_new (self->deleted_printer_name);
      pp_printer_delete_async (printer,
                               NULL,
                               printer_removed_cb,
                               NULL);
    }

  g_clear_object (&self->pp_new_printer_dialog);
  g_clear_pointer (&self->new_printer_name, g_free);
  g_clear_pointer (&self->new_printer_location, g_free);
  g_clear_pointer (&self->new_printer_make_and_model, g_free);
  g_clear_pointer (&self->renamed_printer_name, g_free);
  g_clear_pointer (&self->old_printer_name, g_free);
  g_clear_object (&self->builder);
  g_clear_object (&self->lockdown_settings);
  g_clear_object (&self->permission);
  g_clear_object (&self->subscription_renew_cancellable);
  g_clear_object (&self->actualize_printers_list_cancellable);
  g_clear_object (&self->cups_status_check_cancellable);
  g_clear_handle_id (&self->cups_status_check_id, g_source_remove);
  g_clear_handle_id (&self->remove_printer_timeout_id, g_source_remove);
  g_clear_object (&self->get_all_ppds_cancellable);
  g_clear_pointer (&self->deleted_printer_name, g_free);
  g_clear_pointer (&self->action, g_variant_unref);
  g_clear_pointer (&self->printer_entries, g_hash_table_destroy);
  g_clear_pointer (&self->all_ppds_list, ppd_list_free);
  free_dests (self);

  G_OBJECT_CLASS (cc_printers_panel_parent_class)->dispose (object);
}

static void
cc_printers_panel_finalize (GObject *object)
{
  G_OBJECT_CLASS (cc_printers_panel_parent_class)->finalize (object);
}

static const char *
cc_printers_panel_get_help_uri (CcPanel *panel)
{
  return "help:gnome-help/printing";
}

static void
cc_printers_panel_class_init (CcPrintersPanelClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  CcPanelClass *panel_class = CC_PANEL_CLASS (klass);

  object_class->get_property = cc_printers_panel_get_property;
  object_class->set_property = cc_printers_panel_set_property;
  object_class->constructed = cc_printers_panel_constructed;
  object_class->dispose = cc_printers_panel_dispose;
  object_class->finalize = cc_printers_panel_finalize;

  panel_class->get_help_uri = cc_printers_panel_get_help_uri;

  g_object_class_override_property (object_class, PROP_PARAMETERS, "parameters");
}

static void
on_get_job_attributes_cb (GObject      *source_object,
                          GAsyncResult *res,
                          gpointer      user_data)
{
  CcPrintersPanel        *self = (CcPrintersPanel*) user_data;
  const gchar            *job_originating_user_name;
  const gchar            *job_printer_uri;
  GVariant               *attributes;
  GVariant               *username;
  GVariant               *printer_uri;
  g_autoptr(GError)       error = NULL;

  attributes = pp_job_get_attributes_finish (PP_JOB (source_object), res, &error);
  g_object_unref (source_object);

  if (attributes != NULL)
    {
      if ((username = g_variant_lookup_value (attributes, "job-originating-user-name", G_VARIANT_TYPE ("as"))) != NULL)
        {
          if ((printer_uri = g_variant_lookup_value (attributes, "job-printer-uri", G_VARIANT_TYPE ("as"))) != NULL)
            {
              job_originating_user_name = g_variant_get_string (g_variant_get_child_value (username, 0), NULL);
              job_printer_uri = g_variant_get_string (g_variant_get_child_value (printer_uri, 0), NULL);

              if (job_originating_user_name != NULL && job_printer_uri != NULL &&
                  g_strcmp0 (job_originating_user_name, cupsUser ()) == 0 &&
                  g_strrstr (job_printer_uri, "/") != 0 &&
                  self->dests != NULL)
                {
                  PpPrinterEntry *printer_entry;
                  gchar *printer_name;

                  printer_name = g_strrstr (job_printer_uri, "/") + 1;
                  printer_entry = PP_PRINTER_ENTRY (g_hash_table_lookup (self->printer_entries, printer_name));

                  pp_printer_entry_update_jobs_count (printer_entry);
                }

              g_variant_unref (printer_uri);
            }

          g_variant_unref (username);
        }

      g_variant_unref (attributes);
    }
}

static void
on_cups_notification (GDBusConnection *connection,
                      const char      *sender_name,
                      const char      *object_path,
                      const char      *interface_name,
                      const char      *signal_name,
                      GVariant        *parameters,
                      gpointer         user_data)
{
  CcPrintersPanel        *self = (CcPrintersPanel*) user_data;
  gboolean                printer_is_accepting_jobs;
  gchar                  *printer_name = NULL;
  gchar                  *text = NULL;
  gchar                  *printer_uri = NULL;
  gchar                  *printer_state_reasons = NULL;
  PpJob                  *job;
  gchar                  *job_state_reasons = NULL;
  gchar                  *job_name = NULL;
  guint                   job_id;
  gint                    printer_state;
  gint                    job_state;
  gint                    job_impressions_completed;
  static gchar *requested_attrs[] = {
    "job-printer-uri",
    "job-originating-user-name",
    NULL };

  if (g_strcmp0 (signal_name, "PrinterAdded") != 0 &&
      g_strcmp0 (signal_name, "PrinterDeleted") != 0 &&
      g_strcmp0 (signal_name, "PrinterStateChanged") != 0 &&
      g_strcmp0 (signal_name, "PrinterStopped") != 0 &&
      g_strcmp0 (signal_name, "JobCreated") != 0 &&
      g_strcmp0 (signal_name, "JobCompleted") != 0)
    return;

  if (g_variant_n_children (parameters) == 1)
    g_variant_get (parameters, "(&s)", &text);
 else if (g_variant_n_children (parameters) == 6)
    {
      g_variant_get (parameters, "(&s&s&su&sb)",
                     &text,
                     &printer_uri,
                     &printer_name,
                     &printer_state,
                     &printer_state_reasons,
                     &printer_is_accepting_jobs);
    }
  else if (g_variant_n_children (parameters) == 11)
    {
      g_variant_get (parameters, "(&s&s&su&sbuu&s&su)",
                     &text,
                     &printer_uri,
                     &printer_name,
                     &printer_state,
                     &printer_state_reasons,
                     &printer_is_accepting_jobs,
                     &job_id,
                     &job_state,
                     &job_state_reasons,
                     &job_name,
                     &job_impressions_completed);
    }

  if (g_strcmp0 (signal_name, "PrinterAdded") == 0 ||
      g_strcmp0 (signal_name, "PrinterDeleted") == 0 ||
      g_strcmp0 (signal_name, "PrinterStateChanged") == 0 ||
      g_strcmp0 (signal_name, "PrinterStopped") == 0)
    actualize_printers_list (self);
  else if (g_strcmp0 (signal_name, "JobCreated") == 0 ||
           g_strcmp0 (signal_name, "JobCompleted") == 0)
    {
      job = g_object_new (PP_TYPE_JOB, "id", job_id, NULL);
      pp_job_get_attributes_async (job,
                                   requested_attrs,
                                   NULL,
                                   on_get_job_attributes_cb,
                                   self);
    }
}

static gchar *subscription_events[] = {
  "printer-added",
  "printer-deleted",
  "printer-stopped",
  "printer-state-changed",
  "job-created",
  "job-completed",
  NULL};

static void
renew_subscription_cb (GObject      *source_object,
           GAsyncResult *result,
           gpointer      user_data)
{
  CcPrintersPanel        *self = (CcPrintersPanel*) user_data;
  PpCups                 *cups = PP_CUPS (source_object);
  gint                    subscription_id;

  subscription_id = pp_cups_renew_subscription_finish (cups, result);
  g_object_unref (source_object);

  if (subscription_id > 0)
      self->subscription_id = subscription_id;
}

static gboolean
renew_subscription (gpointer data)
{
  CcPrintersPanel        *self = (CcPrintersPanel*) data;
  PpCups                 *cups;

  cups = pp_cups_new ();
  pp_cups_renew_subscription_async (cups,
                                    self->subscription_id,
                                    subscription_events,
                                    SUBSCRIPTION_DURATION,
                                    self->subscription_renew_cancellable,
                                    renew_subscription_cb,
                                    data);

  return G_SOURCE_CONTINUE;
}

static void
attach_to_cups_notifier_cb (GObject      *source_object,
                            GAsyncResult *result,
                            gpointer      user_data)
{
  CcPrintersPanel        *self = (CcPrintersPanel*) user_data;
  PpCups                 *cups = PP_CUPS (source_object);
  g_autoptr(GError)       error = NULL;
  gint                    subscription_id;

  subscription_id = pp_cups_renew_subscription_finish (cups, result);
  g_object_unref (source_object);

  if (subscription_id > 0)
    {
      self->subscription_id = subscription_id;

      self->subscription_renewal_id =
        g_timeout_add_seconds (RENEW_INTERVAL, renew_subscription, self);

      self->cups_proxy = cc_object_storage_create_dbus_proxy_sync (G_BUS_TYPE_SYSTEM,
                                                                   G_DBUS_PROXY_FLAGS_NONE,
                                                                   CUPS_DBUS_NAME,
                                                                   CUPS_DBUS_PATH,
                                                                   CUPS_DBUS_INTERFACE,
                                                                   NULL,
                                                                   &error);

      if (!self->cups_proxy)
        {
          g_warning ("%s", error->message);
          return;
        }

      self->cups_bus_connection = g_dbus_proxy_get_connection (self->cups_proxy);

      self->dbus_subscription_id =
        g_dbus_connection_signal_subscribe (self->cups_bus_connection,
                                            NULL,
                                            CUPS_DBUS_INTERFACE,
                                            NULL,
                                            CUPS_DBUS_PATH,
                                            NULL,
                                            0,
                                            on_cups_notification,
                                            self,
                                            NULL);
    }
}

static void
attach_to_cups_notifier (gpointer data)
{
  CcPrintersPanel        *self = (CcPrintersPanel*) data;
  PpCups                 *cups;

  cups = pp_cups_new ();
  pp_cups_renew_subscription_async (cups,
                                    self->subscription_id,
                                    subscription_events,
                                    SUBSCRIPTION_DURATION,
                                    self->subscription_renew_cancellable,
                                    attach_to_cups_notifier_cb,
                                    data);
}

static void
subscription_cancel_cb (GObject      *source_object,
                        GAsyncResult *result,
                        gpointer      user_data)
{
  PpCups *cups = PP_CUPS (source_object);

  pp_cups_cancel_subscription_finish (cups, result);
  g_object_unref (source_object);
}

static void
detach_from_cups_notifier (gpointer data)
{
  CcPrintersPanel        *self = (CcPrintersPanel*) data;
  PpCups                 *cups;

  if (self->dbus_subscription_id != 0) {
    g_dbus_connection_signal_unsubscribe (self->cups_bus_connection,
                                          self->dbus_subscription_id);
    self->dbus_subscription_id = 0;
  }

  cups = pp_cups_new ();
  pp_cups_cancel_subscription_async (cups,
                                     self->subscription_id,
                                     subscription_cancel_cb,
                                     NULL);

  self->subscription_id = 0;

  if (self->subscription_renewal_id != 0) {
    g_source_remove (self->subscription_renewal_id);
    self->subscription_renewal_id = 0;
  }

  if (self->cups_proxy != NULL) {
    g_object_unref (self->cups_proxy);
    self->cups_proxy = NULL;
  }
}

static void
free_dests (CcPrintersPanel *self)
{
  if (self->num_dests > 0)
    {
      cupsFreeDests (self->num_dests, self->dests);
    }
  self->dests = NULL;
  self->num_dests = 0;
}

static void
cancel_notification_timeout (CcPrintersPanel *self)
{
  if (self->remove_printer_timeout_id == 0)
    return;

  g_source_remove (self->remove_printer_timeout_id);

  self->remove_printer_timeout_id = 0;
}

static void
on_printer_deletion_undone (GtkButton *button,
                            gpointer   user_data)
{
  CcPrintersPanel        *self = (CcPrintersPanel*) user_data;

  gtk_revealer_set_reveal_child (self->notification, FALSE);

  g_clear_pointer (&self->deleted_printer_name, g_free);
  actualize_printers_list (self);

  cancel_notification_timeout (self);
}

static void
on_notification_dismissed (GtkButton *button,
                           gpointer   user_data)
{
  CcPrintersPanel        *self = (CcPrintersPanel*) user_data;

  if (self->deleted_printer_name != NULL)
    {
      PpPrinter *printer;

      printer = pp_printer_new (self->deleted_printer_name);
      pp_printer_delete_async (printer,
                               NULL,
                               printer_removed_cb,
                               NULL);

      g_clear_pointer (&self->deleted_printer_name, g_free);
    }

  gtk_revealer_set_reveal_child (self->notification, FALSE);
}

static gboolean
on_remove_printer_timeout (gpointer user_data)
{
  on_notification_dismissed (NULL, user_data);

  return G_SOURCE_REMOVE;
}

static void
on_printer_deleted (PpPrinterEntry *printer_entry,
                    gpointer        user_data)
{
  CcPrintersPanel        *self = (CcPrintersPanel*) user_data;
  GtkLabel               *label;
  g_autofree gchar       *notification_message = NULL;
  g_autofree gchar       *printer_name = NULL;

  gtk_widget_hide (GTK_WIDGET (printer_entry));

  on_notification_dismissed (NULL, self);

  g_object_get (printer_entry,
                "printer-name", &printer_name,
                NULL);

  /* Translators: %s is the printer name */
  notification_message = g_strdup_printf (_("Printer “%s” has been deleted"),
                                          printer_name);
  label = (GtkLabel*)
    gtk_builder_get_object (self->builder, "notification-label");
  gtk_label_set_label (label, notification_message);

  self->deleted_printer_name = g_strdup (printer_name);

  gtk_revealer_set_reveal_child (self->notification, TRUE);

  self->remove_printer_timeout_id = g_timeout_add_seconds (10, on_remove_printer_timeout, self);
}

static void
on_printer_renamed (PpPrinterEntry *printer_entry,
                    gchar          *new_name,
                    gpointer        user_data)
{
  CcPrintersPanel        *self = (CcPrintersPanel*) user_data;

  g_object_get (printer_entry,
                "printer-name",
                &self->old_printer_name,
                NULL);
  self->renamed_printer_name = g_strdup (new_name);
}

static void
on_printer_changed (PpPrinterEntry *printer_entry,
                    gpointer        user_data)
{
  actualize_printers_list (user_data);
}

static void
add_printer_entry (CcPrintersPanel *self,
                   cups_dest_t      printer)
{
  PpPrinterEntry         *printer_entry;
  GtkWidget              *content;
  GSList                 *widgets, *l;

  content = (GtkWidget*) gtk_builder_get_object (self->builder, "content");

  printer_entry = pp_printer_entry_new (printer, self->is_authorized);

  widgets = pp_printer_entry_get_size_group_widgets (printer_entry);
  for (l = widgets; l != NULL; l = l->next)
    gtk_size_group_add_widget (self->size_group, GTK_WIDGET (l->data));
  g_slist_free (widgets);

  g_signal_connect (printer_entry,
                    "printer-changed",
                    G_CALLBACK (on_printer_changed),
                    self);
  g_signal_connect (printer_entry,
                    "printer-delete",
                    G_CALLBACK (on_printer_deleted),
                    self);
  g_signal_connect (printer_entry,
                    "printer-renamed",
                    G_CALLBACK (on_printer_renamed),
                    self);

  gtk_list_box_insert (GTK_LIST_BOX (content), GTK_WIDGET (printer_entry), -1);
  gtk_widget_show_all (content);

  g_hash_table_insert (self->printer_entries, g_strdup (printer.name), printer_entry);
}

static void
set_current_page (GObject      *source_object,
                  GAsyncResult *result,
                  gpointer      user_data)
{
  CcPrintersPanel        *self = (CcPrintersPanel *) user_data;
  GtkWidget              *widget;
  PpCups                 *cups = PP_CUPS (source_object);
  gboolean               success;

  success = pp_cups_connection_test_finish (cups, result, NULL);
  g_object_unref (source_object);

  widget = (GtkWidget*) gtk_builder_get_object (self->builder, "main-vbox");
  if (success)
    gtk_stack_set_visible_child_name (GTK_STACK (widget), "empty-state");
  else
    gtk_stack_set_visible_child_name (GTK_STACK (widget), "no-cups-page");

  update_sensitivity (user_data);
}

static void
actualize_printers_list_cb (GObject      *source_object,
                            GAsyncResult *result,
                            gpointer      user_data)
{
  CcPrintersPanel        *self = (CcPrintersPanel*) user_data;
  GtkWidget              *widget;
  PpCups                 *cups = PP_CUPS (source_object);
  PpCupsDests            *cups_dests;
  gboolean                new_printer_available = FALSE;
  g_autoptr(GError)       error = NULL;
  int                     i;

  cups_dests = pp_cups_get_dests_finish (cups, result, &error);

  if (cups_dests == NULL && error != NULL)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        {
          g_warning ("Could not get dests: %s", error->message);
        }

      g_object_unref (cups);
      return;
    }

  free_dests (self);
  self->dests = cups_dests->dests;
  self->num_dests = cups_dests->num_of_dests;
  g_free (cups_dests);

  widget = (GtkWidget*) gtk_builder_get_object (self->builder, "main-vbox");
  if (self->num_dests == 0 && !self->new_printer_name)
    pp_cups_connection_test_async (g_object_ref (cups), NULL, set_current_page, self);
  else
    gtk_stack_set_visible_child_name (GTK_STACK (widget), "printers-list");

  widget = (GtkWidget*) gtk_builder_get_object (self->builder, "content");
  gtk_container_foreach (GTK_CONTAINER (widget), (GtkCallback) gtk_widget_destroy, NULL);

  for (i = 0; i < self->num_dests; i++)
    {
      new_printer_available = g_strcmp0 (self->dests[i].name, self->renamed_printer_name) == 0;
      if (new_printer_available)
        break;
    }

  for (i = 0; i < self->num_dests; i++)
    {
      if (g_strcmp0 (self->dests[i].name, self->deleted_printer_name) == 0)
          continue;

      if (new_printer_available && g_strcmp0 (self->dests[i].name, self->old_printer_name) == 0)
          continue;

      add_printer_entry (self, self->dests[i]);
    }

  if (!self->entries_filled)
    {
      if (self->action != NULL)
        {
          execute_action (self, self->action);
          g_variant_unref (self->action);
          self->action = NULL;
        }

      self->entries_filled = TRUE;
    }

  update_sensitivity (user_data);

  g_object_unref (cups);
}

static void
actualize_printers_list (CcPrintersPanel *self)
{
  PpCups                 *cups;

  cups = pp_cups_new ();
  pp_cups_get_dests_async (cups,
                           self->actualize_printers_list_cancellable,
                           actualize_printers_list_cb,
                           self);
}

static void
new_printer_dialog_pre_response_cb (PpNewPrinterDialog *dialog,
                                    const gchar        *device_name,
                                    const gchar        *device_location,
                                    const gchar        *device_make_and_model,
                                    gboolean            is_network_device,
                                    gpointer            user_data)
{
  CcPrintersPanel        *self = (CcPrintersPanel*) user_data;

  self->new_printer_name = g_strdup (device_name);
  self->new_printer_location = g_strdup (device_location);
  self->new_printer_make_and_model = g_strdup (device_make_and_model);
  self->new_printer_on_network = is_network_device;

  actualize_printers_list (self);
}

static void
new_printer_dialog_response_cb (PpNewPrinterDialog *dialog,
                                gint                response_id,
                                gpointer            user_data)
{
  CcPrintersPanel        *self = (CcPrintersPanel*) user_data;
  GtkScrolledWindow      *scrolled_window;
  GtkAllocation           allocation;
  GtkAdjustment          *adjustment;
  GtkWidget              *printer_entry;

  if (self->pp_new_printer_dialog)
    g_clear_object (&self->pp_new_printer_dialog);

  g_clear_pointer (&self->new_printer_location, g_free);
  g_clear_pointer (&self->new_printer_make_and_model, g_free);

  if (response_id == GTK_RESPONSE_REJECT)
    {
      GtkWidget *message_dialog;

      message_dialog = gtk_message_dialog_new (NULL,
                                               0,
                                               GTK_MESSAGE_ERROR,
                                               GTK_BUTTONS_CLOSE,
      /* Translators: Addition of the new printer failed. */
                                               _("Failed to add new printer."));
      g_signal_connect (message_dialog,
                        "response",
                        G_CALLBACK (gtk_widget_destroy),
                        NULL);
      gtk_widget_show (message_dialog);
    }

  actualize_printers_list (self);

  if (self->new_printer_name == NULL)
    return;

  /* Scroll the view to show the newly added printer-entry. */
  scrolled_window = GTK_SCROLLED_WINDOW (gtk_builder_get_object (self->builder,
                                                                 "scrolled-window"));
  adjustment = gtk_scrolled_window_get_vadjustment (scrolled_window);

  printer_entry = GTK_WIDGET (g_hash_table_lookup (self->printer_entries,
                                                   self->new_printer_name));
  gtk_widget_get_allocation (printer_entry, &allocation);
  g_clear_pointer (&self->new_printer_name, g_free);

  gtk_adjustment_set_value (adjustment,
                            allocation.y - gtk_widget_get_margin_top (printer_entry));
}

static void
printer_add_cb (GtkToolButton *toolbutton,
                gpointer       user_data)
{
  CcPrintersPanel        *self = (CcPrintersPanel*) user_data;
  GtkWidget              *toplevel;

  toplevel = gtk_widget_get_toplevel (GTK_WIDGET (self));
  self->pp_new_printer_dialog = PP_NEW_PRINTER_DIALOG (
    pp_new_printer_dialog_new (GTK_WINDOW (toplevel),
                               self->all_ppds_list));

  g_signal_connect (self->pp_new_printer_dialog,
                    "pre-response",
                    G_CALLBACK (new_printer_dialog_pre_response_cb),
                    self);

  g_signal_connect (self->pp_new_printer_dialog,
                    "response",
                    G_CALLBACK (new_printer_dialog_response_cb),
                    self);
}

static void
update_sensitivity (gpointer user_data)
{
  CcPrintersPanel         *self = (CcPrintersPanel*) user_data;
  const char              *cups_server = NULL;
  GtkWidget               *widget;
  gboolean                 local_server = TRUE;
  gboolean                 no_cups = FALSE;

  self->is_authorized =
    self->permission &&
    g_permission_get_allowed (G_PERMISSION (self->permission)) &&
    self->lockdown_settings &&
    !g_settings_get_boolean (self->lockdown_settings, "disable-print-setup");

  gtk_stack_set_visible_child_name (GTK_STACK (self->headerbar_buttons),
    self->is_authorized ? PAGE_ADDPRINTER : PAGE_LOCK);

  widget = (GtkWidget*) gtk_builder_get_object (self->builder, "main-vbox");
  if (g_strcmp0 (gtk_stack_get_visible_child_name (GTK_STACK (widget)), "no-cups-page") == 0)
    no_cups = TRUE;

  cups_server = cupsServer ();
  if (cups_server &&
      g_ascii_strncasecmp (cups_server, "localhost", 9) != 0 &&
      g_ascii_strncasecmp (cups_server, "127.0.0.1", 9) != 0 &&
      g_ascii_strncasecmp (cups_server, "::1", 3) != 0 &&
      cups_server[0] != '/')
    local_server = FALSE;

  widget = (GtkWidget*) gtk_builder_get_object (self->builder, "headerbar-buttons");
  gtk_widget_set_visible (widget, !no_cups);

  widget = (GtkWidget*) gtk_builder_get_object (self->builder, "search-button");
  gtk_widget_set_visible (widget, !no_cups);

  widget = (GtkWidget*) gtk_builder_get_object (self->builder, "search-bar");
  gtk_widget_set_visible (widget, !no_cups);

  widget = (GtkWidget*) gtk_builder_get_object (self->builder, "printer-add-button");
  gtk_widget_set_sensitive (widget, local_server && self->is_authorized && !no_cups && !self->new_printer_name);

  widget = (GtkWidget*) gtk_builder_get_object (self->builder, "printer-add-button2");
  gtk_widget_set_sensitive (widget, local_server && self->is_authorized && !no_cups && !self->new_printer_name);
}

static void
on_permission_changed (GPermission *permission,
                       GParamSpec  *pspec,
                       gpointer     data)
{
  actualize_printers_list (data);
  update_sensitivity (data);
}

static void
on_lockdown_settings_changed (GSettings  *settings,
                              const char *key,
                              gpointer    user_data)
{
  CcPrintersPanel         *self = (CcPrintersPanel*) user_data;

  if (g_str_equal (key, "disable-print-setup") == FALSE)
    return;

#if 0
  /* FIXME */
  gtk_widget_set_sensitive (self->lock_button,
    !g_settings_get_boolean (self->lockdown_settings, "disable-print-setup"));
#endif

  on_permission_changed (self->permission, NULL, user_data);
}

static void
cups_status_check_cb (GObject      *source_object,
                      GAsyncResult *result,
                      gpointer      user_data)
{
  CcPrintersPanel        *self = (CcPrintersPanel*) user_data;
  gboolean                success;
  PpCups                 *cups = PP_CUPS (source_object);

  success = pp_cups_connection_test_finish (cups, result, NULL);
  if (success)
    {
      actualize_printers_list (self);
      attach_to_cups_notifier (self);

      g_source_remove (self->cups_status_check_id);
      self->cups_status_check_id = 0;
    }

  g_object_unref (cups);
}

static gboolean
cups_status_check (gpointer user_data)
{
  CcPrintersPanel         *self = (CcPrintersPanel*) user_data;
  PpCups                  *cups;

  cups = pp_cups_new ();
  pp_cups_connection_test_async (cups, NULL, cups_status_check_cb, self);

  return self->cups_status_check_id != 0;
}

static void
connection_test_cb (GObject      *source_object,
                    GAsyncResult *result,
                    gpointer      user_data)
{
  CcPrintersPanel        *self;
  gboolean                success;
  PpCups                 *cups = PP_CUPS (source_object);
  g_autoptr(GError)       error = NULL;

  success = pp_cups_connection_test_finish (cups, result, &error);
  g_object_unref (cups);

  if (error != NULL)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        {
          g_warning ("Could not test connection: %s", error->message);
        }

      return;
    }

  self = CC_PRINTERS_PANEL (user_data);

  if (!success)
    {
      self->cups_status_check_id =
        g_timeout_add_seconds (CUPS_STATUS_CHECK_INTERVAL, cups_status_check, self);
    }
}

static void
get_all_ppds_async_cb (PPDList  *ppds,
                       gpointer  user_data)
{
  CcPrintersPanel        *self = (CcPrintersPanel*) user_data;

  self->all_ppds_list = ppds;

  if (self->pp_ppd_selection_dialog)
    pp_ppd_selection_dialog_set_ppd_list (self->pp_ppd_selection_dialog,
                                          self->all_ppds_list);

  if (self->pp_new_printer_dialog)
    pp_new_printer_dialog_set_ppd_list (self->pp_new_printer_dialog,
                                        self->all_ppds_list);

  g_object_unref (self->get_all_ppds_cancellable);
  self->get_all_ppds_cancellable = NULL;
}

static gboolean
filter_function (GtkListBoxRow *row,
                 gpointer       user_data)
{
  CcPrintersPanel        *self = (CcPrintersPanel*) user_data;
  GtkWidget              *search_entry;
  gboolean                retval;
  g_autofree gchar       *search = NULL;
  g_autofree gchar       *name = NULL;
  g_autofree gchar       *location = NULL;
  g_autofree gchar       *printer_name = NULL;
  g_autofree gchar       *printer_location = NULL;

  search_entry = (GtkWidget*)
    gtk_builder_get_object (self->builder, "search-entry");

  if (gtk_entry_get_text_length (GTK_ENTRY (search_entry)) == 0)
    return TRUE;

  g_object_get (G_OBJECT (row),
                "printer-name", &printer_name,
                "printer-location", &printer_location,
                NULL);

  name = cc_util_normalize_casefold_and_unaccent (printer_name);
  location = cc_util_normalize_casefold_and_unaccent (printer_location);

  search = cc_util_normalize_casefold_and_unaccent (gtk_entry_get_text (GTK_ENTRY (search_entry)));

  retval = strstr (name, search) != NULL;
  if (location != NULL)
      retval = retval || (strstr (location, search) != NULL);

  return retval;
}

static void
cc_printers_panel_init (CcPrintersPanel *self)
{
  GtkWidget              *top_widget;
  GtkWidget              *widget;
  PpCups                 *cups;
  g_autoptr(GError)       error = NULL;
  gchar                  *objects[] = { "overlay", "headerbar-buttons", "search-button", NULL };
  guint                   builder_result;

  g_resources_register (cc_printers_get_resource ());

  /* initialize main data structure */
  self->builder = gtk_builder_new ();
  self->dests = NULL;
  self->num_dests = 0;

  self->pp_new_printer_dialog = NULL;

  self->subscription_id = 0;
  self->cups_status_check_id = 0;
  self->subscription_renewal_id = 0;
  self->cups_proxy = NULL;
  self->cups_bus_connection = NULL;
  self->dbus_subscription_id = 0;
  self->remove_printer_timeout_id = 0;

  self->new_printer_name = NULL;
  self->new_printer_location = NULL;
  self->new_printer_make_and_model = NULL;
  self->new_printer_on_network = FALSE;

  self->renamed_printer_name = NULL;
  self->old_printer_name = NULL;
  self->deleted_printer_name = NULL;

  self->permission = NULL;
  self->lockdown_settings = NULL;

  self->all_ppds_list = NULL;

  self->printer_entries = g_hash_table_new_full (g_str_hash,
                                                 g_str_equal,
                                                 g_free,
                                                 NULL);
  self->entries_filled = FALSE;
  self->action = NULL;

  self->actualize_printers_list_cancellable = g_cancellable_new ();
  self->cups_status_check_cancellable = g_cancellable_new ();

  builder_result = gtk_builder_add_objects_from_resource (self->builder,
                                                          "/org/gnome/control-center/printers/printers.ui",
                                                          objects, &error);

  if (builder_result == 0)
    {
      /* Translators: The XML file containing user interface can not be loaded */
      g_warning (_("Could not load ui: %s"), error->message);
      return;
    }

  widget = (GtkWidget*)
    gtk_builder_get_object (self->builder, "headerbar-buttons");
  self->headerbar_buttons = widget;

  self->notification = (GtkRevealer*)
    gtk_builder_get_object (self->builder, "notification");

  widget = (GtkWidget*)
    gtk_builder_get_object (self->builder, "notification-undo-button");
  g_signal_connect (widget, "clicked", G_CALLBACK (on_printer_deletion_undone), self);

  widget = (GtkWidget*)
    gtk_builder_get_object (self->builder, "notification-dismiss-button");
  g_signal_connect (widget, "clicked", G_CALLBACK (on_notification_dismissed), self);

  /* add the top level widget */
  top_widget = (GtkWidget*)
    gtk_builder_get_object (self->builder, "overlay");

  /* connect signals */
  widget = (GtkWidget*)
    gtk_builder_get_object (self->builder, "printer-add-button");
  g_signal_connect (widget, "clicked", G_CALLBACK (printer_add_cb), self);

  widget = (GtkWidget*)
    gtk_builder_get_object (self->builder, "printer-add-button2");
  g_signal_connect (widget, "clicked", G_CALLBACK (printer_add_cb), self);

  widget = (GtkWidget*)
    gtk_builder_get_object (self->builder, "content");
  gtk_list_box_set_filter_func (GTK_LIST_BOX (widget),
                                filter_function,
                                self,
                                NULL);
  g_signal_connect_swapped (gtk_builder_get_object (self->builder, "search-entry"),
                            "search-changed",
                            G_CALLBACK (gtk_list_box_invalidate_filter),
                            widget);

  self->lockdown_settings = g_settings_new ("org.gnome.desktop.lockdown");
  if (self->lockdown_settings)
    g_signal_connect_object (self->lockdown_settings,
                             "changed",
                             G_CALLBACK (on_lockdown_settings_changed),
                             self,
                             G_CONNECT_AFTER);

  /* Add unlock button */
  self->permission = (GPermission *)polkit_permission_new_sync (
    "org.opensuse.cupspkhelper.mechanism.all-edit", NULL, NULL, NULL);
  if (self->permission != NULL)
    {
      g_signal_connect_object (self->permission,
                               "notify",
                               G_CALLBACK (on_permission_changed),
                               self,
                               G_CONNECT_AFTER);
      on_permission_changed (self->permission, NULL, self);
    }
  else
    g_warning ("Your system does not have the cups-pk-helper's policy \
\"org.opensuse.cupspkhelper.mechanism.all-edit\" installed. \
Please check your installation");

  self->subscription_renew_cancellable = g_cancellable_new ();

  self->size_group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);

  actualize_printers_list (self);
  attach_to_cups_notifier (self);

  self->get_all_ppds_cancellable = g_cancellable_new ();
  get_all_ppds_async (self->get_all_ppds_cancellable,
                      get_all_ppds_async_cb,
                      self);

  cups = pp_cups_new ();
  pp_cups_connection_test_async (cups, self->cups_status_check_cancellable, connection_test_cb, self);
  gtk_container_add (GTK_CONTAINER (self), top_widget);
  gtk_widget_show_all (GTK_WIDGET (self));
}
