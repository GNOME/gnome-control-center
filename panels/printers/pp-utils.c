/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright 2009-2010  Red Hat, Inc,
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
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
 */

#include "config.h"

#include <glib.h>
#include <glib/gi18n.h>
#include <glib/gstdio.h>
#include <gtk/gtk.h>
#include <cups/cups.h>
#include <dbus/dbus-glib.h>

#include "pp-utils.h"

DBusGProxy *
get_dbus_proxy (const gchar *name,
                const gchar *path,
                const gchar *iface,
                const gboolean system_bus)
{
  DBusGConnection *bus;
  DBusGProxy      *proxy;
  GError          *error;

  error = NULL;
  if (system_bus)
    bus = dbus_g_bus_get (DBUS_BUS_SYSTEM, &error);
  else
    bus = dbus_g_bus_get (DBUS_BUS_SESSION, &error);

  if (bus == NULL)
    {
      if (system_bus)
        /* Translators: Program cannot connect to DBus' system bus */
        g_warning ("Could not connect to system bus: %s", error->message);
      else
        /* Translators: Program cannot connect to DBus' session bus */
        g_warning ("Could not connect to session bus: %s", error->message);
      g_error_free (error);
      return NULL;
    }

  error = NULL;

  proxy = dbus_g_proxy_new_for_name (bus, name, path, iface);

  return proxy;
}

gchar *get_tag_value (const gchar *tag_string, const gchar *tag_name)
{
  gchar **tag_string_splitted = NULL;
  gchar  *tag_value = NULL;
  gint    tag_name_length = strlen (tag_name);
  gint    i;

  tag_string_splitted = g_strsplit (tag_string, ";", 0);
  for (i = 0; i < g_strv_length (tag_string_splitted); i++)
    if (g_ascii_strncasecmp (tag_string_splitted[i], tag_name, tag_name_length) == 0)
      if (strlen (tag_string_splitted[i]) > tag_name_length + 1)
        tag_value = g_strdup (tag_string_splitted[i] + tag_name_length + 1);
  g_strfreev (tag_string_splitted);

  return tag_value;
}

gchar *
get_ppd_name (gchar *device_class,
              gchar *device_id,
              gchar *device_info,
              gchar *device_make_and_model,
              gchar *device_uri,
              gchar *device_location)
{
  http_t *http = NULL;
  ipp_t  *request = NULL;
  ipp_t  *response = NULL;
  gchar  *mfg = NULL;
  gchar  *mdl = NULL;
  gchar  *result = NULL;

  mfg = get_tag_value (device_id, "mfg");
  mdl = get_tag_value (device_id, "mdl");

  http = httpConnectEncrypt (cupsServer (),
                             ippPort (),
                             cupsEncryption ());

  if (http)
    {
      request = ippNewRequest (CUPS_GET_PPDS);
      if (device_id)
        ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_TEXT, "ppd-device-id",
                     NULL, device_id);
      else if (mfg)
        ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_TEXT, "ppd-make",
                     NULL, mfg);
      response = cupsDoRequest (http, request, "/");

      if (response)
        {
          ipp_attribute_t *attr = NULL;
          const char      *ppd_device_id;
          const char      *ppd_make_model;
          const char      *ppd_make;
          const char      *ppd_name;
          gchar           *ppd_mfg;
          gchar           *ppd_mdl;

          for (attr = response->attrs; attr != NULL; attr = attr->next)
            {
              while (attr != NULL && attr->group_tag != IPP_TAG_PRINTER)
                attr = attr->next;

              if (attr == NULL)
                break;

              ppd_device_id  = "NONE";
              ppd_make_model = NULL;
              ppd_make       = NULL;
              ppd_name       = NULL;
              ppd_mfg        = NULL;
              ppd_mdl        = NULL;

              while (attr != NULL && attr->group_tag == IPP_TAG_PRINTER)
                {
                  if (!strcmp(attr->name, "ppd-device-id") &&
                      attr->value_tag == IPP_TAG_TEXT)
                    ppd_device_id = attr->values[0].string.text;
                  else if (!strcmp(attr->name, "ppd-name") &&
                           attr->value_tag == IPP_TAG_NAME)
                    ppd_name = attr->values[0].string.text;
                  else if (!strcmp(attr->name, "ppd-make") &&
                           attr->value_tag == IPP_TAG_TEXT)
                    ppd_make = attr->values[0].string.text;
                  else if (!strcmp(attr->name, "ppd-make-and-model") &&
                           attr->value_tag == IPP_TAG_TEXT)
                    ppd_make_model = attr->values[0].string.text;

                  attr = attr->next;
                }

              if (mfg && mdl && !result)
                {
                  if (ppd_device_id)
                    {
                      ppd_mfg = get_tag_value (ppd_device_id, "mfg");

                      if (ppd_mfg && g_ascii_strcasecmp (ppd_mfg, mfg) == 0)
                        {
                          ppd_mdl = get_tag_value (ppd_device_id, "mdl");

                          if (ppd_mdl && g_ascii_strcasecmp (ppd_mdl, mdl) == 0)
                            {
                              result = g_strdup (ppd_name);
                              g_free (ppd_mdl);
                            }
                          g_free (ppd_mfg);
                        }
                    }
                }

              if (attr == NULL)
                break;
            }
          ippDelete(response);
        }
      httpClose (http);
    }

  g_free (mfg);
  g_free (mdl);

  return result;
}

char *
get_dest_attr (const char *dest_name,
               const char *attr)
{
  cups_dest_t *dests;
  int          num_dests;
  cups_dest_t *dest;
  const char  *value;
  char        *ret;

  if (dest_name == NULL)
          return NULL;

  ret = NULL;

  num_dests = cupsGetDests (&dests);
  if (num_dests < 1) {
          g_debug ("Unable to get printer destinations");
          return NULL;
  }

  dest = cupsGetDest (dest_name, NULL, num_dests, dests);
  if (dest == NULL) {
          g_debug ("Unable to find a printer named '%s'", dest_name);
          goto out;
  }

  value = cupsGetOption (attr, dest->num_options, dest->options);
  if (value == NULL) {
          g_debug ("Unable to get %s for '%s'", attr, dest_name);
          goto out;
  }
  ret = g_strdup (value);
out:
  cupsFreeDests (num_dests, dests);

  return ret;
}

ipp_t *
execute_maintenance_command (const char *printer_name,
                             const char *command,
                             const char *title)
{
  http_t *http;
  GError *error = NULL;
  ipp_t  *request = NULL;
  ipp_t  *response = NULL;
  char    uri[HTTP_MAX_URI + 1];
  int     fd = -1;

  http = httpConnectEncrypt (cupsServer (),
                             ippPort (),
                             cupsEncryption ());

  if (http)
    {
      request = ippNewRequest (IPP_PRINT_JOB);

      g_snprintf (uri,
                  sizeof (uri),
                  "ipp://localhost/printers/%s",
                  printer_name);

      ippAddString (request,
                    IPP_TAG_OPERATION,
                    IPP_TAG_URI,
                    "printer-uri",
                    NULL,
                    uri);

      ippAddString (request, IPP_TAG_OPERATION, IPP_TAG_NAME, "job-name",
                    NULL, title);

      ippAddString (request, IPP_TAG_JOB, IPP_TAG_MIMETYPE, "document-format",
                    NULL, "application/vnd.cups-command");

      gchar *file_name = NULL;
      fd = g_file_open_tmp ("ccXXXXXX", &file_name, &error);

      if (fd != -1 && !error)
        {
          FILE *file;

          file = fdopen (fd, "w");
          fprintf (file, "#CUPS-COMMAND\n");
          fprintf (file, "%s\n", command);
          fclose (file);

          response = cupsDoFileRequest (http, request, "/", file_name);
          g_unlink (file_name);
        }

      g_free (file_name);
      httpClose (http);
    }

  return response;
}

int
ccGetAllowedUsers (gchar ***allowed_users, const char *printer_name)
{
  const char * const   attrs[1] = { "requesting-user-name-allowed" };
  http_t              *http;
  ipp_t               *request = NULL;
  gchar              **users = NULL;
  ipp_t               *response;
  char                 uri[HTTP_MAX_URI + 1];
  int                  num_allowed_users = 0;

  http = httpConnectEncrypt (cupsServer (),
                             ippPort (),
                             cupsEncryption ());

  if (http || !allowed_users)
    {
      request = ippNewRequest (IPP_GET_PRINTER_ATTRIBUTES);

      g_snprintf (uri, sizeof (uri), "ipp://localhost/printers/%s", printer_name);
      ippAddString (request,
                    IPP_TAG_OPERATION,
                    IPP_TAG_URI,
                    "printer-uri",
                    NULL,
                    uri);
      ippAddStrings (request,
                     IPP_TAG_OPERATION,
                     IPP_TAG_KEYWORD,
                     "requested-attributes",
                     1,
                     NULL,
                     attrs);

      response = cupsDoRequest (http, request, "/");
      if (response)
        {
          ipp_attribute_t *attr = NULL;
          ipp_attribute_t *allowed = NULL;

          for (attr = response->attrs; attr != NULL; attr = attr->next)
            {
              if (attr->group_tag == IPP_TAG_PRINTER &&
                  attr->value_tag == IPP_TAG_NAME &&
                  !g_strcmp0 (attr->name, "requesting-user-name-allowed"))
                allowed = attr;
            }

          if (allowed && allowed->num_values > 0)
            {
              int i;

              num_allowed_users = allowed->num_values;
              users = g_new (gchar*, num_allowed_users);

              for (i = 0; i < num_allowed_users; i ++)
                users[i] = g_strdup (allowed->values[i].string.text);
            }
          ippDelete(response);
        }
       httpClose (http);
     }

  *allowed_users = users;
  return num_allowed_users;
}

gchar *
get_ppd_attribute (const gchar *printer_name, const gchar *attribute_name)
{
  const char *file_name = NULL;
  ppd_file_t *ppd_file = NULL;
  ppd_attr_t *ppd_attr = NULL;
  gchar *result = NULL;

  file_name = cupsGetPPD (printer_name);

  if (file_name)
    {
      ppd_file = ppdOpenFile (file_name);
      if (ppd_file)
        {
          ppd_attr = ppdFindAttr (ppd_file, attribute_name, NULL);
          if (ppd_attr != NULL)
            result = g_strdup (ppd_attr->value);
          ppdClose (ppd_file);
        }
      g_unlink (file_name);
    }

  return result;
}
