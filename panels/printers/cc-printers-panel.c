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

CC_PANEL_REGISTER (CcPrintersPanel, cc_printers_panel)

#define PRINTERS_PANEL_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), CC_TYPE_PRINTERS_PANEL, CcPrintersPanelPrivate))

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

struct _CcPrintersPanelPrivate
{
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

  GtkWidget    *headerbar_buttons;
  PPDList      *all_ppds_list;
  GCancellable *get_all_ppds_cancellable;
  GCancellable *subscription_renew_cancellable;

  gchar    *new_printer_name;
  gchar    *new_printer_location;
  gchar    *new_printer_make_and_model;
  gboolean  new_printer_on_network;
  gboolean  select_new_printer;

  gchar    *renamed_printer_name;

  GHashTable *printer_entries;

  gpointer dummy;
};

#define PAGE_LOCK "_lock"
#define PAGE_ADDPRINTER "_addprinter"

typedef struct
{
  gchar        *printer_name;
  GCancellable *cancellable;
} SetPPDItem;

static void actualize_printers_list (CcPrintersPanel *self);
static void update_sensitivity (gpointer user_data);
static void detach_from_cups_notifier (gpointer data);
static void free_dests (CcPrintersPanel *self);

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
  switch (property_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
cc_printers_panel_constructed (GObject *object)
{
  CcPrintersPanel *self = CC_PRINTERS_PANEL (object);
  CcPrintersPanelPrivate *priv = self->priv;
  GtkLockButton *button;
  CcShell *shell;

  G_OBJECT_CLASS (cc_printers_panel_parent_class)->constructed (object);

  shell = cc_panel_get_shell (CC_PANEL (self));
  cc_shell_embed_widget_in_header (shell, priv->headerbar_buttons);

  button = (GtkLockButton*)
    gtk_builder_get_object (priv->builder, "lock-button");
  gtk_lock_button_set_permission (button, priv->permission);
}

static void
cc_printers_panel_dispose (GObject *object)
{
  CcPrintersPanelPrivate *priv = CC_PRINTERS_PANEL (object)->priv;

  if (priv->pp_new_printer_dialog)
    g_clear_object (&priv->pp_new_printer_dialog);

  free_dests (CC_PRINTERS_PANEL (object));

  g_clear_pointer (&priv->new_printer_name, g_free);
  g_clear_pointer (&priv->new_printer_location, g_free);
  g_clear_pointer (&priv->new_printer_make_and_model, g_free);

  g_clear_pointer (&priv->renamed_printer_name, g_free);

  if (priv->builder)
    {
      g_object_unref (priv->builder);
      priv->builder = NULL;
    }

  if (priv->lockdown_settings)
    {
      g_object_unref (priv->lockdown_settings);
      priv->lockdown_settings = NULL;
    }

  if (priv->permission)
    {
      g_object_unref (priv->permission);
      priv->permission = NULL;
    }

  g_cancellable_cancel (priv->subscription_renew_cancellable);
  g_clear_object (&priv->subscription_renew_cancellable);

  detach_from_cups_notifier (CC_PRINTERS_PANEL (object));

  if (priv->cups_status_check_id > 0)
    {
      g_source_remove (priv->cups_status_check_id);
      priv->cups_status_check_id = 0;
    }

  if (priv->all_ppds_list)
    {
      ppd_list_free (priv->all_ppds_list);
      priv->all_ppds_list = NULL;
    }

  if (priv->get_all_ppds_cancellable)
    {
      g_cancellable_cancel (priv->get_all_ppds_cancellable);
      g_object_unref (priv->get_all_ppds_cancellable);
      priv->get_all_ppds_cancellable = NULL;
    }

  g_clear_pointer (&priv->printer_entries, g_hash_table_destroy);

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

  g_type_class_add_private (klass, sizeof (CcPrintersPanelPrivate));

  object_class->get_property = cc_printers_panel_get_property;
  object_class->set_property = cc_printers_panel_set_property;
  object_class->constructed = cc_printers_panel_constructed;
  object_class->dispose = cc_printers_panel_dispose;
  object_class->finalize = cc_printers_panel_finalize;

  panel_class->get_help_uri = cc_printers_panel_get_help_uri;
}

static void
on_get_job_attributes_cb (GObject      *source_object,
                          GAsyncResult *res,
                          gpointer      user_data)
{
  CcPrintersPanel        *self = (CcPrintersPanel*) user_data;
  CcPrintersPanelPrivate *priv;
  const gchar            *job_originating_user_name;
  const gchar            *job_printer_uri;
  GVariant               *attributes;
  GVariant               *username;
  GVariant               *printer_uri;
  GError                 *error = NULL;

  priv = PRINTERS_PANEL_PRIVATE (self);

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
                  priv->dests != NULL)
                {
                  PpPrinterEntry *printer_entry;
                  gchar *printer_name;

                  printer_name = g_strrstr (job_printer_uri, "/") + 1;
                  printer_entry = PP_PRINTER_ENTRY (g_hash_table_lookup (priv->printer_entries, printer_name));

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
  CcPrintersPanelPrivate *priv;
  CcPrintersPanel        *self = (CcPrintersPanel*) user_data;
  PpCups                 *cups = PP_CUPS (source_object);
  gint                    subscription_id;

  subscription_id = pp_cups_renew_subscription_finish (cups, result);
  g_object_unref (source_object);

  if (subscription_id > 0)
    {
      priv = self->priv;

      priv->subscription_id = subscription_id;
    }
}

static gboolean
renew_subscription (gpointer data)
{
  CcPrintersPanelPrivate *priv;
  CcPrintersPanel        *self = (CcPrintersPanel*) data;
  PpCups                 *cups;

  priv = PRINTERS_PANEL_PRIVATE (self);

  cups = pp_cups_new ();
  pp_cups_renew_subscription_async (cups,
                                    priv->subscription_id,
                                    subscription_events,
                                    SUBSCRIPTION_DURATION,
                                    priv->subscription_renew_cancellable,
                                    renew_subscription_cb,
                                    data);

  return G_SOURCE_CONTINUE;
}

static void
attach_to_cups_notifier_cb (GObject      *source_object,
                            GAsyncResult *result,
                            gpointer      user_data)
{
  CcPrintersPanelPrivate *priv;
  CcPrintersPanel        *self = (CcPrintersPanel*) user_data;
  PpCups                 *cups = PP_CUPS (source_object);
  GError                 *error = NULL;
  gint                    subscription_id;

  subscription_id = pp_cups_renew_subscription_finish (cups, result);
  g_object_unref (source_object);

  if (subscription_id > 0)
    {
      priv = self->priv;

      priv->subscription_id = subscription_id;

      priv->subscription_renewal_id =
        g_timeout_add_seconds (RENEW_INTERVAL, renew_subscription, self);

      priv->cups_proxy = g_dbus_proxy_new_for_bus_sync (G_BUS_TYPE_SYSTEM,
                                                        0,
                                                        NULL,
                                                        CUPS_DBUS_NAME,
                                                        CUPS_DBUS_PATH,
                                                        CUPS_DBUS_INTERFACE,
                                                        NULL,
                                                        &error);

      if (!priv->cups_proxy)
        {
          g_warning ("%s", error->message);
          g_error_free (error);
          return;
        }

      priv->cups_bus_connection = g_dbus_proxy_get_connection (priv->cups_proxy);

      priv->dbus_subscription_id =
        g_dbus_connection_signal_subscribe (priv->cups_bus_connection,
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
  CcPrintersPanelPrivate *priv;
  CcPrintersPanel        *self = (CcPrintersPanel*) data;
  PpCups                 *cups;

  priv = self->priv;

  cups = pp_cups_new ();
  pp_cups_renew_subscription_async (cups,
                                    priv->subscription_id,
                                    subscription_events,
                                    SUBSCRIPTION_DURATION,
                                    priv->subscription_renew_cancellable,
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
  CcPrintersPanelPrivate *priv;
  CcPrintersPanel        *self = (CcPrintersPanel*) data;
  PpCups                 *cups;

  priv = PRINTERS_PANEL_PRIVATE (self);

  if (priv->dbus_subscription_id != 0) {
    g_dbus_connection_signal_unsubscribe (priv->cups_bus_connection,
                                          priv->dbus_subscription_id);
    priv->dbus_subscription_id = 0;
  }

  cups = pp_cups_new ();
  pp_cups_cancel_subscription_async (cups,
                                     priv->subscription_id,
                                     subscription_cancel_cb,
                                     NULL);

  priv->subscription_id = 0;

  if (priv->subscription_renewal_id != 0) {
    g_source_remove (priv->subscription_renewal_id);
    priv->subscription_renewal_id = 0;
  }

  if (priv->cups_proxy != NULL) {
    g_object_unref (priv->cups_proxy);
    priv->cups_proxy = NULL;
  }
}

static void
free_dests (CcPrintersPanel *self)
{
  CcPrintersPanelPrivate *priv;

  priv = PRINTERS_PANEL_PRIVATE (self);

  if (priv->num_dests > 0)
    {
      cupsFreeDests (priv->num_dests, priv->dests);
    }
  priv->dests = NULL;
  priv->num_dests = 0;
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
  CcPrintersPanelPrivate *priv;
  PpPrinterEntry         *printer_entry;
  GtkWidget              *content;

  priv = PRINTERS_PANEL_PRIVATE (self);

  content = (GtkWidget*) gtk_builder_get_object (priv->builder, "content");

  printer_entry = pp_printer_entry_new (printer, priv->is_authorized);
  g_signal_connect (printer_entry,
                    "printer-changed",
                    G_CALLBACK (on_printer_changed),
                    self);

  gtk_box_pack_start (GTK_BOX (content), GTK_WIDGET (printer_entry), FALSE, TRUE, 5);
  gtk_widget_show_all (content);

  g_hash_table_insert (priv->printer_entries, g_strdup (printer.name), printer_entry);
}

static void
set_current_page (GObject      *source_object,
                  GAsyncResult *result,
                  gpointer      user_data)
{
  GtkWidget *widget = GTK_WIDGET (user_data);
  PpCups    *cups = PP_CUPS (source_object);
  gboolean   success;

  success = pp_cups_connection_test_finish (cups, result);
  g_object_unref (source_object);

  if (success)
    gtk_stack_set_visible_child_name (GTK_STACK (widget), "empty-state");
  else
    gtk_stack_set_visible_child_name (GTK_STACK (widget), "no-cups-page");
}

static void
actualize_printers_list_cb (GObject      *source_object,
                            GAsyncResult *result,
                            gpointer      user_data)
{
  CcPrintersPanelPrivate *priv;
  CcPrintersPanel        *self = (CcPrintersPanel*) user_data;
  GtkWidget              *widget;
  PpCups                 *cups = PP_CUPS (source_object);
  PpCupsDests            *cups_dests;
  int                     i;

  priv = PRINTERS_PANEL_PRIVATE (self);

  free_dests (self);
  cups_dests = pp_cups_get_dests_finish (cups, result, NULL);

  priv->dests = cups_dests->dests;
  priv->num_dests = cups_dests->num_of_dests;
  g_free (cups_dests);

  widget = (GtkWidget*) gtk_builder_get_object (priv->builder, "main-vbox");
  if (priv->num_dests == 0 && !priv->new_printer_name)
    pp_cups_connection_test_async (g_object_ref (cups), set_current_page, widget);
  else
    gtk_stack_set_visible_child_name (GTK_STACK (widget), "printers-list");

  widget = (GtkWidget*) gtk_builder_get_object (priv->builder, "content");
  gtk_container_foreach (GTK_CONTAINER (widget), (GtkCallback) gtk_widget_destroy, NULL);
  for (i = 0; i < priv->num_dests; i++)
    add_printer_entry (self, priv->dests[i]);
}

static void
actualize_printers_list (CcPrintersPanel *self)
{
  PpCups *cups;

  cups = pp_cups_new ();
  pp_cups_get_dests_async (cups, NULL, actualize_printers_list_cb, self);
}

static void
new_printer_dialog_pre_response_cb (PpNewPrinterDialog *dialog,
                                    const gchar        *device_name,
                                    const gchar        *device_location,
                                    const gchar        *device_make_and_model,
                                    gboolean            is_network_device,
                                    gpointer            user_data)
{
  CcPrintersPanelPrivate *priv;
  CcPrintersPanel        *self = (CcPrintersPanel*) user_data;

  priv = PRINTERS_PANEL_PRIVATE (self);

  priv->new_printer_name = g_strdup (device_name);
  priv->new_printer_location = g_strdup (device_location);
  priv->new_printer_make_and_model = g_strdup (device_make_and_model);
  priv->new_printer_on_network = is_network_device;
  priv->select_new_printer = TRUE;

  actualize_printers_list (self);
}

static void
new_printer_dialog_response_cb (PpNewPrinterDialog *dialog,
                                gint                response_id,
                                gpointer            user_data)
{
  CcPrintersPanelPrivate *priv;
  CcPrintersPanel        *self = (CcPrintersPanel*) user_data;

  priv = PRINTERS_PANEL_PRIVATE (self);

  if (priv->pp_new_printer_dialog)
    g_clear_object (&priv->pp_new_printer_dialog);

  g_clear_pointer (&priv->new_printer_name, g_free);
  g_clear_pointer (&priv->new_printer_location, g_free);
  g_clear_pointer (&priv->new_printer_make_and_model, g_free);

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
}

static void
printer_add_cb (GtkToolButton *toolbutton,
                gpointer       user_data)
{
  CcPrintersPanelPrivate *priv;
  CcPrintersPanel        *self = (CcPrintersPanel*) user_data;
  GtkWidget              *toplevel;

  priv = PRINTERS_PANEL_PRIVATE (self);

  toplevel = gtk_widget_get_toplevel (GTK_WIDGET (self));
  priv->pp_new_printer_dialog = PP_NEW_PRINTER_DIALOG (
    pp_new_printer_dialog_new (GTK_WINDOW (toplevel),
                               priv->all_ppds_list));

  g_signal_connect (priv->pp_new_printer_dialog,
                    "pre-response",
                    G_CALLBACK (new_printer_dialog_pre_response_cb),
                    self);

  g_signal_connect (priv->pp_new_printer_dialog,
                    "response",
                    G_CALLBACK (new_printer_dialog_response_cb),
                    self);
}

static void
update_sensitivity (gpointer user_data)
{
  CcPrintersPanelPrivate  *priv;
  CcPrintersPanel         *self = (CcPrintersPanel*) user_data;
  const char              *cups_server = NULL;
  GtkWidget               *widget;
  gboolean                 local_server = TRUE;
  gboolean                 no_cups = FALSE;

  priv = PRINTERS_PANEL_PRIVATE (self);

  priv->is_authorized =
    priv->permission &&
    g_permission_get_allowed (G_PERMISSION (priv->permission)) &&
    priv->lockdown_settings &&
    !g_settings_get_boolean (priv->lockdown_settings, "disable-print-setup");

  gtk_stack_set_visible_child_name (GTK_STACK (priv->headerbar_buttons),
    priv->is_authorized ? PAGE_ADDPRINTER : PAGE_LOCK);

  widget = (GtkWidget*) gtk_builder_get_object (priv->builder, "main-vbox");
  if (g_strcmp0 (gtk_stack_get_visible_child_name (GTK_STACK (widget)), "no-cups-page") == 0)
    no_cups = TRUE;

  cups_server = cupsServer ();
  if (cups_server &&
      g_ascii_strncasecmp (cups_server, "localhost", 9) != 0 &&
      g_ascii_strncasecmp (cups_server, "127.0.0.1", 9) != 0 &&
      g_ascii_strncasecmp (cups_server, "::1", 3) != 0 &&
      cups_server[0] != '/')
    local_server = FALSE;

  widget = (GtkWidget*) gtk_builder_get_object (priv->builder, "printer-add-button");
  gtk_widget_set_sensitive (widget, local_server && priv->is_authorized && !no_cups && !priv->new_printer_name);

  widget = (GtkWidget*) gtk_builder_get_object (priv->builder, "printer-add-button2");
  gtk_widget_set_sensitive (widget, local_server && priv->is_authorized && !no_cups && !priv->new_printer_name);
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
  CcPrintersPanelPrivate  *priv;
  CcPrintersPanel         *self = (CcPrintersPanel*) user_data;

  if (g_str_equal (key, "disable-print-setup") == FALSE)
    return;

  priv = PRINTERS_PANEL_PRIVATE (self);

#if 0
  /* FIXME */
  gtk_widget_set_sensitive (priv->lock_button,
    !g_settings_get_boolean (priv->lockdown_settings, "disable-print-setup"));
#endif

  on_permission_changed (priv->permission, NULL, user_data);
}

static void
cups_status_check_cb (GObject      *source_object,
                      GAsyncResult *result,
                      gpointer      user_data)
{
  CcPrintersPanelPrivate *priv;
  CcPrintersPanel        *self = (CcPrintersPanel*) user_data;
  gboolean                success;
  PpCups                 *cups = PP_CUPS (source_object);

  priv = self->priv;

  success = pp_cups_connection_test_finish (cups, result);
  if (success)
    {
      actualize_printers_list (self);
      attach_to_cups_notifier (self);

      g_source_remove (priv->cups_status_check_id);
      priv->cups_status_check_id = 0;
    }

  g_object_unref (cups);
}

static gboolean
cups_status_check (gpointer user_data)
{
  CcPrintersPanelPrivate  *priv;
  CcPrintersPanel         *self = (CcPrintersPanel*) user_data;
  PpCups                  *cups;

  priv = self->priv;

  cups = pp_cups_new ();
  pp_cups_connection_test_async (cups, cups_status_check_cb, self);

  return priv->cups_status_check_id != 0;
}

static void
connection_test_cb (GObject      *source_object,
                    GAsyncResult *result,
                    gpointer      user_data)
{
  CcPrintersPanelPrivate *priv;
  CcPrintersPanel        *self = (CcPrintersPanel*) user_data;
  gboolean                success;
  PpCups                 *cups = PP_CUPS (source_object);

  priv = self->priv;

  success = pp_cups_connection_test_finish (cups, result);
  if (!success)
    {
      priv->cups_status_check_id =
        g_timeout_add_seconds (CUPS_STATUS_CHECK_INTERVAL, cups_status_check, self);
    }

  g_object_unref (cups);
}

static void
get_all_ppds_async_cb (PPDList  *ppds,
                       gpointer  user_data)
{
  CcPrintersPanelPrivate *priv;
  CcPrintersPanel        *self = (CcPrintersPanel*) user_data;

  priv = self->priv = PRINTERS_PANEL_PRIVATE (self);

  priv->all_ppds_list = ppds;

  if (priv->pp_ppd_selection_dialog)
    pp_ppd_selection_dialog_set_ppd_list (priv->pp_ppd_selection_dialog,
                                          priv->all_ppds_list);

  if (priv->pp_new_printer_dialog)
    pp_new_printer_dialog_set_ppd_list (priv->pp_new_printer_dialog,
                                        priv->all_ppds_list);

  g_object_unref (priv->get_all_ppds_cancellable);
  priv->get_all_ppds_cancellable = NULL;
}

static void
cc_printers_panel_init (CcPrintersPanel *self)
{
  CcPrintersPanelPrivate *priv;
  GtkWidget              *top_widget;
  GtkWidget              *widget;
  PpCups                 *cups;
  GError                 *error = NULL;
  gchar                  *objects[] = { "main-vbox", "headerbar-buttons", NULL };
  guint                   builder_result;

  priv = self->priv = PRINTERS_PANEL_PRIVATE (self);
  g_resources_register (cc_printers_get_resource ());

  /* initialize main data structure */
  priv->builder = gtk_builder_new ();
  priv->dests = NULL;
  priv->num_dests = 0;

  priv->pp_new_printer_dialog = NULL;

  priv->subscription_id = 0;
  priv->cups_status_check_id = 0;
  priv->subscription_renewal_id = 0;
  priv->cups_proxy = NULL;
  priv->cups_bus_connection = NULL;
  priv->dbus_subscription_id = 0;

  priv->new_printer_name = NULL;
  priv->new_printer_location = NULL;
  priv->new_printer_make_and_model = NULL;
  priv->new_printer_on_network = FALSE;
  priv->select_new_printer = FALSE;

  priv->renamed_printer_name = NULL;

  priv->permission = NULL;
  priv->lockdown_settings = NULL;

  priv->all_ppds_list = NULL;

  priv->printer_entries = g_hash_table_new_full (g_str_hash,
                                                 g_str_equal,
                                                 g_free,
                                                 NULL);

  builder_result = gtk_builder_add_objects_from_resource (priv->builder,
                                                          "/org/gnome/control-center/printers/printers.ui",
                                                          objects, &error);

  if (builder_result == 0)
    {
      /* Translators: The XML file containing user interface can not be loaded */
      g_warning (_("Could not load ui: %s"), error->message);
      g_error_free (error);
      return;
    }

  widget = (GtkWidget*)
    gtk_builder_get_object (priv->builder, "headerbar-buttons");
  priv->headerbar_buttons = widget;

  /* add the top level widget */
  top_widget = (GtkWidget*)
    gtk_builder_get_object (priv->builder, "main-vbox");

  /* connect signals */
  widget = (GtkWidget*)
    gtk_builder_get_object (priv->builder, "printer-add-button");
  g_signal_connect (widget, "clicked", G_CALLBACK (printer_add_cb), self);

  widget = (GtkWidget*)
    gtk_builder_get_object (priv->builder, "printer-add-button2");
  g_signal_connect (widget, "clicked", G_CALLBACK (printer_add_cb), self);

  priv->lockdown_settings = g_settings_new ("org.gnome.desktop.lockdown");
  if (priv->lockdown_settings)
    g_signal_connect_object (priv->lockdown_settings,
                             "changed",
                             G_CALLBACK (on_lockdown_settings_changed),
                             self,
                             G_CONNECT_AFTER);

  /* Add unlock button */
  priv->permission = (GPermission *)polkit_permission_new_sync (
    "org.opensuse.cupspkhelper.mechanism.all-edit", NULL, NULL, NULL);
  if (priv->permission != NULL)
    {
      g_signal_connect_object (priv->permission,
                               "notify",
                               G_CALLBACK (on_permission_changed),
                               self,
                               G_CONNECT_AFTER);
      on_permission_changed (priv->permission, NULL, self);
    }
  else
    g_warning ("Your system does not have the cups-pk-helper's policy \
\"org.opensuse.cupspkhelper.mechanism.all-edit\" installed. \
Please check your installation");

  priv->subscription_renew_cancellable = g_cancellable_new ();

  actualize_printers_list (self);
  attach_to_cups_notifier (self);

  priv->get_all_ppds_cancellable = g_cancellable_new ();
  get_all_ppds_async (priv->get_all_ppds_cancellable,
                      get_all_ppds_async_cb,
                      self);

  cups = pp_cups_new ();
  pp_cups_connection_test_async (cups, connection_test_cb, self);

  gtk_container_add (GTK_CONTAINER (self), top_widget);
  gtk_widget_show_all (GTK_WIDGET (self));
}
