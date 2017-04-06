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

#include "cc-editable-entry.h"
#include "pp-new-printer-dialog.h"
#include "pp-ppd-selection-dialog.h"
#include "pp-options-dialog.h"
#include "pp-jobs-dialog.h"
#include "pp-utils.h"
#include "pp-maintenance-command.h"
#include "pp-cups.h"
#include "pp-job.h"

CC_PANEL_REGISTER (CcPrintersPanel, cc_printers_panel)

#define PRINTERS_PANEL_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), CC_TYPE_PRINTERS_PANEL, CcPrintersPanelPrivate))

#define SUPPLY_BAR_HEIGHT 20

#define EMPTY_TEXT "\xe2\x80\x94"

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
  gchar **dest_model_names;
  gchar **ppd_file_names;
  int num_dests;
  int current_dest;

  int num_jobs;

  GPermission *permission;

  GSettings *lockdown_settings;

  PpNewPrinterDialog   *pp_new_printer_dialog;
  PpPPDSelectionDialog *pp_ppd_selection_dialog;
  PpOptionsDialog      *pp_options_dialog;
  PpJobsDialog         *pp_jobs_dialog;

  GDBusProxy      *cups_proxy;
  GDBusConnection *cups_bus_connection;
  gint             subscription_id;
  guint            subscription_renewal_id;
  guint            cups_status_check_id;
  guint            dbus_subscription_id;

  GtkWidget    *popup_menu;
  GList        *driver_change_list;
  GCancellable *get_ppd_name_cancellable;
  gboolean      getting_ppd_names;
  PPDList      *all_ppds_list;
  GHashTable   *preferred_drivers;
  GCancellable *get_all_ppds_cancellable;
  GCancellable *subscription_renew_cancellable;
  GCancellable *actualize_printers_list_cancellable;

  gchar    *new_printer_name;
  gchar    *new_printer_location;
  gchar    *new_printer_make_and_model;
  gboolean  new_printer_on_network;
  gboolean  select_new_printer;

  gchar    *renamed_printer_name;

  gpointer dummy;
};

typedef struct
{
  gchar        *printer_name;
  GCancellable *cancellable;
} SetPPDItem;

static void update_jobs_count (CcPrintersPanel *self);
static void actualize_printers_list (CcPrintersPanel *self);
static void update_sensitivity (gpointer user_data);
static void printer_disable_cb (GObject *gobject, GParamSpec *pspec, gpointer user_data);
static void printer_set_default_cb (GtkToggleButton *button, gpointer user_data);
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

  g_cancellable_cancel (priv->actualize_printers_list_cancellable);
  g_clear_object (&priv->actualize_printers_list_cancellable);

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

  if (priv->preferred_drivers)
    {
      g_hash_table_unref (priv->preferred_drivers);
      priv->preferred_drivers = NULL;
    }

  if (priv->get_all_ppds_cancellable)
    {
      g_cancellable_cancel (priv->get_all_ppds_cancellable);
      g_object_unref (priv->get_all_ppds_cancellable);
      priv->get_all_ppds_cancellable = NULL;
    }

  if (priv->driver_change_list)
    {
      GList *iter;

      for (iter = priv->driver_change_list; iter; iter = iter->next)
        {
          SetPPDItem *item = (SetPPDItem *) iter->data;

          g_cancellable_cancel (item->cancellable);
          g_object_unref (item->cancellable);
          g_free (item->printer_name);
          g_free (item);
        }

      g_list_free (priv->driver_change_list);
      priv->driver_change_list = NULL;
    }

  G_OBJECT_CLASS (cc_printers_panel_parent_class)->dispose (object);
}

static void
cc_printers_panel_finalize (GObject *object)
{
  G_OBJECT_CLASS (cc_printers_panel_parent_class)->finalize (object);
}

static GPermission *
cc_printers_panel_get_permission (CcPanel *panel)
{
  CcPrintersPanelPrivate *priv = CC_PRINTERS_PANEL (panel)->priv;

  return priv->permission;
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
  object_class->dispose = cc_printers_panel_dispose;
  object_class->finalize = cc_printers_panel_finalize;

  panel_class->get_permission = cc_printers_panel_get_permission;
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
                  priv->current_dest >= 0 &&
                  priv->current_dest < priv->num_dests &&
                  priv->dests != NULL &&
                  g_strcmp0 (g_strrstr (job_printer_uri, "/") + 1,
                                        priv->dests[priv->current_dest].name) == 0)
                {
                  update_jobs_count (self);
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
  gint                    i;

  priv = PRINTERS_PANEL_PRIVATE (self);

  if (priv->num_dests > 0)
    {
      for (i = 0; i < priv->num_dests; i++)
        {
          g_free (priv->dest_model_names[i]);
          if (priv->ppd_file_names[i]) {
            g_unlink (priv->ppd_file_names[i]);
            g_free (priv->ppd_file_names[i]);
          }
        }
      g_free (priv->dest_model_names);
      g_free (priv->ppd_file_names);
      cupsFreeDests (priv->num_dests, priv->dests);
    }
  priv->dests = NULL;
  priv->num_dests = 0;
  priv->current_dest = -1;
  priv->dest_model_names = NULL;
  priv->ppd_file_names = NULL;
}

enum
{
  NOTEBOOK_INFO_PAGE = 0,
  NOTEBOOK_NO_PRINTERS_PAGE,
  NOTEBOOK_NO_CUPS_PAGE,
  NOTEBOOK_N_PAGES
};

enum
{
  PRINTER_ID_COLUMN,
  PRINTER_NAME_COLUMN,
  PRINTER_PAUSED_COLUMN,
  PRINTER_DEFAULT_ICON_COLUMN,
  PRINTER_ICON_COLUMN,
  PRINTER_N_COLUMNS
};

static void
printer_selection_changed_cb (GtkTreeSelection *selection,
                              gpointer          user_data)
{
  CcPrintersPanelPrivate *priv;
  CcPrintersPanel        *self = (CcPrintersPanel*) user_data;
  GtkTreeModel           *model;
  cups_ptype_t            type = 0;
  GtkTreeIter             iter;
  GtkWidget              *widget;
  GtkWidget              *model_button_label;
  GtkWidget              *model_label;
  gboolean                is_accepting_jobs = TRUE;
  GValue                  value = G_VALUE_INIT;
  gchar                  *printer_make_and_model = NULL;
  gchar                  *printer_model = NULL;
  gchar                  *reason = NULL;
  gchar                 **printer_reasons = NULL;
  gchar                  *marker_types = NULL;
  gchar                  *printer_name = NULL;
  gchar                  *printer_icon = NULL;
  gchar                  *printer_type = NULL;
  gchar                  *supply_type = NULL;
  gchar                  *printer_uri = NULL;
  gchar                  *location = NULL;
  gchar                  *status = NULL;
  gchar                  *device_uri = NULL;
  gchar                  *printer_hostname = NULL;
  int                     printer_state = 3;
  int                     id = -1;
  int                     i, j;
  static const char * const reasons[] =
    {
      "toner-low",
      "toner-empty",
      "developer-low",
      "developer-empty",
      "marker-supply-low",
      "marker-supply-empty",
      "cover-open",
      "door-open",
      "media-low",
      "media-empty",
      "offline",
      "paused",
      "marker-waste-almost-full",
      "marker-waste-full",
      "opc-near-eol",
      "opc-life-over"
    };
  static const char * statuses[] =
    {
      /* Translators: The printer is low on toner */
      N_("Low on toner"),
      /* Translators: The printer has no toner left */
      N_("Out of toner"),
      /* Translators: "Developer" is a chemical for photo development,
       * http://en.wikipedia.org/wiki/Photographic_developer */
      N_("Low on developer"),
      /* Translators: "Developer" is a chemical for photo development,
       * http://en.wikipedia.org/wiki/Photographic_developer */
      N_("Out of developer"),
      /* Translators: "marker" is one color bin of the printer */
      N_("Low on a marker supply"),
      /* Translators: "marker" is one color bin of the printer */
      N_("Out of a marker supply"),
      /* Translators: One or more covers on the printer are open */
      N_("Open cover"),
      /* Translators: One or more doors on the printer are open */
      N_("Open door"),
      /* Translators: At least one input tray is low on media */
      N_("Low on paper"),
      /* Translators: At least one input tray is empty */
      N_("Out of paper"),
      /* Translators: The printer is offline */
      NC_("printer state", "Offline"),
      /* Translators: Someone has stopped the Printer */
      NC_("printer state", "Stopped"),
      /* Translators: The printer marker supply waste receptacle is almost full */
      N_("Waste receptacle almost full"),
      /* Translators: The printer marker supply waste receptacle is full */
      N_("Waste receptacle full"),
      /* Translators: Optical photo conductors are used in laser printers */
      N_("The optical photo conductor is near end of life"),
      /* Translators: Optical photo conductors are used in laser printers */
      N_("The optical photo conductor is no longer functioning")
    };

  priv = PRINTERS_PANEL_PRIVATE (self);

  if (gtk_tree_selection_get_selected (selection, &model, &iter))
    {
      gtk_tree_model_get (model, &iter,
			  PRINTER_ID_COLUMN, &id,
			  PRINTER_NAME_COLUMN, &printer_name,
			  PRINTER_ICON_COLUMN, &printer_icon,
			  -1);
    }
  else
    id = -1;

  priv->current_dest = id;

  update_jobs_count (self);

  if (priv->current_dest >= 0 &&
      priv->current_dest < priv->num_dests &&
      priv->dests != NULL)
    {
      widget = (GtkWidget*)
        gtk_builder_get_object (priv->builder, "main-vbox");
      gtk_stack_set_visible_child_name (GTK_STACK (widget), "printers-list");

      for (i = 0; i < priv->dests[id].num_options; i++)
        {
          if (g_strcmp0 (priv->dests[priv->current_dest].options[i].name, "printer-location") == 0)
            location = g_strdup (priv->dests[priv->current_dest].options[i].value);
          else if (g_strcmp0 (priv->dests[priv->current_dest].options[i].name, "printer-state") == 0)
            printer_state = atoi (priv->dests[priv->current_dest].options[i].value);
          else if (g_strcmp0 (priv->dests[priv->current_dest].options[i].name, "printer-state-reasons") == 0)
            reason = priv->dests[priv->current_dest].options[i].value;
          else if (g_strcmp0 (priv->dests[priv->current_dest].options[i].name, "marker-types") == 0)
            marker_types = priv->dests[priv->current_dest].options[i].value;
          else if (g_strcmp0 (priv->dests[priv->current_dest].options[i].name, "printer-make-and-model") == 0)
            printer_make_and_model = priv->dests[priv->current_dest].options[i].value;
          else if (g_strcmp0 (priv->dests[priv->current_dest].options[i].name, "printer-uri-supported") == 0)
            printer_uri = priv->dests[priv->current_dest].options[i].value;
          else if (g_strcmp0 (priv->dests[priv->current_dest].options[i].name, "printer-type") == 0)
            printer_type = priv->dests[priv->current_dest].options[i].value;
          else if (g_strcmp0 (priv->dests[priv->current_dest].options[i].name, "device-uri") == 0)
            device_uri = priv->dests[priv->current_dest].options[i].value;
          else if (g_strcmp0 (priv->dests[priv->current_dest].options[i].name, "printer-is-accepting-jobs") == 0)
            {
              if (g_strcmp0 (priv->dests[priv->current_dest].options[i].value, "true") == 0)
                is_accepting_jobs = TRUE;
              else
                is_accepting_jobs = FALSE;
            }
        }

      if (priv->ppd_file_names[priv->current_dest] == NULL)
        priv->ppd_file_names[priv->current_dest] =
          g_strdup (cupsGetPPD (priv->dests[priv->current_dest].name));

      if (priv->dest_model_names[priv->current_dest] == NULL)
        priv->dest_model_names[priv->current_dest] =
          get_ppd_attribute (priv->ppd_file_names[priv->current_dest],
                             "ModelName");

      printer_model = g_strdup (priv->dest_model_names[priv->current_dest]);

      if (printer_model == NULL && printer_make_and_model)
        {
          gchar *breakpoint = NULL, *tmp = NULL, *tmp2 = NULL;
          gchar  backup;
          size_t length = 0;
          gchar *forbiden[] = {
              "foomatic",
              ",",
              "hpijs",
              "hpcups",
              "(recommended)",
              "postscript (recommended)",
              NULL };

          tmp = g_ascii_strdown (printer_make_and_model, -1);

          for (i = 0; i < g_strv_length (forbiden); i++)
            {
              tmp2 = g_strrstr (tmp, forbiden[i]);
              if (breakpoint == NULL || 
                  (tmp2 != NULL && tmp2 < breakpoint))
                breakpoint = tmp2;
            }

          if (breakpoint)
            {
              backup = *breakpoint;
              *breakpoint = '\0';
              length = strlen (tmp);
              *breakpoint = backup;
              g_free (tmp);

              if (length > 0)
                printer_model = g_strndup (printer_make_and_model, length);
            }
          else
            printer_model = g_strdup (printer_make_and_model);
        }

      if (priv->new_printer_name &&
          g_strcmp0 (priv->new_printer_name, printer_name) == 0)
        {
          /* Translators: Printer's state (printer is being configured right now) */
          status = g_strdup ( C_("printer state", "Configuring"));
        }

      /* Find the first of the most severe reasons
       * and show it in the status field
       */
      if (!status &&
          reason &&
          !g_str_equal (reason, "none"))
        {
          int errors = 0, warnings = 0, reports = 0;
          int error_index = -1, warning_index = -1, report_index = -1;

          printer_reasons = g_strsplit (reason, ",", -1);
          for (i = 0; i < g_strv_length (printer_reasons); i++)
            {
              for (j = 0; j < G_N_ELEMENTS (reasons); j++)
                if (strncmp (printer_reasons[i],
                             reasons[j],
                             strlen (reasons[j])) == 0)
                    {
                      if (g_str_has_suffix (printer_reasons[i], "-report"))
                        {
                          if (reports == 0)
                            report_index = j;
                          reports++;
                        }
                      else if (g_str_has_suffix (printer_reasons[i], "-warning"))
                        {
                          if (warnings == 0)
                            warning_index = j;
                          warnings++;
                        }
                      else
                        {
                          if (errors == 0)
                            error_index = j;
                          errors++;
                        }
                    }
            }
          g_strfreev (printer_reasons);

          if (error_index >= 0)
            status = g_strdup (_(statuses[error_index]));
          else if (warning_index >= 0)
            status = g_strdup (_(statuses[warning_index]));
          else if (report_index >= 0)
            status = g_strdup (_(statuses[report_index]));
        }

      if (status == NULL)
        {
          switch (printer_state)
            {
              case 3:
                if (is_accepting_jobs)
                  {
                    /* Translators: Printer's state (can start new job without waiting) */
                    status = g_strdup ( C_("printer state", "Ready"));
                  }
                else
                  {
                    /* Translators: Printer's state (printer is ready but doesn't accept new jobs) */
                    status = g_strdup ( C_("printer state", "Does not accept jobs"));
                  }
                break;
              case 4:
                /* Translators: Printer's state (jobs are processing) */
                status = g_strdup ( C_("printer state", "Processing"));
                break;
              case 5:
                /* Translators: Printer's state (no jobs can be processed) */
                status = g_strdup ( C_("printer state", "Stopped"));
                break;
            }
        }

      widget = (GtkWidget*)
        gtk_builder_get_object (priv->builder, "printer-icon");
      g_value_init (&value, G_TYPE_INT);
      g_object_get_property ((GObject *) widget, "icon-size", &value);

      if (printer_icon)
        {
          gtk_image_set_from_icon_name ((GtkImage *) widget, printer_icon, g_value_get_int (&value));
          g_free (printer_icon);
        }
      else
        gtk_image_set_from_icon_name ((GtkImage *) widget, "printer", g_value_get_int (&value));

      widget = (GtkWidget*)
        gtk_builder_get_object (priv->builder, "printer-name-label");

      if (printer_name)
        {
          cc_editable_entry_set_text (CC_EDITABLE_ENTRY (widget), printer_name);
          g_free (printer_name);
        }
      else
        cc_editable_entry_set_text (CC_EDITABLE_ENTRY (widget), EMPTY_TEXT);


      widget = (GtkWidget*)
        gtk_builder_get_object (priv->builder, "printer-status-label");

      if (status)
        {
          cc_editable_entry_set_text (CC_EDITABLE_ENTRY (widget), status);
          g_free (status);
        }
      else
        cc_editable_entry_set_text (CC_EDITABLE_ENTRY (widget), EMPTY_TEXT);


      widget = (GtkWidget*)
        gtk_builder_get_object (priv->builder, "printer-location-label");

      if (location)
        {
          cc_editable_entry_set_text (CC_EDITABLE_ENTRY (widget), location);
          g_free (location);
        }
      else
        cc_editable_entry_set_text (CC_EDITABLE_ENTRY (widget), EMPTY_TEXT);


      model_button_label = GTK_WIDGET (gtk_builder_get_object (priv->builder, "printer-model-button-label"));

      model_label = (GtkWidget*)
        gtk_builder_get_object (priv->builder, "printer-model-label");

      if (printer_model)
        {
          gtk_label_set_text (GTK_LABEL (model_button_label), printer_model);
          gtk_label_set_text (GTK_LABEL (model_label), printer_model);
          g_free (printer_model);
        }
      else
        {
          gtk_label_set_text (GTK_LABEL (model_button_label), EMPTY_TEXT);
          gtk_label_set_text (GTK_LABEL (model_label), EMPTY_TEXT);
        }


      widget = (GtkWidget*)
        gtk_builder_get_object (priv->builder, "printer-ip-address-label");

      if (printer_type)
        type = atoi (printer_type);

      printer_hostname = printer_get_hostname (type, device_uri, printer_uri);

      if (printer_hostname)
        {
          cc_editable_entry_set_text (CC_EDITABLE_ENTRY (widget), printer_hostname);
          g_free (printer_hostname);
        }
      else
        cc_editable_entry_set_text (CC_EDITABLE_ENTRY (widget), EMPTY_TEXT);


      widget = (GtkWidget*)
        gtk_builder_get_object (priv->builder, "printer-disable-switch");

      g_signal_handlers_block_by_func (G_OBJECT (widget), printer_disable_cb, self);
      gtk_switch_set_active (GTK_SWITCH (widget), printer_state != 5 && is_accepting_jobs);
      g_signal_handlers_unblock_by_func (G_OBJECT (widget), printer_disable_cb, self);


      widget = (GtkWidget*)
        gtk_builder_get_object (priv->builder, "printer-default-check-button");

      g_signal_handlers_block_by_func (G_OBJECT (widget), printer_set_default_cb, self);
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), priv->dests[id].is_default);
      g_signal_handlers_unblock_by_func (G_OBJECT (widget), printer_set_default_cb, self);


      widget = (GtkWidget*)
        gtk_builder_get_object (priv->builder, "supply-drawing-area");
      gtk_widget_set_size_request (widget, -1, SUPPLY_BAR_HEIGHT);
      gtk_widget_queue_draw (widget);


      widget = (GtkWidget*)
        gtk_builder_get_object (priv->builder, "supply-label");

      if (marker_types && g_strrstr (marker_types, "toner") != NULL)
        /* Translators: Toner supply */
        supply_type = g_strdup ( _("Toner Level"));
      else if (marker_types && g_strrstr (marker_types, "ink") != NULL)
        /* Translators: Ink supply */
        supply_type = g_strdup ( _("Ink Level"));
      else
        /* Translators: By supply we mean ink, toner, staples, water, ... */
        supply_type = g_strdup ( _("Supply Level"));

      if (supply_type)
        {
          gtk_label_set_text (GTK_LABEL (widget), supply_type);
          g_free (supply_type);
        }
      else
        gtk_label_set_text (GTK_LABEL (widget), EMPTY_TEXT);
    }
  else
    {
      if (id == -1)
        {
          if (priv->new_printer_name &&
              g_strcmp0 (priv->new_printer_name, printer_name) == 0)
            {
              /* Translators: Printer's state (printer is being installed right now) */
              status = g_strdup ( C_("printer state", "Installing"));
              location = g_strdup (priv->new_printer_location);
              printer_model = g_strdup (priv->new_printer_make_and_model);

              widget = (GtkWidget*)
                gtk_builder_get_object (priv->builder, "main-vbox");
              gtk_stack_set_visible_child_name (GTK_STACK (widget), "printers-list");
            }
        }

      widget = (GtkWidget*)
        gtk_builder_get_object (priv->builder, "printer-icon");
      g_value_init (&value, G_TYPE_INT);
      g_object_get_property ((GObject *) widget, "icon-size", &value);

      if (printer_icon)
        {
          gtk_image_set_from_icon_name ((GtkImage *) widget, printer_icon, g_value_get_int (&value));
          g_free (printer_icon);
        }
      else
        gtk_image_set_from_icon_name ((GtkImage *) widget, "printer", g_value_get_int (&value));

      widget = (GtkWidget*)
        gtk_builder_get_object (priv->builder, "printer-name-label");
      if (printer_name)
        {
          cc_editable_entry_set_text (CC_EDITABLE_ENTRY (widget), printer_name);
          g_free (printer_name);
        }
      else
        cc_editable_entry_set_text (CC_EDITABLE_ENTRY (widget), EMPTY_TEXT);

      widget = (GtkWidget*)
        gtk_builder_get_object (priv->builder, "printer-status-label");
      if (status)
        {
          cc_editable_entry_set_text (CC_EDITABLE_ENTRY (widget), status);
          g_free (status);
        }
      else
        cc_editable_entry_set_text (CC_EDITABLE_ENTRY (widget), EMPTY_TEXT);

      widget = (GtkWidget*)
        gtk_builder_get_object (priv->builder, "printer-location-label");

      if (location)
        {
          cc_editable_entry_set_text (CC_EDITABLE_ENTRY (widget), location);
          g_free (location);
        }
      else
        cc_editable_entry_set_text (CC_EDITABLE_ENTRY (widget), EMPTY_TEXT);


      model_button_label = GTK_WIDGET (gtk_builder_get_object (priv->builder, "printer-model-button-label"));

      model_label = (GtkWidget*)
        gtk_builder_get_object (priv->builder, "printer-model-label");

      if (printer_model)
        {
          gtk_label_set_text (GTK_LABEL (model_button_label), printer_model);
          gtk_label_set_text (GTK_LABEL (model_label), printer_model);
          g_free (printer_model);
        }
      else
        {
          gtk_label_set_text (GTK_LABEL (model_button_label), EMPTY_TEXT);
          gtk_label_set_text (GTK_LABEL (model_label), EMPTY_TEXT);
        }

      widget = (GtkWidget*)
        gtk_builder_get_object (priv->builder, "printer-ip-address-label");
      cc_editable_entry_set_text (CC_EDITABLE_ENTRY (widget), EMPTY_TEXT);

      widget = (GtkWidget*)
        gtk_builder_get_object (priv->builder, "printer-jobs-label");
      cc_editable_entry_set_text (CC_EDITABLE_ENTRY (widget), EMPTY_TEXT);

      widget = (GtkWidget*)
        gtk_builder_get_object (priv->builder, "printer-disable-switch");

      g_signal_handlers_block_by_func (G_OBJECT (widget), printer_disable_cb, self);
      gtk_switch_set_active (GTK_SWITCH (widget), FALSE);
      g_signal_handlers_unblock_by_func (G_OBJECT (widget), printer_disable_cb, self);

      widget = (GtkWidget*)
        gtk_builder_get_object (priv->builder, "printer-default-check-button");

      g_signal_handlers_block_by_func (G_OBJECT (widget), printer_set_default_cb, self);
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), FALSE);
      g_signal_handlers_unblock_by_func (G_OBJECT (widget), printer_set_default_cb, self);
    }

  update_sensitivity (self);
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
  GtkTreeSelection       *selection;
  CcPrintersPanel        *self = (CcPrintersPanel*) user_data;
  GtkListStore           *store;
  cups_ptype_t            printer_type = 0;
  GtkTreeModel           *model;
  GtkTreeIter             selected_iter;
  GtkTreeView            *treeview;
  GtkTreeIter             iter;
  cups_job_t             *jobs = NULL;
  GtkWidget              *widget;
  gboolean                paused = FALSE;
  gboolean                selected_iter_set = FALSE;
  gboolean                valid = FALSE;
  PpCups                 *cups = PP_CUPS (source_object);
  PpCupsDests            *cups_dests;
  GError                 *error = NULL;
  gchar                  *current_printer_name = NULL;
  gchar                  *printer_icon_name = NULL;
  gchar                  *default_icon_name = NULL;
  gchar                  *device_uri = NULL;
  gint                    new_printer_position = 0;
  int                     current_dest = -1;
  int                     i, j;
  int                     num_jobs = 0;

  cups_dests = pp_cups_get_dests_finish (cups, result, &error);

  if (cups_dests == NULL && error != NULL)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        {
          g_warning ("Could not get dests: %s", error->message);
        }

      g_error_free (error);
      return;
    }

  priv = PRINTERS_PANEL_PRIVATE (self);

  treeview = (GtkTreeView*)
    gtk_builder_get_object (priv->builder, "printers-treeview");

  if ((selection = gtk_tree_view_get_selection (treeview)) != NULL &&
      gtk_tree_selection_get_selected (selection, &model, &iter))
    {
      gtk_tree_model_get (model, &iter,
			  PRINTER_NAME_COLUMN, &current_printer_name,
			  -1);
    }

  if (priv->renamed_printer_name != NULL)
    {
      g_free (current_printer_name);
      current_printer_name = priv->renamed_printer_name;
      priv->renamed_printer_name = NULL;
    }

  if (priv->new_printer_name &&
      priv->select_new_printer)
    {
      g_free (current_printer_name);
      current_printer_name = g_strdup (priv->new_printer_name);
      priv->select_new_printer = FALSE;
    }

  free_dests (self);

  priv->dests = cups_dests->dests;
  priv->num_dests = cups_dests->num_of_dests;
  g_free (cups_dests);

  priv->dest_model_names = g_new0 (gchar *, priv->num_dests);
  priv->ppd_file_names = g_new0 (gchar *, priv->num_dests);

  store = gtk_list_store_new (PRINTER_N_COLUMNS,
                              G_TYPE_INT,
                              G_TYPE_STRING,
                              G_TYPE_BOOLEAN,
                              G_TYPE_STRING,
                              G_TYPE_STRING);

  if (priv->num_dests == 0 && !priv->new_printer_name)
    {
      widget = (GtkWidget*)
        gtk_builder_get_object (priv->builder, "main-vbox");

      pp_cups_connection_test_async (g_object_ref (cups), set_current_page, widget);

      gtk_widget_set_sensitive (GTK_WIDGET (treeview), FALSE);
    }
  else
    gtk_widget_set_sensitive (GTK_WIDGET (treeview), TRUE);

  g_object_unref (cups);

  for (i = 0; i < priv->num_dests; i++)
    {
      gchar *instance;

      if (priv->new_printer_name && new_printer_position >= 0)
        {
          gint comparison_result = g_ascii_strcasecmp (priv->dests[i].name, priv->new_printer_name);

          if (comparison_result < 0)
            new_printer_position = i + 1;
          else if (comparison_result == 0)
            new_printer_position = -1;
        }

      gtk_list_store_append (store, &iter);

      if (priv->dests[i].instance)
        {
          instance = g_strdup_printf ("%s / %s", priv->dests[i].name, priv->dests[i].instance);
        }
      else
        {
          instance = g_strdup (priv->dests[i].name);
        }

      for (j = 0; j < priv->dests[i].num_options; j++)
        {
          if (g_strcmp0 (priv->dests[i].options[j].name, "printer-state") == 0)
            paused = (g_strcmp0 (priv->dests[i].options[j].value, "5") == 0);
          else if (g_strcmp0 (priv->dests[i].options[j].name, "device-uri") == 0)
            device_uri = priv->dests[i].options[j].value;
          else if (g_strcmp0 (priv->dests[i].options[j].name, "printer-type") == 0)
            printer_type = atoi (priv->dests[i].options[j].value);
        }

      if (priv->dests[i].is_default)
        default_icon_name = g_strdup ("object-select-symbolic");
      else
        default_icon_name = NULL;

      if (printer_is_local (printer_type, device_uri))
        printer_icon_name = g_strdup ("printer");
      else
        printer_icon_name = g_strdup ("printer-network");

      gtk_list_store_set (store, &iter,
                          PRINTER_ID_COLUMN, i,
                          PRINTER_NAME_COLUMN, instance,
                          PRINTER_PAUSED_COLUMN, paused,
                          PRINTER_DEFAULT_ICON_COLUMN, default_icon_name,
                          PRINTER_ICON_COLUMN, printer_icon_name,
                          -1);

      if (g_strcmp0 (current_printer_name, instance) == 0)
        {
          current_dest = i;
          selected_iter = iter;
          selected_iter_set = TRUE;
        }

      g_free (instance);
      g_free (printer_icon_name);
      g_free (default_icon_name);
    }

  if (priv->new_printer_name && new_printer_position >= 0)
    {
      gtk_list_store_insert (store, &iter, new_printer_position);
      gtk_list_store_set (store, &iter,
                          PRINTER_ID_COLUMN, -1,
                          PRINTER_NAME_COLUMN, priv->new_printer_name,
                          PRINTER_PAUSED_COLUMN, TRUE,
                          PRINTER_DEFAULT_ICON_COLUMN, NULL,
                          PRINTER_ICON_COLUMN, priv->new_printer_on_network ?
                            "printer-network" : "printer",
                          -1);

      if (g_strcmp0 (current_printer_name, priv->new_printer_name) == 0)
        {
          selected_iter = iter;
          selected_iter_set = TRUE;
        }
    }

  g_signal_handlers_block_by_func (
    G_OBJECT (gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview))),
    printer_selection_changed_cb,
    self);

  gtk_tree_view_set_model (treeview, GTK_TREE_MODEL (store));

  g_signal_handlers_unblock_by_func (
    G_OBJECT (gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview))),
    printer_selection_changed_cb,
    self);

  if (selected_iter_set)
    {
      priv->current_dest = current_dest;
      gtk_tree_selection_select_iter (
        gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview)),
        &selected_iter);
    }
  else
    {
      num_jobs = cupsGetJobs (&jobs, NULL, 1, CUPS_WHICHJOBS_ALL);

      /* Select last used printer */
      if (num_jobs > 0)
        {
          for (i = 0; i < priv->num_dests; i++)
            if (g_strcmp0 (priv->dests[i].name, jobs[num_jobs - 1].dest) == 0)
              {
                priv->current_dest = i;
                break;
              }
          cupsFreeJobs (num_jobs, jobs);
        }

      /* Select default printer */
      if (priv->current_dest < 0)
        {
          for (i = 0; i < priv->num_dests; i++)
            if (priv->dests[i].is_default)
              {
                priv->current_dest = i;
                break;
              }
        }

      if (priv->current_dest >= 0)
        {
          gint id;
          valid = gtk_tree_model_get_iter_first ((GtkTreeModel *) store,
                                                 &selected_iter);

          while (valid)
            {
              gtk_tree_model_get ((GtkTreeModel *) store, &selected_iter,
                                  PRINTER_ID_COLUMN, &id,
                                  -1);
              if (id == priv->current_dest)
                break;

              valid = gtk_tree_model_iter_next ((GtkTreeModel *) store,
                                                &selected_iter);
            }

          gtk_tree_selection_select_iter (
            gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview)),
            &selected_iter);
        }
      else if (priv->num_dests > 0)
        {
          /* Select first printer */
          gtk_tree_model_get_iter_first ((GtkTreeModel *) store,
                                         &selected_iter);

          gtk_tree_selection_select_iter (
            gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview)),
            &selected_iter);
        }
    }

  g_free (current_printer_name);
  g_object_unref (store);

  update_sensitivity (self);
}

static void
actualize_printers_list (CcPrintersPanel *self)
{
  CcPrintersPanelPrivate *priv;
  PpCups                 *cups;

  priv = PRINTERS_PANEL_PRIVATE (self);

  cups = pp_cups_new ();
  pp_cups_get_dests_async (cups,
                           priv->actualize_printers_list_cancellable,
                           actualize_printers_list_cb,
                           self);
}

static void
set_cell_sensitivity_func (GtkTreeViewColumn *tree_column,
                           GtkCellRenderer   *cell,
                           GtkTreeModel      *tree_model,
                           GtkTreeIter       *iter,
                           gpointer           func_data)
{
  CcPrintersPanelPrivate *priv;
  CcPrintersPanel        *self = (CcPrintersPanel*) func_data;
  gboolean                paused = FALSE;

  priv = PRINTERS_PANEL_PRIVATE (self);

  gtk_tree_model_get (tree_model, iter, PRINTER_PAUSED_COLUMN, &paused, -1);

  if (priv->num_dests == 0)
    g_object_set (G_OBJECT (cell),
                  "ellipsize", PANGO_ELLIPSIZE_NONE,
                  "width-chars", -1,
                  NULL);
  else
    g_object_set (G_OBJECT (cell),
                  "ellipsize", PANGO_ELLIPSIZE_END,
                  "width-chars", 18,
                  NULL);

  g_object_set (cell, "sensitive", !paused, NULL);
}

static void
set_pixbuf_cell_sensitivity_func (GtkTreeViewColumn *tree_column,
                                  GtkCellRenderer   *cell,
                                  GtkTreeModel      *tree_model,
                                  GtkTreeIter       *iter,
                                  gpointer           func_data)
{
  gboolean paused = FALSE;

  gtk_tree_model_get (tree_model, iter, PRINTER_PAUSED_COLUMN, &paused, -1);
  g_object_set (cell, "sensitive", !paused, NULL);
}

static void
populate_printers_list (CcPrintersPanel *self)
{
  CcPrintersPanelPrivate *priv;
  GtkTreeViewColumn      *column;
  GtkCellRenderer        *icon_renderer;
  GtkCellRenderer        *icon_renderer2;
  GtkCellRenderer        *renderer;
  GtkWidget              *treeview;
  int                     icon_width;

  priv = PRINTERS_PANEL_PRIVATE (self);

  treeview = (GtkWidget*)
    gtk_builder_get_object (priv->builder, "printers-treeview");

  g_signal_connect (gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview)),
                    "changed", G_CALLBACK (printer_selection_changed_cb), self);

  actualize_printers_list (self);


  icon_renderer = gtk_cell_renderer_pixbuf_new ();
  g_object_set (icon_renderer, "stock-size", GTK_ICON_SIZE_LARGE_TOOLBAR, NULL);
  gtk_cell_renderer_set_padding (icon_renderer, 4, 4);
  column = gtk_tree_view_column_new_with_attributes ("Icon", icon_renderer,
                                                     "icon-name", PRINTER_ICON_COLUMN, NULL);
  gtk_tree_view_column_set_cell_data_func (column, icon_renderer, set_pixbuf_cell_sensitivity_func,
                                           self, NULL);
  gtk_tree_view_column_set_expand (column, FALSE);
  gtk_tree_view_append_column (GTK_TREE_VIEW (treeview), column);


  renderer = gtk_cell_renderer_text_new ();
  g_object_set (G_OBJECT (renderer),
                "ellipsize", PANGO_ELLIPSIZE_MIDDLE,
                "max-width-chars", 18, NULL);
  column = gtk_tree_view_column_new_with_attributes ("Printer", renderer,
                                                     "text", PRINTER_NAME_COLUMN, NULL);
  gtk_tree_view_column_set_cell_data_func (column, renderer, set_cell_sensitivity_func,
                                           self, NULL);
  gtk_tree_view_column_set_expand (column, FALSE);
  gtk_tree_view_column_set_min_width (column, 120);
  gtk_tree_view_append_column (GTK_TREE_VIEW (treeview), column);


  icon_renderer2 = gtk_cell_renderer_pixbuf_new ();
  g_object_set (G_OBJECT (icon_renderer2), "follow-state", TRUE, NULL);
  column = gtk_tree_view_column_new_with_attributes ("Default", icon_renderer2,
                                                     "icon-name", PRINTER_DEFAULT_ICON_COLUMN, NULL);
  gtk_icon_size_lookup (GTK_ICON_SIZE_MENU, &icon_width, NULL);
  gtk_cell_renderer_set_fixed_size (icon_renderer2, icon_width, -1);
  gtk_tree_view_column_set_expand (column, FALSE);
  gtk_tree_view_append_column (GTK_TREE_VIEW (treeview), column);
}

enum
{
  JOB_ID_COLUMN,
  JOB_TITLE_COLUMN,
  JOB_STATE_COLUMN,
  JOB_CREATION_TIME_COLUMN,
  JOB_N_COLUMNS
};

static void
update_jobs_count (CcPrintersPanel *self)
{
  CcPrintersPanelPrivate *priv;
  cups_job_t             *jobs;
  GtkWidget              *widget;
  gchar                  *active_jobs = NULL;
  gint                    num_jobs;

  priv = PRINTERS_PANEL_PRIVATE (self);

  priv->num_jobs = -1;

  if (priv->current_dest >= 0 &&
      priv->current_dest < priv->num_dests &&
      priv->dests != NULL)
    {
      priv->num_jobs = cupsGetJobs (&jobs, priv->dests[priv->current_dest].name, 1, CUPS_WHICHJOBS_ACTIVE);
      if (priv->num_jobs > 0)
        cupsFreeJobs (priv->num_jobs, jobs);

      num_jobs = priv->num_jobs < 0 ? 0 : (guint) priv->num_jobs;
      /* Translators: there is n active print jobs on this printer */
      active_jobs = g_strdup_printf (ngettext ("%u active", "%u active", num_jobs), num_jobs);
    }

  widget = (GtkWidget*)
    gtk_builder_get_object (priv->builder, "printer-jobs-label");

  if (active_jobs)
    {
      cc_editable_entry_set_text (CC_EDITABLE_ENTRY (widget), active_jobs);
      g_free (active_jobs);
    }
  else
    cc_editable_entry_set_text (CC_EDITABLE_ENTRY (widget), EMPTY_TEXT);

  if (priv->pp_jobs_dialog)
    {
      pp_jobs_dialog_update (priv->pp_jobs_dialog);
    }
}

static void
printer_disable_cb (GObject    *gobject,
                    GParamSpec *pspec,
                    gpointer    user_data)
{
  CcPrintersPanelPrivate *priv;
  CcPrintersPanel        *self = (CcPrintersPanel*) user_data;
  gboolean                paused = FALSE;
  gboolean                is_accepting_jobs = TRUE;
  char                   *name = NULL;
  int                     i;

  priv = PRINTERS_PANEL_PRIVATE (self);

  if (priv->current_dest >= 0 &&
      priv->current_dest < priv->num_dests &&
      priv->dests != NULL)
    {
      name = priv->dests[priv->current_dest].name;

      for (i = 0; i < priv->dests[priv->current_dest].num_options; i++)
        {
          if (g_strcmp0 (priv->dests[priv->current_dest].options[i].name, "printer-state") == 0)
            paused = (g_strcmp0 (priv->dests[priv->current_dest].options[i].value, "5") == 0);
          else if (g_strcmp0 (priv->dests[priv->current_dest].options[i].name, "printer-is-accepting-jobs") == 0)
            {
              if (g_strcmp0 (priv->dests[priv->current_dest].options[i].value, "true") == 0)
                is_accepting_jobs = TRUE;
              else
                is_accepting_jobs = FALSE;
            }
        }
    }

  if (name)
    {
      if (!paused && is_accepting_jobs)
        {
          printer_set_enabled (name, FALSE);
          printer_set_accepting_jobs (name, FALSE, NULL);
        }
      else
        {
          if (paused)
            printer_set_enabled (name, TRUE);

          if (!is_accepting_jobs)
            printer_set_accepting_jobs (name, TRUE, NULL);
        }

      actualize_printers_list (self);
    }
}

typedef struct {
  gchar *color;
  gchar *type;
  gchar *name;
  gint   level;
} MarkerItem;

static gint
markers_cmp (gconstpointer a,
             gconstpointer b)
{
  MarkerItem *x = (MarkerItem*) a;
  MarkerItem *y = (MarkerItem*) b;

  if (x->level < y->level)
    return 1;
  else if (x->level == y->level)
    return 0;
  else
    return -1;
}

static void
rounded_rectangle (cairo_t *cr, double x, double y, double w, double h, double r)
{
    cairo_new_sub_path (cr);
    cairo_arc (cr, x + r, y + r, r, M_PI, 3 * M_PI / 2);
    cairo_arc (cr, x + w - r, y + r, r, 3 *M_PI / 2, 2 * M_PI);
    cairo_arc (cr, x + w - r, y + h - r, r, 0, M_PI / 2);
    cairo_arc (cr, x + r, y + h - r, r, M_PI / 2, M_PI);
    cairo_close_path (cr);
}

static gboolean
supply_levels_draw_cb (GtkWidget *widget,
                       cairo_t *cr,
                       gpointer user_data)
{
  CcPrintersPanelPrivate *priv;
  CcPrintersPanel        *self = (CcPrintersPanel*) user_data;
  GtkStyleContext        *context;
  gchar                  *marker_levels = NULL;
  gchar                  *marker_colors = NULL;
  gchar                  *marker_names = NULL;
  gchar                  *marker_types = NULL;
  gchar                  *tooltip_text = NULL;
  gint                    width;
  gint                    height;
  int                     i;

  priv = PRINTERS_PANEL_PRIVATE (self);

  context = gtk_widget_get_style_context (widget);

  width = gtk_widget_get_allocated_width (widget);
  height = gtk_widget_get_allocated_height (widget);

  gtk_render_background (context, cr, 0, 0, width, height);

  if (priv->current_dest >= 0 &&
      priv->current_dest < priv->num_dests &&
      priv->dests != NULL)
    {
      for (i = 0; i < priv->dests[priv->current_dest].num_options; i++)
        {
          if (g_strcmp0 (priv->dests[priv->current_dest].options[i].name, "marker-names") == 0)
            marker_names = g_strcompress (priv->dests[priv->current_dest].options[i].value);
          else if (g_strcmp0 (priv->dests[priv->current_dest].options[i].name, "marker-levels") == 0)
            marker_levels = priv->dests[priv->current_dest].options[i].value;
          else if (g_strcmp0 (priv->dests[priv->current_dest].options[i].name, "marker-colors") == 0)
            marker_colors = priv->dests[priv->current_dest].options[i].value;
          else if (g_strcmp0 (priv->dests[priv->current_dest].options[i].name, "marker-types") == 0)
            marker_types = priv->dests[priv->current_dest].options[i].value;
        }

      if (marker_levels && marker_colors && marker_names && marker_types)
        {
          GSList   *markers = NULL;
          GSList   *tmp_list = NULL;
          GValue    int_val = G_VALUE_INIT;
          gchar   **marker_levelsv = NULL;
          gchar   **marker_colorsv = NULL;
          gchar   **marker_namesv = NULL;
          gchar   **marker_typesv = NULL;
          gchar    *tmp = NULL;
          gint      border_radius = 0;

          gtk_style_context_save (context);
          gtk_style_context_add_class (context, GTK_STYLE_CLASS_BUTTON);

          gtk_style_context_get_property (
            context, GTK_STYLE_PROPERTY_BORDER_RADIUS, 0, &int_val);
          if (G_VALUE_HOLDS_INT (&int_val))
            border_radius = g_value_get_int (&int_val);

          marker_levelsv = g_strsplit (marker_levels, ",", -1);
          marker_colorsv = g_strsplit (marker_colors, ",", -1);
          marker_namesv = g_strsplit (marker_names, ",", -1);
          marker_typesv = g_strsplit (marker_types, ",", -1);

          if (g_strv_length (marker_levelsv) == g_strv_length (marker_colorsv) &&
              g_strv_length (marker_colorsv) == g_strv_length (marker_namesv) &&
              g_strv_length (marker_namesv) == g_strv_length (marker_typesv))
            {
              for (i = 0; i < g_strv_length (marker_levelsv); i++)
                {
                  MarkerItem *marker;

                  if (g_strcmp0 (marker_typesv[i], "ink") == 0 ||
                      g_strcmp0 (marker_typesv[i], "toner") == 0 ||
                      g_strcmp0 (marker_typesv[i], "inkCartridge") == 0 ||
                      g_strcmp0 (marker_typesv[i], "tonerCartridge") == 0)
                    {
                      marker = g_new0 (MarkerItem, 1);
                      marker->type = g_strdup (marker_typesv[i]);
                      marker->name = g_strdup (marker_namesv[i]);
                      marker->color = g_strdup (marker_colorsv[i]);
                      marker->level = atoi (marker_levelsv[i]);

                      markers = g_slist_prepend (markers, marker);
                    }
                }

              markers = g_slist_sort (markers, markers_cmp);

              for (tmp_list = markers; tmp_list; tmp_list = tmp_list->next)
                {
                  GdkRGBA color = {0.0, 0.0, 0.0, 1.0};
                  double  display_value;
                  int     value;

                  value = ((MarkerItem*) tmp_list->data)->level;

                  gdk_rgba_parse (&color, ((MarkerItem*) tmp_list->data)->color);

                  if (value > 0)
                    {
                      display_value = value / 100.0 * (width - 3.0);
                      gdk_cairo_set_source_rgba (cr, &color);
                      rounded_rectangle (cr, 1.5, 1.5, display_value, SUPPLY_BAR_HEIGHT - 3.0, border_radius);
                      cairo_fill (cr);
                    }

                  if (tooltip_text)
                    {
                      tmp = g_strdup_printf ("%s\n%s",
                                             tooltip_text,
                                             ((MarkerItem*) tmp_list->data)->name);
                      g_free (tooltip_text);
                      tooltip_text = tmp;
                      tmp = NULL;
                    }
                  else
                    tooltip_text = g_strdup_printf ("%s",
                                                    ((MarkerItem*) tmp_list->data)->name);
                }

              gtk_render_frame (context, cr, 1, 1, width - 2, SUPPLY_BAR_HEIGHT - 2);

              for (tmp_list = markers; tmp_list; tmp_list = tmp_list->next)
                {
                  g_free (((MarkerItem*) tmp_list->data)->name);
                  g_free (((MarkerItem*) tmp_list->data)->type);
                  g_free (((MarkerItem*) tmp_list->data)->color);
                }
              g_slist_free_full (markers, g_free);
            }

          gtk_style_context_restore (context);

          g_strfreev (marker_levelsv);
          g_strfreev (marker_colorsv);
          g_strfreev (marker_namesv);
          g_strfreev (marker_typesv);
        }

      g_free (marker_names);

      if (tooltip_text)
        {
          gtk_widget_set_tooltip_text (widget, tooltip_text);
          g_free (tooltip_text);
        }
      else
        {
          gtk_widget_set_tooltip_text (widget, NULL);
          gtk_widget_set_has_tooltip (widget, FALSE);
        }
    }

  return TRUE;
}

static void
printer_set_default_cb (GtkToggleButton *button,
                        gpointer         user_data)
{
  CcPrintersPanelPrivate *priv;
  CcPrintersPanel        *self = (CcPrintersPanel*) user_data;
  char                   *name = NULL;

  priv = PRINTERS_PANEL_PRIVATE (self);

  if (priv->current_dest >= 0 &&
      priv->current_dest < priv->num_dests &&
      priv->dests != NULL)
    name = priv->dests[priv->current_dest].name;

  if (name)
    {
      printer_set_default (name);
      actualize_printers_list (self);

      g_signal_handlers_block_by_func (G_OBJECT (button), printer_set_default_cb, self);
      gtk_toggle_button_set_active (button, priv->dests[priv->current_dest].is_default);
      g_signal_handlers_unblock_by_func (G_OBJECT (button), printer_set_default_cb, self);
  }
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
printer_remove_cb (GtkToolButton *toolbutton,
                   gpointer       user_data)
{
  CcPrintersPanelPrivate *priv;
  CcPrintersPanel        *self = (CcPrintersPanel*) user_data;
  char                   *printer_name = NULL;

  priv = PRINTERS_PANEL_PRIVATE (self);

  if (priv->current_dest >= 0 &&
      priv->current_dest < priv->num_dests &&
      priv->dests != NULL)
    printer_name = priv->dests[priv->current_dest].name;

  if (printer_name && printer_delete (printer_name))
    actualize_printers_list (self);
}

static void
printer_rename_cb (GObject      *source_object,
                   GAsyncResult *res,
                   gpointer      user_data)
{
  CcPrintersPanelPrivate  *priv;
  CcPrintersPanel         *self = (CcPrintersPanel *) user_data;
  gboolean                 result;
  GError                  *error = NULL;
  gchar                   *printer_name = NULL;

  priv = PRINTERS_PANEL_PRIVATE (self);

  result = pp_printer_rename_finish (PP_PRINTER (source_object), res, &error);
  if (result)
    {
      g_object_get (source_object, "printer-name", &printer_name, NULL);
      priv->renamed_printer_name = printer_name;
    }

  g_object_unref (source_object);

  actualize_printers_list (self);
}

static void
printer_name_edit_cb (GtkWidget *entry,
                      gpointer   user_data)
{
  CcPrintersPanelPrivate  *priv;
  CcPrintersPanel         *self = (CcPrintersPanel*) user_data;
  const gchar             *new_name;
  PpPrinter               *printer;

  priv = PRINTERS_PANEL_PRIVATE (self);

  new_name = cc_editable_entry_get_text (CC_EDITABLE_ENTRY (entry));

  if (priv->current_dest >= 0 &&
      priv->current_dest < priv->num_dests &&
      priv->dests != NULL)
    {
      printer = pp_printer_new (priv->dests[priv->current_dest].name);
      pp_printer_rename_async (printer,
                               new_name,
                               NULL,
                               printer_rename_cb,
                               self);
    }
}

static void
printer_location_edit_cb (GtkWidget *entry,
                          gpointer   user_data)
{
  CcPrintersPanelPrivate  *priv;
  CcPrintersPanel         *self = (CcPrintersPanel*) user_data;
  const gchar             *location;
  gchar                   *printer_name = NULL;

  priv = PRINTERS_PANEL_PRIVATE (self);

  location = cc_editable_entry_get_text (CC_EDITABLE_ENTRY (entry));

  if (priv->current_dest >= 0 &&
      priv->current_dest < priv->num_dests &&
      priv->dests != NULL)
    printer_name = priv->dests[priv->current_dest].name;

  if (printer_name && location &&
      printer_set_location (printer_name, location))
    actualize_printers_list (self);
}

static void
set_ppd_cb (gchar    *printer_name,
            gboolean  success,
            gpointer  user_data)
{
  CcPrintersPanelPrivate *priv;
  CcPrintersPanel        *self = (CcPrintersPanel*) user_data;
  GList                  *iter;

  priv = PRINTERS_PANEL_PRIVATE (self);

  for (iter = priv->driver_change_list; iter; iter = iter->next)
    {
      SetPPDItem *item = (SetPPDItem *) iter->data;

      if (g_strcmp0 (item->printer_name, printer_name) == 0)
        {
          priv->driver_change_list = g_list_remove_link (priv->driver_change_list, iter);

          g_object_unref (item->cancellable);
          g_free (item->printer_name);
          g_free (item);
          g_list_free (iter);
          break;
        }
    }

  update_sensitivity (self);

  if (success)
    {
      actualize_printers_list (self);
    }

  g_free (printer_name);
}

static void
select_ppd_manually (GtkMenuItem *menuitem,
                     gpointer     user_data)
{
  CcPrintersPanelPrivate *priv;
  CcPrintersPanel        *self = (CcPrintersPanel*) user_data;
  GtkFileFilter          *filter;
  GtkWidget              *dialog;
  gchar                  *printer_name = NULL;

  priv = PRINTERS_PANEL_PRIVATE (self);

  gtk_menu_shell_cancel (GTK_MENU_SHELL (priv->popup_menu));

  dialog = gtk_file_chooser_dialog_new (_("Select PPD File"),
                                        GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (self))),
                                        GTK_FILE_CHOOSER_ACTION_OPEN,
                                        _("_Cancel"), GTK_RESPONSE_CANCEL,
                                        _("_Open"), GTK_RESPONSE_ACCEPT,
                                        NULL);

  filter = gtk_file_filter_new ();
  gtk_file_filter_set_name (filter,
    _("PostScript Printer Description files (*.ppd, *.PPD, *.ppd.gz, *.PPD.gz, *.PPD.GZ)"));
  gtk_file_filter_add_pattern (filter, "*.ppd");
  gtk_file_filter_add_pattern (filter, "*.PPD");
  gtk_file_filter_add_pattern (filter, "*.ppd.gz");
  gtk_file_filter_add_pattern (filter, "*.PPD.gz");
  gtk_file_filter_add_pattern (filter, "*.PPD.GZ");

  gtk_file_chooser_set_filter (GTK_FILE_CHOOSER (dialog), filter);

  if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT)
    {
      gchar *ppd_filename;

      ppd_filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (dialog));

      if (priv->current_dest >= 0 &&
          priv->current_dest < priv->num_dests &&
          priv->dests != NULL)
        printer_name = priv->dests[priv->current_dest].name;

      if (printer_name && ppd_filename)
        {
          SetPPDItem *item;

          item = g_new0 (SetPPDItem, 1);
          item->printer_name = g_strdup (printer_name);
          item->cancellable = g_cancellable_new ();

          priv->driver_change_list =
            g_list_prepend (priv->driver_change_list, item);
          update_sensitivity (self);
          printer_set_ppd_file_async (printer_name,
                                      ppd_filename,
                                      item->cancellable,
                                      set_ppd_cb,
                                      user_data);
        }

      g_free (ppd_filename);
    }

  gtk_widget_destroy (dialog);
}

static void
ppd_selection_dialog_response_cb (GtkDialog *dialog,
                                  gint       response_id,
                                  gpointer   user_data)
{
  CcPrintersPanelPrivate *priv;
  CcPrintersPanel        *self = (CcPrintersPanel*) user_data;
  gchar                  *printer_name = NULL;

  priv = PRINTERS_PANEL_PRIVATE (self);

  if (response_id == GTK_RESPONSE_OK)
    {
      gchar *ppd_name;

      ppd_name = pp_ppd_selection_dialog_get_ppd_name (priv->pp_ppd_selection_dialog);

      if (priv->current_dest >= 0 &&
          priv->current_dest < priv->num_dests &&
          priv->dests != NULL)
        printer_name = priv->dests[priv->current_dest].name;

      if (printer_name && ppd_name)
        {
          SetPPDItem *item;

          item = g_new0 (SetPPDItem, 1);
          item->printer_name = g_strdup (printer_name);
          item->cancellable = g_cancellable_new ();

          priv->driver_change_list = g_list_prepend (priv->driver_change_list,
                                                     item);
          update_sensitivity (self);
          printer_set_ppd_async (printer_name,
                                 ppd_name,
                                 item->cancellable,
                                 set_ppd_cb,
                                 user_data);
        }

      g_free (ppd_name);
    }

  pp_ppd_selection_dialog_free (priv->pp_ppd_selection_dialog);
  priv->pp_ppd_selection_dialog = NULL;
}

static void
select_ppd_in_dialog (GtkMenuItem *menuitem,
                      gpointer     user_data)
{
  CcPrintersPanelPrivate *priv;
  CcPrintersPanel        *self = (CcPrintersPanel*) user_data;
  GtkWidget              *widget;
  gchar                  *device_id = NULL;
  gchar                  *manufacturer = NULL;

  priv = PRINTERS_PANEL_PRIVATE (self);

  widget = (GtkWidget*)
    gtk_builder_get_object (priv->builder, "main-vbox");

  if (!priv->pp_ppd_selection_dialog)
    {
      if (priv->current_dest >= 0 &&
          priv->current_dest < priv->num_dests)
        {
          device_id =
            get_ppd_attribute (priv->ppd_file_names[priv->current_dest],
                               "1284DeviceID");

          if (device_id)
            {
              manufacturer = get_tag_value (device_id, "mfg");
              if (!manufacturer)
                manufacturer = get_tag_value (device_id, "manufacturer");
            }

          if (manufacturer == NULL)
            {
              manufacturer =
                get_ppd_attribute (priv->ppd_file_names[priv->current_dest],
                                   "Manufacturer");
            }

          if (manufacturer == NULL)
            {
              manufacturer = g_strdup ("Raw");
            }
        }

      priv->pp_ppd_selection_dialog = pp_ppd_selection_dialog_new (
        GTK_WINDOW (gtk_widget_get_toplevel (widget)),
        priv->all_ppds_list,
        manufacturer,
        ppd_selection_dialog_response_cb,
        self);

      g_free (manufacturer);
      g_free (device_id);
    }
}

static void
set_ppd_from_list (GtkMenuItem *menuitem,
                   gpointer     user_data)
{
  CcPrintersPanelPrivate *priv;
  CcPrintersPanel        *self = (CcPrintersPanel*) user_data;
  gchar                  *printer_name = NULL;
  gchar                  *ppd_name;

  priv = PRINTERS_PANEL_PRIVATE (self);

  ppd_name = (gchar *) g_object_get_data (G_OBJECT (menuitem), "ppd-name");

  if (priv->current_dest >= 0 &&
      priv->current_dest < priv->num_dests &&
      priv->dests != NULL)
    printer_name = priv->dests[priv->current_dest].name;

  if (printer_name && ppd_name)
    {
      SetPPDItem *item;

      item = g_new0 (SetPPDItem, 1);
      item->printer_name = g_strdup (printer_name);
      item->cancellable = g_cancellable_new ();

      priv->driver_change_list = g_list_prepend (priv->driver_change_list,
                                                 item);
      update_sensitivity (self);
      printer_set_ppd_async (printer_name,
                             ppd_name,
                             item->cancellable,
                             set_ppd_cb,
                             user_data);
    }
}

static void
ppd_names_free (gpointer user_data)
{
  PPDName **names = (PPDName **) user_data;
  gint      i;

  if (names)
    {
      for (i = 0; names[i]; i++)
        {
          g_free (names[i]->ppd_name);
          g_free (names[i]->ppd_display_name);
          g_free (names[i]);
        }

      g_free (names);
    }
}

static void
get_ppd_names_cb (PPDName     **names,
                  const gchar  *printer_name,
                  gboolean      cancelled,
                  gpointer      user_data)
{
  CcPrintersPanelPrivate  *priv;
  CcPrintersPanel         *self = (CcPrintersPanel*) user_data;
  GtkWidget               *informal = NULL;
  GtkWidget               *placeholders[3];
  GtkWidget               *spinner;
  gpointer                 value = NULL;
  gboolean                 found = FALSE;
  PPDName                **hash_names = NULL;
  GList                   *children, *iter;
  gint                     i;

  priv = PRINTERS_PANEL_PRIVATE (self);

  priv->getting_ppd_names = FALSE;

  for (i = 0; i < 3; i++)
    placeholders[i] = NULL;

  children = gtk_container_get_children (GTK_CONTAINER (priv->popup_menu));
  if (children)
    {
      for (iter = children; iter; iter = iter->next)
        {
          if (g_strcmp0 ((gchar *) g_object_get_data (G_OBJECT (iter->data), "purpose"),
                         "informal") == 0)
              informal = GTK_WIDGET (iter->data);
          else if (g_strcmp0 ((gchar *) g_object_get_data (G_OBJECT (iter->data), "purpose"),
                              "placeholder1") == 0)
              placeholders[0] = GTK_WIDGET (iter->data);
          else if (g_strcmp0 ((gchar *) g_object_get_data (G_OBJECT (iter->data), "purpose"),
                              "placeholder2") == 0)
              placeholders[1] = GTK_WIDGET (iter->data);
          else if (g_strcmp0 ((gchar *) g_object_get_data (G_OBJECT (iter->data), "purpose"),
                              "placeholder3") == 0)
              placeholders[2] = GTK_WIDGET (iter->data);
        }

      g_list_free (children);
    }

  if (!priv->preferred_drivers)
    {
      priv->preferred_drivers = g_hash_table_new_full (g_str_hash, g_str_equal,
                                                       g_free, ppd_names_free);
    }

  if (!cancelled &&
      !g_hash_table_lookup_extended (priv->preferred_drivers,
                                     printer_name, NULL, NULL))
    g_hash_table_insert (priv->preferred_drivers, g_strdup (printer_name), names);

  if (priv->preferred_drivers &&
      g_hash_table_lookup_extended (priv->preferred_drivers,
                                    printer_name, NULL, &value))
    {
      hash_names = (PPDName **) value;
      if (hash_names)
        {
          for (i = 0; hash_names[i]; i++)
            {
              if (placeholders[i])
                {
                  gtk_menu_item_set_label (GTK_MENU_ITEM (placeholders[i]),
                                           hash_names[i]->ppd_display_name);
                  g_object_set_data_full (G_OBJECT (placeholders[i]),
                                          "ppd-name",
                                          g_strdup (hash_names[i]->ppd_name),
                                              g_free);
                  g_signal_connect (placeholders[i],
                                    "activate",
                                    G_CALLBACK (set_ppd_from_list),
                                    self);
                  gtk_widget_set_sensitive (GTK_WIDGET (placeholders[i]), TRUE);
                  gtk_widget_show (placeholders[i]);
                }
            }

          found = TRUE;
        }
      else
        {
          found = FALSE;
        }
    }

  if (informal)
    {
      spinner = g_object_get_data (G_OBJECT (informal), "spinner");
      if (spinner)
        {
          gtk_widget_hide (spinner);
          gtk_spinner_stop (GTK_SPINNER (spinner));
        }

      if (found)
        gtk_widget_hide (informal);
      else
        gtk_label_set_text (GTK_LABEL (g_object_get_data (G_OBJECT (informal), "label")),
                            _("No suitable driver found"));
    }

  gtk_widget_show_all (priv->popup_menu);

  update_sensitivity (self);
}

static void
popup_menu_done (GtkMenuShell *menushell,
                 gpointer      user_data)
{
  CcPrintersPanelPrivate *priv;
  CcPrintersPanel        *self = (CcPrintersPanel*) user_data;

  priv = PRINTERS_PANEL_PRIVATE (self);

  if (priv->get_ppd_name_cancellable)
    {
      g_cancellable_cancel (priv->get_ppd_name_cancellable);
      g_object_unref (priv->get_ppd_name_cancellable);
      priv->get_ppd_name_cancellable = NULL;
    }
}

static void
popup_model_menu_cb (GtkButton *button,
                     gpointer   user_data)
{
  CcPrintersPanelPrivate *priv;
  CcPrintersPanel        *self = (CcPrintersPanel*) user_data;
  GtkWidget              *spinner;
  GtkWidget              *item;
  GtkWidget              *label;
  GtkWidget              *box;

  priv = PRINTERS_PANEL_PRIVATE (self);

  priv->popup_menu = gtk_menu_new ();
  g_signal_connect (priv->popup_menu,
                    "selection-done",
                    G_CALLBACK (popup_menu_done),
                    user_data);

  /*
   * These placeholders are a workaround for a situation
   * when we want to actually append new menu item in a callback.
   * But unfortunately it is not possible to connect to "activate"
   * signal of such menu item (appended after gtk_menu_popup()).
   */
  item = gtk_menu_item_new_with_label ("");
  g_object_set_data_full (G_OBJECT (item), "purpose",
                          g_strdup ("placeholder1"), g_free);
  gtk_menu_shell_append (GTK_MENU_SHELL (priv->popup_menu), item);
  gtk_widget_set_no_show_all (item, TRUE);
  gtk_widget_hide (item);

  item = gtk_menu_item_new_with_label ("");
  g_object_set_data_full (G_OBJECT (item), "purpose",
                          g_strdup ("placeholder2"), g_free);
  gtk_menu_shell_append (GTK_MENU_SHELL (priv->popup_menu), item);
  gtk_widget_set_no_show_all (item, TRUE);
  gtk_widget_hide (item);

  item = gtk_menu_item_new_with_label ("");
  g_object_set_data_full (G_OBJECT (item), "purpose",
                          g_strdup ("placeholder3"), g_free);
  gtk_menu_shell_append (GTK_MENU_SHELL (priv->popup_menu), item);
  gtk_widget_set_no_show_all (item, TRUE);
  gtk_widget_hide (item);

  label = gtk_label_new (_("Searching for preferred drivers…"));
  spinner = gtk_spinner_new ();
  gtk_spinner_start (GTK_SPINNER (spinner));
  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
  gtk_container_add (GTK_CONTAINER (box), spinner);
  gtk_container_add (GTK_CONTAINER (box), label);
  item = gtk_menu_item_new ();
  gtk_container_add (GTK_CONTAINER (item), box);
  gtk_widget_show_all (item);
  g_object_set_data_full (G_OBJECT (item), "purpose",
                          g_strdup ("informal"), g_free);
  g_object_set_data (G_OBJECT (item), "spinner", spinner);
  g_object_set_data (G_OBJECT (item), "label", label);
  gtk_widget_set_sensitive (item, FALSE);
  gtk_menu_shell_append (GTK_MENU_SHELL (priv->popup_menu), item);
  gtk_widget_set_no_show_all (item, TRUE);
  gtk_widget_show (item);

  item = gtk_separator_menu_item_new ();
  gtk_menu_shell_append (GTK_MENU_SHELL (priv->popup_menu), item);

  item = gtk_menu_item_new_with_label (_("Select from database…"));
  g_object_set_data_full (G_OBJECT (item), "purpose",
                          g_strdup ("ppd-select"), g_free);
  g_signal_connect (item, "activate", G_CALLBACK (select_ppd_in_dialog), self);
  gtk_menu_shell_append (GTK_MENU_SHELL (priv->popup_menu), item);

  item = gtk_separator_menu_item_new ();
  gtk_menu_shell_append (GTK_MENU_SHELL (priv->popup_menu), item);

  item = gtk_menu_item_new_with_label (_("Provide PPD File…"));
  g_object_set_data_full (G_OBJECT (item), "purpose",
                          g_strdup ("ppdfile-select"), g_free);
  g_signal_connect (item, "activate", G_CALLBACK (select_ppd_manually), self);
  gtk_menu_shell_append (GTK_MENU_SHELL (priv->popup_menu), item);

  gtk_widget_show_all (priv->popup_menu);

  gtk_menu_popup (GTK_MENU (priv->popup_menu),
                  NULL, NULL, NULL, NULL, 0,
                  gtk_get_current_event_time());

  if (priv->current_dest >= 0 &&
      priv->current_dest < priv->num_dests &&
      priv->dests != NULL)
    {
      if (priv->preferred_drivers &&
          g_hash_table_lookup_extended (priv->preferred_drivers,
                                        priv->dests[priv->current_dest].name,
                                        NULL, NULL))
        {
          get_ppd_names_cb (NULL,
                            priv->dests[priv->current_dest].name,
                            FALSE,
                            user_data);
        }
      else
        {
          priv->get_ppd_name_cancellable = g_cancellable_new ();
          priv->getting_ppd_names = TRUE;
          get_ppd_names_async (priv->dests[priv->current_dest].name,
                               3,
                               priv->get_ppd_name_cancellable,
                               get_ppd_names_cb,
                               user_data);

          update_sensitivity (self);
        }
    }
}

static void
pp_maintenance_command_execute_cb (GObject      *source_object,
                                   GAsyncResult *res,
                                   gpointer      user_data)
{
  PpMaintenanceCommand *command = (PpMaintenanceCommand *) source_object;
  GError               *error = NULL;

  pp_maintenance_command_execute_finish (command, res, &error);

  g_object_unref (command);
}

static gchar *
get_testprint_filename (const gchar *datadir)
{
  const gchar *testprint[] = { "/data/testprint",
                               "/data/testprint.ps",
                               NULL };
  gchar       *filename = NULL;
  gint         i;

  for (i = 0; testprint[i] != NULL; i++)
    {
      filename = g_strconcat (datadir, testprint[i], NULL);
      if (g_access (filename, R_OK) == 0)
        break;

      g_clear_pointer (&filename, g_free);
    }

  return filename;
}

static void
test_page_cb (GtkButton *button,
              gpointer   user_data)
{
  CcPrintersPanelPrivate  *priv;
  CcPrintersPanel         *self = (CcPrintersPanel*) user_data;
  cups_ptype_t             type = 0;
  const gchar             *printer_type = NULL;
  gchar                   *printer_name = NULL;
  gint                     i;

  priv = PRINTERS_PANEL_PRIVATE (self);

  if (priv->current_dest >= 0 &&
      priv->current_dest < priv->num_dests &&
      priv->dests != NULL)
    {
      printer_name = priv->dests[priv->current_dest].name;
      printer_type = cupsGetOption ("printer-type",
                                    priv->dests[priv->current_dest].num_options,
                                    priv->dests[priv->current_dest].options);
      if (printer_type)
        type = atoi (printer_type);
    }

  if (printer_name)
    {
      const gchar  *const dirs[] = { "/usr/share/cups",
                                     "/usr/local/share/cups",
                                     NULL };
      const gchar  *datadir = NULL;
      http_t       *http = NULL;
      gchar        *printer_uri = NULL;
      gchar        *filename = NULL;
      gchar        *resource = NULL;
      ipp_t        *response = NULL;
      ipp_t        *request;

      datadir = getenv ("CUPS_DATADIR");
      if (datadir != NULL)
        {
          filename = get_testprint_filename (datadir);
        }
      else
        {
          for (i = 0; dirs[i] != NULL && filename == NULL; i++)
            filename = get_testprint_filename (dirs[i]);
        }

      if (filename)
        {
          if (type & CUPS_PRINTER_CLASS)
            {
              printer_uri = g_strdup_printf ("ipp://localhost/classes/%s", printer_name);
              resource = g_strdup_printf ("/classes/%s", printer_name);
            }
          else
            {
              printer_uri = g_strdup_printf ("ipp://localhost/printers/%s", printer_name);
              resource = g_strdup_printf ("/printers/%s", printer_name);
            }

          http = httpConnectEncrypt (cupsServer (), ippPort (), cupsEncryption ());
          if (http)
            {
              request = ippNewRequest (IPP_PRINT_JOB);
              ippAddString (request, IPP_TAG_OPERATION, IPP_TAG_URI,
                            "printer-uri", NULL, printer_uri);
              ippAddString (request, IPP_TAG_OPERATION, IPP_TAG_NAME,
                            "requesting-user-name", NULL, cupsUser ());
              ippAddString (request, IPP_TAG_OPERATION, IPP_TAG_NAME,
              /* Translators: Name of job which makes printer to print test page */
                            "job-name", NULL, _("Test page"));
              response = cupsDoFileRequest (http, request, resource, filename);
              httpClose (http);
            }

          if (response)
            {
              if (ippGetState (response) == IPP_ERROR)
                g_warning ("An error has occured during printing of test page.");
              ippDelete (response);
            }

          g_free (filename);
          g_free (printer_uri);
          g_free (resource);
        }
      else
        {
          PpMaintenanceCommand *command;

          command = pp_maintenance_command_new (printer_name,
                                                "PrintSelfTestPage",
          /* Translators: Name of job which makes printer to print test page */
                                                _("Test page"));

          pp_maintenance_command_execute_async (command, NULL, pp_maintenance_command_execute_cb, self);
        }
    }
}

static void
update_sensitivity (gpointer user_data)
{
  CcPrintersPanelPrivate  *priv;
  GtkTreeSelection        *selection;
  CcPrintersPanel         *self = (CcPrintersPanel*) user_data;
  cups_ptype_t             type = 0;
  GtkTreeModel            *model;
  GtkTreeView             *treeview;
  GtkTreeIter              tree_iter;
  const char              *cups_server = NULL;
  GtkWidget               *widget;
  gboolean                 is_authorized;
  gboolean                 is_discovered = FALSE;
  gboolean                 is_class = FALSE;
  gboolean                 is_changing_driver = FALSE;
  gboolean                 printer_selected;
  gboolean                 local_server = TRUE;
  gboolean                 no_cups = FALSE;
  gboolean                 is_new = FALSE;
  gboolean                 already_present_local;
  GList                   *iter;
  gchar                   *current_printer_name = NULL;
  gint                     i;

  priv = PRINTERS_PANEL_PRIVATE (self);

  is_authorized =
    priv->permission &&
    g_permission_get_allowed (G_PERMISSION (priv->permission)) &&
    priv->lockdown_settings &&
    !g_settings_get_boolean (priv->lockdown_settings, "disable-print-setup");

  printer_selected = priv->current_dest >= 0 &&
                     priv->current_dest < priv->num_dests &&
                     priv->dests != NULL;

  if (printer_selected)
    {
      for (i = 0; i < priv->dests[priv->current_dest].num_options; i++)
        {
          if (g_strcmp0 (priv->dests[priv->current_dest].options[i].name, "printer-type") == 0)
            {
              type = atoi (priv->dests[priv->current_dest].options[i].value);
              is_discovered = type & CUPS_PRINTER_DISCOVERED;
              is_class = type & CUPS_PRINTER_CLASS;
              break;
            }
        }

      for (iter = priv->driver_change_list; iter; iter = iter->next)
        {
          SetPPDItem *item = (SetPPDItem *) iter->data;

          if (g_strcmp0 (item->printer_name, priv->dests[priv->current_dest].name) == 0)
            {
              is_changing_driver = TRUE;
            }
        }
    }

  treeview = (GtkTreeView*)
    gtk_builder_get_object (priv->builder, "printers-treeview");

  selection = gtk_tree_view_get_selection (treeview);
  if (selection &&
      gtk_tree_selection_get_selected (selection, &model, &tree_iter))
    {
      gtk_tree_model_get (model, &tree_iter,
                          PRINTER_NAME_COLUMN, &current_printer_name,
                          -1);
    }

  if (priv->new_printer_name &&
      g_strcmp0 (priv->new_printer_name, current_printer_name) == 0)
    {
      printer_selected = TRUE;
      is_discovered = FALSE;
      is_class = FALSE;
      is_new = TRUE;
    }

  g_free (current_printer_name);

  cups_server = cupsServer ();
  if (cups_server &&
      g_ascii_strncasecmp (cups_server, "localhost", 9) != 0 &&
      g_ascii_strncasecmp (cups_server, "127.0.0.1", 9) != 0 &&
      g_ascii_strncasecmp (cups_server, "::1", 3) != 0 &&
      cups_server[0] != '/')
    local_server = FALSE;

  widget = (GtkWidget*) gtk_builder_get_object (priv->builder, "notebook");
  if (gtk_notebook_get_current_page (GTK_NOTEBOOK (widget)) == NOTEBOOK_NO_CUPS_PAGE)
    no_cups = TRUE;

  already_present_local = local_server && !is_discovered && is_authorized && !is_new;

  widget = (GtkWidget*) gtk_builder_get_object (priv->builder, "printer-add-button");
  gtk_widget_set_sensitive (widget, local_server && is_authorized && !no_cups && !priv->new_printer_name);

  widget = (GtkWidget*) gtk_builder_get_object (priv->builder, "printer-add-button2");
  gtk_widget_set_sensitive (widget, local_server && is_authorized && !no_cups && !priv->new_printer_name);

  widget = (GtkWidget*) gtk_builder_get_object (priv->builder, "printer-remove-button");
  gtk_widget_set_sensitive (widget, already_present_local && printer_selected && !no_cups);

  widget = (GtkWidget*) gtk_builder_get_object (priv->builder, "printer-disable-switch");
  gtk_widget_set_sensitive (widget, already_present_local);

  widget = (GtkWidget*) gtk_builder_get_object (priv->builder, "printer-default-check-button");
  gtk_widget_set_sensitive (widget, is_authorized && !is_new);

  widget = (GtkWidget*) gtk_builder_get_object (priv->builder, "print-test-page-button");
  gtk_widget_set_sensitive (widget, printer_selected && !is_new);

  widget = (GtkWidget*) gtk_builder_get_object (priv->builder, "printer-options-button");
  gtk_widget_set_sensitive (widget, printer_selected && local_server && !is_discovered &&
                            !priv->pp_options_dialog && !is_new);

  widget = (GtkWidget*) gtk_builder_get_object (priv->builder, "printer-jobs-button");
  gtk_widget_set_sensitive (widget, printer_selected && !is_new);

  widget = (GtkWidget*) gtk_builder_get_object (priv->builder, "printer-icon");
  gtk_widget_set_sensitive (widget, printer_selected);

  widget = (GtkWidget*) gtk_builder_get_object (priv->builder, "printer-name-label");
  cc_editable_entry_set_editable (CC_EDITABLE_ENTRY (widget), already_present_local);

  widget = (GtkWidget*) gtk_builder_get_object (priv->builder, "printer-location-label");
  cc_editable_entry_set_editable (CC_EDITABLE_ENTRY (widget), already_present_local);

  widget = (GtkWidget*) gtk_builder_get_object (priv->builder, "printer-model-notebook");
  if (is_changing_driver)
    {
      gtk_notebook_set_current_page (GTK_NOTEBOOK (widget), 2);
    }
  else
    {
      if (already_present_local && !is_class && !priv->getting_ppd_names)
        gtk_notebook_set_current_page (GTK_NOTEBOOK (widget), 0);
      else
        gtk_notebook_set_current_page (GTK_NOTEBOOK (widget), 1);
    }
}

static void
on_permission_changed (GPermission *permission,
                       GParamSpec  *pspec,
                       gpointer     data)
{
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
printer_options_response_cb (GtkDialog *dialog,
                             gint       response_id,
                             gpointer   user_data)
{
  CcPrintersPanelPrivate *priv;
  CcPrintersPanel        *self = (CcPrintersPanel*) user_data;

  priv = PRINTERS_PANEL_PRIVATE (self);

  pp_options_dialog_free (priv->pp_options_dialog);
  priv->pp_options_dialog = NULL;
  update_sensitivity (self);

  if (response_id == GTK_RESPONSE_OK)
    actualize_printers_list (self);
}

static void
printer_options_cb (GtkToolButton *toolbutton,
                    gpointer       user_data)
{
  CcPrintersPanelPrivate *priv;
  CcPrintersPanel        *self = (CcPrintersPanel*) user_data;
  GtkWidget              *widget;
  gboolean                is_authorized;

  priv = PRINTERS_PANEL_PRIVATE (self);

  widget = (GtkWidget*)
    gtk_builder_get_object (priv->builder, "main-vbox");

  is_authorized =
    priv->permission &&
    g_permission_get_allowed (G_PERMISSION (priv->permission)) &&
    priv->lockdown_settings &&
    !g_settings_get_boolean (priv->lockdown_settings, "disable-print-setup");

  if (priv->current_dest >= 0 &&
      priv->current_dest < priv->num_dests &&
      priv->dests != NULL)
    {
      priv->pp_options_dialog = pp_options_dialog_new (
        GTK_WINDOW (gtk_widget_get_toplevel (widget)),
        printer_options_response_cb,
        self,
        priv->dests[priv->current_dest].name,
        is_authorized);
      update_sensitivity (self);
    }
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
update_label_padding (GtkWidget     *widget,
                      GtkAllocation *allocation,
                      gpointer       user_data)
{
  CcPrintersPanelPrivate *priv;
  CcPrintersPanel        *self = (CcPrintersPanel*) user_data;
  GtkAllocation           allocation1, allocation2;
  GtkWidget              *label;
  GtkWidget              *sublabel;
  gint                    offset;
  gint                    margin;

  priv = PRINTERS_PANEL_PRIVATE (self);

  sublabel = gtk_bin_get_child (GTK_BIN (widget));
  if (sublabel)
    {
      gtk_widget_get_allocation (widget, &allocation1);
      gtk_widget_get_allocation (sublabel, &allocation2);

      offset = allocation2.x - allocation1.x;

      label = (GtkWidget*)
        gtk_builder_get_object (priv->builder, "printer-model-label");

      margin = gtk_widget_get_margin_start (label);
      if (offset != margin)
        gtk_widget_set_margin_start (label, offset);

      label = GTK_WIDGET (gtk_builder_get_object (priv->builder, "printer-model-setting"));

      margin = gtk_widget_get_margin_start (label);
      if (offset != margin)
        gtk_widget_set_margin_start (label, offset);
    }
}

static void
jobs_dialog_response_cb (GtkDialog *dialog,
                         gint       response_id,
                         gpointer   user_data)
{
  CcPrintersPanelPrivate *priv;
  CcPrintersPanel        *self = (CcPrintersPanel*) user_data;

  priv = PRINTERS_PANEL_PRIVATE (self);

  pp_jobs_dialog_free (priv->pp_jobs_dialog);
  priv->pp_jobs_dialog = NULL;
}

static void
printer_jobs_cb (GtkToolButton *toolbutton,
                 gpointer       user_data)
{
  CcPrintersPanelPrivate *priv;
  CcPrintersPanel        *self = (CcPrintersPanel*) user_data;
  GtkWidget              *widget;

  priv = PRINTERS_PANEL_PRIVATE (self);

  widget = (GtkWidget*)
    gtk_builder_get_object (priv->builder, "main-vbox");

  if (priv->current_dest >= 0 &&
      priv->current_dest < priv->num_dests &&
      priv->dests != NULL)
    priv->pp_jobs_dialog = pp_jobs_dialog_new (
      GTK_WINDOW (gtk_widget_get_toplevel (widget)),
      jobs_dialog_response_cb,
      self,
      priv->dests[priv->current_dest].name);
}

static void
cc_printers_panel_init (CcPrintersPanel *self)
{
  CcPrintersPanelPrivate *priv;
  GtkWidget              *top_widget;
  GtkWidget              *widget;
  PpCups                 *cups;
  GError                 *error = NULL;
  gchar                  *objects[] = { "main-vbox", NULL };
  GtkStyleContext        *context;
  guint                   builder_result;

  priv = self->priv = PRINTERS_PANEL_PRIVATE (self);
  g_resources_register (cc_printers_get_resource ());

  /* initialize main data structure */
  priv->builder = gtk_builder_new ();
  priv->dests = NULL;
  priv->dest_model_names = NULL;
  priv->ppd_file_names = NULL;
  priv->num_dests = 0;
  priv->current_dest = -1;

  priv->num_jobs = 0;

  priv->pp_new_printer_dialog = NULL;
  priv->pp_options_dialog = NULL;

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

  priv->getting_ppd_names = FALSE;

  priv->all_ppds_list = NULL;
  priv->get_all_ppds_cancellable = NULL;

  priv->preferred_drivers = NULL;

  priv->actualize_printers_list_cancellable = g_cancellable_new ();

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

  widget = (GtkWidget*)
    gtk_builder_get_object (priv->builder, "printer-remove-button");
  g_signal_connect (widget, "clicked", G_CALLBACK (printer_remove_cb), self);

  widget = (GtkWidget*)
    gtk_builder_get_object (priv->builder, "printer-disable-switch");
  g_signal_connect (widget, "notify::active", G_CALLBACK (printer_disable_cb), self);

  widget = (GtkWidget*)
    gtk_builder_get_object (priv->builder, "supply-drawing-area");
  g_signal_connect (widget, "draw", G_CALLBACK (supply_levels_draw_cb), self);

  widget = (GtkWidget*)
    gtk_builder_get_object (priv->builder, "printer-default-check-button");
  g_signal_connect (widget, "toggled", G_CALLBACK (printer_set_default_cb), self);

  widget = (GtkWidget*)
    gtk_builder_get_object (priv->builder, "print-test-page-button");
  g_signal_connect (widget, "clicked", G_CALLBACK (test_page_cb), self);

  widget = (GtkWidget*)
    gtk_builder_get_object (priv->builder, "printer-jobs-button");
  g_signal_connect (widget, "clicked", G_CALLBACK (printer_jobs_cb), self);

  widget = (GtkWidget*)
    gtk_builder_get_object (priv->builder, "printer-options-button");
  g_signal_connect (widget, "clicked", G_CALLBACK (printer_options_cb), self);

  widget = (GtkWidget*)
    gtk_builder_get_object (priv->builder, "printer-name-label");
  g_signal_connect (widget, "editing-done", G_CALLBACK (printer_name_edit_cb), self);

  widget = (GtkWidget*)
    gtk_builder_get_object (priv->builder, "printer-location-label");
  g_signal_connect (widget, "editing-done", G_CALLBACK (printer_location_edit_cb), self);

  priv->lockdown_settings = g_settings_new ("org.gnome.desktop.lockdown");
  if (priv->lockdown_settings)
    g_signal_connect_object (priv->lockdown_settings,
                             "changed",
                             G_CALLBACK (on_lockdown_settings_changed),
                             self,
                             G_CONNECT_AFTER);

  widget = (GtkWidget*)
    gtk_builder_get_object (priv->builder, "printer-model-button");
  g_signal_connect (widget, "clicked", G_CALLBACK (popup_model_menu_cb), self);
  g_signal_connect (widget, "size-allocate", G_CALLBACK (update_label_padding), self);


  /* Set junctions */
  widget = (GtkWidget*)
    gtk_builder_get_object (priv->builder, "printers-scrolledwindow");
  context = gtk_widget_get_style_context (widget);
  gtk_style_context_set_junction_sides (context, GTK_JUNCTION_BOTTOM);

  widget = (GtkWidget*)
    gtk_builder_get_object (priv->builder, "printers-toolbar");
  context = gtk_widget_get_style_context (widget);
  gtk_style_context_set_junction_sides (context, GTK_JUNCTION_TOP);


  /* Make model label and ip-address label selectable */
  widget = (GtkWidget*)
    gtk_builder_get_object (priv->builder, "printer-ip-address-label");
  cc_editable_entry_set_selectable (CC_EDITABLE_ENTRY (widget), TRUE);


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

  populate_printers_list (self);
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
