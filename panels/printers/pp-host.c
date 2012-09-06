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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * Author: Marek Kasik <mkasik@redhat.com>
 */

#include "pp-host.h"

struct _PpHostPrivate
{
  gchar *hostname;
  gint   port;
};

G_DEFINE_TYPE (PpHost, pp_host, G_TYPE_OBJECT);

enum {
  PROP_0 = 0,
  PROP_HOSTNAME,
  PROP_PORT,
};

static void
pp_host_finalize (GObject *object)
{
  PpHostPrivate *priv;

  priv = PP_HOST (object)->priv;

  g_clear_pointer (&priv->hostname, g_free);

  G_OBJECT_CLASS (pp_host_parent_class)->finalize (object);
}

static void
pp_host_get_property (GObject    *object,
                      guint       prop_id,
                      GValue     *value,
                      GParamSpec *param_spec)
{
  PpHost *self;

  self = PP_HOST (object);

  switch (prop_id)
    {
      case PROP_HOSTNAME:
        g_value_set_string (value, self->priv->hostname);
        break;
      case PROP_PORT:
        g_value_set_int (value, self->priv->port);
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object,
                                           prop_id,
                                           param_spec);
      break;
    }
}

static void
pp_host_set_property (GObject      *object,
                      guint         prop_id,
                      const GValue *value,
                      GParamSpec   *param_spec)
{
  PpHost *self = PP_HOST (object);

  switch (prop_id)
    {
      case PROP_HOSTNAME:
        g_free (self->priv->hostname);
        self->priv->hostname = g_value_dup_string (value);
        break;
      case PROP_PORT:
        self->priv->port = g_value_get_int (value);
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object,
                                           prop_id,
                                           param_spec);
        break;
    }
}

static void
pp_host_class_init (PpHostClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  g_type_class_add_private (klass, sizeof (PpHostPrivate));

  gobject_class->set_property = pp_host_set_property;
  gobject_class->get_property = pp_host_get_property;
  gobject_class->finalize = pp_host_finalize;

  g_object_class_install_property (gobject_class, PROP_HOSTNAME,
    g_param_spec_string ("hostname",
                         "Hostname",
                         "The hostname",
                         NULL,
                         G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, PROP_PORT,
    g_param_spec_int ("port",
                      "Port",
                      "The port",
                      0, G_MAXINT32, 631,
                      G_PARAM_READWRITE));
}

static void
pp_host_init (PpHost *host)
{
  host->priv = G_TYPE_INSTANCE_GET_PRIVATE (host,
                                            PP_TYPE_HOST,
                                            PpHostPrivate);
}

PpHost *
pp_host_new (const gchar *hostname,
             gint         port)
{
  return g_object_new (PP_TYPE_HOST,
                       "hostname", hostname,
                       "port", port,
                       NULL);
}

typedef struct
{
  PpDevicesList *devices;
} GSDData;

static gchar **
line_split (gchar *line)
{
  gboolean   escaped = FALSE;
  gboolean   quoted = FALSE;
  gboolean   in_word = FALSE;
  gchar    **words = NULL;
  gchar    **result = NULL;
  gchar     *buffer = NULL;
  gchar      ch;
  gint       n = 0;
  gint       i, j = 0, k = 0;

  if (line)
    {
      n = strlen (line);
      words = g_new0 (gchar *, n + 1);
      buffer = g_new0 (gchar, n + 1);

      for (i = 0; i < n; i++)
        {
          ch = line[i];

          if (escaped)
            {
              buffer[k++] = ch;
              escaped = FALSE;
              continue;
            }

          if (ch == '\\')
            {
              in_word = TRUE;
              escaped = TRUE;
              continue;
            }

          if (in_word)
            {
              if (quoted)
                {
                  if (ch == '"')
                    quoted = FALSE;
                  else
                    buffer[k++] = ch;
                }
              else if (g_ascii_isspace (ch))
                {
                  words[j++] = g_strdup (buffer);
                  memset (buffer, 0, n + 1);
                  k = 0;
                  in_word = FALSE;
                }
              else if (ch == '"')
                quoted = TRUE;
              else
                buffer[k++] = ch;
            }
          else
            {
              if (ch == '"')
                {
                  in_word = TRUE;
                  quoted = TRUE;
                }
              else if (!g_ascii_isspace (ch))
                {
                  in_word = TRUE;
                  buffer[k++] = ch;
                }
            }
        }
    }

  if (buffer && buffer[0] != '\0')
    words[j++] = g_strdup (buffer);

  result = g_strdupv (words);
  g_strfreev (words);
  g_free (buffer);

  return result;
}

static void
_pp_host_get_snmp_devices_thread (GSimpleAsyncResult *res,
                                  GObject            *object,
                                  GCancellable       *cancellable)
{
  PpHost         *host = (PpHost *) object;
  PpHostPrivate  *priv = host->priv;
  PpPrintDevice  *device;
  GSDData        *data;
  GError         *error;
  gchar         **argv;
  gchar          *stdout_string = NULL;
  gchar          *stderr_string = NULL;
  gint            exit_status;

  data = g_simple_async_result_get_op_res_gpointer (res);
  data->devices = g_new0 (PpDevicesList, 1);
  data->devices->devices = NULL;

  argv = g_new0 (gchar *, 3);
  argv[0] = g_strdup ("/usr/lib/cups/backend/snmp");
  argv[1] = g_strdup (priv->hostname);

  /* Use SNMP to get printer's informations */
  g_spawn_sync (NULL,
                argv,
                NULL,
                0,
                NULL,
                NULL,
                &stdout_string,
                &stderr_string,
                &exit_status,
                &error);

  g_free (argv[1]);
  g_free (argv[0]);
  g_free (argv);

  if (exit_status == 0 && stdout_string)
    {
      gchar **printer_informations = NULL;
      gint    length;

      printer_informations = line_split (stdout_string);
      length = g_strv_length (printer_informations);

      if (length >= 4)
        {
          device = g_new0 (PpPrintDevice, 1);

          device->device_class = g_strdup (printer_informations[0]);
          device->device_uri = g_strdup (printer_informations[1]);
          device->device_make_and_model = g_strdup (printer_informations[2]);
          device->device_info = g_strdup (printer_informations[3]);
          device->device_name = g_strdup (printer_informations[3]);
          device->device_name =
            g_strcanon (device->device_name, ALLOWED_CHARACTERS, '-');
          device->acquisition_method = ACQUISITION_METHOD_SNMP;

          if (length >= 5 && printer_informations[4][0] != '\0')
            device->device_id = g_strdup (printer_informations[4]);

          if (length >= 6 && printer_informations[5][0] != '\0')
            device->device_location = g_strdup (printer_informations[5]);

          data->devices->devices = g_list_append (data->devices->devices, device);
        }

      g_strfreev (printer_informations);
      g_free (stdout_string);
    }
}

static void
gsd_data_free (GSDData *data)
{
  GList *iter;

  if (data)
    {
      if (data->devices)
        {
          if (data->devices->devices)
            {
              for (iter = data->devices->devices; iter; iter = iter->next)
                pp_print_device_free ((PpPrintDevice *) iter->data);
              g_list_free (data->devices->devices);
            }

          g_free (data->devices);
        }

      g_free (data);
    }
}

void
pp_host_get_snmp_devices_async (PpHost              *host,
                                GCancellable        *cancellable,
                                GAsyncReadyCallback  callback,
                                gpointer             user_data)
{
  GSimpleAsyncResult *res;
  GSDData            *data;

  res = g_simple_async_result_new (G_OBJECT (host), callback, user_data, pp_host_get_snmp_devices_async);
  data = g_new0 (GSDData, 1);
  data->devices = NULL;

  g_simple_async_result_set_check_cancellable (res, cancellable);
  g_simple_async_result_set_op_res_gpointer (res, data, (GDestroyNotify) gsd_data_free);
  g_simple_async_result_run_in_thread (res, _pp_host_get_snmp_devices_thread, 0, cancellable);

  g_object_unref (res);
}

PpDevicesList *
pp_host_get_snmp_devices_finish (PpHost        *host,
                                 GAsyncResult  *res,
                                 GError       **error)
{
  GSimpleAsyncResult *simple = G_SIMPLE_ASYNC_RESULT (res);
  GSDData            *data;
  PpDevicesList      *result;

  g_warn_if_fail (g_simple_async_result_get_source_tag (simple) == pp_host_get_snmp_devices_async);

  if (g_simple_async_result_propagate_error (simple, error))
    return NULL;

  data = g_simple_async_result_get_op_res_gpointer (simple);
  result = data->devices;
  data->devices = NULL;

  return result;
}

static void
_pp_host_get_remote_cups_devices_thread (GSimpleAsyncResult *res,
                                         GObject            *object,
                                         GCancellable       *cancellable)
{
  cups_dest_t   *dests = NULL;
  GSDData       *data;
  PpHost        *host = (PpHost *) object;
  PpHostPrivate *priv = host->priv;
  PpPrintDevice *device;
  http_t        *http;
  gint           num_of_devices = 0;
  gint           i;

  data = g_simple_async_result_get_op_res_gpointer (res);
  data->devices = g_new0 (PpDevicesList, 1);
  data->devices->devices = NULL;

  /* Connect to remote CUPS server and get its devices */
  http = httpConnect (priv->hostname, priv->port);
  if (http)
    {
      num_of_devices = cupsGetDests2 (http, &dests);
      if (num_of_devices > 0)
        {
          for (i = 0; i < num_of_devices; i++)
            {
              device = g_new0 (PpPrintDevice, 1);
              device->device_class = g_strdup ("network");
              device->device_uri = g_strdup_printf ("ipp://%s:%d/printers/%s",
                                           priv->hostname,
                                           priv->port,
                                           dests[i].name);
              device->device_name = g_strdup (dests[i].name);
              device->device_location = g_strdup (cupsGetOption ("printer-location",
                                                        dests[i].num_options,
                                                        dests[i].options));
              device->host_name = g_strdup (priv->hostname);
              device->host_port = priv->port;
              device->acquisition_method = ACQUISITION_METHOD_REMOTE_CUPS_SERVER;
              data->devices->devices = g_list_append (data->devices->devices, device);
            }
        }

      httpClose (http);
    }
}

void
pp_host_get_remote_cups_devices_async (PpHost              *host,
                                       GCancellable        *cancellable,
                                       GAsyncReadyCallback  callback,
                                       gpointer             user_data)
{
  GSimpleAsyncResult *res;
  GSDData            *data;

  res = g_simple_async_result_new (G_OBJECT (host), callback, user_data, pp_host_get_remote_cups_devices_async);
  data = g_new0 (GSDData, 1);
  data->devices = NULL;

  g_simple_async_result_set_check_cancellable (res, cancellable);
  g_simple_async_result_set_op_res_gpointer (res, data, (GDestroyNotify) gsd_data_free);
  g_simple_async_result_run_in_thread (res, _pp_host_get_remote_cups_devices_thread, 0, cancellable);

  g_object_unref (res);
}

PpDevicesList *
pp_host_get_remote_cups_devices_finish (PpHost        *host,
                                        GAsyncResult  *res,
                                        GError       **error)
{
  GSimpleAsyncResult *simple = G_SIMPLE_ASYNC_RESULT (res);
  GSDData            *data;
  PpDevicesList      *result;

  g_warn_if_fail (g_simple_async_result_get_source_tag (simple) == pp_host_get_remote_cups_devices_async);

  if (g_simple_async_result_propagate_error (simple, error))
    return NULL;

  data = g_simple_async_result_get_op_res_gpointer (simple);
  result = data->devices;
  data->devices = NULL;

  return result;
}
