/*
 * Copyright (C) 2016 Red Hat, Inc
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
 * Authors: Martin Hatina <mhatina@redhat.com>
 *          Marek Kasik <mkasik@redhat.com>
 */

#include "pp-printer.h"

#include "pp-job.h"

typedef struct _PpPrinter        PpPrinter;
typedef struct _PpPrinterPrivate PpPrinterPrivate;

struct _PpPrinter
{
  GObject           parent_instance;
  PpPrinterPrivate *priv;
};

struct _PpPrinterPrivate
{
  gchar *printer_name;
};

G_DEFINE_TYPE_WITH_PRIVATE (PpPrinter, pp_printer, G_TYPE_OBJECT)

enum
{
  PROP_0 = 0,
  PROP_NAME
};

static void
pp_printer_dispose (GObject *object)
{
  PpPrinterPrivate *priv = PP_PRINTER (object)->priv;

  g_free (priv->printer_name);

  G_OBJECT_CLASS (pp_printer_parent_class)->dispose (object);
}

static void
pp_printer_finalize (GObject *object)
{
  G_OBJECT_CLASS (pp_printer_parent_class)->finalize (object);
}

static void
pp_printer_get_property (GObject    *object,
                         guint       property_id,
                         GValue     *value,
                         GParamSpec *pspec)
{
  PpPrinter *self = PP_PRINTER (object);

  switch (property_id)
    {
      case PROP_NAME:
        g_value_set_string (value, self->priv->printer_name);
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}

static void
pp_printer_set_property (GObject      *object,
                         guint         property_id,
                         const GValue *value,
                         GParamSpec   *pspec)
{
  PpPrinter *self = PP_PRINTER (object);

  switch (property_id)
    {
      case PROP_NAME:
        g_free (self->priv->printer_name);
        self->priv->printer_name = g_value_dup_string (value);
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }

}

static void
pp_printer_class_init (PpPrinterClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->set_property = pp_printer_set_property;
  gobject_class->get_property = pp_printer_get_property;
  gobject_class->dispose = pp_printer_dispose;
  gobject_class->finalize = pp_printer_finalize;

  g_object_class_install_property (gobject_class, PROP_NAME,
    g_param_spec_string ("printer-name",
                         "Printer name",
                         "Name of this printer",
                         NULL,
                         G_PARAM_READWRITE));
}

static void
pp_printer_init (PpPrinter *printer)
{
  printer->priv = G_TYPE_INSTANCE_GET_PRIVATE (printer,
                                               PP_TYPE_PRINTER,
                                               PpPrinterPrivate);
}

PpPrinter *
pp_printer_new (const gchar *name)
{
  PpPrinter *self = g_object_new (PP_TYPE_PRINTER, "printer-name", name, NULL);

  return self;
}

static void
printer_rename_thread (GTask        *task,
                       gpointer      source_object,
                       gpointer      task_data,
                       GCancellable *cancellable)
{
  PpPrinter *printer = PP_PRINTER (source_object);
  gboolean   result;
  gchar     *new_printer_name = task_data;
  gchar     *old_printer_name;

  g_object_get (printer, "printer-name", &old_printer_name, NULL);

  result = printer_rename (old_printer_name, new_printer_name);

  if (result)
    {
      g_object_set (printer, "printer-name", new_printer_name, NULL);
    }

  g_free (old_printer_name);

  g_task_return_boolean (task, result);
}

static void
printer_rename_dbus_cb (GObject      *source_object,
                        GAsyncResult *res,
                        gpointer      user_data)
{
  PpPrinter *printer;
  GVariant  *output;
  gboolean   result = FALSE;
  GError    *error = NULL;
  GTask     *task = user_data;
  gchar     *old_printer_name;

  output = g_dbus_connection_call_finish (G_DBUS_CONNECTION (source_object),
                                          res,
                                          &error);
  g_object_unref (source_object);

  if (output != NULL)
    {
      const gchar *ret_error;

      printer = g_task_get_source_object (task);
      g_object_get (printer, "printer-name", &old_printer_name, NULL);

      g_variant_get (output, "(&s)", &ret_error);
      if (ret_error[0] != '\0')
        {
          g_warning ("cups-pk-helper: renaming of printer %s failed: %s", old_printer_name, ret_error);
        }
      else
        {
          result = TRUE;
          g_object_set (printer, "printer-name", g_task_get_task_data (task), NULL);
        }

      g_task_return_boolean (task, result);

      g_free (old_printer_name);
      g_variant_unref (output);
    }
  else
    {
      if (error->domain == G_DBUS_ERROR &&
          (error->code == G_DBUS_ERROR_SERVICE_UNKNOWN ||
           error->code == G_DBUS_ERROR_UNKNOWN_METHOD))
        {
          g_warning ("Update cups-pk-helper to at least 0.2.6 please to be able to use PrinterRename method.");
          g_error_free (error);

          g_task_run_in_thread (task, printer_rename_thread);
        }
      else
        {
          g_task_return_boolean (task, FALSE);
        }
    }
}

static void
get_bus_cb (GObject      *source_object,
            GAsyncResult *res,
            gpointer      user_data)
{
  GDBusConnection *bus;
  GError          *error = NULL;
  GTask           *task = user_data;
  gchar           *printer_name;

  bus = g_bus_get_finish (res, &error);
  if (bus != NULL)
    {
      g_object_get (g_task_get_source_object (task),
                    "printer-name", &printer_name,
                    NULL);
      g_dbus_connection_call (bus,
                              MECHANISM_BUS,
                              "/",
                              MECHANISM_BUS,
                              "PrinterRename",
                              g_variant_new ("(ss)",
                                             printer_name,
                                             g_task_get_task_data (task)),
                              G_VARIANT_TYPE ("(s)"),
                              G_DBUS_CALL_FLAGS_NONE,
                              -1,
                              g_task_get_cancellable (task),
                              printer_rename_dbus_cb,
                              task);

      g_free (printer_name);
    }
  else
    {
      g_warning ("Failed to get system bus: %s", error->message);
      g_error_free (error);
      g_task_return_boolean (task, FALSE);
    }
}

void
pp_printer_rename_async (PpPrinter           *printer,
                         const gchar         *new_printer_name,
                         GCancellable        *cancellable,
                         GAsyncReadyCallback  callback,
                         gpointer             user_data)
{
  GTask *task;

  g_return_if_fail (new_printer_name != NULL);

  task = g_task_new (G_OBJECT (printer), cancellable, callback, user_data);
  g_task_set_task_data (task, g_strdup (new_printer_name), g_free);

  g_bus_get (G_BUS_TYPE_SYSTEM,
             cancellable,
             get_bus_cb,
             task);
}

gboolean
pp_printer_rename_finish (PpPrinter     *printer,
                          GAsyncResult  *res,
                          GError       **error)
{
  g_return_val_if_fail (g_task_is_valid (res, printer), FALSE);
  g_object_unref (res);

  return g_task_propagate_boolean (G_TASK (res), error);
}

typedef struct
{
  gboolean  myjobs;
  gint      which_jobs;
} GetJobsData;

static void
get_jobs_thread (GTask        *task,
                 gpointer      source_object,
                 gpointer      task_data,
                 GCancellable *cancellable)
{
  GetJobsData *get_jobs_data = task_data;
  cups_job_t  *jobs = NULL;
  PpPrinter   *printer = PP_PRINTER (source_object);
  gchar       *printer_name;
  GList       *list = NULL;
  gint         num_jobs;
  gint         i;

  g_object_get (printer, "printer-name", &printer_name, NULL);

  num_jobs = cupsGetJobs (&jobs,
                          printer_name,
                          get_jobs_data->myjobs ? 1 : 0,
                          get_jobs_data->which_jobs);
  g_free (printer_name);

  for (i = 0; i < num_jobs; i++)
    {
      PpJob *job;

      job = g_object_new (pp_job_get_type (),
                          "id",    jobs[i].id,
                          "title", jobs[i].title,
                          "state", jobs[i].state,
                          NULL);

      list = g_list_append (list, job);
    }
  cupsFreeJobs (num_jobs, jobs);

  if (g_task_set_return_on_cancel (task, FALSE))
    {
      g_task_return_pointer (task, list, (GDestroyNotify) g_list_free);
    }
}

void
pp_printer_get_jobs_async (PpPrinter           *printer,
                           gboolean             myjobs,
                           gint                 which_jobs,
                           GCancellable        *cancellable,
                           GAsyncReadyCallback  callback,
                           gpointer             user_data)
{
  GetJobsData *get_jobs_data;
  GTask       *task;

  get_jobs_data = g_new (GetJobsData, 1);
  get_jobs_data->myjobs = myjobs;
  get_jobs_data->which_jobs = which_jobs;

  task = g_task_new (G_OBJECT (printer), cancellable, callback, user_data);
  g_task_set_task_data (task, get_jobs_data, g_free);
  g_task_set_return_on_cancel (task, TRUE);
  g_task_run_in_thread (task, get_jobs_thread);
  g_object_unref (task);
}

GList *
pp_printer_get_jobs_finish (PpPrinter          *printer,
                            GAsyncResult       *res,
                            GError            **error)
{
  g_return_val_if_fail (g_task_is_valid (res, printer), NULL);

  return g_task_propagate_pointer (G_TASK (res), error);
}
