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

#include <glib/gstdio.h>

#include "pp-maintenance-command.h"

#include "pp-utils.h"

#if (CUPS_VERSION_MAJOR > 1) || (CUPS_VERSION_MINOR > 5)
#define HAVE_CUPS_1_6 1
#endif

#ifndef HAVE_CUPS_1_6
#define ippGetCount(attr)     attr->num_values
#define ippGetValueTag(attr)  attr->value_tag
#define ippGetStatusCode(ipp) ipp->request.status.status_code
#define ippGetString(attr, element, language) attr->values[element].string.text
#endif

struct _PpMaintenanceCommandPrivate
{
  gchar *printer_name;
  gchar *command;
  gchar *title;
};

G_DEFINE_TYPE (PpMaintenanceCommand, pp_maintenance_command, G_TYPE_OBJECT);

enum {
  PROP_0 = 0,
  PROP_PRINTER_NAME,
  PROP_COMMAND,
  PROP_TITLE
};

static void
pp_maintenance_command_finalize (GObject *object)
{
  PpMaintenanceCommandPrivate *priv;

  priv = PP_MAINTENANCE_COMMAND (object)->priv;

  g_clear_pointer (&priv->printer_name, g_free);
  g_clear_pointer (&priv->command, g_free);
  g_clear_pointer (&priv->title, g_free);

  G_OBJECT_CLASS (pp_maintenance_command_parent_class)->finalize (object);
}

static void
pp_maintenance_command_get_property (GObject    *object,
                                     guint       prop_id,
                                     GValue     *value,
                                     GParamSpec *param_spec)
{
  PpMaintenanceCommand *self;

  self = PP_MAINTENANCE_COMMAND (object);

  switch (prop_id)
    {
      case PROP_PRINTER_NAME:
        g_value_set_string (value, self->priv->printer_name);
        break;
      case PROP_COMMAND:
        g_value_set_string (value, self->priv->command);
        break;
      case PROP_TITLE:
        g_value_set_string (value, self->priv->title);
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object,
                                           prop_id,
                                           param_spec);
      break;
    }
}

static void
pp_maintenance_command_set_property (GObject      *object,
                                     guint         prop_id,
                                     const GValue *value,
                                     GParamSpec   *param_spec)
{
  PpMaintenanceCommand *self = PP_MAINTENANCE_COMMAND (object);

  switch (prop_id)
    {
      case PROP_PRINTER_NAME:
        g_free (self->priv->printer_name);
        self->priv->printer_name = g_value_dup_string (value);
        break;
      case PROP_COMMAND:
        g_free (self->priv->command);
        self->priv->command = g_value_dup_string (value);
        break;
      case PROP_TITLE:
        g_free (self->priv->title);
        self->priv->title = g_value_dup_string (value);
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object,
                                           prop_id,
                                           param_spec);
        break;
    }
}

static void
pp_maintenance_command_class_init (PpMaintenanceCommandClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  g_type_class_add_private (klass, sizeof (PpMaintenanceCommandPrivate));

  gobject_class->set_property = pp_maintenance_command_set_property;
  gobject_class->get_property = pp_maintenance_command_get_property;
  gobject_class->finalize = pp_maintenance_command_finalize;

  g_object_class_install_property (gobject_class, PROP_PRINTER_NAME,
    g_param_spec_string ("printer-name",
                         "Printer name",
                         "Name of the printer",
                         NULL,
                         G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, PROP_COMMAND,
    g_param_spec_string ("command",
                         "Maintenance command",
                         "Command to execute",
                         NULL,
                         G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, PROP_TITLE,
    g_param_spec_string ("title",
                         "Command title",
                         "Title of the job by which the command will be executed",
                         NULL,
                         G_PARAM_READWRITE));
}

static void
pp_maintenance_command_init (PpMaintenanceCommand *command)
{
  command->priv = G_TYPE_INSTANCE_GET_PRIVATE (command,
                                               PP_TYPE_MAINTENANCE_COMMAND,
                                               PpMaintenanceCommandPrivate);
}

PpMaintenanceCommand *
pp_maintenance_command_new (const gchar *printer_name,
                            const gchar *command,
                            const gchar *title)
{
  return g_object_new (PP_TYPE_MAINTENANCE_COMMAND,
                       "printer-name", printer_name,
                       "command", command,
                       "title", title,
                       NULL);
}

static void
_pp_maintenance_command_execute_thread (GSimpleAsyncResult *res,
                                        GObject            *object,
                                        GCancellable       *cancellable)
{
  PpMaintenanceCommand        *command = (PpMaintenanceCommand *) object;
  PpMaintenanceCommandPrivate *priv = command->priv;
  static const char           *attrs[] = {"printer-commands"};
  ipp_attribute_t             *attr = NULL;
  gboolean                     success = FALSE;
  GError                      *error = NULL;
  ipp_t                       *request;
  ipp_t                       *response = NULL;
  gchar                       *printer_uri;
  gchar                       *printer_commands = NULL;
  gchar                       *printer_commands_lowercase = NULL;
  gchar                       *command_lowercase;
  gchar                       *file_name = NULL;
  int                          fd = -1;

  printer_uri = g_strdup_printf ("ipp://localhost/printers/%s",
                                 priv->printer_name);

  request = ippNewRequest (IPP_GET_PRINTER_ATTRIBUTES);
  ippAddString (request, IPP_TAG_OPERATION, IPP_TAG_URI,
                "printer-uri", NULL, printer_uri);
  ippAddStrings (request, IPP_TAG_OPERATION, IPP_TAG_KEYWORD,
                 "requested-attributes", 1, NULL, attrs);
  response = cupsDoRequest (CUPS_HTTP_DEFAULT, request, "/");

  if (response)
    {
      if (ippGetStatusCode (response) <= IPP_OK_CONFLICT)
        {
          attr = ippFindAttribute (response, "printer-commands", IPP_TAG_ZERO);
          if (attr && ippGetCount (attr) > 0 && ippGetValueTag (attr) != IPP_TAG_NOVALUE)
            {
              if (ippGetValueTag (attr) == IPP_TAG_KEYWORD)
                {
                  printer_commands = g_strdup (ippGetString (attr, 0, NULL));
                }
            }
          else
            {
              success = TRUE;
            }
        }

      ippDelete (response);
    }

  if (printer_commands)
    {
      command_lowercase = g_ascii_strdown (priv->command, -1);
      printer_commands_lowercase = g_ascii_strdown (printer_commands, -1);

      if (g_strrstr (printer_commands_lowercase, command_lowercase))
        {
          request = ippNewRequest (IPP_PRINT_JOB);

          ippAddString (request, IPP_TAG_OPERATION, IPP_TAG_URI,
                        "printer-uri", NULL, printer_uri);
          ippAddString (request, IPP_TAG_OPERATION, IPP_TAG_NAME,
                        "job-name", NULL, priv->title);
          ippAddString (request, IPP_TAG_JOB, IPP_TAG_MIMETYPE,
                        "document-format", NULL, "application/vnd.cups-command");

          fd = g_file_open_tmp ("ccXXXXXX", &file_name, &error);

          if (fd != -1)
            {
              FILE *file;

              file = fdopen (fd, "w");
              fprintf (file, "#CUPS-COMMAND\n");
              fprintf (file, "%s\n", priv->command);
              fclose (file);

              response = cupsDoFileRequest (CUPS_HTTP_DEFAULT, request, "/", file_name);
              g_unlink (file_name);

              if (response)
                {
                  if (ippGetStatusCode (response) <= IPP_OK_CONFLICT)
                    {
                      success = TRUE;
                    }

                  ippDelete (response);
                }
            }

          g_free (file_name);
        }
      else
        {
          success = TRUE;
        }

      g_free (command_lowercase);
      g_free (printer_commands_lowercase);
      g_free (printer_commands);
    }

  g_free (printer_uri);

  if (!success)
    {
      g_simple_async_result_set_error (res,
                                       G_IO_ERROR,
                                       G_IO_ERROR_FAILED,
                                       "Execution of maintenance command failed.");
    }

  g_simple_async_result_set_op_res_gboolean (res, success);
}

void
pp_maintenance_command_execute_async (PpMaintenanceCommand *command,
                                      GCancellable         *cancellable,
                                      GAsyncReadyCallback   callback,
                                      gpointer              user_data)
{
  GSimpleAsyncResult *res;

  res = g_simple_async_result_new (G_OBJECT (command), callback, user_data, pp_maintenance_command_execute_async);

  g_simple_async_result_set_check_cancellable (res, cancellable);
  g_simple_async_result_run_in_thread (res, _pp_maintenance_command_execute_thread, 0, cancellable);

  g_object_unref (res);
}

gboolean
pp_maintenance_command_execute_finish (PpMaintenanceCommand  *command,
                                       GAsyncResult          *res,
                                       GError               **error)
{
  GSimpleAsyncResult *simple = G_SIMPLE_ASYNC_RESULT (res);

  g_warn_if_fail (g_simple_async_result_get_source_tag (simple) == pp_maintenance_command_execute_async);

  if (g_simple_async_result_propagate_error (simple, error))
    {
      return FALSE;
    }

  return g_simple_async_result_get_op_res_gboolean (simple);
}
