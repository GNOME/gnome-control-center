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

#include "pp-new-printer.h"

#include <glib/gstdio.h>
#include <glib/gi18n.h>

#include "pp-utils.h"
#include "pp-maintenance-command.h"

#define PACKAGE_KIT_BUS "org.freedesktop.PackageKit"
#define PACKAGE_KIT_PATH "/org/freedesktop/PackageKit"
#define PACKAGE_KIT_MODIFY_IFACE "org.freedesktop.PackageKit.Modify"
#define PACKAGE_KIT_QUERY_IFACE  "org.freedesktop.PackageKit.Query"

#define DBUS_TIMEOUT      120000
#define DBUS_TIMEOUT_LONG 600000

#if (CUPS_VERSION_MAJOR > 1) || (CUPS_VERSION_MINOR > 5)
#define HAVE_CUPS_1_6 1
#endif

#ifndef HAVE_CUPS_1_6
#define ippGetState(ipp)      ipp->state
#endif

struct _PpNewPrinter
{
  GObject   parent_instance;

  gchar    *name;
  gchar    *original_name;
  gchar    *device_uri;
  gchar    *device_id;
  gchar    *ppd_name;
  gchar    *ppd_file_name;
  gchar    *info;
  gchar    *location;
  gchar    *make_and_model;
  gchar    *host_name;
  gint      host_port;
  gboolean  is_network_device;
  guint     window_id;
  gboolean  unlink_ppd_file;

  GSimpleAsyncResult *res;
  GCancellable *cancellable;
};

G_DEFINE_TYPE (PpNewPrinter, pp_new_printer, G_TYPE_OBJECT);

enum {
  PROP_0 = 0,
  PROP_NAME,
  PROP_ORIGINAL_NAME,
  PROP_DEVICE_URI,
  PROP_DEVICE_ID,
  PROP_PPD_NAME,
  PROP_PPD_FILE_NAME,
  PROP_INFO,
  PROP_LOCATION,
  PROP_MAKE_AND_MODEL,
  PROP_HOST_NAME,
  PROP_HOST_PORT,
  PROP_IS_NETWORK_DEVICE,
  PROP_WINDOW_ID
};

static void
pp_new_printer_finalize (GObject *object)
{
  PpNewPrinter *self = PP_NEW_PRINTER (object);

  if (self->unlink_ppd_file && self->ppd_file_name)
    g_unlink (self->ppd_file_name);

  g_clear_pointer (&self->name, g_free);
  g_clear_pointer (&self->original_name, g_free);
  g_clear_pointer (&self->device_uri, g_free);
  g_clear_pointer (&self->device_id, g_free);
  g_clear_pointer (&self->ppd_name, g_free);
  g_clear_pointer (&self->ppd_file_name, g_free);
  g_clear_pointer (&self->info, g_free);
  g_clear_pointer (&self->location, g_free);
  g_clear_pointer (&self->make_and_model, g_free);
  g_clear_pointer (&self->host_name, g_free);
  g_clear_object (&self->res);
  g_clear_object (&self->cancellable);

  G_OBJECT_CLASS (pp_new_printer_parent_class)->finalize (object);
}

static void
pp_new_printer_get_property (GObject    *object,
                             guint       prop_id,
                             GValue     *value,
                             GParamSpec *param_spec)
{
  PpNewPrinter *self;

  self = PP_NEW_PRINTER (object);

  switch (prop_id)
    {
      case PROP_NAME:
        g_value_set_string (value, self->name);
        break;
      case PROP_ORIGINAL_NAME:
        g_value_set_string (value, self->original_name);
        break;
      case PROP_DEVICE_URI:
        g_value_set_string (value, self->device_uri);
        break;
      case PROP_DEVICE_ID:
        g_value_set_string (value, self->device_id);
        break;
      case PROP_PPD_NAME:
        g_value_set_string (value, self->ppd_name);
        break;
      case PROP_PPD_FILE_NAME:
        g_value_set_string (value, self->ppd_file_name);
        break;
      case PROP_INFO:
        g_value_set_string (value, self->info);
        break;
      case PROP_LOCATION:
        g_value_set_string (value, self->location);
        break;
      case PROP_MAKE_AND_MODEL:
        g_value_set_string (value, self->make_and_model);
        break;
      case PROP_HOST_NAME:
        g_value_set_string (value, self->host_name);
        break;
      case PROP_HOST_PORT:
        g_value_set_int (value, self->host_port);
        break;
      case PROP_IS_NETWORK_DEVICE:
        g_value_set_boolean (value, self->is_network_device);
        break;
      case PROP_WINDOW_ID:
        g_value_set_uint (value, self->window_id);
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object,
                                           prop_id,
                                           param_spec);
      break;
    }
}

static void
pp_new_printer_set_property (GObject      *object,
                             guint         prop_id,
                             const GValue *value,
                             GParamSpec   *param_spec)
{
  PpNewPrinter *self = PP_NEW_PRINTER (object);

  switch (prop_id)
    {
      case PROP_NAME:
        g_free (self->name);
        self->name = g_value_dup_string (value);
        break;
      case PROP_ORIGINAL_NAME:
        g_free (self->original_name);
        self->original_name = g_value_dup_string (value);
        break;
      case PROP_DEVICE_URI:
        g_free (self->device_uri);
        self->device_uri = g_value_dup_string (value);
        break;
      case PROP_DEVICE_ID:
        g_free (self->device_id);
        self->device_id = g_value_dup_string (value);
        break;
      case PROP_PPD_NAME:
        g_free (self->ppd_name);
        self->ppd_name = g_value_dup_string (value);
        break;
      case PROP_PPD_FILE_NAME:
        g_free (self->ppd_file_name);
        self->ppd_file_name = g_value_dup_string (value);
        break;
      case PROP_INFO:
        g_free (self->info);
        self->info = g_value_dup_string (value);
        break;
      case PROP_LOCATION:
        g_free (self->location);
        self->location = g_value_dup_string (value);
        break;
      case PROP_MAKE_AND_MODEL:
        g_free (self->make_and_model);
        self->make_and_model = g_value_dup_string (value);
        break;
      case PROP_HOST_NAME:
        g_free (self->host_name);
        self->host_name = g_value_dup_string (value);
        break;
      case PROP_HOST_PORT:
        self->host_port = g_value_get_int (value);
        break;
      case PROP_IS_NETWORK_DEVICE:
        self->is_network_device = g_value_get_boolean (value);
        break;
      case PROP_WINDOW_ID:
        self->window_id = g_value_get_uint (value);
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object,
                                           prop_id,
                                           param_spec);
        break;
    }
}

static void
pp_new_printer_class_init (PpNewPrinterClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->set_property = pp_new_printer_set_property;
  gobject_class->get_property = pp_new_printer_get_property;

  gobject_class->finalize = pp_new_printer_finalize;

  g_object_class_install_property (gobject_class, PROP_NAME,
    g_param_spec_string ("name",
                         "Name",
                         "The new printer's name",
                         NULL,
                         G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, PROP_ORIGINAL_NAME,
    g_param_spec_string ("original-name",
                         "Original name",
                         "Original name of the new printer",
                         NULL,
                         G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, PROP_DEVICE_URI,
    g_param_spec_string ("device-uri",
                         "Device URI",
                         "The new printer's device URI",
                         NULL,
                         G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, PROP_DEVICE_ID,
    g_param_spec_string ("device-id",
                         "DeviceID",
                         "The new printer's DeviceID",
                         NULL,
                         G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, PROP_PPD_NAME,
    g_param_spec_string ("ppd-name",
                         "PPD name",
                         "Name of PPD for the new printer",
                         NULL,
                         G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, PROP_PPD_FILE_NAME,
    g_param_spec_string ("ppd-file-name",
                         "PPD file name",
                         "PPD file for the new printer",
                         NULL,
                         G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, PROP_INFO,
    g_param_spec_string ("info",
                         "Printer info",
                         "The new printer's info",
                         NULL,
                         G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, PROP_LOCATION,
    g_param_spec_string ("location",
                         "Printer location",
                         "The new printer's location",
                         NULL,
                         G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, PROP_MAKE_AND_MODEL,
    g_param_spec_string ("make-and-model",
                         "Printer make and model",
                         "The new printer's make and model",
                         NULL,
                         G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, PROP_HOST_NAME,
    g_param_spec_string ("host-name",
                         "Hostname",
                         "The new printer's hostname",
                         NULL,
                         G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, PROP_HOST_PORT,
    g_param_spec_int ("host-port",
                      "Host port",
                      "The port of the host",
                      0, G_MAXINT32, 631,
                      G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, PROP_IS_NETWORK_DEVICE,
    g_param_spec_boolean ("is-network-device",
                          "Network device",
                          "Whether the new printer is a network device",
                          FALSE,
                          G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, PROP_WINDOW_ID,
    g_param_spec_uint ("window-id",
                       "WindowID",
                       "Window ID of parent window",
                       0, G_MAXUINT, 0,
                       G_PARAM_READWRITE));
}

static void
pp_new_printer_init (PpNewPrinter *printer)
{
  printer->unlink_ppd_file = FALSE;
  printer->cancellable = NULL;
  printer->res = NULL;
}

PpNewPrinter *
pp_new_printer_new ()
{
  return g_object_new (PP_TYPE_NEW_PRINTER, NULL);
}

static void printer_configure_async (PpNewPrinter *new_printer);

static void
_pp_new_printer_add_async_cb (gboolean      success,
                              PpNewPrinter *printer)
{
  if (!success)
    {
      g_simple_async_result_set_error (printer->res,
                                       G_IO_ERROR,
                                       G_IO_ERROR_FAILED,
                                       "Installation of the new printer failed.");
    }

  g_simple_async_result_set_op_res_gboolean (printer->res, success);
  g_simple_async_result_complete_in_idle (printer->res);
}

static void
printer_add_real_async_cb (cups_dest_t *destination,
                           gpointer     user_data)
{
  PpNewPrinter        *printer = (PpNewPrinter *) user_data;
  gboolean             success = FALSE;

  if (destination)
    {
      success = TRUE;
      cupsFreeDests (1, destination);
    }

  if (success)
    {
      printer_configure_async (printer);
    }
  else
    {
      _pp_new_printer_add_async_cb (FALSE, printer);
    }
}

static void
printer_add_real_async_dbus_cb (GObject      *source_object,
                                GAsyncResult *res,
                                gpointer      user_data)
{
  PpNewPrinter        *printer = (PpNewPrinter *) user_data;
  GVariant            *output;
  GError              *error = NULL;

  output = g_dbus_connection_call_finish (G_DBUS_CONNECTION (source_object),
                                          res,
                                          &error);
  g_object_unref (source_object);

  if (output)
    {
      const gchar *ret_error;

      g_variant_get (output, "(&s)", &ret_error);
      if (ret_error[0] != '\0')
        {
          g_warning ("cups-pk-helper: addition of printer %s failed: %s", printer->name, ret_error);
        }

      g_variant_unref (output);
    }
  else
    {
      if (error->domain != G_IO_ERROR ||
          error->code != G_IO_ERROR_CANCELLED)
        g_warning ("%s", error->message);
    }

  if (!error ||
      error->domain != G_IO_ERROR ||
      error->code != G_IO_ERROR_CANCELLED)
    {
      get_named_dest_async (printer->name,
                            printer_add_real_async_cb,
                            printer);
    }

  if (error)
      g_error_free (error);
}

static void
printer_add_real_async (PpNewPrinter *printer)
{
  GDBusConnection     *bus;
  GError              *error = NULL;

  if (!printer->ppd_name && !printer->ppd_file_name)
    {
      _pp_new_printer_add_async_cb (FALSE, printer);
      return;
    }

  bus = g_bus_get_sync (G_BUS_TYPE_SYSTEM, NULL, &error);
  if (!bus)
    {
      g_warning ("Failed to get system bus: %s", error->message);
      g_error_free (error);
      _pp_new_printer_add_async_cb (FALSE, printer);
      return;
    }

  g_dbus_connection_call (bus,
                          MECHANISM_BUS,
                          "/",
                          MECHANISM_BUS,
                          printer->ppd_name ? "PrinterAdd" : "PrinterAddWithPpdFile",
                          g_variant_new ("(sssss)",
                                         printer->name,
                                         printer->device_uri,
                                         printer->ppd_name ? printer->ppd_name : printer->ppd_file_name,
                                         printer->info ? printer->info : "",
                                         printer->location ? printer->location : ""),
                          G_VARIANT_TYPE ("(s)"),
                          G_DBUS_CALL_FLAGS_NONE,
                          DBUS_TIMEOUT,
                          NULL,
                          printer_add_real_async_dbus_cb,
                          printer);
}

static PPDName *
get_ppd_item_from_output (GVariant *output)
{
  GVariant *array;
  PPDName  *ppd_item = NULL;
  gint      j;
  static const char * const match_levels[] = {
             "exact-cmd",
             "exact",
             "close",
             "generic",
             "none"};

  if (output)
    {
      g_variant_get (output, "(@a(ss))", &array);
      if (array)
        {
          GVariantIter *iter;
          GVariant     *item;
          gchar        *driver;
          gchar        *match;

          for (j = 0; j < G_N_ELEMENTS (match_levels) && !ppd_item; j++)
            {
              g_variant_get (array, "a(ss)", &iter);
              while ((item = g_variant_iter_next_value (iter)) && !ppd_item)
                {
                  g_variant_get (item, "(ss)", &driver, &match);
                  if (g_str_equal (match, match_levels[j]))
                    {
                      ppd_item = g_new0 (PPDName, 1);
                      ppd_item->ppd_name = g_strdup (driver);

                      if (g_strcmp0 (match, "exact-cmd") == 0)
                        ppd_item->ppd_match_level = PPD_EXACT_CMD_MATCH;
                      else if (g_strcmp0 (match, "exact") == 0)
                        ppd_item->ppd_match_level = PPD_EXACT_MATCH;
                      else if (g_strcmp0 (match, "close") == 0)
                        ppd_item->ppd_match_level = PPD_CLOSE_MATCH;
                      else if (g_strcmp0 (match, "generic") == 0)
                        ppd_item->ppd_match_level = PPD_GENERIC_MATCH;
                      else if (g_strcmp0 (match, "none") == 0)
                        ppd_item->ppd_match_level = PPD_NO_MATCH;
                    }

                  g_free (driver);
                  g_free (match);
                  g_variant_unref (item);
                }
            }

          g_variant_unref (array);
        }
    }

  return ppd_item;
}


static void
printer_add_async_scb3 (GObject      *source_object,
                        GAsyncResult *res,
                        gpointer      user_data)
{
  PpNewPrinter        *printer = (PpNewPrinter *) user_data;
  GVariant            *output;
  PPDName             *ppd_item = NULL;
  GError              *error = NULL;

  output = g_dbus_connection_call_finish (G_DBUS_CONNECTION (source_object),
                                          res,
                                          &error);
  g_object_unref (source_object);

  if (output)
    {
      ppd_item = get_ppd_item_from_output (output);
      g_variant_unref (output);
    }
  else
    {
      if (error->domain != G_IO_ERROR ||
          error->code != G_IO_ERROR_CANCELLED)
        g_warning ("%s", error->message);
    }

  if ((!error ||
      error->domain != G_IO_ERROR ||
      error->code != G_IO_ERROR_CANCELLED) &&
      ppd_item && ppd_item->ppd_name)
    {
      printer->ppd_name = g_strdup (ppd_item->ppd_name);
      printer_add_real_async (printer);
    }
  else
    {
      _pp_new_printer_add_async_cb (FALSE, printer);
    }

  if (error)
    {
      g_error_free (error);
    }

  if (ppd_item)
    {
      g_free (ppd_item->ppd_name);
      g_free (ppd_item);
    }
}

static void
install_printer_drivers_cb (GObject      *source_object,
                            GAsyncResult *res,
                            gpointer      user_data)
{
  PpNewPrinter        *printer;
  GVariant            *output;
  GError              *error = NULL;

  output = g_dbus_connection_call_finish (G_DBUS_CONNECTION (source_object),
                                          res,
                                          &error);
  g_object_unref (source_object);

  if (output)
    {
      g_variant_unref (output);
    }
  else
    {
      if (error->domain != G_IO_ERROR ||
          error->code != G_IO_ERROR_CANCELLED)
        g_warning ("%s", error->message);
    }

  if (!error ||
      error->domain != G_IO_ERROR ||
      error->code != G_IO_ERROR_CANCELLED)
    {
      GDBusConnection *bus;
      GError          *error = NULL;

      printer = (PpNewPrinter *) user_data;

      /* Try whether CUPS has a driver for the new printer */
      bus = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, &error);
      if (bus)
        {
          g_dbus_connection_call (bus,
                                  SCP_BUS,
                                  SCP_PATH,
                                  SCP_IFACE,
                                  "GetBestDrivers",
                                  g_variant_new ("(sss)",
                                                 printer->device_id,
                                                 printer->make_and_model ? printer->make_and_model : "",
                                                 printer->device_uri ? printer->device_uri : ""),
                                  G_VARIANT_TYPE ("(a(ss))"),
                                  G_DBUS_CALL_FLAGS_NONE,
                                  DBUS_TIMEOUT_LONG,
                                  printer->cancellable,
                                  printer_add_async_scb3,
                                  printer);
        }
      else
        {
          g_warning ("Failed to get system bus: %s", error->message);
          g_error_free (error);
          _pp_new_printer_add_async_cb (FALSE, printer);
        }
    }

  if (error)
    g_error_free (error);
}

static void
printer_add_async_scb (GObject      *source_object,
                       GAsyncResult *res,
                       gpointer      user_data)
{
  PpNewPrinter        *printer = (PpNewPrinter *) user_data;
  GDBusConnection     *bus;
  GVariantBuilder      array_builder;
  GVariant            *output;
  gboolean             cancelled = FALSE;
  PPDName             *ppd_item = NULL;
  GError              *error = NULL;

  output = g_dbus_connection_call_finish (G_DBUS_CONNECTION (source_object),
                                          res,
                                          &error);
  g_object_unref (source_object);

  if (output)
    {
      ppd_item = get_ppd_item_from_output (output);
      g_variant_unref (output);
    }
  else
    {
      cancelled = g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED);

      if (!cancelled)
        g_warning ("%s", error->message);

      g_clear_error (&error);
    }

  if (!cancelled)
    {
      if (ppd_item == NULL || ppd_item->ppd_match_level < PPD_EXACT_MATCH)
        {
          bus = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, &error);
          if (bus)
            {
              g_variant_builder_init (&array_builder, G_VARIANT_TYPE ("as"));
              g_variant_builder_add (&array_builder, "s", printer->device_id);

              g_dbus_connection_call (bus,
                                      PACKAGE_KIT_BUS,
                                      PACKAGE_KIT_PATH,
                                      PACKAGE_KIT_MODIFY_IFACE,
                                      "InstallPrinterDrivers",
                                      g_variant_new ("(uass)",
                                                     printer->window_id,
                                                     &array_builder,
                                                     "hide-finished"),
                                      G_VARIANT_TYPE ("()"),
                                      G_DBUS_CALL_FLAGS_NONE,
                                      DBUS_TIMEOUT_LONG,
                                      NULL,
                                      install_printer_drivers_cb,
                                      printer);
            }
          else
            {
              g_warning ("Failed to get session bus: %s", error->message);
              g_error_free (error);
              _pp_new_printer_add_async_cb (FALSE, printer);
            }
        }
      else if (ppd_item && ppd_item->ppd_name)
        {
          printer->ppd_name = g_strdup (ppd_item->ppd_name);
          printer_add_real_async (printer);
        }
      else
        {
          _pp_new_printer_add_async_cb (FALSE, printer);
        }
    }

  if (ppd_item)
    {
      g_free (ppd_item->ppd_name);
      g_free (ppd_item);
    }
}

static void
printer_add_async_scb4 (const gchar *ppd_filename,
                        gpointer     user_data)
{
  PpNewPrinter        *printer = (PpNewPrinter *) user_data;

  printer->ppd_file_name = g_strdup (ppd_filename);
  if (printer->ppd_file_name)
    {
      printer->unlink_ppd_file = TRUE;
      printer_add_real_async (printer);
    }
  else
    {
      _pp_new_printer_add_async_cb (FALSE, printer);
    }
}

static GList *
glist_uniq (GList *list)
{
  GList *result = NULL;
  GList *iter = NULL;
  GList *tmp = NULL;

  for (iter = list; iter; iter = iter->next)
    {
      if (tmp == NULL ||
          g_strcmp0 ((gchar *) tmp->data, (gchar *) iter->data) != 0)
        {
          tmp = iter;
          result = g_list_append (result, g_strdup (iter->data));
        }
    }

  g_list_free_full (list, g_free);

  return result;
}

typedef struct
{
  PpNewPrinter *new_printer;
  GCancellable *cancellable;
  gboolean      set_accept_jobs_finished;
  gboolean      set_enabled_finished;
  gboolean      autoconfigure_finished;
  gboolean      set_media_size_finished;
  gboolean      install_missing_executables_finished;
} PCData;

static void
printer_configure_async_finish (PCData *data)
{
  PpNewPrinter *printer = data->new_printer;

  if (data->set_accept_jobs_finished &&
      data->set_enabled_finished &&
      (data->autoconfigure_finished || printer->is_network_device) &&
      data->set_media_size_finished &&
      data->install_missing_executables_finished)
    {
      _pp_new_printer_add_async_cb (TRUE, data->new_printer);

      if (data->cancellable)
        g_object_unref (data->cancellable);
      g_free (data);
    }
}

static void
pao_cb (gboolean success,
        gpointer user_data)
{
  PCData *data = (PCData *) user_data;

  data->set_media_size_finished = TRUE;
  printer_configure_async_finish (data);
}

static void
printer_set_accepting_jobs_cb (GObject      *source_object,
                               GAsyncResult *res,
                               gpointer      user_data)
{
  GVariant *output;
  PCData   *data = (PCData *) user_data;
  GError   *error = NULL;

  output = g_dbus_connection_call_finish (G_DBUS_CONNECTION (source_object),
                                          res,
                                          &error);
  g_object_unref (source_object);

  if (output)
    {
      g_variant_unref (output);
    }
  else
    {
      if (error->domain != G_IO_ERROR ||
          error->code != G_IO_ERROR_CANCELLED)
        g_warning ("%s", error->message);
      g_error_free (error);
    }

  data->set_accept_jobs_finished = TRUE;
  printer_configure_async_finish (data);
}

static void
printer_set_enabled_cb (GObject      *source_object,
                        GAsyncResult *res,
                        gpointer      user_data)
{
  GVariant *output;
  PCData   *data = (PCData *) user_data;
  GError   *error = NULL;

  output = g_dbus_connection_call_finish (G_DBUS_CONNECTION (source_object),
                                          res,
                                          &error);
  g_object_unref (source_object);

  if (output)
    {
      g_variant_unref (output);
    }
  else
    {
      if (error->domain != G_IO_ERROR ||
          error->code != G_IO_ERROR_CANCELLED)
        g_warning ("%s", error->message);
      g_error_free (error);
    }

  data->set_enabled_finished = TRUE;
  printer_configure_async_finish (data);
}

typedef struct
{
  GList        *executables;
  GList        *packages;
  guint         window_id;
  gchar        *ppd_file_name;
  GCancellable *cancellable;
  gpointer      user_data;
} IMEData;

static void
install_missing_executables_cb (IMEData *data)
{
  PCData *pc_data = (PCData *) data->user_data;

  pc_data->install_missing_executables_finished = TRUE;
  printer_configure_async_finish (pc_data);

  if (data->ppd_file_name)
    {
      g_unlink (data->ppd_file_name);
      g_clear_pointer (&data->ppd_file_name, g_free);
    }

  if (data->executables)
    {
      g_list_free_full (data->executables, g_free);
      data->executables = NULL;
    }

  if (data->packages)
    {
      g_list_free_full (data->packages, g_free);
      data->packages = NULL;
    }

  if (data->cancellable)
    g_clear_object (&data->cancellable);

  g_free (data);
}

static void
install_package_names_cb (GObject      *source_object,
                          GAsyncResult *res,
                          gpointer      user_data)
{
  GVariant *output;
  IMEData  *data = (IMEData *) user_data;
  GError   *error = NULL;

  output = g_dbus_connection_call_finish (G_DBUS_CONNECTION (source_object),
                                          res,
                                          &error);
  g_object_unref (source_object);

  if (output)
    {
      g_variant_unref (output);
    }
  else
    {
      if (error->domain != G_IO_ERROR ||
          error->code != G_IO_ERROR_CANCELLED)
        g_warning ("%s", error->message);
      g_error_free (error);
    }

  install_missing_executables_cb (data);
}


static void
search_files_cb (GObject      *source_object,
                 GAsyncResult *res,
                 gpointer      user_data)
{
  GVariant *output;
  IMEData  *data = (IMEData *) user_data;
  GError   *error = NULL;
  GList    *item;

  output = g_dbus_connection_call_finish (G_DBUS_CONNECTION (source_object),
                                          res,
                                          &error);
  if (output)
    {
      gboolean  installed;
      gchar    *package;

      g_variant_get (output,
                     "(bs)",
                     &installed,
                     &package);

      if (!installed)
        data->packages = g_list_append (data->packages, g_strdup (package));
      g_variant_unref (output);
    }
  else
    {
      if (error->domain != G_IO_ERROR ||
          error->code != G_IO_ERROR_CANCELLED)
        g_warning ("%s", error->message);
      g_error_free (error);
    }

  if (data->executables)
    {
      item = data->executables;
      g_dbus_connection_call (G_DBUS_CONNECTION (source_object),
                              PACKAGE_KIT_BUS,
                              PACKAGE_KIT_PATH,
                              PACKAGE_KIT_QUERY_IFACE,
                              "SearchFile",
                              g_variant_new ("(ss)",
                                             (gchar *) item->data,
                                             ""),
                              G_VARIANT_TYPE ("(bs)"),
                              G_DBUS_CALL_FLAGS_NONE,
                              DBUS_TIMEOUT_LONG,
                              data->cancellable,
                              search_files_cb,
                              data);

      data->executables = g_list_remove_link (data->executables, item);
      g_list_free_full (item, g_free);
    }
  else
    {
      GVariantBuilder  array_builder;
      GList           *pkg_iter;

      data->packages = g_list_sort (data->packages, (GCompareFunc) g_strcmp0);
      data->packages = glist_uniq (data->packages);

      if (data->packages)
        {
          g_variant_builder_init (&array_builder, G_VARIANT_TYPE ("as"));

          for (pkg_iter = data->packages; pkg_iter; pkg_iter = pkg_iter->next)
            g_variant_builder_add (&array_builder,
                                   "s",
                                   (gchar *) pkg_iter->data);

          g_dbus_connection_call (G_DBUS_CONNECTION (source_object),
                                  PACKAGE_KIT_BUS,
                                  PACKAGE_KIT_PATH,
                                  PACKAGE_KIT_MODIFY_IFACE,
                                  "InstallPackageNames",
                                  g_variant_new ("(uass)",
                                                 data->window_id,
                                                 &array_builder,
                                                 "hide-finished"),
                                  NULL,
                                  G_DBUS_CALL_FLAGS_NONE,
                                  DBUS_TIMEOUT_LONG,
                                  data->cancellable,
                                  install_package_names_cb,
                                  data);

          g_list_free_full (data->packages, g_free);
          data->packages = NULL;
        }
      else
        {
          g_object_unref (source_object);
          install_missing_executables_cb (data);
        }
    }
}

static void
get_missing_executables_cb (GObject      *source_object,
                            GAsyncResult *res,
                            gpointer      user_data)
{
  GVariant *output;
  IMEData  *data = (IMEData *) user_data;
  GError   *error = NULL;
  GList    *executables = NULL;
  GList    *item;

  if (data->ppd_file_name)
    {
      g_unlink (data->ppd_file_name);
      g_clear_pointer (&data->ppd_file_name, g_free);
    }

  output = g_dbus_connection_call_finish (G_DBUS_CONNECTION (source_object),
                                          res,
                                          &error);

  if (output)
    {
      GVariant *array;

      g_variant_get (output, "(@as)", &array);

      if (array)
        {
          GVariantIter *iter;
          GVariant     *item;
          gchar        *executable;

          g_variant_get (array, "as", &iter);
          while ((item = g_variant_iter_next_value (iter)))
            {
              g_variant_get (item, "s", &executable);
              executables = g_list_append (executables, executable);
              g_variant_unref (item);
            }

          g_variant_unref (array);
        }

      g_variant_unref (output);
    }
  else if (error->domain == G_DBUS_ERROR &&
           (error->code == G_DBUS_ERROR_SERVICE_UNKNOWN ||
            error->code == G_DBUS_ERROR_UNKNOWN_METHOD))
    {
      g_warning ("Install system-config-printer which provides \
DBus method \"MissingExecutables\" to find missing executables and filters.");
      g_error_free (error);
    }
  else
    {
      if (error->domain != G_IO_ERROR ||
          error->code != G_IO_ERROR_CANCELLED)
        g_warning ("%s", error->message);
      g_error_free (error);
    }

  executables = g_list_sort (executables, (GCompareFunc) g_strcmp0);
  executables = glist_uniq (executables);

  if (executables)
    {
      data->executables = executables;

      item = data->executables;
      g_dbus_connection_call (g_object_ref (source_object),
                              PACKAGE_KIT_BUS,
                              PACKAGE_KIT_PATH,
                              PACKAGE_KIT_QUERY_IFACE,
                              "SearchFile",
                              g_variant_new ("(ss)",
                                             (gchar *) item->data,
                                             ""),
                              G_VARIANT_TYPE ("(bs)"),
                              G_DBUS_CALL_FLAGS_NONE,
                              DBUS_TIMEOUT_LONG,
                              data->cancellable,
                              search_files_cb,
                              data);

      data->executables = g_list_remove_link (data->executables, item);
      g_list_free_full (item, g_free);
    }
  else
    {
      g_object_unref (source_object);
      install_missing_executables_cb (data);
    }
}

static void
printer_get_ppd_cb (const gchar *ppd_filename,
                    gpointer     user_data)
{
  GDBusConnection *bus;
  IMEData         *data = (IMEData *) user_data;
  GError          *error = NULL;

  data->ppd_file_name = g_strdup (ppd_filename);
  if (data->ppd_file_name)
    {
      bus = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, &error);
      if (!bus)
        {
          g_warning ("%s", error->message);
          g_error_free (error);
        }
      else
        {
          g_dbus_connection_call (bus,
                                  SCP_BUS,
                                  SCP_PATH,
                                  SCP_IFACE,
                                  "MissingExecutables",
                                  g_variant_new ("(s)", data->ppd_file_name),
                                  G_VARIANT_TYPE ("(as)"),
                                  G_DBUS_CALL_FLAGS_NONE,
                                  DBUS_TIMEOUT,
                                  data->cancellable,
                                  get_missing_executables_cb,
                                  data);
          return;
        }
    }

  install_missing_executables_cb (data);
}

static void
pp_maintenance_command_execute_cb (GObject      *source_object,
                                   GAsyncResult *res,
                                   gpointer      user_data)
{
  PpMaintenanceCommand *command = (PpMaintenanceCommand *) source_object;
  GError               *error = NULL;
  PCData               *data;
  gboolean              result;

  result = pp_maintenance_command_execute_finish (command, res, &error);
  g_object_unref (source_object);

  if (result)
    {
      data = (PCData *) user_data;

      data->autoconfigure_finished = TRUE;
      printer_configure_async_finish (data);
    }
  else
    {
      if (error->domain != G_IO_ERROR ||
          error->code != G_IO_ERROR_CANCELLED)
        {
          data = (PCData *) user_data;

          g_warning ("%s", error->message);

          data->autoconfigure_finished = TRUE;
          printer_configure_async_finish (data);
        }

      g_error_free (error);
    }
}

static void
printer_configure_async (PpNewPrinter *new_printer)
{
  GDBusConnection      *bus;
  PCData               *data;
  IMEData              *ime_data;
  gchar               **values;
  GError               *error = NULL;

  data = g_new0 (PCData, 1);
  data->new_printer = new_printer;
  data->set_accept_jobs_finished = FALSE;
  data->set_enabled_finished = FALSE;
  data->autoconfigure_finished = FALSE;
  data->set_media_size_finished = FALSE;
  data->install_missing_executables_finished = FALSE;

  /* Enable printer and make it accept jobs */
  if (new_printer->name)
    {
      bus = g_bus_get_sync (G_BUS_TYPE_SYSTEM, NULL, &error);
      if (bus)
        {
          g_dbus_connection_call (bus,
                                  MECHANISM_BUS,
                                  "/",
                                  MECHANISM_BUS,
                                  "PrinterSetAcceptJobs",
                                  g_variant_new ("(sbs)",
                                                 new_printer->name,
                                                 TRUE,
                                                 ""),
                                  G_VARIANT_TYPE ("(s)"),
                                  G_DBUS_CALL_FLAGS_NONE,
                                  -1,
                                  NULL,
                                  printer_set_accepting_jobs_cb,
                                  data);

          g_dbus_connection_call (g_object_ref (bus),
                                  MECHANISM_BUS,
                                  "/",
                                  MECHANISM_BUS,
                                  "PrinterSetEnabled",
                                  g_variant_new ("(sb)",
                                                 new_printer->name,
                                                 TRUE),
                                  G_VARIANT_TYPE ("(s)"),
                                  G_DBUS_CALL_FLAGS_NONE,
                                  -1,
                                  NULL,
                                  printer_set_enabled_cb,
                                  data);
        }
      else
        {
          g_warning ("Failed to get system bus: %s", error->message);
          g_error_free (error);
          data->set_accept_jobs_finished = TRUE;
          data->set_enabled_finished = TRUE;
        }
    }
  else
    {
      data->set_accept_jobs_finished = TRUE;
      data->set_enabled_finished = TRUE;
    }

  /* Run autoconfiguration of printer */
  if (!new_printer->is_network_device)
    {
      PpMaintenanceCommand *command;
      command = pp_maintenance_command_new (new_printer->name,
                                            "autoconfigure",
                                            NULL,
      /* Translators: Name of job which makes printer to autoconfigure itself */
                                            _("Automatic configuration"));

      pp_maintenance_command_execute_async (command,
                                            NULL,
                                            pp_maintenance_command_execute_cb,
                                            data);
    }

  /* Set media size for printer */
  values = g_new0 (gchar *, 2);
  values[0] = g_strdup (get_page_size_from_locale ());

  printer_add_option_async (new_printer->name, "PageSize", values, FALSE, NULL, pao_cb, data);

  g_strfreev (values);

  /* Install missing executables for printer */
  ime_data = g_new0 (IMEData, 1);
  ime_data->window_id = new_printer->window_id;
  if (data->cancellable)
    ime_data->cancellable = g_object_ref (data->cancellable);
  ime_data->user_data = data;

  printer_get_ppd_async (new_printer->name,
                         NULL,
                         0,
                         printer_get_ppd_cb,
                         ime_data);
}

static void
_pp_new_printer_add_async (GSimpleAsyncResult *res,
                           GObject            *object,
                           GCancellable       *cancellable)
{
  PpNewPrinter        *printer = PP_NEW_PRINTER (object);

  printer->res = g_object_ref (res);
  printer->cancellable = g_object_ref (cancellable);

  if (printer->ppd_name || printer->ppd_file_name)
    {
      /* We have everything we need */
      printer_add_real_async (printer);
    }
  else if (printer->device_id)
    {
      GDBusConnection *bus;
      GError          *error = NULL;

      /* Try whether CUPS has a driver for the new printer */
      bus = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, &error);
      if (bus)
        {
          g_dbus_connection_call (bus,
                                  SCP_BUS,
                                  SCP_PATH,
                                  SCP_IFACE,
                                  "GetBestDrivers",
                                  g_variant_new ("(sss)",
                                                 printer->device_id,
                                                 printer->make_and_model ? printer->make_and_model : "",
                                                 printer->device_uri ? printer->device_uri : ""),
                                  G_VARIANT_TYPE ("(a(ss))"),
                                  G_DBUS_CALL_FLAGS_NONE,
                                  DBUS_TIMEOUT_LONG,
                                  cancellable,
                                  printer_add_async_scb,
                                  printer);
        }
      else
        {
          g_warning ("Failed to get system bus: %s", error->message);
          g_error_free (error);
          _pp_new_printer_add_async_cb (FALSE, printer);
        }
    }
  else if (printer->original_name && printer->host_name)
    {
      /* Try to get PPD from remote CUPS */
      printer_get_ppd_async (printer->original_name,
                             printer->host_name,
                             printer->host_port,
                             printer_add_async_scb4,
                             printer);
    }
  else
    {
      _pp_new_printer_add_async_cb (FALSE, printer);
    }
}

void
pp_new_printer_add_async (PpNewPrinter        *printer,
                          GCancellable        *cancellable,
                          GAsyncReadyCallback  callback,
                          gpointer             user_data)
{
  GSimpleAsyncResult *res;

  res = g_simple_async_result_new (G_OBJECT (printer), callback, user_data, pp_new_printer_add_async);

  g_simple_async_result_set_check_cancellable (res, cancellable);
  _pp_new_printer_add_async (res, G_OBJECT (printer), cancellable);

  g_object_unref (res);
}

gboolean
pp_new_printer_add_finish (PpNewPrinter  *printer,
                           GAsyncResult  *res,
                           GError       **error)
{
  GSimpleAsyncResult *simple = G_SIMPLE_ASYNC_RESULT (res);

  g_warn_if_fail (g_simple_async_result_get_source_tag (simple) == pp_new_printer_add_async);

  if (g_simple_async_result_propagate_error (simple, error))
    return FALSE;

  return g_simple_async_result_get_op_res_gboolean (simple);
}
